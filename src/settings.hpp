#pragma once

#include <vector>
#include <tuple>

#include <QtGlobal>
#include <QSettings>
#include <QListWidgetItem>
#include <QObject>
#include <QWidget>
#include <QTranslator>
#include <QCloseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>

#include "util.hpp"

#define SETTINGS_VERSION 1

namespace Ui {
    class Settings;
    class SettingsGeneral;
    class SettingsFileHandling;
    class SettingsTranscoding;
    class SettingsMP3;
    class SettingsAAC;
    class SettingsOggVorbis;
    class SettingsOpus;
    class CleanDestWarningDialog;
}
class SettingsWidgetBase;
class ComboMapBase;
class Config;

class Settings : public QDialog
{
    Q_OBJECT

    public:
        explicit Settings(Config &config, QSettings &settings, QTranslator &translator, QWidget *parent = nullptr);
        ~Settings() override;

    public slots:
        void done(int r) {
            if (r == QDialog::Accepted)
                save();
            close();
            }

    private slots:
        void on_list_currentRowChanged(int currentRow);

    private:
        std::unique_ptr<Ui::Settings> ui;
        Config &config;
        QSettings &settings;
        QTranslator &translator;

        // title, translatable, listitem, widget
        std::vector<std::tuple<const char*, bool, QListWidgetItem*, SettingsWidgetBase*>> pages;

        void closeEvent(QCloseEvent *event) override {
            close();
            event->accept();
        }
        void changeEvent(QEvent *event) override;
        void retranslate();
        void save();
        void close();
};

class SettingsWidgetBase : public QWidget
{
    public:
        SettingsWidgetBase(QWidget *parent) : QWidget(parent) {}
        virtual ~SettingsWidgetBase() = default;
        virtual void retranslate() = 0;
        virtual void load(const Config &config) = 0;
        virtual void save(Config &config, QSettings &settings) = 0;
    
    protected:
        std::vector<std::tuple<const ComboMapBase*, QComboBox*, bool>> combo_boxes;

        void populate_comboboxes(const char *tr_context);
        void retranslate_comboboxes(const char *tr_context);
        inline void save_checkbox(const char *key, bool &setting, QCheckBox *box, QSettings &settings);
        inline void save_spinbox(const char *key, int &setting, QSpinBox *box, QSettings &settings);
};

class SettingsGeneral : public SettingsWidgetBase
{
    Q_OBJECT

    public: 
        SettingsGeneral(Config &config, QTranslator &translator, QWidget *parent);
        ~SettingsGeneral() override;
        void retranslate() override;
        void load(const Config &config) override;
        void save(Config &config, QSettings &settings) override;

    private slots:
        void on_skip_existing_box_stateChanged(int state);
        void on_clean_dest_box_clicked(bool checked);
        void on_language_box_activated(int index);
        void clean_dest_warning_finished(int result);

    private:
        std::unique_ptr<Ui::SettingsGeneral> ui;
        Config &config;
        QTranslator &translator;
};

class SettingsFileHandling : public SettingsWidgetBase
{
    Q_OBJECT

    public:
        SettingsFileHandling(const Config &config, QWidget *parent);
        ~SettingsFileHandling() override;
        void retranslate() override;
        void load(const Config &config) override;
        void save(Config &config, QSettings &settings) override;


    private slots:
        void on_flac_copy_button_toggled(bool checked);
        void on_alac_copy_button_toggled(bool checked);
        void on_wv_copy_button_toggled(bool checked);
        void on_ape_copy_button_toggled(bool checked);
        void on_wav_copy_button_toggled(bool checked);
        void on_mp3_copy_button_toggled(bool checked);
        void on_aac_copy_button_toggled(bool checked);
        void on_ogg_copy_button_toggled(bool checked);
        void on_opus_copy_button_toggled(bool checked);
        void on_mpc_copy_button_toggled(bool checked);

    private:
        struct GUIFileHandling {
            Codec codec;
            QString key;
            QRadioButton *copy_button;
            QRadioButton *transcode_button;
            QComboBox *box;
        };
        std::unique_ptr<Ui::SettingsFileHandling> ui;
        std::vector<std::tuple<Codec, QString, QString>> transcode_outputs;
        std::vector<GUIFileHandling> file_handling;

        void set_transcode_box_state(QComboBox *box, bool checked) { box->setEnabled(checked); }
};

class SettingsTranscoding : public SettingsWidgetBase
{
    Q_OBJECT

    private slots:
        void on_cpu_threads_max_button_toggled(bool checked);
        void on_copy_metadata_box_stateChanged(int state);

    private:
        std::unique_ptr<Ui::SettingsTranscoding> ui;

    public:
        SettingsTranscoding(const Config &config, QWidget *parent);
        ~SettingsTranscoding() override;
        void retranslate() override;
        void load(const Config &config) override;
        void save(Config &config, QSettings &settings) override;
};

class SettingsMP3 : public SettingsWidgetBase
{
    Q_OBJECT

    private:
        std::unique_ptr<Ui::SettingsMP3> ui;

    public:
        SettingsMP3(const Config &config, QWidget *parent);
        ~SettingsMP3() override;
        void retranslate() override;
        void load(const Config &config) override;
        void save(Config &config, QSettings &settings) override;
};

class SettingsAAC : public SettingsWidgetBase
{
    Q_OBJECT

    private:
        std::unique_ptr<Ui::SettingsAAC> ui;
        std::vector<std::pair<std::string, QString>> encoders;
        void set_fdk_aac_state(bool enabled);
        void set_lavc_aac_state(bool enabled);

    public:
        SettingsAAC(const Config &config, QWidget *parent);
        ~SettingsAAC() override;
        void retranslate() override;
        void load(const Config &config) override;
        void save(Config &config, QSettings &settings) override;

    private slots:
        void on_aac_encoder_box_currentIndexChanged(int index);
};

class SettingsOggVorbis : public SettingsWidgetBase
{
    Q_OBJECT

    private:
        std::unique_ptr<Ui::SettingsOggVorbis> ui;

    public:
        SettingsOggVorbis(const Config &config, QWidget *parent);
        ~SettingsOggVorbis() override;
        void retranslate() override;
        void load(const Config &config) override;
        void save(Config &config, QSettings &settings) override;
};

class SettingsOpus : public SettingsWidgetBase
{
    Q_OBJECT

    private:
        std::unique_ptr<Ui::SettingsOpus> ui;

    private slots:
        void on_opus_convert_r128_box_stateChanged(int state);

    public:
        SettingsOpus(const Config &config, QWidget *parent);
        ~SettingsOpus() override;
        void retranslate() override;
        void load(const Config &config) override;
        void save(Config &config, QSettings &settings) override;
};

class CleanDestWarningDialog : public QDialog
{
    Q_OBJECT

private slots:
    void on_accept_box_clicked(bool checked);

private:
    std::unique_ptr<Ui::CleanDestWarningDialog> ui;

public:
    explicit CleanDestWarningDialog(QWidget *parent = nullptr);
    ~CleanDestWarningDialog();
    void closeEvent(QCloseEvent *event) override;
};
