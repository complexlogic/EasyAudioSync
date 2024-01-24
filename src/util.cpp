#include <string>
#include <algorithm>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <thread>
#include <cassert>

#include <QSettings>
#include <QWidget>
#include <QString>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include <fmt/core.h>

#include "util.hpp"

FileType filetype_from_path(const std::filesystem::path &path)
{
    static const std::unordered_map<std::string, FileType> map {
        {".mp3",  FileType::MP3},
        {".flac", FileType::FLAC},
        {".ogg",  FileType::OGG},
        {".opus", FileType::OPUS},
        {".m4a",  FileType::MP4},
        {".m4b",  FileType::MP4},
        {".wav",  FileType::WAV},
        {".wv",   FileType::WAVPACK},
        {".ape",  FileType::APE},
        {".mpc",  FileType::MPC},
        {".wma",  FileType::WMA},
    };
    std::string ext(path.extension().string());
    std::transform(ext.begin(), ext.end(), ext.begin(), tolower);
    auto it = map.find(path.extension().string());
    return it == map.end() ? FileType::NONE : it->second;
}

// Get the exact codec, opening the file and checking if necessary
Codec codec_from_path(const std::filesystem::path &path)
{
    static const std::unordered_map<std::string, Codec> map {
        {".mp3",  Codec::MP3},
        {".flac", Codec::FLAC},
        {".opus", Codec::OPUS},
        {".wv",   Codec::WAVPACK},
        {".ape",  Codec::APE},
        {".mpc",  Codec::MPC},
        {".wav",  Codec::WAV},
        {".wma",  Codec::WMA}
    };
    if (!path.has_extension())
        return Codec::NONE;
    std::string ext(path.extension().string());
    std::transform(ext.begin(), ext.end(), ext.begin(), tolower);
    auto it = map.find(ext);
    if (it != map.end())
        return it->second;
    else {
        AVCodecID codec_id = ffcodec_from_file(path);
        if (codec_id == AV_CODEC_ID_ALAC)
            return Codec::ALAC;
        else if (codec_id == AV_CODEC_ID_AAC)
            return Codec::AAC;
        else if (codec_id == AV_CODEC_ID_VORBIS)
            return Codec::VORBIS;
        else
            return Codec::NONE;
    }
}

// This returns a vector of possible Codecs, so the file is not opened (faster)
std::vector<Codec> codecs_from_path(const std::filesystem::path &path)
{
    static const std::unordered_map<std::string, std::vector<Codec>> map {
        {".mp3", { Codec::MP3 }},
        {".flac", { Codec::FLAC }},
        {".ogg", { Codec::VORBIS, Codec::OPUS }},
        {".opus", { Codec::OPUS}},
        {".m4a", { Codec::ALAC, Codec::AAC }},
        {".m4b", { Codec::ALAC, Codec::AAC }},
        {".wav", { Codec::WAV }},
        {".wv", { Codec::WAVPACK }},
        {".ape", { Codec::APE }},
        {".mpc", { Codec::MPC }},
        {".wma", { Codec::WMA }}
    };
    std::string ext(path.extension().string());
    std::transform(ext.begin(), ext.end(), ext.begin(), tolower);
    auto it = map.find(ext);
    assert(it != map.end());
    return it == map.end() ? std::vector<Codec>() : it->second;
}

AVCodecID ffcodec_from_path(const std::filesystem::path &path)
{
    static const std::unordered_map<std::string, AVCodecID> map {
        {".mp3",  AV_CODEC_ID_MP3},
        {".flac", AV_CODEC_ID_FLAC},
        {".opus", AV_CODEC_ID_OPUS},
        {".wv",   AV_CODEC_ID_WAVPACK},
        {".ape",  AV_CODEC_ID_APE}
    };
    if (!path.has_extension())
        return AV_CODEC_ID_NONE;
    std::string ext(path.extension().string());
    std::transform(ext.begin(), ext.end(), ext.begin(), tolower);
    auto it = map.find(ext);
    assert(it != map.end());
    return it != map.end() ? it->second : AV_CODEC_ID_NONE;
}

