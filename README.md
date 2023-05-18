# Easy Audio Sync
1. [About](#about)
2. [Screenshots](#screenshots)
3. [Installation](#installation)
4. [Usage](#usage)
5. [Support](#support)

## About
Easy Audio Sync is an audio library syncing and conversion utility. The intended use is syncing an audio library with many lossless files to a mobile device with limited storage.

The program's design is inspired by the [rsync](https://github.com/WayneD/rsync) utility. It supports folder-based source to destination syncing, with added audio transcoding capability, and is GUI-based instead of CLI-based.

### Features
- Custom FFmpeg-based audio transcoding engine
  - Multithreaded operation for fast conversions
  - 4 lossy output codecs supported: MP3, AAC, Ogg Vorbis, and Opus
- Robust metadata parser ensures tags and cover art are correctly transferred when converting between different formats
- ReplayGain volume adjustments
- Destination folder cleaning (deleting files that no longer exist in the source)
- Cross-platform Windows/macOS/Linux

## Screenshots
![Main Window](https://github.com/complexlogic/EasyAudioSync/assets/95071366/e32beb0c-2f07-4b39-a75f-93ed6226c014)

![Settings](https://github.com/complexlogic/EasyAudioSync/assets/95071366/9f4bcc67-995f-4f74-b533-13581e2bbd92)

## Installation

### Windows

### macOS

### Linux

## Usage
Easy Audio Sync operates based on a source folder and a destination folder, where the source folder contains the primary music library, and the destination folder is the desired output location. After selecting the source and destination folders, click the "Sync" button to start the sync. The program will recreate the source's entire subfolder structure in the destination, copying or transcoding files as specified in the settings. See the [settings documentation](docs/settings.md) for help on configuring the program's settings.

The stop button next to the progress bar can be used stop the sync at any time. The worker threads are allowed to finish their current file operation before quitting to avoid leaving corrupted files in the destination. Consequently, it may take several seconds after the stop button is clicked for the sync to stop.

### Supported File Formats
The following file format are supported, per-mode:

| Codec          | Copy               | Transcode Input    | Transcode Output   |
| -------------- | ------------------ | ------------------ | ------------------ |
| FLAC           | :heavy_check_mark: | :heavy_check_mark: | :x:                |
| ALAC           | :heavy_check_mark: | :heavy_check_mark: | :x:                |
| Wavpack        | :heavy_check_mark: | :heavy_check_mark: | :x:                |
| Monkey's Audio | :heavy_check_mark: | :heavy_check_mark: | :x:                |
| WAV            | :heavy_check_mark: | :heavy_check_mark: | :x:                |
| MP3            | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| AAC            | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Ogg Vorbis     | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Opus           | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Musepack       | :heavy_check_mark: | :heavy_check_mark: | :x:                |
| WMA            | :heavy_check_mark: | :x:                | :x:                |

### Supported Encoders
The following encoders are supported for transcoding:

| Codec        | Encoder(s)                     |
| ------------ | ------------------------------ |
| MP3          | LAME                           |
| AAC          | Fraunhofer FDK AAC, libavcodec |
| Ogg Vorbis   | libvorbis                      |
| Opus         | libopus                        |

## Support
If you encounter any bugs, please open an issue on the issue tracker. For general help, use the Discussions page instead.

### Contributing
Pull Requests will be reviewed for bug fixes and new features.

The program is currently available in English only. See the [translation README](translations/README.md) if you would like to translate the program into your language.
