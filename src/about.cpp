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

#include <taglib/taglib.h>
#if TAGLIB_MAJOR_VERSION > 1
#include <taglib/tversionnumber.h>
#endif

#include "ui_about.h"
#include "config.hpp"
#include "about.hpp"
#include <easync.hpp>

#define PROGRAM_VER "<html><head/><body><p><span style=\" font-size:12pt;\">%1</span></p></body></html>"
#define ABOUT_HTML "<html><head/><body><p>%1</p><p>%2</p></body></html>"

AboutDialog::AboutDialog(const Config &config, QWidget *parent) :
QDialog(parent),
ui(std::make_unique<Ui::AboutDialog>())
{
    ui->setupUi(this);
    ui->tabs->setCurrentIndex(0);
    ui->program_ver->setText(QString(PROGRAM_VER).arg(QString(tr("Version %1")).arg(
#ifdef PROJECT_VERSION_GIT
        PROJECT_VERSION_GIT
#else
        PROJECT_VERSION
#endif
    )));

    ui->about_label->setText(QString(ABOUT_HTML)
        .arg(tr("Easy Audio Sync is an audio library syncing and conversion utility. "
                "The intended use is syncing an audio library with many lossless files to a mobile device with limited storage."))
        .arg(tr("See the %1 for more information.")
            .arg(QString("<a href=\"https://github.com/complexlogic/EasyAudioSync\"><span style=\" text-decoration: underline; color:#2980b9;\">%1</span></a>")
                .arg(tr("GitHub page"))
            )
        )
    );

    // Library Versions
    ui->qt_ver->setText(qVersion());
    ffmpeg_version(avformat_version, ui->lavf_ver);
    ffmpeg_version(avcodec_version, ui->lavc_ver);
    ffmpeg_version(avfilter_version, ui->lavfilter_ver);
    ffmpeg_version(swresample_version, ui->lswr_ver);
    ffmpeg_version(avutil_version, ui->lavu_ver);

#if TAGLIB_MAJOR_VERSION > 1
    {
        const auto tversion = TagLib::runtimeVersion();
        ui->formLayout->addRow(
            new QLabel("TagLib:", this),
            new QLabel(
                    QString("%1.%2%3")
                        .arg(tversion.majorVersion())
                        .arg(tversion.minorVersion())
                        .arg(tversion.patchVersion() ? QString(".%1").arg(tversion.patchVersion()) : ""),
                    this
                )
        );
    }
#endif

    // Encoders
#define SUPPORT_FEATURE(feature, label) ui->label->setText(feature ? tr("Yes") : tr( "No"))
    SUPPORT_FEATURE(config.has_feature.lame, support_lame);
    SUPPORT_FEATURE(config.has_feature.fdk_aac, support_fdk_aac);
    SUPPORT_FEATURE(config.has_feature.lavc_aac, support_lavc_aac);
    SUPPORT_FEATURE(config.has_feature.libvorbis, support_libvorbis);
    SUPPORT_FEATURE(config.has_feature.libopus, support_libopus);
    SUPPORT_FEATURE(config.has_feature.soxr, support_soxr);

    // Build info
    ui->build_date->setText(BUILD_DATE);
#if defined(__GNUC__) && !defined(__clang__)
    ui->compiler->setText(QString("GCC %1.%2"
#if __GNUC_PATCHLEVEL__ > 0
        ".%3"
#endif
        )
        .arg(__GNUC__)
        .arg(__GNUC_MINOR__)
#if __GNUC_PATCHLEVEL__ > 0
        .arg(__GNUC_PATCHLEVEL__)
#endif
    );
#endif

#if defined(__clang__)
    ui->compiler->setText(QString(
#ifdef __apple_build_version__
        "Apple "
#endif
        "Clang %1.%2.%3").arg(__clang_major__).arg( __clang_minor__).arg(__clang_patchlevel__));
#endif

#ifdef _MSC_VER
    ui->compiler->setText(QString("Microsoft C/C++ %1").arg(QString::number(_MSC_VER / 100.0f, 'f', 2)));
#endif

}
AboutDialog::~AboutDialog() = default;

void AboutDialog::ffmpeg_version(unsigned(*fn)(), QLabel *label)
{
    int ffver = fn();
    label->setText(QString("%1.%2.%3").arg(AV_VERSION_MAJOR(ffver)).arg(AV_VERSION_MINOR(ffver)).arg(AV_VERSION_MICRO(ffver)));
}
