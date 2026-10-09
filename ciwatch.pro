QT += core gui network qml quick quickcontrols2

CONFIG += c++17 release
TARGET = ciwatch
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/demo.h \
    src/github.h \
    src/logmodel.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/demo.cpp \
    src/github.cpp \
    src/logmodel.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc
