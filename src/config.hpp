#pragma once

#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <unordered_set>
#include <tuple>
#include <cassert>

#include <QString>
#include <spdlog/spdlog.h>

#include "util.hpp"

#define MP3_DEFAULT_BITRATE 256000
#define MP3_DEFAULT_QUALITY 2
#define AAC_DEFAULT_BITRATE 128000
#define AAC_DEFAULT_QUALITY 5
#define VORBIS_DEFAULT_QUALITY 4
#define OPUS_DEFAULT_BITRATE 128000

struct Config {
    class ComboMapBase {
        public:
            virtual const char* get_title(int i) const = 0;
            virtual size_t size() const = 0;
    };
    template <typename T>
    class ComboMap : public ComboMapBase {
        public:
            virtual const char* get_title(int i) const { return std::get<1>(*(map.begin() + i)); }
            virtual size_t size() const { return map.size(); }
            const QString& get_current_key() const { return std::get<0>(*current); }
            const T& get_current_value() const { return std::get<2>(*current); }
            int get_current_index() const { return (int) (current - map.begin()); }
            void set(const QString &key) { 
                if (key.isEmpty())
                    return;
                auto it = find(key);
                if (it != map.end())
                    current = it;
            }
            void set(int i) { current = map.begin() + i; }
            ComboMap(const std::vector<std::tuple<QString, const char*, T>> &&map, const QString &initial)
            : map(map), current(find(initial)) { assert(current != map.end()); }

        private:
            std::vector<std::tuple<QString, const char*, T>> map;
            typename std::vector<std::tuple<QString, const char*, T>>::iterator current;
            typename std::vector<std::tuple<QString, const char*, T>>::iterator find(const QString &key) {
                return std::find_if(map.begin(), map.end(), [&](const auto &i) { return key == std::get<0>(i); });
            }
    };

    // MP3 settings
    struct MP3 {
        enum Mode {
            CBR,
            VBR
        };
        struct Preset {
            Mode mode;
            int64_t bit_rate;
            int quality;
        };
        int id3v2_version = 3;
        ComboMap<Preset> preset {
            {
                {"vbr_low",     QT_TRANSLATE_NOOP("SettingsMP3", "VBR Low (V6)"),     {Mode::VBR, MP3_DEFAULT_BITRATE, 6}},
                {"vbr_medium",  QT_TRANSLATE_NOOP("SettingsMP3", "VBR Medium (V4)"),  {Mode::VBR, MP3_DEFAULT_BITRATE, 4}},
                {"vbr_high",    QT_TRANSLATE_NOOP("SettingsMP3", "VBR High (V2)"),    {Mode::VBR, MP3_DEFAULT_BITRATE, 2}},
                {"vbr_extreme", QT_TRANSLATE_NOOP("SettingsMP3", "VBR Extreme (V0)"), {Mode::VBR, MP3_DEFAULT_BITRATE, 0}},
                {"cbr_128kbps", QT_TRANSLATE_NOOP("SettingsMP3", "CBR 128 kbps"),     {Mode::CBR, 128000, MP3_DEFAULT_QUALITY}},
                {"cbr_192kbps", QT_TRANSLATE_NOOP("SettingsMP3", "CBR 192 kbps"),     {Mode::CBR, 192000, MP3_DEFAULT_QUALITY}},
                {"cbr_256kbps", QT_TRANSLATE_NOOP("SettingsMP3", "CBR 256 kbps"),     {Mode::CBR, 256000, MP3_DEFAULT_QUALITY}},
                {"cbr_320kbps", QT_TRANSLATE_NOOP("SettingsMP3", "CBR 320 kbps"),     {Mode::CBR, 320000, MP3_DEFAULT_QUALITY}},
            },
            "vbr_medium"
        };

