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
class QPixmap;

class AboutDialog : public QDialog
{
    Q_OBJECT

private:
    std::unique_ptr<Ui::AboutDialog> ui;

    void ffmpeg_version(unsigned(*fn)(), QLabel *label);

public:
    void closeEvent(QCloseEvent *event) override { event->accept(); }
    explicit AboutDialog(const Config &config, const QPixmap &icon, QWidget *parent = nullptr);
    ~AboutDialog() override;
};
