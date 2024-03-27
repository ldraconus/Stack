#include "WordDialog.h"
#include "ui_WordDialog.h"

WordDialog::WordDialog(Fifth::VM* vm, QWidget *parent)
    : QDialog(parent)
    , mVM(vm)
    , ui(new Ui::WordDialog)
    , mOk(nullptr) {
    ui->setupUi(this);

    ui->comboBox->addItems(toStringList(mVM->getCompiled()));

    connect(ui->comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(selectionChanged(int)));

    mOk = ui->buttonBox->button(QDialogButtonBox::Ok); // NOLINT
    mOk->setDisabled(ui->comboBox->currentText().isEmpty());
}

WordDialog::~WordDialog() {
    delete ui;
}

QString WordDialog::word() {
    return ui->comboBox->currentText();
}

const QStringList WordDialog::toStringList(const std::vector<std::wstring>& inputList) {
    QStringList names;
    for (const auto name: inputList) names.append(QString::fromStdWString(name));
    return names;
}

void WordDialog::selectionChanged(int) {
    mOk->setDisabled(ui->comboBox->currentText().isEmpty());
}