        const Preset& get_preset() const { return preset.get_current_value(); }
    };
    struct AAC {
        enum Mode {
            CBR,
            VBR
        };
        struct FDK {
            struct Preset {
                Mode mode;
                int64_t bit_rate;
                int quality;
            };
            bool afterburner = true;
            ComboMap<Preset> preset {
                {
                    {"vbr_low",     QT_TRANSLATE_NOOP("SettingsAAC", "VBR Low"),      {Mode::VBR, AAC_DEFAULT_BITRATE, 3}},
                    {"vbr_medium",  QT_TRANSLATE_NOOP("SettingsAAC", "VBR Medium"),   {Mode::VBR, AAC_DEFAULT_BITRATE, 4}},
                    {"vbr_high",    QT_TRANSLATE_NOOP("SettingsAAC", "VBR High"),     {Mode::VBR, AAC_DEFAULT_BITRATE, 5}},
                    {"cbr_96kbps",  QT_TRANSLATE_NOOP("SettingsAAC", "CBR 96 kbps"),  {Mode::VBR, 96000, AAC_DEFAULT_QUALITY}},
                    {"cbr_128kbps", QT_TRANSLATE_NOOP("SettingsAAC", "CBR 128 kbps"), {Mode::VBR, 128000, AAC_DEFAULT_QUALITY}},
                    {"cbr_192kbps", QT_TRANSLATE_NOOP("SettingsAAC", "CBR 192 kbps"), {Mode::VBR, 192000, AAC_DEFAULT_QUALITY}},
                    {"cbr_256kbps", QT_TRANSLATE_NOOP("SettingsAAC", "CBR 256 kbps"), {Mode::VBR, 256000, AAC_DEFAULT_QUALITY}},
                },
                "vbr_medium"
            };
            const Preset& get_preset() const { return preset.get_current_value(); }
        };
        struct LAVC {
            struct Preset {
                int64_t bit_rate;
            };
            ComboMap<Preset> preset {
                {
                    {"cbr_96kbps",  QT_TRANSLATE_NOOP("SettingsAAC", "CBR 96 kbps"),  {96000}},
                    {"cbr_128kbps", QT_TRANSLATE_NOOP("SettingsAAC", "CBR 128 kbps"), {128000}},
                    {"cbr_192kbps", QT_TRANSLATE_NOOP("SettingsAAC", "CBR 192 kbps"), {192000}},
                    {"cbr_256kbps", QT_TRANSLATE_NOOP("SettingsAAC", "CBR 256 kbps"), {256000}},
                },
                "cbr_128kbps"
            };
            const Preset& get_preset() const { return preset.get_current_value(); }
        };
        FDK fdk;
        LAVC lavc;
        int profile = FF_PROFILE_AAC_LOW;
    };
    struct OGG {
        int quality = VORBIS_DEFAULT_QUALITY;
    };
    struct Opus {
        struct Preset {
            int64_t bit_rate;
        };
        bool convert_r128 = false;
        int r128_adjustment = 0;
        std::string ext = ".opus";
        ComboMap<Preset> preset {
            {
                {"vbr_64kbps",  QT_TRANSLATE_NOOP("SettingsOpus", "VBR 64 kbps"),  {64000}},
                {"vbr_96kbps",  QT_TRANSLATE_NOOP("SettingsOpus", "VBR 96 kbps"),  {96000}},
                {"vbr_128kbps", QT_TRANSLATE_NOOP("SettingsOpus", "VBR 128 kbps"), {128000}},
                {"vbr_160bps",  QT_TRANSLATE_NOOP("SettingsOpus", "VBR 160 kbps"), {160000}},
                {"vbr_192kbps", QT_TRANSLATE_NOOP("SettingsOpus", "VBR 192 kbps"), {192000}},
                {"vbr_256kbps", QT_TRANSLATE_NOOP("SettingsOpus", "VBR 256 kbps"), {256000}},
            },
            "vbr_128kbps"
        };
        const Preset& get_preset() const { return preset.get_current_value(); }
    };
    struct Encoders {
        bool lame = false;
        bool fdk_aac = false;
        bool lavc_aac = false;
        bool libvorbis = false;
        bool libopus = false;
    };
    enum ReplayGainMode {
        NONE,
        TRACK,
        ALBUM
    };

    ComboMap<ReplayGainMode> rg_mode {
        {
            {"none",  QT_TRANSLATE_NOOP("SettingsTranscoding", "Don't encode ReplayGain"), ReplayGainMode::NONE},
            {"track", QT_TRANSLATE_NOOP("SettingsTranscoding", "Encode track gain"),       ReplayGainMode::TRACK},
            {"album", QT_TRANSLATE_NOOP("SettingsTranscoding", "Encode album gain"),       ReplayGainMode::ALBUM},
        },
        "none"
    };

    ComboMap<spdlog::level::level_enum> log_level {
        {
            {"error", QT_TRANSLATE_NOOP("SettingsGeneral", "Error"), spdlog::level::err},
            {"info",  QT_TRANSLATE_NOOP("SettingsGeneral", "Info"),  spdlog::level::info},
            {"debug", QT_TRANSLATE_NOOP("SettingsGeneral", "Debug"), spdlog::level::debug},
        },
        "error"
    };

    std::vector<std::pair<Codec, std::string>> encoders = {
        {Codec::MP3,    "libmp3lame"},
        {Codec::AAC,    "libfdk_aac"},
        {Codec::VORBIS, "libvorbis"},
        {Codec::OPUS,   "libopus"}
    };

    std::array<FileHandling, 11> file_handling = {{
        { Codec::MP3, Action::COPY, Codec::MP3 },
        { Codec::FLAC, Action::COPY, Codec::MP3 },
        { Codec::VORBIS, Action::COPY, Codec::MP3 },
        { Codec::OPUS, Action::COPY, Codec::MP3 },
        { Codec::AAC, Action::COPY, Codec::MP3 },
        { Codec::ALAC, Action::COPY, Codec::MP3 },
        { Codec::WAV, Action::COPY, Codec::MP3 },
        { Codec::WAVPACK, Action::COPY, Codec::MP3 },
        { Codec::APE, Action::COPY, Codec::MP3 },
        { Codec::MPC, Action::COPY,Codec::MP3 },
        { Codec::WMA, Action::COPY, Codec::MP3 }
    }};

#include <languages.hpp>

    MP3 mp3;
    AAC aac;
    OGG ogg;
    Opus opus;
    Encoders has_encoder;
    std::unordered_set<Codec> codec_support;

    bool copy_metadata = true;
    bool copy_artwork = true;
    bool skip_existing = true;
    bool sync_newer = true;
    bool abort_on_error = false;
    bool clean_dest = false;
    bool copy_nonaudio = false;
    int cpu_threads = 1;
    bool cpu_max_threads = true;
    bool downsample_hi_res = true;
    bool downmix_multichannel = true;
    QString lang;

    void check_encoders();
    std::pair<Action, Codec> get_file_handling(const std::filesystem::path &path) const;
    Codec get_input_codec(Codec out_codec) const;
    std::string get_encoder_name(Codec codec) const;
    bool set_encoder_name(Codec codec, const std::string &encoder);
    std::string get_output_ext(Codec codec, const std::filesystem::path &path) const;
    void load(const QSettings &settings);
    spdlog::level::level_enum get_log_level() const { return log_level.get_current_value(); }
};