# Settings
This page contains documentation for Easy Audio Sync's configuration settings.

1. [General](#general)
2. [File Handling](#file-handling)
3. [Transcoding](#transcoding)
4. [MP3](#mp3)
5. [AAC](#aac)
6. [Ogg Vorbis](#ogg-vorbis)
7. [Opus](#opus)

## General
The settings in this section control the general behavior of the program.

#### Skip files that exist in destination
When this setting is enabled, the program will check if each file in the source exists in the destination, and skip syncing files any that do. When transcoding, your [File Handling](#file-handling) settings will be taken into account when checking the destination.

#### Except Files with Older Timestamps
When this setting is enabled and the equivalent source file exists in the destination, the program will compare the files' modification timestamps. If the source file is newer than the destination file, the file will be synced again. This may happen, for example, if you update the metadata on the source file.

#### Copy Non-audio Files
When this setting is enabled, the program will copy all non-audio files in the source to the destination. For example, if you have artwork files, you may want these transferred to the destination along with the audio files.

#### Clean Destination Directory
When this setting is enabled, the program will check if each file in the destination exists in the source, and delete any files that do not. Your [File Handling](#file-handling) settings will be taken into account when checking the source. After deleting files, the program will walk the destination directory tree from the bottom to the top and delete any empty directories.

The files are deleted permanently without any way to recover them. You will not be prompted for confirmation before the files are deleted. **Don't use this setting if your destination contains files other than those created by a previous sync.**

Misuse of this feature can cause unwanted data loss, so it's important to understand how it works before enabling it. When the enable box is checked, the program will present you with a dialog explaining how the feature works and ask you acknowledge by checking a box to proceed.

#### Language
Sets the language of the program

#### Minimum Log Level
Sets the log level of the program. Leave this as "Error", unless you're experiencing issues.

#### Abort on Errors
Causes the sync to stop whenever an error is encountered. Normally, the program will try to continue when errors occur.

## File Handling
The settings in this section determine how files in the source are handled when synced to the destination. The codecs in the leftmost column represent the input files from the source. With the radio buttons, choose whether to copy or transcode files of that particular format. If transcode is selected, pick an output codec from the combo box on the right.

The usual strategy when syncing a library to mobile is to transcode lossless files to a lossy format, and copy files that are already in a lossy format.

## Transcoding
The settings in this section determine how files are transcoded. Here are a few general notes about transcoding:
- Bit rate is the amount of data that is used to encode an audio signal. In "lossy" encoding, more bitrate results in better audio quality, up to a subjective point called "transparency", where a lossy-encoded signal sounds indistinguishable from its source signal. 
- CBR refers to "Constant Bit Rate" and VBR refers to "Variable Bit Rate". 
  - In CBR, the same amount of data is used to encode a signal at all points in time. 
  - In VBR, the amount of data varies based on the complexity of the source at a given point in time.
  - Generally, VBR is superior to CBR because it results in a higher quality at the same average bitrate.
- The bitrate settings in Easy Audio Sync are normalized to stereo (2 channel) files. The actual encoded bitrate will depend on the output channel layout. This normalization system allows the bitrate to scale, resulting in the same quality regardless of the channel layout. For example, if the bit rate is selected as 128 kbps: 
  - If the source is mono, the actual bitrate will be 128 * (1/2) = 64 kbps.
  - If the source is a 5 channel surround, the actual bitrate will be 128 * (5/2) = 320 kbps.

The following settings are available in the transcoding section:

#### Copy Metadata
When enabled, the program will copy the source's metadata "tags" when transcoding. 

There is a significant amount of fragmentation in the audio metadata ecosystem due to vague or non-existent standards. Internally, the program uses the [same tag mappings as MusicBrainz Picard](https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html). The MusicBrainz Picard project is one of the most authoritative references on audio metadata. You will have the best possible results if your source library is tagged with Picard.

There are a few tags that have not been implemented.
- Ratings - due to complexities between the various formats
- Anything related to the input file itself rather than the audio it contains, such as original filename, encoder, encoder settings, etc. It's not appropriate to transfer these when creating a new file
- Original date - again, due to complexities caused by format differences

#### Copy Cover Art
When enabled, the program will copy the source's embedded cover art when transcoding.

#### Downsample High Resolution Audio
When enabled, it sets the maximum sampling rate in output files to 48 kHz:
- If the source sampling rate is 48 kHz or less, the sampling rate will be preserved to the output (assuming it's supported by the encoder).
- If the source sampling rate is greater than 48 kHz, the audio will be downsampled to 48 kHz in the output.

#### Downmix Multi-channel Audio
When enabled, it sets the maximum number of channels in output files to 2:
- If the source has 2 channels or fewer, the channel layout will be preserved to the output
- If the source has more than 2 channels, the audio will be downmixed to 2 channels in the output.

#### Number of CPU Threads
This sets the number of threads that are used to transcode. Can be maximum (provided by OS at runtime), or a specific number. More threads will yield faster transcode times.

#### ReplayGain
This setting reads the ReplayGain metadata tags in the source file, and adjusts the volume  of output file's encoded audio stream by the gain value. Both track level and album level tags are supported. If album level is set but not available in the file, it will fall back to track level. After adjusting the volume, the ReplayGain metadata will be stripped from the output file.

Note that this setting only applies to transcoded files; copied files are not modified in any way. If your [File Handling](#file-handling) settings contain a mixture of transcoding and copying, you will need to take that into account.

### MP3
The settings in this section pertain to transcoding MP3 files

#### Encoder Preset
This setting controls the bitrate of the encoder. Both CBR and VBR are supported. VBR is generally preferable, but there are a few legacy players that support CBR MP3s only.

Refer to the [HydrogenAudio LAME Recommendations](https://wiki.hydrogenaud.io/index.php/LAME#Recommended_encoder_settings) for guidance on the preset selection. Transparency for VBR is usually between V4 and V2, depending on the listener and the environment

#### ID3 Version
If [Copy Metadata](#copy-metadata) is enabled, this setting controls the version of the ID3 tags. There are a few players that still don't support 2.4, so it's recommended to stick with 2.3 unless you have a specific reason to prefer 2.4.

### AAC
The settings in this section pertain to transcoding AAC files.

There are two AAC encoders available: Fraunhofer FDK AAC and libavcodec AAC. Fraunhofer FDK AAC is superior to libavcodec AAC in both quality and speed, and you should use it if it's available to you. The reason libavcodec AAC is supported is because Fraunhofer FDK AAC is less widely available due to legal uncertainties over the Fraunhofer Society's patent assertions. Despite this, the official Windows and macOS builds as well as the Linux Flatpak all include support for the Fraunhofer FDK AAC encoder.

All transcoded AAC files will be of Low Complexity object type (AAC-LC), regardless of the encoder or preset chosen.

#### Encoder
This combo box allows you to select which encoder is used for AAC files. The program checks for the availability at startup. You may not have both encoders available depending on your build configuration.

#### Fraunhofer FDK AAC
The settings in this section control the Fraunhofer FDK AAC encoder

##### Encoder Preset
This setting controls the bitrate of the encoder. Both CBR and VBR are supported. The VBR modes Low, Medium, and High correspond to the encoder VBR modes 3, 4, and 5, respectively. Refer to the [Hydrogen Audio page for Fraunhofer FDK AAC](https://wiki.hydrogenaud.io/index.php?title=Fraunhofer_FDK_AAC#Bitrate_Modes) for more information.

##### Enable Afterburner
The afterburner feature increases encoding quality at the cost of encoding effort. It's recommended to keep this enabled.

#### Libavcodec AAC Encoder Preset
This setting controls the bit rate of the libavcodec AAC encoder. Currently, only CBR mode is available; VBR in the libavcodec AAC encoder is considered experimental.

### Ogg Vorbis
The settings in this section pertain to transcoding Ogg Vorbis files.

#### Encoding Quality
This controls the quality of the encoded audio. The value is on the scale of -1 to 10, with higher values resulting in better quality. Reasonable values are in the range of 3-8. Refer to the [HydrogenAudio Ogg Vorbis recommendations](https://wiki.hydrogenaud.io/index.php?title=Recommended_Ogg_Vorbis#Recommended_Encoder_Settings) for guidance on the quality setting.

### Opus
The settings in this section pertain to transcoding Opus files

#### Encoder Preset
This setting controls the bitrate of the encoder. VBR is supported only. Refer to the [HydrogenAudio Opus page](https://wiki.hydrogenaud.io/index.php?title=Opus) for guidance on bitrate selection.

#### File Extension
This setting controls the file extension of the outputted files. The .opus file extension is preferable, but some older devices support .ogg only.

#### Convert ReplayGain Tags to R128 Format
Opus files are governed by [RFC 7845](https://datatracker.ietf.org/doc/html/rfc7845), which specifies an alternative loudness normalization scheme to ReplayGain where track and album gain are stored in `R128_TRACK_GAIN` and `R128_ALBUM_GAIN` tags, respectively. If you want to follow the RFC 7845 scheme, this setting will convert the format of any ReplayGain tags from the source files.

#### Adjust Gain By
The RFC 7845 requires that the gains are referenced to -23 LUFS. If your source files were not referenced to -23 LUFS, you can use this setting to adjust the gain with a constant value. For example, if your source files are normalized to the ReplayGain 2.0 standard -18 LUFS, you can set this to -5 dB to re-reference them to -23 LUFS.
