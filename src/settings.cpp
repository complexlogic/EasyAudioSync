#include <thread>
#include <algorithm>

#include <QListWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QTranslator>
#include <QDebug>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QSpinBox>
#include <QRadioButton>

#include "settings.hpp"
#include "config.hpp"
#include <easync.hpp>

#include "ui_settings.h"
#include "ui_settings_general.h"
#include "ui_settings_file_handling.h"
#include "ui_settings_transcoding.h"
#include "ui_settings_mp3.h"
#include "ui_settings_aac.h"
#include "ui_settings_ogg_vorbis.h"
#include "ui_settings_opus.h"
#include "ui_clean_dest_warning.h"

#define CLEAN_DEST_WARNING_HTML "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\r\n<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\r\np, li { white-space: pre-wrap; }\r\n</style></head><body style=\" font-family:\'Noto Sans\'; font-size:10pt; font-weight:400; font-style:normal;\">\r\n<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">%1</p>\r\n<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">%2</p>\r\n<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">%3</p></body></html>"

Settings::Settings(Config &config, QSettings &settings, QTranslator &translator, QWidget *parent) :
QDialog(parent),
ui(std::make_unique<Ui::Settings>()),
config(config),
settings(settings),
translator(translator)
{
    pages =  {
        { QT_TR_NOOP("General"), true, nullptr, new SettingsGeneral(config, translator, this) },
        { QT_TR_NOOP("File Handling"), true, nullptr, new SettingsFileHandling(config, this) },
        { QT_TR_NOOP("Transcoding"), true, nullptr, new SettingsTranscoding(config, this) },
        { "MP3", false, nullptr, new SettingsMP3(config, this) },
        { "AAC", false, nullptr, new SettingsAAC(config, this) },
        { "Ogg Vorbis", false, nullptr, new SettingsOggVorbis(config, this) },
        { "Opus", false, nullptr, new SettingsOpus(config, this) }
    };

    ui->setupUi(this);
    for (auto& [title, translatable, list_item, widget] : pages ) {
        list_item = new QListWidgetItem(translatable ? tr(title) : title, ui->list);
        ui->stack->addWidget(widget);
        widget->load(config);
    }
    ui->list->setCurrentRow(0);
    ui->stack->setCurrentIndex(0);

    // Sizing
    ui->splitter->setSizes({4, 10});
    ui->list->setMinimumWidth(static_cast<int>(std::round(static_cast<float>(ui->list->sizeHintForColumn(0)) * 1.25f)));
#ifdef PERSIST_GEOMETRY
    if (!restore_window_size(this, "Settings", settings))
#endif
    {
        QSize size = minimumSizeHint();
        resize(static_cast<int>(std::round(static_cast<float>(size.width()) * 1.1f)), height());
    }
}
Settings::~Settings() = default;

void Settings::on_list_currentRowChanged(int currentRow)
{
    ui->stack->setCurrentIndex(currentRow);
}

void Settings::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        event->accept();
    }
}

void Settings::retranslate()
{
    ui->retranslateUi(this);
    for (auto& [title, translatable, list_item, widget] : pages) {
        if (translatable)
            list_item->setText(tr(title));
        widget->retranslate();
    }
}

void Settings::save()
{
    for (auto& [title, translatable, list_item, widget] : pages )
        widget->save(config, settings);
}

void Settings::close()
{
#ifdef PERSIST_GEOMETRY
    save_window_size(this, "Settings", settings);
#endif
    QWidget::close();
}

void SettingsWidgetBase::populate_comboboxes(const char *tr_context)
{
    for (const auto& [map, combo_box, translatable] : combo_boxes) {
        int nb_items = (int) map->size();
        for (int i = 0; i < nb_items; i++) {
            const char *title = map->get_title(i);
            combo_box->addItem(translatable ? QCoreApplication::translate(tr_context, title) : title);
        }
        if (nb_items < 2)
            combo_box->setEnabled(false);
    }
}