AVCodecID ffcodec_from_file(const std::filesystem::path &path)
{
    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    const AVCodec *codec = nullptr;
    AVCodecID codec_id = AV_CODEC_ID_NONE;
    int stream_id = -1;

    if (!avformat_open_input(&fmt_ctx, path.string().c_str(), nullptr, nullptr)
        && (avformat_find_stream_info(fmt_ctx, nullptr) >= 0)
        && ((stream_id = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0)) >= 0)
        && ((codec_ctx = avcodec_alloc_context3(codec)) != nullptr)
        && (avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[stream_id]->codecpar) >= 0))
        codec_id = codec_ctx->codec_id;
        
    if(fmt_ctx)
        avformat_close_input(&fmt_ctx);
    if(codec_ctx)
        avcodec_free_context(&codec_ctx);
    return codec_id;
}

const std::vector<std::string>& exts_for_codec(Codec codec)
{
    static const std::vector<std::pair<Codec, std::vector<std::string>>> exts {
        { Codec::MP3, {".mp3"} },
        { Codec::FLAC, {".flac"} },
        { Codec::VORBIS, {".ogg"} },
        { Codec::OPUS, {".opus"} },
        { Codec::AAC, {".m4a", ".m4b"} },
        { Codec::ALAC, {".m4a", ".m4b"} },
        { Codec::WMA, {".wma"} },
        { Codec::WAV, {".wav"} },
        { Codec::WAVPACK, {".wv"} },
        { Codec::APE, {".ape"} },
        { Codec::MP3, {".ape"} },
        { Codec::MPC, {".mpc"}},
    };
    return std::find_if(exts.begin(), exts.end(), [&](const auto &i){return i.first == codec;})->second;
}

int determine_sample_rate(Codec codec, const AVCodec *avcodec, int in_rate)
{
    if (codec == Codec::MP3)
        return std::min(48000, in_rate);
    if (!avcodec->supported_samplerates)
        return in_rate;
    
    const int *rate = avcodec->supported_samplerates;
    while (*(rate + 1))
        rate++;
    while (rate > avcodec->supported_samplerates && (*rate - in_rate < 0))
        rate--;
    return *rate;
}

int64_t determine_bitrate(int64_t in_rate, int nb_channels)
{
    return nb_channels == 2 ? in_rate : (in_rate * nb_channels) / 2;
}

void ExpAvg::update(double speed, double time)
{
    bool is_empty = empty();
    avg_speed = is_empty ? speed : avg_speed + (speed - avg_speed) / window;
    avg_time = is_empty ? time : avg_time + (time - avg_time) / window;
}

void ETA::update(int jobs_left, double avg_time, int nb_threads)
{
    time_remaining = std::chrono::milliseconds(static_cast<int64_t>(static_cast<double>((jobs_left) * avg_time / static_cast<double>(nb_threads))));
    nb_updates++;
}

void ETA::operator-=(std::chrono::duration<int64_t, std::milli> duration)
{
    time_remaining -= duration;
    if (time_remaining < time_remaining.zero())
        time_remaining = time_remaining.zero();
}

void save_window_size(QWidget *window, const QString &name, QSettings &settings)
{
    if (window->isMaximized())
        return;
    settings.beginGroup("WindowSize");
    settings.setValue(name, window->size());
    settings.endGroup();
}

bool restore_window_size(QWidget *window, const QString &name, QSettings &settings)
{
    bool ret;
    settings.beginGroup("WindowSize");
    QSize size = settings.value(name).toSize();
    if ((ret = !size.isEmpty()))
        window->resize(size);
    settings.endGroup();
    return ret;
}

void save_window_pos(QWidget *window, const QString &name, QSettings &settings)
{
    if (window->isMaximized())
        return;
    settings.beginGroup("WindowPos");
    settings.setValue(name, window->pos());
    settings.endGroup();
}

bool restore_window_pos(QWidget *window, const QString &name, QSettings &settings)
{
    bool ret;
    settings.beginGroup("WindowPos");
    QPoint point = settings.value(name).toPoint();
    if (!(ret = point.isNull()))
        window->move(point);
    settings.endGroup();
    return ret;
}
