
#include <string>
#include <array>
#include <vector>
#include <algorithm>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <stdio.h>
#include <math.h>

extern "C" {
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/avutil.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/opt.h"
#include "libavutil/dict.h"
}
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>

#include "sync.hpp"
#include "transcode.hpp"
#include "config.hpp"
#include "util.hpp"


bool Transcoder::InputFile::open()
{
    int error;
    if ((error = avformat_open_input(&fmt_ctx, path.string().c_str(), nullptr, nullptr)) < 0) {
        transcoder->ffmpeg_error(error, "Could not open input file");
        return false;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        transcoder->ffmpeg_error(error, "Could not find input stream info");
        return false;
    }

    if ((stream_id = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0)) < 0) {
        transcoder->logger.error("Failed to detect best input stream");
        return false;
    }
    stream = fmt_ctx->streams[stream_id];

    if (!(codec_ctx = avcodec_alloc_context3(codec))) {
        transcoder->logger.error("Failed to allocate an input codec context");
        return false;
    }

    if ((error = avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[stream_id]->codecpar)) < 0) {
        transcoder->ffmpeg_error(error, "Could not allocate input stream paraments to codec context");
        return false;
    }

    if ((error = avcodec_open2(codec_ctx, codec, nullptr)) < 0) {
        transcoder->ffmpeg_error(error, "Could not open decoder");
        return false;
    }

    codec_ctx->pkt_timebase = fmt_ctx->streams[stream_id]->time_base;
    return true;
}

double Transcoder::InputFile::duration() const
{
    return ((double) stream->duration) * av_q2d(stream->time_base) * 1000.0;
}

bool Transcoder::OutputFile::open(Codec out_codec, const InputFile &in_file, const Config &config)
{
    transcoder->logger.debug("Opening output file '{}'", path.string());
    std::filesystem::path directory = path.parent_path();
    if (!std::filesystem::exists(directory)) {
        try {
            transcoder->logger.info("Creating directory '{}'", directory.string());
            std::filesystem::create_directories(directory);
        }
        catch(...) {
            transcoder->logger.error("Could not create directory '{}'", directory.string());
            return false;
        }
    }
    int error;
    if ((error = avio_open(&io_ctx, fmt::format("file:{}", path.string()).c_str(), AVIO_FLAG_WRITE)) < 0) {
        transcoder->ffmpeg_error(error, "Could not open output IO context at '{}'", path.string());
        return false;
    }

    if (!(fmt_ctx = avformat_alloc_context())) {
        transcoder->logger.error("Could not allocate output format context");
        return false;
    }
    fmt_ctx->pb = io_ctx;
    fmt_ctx->flags |= AVFMT_FLAG_BITEXACT;

    if (!(fmt_ctx->oformat = av_guess_format(nullptr, path.string().c_str(), nullptr))) {
        transcoder->logger.error("Could not determine output container type");
        return false;
    }
    fmt_ctx->url = av_strdup(path.string().c_str());

    std::string encoder = config.get_encoder_name(out_codec);
    if (!(codec = avcodec_find_encoder_by_name(encoder.c_str()))) {
        transcoder->logger.error("Could not find an encoder for the output file");
        return false;
    }

    if(!(stream = avformat_new_stream(fmt_ctx, nullptr))) {
        transcoder->logger.error("Could not allocate stream in output file");
        return false;
    }

    if (!(codec_ctx = avcodec_alloc_context3(codec))) {
        transcoder->logger.error("Could not allocate output codec context");
        return false;
    }
    
    if (in_file.codec_ctx->ch_layout.nb_channels <= 2 || !config.downmix_multichannel) {
        if(av_channel_layout_copy(&codec_ctx->ch_layout, &in_file.codec_ctx->ch_layout) < 0) {
            transcoder->logger.error("Failed to set channel layout in output file");
            return false;
        }
    }
    else
        av_channel_layout_default(&codec_ctx->ch_layout, 2);

    int in_rate = in_file.codec_ctx->sample_rate > 48000 && config.downsample_hi_res ? 48000 : in_file.codec_ctx->sample_rate;
    codec_ctx->sample_rate = determine_sample_rate(out_codec, codec, in_rate);

    // Set bitrate/quality per the user settings
    if (out_codec == Codec::MP3) {
        const auto &preset = config.mp3.get_preset();
        if (preset.mode == Config::MP3::Mode::VBR) {
            codec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
            codec_ctx->global_quality = FF_QP2LAMBDA * preset.quality;
        }
        else if (preset.mode == Config::MP3::Mode::CBR)
            codec_ctx->bit_rate = determine_bitrate(preset.bit_rate, codec_ctx->ch_layout.nb_channels);

    }
    else if (out_codec == Codec::AAC) {
        codec_ctx->profile = config.aac.profile;
        if (encoder == "libfdk_aac") {
            const auto &preset = config.aac.fdk.get_preset();
            if (preset.mode == Config::AAC::Mode::VBR)
                av_opt_set_int(codec_ctx->priv_data, "vbr", preset.quality, 0);
            else
                codec_ctx->bit_rate = determine_bitrate(preset.bit_rate, codec_ctx->ch_layout.nb_channels);
            av_opt_set_int(codec_ctx->priv_data, "afterburner", config.aac.fdk.afterburner ? 1 : 0, 0);
        }
        else {
            const auto &preset = config.aac.lavc.get_preset();
            codec_ctx->bit_rate = determine_bitrate(preset.bit_rate, codec_ctx->ch_layout.nb_channels);
        }
    }
    else if (out_codec == Codec::VORBIS) {
        codec_ctx->flags |= AV_CODEC_FLAG_QSCALE;
        codec_ctx->global_quality = FF_QP2LAMBDA * config.ogg.quality;
    }
    else if (out_codec == Codec::OPUS) {
        const auto &preset = config.opus.get_preset();
        codec_ctx->bit_rate = determine_bitrate(preset.bit_rate, codec_ctx->ch_layout.nb_channels);
    }
    codec_ctx->sample_fmt = codec->sample_fmts[0];
    stream->time_base.den = codec_ctx->sample_rate;
    stream->time_base.num = 1;

    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if((error = avcodec_open2(codec_ctx, codec, nullptr)) < 0) {
        transcoder->ffmpeg_error(error, "Failed to open encoder");
        return false;
    }

    if ((error = avcodec_parameters_from_context(stream->codecpar, codec_ctx)) < 0) {
        transcoder->ffmpeg_error(error, "Could not initialize stream parameters");
        return false;
    }
    return true;
}

