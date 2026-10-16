TEMPLATE = app
TARGET = audio_smoke
CONFIG += console audio_smoke
CONFIG -= app_bundle
QT -= gui

SOURCES = audio_smoke.cpp

include(../audio.pri)
