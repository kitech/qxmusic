TEMPLATE = app
TARGET = id3smoke
CONFIG += console
CONFIG -= app_bundle
QT =

SOURCES = id3smoke.cpp

unix:!macx: LIBS += $$system(pkg-config --libs id3tag 2>/dev/null)
