/*#-------------------------------------------------
#
#   Segmentation to Depthmap to 3D with openCV
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v1.5 - 2019/07/08
#
#-------------------------------------------------*/

#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setApplicationName("segmentation-depthmap-3d-opencv");
    a.setOrganizationName("AbsurdePhoton");
    a.setOrganizationDomain("www.absurdephoton.fr");

    MainWindow w;
    w.readPositionSettings();
    w.show();

    return a.exec();
}