inline void SettingsWidgetBase::save_checkbox(const char *key, bool &setting, QCheckBox *box, QSettings &settings)
{
    setting = box->isChecked();
    settings.setValue(key, setting);
}

inline void SettingsWidgetBase::save_spinbox(const char *key, int &setting, QSpinBox *box, QSettings &settings)
{
    setting = box->value();
    settings.setValue(key, setting);
}

void SettingsWidgetBase::retranslate_comboboxes(const char *tr_context)
{
    for (const auto& [map, combo_box, translatable] : combo_boxes) {
        if (!translatable)
            continue;
        int nb_items = (int) map->size();
        for (int i = 0; i < nb_items; i++)
            combo_box->setItemText(i, QCoreApplication::translate(tr_context, map->get_title(i)));
    }
}

SettingsGeneral::SettingsGeneral(Config &config, QTranslator &translator, QWidget *parent) :
SettingsWidgetBase(parent),
ui(std::make_unique<Ui::SettingsGeneral>()),
config(config),
translator(translator)
{
    ui->setupUi(this);
    combo_boxes = {
        {&config.log_level, ui->log_level_box, true}
    };
    populate_comboboxes("SettingsGeneral");
    for (const auto& [code, name] : config.langs)
        ui->language_box->addItem(name);
}
SettingsGeneral::~SettingsGeneral() = default;

void SettingsGeneral::load(const Config &config)
{
    ui->skip_existing_box->setChecked(config.skip_existing);
    ui->sync_newer_box->setChecked(config.sync_newer);
    ui->copy_nonaudio_box->setChecked(config.copy_nonaudio);
    ui->clean_dest_box->setChecked(config.clean_dest);

    {
        auto it = std::find_if(config.langs.begin(),
            config.langs.end(),
            [&](const auto &i){ return i.first == config.lang; }
        );
        if (it != config.langs.end())
            ui->language_box->setCurrentIndex((int) (it - config.langs.begin()));
    }
    ui->log_level_box->setCurrentIndex(config.log_level.get_current_index());
    ui->abort_on_error_box->setChecked(config.abort_on_error);
}

void SettingsGeneral::retranslate()
{
    ui->retranslateUi(this);
    retranslate_comboboxes("SettingsGeneral");

}

void SettingsGeneral::save(Config &config, QSettings &settings)
{
    save_checkbox("skip_existing", config.skip_existing, ui->skip_existing_box, settings);
    save_checkbox("sync_newer", config.sync_newer, ui->sync_newer_box, settings);
    save_checkbox("copy_nonaudio", config.copy_nonaudio, ui->copy_nonaudio_box, settings);
    save_checkbox("clean_dest", config.clean_dest, ui->clean_dest_box, settings);
    settings.setValue("language", config.lang);
    config.log_level.set(ui->log_level_box->currentIndex());
    settings.setValue("log_level", config.log_level.get_current_key());
    save_checkbox("abort_on_error", config.abort_on_error, ui->abort_on_error_box, settings);
}

void SettingsGeneral::on_skip_existing_box_stateChanged(int state)
{
    ui->sync_newer_box->setEnabled(state == Qt::Checked);
}

void SettingsGeneral::clean_dest_warning_finished(int result)
{
    if (result != QDialog::Accepted)
        ui->clean_dest_box->setChecked(false);
}


void SettingsGeneral::on_clean_dest_box_clicked(bool checked)
{
    if (checked) {
        auto dialog = new CleanDestWarningDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->setModal(true);
        connect(dialog, &QDialog::finished, this, &SettingsGeneral::clean_dest_warning_finished);
        dialog->show();
    }
}

void SettingsGeneral::on_language_box_activated(int index)
{
    if (config.lang != config.langs[index].first) {
        config.lang = config.langs[index].first;
        std::ignore = translator.load(config.lang, ":/translations");
    }
}

