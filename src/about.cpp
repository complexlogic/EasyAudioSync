#include <QDialog>
#include <QString>
#include <QTabWidget>
#include <QTabBar>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}

#include "config.hpp"
#include "about.hpp"
#include <easync.hpp>

#define FFMPEG_VERSION(fn, label) \
  ffver = fn(); \
  label->setText(QString("%1.%2.%3").arg(AV_VERSION_MAJOR(ffver)).arg(AV_VERSION_MINOR(ffver)).arg(AV_VERSION_MICRO(ffver)))

#define PROGRAM_VER "<html><head/><body><p><span style=\" font-size:12pt;\">%1</span></p></body></html>"

AboutDialog::AboutDialog(const Config &config, QWidget *parent) : QDialog(parent)
{
    setupUi(this);
    tabs->setCurrentIndex(0);
    program_ver->setText(QString(PROGRAM_VER).arg(QString(tr("Version %1")).arg(PROJECT_VERSION)));

    // Library Versions
    qt_ver->setText(qVersion());
    int ffver;
    FFMPEG_VERSION(avformat_version, lavf_ver);
    FFMPEG_VERSION(avcodec_version, lavc_ver);
    FFMPEG_VERSION(avfilter_version, lavfilter_ver);
    FFMPEG_VERSION(swresample_version, lswr_ver);
    FFMPEG_VERSION(avutil_version, lavu_ver);

    // Encoders
#define SUPPORT_FEATURE(feature, label) label->setText(feature ? tr("Yes") : tr("No"))
    SUPPORT_FEATURE(config.has_feature.lame, support_lame);
    SUPPORT_FEATURE(config.has_feature.fdk_aac, support_fdk_aac);
    SUPPORT_FEATURE(config.has_feature.lavc_aac, support_lavc_aac);
    SUPPORT_FEATURE(config.has_feature.libvorbis, support_libvorbis);
    SUPPORT_FEATURE(config.has_feature.libopus, support_libopus);
    SUPPORT_FEATURE(config.has_feature.soxr, support_soxr);


    // Build info
    build_date->setText(BUILD_DATE);
#if defined(__GNUC__) && !defined(__clang__)
    compiler->setText(QString("GCC %1.%2").arg(__GNUC__).arg(__GNUC_MINOR__));
#endif

#if defined(__clang__)
    compiler->setText(QString(
#ifdef __apple_build_version__
        "Apple "
#endif
        "Clang %1.%2.%3").arg(__clang_major__).arg( __clang_minor__).arg(__clang_patchlevel__));
#endif

#ifdef _MSC_VER
    compiler->setText(QString("Microsoft C/C++ %1").arg(QString::number(_MSC_VER / 100.0f, 'f', 2)));
#endif
}