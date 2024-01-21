#include <thread>
#include <algorithm>

#include <QListWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QTranslator>
#include <QDebug>

#include "settings.hpp"
#include "config.hpp"
#include <easync.hpp>

Settings::Settings(Config &config, QSettings &settings, QTranslator &translator, QWidget *parent) : QDialog(parent), config(config), settings(settings), translator(translator) 
{
    setupUi(this);
    for (auto& [title, translatable, list_item, widget] : pages ) {
        list_item = new QListWidgetItem(translatable ? tr(title) : title, list);
        stack->addWidget(widget);
        widget->load(config);
    }
    list->setCurrentRow(0);
    stack->setCurrentIndex(0);

    // Sizing
    splitter->setSizes({4, 10});
    list->setMinimumWidth(static_cast<int>(std::round(static_cast<float>(list->sizeHintForColumn(0)) * 1.25f)));
#ifdef PERSIST_GEOMETRY
    if (!restore_window_size(this, "Settings", settings))
#endif
    {
        QSize size = minimumSizeHint();
        resize(static_cast<int>(std::round(static_cast<float>(size.width()) * 1.1f)), height());
    }
}

void Settings::save()
{
    for (auto& [title, translatable, list_item, widget] : pages )
        widget->save(config, settings);
}

void SettingsWidgetBase::populate_comboboxes()
{
    for (const auto& [map, combo_box, translatable] : combo_boxes) {
        int nb_items = (int) map->size();
        for (int i = 0; i < nb_items; i++) {
            const char *title = map->get_title(i);
            combo_box->addItem(translatable ? tr(title) : title);
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

void SettingsWidgetBase::retranslate()
{
    for (const auto& [map, combo_box, translatable] : combo_boxes) {
        if (!translatable)
            continue;
        int nb_items = (int) map->size();
        for (int i = 0; i < nb_items; i++)
            combo_box->setItemText(i, tr(map->get_title(i)));
    }
}

SettingsGeneral::SettingsGeneral(Config &config, QTranslator &translator, QWidget *parent) : SettingsWidgetBase(parent), config(config), translator(translator)
{
    setupUi(this);
    combo_boxes = {
        {&config.log_level, log_level_box, true}
    };
    populate_comboboxes();
    for (const auto& [code, name] : config.langs)
        language_box->addItem(name);
}

void SettingsGeneral::load(const Config &config)
{
    skip_existing_box->setChecked(config.skip_existing);
    sync_newer_box->setChecked(config.sync_newer);
    copy_nonaudio_box->setChecked(config.copy_nonaudio);
    clean_dest_box->setChecked(config.clean_dest);

    {
        auto it = std::find_if(config.langs.begin(),
            config.langs.end(),
            [&](const auto &i){ return i.first == config.lang; }
        );
        if (it != config.langs.end())
            language_box->setCurrentIndex((int) (it - config.langs.begin()));
    }
    log_level_box->setCurrentIndex(config.log_level.get_current_index());
    abort_on_error_box->setChecked(config.abort_on_error);
}
void SettingsGeneral::save(Config &config, QSettings &settings)
{
    save_checkbox("skip_existing", config.skip_existing, skip_existing_box, settings);
    save_checkbox("sync_newer", config.sync_newer, sync_newer_box, settings);
    save_checkbox("copy_nonaudio", config.copy_nonaudio, copy_nonaudio_box, settings);
    save_checkbox("clean_dest", config.clean_dest, clean_dest_box, settings);
    settings.setValue("language", config.lang);
    config.log_level.set(log_level_box->currentIndex());
    settings.setValue("log_level", config.log_level.get_current_key());
    save_checkbox("abort_on_error", config.abort_on_error, abort_on_error_box, settings);
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

SettingsFileHandling::SettingsFileHandling(const Config &config, QWidget *parent) : SettingsWidgetBase(parent)
{
    setupUi(this);
    file_handling = {
        {Codec::FLAC, "flac", flac_copy_button, flac_transcode_button, flac_transcode_box}, 
        {Codec::ALAC, "alac", alac_copy_button, alac_transcode_button, alac_transcode_box}, 
        {Codec::WAVPACK, "wv", wv_copy_button, wv_transcode_button, wv_transcode_box}, 
        {Codec::APE, "ape", ape_copy_button, ape_transcode_button, ape_transcode_box}, 
        {Codec::WAV, "wav", wav_copy_button, wav_transcode_button, wav_transcode_box}, 
        {Codec::MP3, "mp3", mp3_copy_button, mp3_transcode_button, mp3_transcode_box},
        {Codec::AAC, "aac", aac_copy_button, aac_transcode_button, aac_transcode_box}, 
        {Codec::VORBIS, "vorbis", ogg_copy_button, ogg_transcode_button, ogg_transcode_box}, 
        {Codec::OPUS, "opus", opus_copy_button, opus_transcode_button, opus_transcode_box}, 
        {Codec::MPC, "mpc", mpc_copy_button, mpc_transcode_button, mpc_transcode_box}
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


SettingsTranscoding::SettingsTranscoding(const Config &config, QWidget *parent) : SettingsWidgetBase(parent)
{
    setupUi(this);
    combo_boxes = {
        {&config.resampling_engine, resampling_engine_box, false},
        {&config.rg_mode, replaygain_mode_box, true}
    };
    populate_comboboxes();
}

void SettingsTranscoding::load(const Config &config)
{
    copy_metadata_box->setChecked(config.copy_metadata);
    extended_tags_box->setChecked(config.extended_tags);
    copy_artwork_box->setChecked(config.copy_metadata);
    resampling_engine_box->setCurrentIndex(config.resampling_engine.get_current_index());
    downsample_hi_res_box->setChecked(config.downsample_hi_res);
    downmix_multichannel_box->setChecked(config.downmix_multichannel);
    replaygain_mode_box->setCurrentIndex(config.rg_mode.get_current_index());
    cpu_threads_box->setValue(config.cpu_threads);
    int max_threads = std::thread::hardware_concurrency();
    cpu_threads_box->setMaximum(max_threads ? max_threads : 1);
    cpu_threads_max_button->setChecked(config.cpu_max_threads);
    cpu_threads_specific_button->setChecked(!config.cpu_max_threads);
    cpu_threads_box->setEnabled(!config.cpu_max_threads);
}

void SettingsTranscoding::save(Config &config, QSettings &settings)
{
    save_checkbox("copy_metadata", config.copy_metadata, copy_metadata_box, settings);
    save_checkbox("extended_tags", config.extended_tags, extended_tags_box, settings);
    save_checkbox("copy_artwork", config.copy_artwork, copy_artwork_box, settings);
    config.resampling_engine.set(resampling_engine_box->currentIndex());
    settings.setValue("resampling_engine", config.resampling_engine.get_current_key());
    save_checkbox("downsample_hi_res", config.downsample_hi_res, downsample_hi_res_box, settings);
    save_checkbox("downmix_multichannel", config.downmix_multichannel, downmix_multichannel_box, settings);
    config.rg_mode.set(replaygain_mode_box->currentIndex());
    settings.setValue("replaygain_mode", config.rg_mode.get_current_key());
    config.cpu_max_threads = cpu_threads_max_button->isChecked();
    settings.setValue("cpu_max_threads", config.cpu_max_threads);
    save_spinbox("cpu_threads", config.cpu_threads, cpu_threads_box, settings);
}

void SettingsTranscoding::retranslate()
{
    retranslateUi(this);
    SettingsWidgetBase::retranslate();
}

SettingsMP3::SettingsMP3(const Config &config, QWidget *parent) : SettingsWidgetBase(parent)
{
    setupUi(this);
    combo_boxes = {
        {&config.mp3.preset, mp3_preset_box, true},
    };
    populate_comboboxes();
}

void SettingsMP3::load(const Config &config)
{
    mp3_preset_box->setCurrentIndex(config.mp3.preset.get_current_index());
    id324_button->setChecked(config.mp3.id3v2_version == 4);
}

void SettingsMP3::save(Config &config, QSettings &settings)
{
    config.mp3.preset.set(mp3_preset_box->currentIndex());
    settings.setValue("mp3_preset", config.mp3.preset.get_current_key());
    config.mp3.id3v2_version = id324_button->isChecked() ? 4 : 3;
    settings.setValue("mp3_id3v2_version", config.mp3.id3v2_version);
}

void SettingsMP3::retranslate()
{
    retranslateUi(this);
    SettingsWidgetBase::retranslate();
}

SettingsAAC::SettingsAAC(const Config &config, QWidget *parent) : SettingsWidgetBase(parent)
{
    setupUi(this);
    combo_boxes = {
        {&config.aac.fdk.preset, fdk_aac_preset_box, true},
        {&config.aac.lavc.preset, lavc_aac_preset_box, true}
    };
    populate_comboboxes();
    if (config.has_feature.fdk_aac)
        encoders.emplace_back("libfdk_aac", "Fraunhofer FDK AAC");
    if (config.has_feature.lavc_aac)
        encoders.emplace_back("aac", "Libavcodec AAC");
    for (const auto& [key, name] : encoders)
        aac_encoder_box->addItem(name);
}

void SettingsAAC::load(const Config &config)
{
    std::string aac_encoder = config.get_encoder_name(Codec::AAC);
    auto it = std::find_if(encoders.begin(), encoders.end(), [&](const auto i) {return i.first == aac_encoder;});
    aac_encoder_box->setCurrentIndex(it != encoders.end() ? (int) (it - encoders.begin()) : 0);
    fdk_aac_preset_box->setCurrentIndex(config.aac.fdk.preset.get_current_index());
    lavc_aac_preset_box->setCurrentIndex(config.aac.lavc.preset.get_current_index());
    bool fdk_aac = aac_encoder == "libfdk_aac";
    set_fdk_aac_state(fdk_aac);
    set_lavc_aac_state(!fdk_aac);
}

void SettingsAAC::save(Config &config, QSettings &settings)
{
    if (aac_encoder_box->count()) {
        const std::string &encoder = encoders[aac_encoder_box->currentIndex()].first;
        config.set_encoder_name(Codec::AAC, encoder);
        settings.setValue("aac_encoder", encoder.c_str());
    }
    config.aac.fdk.preset.set(fdk_aac_preset_box->currentIndex());
    settings.setValue("aac_fdk_preset", config.aac.fdk.preset.get_current_key());
    save_checkbox("aac_fdk_afterburner", config.aac.fdk.afterburner, fdk_aac_afterburner_box, settings);
    config.aac.lavc.preset.set(lavc_aac_preset_box->currentIndex());
    settings.setValue("aac_lavc_preset", config.aac.lavc.preset.get_current_key());
}

void SettingsAAC::retranslate()
{
    retranslateUi(this);
    SettingsWidgetBase::retranslate();
}

void SettingsAAC::on_aac_encoder_box_currentIndexChanged(int index)
{
    bool fdk_aac = encoders[index].first == "libfdk_aac";
    set_fdk_aac_state(fdk_aac);
    set_lavc_aac_state(!fdk_aac);
}

void SettingsAAC::set_fdk_aac_state(bool enabled)
{
    fdk_aac_preset_box->setEnabled(enabled);
    fdk_aac_afterburner_box->setEnabled(enabled);
}

void SettingsAAC::set_lavc_aac_state(bool enabled)
{
    lavc_aac_preset_box->setEnabled(enabled);
}

SettingsOggVorbis::SettingsOggVorbis([[maybe_unused]] const Config &config, QWidget *parent) : SettingsWidgetBase(parent)
{
    setupUi(this);
}

void SettingsOggVorbis::load(const Config &config)
{
    ogg_quality_box->setValue(config.ogg.quality);
}

void SettingsOggVorbis::save(Config &config, QSettings &settings)
{
    save_spinbox("ogg_quality", config.ogg.quality, ogg_quality_box, settings);
}

SettingsOpus::SettingsOpus(const Config &config, QWidget *parent) : SettingsWidgetBase(parent)
{
    setupUi(this);
    combo_boxes = {
        {&config.opus.preset, opus_preset_box, true}
    };
    populate_comboboxes();
}

void SettingsOpus::load(const Config &config)
{
    opus_preset_box->setCurrentIndex(config.opus.preset.get_current_index());
    bool opus_ext_opus = config.opus.ext == ".opus";
    opus_ext_opus_button->setChecked(opus_ext_opus);
    opus_ext_ogg_button->setChecked(!opus_ext_opus);
    opus_convert_r128_box->setChecked(config.opus.convert_r128);
    opus_r128_adjustment_box->setValue(config.opus.r128_adjustment);
}

void SettingsOpus::save(Config &config, QSettings &settings)
{
    config.opus.preset.set(opus_preset_box->currentIndex());
    settings.setValue("opus_preset", config.opus.preset.get_current_key());
    bool ext_opus = opus_ext_opus_button->isChecked();
    config.opus.ext = ext_opus ? ".opus" : ".ogg";
    settings.setValue("opus_ext", ext_opus ? ".opus" : ".ogg");
    save_checkbox("opus_convert_r128", config.opus.convert_r128, opus_convert_r128_box, settings);
    config.opus.r128_adjustment = opus_r128_adjustment_box->value();
    settings.setValue("opus_r128_adjustment", config.opus.r128_adjustment);
}

void SettingsOpus::retranslate()
{
    retranslateUi(this);
    SettingsWidgetBase::retranslate();
}

void Settings::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        event->accept();
    }
}

void SettingsGeneral::retranslate()
{
    retranslateUi(this);
    SettingsWidgetBase::retranslate();
}

void Settings::retranslate()
{
    retranslateUi(this);
    for (auto& [title, translatable, list_item, widget] : pages) {
        if (translatable)
            list_item->setText(tr(title));
        widget->retranslate();
    }
}

CleanDestWarningDialog::CleanDestWarningDialog(QWidget *parent) : QDialog(parent)
{
    setupUi(this);
    button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
}