SettingsFileHandling::SettingsFileHandling(const Config &config, QWidget *parent) :
SettingsWidgetBase(parent),
ui(std::make_unique<Ui::SettingsFileHandling>())
{
    ui->setupUi(this);
    file_handling = {
        {Codec::FLAC, "flac", ui->flac_copy_button, ui->flac_transcode_button, ui->flac_transcode_box},
        {Codec::ALAC, "alac", ui->alac_copy_button, ui->alac_transcode_button, ui->alac_transcode_box},
        {Codec::WAVPACK, "wv", ui->wv_copy_button, ui->wv_transcode_button, ui->wv_transcode_box},
        {Codec::APE, "ape", ui->ape_copy_button, ui->ape_transcode_button, ui->ape_transcode_box},
        {Codec::WAV, "wav", ui->wav_copy_button, ui->wav_transcode_button, ui->wav_transcode_box},
        {Codec::MP3, "mp3", ui->mp3_copy_button, ui->mp3_transcode_button, ui->mp3_transcode_box},
        {Codec::AAC, "aac", ui->aac_copy_button, ui->aac_transcode_button, ui->aac_transcode_box},
        {Codec::VORBIS, "vorbis", ui->ogg_copy_button, ui->ogg_transcode_button, ui->ogg_transcode_box},
        {Codec::OPUS, "opus", ui->opus_copy_button, ui->opus_transcode_button, ui->opus_transcode_box},
        {Codec::MPC, "mpc", ui->mpc_copy_button, ui->mpc_transcode_button, ui->mpc_transcode_box}
    };
    if (config.codec_support.contains(Codec::MP3))
        transcode_outputs.emplace_back(Codec::MP3, "MP3", "mp3");
    if (config.codec_support.contains(Codec::AAC))
        transcode_outputs.emplace_back(Codec::AAC, "AAC", "aac");
    if (config.codec_support.contains(Codec::VORBIS))
        transcode_outputs.emplace_back(Codec::VORBIS, "Ogg Vorbis", "vorbis");
    if (config.codec_support.contains(Codec::OPUS))
        transcode_outputs.emplace_back(Codec::OPUS, "Opus", "opus");
}
SettingsFileHandling::~SettingsFileHandling() = default;

void SettingsFileHandling::load(const Config &config)
{
    for (const auto &row : file_handling) {
        for (const auto& [codec, name, key] : transcode_outputs)
            row.box->addItem(name);
        auto it = std::find_if(config.file_handling.begin(), 
            config.file_handling.end(), 
            [&](const auto &i) { return i.in_codec == row.codec; }
        );
        it->action == Action::COPY ? row.copy_button->setChecked(true) : row.transcode_button->setChecked(true);
        auto it2 = std::find_if(transcode_outputs.begin(),
            transcode_outputs.end(),
            [&](const auto &i) { return std::get<0>(i) == it->out_codec;}
        );
        row.box->setCurrentIndex((int) (it2 - transcode_outputs.begin()));
        set_transcode_box_state(row.box, it->action == Action::TRANSCODE);
    }
}

void SettingsFileHandling::save(Config &config, QSettings &settings)
{
    for (const auto &fh : file_handling) {
        Action action = fh.copy_button->isChecked() ? Action::COPY : Action::TRANSCODE;
        auto it = std::find_if(config.file_handling.begin(),
            config.file_handling.end(),
            [&](const auto &i) { return i.in_codec == fh.codec; }
        );
        it->action = action;
        const auto& [codec, name, key] = transcode_outputs[fh.box->currentIndex()];
        it->out_codec = codec;
        settings.setValue("action_" + fh.key, action == Action::COPY ? "copy" : "transcode");
        settings.setValue("transcode_" + fh.key, key);
    }
}

void SettingsFileHandling::on_flac_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->flac_transcode_box, !checked);
}

void SettingsFileHandling::on_alac_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->alac_transcode_box, !checked);
}

void SettingsFileHandling::on_wv_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->wv_transcode_box, !checked);
}

void SettingsFileHandling::on_ape_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->ape_transcode_box, !checked);
}

void SettingsFileHandling::on_wav_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->wav_transcode_box, !checked);
}

void SettingsFileHandling::on_mp3_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->mp3_transcode_box, !checked);
}

