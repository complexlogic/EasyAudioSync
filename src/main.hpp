#pragma once

#include <vector>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>

#include <QMainWindow>
#include <QMessageBox>
#include <QCloseEvent>
#include <QTranslator>
#include <QThread>
#include <QString>
#include <QSettings>
#include <QPlainTextEdit>
#include <QScrollBar>

class Sync;
struct Config;
class QLineEdit;

namespace Ui {
    class MainWindow;
}

class Application : public QMainWindow
{
    Q_OBJECT

private slots:
    void on_browse_source_button_clicked();
    void on_browse_dest_button_clicked();
    void on_sync_button_clicked();
    void on_stop_button_clicked();
    void on_action_settings_triggered();
    void on_action_about_triggered();
    void on_action_quit_triggered();

public slots:
    void sync_finished(int res);
    void quit_confirm_finished(int result);
    void set_progress_bar_max(int max);
    void set_progress_bar(int val);
    void message(const QString &msg);
    void update_speed_eta(const QString &speed, const QString &eta);

signals:
    void sync_begin();
    void stop_export();
    void close_dialogs();

private:
    std::unique_ptr<Ui::MainWindow> ui;
    std::unique_ptr<Config> config;
    QStyle *style;
    QSettings settings;
    QTranslator translator;
    QThread thread;
    Sync *sync = nullptr;
    std::filesystem::path log_path;
    std::filesystem::path source;
    std::filesystem::path dest;
    int progress_count = 0;
    std::atomic_flag stop_flag;
    bool quit_flag = false;
    std::chrono::time_point<std::chrono::system_clock> begin;

    void open_directory(std::filesystem::path &path, QLineEdit *box, const QString &title);
    void set_sync_button_state();
    void read_settings();
    void write_settings();
    void quit();
    void show_dialog(QMessageBox::Icon icon, const QString &&title, const QString &&message);

public:
    explicit Application(QWidget *parent = nullptr);
    ~Application();
    void closeEvent(QCloseEvent *event);
    void changeEvent(QEvent *event);
};
