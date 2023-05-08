#include <string>
#include <vector>
#include <queue>
#include <filesystem>
#include <memory>
#include <chrono>
#include <atomic>
#include <stdio.h>

#include <QLocale>
#include <spdlog/spdlog.h>
#include "basic_lazy_file_sink.hpp"
#include <spdlog/logger.h>
#include <fmt/core.h>
#include <fmt/chrono.h>

#include "sync.hpp"
#include "main.hpp"
#include "transcode.hpp"
#include "config.hpp"
#include <easync.hpp>


Sync::Sync(const std::filesystem::path &source, const std::filesystem::path &dest, const std::filesystem::path &log_path, 
           Application &app, const Config &config, std::atomic_flag &stop_flag)
: source(source), dest(dest), app(app), config(config), stop_flag(stop_flag)
{
    connect(this, &Sync::message, &app, &Application::message);
    connect(this, &Sync::set_progress_bar, &app, &Application::set_progress_bar);
    connect(this, &Sync::set_progress_bar_max, &app, &Application::set_progress_bar_max);
    connect(this, &Sync::update_speed_eta, &app, &Application::update_speed_eta);
    if (!log_path.empty()) {
        file_sink = std::make_shared<spdlog::sinks::basic_lazy_file_sink_mt>(log_path.string(), true);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        logger = std::make_unique<spdlog::logger>("", file_sink);
        spdlog::level::level_enum level = config.get_log_level();
        logger->set_level(level);
        logger->flush_on(level == spdlog::level::debug ? spdlog::level::debug : spdlog::level::err);
    }
}

void Sync::scan_source()
{
    emit message(tr("Scanning source directory..."));
    for (const auto &entry : std::filesystem::recursive_directory_iterator(source)) {
        if (!entry.is_regular_file())
            continue;
        FileType type = filetype_from_path(entry.path());
        if (type == FileType::NONE && !config.copy_nonaudio)
            continue;

        const std::filesystem::path &input = entry.path();
        const auto& [action, out_codec] = config.get_file_handling(input);
        std::filesystem::path output = dest / std::filesystem::relative(input, source);
        if (action == Action::TRANSCODE)
            output.replace_extension(config.get_output_ext(out_codec, input));
        if (!config.skip_existing 
            || !std::filesystem::exists(output)
            || (config.sync_newer && std::filesystem::last_write_time(input) > std::filesystem::last_write_time(output))) {
            if (action == Action::TRANSCODE)
                jobs.emplace(std::make_unique<Transcoder::Job>(input, output, out_codec));
            else
                files.emplace(input, std::move(output));
        }
    }
}

bool Sync::transcode()
{
    std::vector<std::unique_ptr<Transcoder::WorkerThread>> threads;
    std::mutex mutex;
    std::mutex ffmutex;
    std::unique_lock lock(mutex);
    std::condition_variable cond;
    avg = std::make_unique<ExpAvg>(nb_jobs);
    emit set_progress_bar_max((int) nb_jobs);
    emit set_progress_bar((progress = 0));
    emit message(tr("Transcoding %1 file(s)...").arg(QLocale().toString(nb_jobs)));
    last_update = std::chrono::system_clock::now();

    if (config.cpu_max_threads) {
        nb_threads = std::thread::hardware_concurrency();
        if (!nb_threads)
            nb_threads = 1;
    }
    else
        nb_threads = config.cpu_threads;
    if (nb_threads > nb_jobs)
        nb_threads = nb_jobs;
    logger->debug("Transcoding with {} threads", nb_threads);

    // Spawn threads
    for (size_t i = 0; i < nb_threads; i++) {
        threads.emplace_back(std::make_unique<Transcoder::WorkerThread>(i, jobs.front(), *this, mutex, cond, ffmutex, *logger, config));
        cond.wait_for(lock, std::chrono::milliseconds(100)); // Wait for the thread to initialize before starting the next
        jobs.pop();
    }

    // Feed jobs to the worker threads
    while (jobs.size() && !(stop |= stop_flag.test())) {
        cond.wait_for(lock, max_sleep_time());
        for (auto &thread : threads) {
            if (thread->place_job(jobs.front())) {
                jobs.pop();
                break;
            }
        }
        update_transcode_progress();
    }
    if (!stop)
        cond.wait_for(lock, std::chrono::milliseconds(100));

    // Wait for threads to finish their jobs
    while (1) {
        for (auto thread = threads.begin(); thread != threads.end();)
            thread = (*thread)->wait() ? threads.erase(thread) : thread + 1;
        if (!threads.size())
            break;
        cond.wait_for(lock, max_sleep_time());
        update_transcode_progress();
    }
    return true;
}

