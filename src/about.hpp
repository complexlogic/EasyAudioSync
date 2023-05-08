#pragma once

#include <QObject>
#include <QEvent>
#include <QCloseEvent>
#include "ui_about.h"

struct Config;

class AboutDialog : public QDialog, private Ui::AboutDialog
{
    Q_OBJECT

    void closeEvent(QCloseEvent *event) { event->accept(); }

public:
    explicit AboutDialog(const Config &config, QWidget *parent = nullptr);
};
