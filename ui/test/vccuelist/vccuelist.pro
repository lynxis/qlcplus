include(../../../variables.pri)

TEMPLATE = app
LANGUAGE = C++
TARGET   = vccuelist_test

QT      += testlib xml gui script
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

INCLUDEPATH += ../../../plugins/interfaces
INCLUDEPATH += ../../../engine/src
INCLUDEPATH += ../../src
INCLUDEPATH += $${OUT_PWD}/../../src/

DEPENDPATH  += ../../src

QMAKE_LIBDIR += ../../../engine/src
QMAKE_LIBDIR += ../../src
LIBS        += -lqlcplusengine -lqlcplusui

# Test sources
SOURCES += vccuelist_test.cpp
HEADERS += vccuelist_test.h