std::chrono::duration<int64_t, std::milli> Sync::max_sleep_time()
{
    auto diff = std::chrono::round<std::chrono::milliseconds>(std::chrono::system_clock::now() - last_update);
    auto duration = progress_interval - diff;
    return duration > max_sleep || duration < duration.zero() ? max_sleep : duration;
}

// Called by the main thread periodically throughout the transcoding
void Sync::update_transcode_progress()
{
    const auto now = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update) < progress_interval)
        return;
    last_update = now;
    if (avg->empty())
        return;

    if (received_update)
        eta.update((int) (nb_jobs - jobs_finished), avg->time(), (int) nb_threads);
    else
        eta -= progress_interval;
    received_update = false;

    QString speed_str(QString::fromStdString(fmt::format("{:.1f}x", avg->speed() * static_cast<double>(nb_threads))));
    QString eta_str(eta.get_updates() < nb_threads
                    ? tr("TBD") 
                    : QString::fromStdString(fmt::format("{:%H:%M:%S}",std::chrono::round<std::chrono::seconds>(eta.eta()))));
    emit update_speed_eta(speed_str, eta_str);
}

// Called by a worker thread after finishing a transcode while owning the main mutex
void Sync::transcode_finished(double speed, double time)
{
    if (avg)
        avg->update(speed, time);
    jobs_finished++;
    received_update = true;

    emit set_progress_bar(++progress);
}

// Called by a worker thread after a transcode has failed while owning the main mutex
void Sync::report_transcode_error(const std::filesystem::path &path)
{
    emit message(tr("Failed to transcode file '%1'").arg(QString::fromStdString(path.string())));
    ret = false;
    if (config.abort_on_error)
        stop = true;
    jobs_finished++;
    emit set_progress_bar(++progress);
}

void Sync::copy_finished(double speed, double time)
{
    emit set_progress_bar(++progress);
    received_update = true;
    avg->update(speed, time);
    files_remaining--;
}

void Sync::report_copy_error(const std::filesystem::path &path)
{
    emit message(tr("Failed to copy file '%1'").arg(QString::fromStdString(path.string())));
    ret = false;
    if (config.abort_on_error)
        stop = true;
    emit set_progress_bar(++progress);
    files_remaining--;
}

