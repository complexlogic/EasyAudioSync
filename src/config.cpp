
#include <string>
#include <algorithm>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <thread>

#include <QSettings>
#include <QWidget>
#include <QString>
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

#include "config.hpp"
#include "util.hpp"

#define HAS_ENCODER(e) avcodec_find_encoder_by_name(e) != nullptr

void Config::check_features()
{
    has_feature.lame = HAS_ENCODER("libmp3lame");
    has_feature.fdk_aac = HAS_ENCODER("libfdk_aac");
    has_feature.lavc_aac = HAS_ENCODER("aac");
    has_feature.libvorbis = HAS_ENCODER("libvorbis");
    has_feature.libopus = HAS_ENCODER("libopus");
    if (has_feature.lame)
        codec_support.insert(Codec::MP3);
    if (has_feature.lavc_aac || has_feature.fdk_aac)
        codec_support.insert(Codec::AAC);
    if (has_feature.libvorbis)
        codec_support.insert(Codec::VORBIS);
    if (has_feature.libopus)
        codec_support.insert(Codec::OPUS);

    // Check for SoX resampler
    SwrContext *swr_ctx = nullptr;
    AVChannelLayout ch;
    av_channel_layout_default(&ch, 2);
    if (!swr_alloc_set_opts2(&swr_ctx, &ch, AV_SAMPLE_FMT_S16, 44100, &ch, AV_SAMPLE_FMT_S16, 48000, 0, nullptr)) {
        av_opt_set_int(swr_ctx, "resampler", SWR_ENGINE_SOXR, 0);
        if (swr_init(swr_ctx) == 0) {
            has_feature.soxr = true;
            resampling_engine.add("soxr", "SoX", ResamplingEngine::SOXR);
        }
        swr_free(&swr_ctx);
    }
}

std::pair<Action, Codec> Config::get_file_handling(const std::filesystem::path &path) const
{
    Codec in_codec = codec_from_path(path);
    auto it = std::find_if(file_handling.cbegin(),
        file_handling.cend(),
        [&](const auto &i){return i.in_codec == in_codec; }
    );
    return it == file_handling.end() ? std::make_pair(Action::COPY, Codec::NONE) : std::make_pair(it->action, it->out_codec);
}

Codec Config::get_input_codec(Codec out_codec) const
{
    auto it = std::find_if(file_handling.cbegin(),
        file_handling.cend(),
        [&](const auto &i){return i.out_codec == out_codec;}
    );
    assert(it != file_handling.end());
    return it == file_handling.end() ? Codec::NONE : it->in_codec;
}

// Determine the output file extension based on the output codec, and possibly 
// the input path
std::string Config::get_output_ext(Codec codec, const std::filesystem::path &path) const
{
    static const std::vector<std::pair<Codec, std::string>> codecs = {
        {Codec::MP3,     ".mp3"},
        {Codec::FLAC,    ".flac"},
        {Codec::VORBIS,  ".ogg"},
        {Codec::WMA,     ".wma"},
        {Codec::WAV,     ".wav"},
        {Codec::WAVPACK, ".wv"},
        {Codec::APE,     ".ape"},
        {Codec::MPC,     ".mpc"}
    };

    if (codec == Codec::OPUS)
        return opus.ext;
    if (codec == Codec::AAC || codec == Codec::ALAC) {
        if (filetype_from_path(path) == FileType::MP4)
            return path.extension().string();
        else
            return std::string(".m4a");
    }
    auto it = std::find_if(codecs.begin(), codecs.end(), [&](auto i){return i.first == codec;});
    if (it != codecs.end())
        return it->second;
    else
        return std::string();
}

std::string Config::get_encoder_name(Codec codec) const
{
    auto get_encoder = [=](const std::pair<Codec, std::string> &e){ return e.first == codec; };
    auto it = std::find_if(encoders.begin(), encoders.end(), get_encoder);
    assert(it != encoders.end());
    return it == encoders.end() ? std::string() : it->second;
}

bool Config::set_encoder_name(Codec codec, const std::string &encoder)
{
    auto set_encoder = [=](const std::pair<Codec, std::string> &e){ return e.first == codec; };
    auto it = std::find_if(encoders.begin(), encoders.end(), set_encoder);
    assert(it != encoders.end());
    if (it != encoders.end()) {
        it->second = encoder;
        return true;
    }
    return false;
}

