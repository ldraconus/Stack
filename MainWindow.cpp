#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "WordDialog.h"

#include <QCloseEvent>
#include <QMessageBox>

#include <functional>

#include "cstdio.h"

std::shared_ptr<class QMessageBox> Msg::Box; // NOLINT
std::function<void()> Msg::_Cancel; // NOLINT
std::function<void()> Msg::_No; // NOLINT
std::function<void()> Msg::_Ok; // NOLINT
std::function<void ()> Msg::_Yes; // NOLINT

Msg msngr; // NOLINT

void Msg::button(QAbstractButton* btn) {
    QString txt = btn->text();
    if (txt == "Cancel") Msg::_Cancel();
    else if (txt == "&No") Msg::_No();
    else if (txt == "&Ok") Msg::_Ok();
    else if (txt == "&Yes") Msg::_Yes();
}

void YesNo(const QString& msg, std::function<void()> yes, std::function<void()> no, const QString title = "") {
    Msg::Box = std::make_shared<QMessageBox>();
    Msg::Box->connect(Msg::Box.get(), SIGNAL(buttonClicked(QAbstractButton*)), &msngr, SLOT(button(QAbstractButton*)));
    Msg::_Yes = yes;
    Msg::_No  = no;
    Msg::Box->setIcon(QMessageBox::Question);
    Msg::Box->setText(!title.isEmpty() ? title : "Are you sure?");
    Msg::Box->setInformativeText(msg);
    Msg::Box->setStandardButtons({ QMessageBox::Yes, QMessageBox::No });
    Msg::Box->open();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);

    ui->actionRun->setDisabled(true);
    ui->actionSet_Breakpoint->setDisabled(true);
    ui->actionStep_Into->setDisabled(true);
    ui->actionStep_Over->setDisabled(true);

    connect(ui->action_Exit,     SIGNAL(triggered()), this, SLOT(exitTrigger()));
    connect(ui->action_Word,     SIGNAL(triggered()), this, SLOT(getWord()));
    connect(ui->actionStep_Over, SIGNAL(triggered()), this, SLOT(stepOver()));

    runTests();
    update();
}

MainWindow::~MainWindow() {
    delete ui;
}

bool MainWindow::checkAndClose()
{
    YesNo("Are you sure you want exit??",
          std::bind(&MainWindow::justClose, this),
          std::bind(&MainWindow::doNothing, this),
          "Currently debugging!");
    return true;
}

void MainWindow::justClose() {
    mRunning = false;
    close(); // really exit this time
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (mRunning) {
        checkAndClose();
        event->ignore();
    } else event->accept();
}