void Sync::Copier::copy()
{
    bool ret = true;
    size_t bytes;
    std::filesystem::path error_path;
    char buffer[BUFSIZ];
    {
        std::scoped_lock lock(mutex); // Wait for main thread before starting the copying
    }

    while (!files.empty() && (ret || !config.abort_on_error) && !(stop |= stop_flag.test())) {
        auto& [in_file, out_file] = files.front();
        const auto begin = std::chrono::system_clock::now();
        bool file_ret = true;

        // Make sure directory exists
        std::filesystem::path directory(out_file.parent_path());
        if (!std::filesystem::exists(directory)) {
            try {
                logger.info("Creating directory '{}'", directory.string());
                std::filesystem::create_directories(directory);
            }
            catch(...) {
                logger.error("Could not create directory '{}'", directory.string());
                file_ret = false;
            }
        }
        if (file_ret) {
            std::unique_ptr<FILE, decltype(&fclose)> in(fopen(in_file.string().c_str(), "rb"), fclose);
            std::unique_ptr<FILE, decltype(&fclose)> out(fopen(out_file.string().c_str(), "wb"), fclose);
            file_ret = in && out;
            if (file_ret) {
                logger.info("Copying file '{}' to '{}'", in_file.string(), out_file.string());
                while(file_ret && (bytes = fread(buffer, 1, sizeof(buffer), in.get())))
                    file_ret = fwrite(buffer, 1, bytes, out.get()) == bytes;
            }
        }
        const auto duration = std::chrono::ceil<std::chrono::microseconds>(std::chrono::system_clock::now() - begin);
        double time = static_cast<double>(duration.count());
        double size_mb = static_cast<double>(std::filesystem::file_size(in_file)) / 1048576.0;
        double speed =  size_mb / (time / 1000000.0);
        std::scoped_lock lock(mutex);
        if (file_ret)
            sync.copy_finished(speed, time);
        else {
            logger.error("Failed to copy file '{}'", in_file.string());
            sync.report_copy_error(in_file);
        }
        files.pop();
        if (files.empty())
            finished = true;
        ret &= file_ret;
        cv.notify_all();
    }
}

bool Sync::copy_files()
{
    bool finished = false;
    emit set_progress_bar((progress = 0));
    emit set_progress_bar_max((int) nb_files);
    emit message(tr("Copying %1 file(s)...").arg(QLocale().toString(nb_files)));
    avg = std::make_unique<ExpAvg>(nb_files);
    files_remaining = nb_files;
    std::mutex mutex;
    std::unique_lock lock(mutex);
    std::condition_variable cv;
    Copier copier(files, *this, finished, mutex, cv, config, *logger, stop_flag);
    received_update = false;
    last_update = std::chrono::system_clock::now();
    while (!finished && !(stop |= stop_flag.test())) {
        cv.wait_for(lock, max_sleep_time());
        update_copy_progress(files_remaining);
    }
    if (lock.owns_lock()) // Make sure we aren't holding the lock if we quit early
        lock.unlock();
    emit set_progress_bar(++progress);
    copier.finish();
    return ret;
}

void Sync::update_copy_progress(size_t files_remaining)
{
    const auto now = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update) < progress_interval)
        return;
    last_update = now;
    if (avg->empty())
        return;

    if (received_update)
        eta.update((int) files_remaining, avg->time() / 1000.0, 1);
    else
        eta -= progress_interval;
    received_update = false;

    QString speed_str(QString::fromStdString(fmt::format("{:.1f} MB/s", avg->speed())));
    QString eta_str(QString::fromStdString(fmt::format("{:%H:%M:%S}", std::chrono::round<std::chrono::seconds>(eta.eta()))));
    emit update_speed_eta(speed_str, eta_str);
}

// Generate a list of files to delete
void Sync::scan_dest()
{
    emit message (tr("Scanning destination directory..."));

    // Generate vectors of all possible input file extensions for a given output extension
    // based on user settings
    std::vector<std::pair<Codec, std::vector<std::string>>> exts;
    for (const auto &fh : config.file_handling) {
        std::vector<std::string> *ext_vec = nullptr;
        Codec codec = fh.action == Action::COPY ? fh.in_codec : fh.out_codec;
        auto it = std::find_if(exts.begin(), exts.end(), [&](const auto &i) { return i.first == codec; });
        if (it == exts.end()) {
            exts.emplace_back(std::make_pair<Codec, std::vector<std::string>>(std::move(codec), {}));
            ext_vec = &exts.back().second;
        }
        else
            ext_vec = &(it->second);

        const auto &input_exts = exts_for_codec(fh.in_codec);
        for (const auto &ext : input_exts)
            ext_vec->push_back(ext);
    }
    
    // Scan destination directory
    for (const auto &entry : std::filesystem::recursive_directory_iterator(dest)) {
        if (entry.is_directory()) {
            directory_list.add(entry.path(), dest);
            continue;
        }
        if (!entry.is_regular_file())
            continue;
        const auto &path = entry.path();
        FileType type = filetype_from_path(path);
        std::vector<Codec> codecs;
        if (type == FileType::NONE) {
            if (!config.copy_nonaudio)
               continue;
        }
        else {
            codecs = codecs_from_path(path);
            if (codecs.empty())
                continue;
        }
        bool exists = false;
        std::filesystem::path source_path = source / std::filesystem::relative(path, dest);

        // Non-audio, same file exension
        if (type == FileType::NONE)
            exists = std::filesystem::exists(source_path);

        // Audio, check every possible input file extension
        else {
            for (Codec codec : codecs) {
                auto it = std::find_if(exts.begin(), exts.end(), [&](const auto &i){ return i.first == codec; });
                if (it != exts.end()) {
                    for (const auto &ext : it->second) {
                        source_path.replace_extension(ext);
                        if ((exists = std::filesystem::exists(source_path)))
                            goto end;
                    }
                }
            }
        }
end:
        if (!exists)
            deleted_files.push_back(path);
    }
}

