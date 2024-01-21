#include <string>
#include <filesystem>
#include <algorithm>
#include <utility>
#include <vector>
#include <queue>
#include <memory>
#include <chrono>
#include <atomic>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif

#include <QApplication>
#include <QTranslator>
#include <QMessageBox>
#include <QFileDialog>
#include <QStyle>
#include <QStyleOption>
#include <QIcon>
#include <fmt/core.h>
#include <fmt/chrono.h>

#include "main.hpp"
#include "sync.hpp"
#include "config.hpp"
#include "util.hpp"
#include "settings.hpp"
#include "about.hpp"
#include <easync.hpp>

#include "ui_mainwindow.h"

#define LOG_FILENAME EXECUTABLE_NAME ".log"

Application::Application(QWidget *parent) : QMainWindow(parent), style(QApplication::style()), settings(EXECUTABLE_NAME, EXECUTABLE_NAME)
{
    config->check_features();
    config->load(settings);
#ifdef _WIN32
    char buffer[MAX_PATH];
    if (GetEnvironmentVariableA("USERPROFILE", buffer, sizeof(buffer)))
        log_path = join_paths(buffer, "." EXECUTABLE_NAME, LOG_FILENAME);
#else
#ifdef __APPLE__
    log_path = join_paths(getenv("HOME"), "Library", EXECUTABLE_NAME, LOG_FILENAME);
#else
    const char* const XDG_DATA = getenv("XDG_DATA_HOME");
    if (XDG_DATA)
        log_path = join_paths(XDG_DATA, EXECUTABLE_NAME, LOG_FILENAME);
    else
        log_path = join_paths(getenv("HOME"), ".local", "share", EXECUTABLE_NAME, LOG_FILENAME);
#endif
#endif

    // Load translator
    if (config->lang.isEmpty()) {
        if (!translator.load(QLocale(), "", "", ":/translations"))
            std::ignore = translator.load("en", ":/translations");
        QString lang(QFileInfo(translator.filePath()).baseName());
        settings.setValue("language", lang);
    }
    else
        std::ignore = translator.load(config->lang, ":/translations");
    QCoreApplication::installTranslator(&translator);
    ui = new Ui::MainWindow();
    ui->setupUi(this);
    ui->stop_button->setEnabled(false);
    ui->stop_button->setIcon(style->standardIcon(QStyle::SP_BrowserStop));
#ifdef _WIN32
    ui->stop_button->setIconSize({ 22, 22 });
    ui->stop_button->setStyleSheet("padding: 4px;");
#else
    ui->stop_button->setStyleSheet("padding: 2px;");
#endif
    read_settings();

    set_sync_button_state();
}

Application::~Application()
{
    delete ui;
}

void Application::read_settings()
{
    QString source_dir = settings.value("source_dir").toString();
    if (!source_dir.isEmpty()) {
        std::filesystem::path path(source_dir.toStdString());
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            ui->source_dir_box->setText(source_dir);
            ui->source_dir_box->setCursorPosition(0);
            source = std::move(path);
        }
    }
    QString dest_dir = settings.value("dest_dir").toString();
    if (!dest_dir.isEmpty()) {
        std::filesystem::path path(dest_dir.toStdString());
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            ui->dest_dir_box->setText(dest_dir);
            ui->dest_dir_box->setCursorPosition(0);
            dest = std::move(path);
        }
    }
#ifdef PERSIST_GEOMETRY
    restore_window_size(this, "MainWindow", settings);
    restore_window_pos(this, "MainWindow", settings);
#endif
}

void Application::write_settings()
{
    settings.setValue("settings_version", SETTINGS_VERSION);
    settings.setValue("source_dir", ui->source_dir_box->text());
    settings.setValue("dest_dir", ui->dest_dir_box->text());
#ifdef PERSIST_GEOMETRY
    save_window_size(this, "MainWindow", settings);
    save_window_pos(this, "MainWindow", settings);
#endif
}

void Application::on_browse_source_button_clicked()
{
    open_directory(source, ui->source_dir_box, tr("Select Source Directory"));
}

void Application::on_browse_dest_button_clicked()
{
    open_directory(dest, ui->dest_dir_box, tr("Select Destination Directory"));
}

void Application::open_directory(std::filesystem::path &path, QLineEdit *box, const QString &title)
{
    QString dir = QFileDialog::getExistingDirectory(this, title, box->text());
    if (!dir.isNull()) {
#ifdef _WIN32
        dir = QDir::toNativeSeparators(dir);
#endif
            box->setText(dir);
            box->setCursorPosition(0);
            path = dir.toStdString();
    }
    set_sync_button_state();
}

void Application::show_dialog(QMessageBox::Icon icon, const QString &&title, const QString &&message)
{
    auto msg_box = new QMessageBox(icon,
        title,
        message,
        QMessageBox::Ok,
        this
    );
    msg_box->setModal(true);
    msg_box->setAttribute(Qt::WA_DeleteOnClose, true);
    msg_box->show();
}