void MainWindow::runTests() {
    bool result = true;
    bool test = true;

    result &= testExpression(RUN, "'Hello, world!' print 13 ch 10 ch");
    result &= testExpression(RUN, "def nl"
                                  " 13 ch"
                                  " 10 ch "
                                  "end");
    result &= testExpression(OUT, "dbg nl:");
    result &= testExpression(RUN, "dbg nl");
    result &= testExpression(RUN, "'Hello, world!' print nl");
    result &= testExpression(RUN, "( 1 + 2 * 3 )", "7");
    result &= testExpression(RUN, "def math"
                                  " ( 1 + 2 * 3 ) "
                                  "end");
    result &= testExpression(OUT, "dbg math:");
    result &= testExpression(RUN, "dbg math");
    result &= testExpression(RUN, "math", "7");
    result &= testExpression(RUN, "def if-test"
                                  " if math 7 = then"
                                  "  'Yes'"
                                  " else"
                                  "  'No'"
                                  " endif"
                                  " print nl "
                                  "end");
    result &= testExpression(OUT, "dbg if-test:");
    result &= testExpression(RUN, "dbg if-test");
    result &= testExpression(RUN, "if-test");
    result &= testExpression(RUN, "def if-test2"
                                  " if ( math = 7 ) then"
                                  "  'Yes'"
                                  " else"
                                  "  'No'"
                                  " endif"
                                  " print nl "
                                  "end");
    result &= testExpression(OUT, "dbg if-test2:");
    result &= testExpression(RUN, "dbg if-test2");
    result &= testExpression(RUN, "if-test2");
    result &= testExpression(RUN, "def while-test"
                                  " var x"
                                  " x 10 <- "
                                  " while ( *x <> 0 ) do"
                                  "  x print"
                                  "  ' ' print"
                                  "  x ( *x - 1 ) <-"
                                  " done"
                                  " nl "
                                  "end");
    result &= testExpression(OUT, "dbg while-test:");
    result &= testExpression(RUN, "dbg while-test");
    result &= testExpression(RUN, "while-test");
    result &= testExpression(RUN, "def for-test"
                                  " for x 1 10 each"
                                  "  x print"
                                  "  ' ' print"
                                  " next"
                                  " nl "
                                  "end");
    result &= testExpression(OUT, "dbg for-test:");
    result &= testExpression(RUN, "dbg for-test");
    result &= testExpression(RUN, "for-test");
    result &= testExpression(RUN, "def for-test2"
                                  " for x 1 10 by 2 each"
                                  "  x print"
                                  "  ' ' print"
                                  " next"
                                  " nl "
                                  "end");
    result &= testExpression(OUT, "dbg for-test2:");
    result &= testExpression(RUN, "dbg for-test2");
    result &= testExpression(RUN, "for-test2");
    result &= testExpression(RUN, "1 2 swap", "2 1");
    result &= testExpression(RUN, "var test");
    result &= testExpression(RUN, "test 12 <-");
    result &= testExpression(RUN, "test get", "12");
    result &= testExpression(RUN, "( 1 + *test )", "13");
    result &= testExpression(RUN, "array a 10");
    result &= testExpression(RUN, "a 1 + 1 <-");
    result &= testExpression(RUN, "a 2 + 2 <- a 1 + get a 2 + get", "1 2");
    result &= testExpression(RUN, "'this,is,a,test' ',' /", "this is a test 4");
    result &= testExpression(RUN, "'this' len", "4");
    result &= testExpression(RUN, "'this' explode", "t h i s 4");
    result &= testExpression(DUMP);
    cstd::out.putString("------\r\n");
    cstd::out.putString(QString("[%1] %2\r\n").arg(result ? "PASS" : "FAIL", result ? "All tests passed" : "Some tests failed").toStdWString());
    cstd::out.flush();
}

void MainWindow::setCell(QTableWidget* tbl, int row, int col, QString val) {
    QLabel* cell = new QLabel(val); // NOLINT
    cell->setFont(tbl->font());
    tbl->setCellWidget(row, col, cell);
}

bool MainWindow::testExpression(testType type, const QString& in, const QString& out) {
    static QList<QString> run;

    if (type != OUT) mVM.execute(in.toStdWString());
    bool ret = true;
    if (type == STACK) {
        QString result = QString::fromStdWString(mVM.debugUserStack());
        ret = result == out;
        cstd::out.putString(QString("[%1] %2 --> %3\r\n").arg(ret ? "PASS" : "FAIL", in, "'" + out + (ret ? "'" : "' got '" + result + "'")).toStdWString());
    } else if (type == OUT) {
        cstd::out.putString((in + "\r\n").toStdWString());
    } else if (type == RUN) {
        QString result = QString::fromStdWString(mVM.debugUserStack());
        ret = result == out;
        run.append(QString("[%1] %2 --> %3\r\n").arg(ret ? "PASS" : "FAIL", in, "'" + out + (ret ? "'" : "' got '" + result + "'")));
    } else if (type == DUMP) {
        for (const auto& str: run) cstd::out.putString(str.toStdWString().c_str());
    } else {
        cstd::out.putString(QString("[PASS] %1 --> NOCHECK\r\n").arg(in).toStdWString());
    }
    return ret;
}

QStringList MainWindow::toStringList(const std::vector<std::wstring>& inputList) {
    QStringList names;
    for (const auto name: inputList) names.append(QString::fromStdWString(name));
    return names;
}

