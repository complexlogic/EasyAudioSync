#include <string>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <string.h>

#include <taglib/tstring.h>
#include <taglib/tstringlist.h>
#include <taglib/apefile.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mpegfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/opusfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>
#include <taglib/fileref.h>
#include <taglib/id3v2tag.h>
#include <taglib/xiphcomment.h>
#include <taglib/mp4tag.h>
#include <taglib/apetag.h>
#include <taglib/textidentificationframe.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/uniquefileidentifierframe.h>
#include <taglib/commentsframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/flacpicture.h>
#include <fmt/core.h>

#include "main.hpp"
#include "transcode.hpp"
#include "metadata.hpp"
#include "config.hpp"
#include "util.hpp"
#include <easync.hpp>

#define ITUNES_ATOM "----:com.apple.iTunes:"
#define ENCODER_STRING PROJECT_NAME " " PROJECT_VERSION
#define FORMAT_PAIR(p) fmt::format("{}{}", p->first, p->second ? fmt::format("/{}", p->second) : "")
#define FORMAT_DATE(d) fmt::format("{:04d}{}", d->tm_year, d->tm_mon ? fmt::format("-{:02d}{}", d->tm_mon, d->tm_mday ? fmt::format("-{:02d}", d->tm_mday) : "") : "")

/* We are going to copy MusicBrainz Picard's internal metadata data structure and tag mapping
*  See: https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html
*
*  The following tags have NOT been implemented
*    - encodedby and encodedsettings
*    - Any type of rating
*    - originaldate
*
*  The following tags are implemeted outside the map, due to complexities
*  in value formatting between the metadata types:
*    - discnumber and totaldiscs
*    - tracknumber and totaltracks
*    - date
*/