void SettingsFileHandling::on_aac_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->aac_transcode_box, !checked);
}

void SettingsFileHandling::on_ogg_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->ogg_transcode_box, !checked);
}

void SettingsFileHandling::on_opus_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->opus_transcode_box, !checked);
}

void SettingsFileHandling::on_mpc_copy_button_toggled(bool checked)
{
    set_transcode_box_state(ui->mpc_transcode_box, !checked);
}

void SettingsFileHandling::retranslate()
{
    ui->retranslateUi(this);
}

SettingsTranscoding::SettingsTranscoding(const Config &config, QWidget *parent) :
SettingsWidgetBase(parent),
ui(std::make_unique<Ui::SettingsTranscoding>())
{
    ui->setupUi(this);
    combo_boxes = {
        {&config.resampling_engine, ui->resampling_engine_box, false},
        {&config.rg_mode, ui->replaygain_mode_box, true}
    };
    populate_comboboxes("SettingsTranscoding");
}
SettingsTranscoding::~SettingsTranscoding() = default;

void SettingsTranscoding::load(const Config &config)
{
    ui->copy_metadata_box->setChecked(config.copy_metadata);
    ui->extended_tags_box->setChecked(config.extended_tags);
    ui->copy_artwork_box->setChecked(config.copy_metadata);
    ui->resampling_engine_box->setCurrentIndex(config.resampling_engine.get_current_index());
    ui->downsample_hi_res_box->setChecked(config.downsample_hi_res);
    ui->downmix_multichannel_box->setChecked(config.downmix_multichannel);
    ui->replaygain_mode_box->setCurrentIndex(config.rg_mode.get_current_index());
    ui->cpu_threads_box->setValue(config.cpu_threads);
    int max_threads = std::thread::hardware_concurrency();
    ui->cpu_threads_box->setMaximum(max_threads ? max_threads : 1);
    ui->cpu_threads_max_button->setChecked(config.cpu_max_threads);
    ui->cpu_threads_specific_button->setChecked(!config.cpu_max_threads);
    ui->cpu_threads_box->setEnabled(!config.cpu_max_threads);
}

void SettingsTranscoding::save(Config &config, QSettings &settings)
{
    save_checkbox("copy_metadata", config.copy_metadata, ui->copy_metadata_box, settings);
    save_checkbox("extended_tags", config.extended_tags, ui->extended_tags_box, settings);
    save_checkbox("copy_artwork", config.copy_artwork, ui->copy_artwork_box, settings);
    config.resampling_engine.set(ui->resampling_engine_box->currentIndex());
    settings.setValue("resampling_engine", config.resampling_engine.get_current_key());
    save_checkbox("downsample_hi_res", config.downsample_hi_res, ui->downsample_hi_res_box, settings);
    save_checkbox("downmix_multichannel", config.downmix_multichannel, ui->downmix_multichannel_box, settings);
    config.rg_mode.set(ui->replaygain_mode_box->currentIndex());
    settings.setValue("replaygain_mode", config.rg_mode.get_current_key());
    config.cpu_max_threads = ui->cpu_threads_max_button->isChecked();
    settings.setValue("cpu_max_threads", config.cpu_max_threads);
    save_spinbox("cpu_threads", config.cpu_threads, ui->cpu_threads_box, settings);
}

void SettingsTranscoding::on_cpu_threads_max_button_toggled(bool checked)
{
    ui->cpu_threads_box->setEnabled(!checked);
}

void SettingsTranscoding::on_copy_metadata_box_stateChanged(int state)
{
    ui->extended_tags_box->setEnabled(state == Qt::Checked);
}

void SettingsTranscoding::retranslate()
{
    ui->retranslateUi(this);
    retranslate_comboboxes("SettingsTranscoding");
}

SettingsMP3::SettingsMP3(const Config &config, QWidget *parent) :
SettingsWidgetBase(parent),
ui(std::make_unique<Ui::SettingsMP3>())
{
    ui->setupUi(this);
    combo_boxes = {
        {&config.mp3.preset, ui->mp3_preset_box, true},
    };
    populate_comboboxes("SettingsMP3");
}
SettingsMP3::~SettingsMP3() = default;