bool Transcoder::OutputFile::write_header(Codec codec, const Config &config)
{
    int error;
    AVDictionary *dict = nullptr;
    if (config.copy_metadata && codec == Codec::MP3)
        av_dict_set(&dict, "id3v2_version", std::to_string(config.mp3.id3v2_version).c_str(), 0);
    if ((error = avformat_write_header(fmt_ctx, &dict)) < 0) {
        transcoder->ffmpeg_error(error, "Could not write output file header");
        return false;
    }
    return true;
}

bool Transcoder::OutputFile::write_trailer()
{
    int error;
    if ((error = av_write_trailer(fmt_ctx)) < 0) {
        transcoder->ffmpeg_error(error, "Could not write output file trailer");
        return false;
    }
    avio_closep(&fmt_ctx->pb);
    fmt_ctx->pb = nullptr;
    return true;
}

Transcoder::OutputFile::~OutputFile()
{
    if (fmt_ctx) {
        if (fmt_ctx->pb)
            avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
        fmt_ctx = nullptr;
    }
}

Transcoder::File::~File()
{
    if(fmt_ctx)
        avformat_close_input(&fmt_ctx);
    if(codec_ctx)
        avcodec_free_context(&codec_ctx);
}

void Transcoder::File::debug_info(Type type, spdlog::logger &logger)
{
    char buffer[256];
    logger.debug("{} file characteristics:", type == Type::INPUT ? "Input" : "Output");
    logger.debug("  {:<15} {}", "Container:", type == Type::INPUT ? fmt_ctx->iformat->long_name : fmt_ctx->oformat->long_name);
    logger.debug("  {:<15} {}", "Codec:", codec->long_name);
    logger.debug("  {:<15} {}", "Bitrate:", codec_ctx->bit_rate ? fmt::format("{} bps", codec_ctx->bit_rate) : "Not available");
    logger.debug("  {:<15} {} Hz", "Sample rate:", codec_ctx->sample_rate);
    const char *sfmt;
    if ((sfmt = av_get_sample_fmt_name(codec_ctx->sample_fmt)))
        logger.debug("  {:<15} {}", "Sample format:", sfmt);
    if (av_channel_layout_describe(&codec_ctx->ch_layout, buffer, sizeof(buffer)) > 0)
        logger.debug("  {:<15} {} ch ({})", "Channel layout:", codec_ctx->ch_layout.nb_channels, buffer);
}

