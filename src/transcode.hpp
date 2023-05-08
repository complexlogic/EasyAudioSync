#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

extern "C" {
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavutil/avutil.h"
#include "libavutil/audio_fifo.h"
}
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>

#include "metadata.hpp"
#include "util.hpp"

class Sync;
struct Config;

class Transcoder {
    public:
        struct Job {
            std::filesystem::path input;
            std::filesystem::path output;
            Codec out_codec;
            
            Job(const std::filesystem::path &input, std::filesystem::path &output, Codec out_codec)
            : input(input), output(std::move(output)), out_codec(out_codec) {}
            Job(Job &&o) : input(std::move(o.input)), output(std::move(o.output)), out_codec(o.out_codec) {}
        };
        class WorkerThread {
            public:
                WorkerThread(int id, std::unique_ptr<Job> &initial_job, Sync &sync, std::mutex &mutex,
                             std::condition_variable &cond, std::mutex &ffmutex, spdlog::logger &logger,
                             const Config &config);
                bool place_job(std::unique_ptr<Job> &job);
                bool wait();

            private:
                int id;
                std::unique_ptr<Job> job;
                Sync &sync;
                std::mutex &main_mutex;
                std::condition_variable &main_cond;
                std::mutex &ffmutex;
                spdlog::logger &logger;
                const Config &config;
                std::unique_ptr<std::thread> thread;
                std::mutex mutex;
                std::condition_variable cond;
                bool quit = false;
                bool job_available = true;

                void work();
        };

        Transcoder(Job &job, std::mutex &mutex, const Config &config, spdlog::logger &logger) 
        : in_file(job.input, this), out_file(job.output, this), out_codec(job.out_codec),
          mutex(mutex), config(config), metadata(config), logger(logger) {}
        ~Transcoder();
        bool transcode();
        const std::filesystem::path& in_path() { return in_file.path; }
        inline double get_duration() const { return duration; }
        template<typename ...Args>
        void ffmpeg_error(int error, fmt::format_string<Args...> fmts, Args&&... args);

    private:
        struct File {
            enum Type {
                INPUT,
                OUTPUT
            };
            const std::filesystem::path path;
            AVFormatContext *fmt_ctx = nullptr;
            AVCodecContext *codec_ctx = nullptr;
            const AVCodec *codec = nullptr;
            AVStream *stream = nullptr;
            Transcoder *transcoder;
            
            File(std::filesystem::path &path, Transcoder *transcoder) : path(std::move(path)), transcoder(transcoder) {}
            ~File();
            void debug_info(Type type, spdlog::logger &logger);
        };
        struct InputFile : File {
            int stream_id = -1;

            bool open();
            double duration() const;
            InputFile(std::filesystem::path &path, Transcoder *transcoder) : File(path, transcoder) {}
        };
        struct OutputFile : File {
            AVIOContext *io_ctx = nullptr;

            bool open(Codec codec, const InputFile &in_file, const Config &config);
            bool write_header(Codec out_codec, const Config &config);
            bool write_trailer();
            OutputFile(std::filesystem::path &path, Transcoder *transcoder) : File(path, transcoder) {}
            ~OutputFile();
        };
        class VolumeFilter {
            public:
                bool adjust(AVFrame *frame);
                inline double get_volume() const { return volume; }
                static VolumeFilter* factory(const AVCodecContext *codec_ctx, const Metadata &metadata, const Config &config);
                VolumeFilter(double volume, AVFilterGraph *graph, AVFilterContext *src_ctx, AVFilterContext *sink_ctx)
                : volume(volume), graph(graph), src_ctx(src_ctx), sink_ctx(sink_ctx) {}
                ~VolumeFilter();

            private:
                double volume = 0.0;
                AVFilterGraph *graph;
                AVFilterContext *src_ctx;
                AVFilterContext *sink_ctx;

                static double parse_replaygain(const Metadata &metadata, const Config &config);
        };

        InputFile in_file;
        OutputFile out_file;
        Codec out_codec;
        std::mutex &mutex;
        const Config &config;
        Metadata metadata;
        spdlog::logger &logger;
        int id = 0;
        int64_t pts = 0;
        double duration = 0.0;
        SwrContext *swr_ctx = nullptr;
        AVAudioFifo *fifo = nullptr;
        std::unique_ptr<VolumeFilter> volume_filter;
        bool init_resampler();
        bool init_fifo();
        bool decode_to_fifo(bool &finished);
        bool encode_from_fifo();
        bool encode_frame(AVFrame *frame, bool &has_data);
        void cleanup();
};