static const std::unordered_map<std::string, Metadata::Tag> supported_tags = {
    {"acoustid_id", {{{"TXXX", ""}, {"Acoustid Id"}}, "ACOUSTID_ID", "ACOUSTID_ID", {ITUNES_ATOM "Acoustid Id"}}},
    {"acoustid_fingerprint", {{{"TXXX", ""}, {"Acoustid Fingerprint"}}, "ACOUSTID_FINGERPRINT", "ACOUSTID_FINGERPRINT", {ITUNES_ATOM "Acoustid Fingerprint"}}},
    {"album", {{{"TALB", ""}, {}}, "ALBUM", "Album", {"\251alb"}}},
    {"albumartist", {{{"TPE2", ""}, {}}, "ALBUMARTIST", "Album Artist", {"aART"}}},
    {"albumartistsort", {{{"TSO2", ""}, {}}, "ALBUMARTISTSORT", "ALBUMARTISTSORT", {"soaa"}}},
    {"arranger", {{{"IPLS", "TIPL"}, {"arranger"}}, "ARRANGER", "Arranger", {}}},
    {"artist", {{{"TPE1", ""}, {}}, "ARTIST", "Artist", {"\251ART"}}},
    {"artistsort", {{{"TSOP", ""}, {}}, "ARTISTSORT", "ARTISTSORT", {"soar"}}},
    {"artists", {{{"TXXX", ""}, {"ARTISTS"}}, "ARTISTS", "Artists", {ITUNES_ATOM "ARTISTS"}}},
    {"asin", {{{"TXXX", ""}, {"ASIN"}}, "ASIN", "ASIN", {ITUNES_ATOM "ASIN"}}},
    {"barcode", {{{"TXXX", ""}, {"BARCODE"}}, "BARCODE", "Barcode", {ITUNES_ATOM "BARCODE"}}},
    {"bpm", {{{"TBPM", ""}, {}}, "BPM", "BPM", {"tmpo"}}},
    {"catalognumber", {{{"TXXX", ""}, {"CATALOGNUMBER"}}, "CATALOGNUMBER", "CatalogNumber", {ITUNES_ATOM "CATALOGNUMBER"}}},
    {"comment", {{{"COMM", ""}, {"description"}}, "COMMENT", "Comment", {"\251cmt"}}},
    {"compilation", {{{"TCMP", ""}, {}}, "COMPILATION", "Compilation", {"cpil"}}},
    {"composer", {{{"TCOM", ""}, {}}, "COMPOSER", "Composer", {"\251wrt"}}},
    {"composersort", {{{"TSOC", ""}, {}}, "COMPOSERSORT", "COMPOSERSORT", {"soco"}}},
    {"conductor", {{{"TPE3", ""}, {}}, "CONDUCTOR", "Conductor", {ITUNES_ATOM "CONDUCTOR"}}},
    {"copyright", {{{"TCOP", ""}, {}}, "COPYRIGHT", "Copyright", {"cprt"}}},
    {"director", {{{"TXXX", ""}, {"DIRECTOR"}}, "DIRECTOR", "Director", {"\251dir"}}},
    {"discsubtitle", {{{"", "TSST"}, {}}, "DISCSUBTITLE", "DiscSubtitle", {ITUNES_ATOM "DISCSUBTITLE"}}},
    {"engineer", {{{"IPLS", "TIPL"}, {"engineer"}}, "ENGINEER", "Engineer", {ITUNES_ATOM "ENGINEER"}}},
    {"gapless", {{{"", ""}, {}}, "", "", {"pgap"}}},
    {"genre", {{{"TCON", ""}, {}}, "GENRE", "Genre", {"\251gen"}}},
    {"grouping", {{{"TIT1", ""}, {}}, "GROUPING", "Grouping", {"\251grp"}}},
    {"key", {{{"TKEY", ""}, {}}, "KEY", "KEY", {ITUNES_ATOM "initialkey"}}},
    {"language", {{{"TLAN", ""}, {}}, "LANGUAGE", "Language", {ITUNES_ATOM "LANGUAGE"}}},
    {"license", {{{"WCOP", ""}, {}}, "LICENSE", "LICENSE", {ITUNES_ATOM "LICENSE"}}},
    {"lyricist", {{{"TEXT", ""}, {}}, "LYRICIST", "Lyricist", {ITUNES_ATOM "LYRICIST"}}},
    {"lyrics", {{{"USLT", ""}, {}}, "LYRICS", "Lyrics", {"\251lyr"}}},
    {"media", {{{"TMED", ""}, {}}, "MEDIA", "Media", {ITUNES_ATOM "MEDIA"}}},
    {"djmixer", {{{"IPLS", "TIPL"}, {"DJ-mix"}}, "DJMIXER", "DJMixer", {ITUNES_ATOM "DJMIXER"}}},
    {"mixer", {{{"IPLS", "TIPL"}, {"mix"}}, "MIXER", "Mixer", {ITUNES_ATOM "MIXER"}}},
    {"mood", {{{"", "TMOO"}, {}}, "MOOD", "Mood", {ITUNES_ATOM "MOOD"}}},
    {"movement", {{{"MVNM", ""}, {}}, "MOVEMENTNAME", "MOVEMENTNAME", {"\251mvn"}}},
    {"movementotal", {{{"MVIM", ""}, {}}, "MOVEMENTTOTAL", "MOVEMENTTOTAL", {"mvc"}}},
    {"movementnumber", {{{"MVIN", ""}, {}}, "MOVEMENT", "MOVEMENT", {"mvi"}}},
    {"musicbrainz_artistid", {{{"TXXX", ""}, {"MusicBrainz Artist Id"}}, "MUSICBRAINZ_ARTISTID", "MUSICBRAINZ_ARTISTID", {ITUNES_ATOM "MusicBrainz Artist Id"}}},
    {"musicbrainz_discid", {{{"TXXX", ""}, {"MusicBrainz Disc Id"}}, "MUSICBRAINZ_DISCID", "MUSICBRAINZ_DISCID", {ITUNES_ATOM "MusicBrainz Disc Id"}}},
    {"musicbrainz_originalartistid", {{{"TXXX", ""}, {"MusicBrainz Original Artist Id"}}, "MUSICBRAINZ_ORIGINALARTISTID", "MUSICBRAINZ_ORIGINALARTISTID", {ITUNES_ATOM "MusicBrainz Original Artist Id"}}},
    {"musicbrainz_originalreleaseid", {{{"TXXX", ""}, {"MusicBrainz Original Release Id"}}, "MUSICBRAINZ_ORIGINALRELEASEID", "MUSICBRAINZ_ORIGINALRELEASEID", {ITUNES_ATOM "MusicBrainz Original Release Id"}}},
    {"musicbrainz_recordingid", {{{"UFID", ""}, {"http://musicbrainz.org"}}, "MUSICBRAINZ_TRACKID", "MUSICBRAINZ_TRACKID", {ITUNES_ATOM "MusicBrainz Track Id"}}},
    {"musicbrainz_albumartistid", {{{"TXXX", ""}, {"MusicBrainz Album Artist Id"}}, "MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ_ALBUMARTISTID", {ITUNES_ATOM "MusicBrainz Album Artist Id"}}},
    {"musicbrainz_releasegroupid", {{{"TXXX", ""}, {"MusicBrainz Release Group Id"}}, "MUSICBRAINZ_RELEASEGROUPID", "MUSICBRAINZ_RELEASEGROUPID", {ITUNES_ATOM "MusicBrainz Release Group Id"}}},
    {"musicbrainz_albumid", {{{"TXXX", ""}, {"MusicBrainz Album Id"}}, "MUSICBRAINZ_ALBUMID", "MUSICBRAINZ_ALBUMID", {ITUNES_ATOM "MusicBrainz Album Id"}}},
    {"musicbrainz_trackid", {{{"TXXX", ""}, {"MusicBrainz Release Track Id"}}, "MUSICBRAINZ_RELEASETRACKID", "MUSICBRAINZ_RELEASETRACKID", {ITUNES_ATOM "MusicBrainz Release Track Id"}}},
    {"musicbrainz_workid", {{{"TXXX", ""}, {"MusicBrainz Work Id"}}, "MUSICBRAINZ_WORKID", "MUSICBRAINZ_WORKID", {ITUNES_ATOM "MusicBrainz Work Id"}}},
    {"musicip_fingerprint", {{{"TXXX", ""}, {"MusicMagic Fingerprint"}}, "FINGERPRINT", "", {ITUNES_ATOM "fingerprint"}}},
    {"musicip_puid", {{{"TXXX", ""}, {"MusicIP PUID"}}, "MUISCIP_PUID", "MUSICIP_PUID", {ITUNES_ATOM "MusicIP_PUID"}}},
    {"originalalbum", {{{"TOAL", ""}, {}}, "", "", {}}},
    {"originalartist", {{{"TOPE", ""}, {}}, "", "Original Artist", {}}},
    {"originaldate", {{{"TORY", "TDOR"}, {}}, "ORIGINALDATE", "", {}}},
    {"originalyear", {{{"", ""}, {}}, "ORIGINALYEAR", "ORIGINALYEAR", {}}},
    {"performer", {{{"IPLS", "TCML"}, {"instrument"}}, "PERFORMER", "Performer", {}}},
    {"podcast", {{{"", ""}, {}}, "", "", {"pcst"}}},
    {"podcasturl", {{{"", ""}, {}}, "", "", {"purl"}}},
    {"producer", {{{"IPLS", "TCML"}, {"producer"}}, "PRODUCER", "Producer", {ITUNES_ATOM "PRODUCER"}}},
    {"label", {{{"TPUB", ""}, {}}, "LABEL", "Label", {ITUNES_ATOM "LABEL"}}},
    {"releasecountry", {{{"TXXX", ""}, {"MusicBrainz Album Release Country"}}, "RELEASECOUNTRY", "RELEASECOUNTRY", {ITUNES_ATOM "MusicBrainz Album Release Country"}}},
    {"releasestatus", {{{"TXXX", ""}, {"MusicBrainz Album Status"}}, "RELEASESTATUS", "MUSICBRAINZ_ALBUMSTATUS", {ITUNES_ATOM "MusicBrainz Album Status"}}},
    {"releasetype", {{{"TXXX", ""}, {"MusicBrainz Album Type"}}, "RELEASETYPE", "MUSICBRAINZ_ALBUMTYPE", {ITUNES_ATOM "MusicBrainz Album Type"}}},
    {"remixer", {{{"TPE4", ""}, {}}, "REMIXER", "MixArtist", {ITUNES_ATOM "REMIXER"}}},
    {"r128_track_gain", {{{"", ""}, {}}, "R128_TRACK_GAIN", "", {}}},
    {"r128_album_gain", {{{"", ""}, {}}, "R128_ALBUM_GAIN", "", {}}},
    {"replaygain_album_gain", {{{"TXXX", ""}, {"REPLAYGAIN_ALBUM_GAIN", "replaygain_album_gain"}}, "REPLAYGAIN_ALBUM_GAIN", "REPLAYGAIN_ALBUM_GAIN", {ITUNES_ATOM "REPLAYGAIN_ALBUM_GAIN", ITUNES_ATOM "replaygain_album_gain"}}},
    {"replaygain_album_peak", {{{"TXXX", ""}, {"REPLAYGAIN_ALBUM_PEAK", "replaygain_album_peak"}}, "REPLAYGAIN_ALBUM_PEAK", "REPLAYGAIN_ALBUM_PEAK", {ITUNES_ATOM "REPLAYGAIN_ALBUM_PEAK", ITUNES_ATOM "replaygain_album_peak"}}},
    {"replaygain_track_gain", {{{"TXXX", ""}, {"REPLAYGAIN_TRACK_GAIN", "replaygain_track_gain"}}, "REPLAYGAIN_TRACK_GAIN", "REPLAYGAIN_TRACK_GAIN", {ITUNES_ATOM "REPLAYGAIN_TRACK_GAIN", ITUNES_ATOM "replaygain_track_gain"}}},
    {"replaygain_track_peak", {{{"TXXX", ""}, {"REPLAYGAIN_TRACK_PEAK", "replaygain_track_peak"}}, "REPLAYGAIN_TRACK_PEAK", "REPLAYGAIN_TRACK_PEAK", {ITUNES_ATOM "REPLAYGAIN_TRACK_PEAK", ITUNES_ATOM "replaygain_track_peak"}}},
    {"script", {{{"TXXX", ""}, {"SCRIPT"}}, "SCRIPT", "Script", {ITUNES_ATOM "SCRIPT"}}},
    {"show", {{{"", ""}, {}}, "", "", {"tvsh"}}},
    {"showsort", {{{"", ""}, {}}, "", "", {"sosn"}}},
    {"showmovement", {{{"TXXX", ""}, {"SHOWMOVEMENT"}}, "SHOWMOVEMENT", "SHOWMOVEMENT", {ITUNES_ATOM "shwm"}}},
    {"subtitle", {{{"TIT3", ""}, {}}, "SUBTITLE", "Subtitle", {ITUNES_ATOM "SUBTITLE"}}},
    {"title", {{{"TIT2", ""}, {}}, "TITLE", "Title", {"\251nam"}}},
    {"titlesort", {{{"TSOT", ""}, {}}, "TITLESORT", "TITLESORT", {"sonm"}}},
    {"website", {{{"WOAR", ""}, {}}, "WEBSITE", "Weblink", {}}},
    {"work", {{{"TIT1", ""}, {}}, "WORK", "WORK", {"\251wrk"}}},
    {"writer", {{{"TXXX", ""}, {"Writer"}}, "WRITER", "Writer", {}}}
};