Transcoder::VolumeFilter* Transcoder::VolumeFilter::factory(const AVCodecContext *codec_ctx, const Metadata &metadata, const Config &config)
{
    AVFilterGraph *graph = nullptr;
    AVFilterContext *abuffer_ctx = nullptr;
    AVFilterContext *avolume_ctx = nullptr;
    AVFilterContext *asink_ctx = nullptr;
    AVBufferSrcParameters *params = nullptr;
    AVDictionary *dict = nullptr;
    const char *precision = nullptr;
    AVSampleFormat fmt = codec_ctx->sample_fmt;
    double gain = parse_replaygain(metadata, config);
    if (gain == 0.0)
        return nullptr;
    static const AVFilter *abuffer = avfilter_get_by_name("abuffer");
    static const AVFilter *avolume = avfilter_get_by_name("volume");
    static const AVFilter *asink = avfilter_get_by_name("abuffersink");
    if (!abuffer || !avolume || !asink)
        return nullptr;

    if (!(graph = avfilter_graph_alloc()))
        return nullptr;

    if (!(abuffer_ctx = avfilter_graph_alloc_filter(graph, abuffer, "src"))
        || !(avolume_ctx = avfilter_graph_alloc_filter(graph, avolume, "volume"))
        || !(asink_ctx = avfilter_graph_alloc_filter(graph, asink, "sink")))
        goto fail;

    // abuffer options
    params = av_buffersrc_parameters_alloc();
    params->format = static_cast<int>(fmt);
    params->sample_rate = codec_ctx->sample_rate;
    params->time_base = codec_ctx->time_base;
    av_channel_layout_copy(&params->ch_layout, &codec_ctx->ch_layout);
    av_opt_set_int(abuffer_ctx, "channels", codec_ctx->ch_layout.nb_channels, AV_OPT_SEARCH_CHILDREN);
    av_buffersrc_parameters_set(abuffer_ctx, params);
    av_free(params);

    // avolume options
    av_dict_set(&dict, "volume", fmt::format("{:.6f}", pow(10.0, gain / 20.0)).c_str(), 0);
    static const std::vector<std::pair<std::string, std::vector<AVSampleFormat>>> fmt_map {
        {"fixed", {AV_SAMPLE_FMT_U8, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_U8P, 
                   AV_SAMPLE_FMT_S16P, AV_SAMPLE_FMT_S32P, AV_SAMPLE_FMT_S64, AV_SAMPLE_FMT_S64P}},
        {"float", {AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_FLTP}},
        {"double", {AV_SAMPLE_FMT_DBL, AV_SAMPLE_FMT_DBLP}}
    };
    for (auto it = fmt_map.begin(); it != fmt_map.end() && !precision; ++it) {
        auto it2 = std::find(it->second.begin(), it->second.end(), fmt);
        if (it2 != it->second.end())
            precision = it->first.c_str();
    }
    if (!precision)
        goto fail;

    av_dict_set(&dict, "precision", precision, 0);
    avfilter_init_dict(avolume_ctx, &dict);
    avfilter_init_dict(abuffer_ctx, nullptr);
    avfilter_init_str(asink_ctx, nullptr);

    if (avfilter_link(abuffer_ctx, 0, avolume_ctx, 0)
        || avfilter_link(avolume_ctx, 0, asink_ctx, 0))
        goto fail;
    avfilter_graph_config(graph, nullptr);

    return new VolumeFilter(gain, graph, abuffer_ctx, asink_ctx);
fail:
    avfilter_graph_free(&graph);
    return nullptr;
}

double Transcoder::VolumeFilter::parse_replaygain(const Metadata &metadata, const Config &config)
{
    std::string gain_str;
    std::string peak_str;
    double gain = 0.0;
    double peak = 0.0;
    if (config.rg_mode.get_current_value() == Config::ReplayGainMode::ALBUM
        && metadata.contains("replaygain_album_gain")) {
        gain_str = metadata.get("replaygain_album_gain");
        peak_str = metadata.get("replaygain_album_peak");
    }
    else {
        gain_str = metadata.get("replaygain_track_gain");
        peak_str = metadata.get("replaygain_track_peak");
    }
    if (gain_str.empty())
        return gain;
    if (gain_str.front() == '+')
        gain_str = gain_str.substr(1);
    gain = strtod(gain_str.c_str(), nullptr);
    if (gain == 0.0)
        return gain;
    if (!peak_str.empty()) {
        peak = strtod(peak_str.c_str(), nullptr);
        if (peak < 0.0)
            peak = 0.0;
        else if (peak > 1.0)
            peak = 1.0;
    }

    // Clipping protection
    if (gain > 0.0 && peak > 0.0) {
        double peak_db = 20.0 * log10(peak);
        if (peak_db > 0.0)
            return 0.0;
        if (gain > abs(peak_db))
            gain = abs(peak_db);
    }

    return gain;
}

