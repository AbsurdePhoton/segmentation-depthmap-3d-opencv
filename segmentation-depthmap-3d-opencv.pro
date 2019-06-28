#-------------------------------------------------
#
#   Segmentation to Depthmap to 3D with openCV
#
#    by AbsurdePhoton - www.absurdephoton.fr
#
#                v1.4 - 2019/05/10
#
#-------------------------------------------------

QT       += core gui opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = segmentation-depthmap-3d-opencv
TEMPLATE = app

INCLUDEPATH += /usr/local/include/opencv2

LIBS += -L/usr/local/lib -lopencv_core -lopencv_imgcodecs -lopencv_highgui

SOURCES +=  main.cpp\
            mainwindow.cpp \
            mat-image-tools.cpp \
            openglwidget.cpp

HEADERS  += mainwindow.h \
            mat-image-tools.h \
            openglwidget.h

FORMS    += mainwindow.ui

# we add the package opencv to pkg-config
CONFIG += link_pkgconfig
PKGCONFIG += opencv4

QMAKE_CXXFLAGS += -std=c++11

# icons
RESOURCES += resources.qrc
