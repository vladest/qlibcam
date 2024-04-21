QT += core gui widgets multimedia multimedia-private quick qml concurrent

CONFIG += c++17
TARGET = qlibcam
TEMPLATE = app

SOURCES += common/image.cpp \
           main.cpp \
           qlibcamera.cpp \
           qlibcameramanager.cpp
HEADERS += common/image.h \
           qlibcamera.h \
           qlibcameramanager.h

RESOURCES += resources.qrc

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += libcamera

DEFINES += QT_NO_KEYWORDS
unix:!android: {
      target.path = /opt/$${TARGET}
      QMAKE_LFLAGS += "-Wl,-rpath,\'\$$ORIGIN\'"
}

!isEmpty(target.path): INSTALLS += target


