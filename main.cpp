#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) { // NOLINT
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
