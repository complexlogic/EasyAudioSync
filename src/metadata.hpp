#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <ctime>

#include <taglib/tstring.h>
#include <taglib/tstringlist.h>
#include <taglib/tbytevector.h>
#include <taglib/id3v2tag.h>
#include <taglib/xiphcomment.h>
#include <taglib/mp4tag.h>
#include <taglib/apetag.h>
#include <taglib/flacfile.h>
#include <spdlog/spdlog.h>

#include "util.hpp"
struct Config;

class Metadata {
    public: 
        struct Tag {
            struct ID3v2 {
                enum Version {
                    V3,
                    v4
                };
                TagLib::String frame_ids[2];
                std::vector<TagLib::String> descs;
            };

            ID3v2 id3v2;
            TagLib::String vorbis;
            TagLib::String ape;
            std::vector<TagLib::String> mp4;
        };
        struct AlbumArt {
            TagLib::ByteVector data;
            TagLib::String mime;

            AlbumArt(const TagLib::ByteVector &data, const TagLib::String mime) : data(data), mime(mime) {}
        };

        Metadata(const Config &config) : config(config) {}
        bool read(const std::filesystem::path &path);
        bool write(const std::filesystem::path &path, Codec codec);
        std::string get(const std::string &key) const;
        inline bool contains(const std::string &key) const { return tag_map.contains(key); }
        void convert_r128();
        void strip_replaygain();
        void print(spdlog::logger &logger);

    private:
        std::unordered_map<std::string, TagLib::StringList> tag_map;
        std::unique_ptr<AlbumArt> album_art;
        const Config &config;

        std::unique_ptr<std::pair<int, int>> track;
        std::unique_ptr<std::pair<int, int>> disc;
        std::unique_ptr<std::tm> date;

        bool read_tags(const TagLib::Ogg::XiphComment *tag, const TagLib::List<TagLib::FLAC::Picture*> &pictures);
        bool read_tags(const TagLib::ID3v2::Tag *tag);
        template <typename T>
        void read_frame(const std::string &key, const TagLib::ID3v2::FrameListMap &map, const TagLib::String &frame_id);
        bool read_tags(TagLib::MP4::Tag *tag);
        bool read_tags(TagLib::APE::Tag *tag);
        void parse_intpair(const TagLib::Ogg::XiphComment *tag, const TagLib::String &first_key, const TagLib::String &second_key, std::unique_ptr<std::pair<int, int>> &ptr);
        void parse_intpair(const TagLib::ID3v2::Tag *tag, const TagLib::ByteVector &frame_id, std::unique_ptr<std::pair<int, int>> &ptr);
        void parse_intpair(const TagLib::MP4::Tag *tag, const TagLib::String &key, std::unique_ptr<std::pair<int, int>> &ptr);
        void parse_intpair(const TagLib::APE::Tag *tag, const TagLib::String &key, std::unique_ptr<std::pair<int, int>> &ptr);
        void parse_intpair_sep(const TagLib::ByteVector &value, std::unique_ptr<std::pair<int, int>> &ptr);
        void parse_date(const TagLib::Ogg::XiphComment *tag, const TagLib::String &key, std::unique_ptr<std::tm> &ptr);
        void parse_date(const TagLib::ID3v2::Tag *tag, std::unique_ptr<std::tm> &ptr);
        void parse_date(const TagLib::MP4::Tag *tag, const TagLib::String &key, std::unique_ptr<std::tm> &ptr);
        void parse_date(const TagLib::APE::Tag *tag, const TagLib::String &key, std::unique_ptr<std::tm> &ptr);

        void parse_iso8601(const TagLib::ByteVector &value, std::unique_ptr<std::tm> &ptr);

        bool write_tags(TagLib::Ogg::XiphComment *tag);
        bool write_tags(TagLib::ID3v2::Tag *tag);
        bool write_tags(TagLib::MP4::Tag *tag);
        void write_intpair(TagLib::Ogg::XiphComment *tag, const TagLib::String &first_key, const TagLib::String &second_key, const std::unique_ptr<std::pair<int, int>> &ptr);
        void write_intpair(TagLib::ID3v2::Tag *tag, const TagLib::String &key, const std::unique_ptr<std::pair<int, int>> &ptr);
        void write_intpair(TagLib::MP4::Tag *tag, const TagLib::String &key, const std::unique_ptr<std::pair<int, int>> &ptr);
        void write_date(TagLib::ID3v2::Tag *tag, std::unique_ptr<std::tm> &ptr);
};