std::string Metadata::get(const std::string &key) const
{
    auto it = tag_map.find(key);
    assert(it != tag_map.end());
    return it == tag_map.end() ? std::string() : it->second.front().to8Bit(true);
}

bool Metadata::read(const std::filesystem::path &path)
{
    TagLib::File *file = nullptr;
    TagLib::FileRef *ref_file = nullptr;
    TagLib::MPEG::File *mpeg_file = nullptr;
    TagLib::FLAC::File *flac_file = nullptr;
    TagLib::MP4::File *mp4_file = nullptr;
    TagLib::APE::File *ape_file = nullptr;
    TagLib::RIFF::WAV::File *wav_file = nullptr;
    TagLib::WavPack::File *wv_file = nullptr;

    TagLib::APE::Tag *ape_tag = nullptr;
    TagLib::ID3v2::Tag *id3v2_tag = nullptr;
    TagLib::MP4::Tag *mp4_tag = nullptr;
    TagLib::Ogg::XiphComment *xiph_tag = nullptr;

    FileType file_type = filetype_from_path(path);
    switch (file_type) {
        case FileType::MP3:
            file = mpeg_file = new TagLib::MPEG::File(path.string().c_str(), false);
            if (mpeg_file)
                id3v2_tag = mpeg_file->ID3v2Tag(false);
            break;

        case FileType::FLAC:
            file = flac_file = new TagLib::FLAC::File(path.string().c_str(), false);
            if (flac_file)
                xiph_tag = flac_file->xiphComment(false);
            break;

        case FileType::MP4:
            file = mp4_file = new TagLib::MP4::File(path.string().c_str(), false);
            if (mp4_file)
                mp4_tag = mp4_file->tag();
            break;

        case FileType::APE:
            file = ape_file = new TagLib::APE::File(path.string().c_str(), false);
            if (ape_file)
                ape_tag = ape_file->APETag();
            break;

        case FileType::WAVPACK:
            file = wv_file = new TagLib::WavPack::File(path.string().c_str(), false);
            if (wv_file)
                ape_tag = wv_file->APETag(false);
            break;

        case FileType::WAV:
            file = wav_file = new TagLib::RIFF::WAV::File(path.string().c_str(), false);
            if (wav_file)
                id3v2_tag = wav_file->ID3v2Tag();
            break;
        
        case FileType::OGG:
        case FileType::OPUS:
            ref_file = new TagLib::FileRef(path.string().c_str(), false);
            if (ref_file)
                xiph_tag = dynamic_cast<TagLib::Ogg::XiphComment*>(ref_file->tag());
            break;

        default:
            assert(0);
            return false;
    }

    if (id3v2_tag)
        read_tags(id3v2_tag);
    else if (xiph_tag)
        read_tags(xiph_tag, flac_file ? flac_file->pictureList() : xiph_tag->pictureList());
    else if (mp4_tag)
        read_tags(mp4_tag);
    else if (ape_tag)
        read_tags(ape_tag);

    delete file;
    delete ref_file;
    return tag_map.size() > 0;
}