bool Transcoder::VolumeFilter::adjust(AVFrame *frame)
{
    if (av_buffersrc_add_frame(src_ctx, frame)
        || av_buffersink_get_frame(sink_ctx, frame))
        return false;
    return true;
}

Transcoder::VolumeFilter::~VolumeFilter()
{
    avfilter_graph_free(&graph);
}

bool Transcoder::init_resampler()
{
    int ret;
    ret = swr_alloc_set_opts2(&swr_ctx,
              &out_file.codec_ctx->ch_layout,
              out_file.codec_ctx->sample_fmt,
              out_file.codec_ctx->sample_rate,
              &in_file.codec_ctx->ch_layout,
              in_file.codec_ctx->sample_fmt,
              in_file.codec_ctx->sample_rate,
              0,
              nullptr
          );
    if (ret < 0) {
        ffmpeg_error(ret, "Could not allocate swresample context");
        return false;
    }
    if (in_file.codec_ctx->sample_rate != out_file.codec_ctx->sample_rate
        && config.resampling_engine.get_current_value() == Config::ResamplingEngine::SOXR)
        av_opt_set_int(swr_ctx, "resampler", SWR_ENGINE_SOXR, 0);

    ret = swr_init(swr_ctx);
    if (ret < 0) {
        ffmpeg_error(ret, "Could not initialize swresample context");
        return false;
    }
    return true;
}

bool Transcoder::init_fifo()
{
    fifo = av_audio_fifo_alloc(out_file.codec_ctx->sample_fmt, 
               out_file.codec_ctx->ch_layout.nb_channels, 
               1
           );
    if (!fifo) {
        logger.error("Could not allocate FIFO buffer");
        return false;
    }
    return true;
}

// Decodes a single frame from the input file and places the audio data into the FIFO buffer
bool Transcoder::decode_to_fifo(bool &finished)
{
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;
    uint8_t **out_samples = nullptr;
    int error;
    int nb_out_samples;
    bool ret = false;

    if(!(frame = av_frame_alloc())) {
        logger.error("Failed to allocate input frame");
        return false;
    }

    if (!(pkt = av_packet_alloc())) {
        logger.error("Failed to allocate input packet");
        return false;
    }

    // Read packet from input stream
    bool has_pkts = true;
    bool has_frame = false;
    bool again;
    while (!has_frame && !finished) {
        if (has_pkts) {
            error = av_read_frame(in_file.fmt_ctx, pkt);
            if (error == AVERROR_EOF) {
                has_pkts = false;
                finished = true;
            }
            if (pkt->stream_index != in_file.stream_id) {
                av_packet_unref(pkt);
                continue;
            }
        }

        // Decode packet into a frame
        again = false;
        do {
            error = avcodec_send_packet(in_file.codec_ctx, pkt);
            if (!error || error == AVERROR(EAGAIN)) {
                again = error == AVERROR(EAGAIN);
                error = avcodec_receive_frame(in_file.codec_ctx, frame);
                if (!error)
                    has_frame = true;
                else if (error < 0)
                    goto end;
            }
        } while(again);
        av_packet_unref(pkt);
    }
    if (!frame->nb_samples) { // Sometimes the last frame is empty; this is not a failure
        ret = true;
        goto end;
    }

    // Adjust volume
    if (volume_filter)
        volume_filter->adjust(frame);

    // Convert frame to correct format for encoder
    nb_out_samples = swr_get_out_samples(swr_ctx, frame->nb_samples + (int) swr_get_delay(swr_ctx, in_file.codec_ctx->sample_rate));
    out_samples = new uint8_t*[in_file.codec_ctx->ch_layout.nb_channels];
    if (!out_samples) {
        logger.error("Could not allocate samples buffer");
        goto end;
    }
    memset((void*) out_samples, 0, sizeof(uint8_t*) * (out_file.codec_ctx->ch_layout.nb_channels));
    error = av_samples_alloc(out_samples,
                nullptr,
                out_file.codec_ctx->ch_layout.nb_channels,
                nb_out_samples,
                out_file.codec_ctx->sample_fmt,
                0
            );
    if (error < 0) {
        ffmpeg_error(error, "Failed to allocate converted samples");
        goto end;
    }

    nb_out_samples = swr_convert(swr_ctx,
                        out_samples,
                        nb_out_samples,
                        (const uint8_t**) frame->extended_data,
                        frame->nb_samples
                     );

    // Add the converted samples to the FIFO buffer
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + nb_out_samples)) < 0) {
        ffmpeg_error(error, "Could not reallocate FIFO");
        goto end;
    }
    if (av_audio_fifo_write(fifo, (void**) out_samples, nb_out_samples) < nb_out_samples) {
        logger.error("Could not send decoded samples to FIFO buffer");
        goto end;
    }

    ret = true;