void SettingsMP3::load(const Config &config)
{
    ui->mp3_preset_box->setCurrentIndex(config.mp3.preset.get_current_index());
    ui->id324_button->setChecked(config.mp3.id3v2_version == 4);
}

void SettingsMP3::save(Config &config, QSettings &settings)
{
    config.mp3.preset.set(ui->mp3_preset_box->currentIndex());
    settings.setValue("mp3_preset", config.mp3.preset.get_current_key());
    config.mp3.id3v2_version = ui->id324_button->isChecked() ? 4 : 3;
    settings.setValue("mp3_id3v2_version", config.mp3.id3v2_version);
}

void SettingsMP3::retranslate()
{
    ui->retranslateUi(this);
    retranslate_comboboxes("SettingsMP3");
}

SettingsAAC::SettingsAAC(const Config &config, QWidget *parent) :
SettingsWidgetBase(parent),
ui(std::make_unique<Ui::SettingsAAC>())
{
    ui->setupUi(this);
    combo_boxes = {
        {&config.aac.fdk.preset, ui->fdk_aac_preset_box, true},
        {&config.aac.lavc.preset, ui->lavc_aac_preset_box, true}
    };
    populate_comboboxes("SettingsAAC");
    if (config.has_feature.fdk_aac)
        encoders.emplace_back("libfdk_aac", "Fraunhofer FDK AAC");
    if (config.has_feature.lavc_aac)
        encoders.emplace_back("aac", "Libavcodec AAC");
    for (const auto& [key, name] : encoders)
        ui->aac_encoder_box->addItem(name);
}
SettingsAAC::~SettingsAAC() = default;

void SettingsAAC::load(const Config &config)
{
    std::string aac_encoder = config.get_encoder_name(Codec::AAC);
    auto it = std::find_if(encoders.begin(), encoders.end(), [&](const auto i) {return i.first == aac_encoder;});
    ui->aac_encoder_box->setCurrentIndex(it != encoders.end() ? (int) (it - encoders.begin()) : 0);
    ui->fdk_aac_preset_box->setCurrentIndex(config.aac.fdk.preset.get_current_index());
    ui->lavc_aac_preset_box->setCurrentIndex(config.aac.lavc.preset.get_current_index());
    bool fdk_aac = aac_encoder == "libfdk_aac";
    set_fdk_aac_state(fdk_aac);
    set_lavc_aac_state(!fdk_aac);
}

void SettingsAAC::save(Config &config, QSettings &settings)
{
    if (ui->aac_encoder_box->count()) {
        const std::string &encoder = encoders[ui->aac_encoder_box->currentIndex()].first;
        config.set_encoder_name(Codec::AAC, encoder);
        settings.setValue("aac_encoder", encoder.c_str());
    }
    config.aac.fdk.preset.set(ui->fdk_aac_preset_box->currentIndex());
    settings.setValue("aac_fdk_preset", config.aac.fdk.preset.get_current_key());
    save_checkbox("aac_fdk_afterburner", config.aac.fdk.afterburner, ui->fdk_aac_afterburner_box, settings);
    config.aac.lavc.preset.set(ui->lavc_aac_preset_box->currentIndex());
    settings.setValue("aac_lavc_preset", config.aac.lavc.preset.get_current_key());
}

void SettingsAAC::retranslate()
{
    ui->retranslateUi(this);
    retranslate_comboboxes("SettingsAAC");
}

void SettingsAAC::on_aac_encoder_box_currentIndexChanged(int index)
{
    bool fdk_aac = encoders[index].first == "libfdk_aac";
    set_fdk_aac_state(fdk_aac);
    set_lavc_aac_state(!fdk_aac);
}

void SettingsAAC::set_fdk_aac_state(bool enabled)
{
    ui->fdk_aac_preset_box->setEnabled(enabled);
    ui->fdk_aac_afterburner_box->setEnabled(enabled);
}