void Application::on_sync_button_clicked()
{
    if (!std::filesystem::exists(source)) {
        show_dialog(QMessageBox::Critical, tr("Error"), tr("The source directory does not exist."));
        return;
    }
    if (!std::filesystem::exists(dest)) {
        show_dialog(QMessageBox::Critical, tr("Error"), tr("The destination directory does not exist."));
        return;
    }
    if (source == dest) {
        show_dialog(QMessageBox::Critical, tr("Error"), tr("The source and destination directories cannot be the same."));
        return;
    }

    sync = new Sync(source, dest, log_path, *this, *config, stop_flag);
    sync->moveToThread(&thread);
    connect(sync, &Sync::finished, this, &Application::sync_finished);
    connect(this, &Application::sync_begin, sync, &Sync::begin);
    ui->sync_button->setEnabled(false);
    ui->stop_button->setEnabled(true);
    ui->action_settings->setEnabled(false);
    ui->browse_source_button->setEnabled(false);
    ui->browse_dest_button->setEnabled(false);
    stop_flag.clear();
    quit_flag = false;
    ui->console->clear();

    thread.start();
    emit sync_begin();
    begin = std::chrono::system_clock::now();
}

void Application::on_stop_button_clicked()
{
    message(tr("Stop requested"));
    ui->stop_button->setEnabled(false);
    stop_flag.test_and_set();
}

void Application::set_sync_button_state()
{
    ui->sync_button->setEnabled(!(ui->source_dir_box->text().isEmpty() || ui->dest_dir_box->text().isEmpty()));
}

void Application::on_action_settings_triggered()
{
    auto dialog = new Settings(*config, settings, translator, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setModal(true);
    dialog->show();
}

void Application::on_action_about_triggered()
{
    auto dialog = new AboutDialog(*config, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setModal(true);
    dialog->show();
}

void Application::on_action_quit_triggered()
{
    if (sync) {
        auto msg_box = new QMessageBox(QMessageBox::Question,
          tr("Quit?"),
          tr("Are you sure you want to quit while a sync is in-progress?"),
          QMessageBox::Yes | QMessageBox::No,
          this
        );
        connect(msg_box, &QMessageBox::finished, this, &Application::quit_confirm_finished);
        connect(this, &Application::close_dialogs, msg_box, &QMessageBox::close);
        msg_box->setModal(true);
        msg_box->setAttribute(Qt::WA_DeleteOnClose, true);
        msg_box->show();
    }
    else
        quit();
}

void Application::quit_confirm_finished(int result)
{
    if (result == QMessageBox::Yes) {
        if (!stop_flag.test())
            on_stop_button_clicked();
        quit_flag = true;
        ui->action_quit->setEnabled(false);
    }
}

void Application::quit()
{
    write_settings();
    QApplication::exit();
}

void Application::set_progress_bar_max(int max) 
{
    ui->progress_bar->setMaximum(max);
}

void Application::set_progress_bar(int val)
{
    ui->progress_bar->setValue(val);
}

void Application::sync_finished(int res)
{
    thread.quit();
    thread.wait();
    if (quit_flag)
        quit();
    std::chrono::duration duration = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now() - begin);
    ui->sync_button->setEnabled(true);
    ui->stop_button->setEnabled(false);
    ui->speed_label->clear();
    ui->eta_label->clear();
    ui->action_settings->setEnabled(true);
    ui->browse_source_button->setEnabled(true);
    ui->browse_dest_button->setEnabled(true);
    emit close_dialogs();

    auto result = static_cast<Sync::Result>(res);
    switch(result) {
        case Sync::Result::NOTHING:
            message(tr("Nothing to do"));
            show_dialog(QMessageBox::Information, PROJECT_NAME, tr("The destination directory is already up-to-date."));
            break;

        case Sync::Result::SUCCESS:
            message(tr("Sync completed successfully in %1").arg(QString::fromStdString(fmt::format("{:%H:%M:%S}", duration))));
            show_dialog(QMessageBox::Information, PROJECT_NAME, tr("The sync completed successfully."));
            break;

        case Sync::Result::ERRORS:
            message(tr("Sync completed with one or more errors in %1").arg(QString::fromStdString(fmt::format("{:%H:%M:%S}", duration))));
            message(tr("See the log file '%1' for more information").arg(QString::fromStdString(log_path.string())));
            show_dialog(QMessageBox::Critical, tr("Error"), tr("The sync completed with one or more errors."));
            break;

        case Sync::Result::STOPPED:
            message(tr("Sync stopped"));
            break;

        case Sync::Result::ABORTED:
            message(tr("Sync aborted due to error(s)"));
            break;
    }

    delete sync;
    sync = nullptr;
}

void Application::update_speed_eta(const QString &speed, const QString &eta)
{
    ui->speed_label->setText(speed);
    ui->eta_label->setText(eta);
}

void Application::closeEvent(QCloseEvent *event)
{
    if (sync) {
        if (!quit_flag)
            on_action_quit_triggered();
        event->ignore();
    }
    else {
        write_settings();
        event->accept();
    }
}

void Application::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        event->accept();
    }
}

void Application::message(const QString &msg)
{
    ui->console->appendPlainText(msg);
    ui->console->verticalScrollBar()->setValue(ui->console->verticalScrollBar()->maximum());
}

int main(int argc, char *argv[])
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#ifndef FFDEBUG
    av_log_set_callback(nullptr);
#endif

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/" EXECUTABLE_NAME "256.png"));
    Application window;
    window.setWindowTitle(PROJECT_NAME);
    window.show();
    return app.exec();
}