end:
    if (pkt)
        av_packet_free(&pkt);
    if (frame)
        av_frame_free(&frame);
    if (out_samples) {
        av_freep(&out_samples[0]);
        delete[] out_samples;
    }
    return ret;
}

// Encodes a single frame from the FIFO buffer, or the remainder
// of the buffer if the decoder is finished and there is less than a frame left
bool Transcoder::encode_from_fifo()
{
    bool ret = false;
    bool data_present;
    int error;
    AVFrame *frame = nullptr;
    const int nb_samples = std::min(av_audio_fifo_size(fifo), out_file.codec_ctx->frame_size);

    if (!(frame = av_frame_alloc())) {
        logger.error("Could not allocate output frame");
        goto end;
    }
    frame->nb_samples = nb_samples;
    av_channel_layout_copy(&frame->ch_layout, &out_file.codec_ctx->ch_layout);
    frame->format = out_file.codec_ctx->sample_fmt;
    frame->sample_rate = out_file.codec_ctx->sample_rate;
    if ((error = av_frame_get_buffer(frame, 0)) < 0) {
        logger.error("Could not allocated output frame buffer");
        goto end;
    }

    // Read data from the FIFO buffer
    if (av_audio_fifo_read(fifo, (void**) frame->data, nb_samples) < nb_samples) {
        logger.error("Could not read data from FIFO");
        goto end;
    }

    if (!encode_frame(frame, data_present))
        goto end;

    ret = true;

end:
    if (frame)
        av_frame_free(&frame);
    return ret;
}

bool Transcoder::encode_frame(AVFrame *frame, bool &has_data)
{
    int error;
    bool ret = false;
    AVPacket *pkt = nullptr;
    has_data = false;

    // Allocate an output packet and frame and initialize
    if (!(pkt = av_packet_alloc())) {
        logger.error("Failed to allocate output packet");
        goto end;
    }

    // frame is null when flushing the encoder
    if (frame) {
        frame->pts = pts;
        pts += frame->nb_samples;
    }

    error = avcodec_send_frame(out_file.codec_ctx, frame);
    if (error < 0 && error != AVERROR_EOF) {
        ffmpeg_error(error, "Could not send packet to encoder");
        goto end;
    }

    error = avcodec_receive_packet(out_file.codec_ctx, pkt);
    if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
        ret = true;
        goto end;
    }
    else if (error < 0) {
        ffmpeg_error(error, "Could not receive packet from encoder");
        goto end;
    }
    else
        has_data = true;

    if ((error = av_write_frame(out_file.fmt_ctx, pkt)) < 0) {
        ffmpeg_error(error, "Could not write packet to file");
        goto end;
    }

    ret = true;
end:
    if (pkt)
        av_packet_free(&pkt);
    return ret;
}

