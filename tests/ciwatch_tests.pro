QT += core gui network quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = ciwatch_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/demo.h ../src/github.h ../src/logmodel.h ../src/theme.h
SOURCES += ciwatch_tests.cpp ../src/backend.cpp ../src/demo.cpp ../src/github.cpp ../src/logmodel.cpp ../src/theme.cpp
