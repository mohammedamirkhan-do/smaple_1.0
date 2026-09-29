QT       += core gui widgets network

CONFIG   += c++17
CONFIG   -= app_bundle console

TARGET    = SampleApp
TEMPLATE  = app

SOURCES  += src/main.cpp \
            src/mainwindow.cpp \
            src/updatemanager.cpp

HEADERS  += src/mainwindow.h \
            src/updatemanager.h \
            src/version.h

# NOTE: default repo fallback is hardcoded in src/mainwindow.cpp
# (avoids qmake/MSVC DEFINES quoting issues with URLs).
# Runtime still reads `git config --get remote.origin.url` so
# whatever the git repository name is gets displayed on top UI.
# App version lives in src/version.h (1.2 on this branch).