void MainWindow::update() {
    QStringList code;
    if (!mWord.isEmpty() && mWord.toStdWString() != mVM.debugging()) code = toStringList(mVM.debug(mWord.toStdWString()));
    if (code.empty() && mWord.isEmpty()) {
        ui->actionRun->setDisabled(true);
        ui->actionSet_Breakpoint->setDisabled(true);
        ui->actionStep_Into->setDisabled(true);
        ui->actionStep_Over->setDisabled(true);
        ui->codeTable->setRowCount(0);
    }

    updateCode(code);
    updateStack(ui->userTable, toStringList(mVM.user()));
    updateStack(ui->systemTable, toStringList(mVM.system()));
    updateVars(ui->globalTable, mWord.isEmpty() ? QStringList() : toStringList(mVM.globalVars()));
    updateVars(ui->localTable, mWord.isEmpty() ? QStringList() : toStringList(mVM.localVars()));
}

void MainWindow::updateCode(const QStringList& code) {
    auto horizontalHeader = ui->codeTable->horizontalHeader();
    horizontalHeader->setStretchLastSection(true);
    horizontalHeader->setDefaultSectionSize(10); // NOLINT
    horizontalHeader->setDefaultAlignment(Qt::AlignLeft);
    ui->codeTable->setHorizontalHeaderLabels({ "OpCode", "Argmuents" });
    if (!code.isEmpty()) {
        ui->codeTable->clear();
        ui->codeTable->setRowCount(code.size());  // NOLINT
        int row = 0;
        for (const auto& line: code) {
            auto parts = line.split(",");
            setCell(ui->codeTable, row, 0, parts[1]);
            if (parts.size() > 2) {
                QString content;
                for (int i = 2; i < parts.size(); ++i) content += ((i == 2) ? "" : ",") + parts[i];
                setCell(ui->codeTable, row, 1, content);
            }
            ++row;
        }
    }
    for (int i = 0; i < ui->codeTable->rowCount(); ++i) ui->codeTable->resizeRowToContents(i);
    for (int i = 0; i < ui->codeTable->columnCount(); ++i) ui->codeTable->resizeColumnToContents(i);

    if (ui->codeTable->rowCount() != 0) ui->codeTable->selectRow(int(mVM.pc()));
}

void MainWindow::updateStack(QTableWidget* table, const QStringList& contents)
{
    table->clear();
    table->setRowCount(contents.size());  // NOLINT
    int row = 0;
    for (const auto& line: contents) {
        setCell(table, row, 0, line);
        ++row;
    }
    for (int i = 0; i < table->rowCount(); ++i) table->resizeRowToContents(i);
    for (int i = 0; i < table->columnCount(); ++i) table->resizeColumnToContents(i);
}

void MainWindow::updateVars(QTableWidget* table, const QStringList& vars)
{
    table->clear();
    auto horizontalHeader = table->horizontalHeader();
    horizontalHeader->setStretchLastSection(true);
    horizontalHeader->setDefaultSectionSize(10); // NOLINT
    horizontalHeader->setDefaultAlignment(Qt::AlignLeft);
    table->setHorizontalHeaderLabels({ "Name", "Value" });
    table->setRowCount(vars.size());  // NOLINT
    int row = 0;
    for (const auto& line: vars) {
        auto parts = line.split(",");
        setCell(table, row, 0, parts[0]);
        QString content;
        for (int i = 1; i < parts.size(); ++i) content += ((i == 1) ? "" : ",") + parts[i];
        setCell(table, row, 1, content);
        ++row;
    }
    for (int i = 0; i < table->rowCount(); ++i) table->resizeRowToContents(i);
    for (int i = 0; i < table->columnCount(); ++i) table->resizeColumnToContents(i);
}

void MainWindow::exitTrigger() {
    this->close();
}

void MainWindow::getWord() {
    WordDialog dlg(&mVM, this);
    if (dlg.exec() == QDialog::DialogCode::Accepted) {
        ui->actionRun->setEnabled(true);
        ui->actionSet_Breakpoint->setEnabled(true);
        ui->actionStep_Into->setEnabled(true);
        ui->actionStep_Over->setEnabled(true);
        mWord = dlg.word();
        update();
    }
}

void MainWindow::stepOver()
{
    mVM.stepOver();
    update();
}