void SettingsAAC::set_lavc_aac_state(bool enabled)
{
    ui->lavc_aac_preset_box->setEnabled(enabled);
}

SettingsOggVorbis::SettingsOggVorbis([[maybe_unused]] const Config &config, QWidget *parent) :
SettingsWidgetBase(parent),
ui(std::make_unique<Ui::SettingsOggVorbis>())
{
    ui->setupUi(this);
}
SettingsOggVorbis::~SettingsOggVorbis() = default;

void SettingsOggVorbis::load(const Config &config)
{
    ui->ogg_quality_box->setValue(config.ogg.quality);
}

void SettingsOggVorbis::save(Config &config, QSettings &settings)
{
    save_spinbox("ogg_quality", config.ogg.quality, ui->ogg_quality_box, settings);
}

void SettingsOggVorbis::retranslate()
{
    ui->retranslateUi(this);
}

SettingsOpus::SettingsOpus(const Config &config, QWidget *parent) :
SettingsWidgetBase(parent),
ui(std::make_unique<Ui::SettingsOpus>())
{
    ui->setupUi(this);
    combo_boxes = {
        {&config.opus.preset, ui->opus_preset_box, true}
    };
    populate_comboboxes("SettingsOpus");
}
SettingsOpus::~SettingsOpus() = default;

void SettingsOpus::load(const Config &config)
{
    ui->opus_preset_box->setCurrentIndex(config.opus.preset.get_current_index());
    bool opus_ext_opus = config.opus.ext == ".opus";
    ui->opus_ext_opus_button->setChecked(opus_ext_opus);
    ui->opus_ext_ogg_button->setChecked(!opus_ext_opus);
    ui->opus_convert_r128_box->setChecked(config.opus.convert_r128);
    ui->opus_r128_adjustment_box->setValue(config.opus.r128_adjustment);
}

void SettingsOpus::save(Config &config, QSettings &settings)
{
    config.opus.preset.set(ui->opus_preset_box->currentIndex());
    settings.setValue("opus_preset", config.opus.preset.get_current_key());
    bool ext_opus = ui->opus_ext_opus_button->isChecked();
    config.opus.ext = ext_opus ? ".opus" : ".ogg";
    settings.setValue("opus_ext", ext_opus ? ".opus" : ".ogg");
    save_checkbox("opus_convert_r128", config.opus.convert_r128, ui->opus_convert_r128_box, settings);
    config.opus.r128_adjustment = ui->opus_r128_adjustment_box->value();
    settings.setValue("opus_r128_adjustment", config.opus.r128_adjustment);
}

void SettingsOpus::on_opus_convert_r128_box_stateChanged(int state)
{
    ui->opus_r128_adjustment_box->setEnabled(state == Qt::Checked);
}

void SettingsOpus::retranslate()
{
    ui->retranslateUi(this);
    retranslate_comboboxes("SettingsOpus");
}

CleanDestWarningDialog::CleanDestWarningDialog(QWidget *parent) :
QDialog(parent),
ui(std::make_unique<Ui::CleanDestWarningDialog>())
{
    ui->setupUi(this);
    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
    ui->textBrowser->setText(QString(CLEAN_DEST_WARNING_HTML)
        .arg(tr("The clean destination feature deletes files in the destination that do not exist in the source. "
                "After deleting files, the program removes any empty directories in the destination."))
        .arg(tr("Destination files are deleted permanently with no recovery option. "
                "You will not be prompted for confirmation before the files are deleted."))
        .arg(tr("This feature should only be enabled when the program is being used as intended. "
                "Any existing files in the destination directory should have been created by a previous sync. "
                "If your destination directory contains other files, you may experience unwanted data loss."))
    );
}
CleanDestWarningDialog::~CleanDestWarningDialog() = default;

void CleanDestWarningDialog::closeEvent(QCloseEvent *event)
{
    emit finished(QDialogButtonBox::Close);
    event->accept();
}

void CleanDestWarningDialog::on_accept_box_clicked(bool checked)
{
    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(checked);
}
