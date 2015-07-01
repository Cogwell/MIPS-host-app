#include "mips.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MIPS w;
    w.show();

    return a.exec();
}