bool Transcoder::transcode()
{
    bool finished = false;
    logger.info("Transcoding '{}'", in_file.path.string());

    // Initialization
    if (config.copy_metadata) {
        if(!metadata.read(in_file.path))
            logger.error("Failed to read metadata for input file '{}'", in_file.path.string());
        if (out_codec == Codec::OPUS && config.opus.convert_r128)
            metadata.convert_r128();
    }
    {
        std::scoped_lock lock(mutex);
        if (!in_file.open()
            || !out_file.open(out_codec, in_file, config)
            || !init_resampler()
            || !init_fifo()
            || !out_file.write_header(out_codec, config))
            return false;

        if (config.rg_mode.get_current_value() != Config::ReplayGainMode::NONE) {
            volume_filter = std::unique_ptr<VolumeFilter>(VolumeFilter::factory(
                in_file.codec_ctx,
                metadata,
                config
            ));
            volume_filter ? metadata.strip_replaygain() : logger.debug("Could not create ReplayGain volume filter");                
        }
    }
    duration = in_file.duration();

    // Print some helpful debug info to the log
    if (config.get_log_level() == spdlog::level::debug) {
        in_file.debug_info(File::Type::INPUT, logger);
        out_file.debug_info(File::Type::OUTPUT, logger);
        metadata.print(logger);
        if (config.rg_mode.get_current_value() != Config::ReplayGainMode::NONE)
            logger.debug("Volume Adjustment: {}", volume_filter ? fmt::format("{:.2f} dB", volume_filter->get_volume()) : "None");

    }

    // Begin transcoding
    while (1) {
        const int out_frame_size = out_file.codec_ctx->frame_size;

        // Decode into FIFO buffer
        while (av_audio_fifo_size(fifo) < out_frame_size && !finished)
            decode_to_fifo(finished);

        
        // Encode from FIFO buffer
        while (av_audio_fifo_size(fifo) >= out_frame_size 
               || (finished && av_audio_fifo_size(fifo))) {
            if (!encode_from_fifo())
                return false;
        }

        // Flush the encoder
        if (finished) {
            bool has_data = false;
            do {
                if (!encode_frame(nullptr, has_data))
                    return false;
            } while (has_data);
            break;
        }
    }
    out_file.write_trailer();
    if (config.copy_metadata)
        metadata.write(out_file.path, out_codec);

    return true;
}

template<typename... Args>
void Transcoder::ffmpeg_error(int error, fmt::format_string<Args...> fmts, Args&&... args)
{
    logger.error(fmts, std::forward<Args>(args)...);
    if (error) {
        char errbuf[256];
        av_strerror(error, errbuf, sizeof(errbuf));
        logger.error("FFmpeg error: {}", errbuf);
    }
}

Transcoder::~Transcoder()
{
    if (swr_ctx)
        swr_free(&swr_ctx);
    if (fifo)
        av_audio_fifo_free(fifo);
}

Transcoder::WorkerThread::WorkerThread(int id, std::unique_ptr<Transcoder::Job> &initial_job, Sync &sync, std::mutex &mutex, std::condition_variable &cond, 
                                       std::mutex &ffmutex, spdlog::logger &logger, const Config &config)
: id(id), job(std::move(initial_job)), sync(sync), main_mutex(mutex), main_cond(cond), ffmutex(ffmutex), logger(logger), config(config) 
{
    thread = std::move(std::make_unique<std::thread>(&WorkerThread::work, this));
}

bool Transcoder::WorkerThread::place_job(std::unique_ptr<Job> &job)
{
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock() || job_available)
        return false;
    this->job = std::move(job);
    job_available = true;
    cond.notify_all();
    return true;
}

void Transcoder::WorkerThread::work()
{
    std::unique_lock lock(mutex);
    {
        std::scoped_lock main_lock(main_mutex);
        main_cond.notify_all();
    }
    logger.debug("Thread {} successfully initialized", id);

    while(!quit) {
        if (job_available) {
            auto begin = std::chrono::system_clock::now();
            Transcoder transcoder(*job, ffmutex, config, logger);
            bool ret = transcoder.transcode();
            job_available = false;
            auto duration = std::chrono::round<std::chrono::milliseconds>(std::chrono::system_clock::now() - begin);
            double durationc = static_cast<double>(duration.count());
            double speed = transcoder.get_duration() / durationc;

            // Inform the main thread that we have finished transcoding
            {
                std::scoped_lock main_lock(main_mutex);
                if (ret)
                    sync.transcode_finished(speed, durationc);
                else
                    sync.report_transcode_error(transcoder.in_path());
                main_cond.notify_all();
            }
        }

        // Wait until we get a new job from the main thread
        cond.wait_for(lock, std::chrono::seconds(10));
    }
}

bool Transcoder::WorkerThread::wait()
{   
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return false;
    quit = true;
    cond.notify_all();
    lock.unlock();
    if (thread->joinable()) {
        thread->join();
        return true;
    }
    return false;
}
