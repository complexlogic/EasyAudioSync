#pragma once

#include <array>
#include <vector>
#include <tuple>

#include <QtGlobal>
#include <QSettings>
#include <QListWidgetItem>
#include <QObject>
#include <QWidget>
#include <QTranslator>
#include <QCloseEvent>
#include <QPushButton>

#include "ui_settings.h"
#include "ui_settings_general.h"
#include "ui_settings_file_handling.h"
#include "ui_settings_transcoding.h"
#include "ui_settings_mp3.h"
#include "ui_settings_aac.h"
#include "ui_settings_ogg_vorbis.h"
#include "ui_settings_opus.h"
#include "ui_clean_dest_warning.h"

#include "util.hpp"
#include "config.hpp"

#define SETTINGS_VERSION 1

class SettingsWidgetBase : public QWidget
{
    public:
        SettingsWidgetBase(QWidget *parent) : QWidget(parent) {}
        virtual ~SettingsWidgetBase() = default;
        virtual void retranslate();
        virtual void load(const Config &config) = 0;
        virtual void save(Config &config, QSettings &settings) = 0;
    
    protected:
        std::vector<std::tuple<const Config::ComboMapBase*, QComboBox*, bool>> combo_boxes;

        void populate_comboboxes();
        inline void save_checkbox(const char *key, bool &setting, QCheckBox *box, QSettings &settings);
        inline void save_spinbox(const char *key, int &setting, QSpinBox *box, QSettings &settings);

};

class SettingsGeneral : public SettingsWidgetBase, private Ui::SettingsGeneral
{
    Q_OBJECT

    public: 
        SettingsGeneral(Config &config, QTranslator &translator, QWidget *parent);
        virtual void retranslate();
        virtual void load(const Config &config);
        virtual void save(Config &config, QSettings &settings);

    private slots:
        void on_skip_existing_box_stateChanged(int state) { sync_newer_box->setEnabled(state == Qt::Checked); }
        void on_clean_dest_box_clicked(bool checked);
        void on_language_box_activated(int index);
        void clean_dest_warning_finished(int result) { if (result != QDialog::Accepted) clean_dest_box->setChecked(false); }

    private:
        Config &config;
        QTranslator &translator;

};

class SettingsFileHandling : public SettingsWidgetBase, private Ui::SettingsFileHandling
{
    Q_OBJECT

    public:
        SettingsFileHandling(const Config &config, QWidget *parent);
        virtual void retranslate() { retranslateUi(this); }
        virtual void load(const Config &config);
        virtual void save(Config &config, QSettings &settings);


    private slots:
        void on_flac_copy_button_toggled(bool checked) { set_transcode_box_state(flac_transcode_box, !checked); }
        void on_alac_copy_button_toggled(bool checked) { set_transcode_box_state(alac_transcode_box, !checked); }
        void on_wv_copy_button_toggled(bool checked)   { set_transcode_box_state(wv_transcode_box, !checked); }
        void on_ape_copy_button_toggled(bool checked)  { set_transcode_box_state(ape_transcode_box, !checked); }
        void on_wav_copy_button_toggled(bool checked)  { set_transcode_box_state(wav_transcode_box, !checked); }
        void on_mp3_copy_button_toggled(bool checked)  { set_transcode_box_state(mp3_transcode_box, !checked); }
        void on_aac_copy_button_toggled(bool checked)  { set_transcode_box_state(aac_transcode_box, !checked); }
        void on_ogg_copy_button_toggled(bool checked)  { set_transcode_box_state(ogg_transcode_box, !checked); }
        void on_opus_copy_button_toggled(bool checked) { set_transcode_box_state(opus_transcode_box, !checked); }
        void on_mpc_copy_button_toggled(bool checked)  { set_transcode_box_state(mpc_transcode_box, !checked); }

    private:
        struct GUIFileHandling {
            Codec codec;
            QString key;
            QRadioButton *copy_button;
            QRadioButton *transcode_button;
            QComboBox *box;
        };
        std::vector<std::tuple<Codec, QString, QString>> transcode_outputs;
        std::vector<GUIFileHandling> file_handling;

        void set_transcode_box_state(QComboBox *box, bool checked) { box->setEnabled(checked); }

};

