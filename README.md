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
Builds are available for Windows, macOS, and some Linux distributions. You can also build from source yourself, see [BUILDING](docs/building.md)

### Windows
Easy Audio Sync is compatible with Windows 10 and later. Download the installer executable from the link below, run it, and follow the guided setup process:
- [Easy Audio Sync v1.1.1 Installer EXE (x64)](https://github.com/complexlogic/EasyAudioSync/releases/download/v1.1.1/easyaudiosync-1.1.1-setup.exe)

If Windows raises a SmartScreen warning when you try to run the executable, select More Info->Run Anyway.

### macOS
*Note: I haven't tested these builds because I have no access to macOS hardware. Consider macOS support experimental, and open an issue on the [Issue Tracker](https://github.com/complexlogic/EasyAudioSync/issues) if you encounter any bugs.*

Separate builds are available for Intel and Apple Silicon based Macs (both require macOS 11 or later):
- [Easy Audio Sync v1.1.1 DMG (Intel)](https://github.com/complexlogic/EasyAudioSync/releases/download/v1.1.1/easyaudiosync-1.1.1-x86_64.dmg)
- [Easy Audio Sync v1.1.1 DMG (Apple Silicon)](https://github.com/complexlogic/EasyAudioSync/releases/download/v1.1.1/easyaudiosync-1.1.1-arm64.dmg)

These builds are not codesigned, and the macOS Gatekeeper will most likely block execution. To work around this, you can remove the quarantine bit using the command below:

```bash
xattr -d com.apple.quarantine /path/to/easyaudiosync.dmg
```

Substitute `/path/to/easyaudiosync.dmg` with the actual path on your system.

### Linux

#### Arch/Manjaro
There is an [AUR package](https://aur.archlinux.org/packages/easyaudiosync) available, which can be installed using a helper such as yay:

```bash
yay -S easyaudiosync
```

#### APT-based (Debian, Ubuntu)
A .deb package is available on the release page. It was built on Debian Bullseye and is compatible with the most recent release of most `apt`-based distros (anything that ships GCC 12 or later). Execute the following commands to install:

```bash
wget https://github.com/complexlogic/EasyAudioSync/releases/download/v1.1.1/easyaudiosync_1.1.1_amd64.deb
sudo apt install ./easyaudiosync_1.1.1_amd64.deb
```

#### Fedora
A .rpm package is available on the release page that is compatible with Fedora 39 and later. Execute the following commands to install:

```bash
sudo dnf install https://github.com/complexlogic/EasyAudioSync/releases/download/v1.1.1/easyaudiosync-1.1.1-1.x86_64.rpm
```

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

### General Tips
This section gives usage tips for Easy Audio Sync

#### Filesystem Differences
If you run the program on Linux or Mac, have a destination folder that's on a removeable storage drive, and are getting transcoding errors, it may be because of filesystem incompatibilities. Most removable storage drives have Microsoft filesystems. In such filesystems, there are several illegal characters for paths that are allowable in Unix filesystems.

The program will not remove the illegal characters for you, because there needs to be a one-to-one correspondence between source and destination file paths to support the "Clean Destination" feature. It is the user's responsibility to be aware of filesytem differences between the source and destination folders, and prepare accordingly. There are several tools that can rename files in your source directory such that they will be compatible with all major filesystems.

#### Syncing to an Android Device
The best way of syncing an Anroid device is to use a removable SD card. Physically remove the SD card from the device, connect it to your PC, perform the sync, then replace it in the device. This may be somewhat inconvenient, but it is the best possible option. Unfortunately, not all Android devices support removable SD cards, so this option may not be available to you.

Most modern Android devies no longer support USB mass storage. The replacement is called Media Transfer Protocol (MTP). Easy Audio Sync does not include built-in support for MTP; it can only write to regular files. If you want to use the program with MTP, you will need to use it in combination with software that abstracts MTP and presents the device to the system as an ordinary storage drive, such as [go-mtpfs](https://github.com/hanwen/go-mtpfs) for Linux.

In my experience, MTP is both slow and unreliable, and not worth using. If you can't use the SD card method, another option is to use a local directory on your PC as the destination, and then use a third party sync program to transfer the files to your device. A good option is [Syncthing](https://github.com/syncthing/syncthing). It supports network-based syncing, so you don't need to directly connect your device to your PC. The disadvantage of this method is that you will need to keep multiple versions of your music library on your PC, which uses more storage space.

## Support
If you encounter any bugs, please open an issue on the issue tracker. For general help, use the Discussions page instead.

### Contributing
Pull Requests will be reviewed for bug fixes and new features.

The program is currently available in English only. See the [translation README](translations/README.md) if you would like to translate the program into your language.
