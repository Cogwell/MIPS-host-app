#-------------------------------------------------
#
# Project created by QtCreator 2015-06-27T10:54:52
#
#-------------------------------------------------

QT       += core gui
QT       += serialport
QT       += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MIPS
TEMPLATE = app

CONFIG += static

SOURCES += main.cpp\
        mips.cpp \
    console.cpp \
    settingsdialog.cpp \
    ringbuffer.cpp \
    pse.cpp

HEADERS  += mips.h \
    console.h \
    settingsdialog.h \
    ringbuffer.h \
    pse.h

FORMS    += mips.ui \
    settingsdialog.ui \
    pse.ui

RESOURCES += \
    files.qrc

QMAKE_LFLAGS_WINDOWS = /SUBSYSTEM:WINDOWS,5.01

