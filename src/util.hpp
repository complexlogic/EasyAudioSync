#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

#include <QtGlobal>
#include <QSettings>
#include <QString>

extern "C" {
#include "libavcodec/avcodec.h"
}

#define AVG_WINDOW 5.0

enum class FileType {
    NONE = -1,
    MP3,
    FLAC,
    OGG,
    OPUS,
    MP4,
    WMA,
    WAV,
    WAVPACK,
    APE,
	MPC
};

enum class Codec {
    NONE = -1,
    MP3,
    FLAC,
    VORBIS,
    OPUS,
    AAC,
    ALAC,
    WMA,
    WAV,
    WAVPACK,
    APE,
	MPC
};

enum class Action {
    COPY,
    TRANSCODE
};
struct FileHandling {
    Codec in_codec;
    Action action;
    Codec out_codec;
};

class ExpAvg {
    public:
        void update(double speed, double time);
        bool empty() const { return avg_speed == 0.0; }
        double speed() const { return avg_speed; }
        double time() const { return avg_time; }

        ExpAvg(int size) : size(size) {}

    private:
        double avg_speed = 0.0;
        double avg_time = 0.0;
        double window = AVG_WINDOW;
        int size;
};

class ETA {
    public:
        void update(int jobs_left, double avg_time, int nb_threads);
        void operator-=(std::chrono::duration<int64_t, std::milli> duration);
        const std::chrono::milliseconds& eta() const { return time_remaining; }
        size_t get_updates() const { return nb_updates; }

    private:
        std::chrono::milliseconds time_remaining;
        size_t nb_updates = 0;
};

int determine_sample_rate(Codec codec, const AVCodec *avcodec, int in_rate);
int64_t determine_bitrate(int64_t in_rate, int nb_channels);
FileType filetype_from_path(const std::filesystem::path &path);
Codec codec_from_path(const std::filesystem::path &path);
std::vector<Codec> codecs_from_path(const std::filesystem::path &path);
AVCodecID ffcodec_from_file(const std::filesystem::path &path);
std::string get_output_ext(Codec codec, const std::filesystem::path &path);
const std::vector<std::string>& exts_for_codec(Codec codec);
void save_window_size(QWidget *window, const QString &name, QSettings &settings);
void save_window_pos(QWidget *window, const QString &name, QSettings &settings);
bool restore_window_size(QWidget *window, const QString &name, QSettings &settings);
bool restore_window_pos(QWidget *window, const QString &name, QSettings &settings);

inline bool join_path([[maybe_unused]] std::filesystem::path &p)
{
    return true;
}

template<typename... Args>
inline bool join_path(std::filesystem::path &path, const std::filesystem::path &first, const Args&... args)
{
    path /= first;
    return join_path(path, args...);
}

template<typename... Args>
inline std::filesystem::path join_paths(const std::filesystem::path &first, const Args&... args)
{
    std::filesystem::path path(first);
    return join_path(path, args...) ? path : std::filesystem::path();
}
