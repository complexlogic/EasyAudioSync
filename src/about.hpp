#pragma once

#include <QObject>
#include <QDialog>
#include <QEvent>
#include <QCloseEvent>

struct Config;
namespace Ui {
    class AboutDialog;
}
class QLabel;

class AboutDialog : public QDialog
{
    Q_OBJECT

private:
    std::unique_ptr<Ui::AboutDialog> ui;

    void ffmpeg_version(unsigned(*fn)(), QLabel *label);

public:
    void closeEvent(QCloseEvent *event) { event->accept(); }
    explicit AboutDialog(const Config &config, QWidget *parent = nullptr);
    ~AboutDialog() override;
};
