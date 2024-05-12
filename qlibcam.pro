QT += core gui widgets multimedia multimedia-private quick qml concurrent

CONFIG += c++17
TARGET = qlibcam
TEMPLATE = app

SOURCES += common/image.cpp \
           ML/tensorflowfilter.cpp \
           ML/tensorflowtpuneuralnetwork.cpp \
           abstractvideofilter.cpp \
           main.cpp \
           qlibcamera.cpp \
           qlibcameramanager.cpp
HEADERS += common/image.h \
           ML/abstractneuralnetwork.h \
           ML/tensorflowfilter.h \
           ML/tensorflowtpuneuralnetwork.h \
           abstractvideofilter.h \
           qlibcamera.h \
           qlibcameramanager.h

RESOURCES += resources.qrc

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += libcamera eigen3

DEFINES += QT_NO_KEYWORDS
#DEFINES += SCODES_DEBUG
unix:!android: {
      target.path = /opt/$${TARGET}
      QMAKE_LFLAGS += "-Wl,-rpath,\'\$$ORIGIN\'"
}

!isEmpty(target.path): INSTALLS += target

include(scodes/SCodes.pri)

TENSORFLOW_PATH = $$PWD/ML/tpu
LIBS += -L$$TENSORFLOW_PATH/lib -ltensorflow-lite -ledgetpu
INCLUDEPATH += $$PWD/ML $$PWD/ML/tpu/include $$PWD/ML/tpu/include/edgetpu