class SettingsTranscoding : public SettingsWidgetBase, private Ui::SettingsTranscoding
{
    Q_OBJECT

    public:
        SettingsTranscoding(const Config &config, QWidget *parent);
        virtual void retranslate();
        virtual void load(const Config &config);
        virtual void save(Config &config, QSettings &settings);

    private slots:
        void on_cpu_threads_max_button_toggled(bool checked) { cpu_threads_box->setEnabled(!checked); }

};

class SettingsMP3 : public SettingsWidgetBase, private Ui::SettingsMP3
{
    Q_OBJECT

    public:
        SettingsMP3(const Config &config, QWidget *parent);
        virtual void retranslate();
        virtual void load(const Config &config);
        virtual void save(Config &config, QSettings &settings);

};

class SettingsAAC : public SettingsWidgetBase, private Ui::SettingsAAC
{
    Q_OBJECT

    public:
        SettingsAAC(const Config &config, QWidget *parent);
        virtual void retranslate();
        virtual void load(const Config &config);
        virtual void save(Config &config, QSettings &settings);

    private slots:
        void on_aac_encoder_box_currentIndexChanged(int index);

    private:
        std::vector<std::pair<std::string, QString>> encoders;
        void set_fdk_aac_state(bool enabled);
        void set_lavc_aac_state(bool enabled);

};

class SettingsOggVorbis : public SettingsWidgetBase, private Ui::SettingsOggVorbis
{
    Q_OBJECT

    public:
        SettingsOggVorbis(const Config &config, QWidget *parent);
        virtual void retranslate() { retranslateUi(this); }
        virtual void load(const Config &config);
        virtual void save(Config &config, QSettings &settings);

};

class SettingsOpus : public SettingsWidgetBase, private Ui::SettingsOpus
{
    Q_OBJECT

    public:
        SettingsOpus(const Config &config, QWidget *parent);
        virtual void retranslate();
        virtual void load(const Config &config);
        virtual void save(Config &config, QSettings &settings);

    private slots:
        void on_opus_convert_r128_box_stateChanged(int state) { opus_r128_adjustment_box->setEnabled(state == Qt::Checked);}
};

class CleanDestWarningDialog : public QDialog, private Ui::CleanDestWarningDialog
{
    Q_OBJECT

private slots:
    void on_accept_box_clicked(bool checked) { button_box->button(QDialogButtonBox::Ok)->setEnabled(checked); }

public:
    explicit CleanDestWarningDialog(QWidget *parent = nullptr);
    void closeEvent(QCloseEvent *event) { 
        emit finished(QDialogButtonBox::Close);
        event->accept();
    }
};

class Settings : public QDialog, private Ui::Settings
{
    Q_OBJECT

    public:
        explicit Settings(Config &config, QSettings &settings, QTranslator &translator, QWidget *parent = nullptr);

        void closeEvent(QCloseEvent *event) {
            close();
            event->accept();
        }
        void changeEvent(QEvent *event);

    public slots:
        void done(int r) {
            if (r == QDialog::Accepted)
                save();
            close();
            }

    private slots:
        void on_list_currentRowChanged(int currentRow) { stack->setCurrentIndex(currentRow); }

    private:
        Config &config;
        QSettings &settings;
        QTranslator &translator;

        // title, translatable, listitem, widget
        std::array<std::tuple<const char*, bool, QListWidgetItem*, SettingsWidgetBase*>, 7> pages = {{
            { QT_TR_NOOP("General"), true, nullptr, new SettingsGeneral(config, translator, this) },
            { QT_TR_NOOP("File Handling"), true, nullptr, new SettingsFileHandling(config, this) },
            { QT_TR_NOOP("Transcoding"), true, nullptr, new SettingsTranscoding(config, this) },
            { "MP3", false, nullptr, new SettingsMP3(config, this) },
            { "AAC", false, nullptr, new SettingsAAC(config, this) },
            { "Ogg Vorbis", false, nullptr, new SettingsOggVorbis(config, this) },
            { "Opus", false, nullptr, new SettingsOpus(config, this) }
        }};

        void retranslate();
        void save();
        virtual void close() {
#ifdef PERSIST_GEOMETRY
            save_window_size(this, "Settings", settings);
#endif
            QWidget::close();
        }
};