bool Sync::delete_files()
{
    bool ret = true;
    emit message(tr("Deleting %1 file(s) in destination directory...").arg(QLocale().toString(deleted_files.size())));
    for (auto it = deleted_files.begin(); it != deleted_files.end() && (ret || !config.abort_on_error); ++it) {
        logger->info("Deleting destination file '{}'", it->string());
        try {
            std::filesystem::remove(*it);
        }
        catch(...) {
            ret = false;
            logger->error("Could not delete destination file '{}'", it->string());
            emit message(tr("Could not delete destination file '%1'").arg(it->string().c_str()));
        }
    }
    
    if (!directory_list.empty() && !(ret &= directory_list.delete_all(config, *logger)))
        emit message(tr("Could not delete one or more empty subdirectories in destination"));

    return this->ret;
}

Sync::Result Sync::sync()
{
    scan_source();

    nb_jobs = jobs.size();
    nb_files = files.size();
    if (nb_jobs && !(stop |= stop_flag.test()))
        ret &= transcode();

    if (nb_files && (ret || !config.abort_on_error) && !(stop |= stop_flag.test()))
        ret &= copy_files();

    if (config.clean_dest && (ret || !config.abort_on_error) && !(stop |= stop_flag.test())) {
        scan_dest();
        nb_delete = deleted_files.size();
    }
    if (nb_delete && !(stop |= stop_flag.test()))
        ret &= delete_files();

    if (!nb_jobs && !nb_files && !nb_delete)
        return Result::NOTHING;

    if (stop) {
        if (!ret && config.abort_on_error)
            return Result::ABORTED;
        else
            return Result::STOPPED;
    }
    else
        return ret ? Result::SUCCESS : Result::ERRORS;
}


void Sync::DirectoryList::add(const std::filesystem::path &path, const std::filesystem::path &dest)
{
    static constexpr char separator = std::filesystem::path::preferred_separator;
    std::filesystem::path relative = std::filesystem::relative(path, dest);
    const std::string &s = relative.string();
    int n = static_cast<int>(std::count(s.begin(), s.end(), separator));

    auto it = dirs.find(n);
    if (it == dirs.end())
        dirs.insert({n, {path}});
    else
        it->second.push_back(path);
}

bool Sync::DirectoryList::delete_all(const Config &config, spdlog::logger &logger)
{
    bool ret = true;
    for (auto it = dirs.rbegin(); it != dirs.rend() && (ret || !config.abort_on_error); ++it) {
        const auto &vec = it->second;
        for (auto dir = vec.begin(); dir != vec.end() && (ret || !config.abort_on_error); ++dir) {
            if (std::filesystem::is_empty(*dir)) {
                logger.info("Deleting empty destination directory '{}'", dir->string());
                try {
                    std::filesystem::remove(*dir);
                }
                catch(...) {
                    logger.error("Could not delete destination directory '{}'", dir->string());
                    ret = false;
                }
            }
        }
    }

    return ret;
}

