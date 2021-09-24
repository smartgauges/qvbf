QT += widgets

TARGET = qvbf
TEMPLATE = app

SOURCES += main.cpp vbffile.cpp vbfmodel.cpp wdg_hexview.cpp
HEADERS += main.h vbffile.h vbfmodel.h wdg_hexview.h spinbox.h
FORMS += main.ui

RESOURCES += qvbf.qrc

CONFIG += static

static {

	win32 {

		#QTPLUGIN += qico
		QMAKE_LFLAGS += -static -static-libgcc
	}
}

