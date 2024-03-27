#pragma once

#include <QDialog>
#include <QPushButton>

#include "FIfth.h"

namespace Ui {
class WordDialog;
}

class WordDialog: public QDialog {
    Q_OBJECT

public:
    explicit WordDialog(Fifth::VM* vm, QWidget *parent = nullptr);
    ~WordDialog() override;

    NO(WordDialog);

    QString word();

private:
    Ui::WordDialog *ui;
        Fifth::VM* mVM;
      QPushButton* mOk;

    const QStringList toStringList(const std::vector<std::wstring>& inputList);

public slots:
    void selectionChanged(int);
};
