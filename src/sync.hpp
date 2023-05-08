#pragma once

#include <filesystem>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <map>

#include <QObject>
#include <QString>
#include <spdlog/logger.h>
#include "basic_lazy_file_sink.hpp"
#include <fmt/core.h>

#include "transcode.hpp"

class Application;
struct Config;

class Sync : public QObject
{
    Q_OBJECT

    public:
        enum Result {
            NOTHING,
            SUCCESS,
            ERRORS,
            STOPPED,
            ABORTED
        };

        void transcode_finished(double speed, double time);
        void report_transcode_error(const std::filesystem::path &path);
        void copy_finished(double speed, double time);
        void report_copy_error(const std::filesystem::path &path);
        Sync(const std::filesystem::path &source, const std::filesystem::path &dest,
             const std::filesystem::path &log_path, Application &app, const Config &config,
             std::atomic_flag &stop_flag);

    public slots:
        void begin() { emit finished(static_cast<int>(sync())); }

    signals:
        void message(const QString &message);
        void set_progress_bar_max(int max);
        void set_progress_bar(int val);
        void update_speed_eta(const QString &speed, const QString &eta);
        void finished(int result);

    private:
        class DirectoryList
        {
            public:
                void add(const std::filesystem::path &path, const std::filesystem::path &dest);
                bool delete_all(const Config &config, spdlog::logger &logger);
                inline bool empty() { return dirs.empty(); }

            private:
                std::map<int, std::vector<std::filesystem::path>> dirs;
        };
        class Copier {
            public:
                Copier(std::queue<std::pair<std::filesystem::path, std::filesystem::path>> &files, Sync &sync,
                       bool &finished, std::mutex &mutex, std::condition_variable &cv, const Config &config, 
                       spdlog::logger &logger, std::atomic_flag &stop_flag)
                : files(files), sync(sync), finished(finished), mutex(mutex), cv(cv), config(config), 
                  logger(logger), stop_flag(stop_flag)
                {
                    thread = std::make_unique<std::thread>(&Copier::copy, this);
                }
                void finish() { thread->join(); }

            private:
                std::queue<std::pair<std::filesystem::path, std::filesystem::path>> &files;
                Sync &sync;
                bool &finished;
                std::mutex &mutex;
                std::condition_variable &cv;
                const Config &config;
                spdlog::logger &logger;
                std::atomic_flag &stop_flag;
                std::unique_ptr<std::thread> thread;
                bool stop = false;
                void copy();
        };

        std::filesystem::path source;
        std::filesystem::path dest;
        Application &app;
        const Config &config;
        std::atomic_flag &stop_flag;
        bool stop = false;
        std::shared_ptr<spdlog::sinks::basic_lazy_file_sink_mt> file_sink;
        std::unique_ptr<spdlog::logger> logger;
        std::queue<std::unique_ptr<Transcoder::Job>> jobs;
        std::queue<std::pair<std::filesystem::path, std::filesystem::path>> files;
        std::vector<std::filesystem::path> deleted_files;
        std::vector<std::vector<double>> speeds;
        ETA eta;
        std::unique_ptr<ExpAvg> avg;
        bool received_update = false;
        std::chrono::time_point<std::chrono::system_clock> last_update;
        const std::chrono::duration<int64_t, std::milli> progress_interval = std::chrono::milliseconds(1000);
        const std::chrono::duration<int64_t, std::milli> max_sleep = std::chrono::milliseconds(200);
        DirectoryList directory_list;
        bool ret = true;
        int progress = 0;
        size_t nb_directories = 0;
        size_t nb_jobs = 0;
        size_t nb_files = 0;
        size_t files_remaining = 0;
        size_t nb_delete = 0;
        size_t nb_threads;
        size_t jobs_finished = 0;

        Result sync();
        std::chrono::duration<int64_t, std::milli> max_sleep_time();
        void update_transcode_progress();
        void update_copy_progress(size_t files_remaining);
        void scan_source();
        bool transcode();
        bool copy_files();
        void scan_dest();
        bool delete_files();
};