bool Metadata::read_tags(const TagLib::Ogg::XiphComment *tag, const TagLib::List<TagLib::FLAC::Picture*> &pictures)
{

    // Manual cases
    parse_intpair(tag, "TRACKNUMBER", "TRACKTOTAL", track);
    parse_intpair(tag, "DISCNUMBER", "DISCTOTAL", disc);
    parse_date(tag, "DATE", date);

    const auto &map = tag->fieldListMap();
    for (const auto& [key, tags] : supported_tags) {
        if (tags.vorbis.isEmpty())
            continue;
        const TagLib::StringList &list = map[tags.vorbis];
        if (!list.isEmpty())
            tag_map[key] = list;
    }

    if (config.copy_artwork) {
        const auto find_pic = [](const auto &picture){ return picture->type() == TagLib::FLAC::Picture::Type::FrontCover; };
        auto it = std::find_if(pictures.begin(), pictures.end(), find_pic);
        if (it != pictures.end())
            album_art = std::make_unique<AlbumArt>((*it)->data(), (*it)->mimeType());
    }
    return true;
}

bool Metadata::read_tags(const TagLib::ID3v2::Tag *tag)
{
    parse_intpair(tag, "TRCK", track);
    parse_intpair(tag, "TPOS", disc);
    parse_date(tag, date);

    const auto &map = tag->frameListMap();
    const auto it_txxx = map.find("TXXX");
    TagLib::ID3v2::FrameList txxx_frames = it_txxx != map.end() ? it_txxx->second : TagLib::ID3v2::FrameList();
    for (const auto& [key, tags] : supported_tags) {
        for (int i = 0; i < 2; i++) {
            if (tags.id3v2.frame_ids[i].isEmpty())
                continue;

            // User text frames
            if (tags.id3v2.frame_ids[i] == "TXXX") {
                bool found = false;
                for (const auto &frame : txxx_frames) {
                    const auto txxx_frame = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(frame);
                    if (!txxx_frame)
                        continue;
                    
                    for (const auto &desc : tags.id3v2.descs) {
                        if (txxx_frame->description() == desc) {
                            tag_map[key] = txxx_frame->toString();
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        break;
                }
            }
            else if (tags.id3v2.frame_ids[i] == "UFID")
                read_frame<TagLib::ID3v2::UniqueFileIdentifierFrame>(key, map, tags.id3v2.frame_ids[i]);
            else if (tags.id3v2.frame_ids[i] == "USLT")
                read_frame<TagLib::ID3v2::UnsynchronizedLyricsFrame>(key, map, tags.id3v2.frame_ids[i]);
            else if (tags.id3v2.frame_ids[i] == "COMM")
                read_frame<TagLib::ID3v2::CommentsFrame>(key, map, tags.id3v2.frame_ids[i]);

            // Standard text frames
            else {
                const auto it = map.find(tags.id3v2.frame_ids[i].data(TagLib::String::Type::Latin1));
                if (it == map.end())
                    continue;
                const auto &frames = it->second;
                if (tags.id3v2.descs.empty())
                    tag_map[key] = frames.front()->toString();
                else {
                    for (const auto &frame : frames) {
                        const auto tframe = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame*>(frame);
                        if (!tframe)
                            continue;
                        TagLib::StringList list = tframe->fieldList();
                        if (list.front() == tags.id3v2.descs.front()) {
                            list.erase(list.begin());
                            tag_map[key] = list;
                        }
                    }
                }
            }
        }
    }

    if (config.copy_artwork) {
        const auto it_pic = map.find("APIC");
        if (it_pic != map.end()) {
            const auto &pictures = it_pic->second;
            for (const auto &picture : pictures) {
                const auto frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(picture);
                if (!frame || frame->type() != TagLib::ID3v2::AttachedPictureFrame::Type::FrontCover)
                    continue;
                album_art = std::make_unique<AlbumArt>(frame->picture(), frame->mimeType());
                break;
            }
        }
    }

    return true;
}

template <typename T>
void Metadata::read_frame(const std::string &key, const TagLib::ID3v2::FrameListMap &map, const TagLib::String &frame_id)
{
    const auto it = map.find(frame_id.data(TagLib::String::Type::Latin1));
    if (it != map.end() && !it->second.isEmpty()) {
        const auto frame = it->second.front();
        auto subframe = dynamic_cast<T*>(frame);
        if (subframe)
            tag_map[key] = subframe->toString();
    }
}

bool Metadata::read_tags(TagLib::MP4::Tag *tag)
{

    parse_intpair(tag, "trkn", track);
    parse_intpair(tag, "disk", disc);
    parse_date(tag, "\251day", date);

    const auto &map = tag->itemMap();
    for (const auto &[key, tags] : supported_tags) {
        for (const auto &tag : tags.mp4) {
            const auto &it = map.find(tag);
            if (it != map.end()) {
                tag_map[key] = it->second.toStringList();
                break;
            }
        }
    }

    if (config.copy_artwork) {
        const auto &it = map.find("covr");
        if (it != map.end()) {
            auto list = it->second.toCoverArtList();
            auto find_pic = [](const auto &c) {
                auto fmt = c.format();
                return fmt == TagLib::MP4::CoverArt::Format::JPEG || fmt == TagLib::MP4::CoverArt::Format::PNG; 
            };
            const auto it_art = std::find_if(list.begin(), list.end(), find_pic);
            if (it_art != list.end())
                album_art = std::make_unique<AlbumArt>(it_art->data(), 
                                it_art->format() == TagLib::MP4::CoverArt::JPEG ? "image/jpg" : "image/png"
                            );
        }
    }
    return true;
}

bool Metadata::read_tags(TagLib::APE::Tag *tag)
{
    parse_intpair(tag, "TRACK", track);
    parse_intpair(tag, "DISC", disc);
    parse_date(tag, "YEAR", date);

    const auto &map = tag->itemListMap();
    for (const auto &[key, tags] : supported_tags) {
        if (tags.ape.isEmpty())
            continue;
        const auto &item = map[tags.ape.upper()];
        if (item.isEmpty())
            continue;
        tag_map[key] = item.toString();
    }
    if (config.copy_artwork) {
        const auto &item = map["COVER ART (FRONT)"];
        if (!item.isEmpty() && item.type() == TagLib::APE::Item::ItemTypes::Binary) {
            TagLib::ByteVector data = item.binaryData();
            int offset = data.find('\0');
            if (offset != -1) {
                offset++; // Skip over null byte
                TagLib::String mime;
                const char *mime_bin = data.data() + offset;
                if (!strncmp(mime_bin, "\xFF\xD8\xFF", 3))
                    mime = "image/jpeg";
                else if (!strncmp(mime_bin, "\x89\x50\x4E\x47", 4))
                    mime = "image/png";
                if (!mime.isEmpty())
                    album_art = std::make_unique<AlbumArt>(TagLib::ByteVector(data, offset, data.size() - offset), mime);
            }
        }
    }
    return true;
}

void Metadata::parse_intpair(const TagLib::Ogg::XiphComment *tag, 
                             const TagLib::String &first_key, 
                             const TagLib::String &second_key, 
                             std::unique_ptr<std::pair<int, int>> &ptr)
{
    const auto &map = tag->fieldListMap();
    const TagLib::StringList &first = map[first_key];
    const TagLib::StringList &second = map[second_key];
    int ifirst = 0;
    int isecond = 0;
    if (!first.isEmpty()) {
        ifirst = atoi(first.front().toCString());
        if (!second.isEmpty())
            isecond = atoi(second.front().toCString());
        ptr = std::make_unique<std::pair<int, int>>(ifirst, isecond);
    }
}

void Metadata::parse_intpair(const TagLib::ID3v2::Tag *tag, const TagLib::ByteVector &frame_id, std::unique_ptr<std::pair<int, int>> &ptr)
{
    const auto &map = tag->frameListMap();
    const auto it = map.find(frame_id);
    if (it == map.end() || it->second.isEmpty())
        return;

    auto frame = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame*>(it->second.front());
    if (!frame)
        return;
    parse_intpair_sep(frame->toString().data(TagLib::String::Type::Latin1), ptr);
}

void Metadata::parse_intpair(const TagLib::MP4::Tag *tag, const TagLib::String &key, std::unique_ptr<std::pair<int, int>> &ptr)
{
    const auto &map = tag->itemMap();
    const auto it = map.find(key);
    if (it == map.end())
        return;
    const TagLib::MP4::Item &item = it->second;
    TagLib::MP4::Item::IntPair pair = item.toIntPair();
    ptr = std::make_unique<std::pair<int, int>>(pair.first, pair.second);
}

void Metadata::parse_intpair(const TagLib::APE::Tag *tag, const TagLib::String &key, std::unique_ptr<std::pair<int, int>> &ptr)
{
    const auto &map = tag->itemListMap();
    auto it = map.find(key);
    if (it == map.end())
        return;
    parse_intpair_sep(it->second.toString().data(TagLib::String::Latin1), ptr);
}

void Metadata::parse_intpair_sep(const TagLib::ByteVector &value, std::unique_ptr<std::pair<int, int>> &ptr)
{
    if (value.isEmpty())
        return;
    int sep = value.find("/");
    if (sep == -1) {
        ptr = std::make_unique<std::pair<int, int>>(atoi(value.data()), 0);
        return;
    }
    int first = atoi(TagLib::ByteVector(value, 0, sep).data());
    int second = atoi(value.data() + sep + 1);
    ptr = std::make_unique<std::pair<int, int>>(first, second);
}

void Metadata::parse_date(const TagLib::Ogg::XiphComment *tag, const TagLib::String &key, std::unique_ptr<std::tm> &ptr)
{
    const auto &map = tag->fieldListMap();
    const auto it = map.find(key);
    if (it == map.end())
        return;
    parse_iso8601(it->second.front().data(TagLib::String::Latin1), ptr);
}

void Metadata::parse_date(const TagLib::ID3v2::Tag *tag, std::unique_ptr<std::tm> &ptr)
{
    const auto &map = tag->frameListMap();
    const TagLib::ByteVector frame_ids[] {
        "TDRC",
        "TDAT",
        "TYER"
    };
    std::map<TagLib::ByteVector, TagLib::ID3v2::TextIdentificationFrame*> frames;

    for (const auto &id : frame_ids) {
        const auto it = map.find(id);
        if (it == map.end() || it->second.isEmpty())
            continue;
        auto frame = dynamic_cast<TagLib::ID3v2::TextIdentificationFrame*>(it->second.front());
        if (frame)
            frames[id] = frame;
    }
    if (frames.empty())
        return;
    ptr = std::make_unique<std::tm>(std::tm {});

    // Assume ID3v2.4 source if the TDRC frame is present
    const auto tdrc = frames.find("TDRC");
    if (tdrc != frames.end())
        parse_iso8601(tdrc->second->toString().data(TagLib::String::Latin1), ptr);
    else {
        const auto tdat = frames.find("TDAT");
        if (tdat != frames.end()) {
            TagLib::ByteVector value = tdat->second->toString().data(TagLib::String::Latin1);
            if (value.size() == 5) { // 4 bytes plus null terminator
                ptr->tm_mon = atoi(TagLib::ByteVector(value, 0, 2).data());
                ptr->tm_mday = atoi(TagLib::ByteVector(value, 2, 2).data());
            }
        }
        const auto tyer = frames.find("TYER");
        if (tyer != frames.end()) {
            TagLib::ByteVector value = tyer->second->toString().data(TagLib::String::Latin1);
            if (value.size() == 5)
                ptr->tm_year = atoi(value.data());
        }
    }
}

void Metadata::parse_date(const TagLib::MP4::Tag *tag, const TagLib::String &key, std::unique_ptr<std::tm> &ptr)
{
    const auto &map = tag->itemMap();
    const auto it = map.find(key);
    if (it == map.end())
        return;
    parse_iso8601(it->second.toStringList().front().data(TagLib::String::Latin1), ptr);
}

void Metadata::parse_date(const TagLib::APE::Tag *tag, const TagLib::String &key, std::unique_ptr<std::tm> &ptr)
{
    const auto &map = tag->itemListMap();
    const auto it = map.find(key);
    if (it == map.end())
        return;
    parse_iso8601(it->second.toString().data(TagLib::String::Latin1), ptr);
}

void Metadata::parse_iso8601(const TagLib::ByteVector &value, std::unique_ptr<std::tm> &ptr)
{
    if (value.isEmpty())
        return;
    ptr = std::make_unique<std::tm>(std::tm {});

    // Year
    int i = value.find('-');
    if (i == -1) {
        ptr->tm_year = atoi(value.data());
        return;
    }
    ptr->tm_year = atoi(TagLib::ByteVector(value.data(), i).data());

    // Month
    int j = value.find('-', i + 1);
    if (j == -1) {
        ptr->tm_mon = atoi(value.data() + i + 1);
        return;
    }
    ptr->tm_mon = atoi(TagLib::ByteVector(value.data(), i + 1, j - i).data());
    
    // Day
    ptr->tm_mday = atoi(value.data() + j + 1);
}

bool Metadata::write(const std::filesystem::path &path, Codec codec)
{
    if (tag_map.empty())
        return false;

    bool ret = false;
    TagLib::File *file = nullptr;
    TagLib::MPEG::File *mpeg_file = nullptr;
    TagLib::MP4::File *mp4_file = nullptr;
    TagLib::Ogg::Vorbis::File *vorbis_file = nullptr;
    TagLib::Ogg::Opus::File *opus_file = nullptr;

    TagLib::ID3v2::Tag *id3v2_tag = nullptr;
    TagLib::MP4::Tag *mp4_tag = nullptr;
    TagLib::Ogg::XiphComment *xiph_tag = nullptr;

    // Open file
    switch (codec) {
        case Codec::MP3:
            file = mpeg_file = new TagLib::MPEG::File(path.string().c_str(), false);
            if (mpeg_file)
                id3v2_tag = mpeg_file->ID3v2Tag(true);
            break;

        case Codec::AAC:
            file = mp4_file = new TagLib::MP4::File(path.string().c_str(), false);
            if (mp4_file)
                mp4_tag = mp4_file->tag();
            break;

        case Codec::VORBIS:
            file = vorbis_file = new TagLib::Ogg::Vorbis::File(path.string().c_str(), false);
            if (vorbis_file)
                xiph_tag = vorbis_file->tag();
            break;

        case Codec::OPUS:
            file = opus_file = new TagLib::Ogg::Opus::File(path.string().c_str(), false);
            if (opus_file)
                xiph_tag = opus_file->tag();
            break;

        default:
            assert(0);
            return false;
    }

    // Write
    if (id3v2_tag)
        write_tags(id3v2_tag);
    else if (xiph_tag)
        write_tags(xiph_tag);
    else if (mp4_tag)
        write_tags(mp4_tag);

    // Save
    if (mpeg_file)
        ret = mpeg_file->save(TagLib::MPEG::File::TagTypes::AllTags, 
                  TagLib::File::StripTags::StripNone,
                  config.mp3.id3v2_version == 3 ? TagLib::ID3v2::Version::v3 : TagLib::ID3v2::Version::v4,
                  TagLib::File::DuplicateTags::DoNotDuplicate
              );
    else
        ret = file->save();

    delete file;
    return ret;
}

bool Metadata::write_tags(TagLib::Ogg::XiphComment *tag)
{
    tag->addField("ENCODEDBY", ENCODER_STRING);

    if (track)
        write_intpair(tag, "TRACKNUMBER", "TRACKTOTAL", track);
    if (disc)
        write_intpair(tag, "DISCNUMBER", "DISCTOTAL", disc);
    if (date)
        tag->addField("DATE", FORMAT_DATE(date));

    for (const auto& [key, value_list] : tag_map) {
        try {
            const auto vorbis_tag = supported_tags.at(key).vorbis;
            for (auto &value : value_list)
                tag->addField(vorbis_tag, value, false);
        }
        catch (...) {}
    }

    if (album_art) {
        auto picture = new TagLib::FLAC::Picture;
        picture->setData(album_art->data);
        picture->setMimeType(album_art->mime);
        picture->setType(TagLib::FLAC::Picture::Type::FrontCover);
        tag->addPicture(picture);
    }
    return true;
}

bool Metadata::write_tags(TagLib::ID3v2::Tag *tag)
{
    auto encoder_frame = new TagLib::ID3v2::TextIdentificationFrame("TENC");
    encoder_frame->setText(ENCODER_STRING);
    tag->addFrame(encoder_frame);
    if (track)
        write_intpair(tag, "TRCK", track);
    if (disc)
        write_intpair(tag, "TPOS", disc);
    if (date)
        write_date(tag, date);

    for (const auto& [key, value] : tag_map) {
        try {
            const auto &id3v2 = supported_tags.at(key).id3v2;
            const TagLib::String &frame_id = (config.mp3.id3v2_version == 4 && !id3v2.frame_ids[1].isEmpty()) ? id3v2.frame_ids[1] : id3v2.frame_ids[0];
            if (frame_id == "TXXX") {
                if (!id3v2.descs.empty()) {
                    auto frame = new TagLib::ID3v2::UserTextIdentificationFrame(id3v2.descs.front(), value);
                    tag->addFrame(frame);
                }
            }
            else if (frame_id == "UFID") {
                if (!id3v2.descs.empty()) {
                    auto frame = new TagLib::ID3v2::UniqueFileIdentifierFrame(id3v2.descs.front(), value.front().data(TagLib::String::Type::Latin1));
                    tag->addFrame(frame);
                }
            }
            else if (frame_id == "USLT") {
                auto frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame();
                frame->setText(value.front());
                tag->addFrame(frame);
            }
            else if (frame_id == "COMM") {
                auto frame = new TagLib::ID3v2::CommentsFrame();
                frame->setText(value.front());
                tag->addFrame(frame);
            }
            else {
                auto frame = new TagLib::ID3v2::TextIdentificationFrame(frame_id.data(TagLib::String::Type::Latin1));
                frame->setText(value);
                tag->addFrame(frame);
            }
        }
        catch (...) {}
    }

    if (album_art) {
         auto frame = new TagLib::ID3v2::AttachedPictureFrame();
         frame->setPicture(album_art->data);
         frame->setMimeType(album_art->mime);
         frame->setType(TagLib::ID3v2::AttachedPictureFrame::Type::FrontCover);
         tag->addFrame(frame);
    }
    return true;
}

bool Metadata::write_tags(TagLib::MP4::Tag *tag)
{
    tag->setItem("\xa9too", TagLib::MP4::Item(TagLib::StringList(ENCODER_STRING)));
    if (track)
        write_intpair(tag, "trkn", track);
    if (disc)
        write_intpair(tag, "disk", disc);
    if (date)
        tag->setItem("\251day", TagLib::MP4::Item(TagLib::String(FORMAT_DATE(date))));

    for (const auto& [key, value] : tag_map) {
        try {
            const auto &mp4_tags = supported_tags.at(key).mp4;
            if (!mp4_tags.empty())
                tag->setItem(mp4_tags.front(), TagLib::MP4::Item(value));
        }
        catch (...) {}
    }

    if (album_art) {
        if (album_art->mime == "image/jpeg" || album_art->mime == "image/png") {
            TagLib::MP4::CoverArt::Format fmt = album_art->mime == "image/jpeg"
                                                    ? TagLib::MP4::CoverArt::Format::JPEG 
                                                    : TagLib::MP4::CoverArt::Format::PNG;
            TagLib::MP4::CoverArtList list;
            list.append(TagLib::MP4::CoverArt(fmt, album_art->data));
            tag->setItem("covr", list);
        }
    }
    return true;
}

void Metadata::write_intpair(TagLib::Ogg::XiphComment *tag, const TagLib::String &first_key, const TagLib::String &second_key, const std::unique_ptr<std::pair<int, int>> &ptr)
{
    tag->addField(first_key, std::to_string(ptr->first));
    tag->addField(second_key, std::to_string(ptr->second));
}

void Metadata::write_intpair(TagLib::ID3v2::Tag *tag, const TagLib::String &key, const std::unique_ptr<std::pair<int, int>> &ptr)
{
    auto frame = new TagLib::ID3v2::TextIdentificationFrame(key.data(TagLib::String::Type::Latin1));
    frame->setText(FORMAT_PAIR(ptr));
    tag->addFrame(frame);
}

void Metadata::write_intpair(TagLib::MP4::Tag *tag, const TagLib::String &key, const std::unique_ptr<std::pair<int, int>> &ptr)
{
    tag->setItem(key, TagLib::MP4::Item(ptr->first, ptr->second));
}

void Metadata::write_date(TagLib::ID3v2::Tag *tag, std::unique_ptr<std::tm> &ptr)
{
    if (config.mp3.id3v2_version == 3) {
        auto tdat = new TagLib::ID3v2::TextIdentificationFrame("TDAT");
        tdat->setText(fmt::format("{:02d}{:02d}", ptr->tm_mday, ptr->tm_mon));
        tag->addFrame(tdat);
        auto tyer = new TagLib::ID3v2::TextIdentificationFrame("TYER");
        tyer->setText(fmt::format("{:04d}", ptr->tm_year));
        tag->addFrame(tyer);
    }
    else if (config.mp3.id3v2_version == 4) {
        auto tdrc = new TagLib::ID3v2::TextIdentificationFrame("TDRC");
        tdrc->setText(FORMAT_DATE(ptr));
        tag->addFrame(tdrc);
    }
}

void Metadata::convert_r128()
{
    std::string gain;
    static const std::array<std::pair<std::string, std::string>, 2> tags = {{
        {"replaygain_track_gain", "r128_track_gain"},
        {"replaygain_album_gain", "r128_album_gain"}
    }};
    for (const auto& [rg_tag, r128_tag] : tags) {
        gain = get(rg_tag);
        if (!gain.empty()) {
            if (gain.front() == '+')
                gain = gain.substr(1);
            double val = strtod(gain.c_str(), nullptr);
            if (config.opus.r128_adjustment)
                val += static_cast<double>(config.opus.r128_adjustment);
            int r128_gain = static_cast<int>(std::round(val * 256.0));
            tag_map[r128_tag] = TagLib::StringList(std::to_string(r128_gain).c_str());
        }
    }
    strip_replaygain();
}

void Metadata::strip_replaygain()
{
    static const std::string rg_tags[] = {
        "replaygain_track_gain",
        "replaygain_track_peak",
        "replaygain_album_gain",
        "replaygain_album_peak"
    };

    for (const auto &tag : rg_tags)
        tag_map.erase(tag);
}

void Metadata::print(spdlog::logger &logger)
{
    logger.debug("Found {} metadata tags:", tag_map.size());
    for (const auto &[key, value] : tag_map)
        logger.debug("  {}: {}", key, value.toString().to8Bit());
    if (track)
        logger.debug("  track: {}", FORMAT_PAIR(track));
    if (disc)
        logger.debug("  disc: {}", FORMAT_PAIR(disc));
    if (date)
       logger.debug("  date: {}", FORMAT_DATE(date));
    logger.debug("Cover art: {}", album_art ? "Yes" : "No");
}
