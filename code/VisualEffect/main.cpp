#include "visualeffect.hpp"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    VisualEffect w;
    w.show();

    return a.exec();
}