void Config::load(const QSettings &settings)
{
    // General
    skip_existing = settings.value("skip_existing", skip_existing).toBool();
    sync_newer = settings.value("sync_newer", sync_newer).toBool();
    copy_nonaudio = settings.value("copy_nonaudio", copy_nonaudio).toBool();
    clean_dest = settings.value("clean_dest", clean_dest).toBool();
    QString lang = settings.value("language").toString();
    if (!lang.isEmpty()) {
        auto it = std::find_if(langs.begin(),
            langs.end(), 
            [&](const auto &i) { return i.first == lang; }
        );
        if (it != langs.end())
            this->lang = lang;
    }

    log_level.set(settings.value("log_level").toString());
    abort_on_error = settings.value("abort_on_error", abort_on_error).toBool();

    // Transcoding
    copy_metadata = settings.value("copy_metadata", copy_metadata).toBool();
    extended_tags = settings.value("extended_tags", extended_tags).toBool();
    copy_artwork = settings.value("copy_artwork", copy_artwork).toBool();
    resampling_engine.set(settings.value("resampling_engine").toString());
    downsample_hi_res = settings.value("downsample_hi_res", downsample_hi_res).toBool();
    downmix_multichannel = settings.value("downsample_hi_res", downmix_multichannel).toBool();
    rg_mode.set(settings.value("replaygain_mode").toString());
    cpu_max_threads = settings.value("cpu_max_threads", cpu_max_threads).toBool();
    int threads = settings.value("cpu_threads", cpu_threads).toInt();
    int max_threads = std::thread::hardware_concurrency();
    if (threads > max_threads)
        threads = max_threads;
    else if (threads < 1)
        threads = 1;
    cpu_threads = threads;

    // File handling
    static const std::pair<QString, Codec> transcode_inputs[] { 
        {"flac", Codec::FLAC}, {"alac", Codec::ALAC}, {"wv", Codec::WAVPACK}, {"ape", Codec::APE}, 
        {"wav", Codec::WAV}, {"mp3", Codec::MP3}, {"aac", Codec::AAC}, {"vorbis", Codec::VORBIS}, 
        {"opus", Codec::OPUS}, {"mpc", Codec::MPC}, {"mp3", Codec::MP3}, {"aac", Codec::AAC},
        {"vorbis", Codec::VORBIS}, {"opus", Codec::OPUS}
    };
    const std::array<std::pair<QString, Codec>, 4> transcode_outputs {{ 
        {"mp3", Codec::MP3}, {"aac", Codec::AAC}, {"vorbis", Codec::VORBIS}, {"opus", Codec::OPUS}
    }};

    for (const auto &[name, codec] : transcode_inputs) {
        QString val = settings.value("action_" + name).toString();
        Action action = (!val.isEmpty() && val == "transcode") ? Action::TRANSCODE : Action::COPY;
        Codec in_codec = codec; // Clang bugworkaround (capturing structured binding)
        auto it = std::find_if(file_handling.begin(), file_handling.end(), [&](const auto &i) {return i.in_codec == in_codec;});
        if (it != file_handling.end()) {
            it->action = action;
            val = settings.value("transcode_" + name).toString();
            if (!val.isEmpty()) {
                auto it2 = std::find_if(transcode_outputs.cbegin(),
                    transcode_outputs.cend(),
                    [&](const auto &i){ return i.first == val; }
                );
                Codec out_codec = it2 == transcode_outputs.cend() ? Codec::MP3 : it2->second;
                it->out_codec = out_codec;
            }
        }
    }

    // MP3
    mp3.preset.set(settings.value("mp3_preset").toString());
    mp3.id3v2_version = settings.value("mp3_id3v2_version", 3).toInt();

    // AAC
    QString aac_encoder = settings.value("aac_encoder").toString();
    if (aac_encoder.isEmpty())
        set_encoder_name(Codec::AAC, has_feature.fdk_aac ? "libfdk_aac" : "aac");
    else
        set_encoder_name(Codec::AAC, aac_encoder == "libfdk_aac" && has_feature.fdk_aac ? "libfdk_aac" : "aac");
    aac.fdk.preset.set(settings.value("aac_fdk_preset").toString());
    aac.fdk.afterburner = settings.value("aac_fdk_afterburner", aac.fdk.afterburner).toBool();
    aac.lavc.preset.set(settings.value("aac_lavc_preset").toString());

    // Ogg
    int ogg_quality = settings.value("ogg_quality", ogg.quality).toInt();
    if (ogg_quality >= -1 && ogg_quality <= 10)
        ogg.quality = ogg_quality;

    // Opus
    opus.preset.set(settings.value("opus_preset").toString());
    QString opus_ext = settings.value("opus_ext").toString();
    if (!opus_ext.isEmpty())
        opus.ext = opus_ext == ".ogg" ? ".ogg" : ".opus";
    opus.convert_r128 = settings.value("opus_convert_r128", opus.convert_r128).toBool();
    int r128_adjustment = settings.value("opus_r128_adjustment", opus.r128_adjustment).toInt();
    if (abs(r128_adjustment) <= 10)
        opus.r128_adjustment = r128_adjustment;

}
