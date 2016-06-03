#-------------------------------------------------
#
# Project created by QtCreator 2016-06-01T18:53:25
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QTChatClient
TEMPLATE = app


SOURCES += main.cpp\
        chatclient.cpp\
        chatserver.cpp

INCLUDEPATH = "/Users/patrickmintram/Documents/Programming/PatChat/QTChatClient/"

HEADERS  += chatclient.h \
    chatserver.h

FORMS    += chatclient.ui
