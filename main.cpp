#include "mips.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MIPS w;
    //connect(&a, SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(setWidgets(QWidget*, QWidget*)));    connect(ui->actionClear, SIGNAL(triggered()), console, SLOT(clear()));
    QObject::connect(&a, SIGNAL(focusChanged(QWidget*,QWidget*)), &w, SLOT(setWidgets(QWidget*,QWidget*)));
    w.show();

    return a.exec();
}
