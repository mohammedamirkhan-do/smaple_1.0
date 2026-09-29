QT       += core gui widgets

CONFIG   += c++17
CONFIG   -= app_bundle console

TARGET    = SampleApp
TEMPLATE  = app

SOURCES  += src/main.cpp \
            src/mainwindow.cpp

HEADERS  += src/mainwindow.h

# Default (compile-time) fallback values.
# These match https://github.com/Amirk9/Sample_0.1.git
# At runtime the app tries to read the real `git remote` so
# "whatever the git repository name is" gets displayed.
DEFINES  += 'DEFAULT_REPO_NAME="Sample_0.1"'
DEFINES  += 'DEFAULT_REPO_URL="https://github.com/Amirk9/Sample_0.1.git"'

