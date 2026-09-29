QT       += core gui widgets

CONFIG   += c++17
CONFIG   -= app_bundle console

TARGET    = SampleApp
TEMPLATE  = app

SOURCES  += src/main.cpp \
            src/mainwindow.cpp

HEADERS  += src/mainwindow.h

# NOTE: default repo fallback is hardcoded in src/mainwindow.cpp
# (avoids qmake/MSVC DEFINES quoting issues with URLs).
# Runtime still reads `git config --get remote.origin.url` so
# whatever the git repository name is gets displayed on top UI.


