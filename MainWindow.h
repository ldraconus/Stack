#pragma once

#include <QAbstractButton>
#include <QMainWindow>
#include <QTableWidget>

#include "Fifth.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow: public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    NO(MainWindow);

    bool checkAndClose();
    void closeEvent(QCloseEvent*) override;

private:
    Ui::MainWindow *ui;
         Fifth::VM mVM;
              bool mRunning = true;
           QString mWord;

    enum testType { RUN, STACK, DUMP, OUT };

    void doNothing() { }

           void justClose();
           void runTests();
           void setCell(QTableWidget* tbl, int row, int col, QString val);
           bool testExpression(testType, const QString& in = "", const QString& out = "");
    QStringList toStringList(const std::vector<std::wstring>& inputList);
           void update();
           void updateCode(const QStringList& code);
           void updateStack(QTableWidget* table, const QStringList& contents);
           void updateVars(QTableWidget* table, const QStringList& vars);

public slots:
    void exitTrigger();
    void getWord();
    void stepOver();
};

class Msg: public QObject {
    Q_OBJECT

public:
    static std::shared_ptr<class QMessageBox> Box; // NOLINT

    static std::function<void ()> _Cancel; // NOLINT
    static std::function<void ()> _No; // NOLINT
    static std::function<void ()> _Ok; // NOLINT
    static std::function<void ()> _Yes; // NOLINT

public slots:
    void button(QAbstractButton*);
};

