TEMPLATE = app

QT += widgets script

macx {
	QT += macextras
}

CONFIG += precompile_header

PRECOMPILED_HEADER = stable.h

CONFIG(debug, debug|release):TARGET = MultiViewer-debug
CONFIG(release, debug|release):TARGET = MultiViewer

equals(AK_IS_DEBUGRELEASE, "1") {
	DEFINES += "AK_IS_DEBUGRELEASE"
}

HEADERS += \
	action.h \
	actionlistitemwidget.h \
	actionthread.h \
	application.h \
	consolewidget.h \
	constants.h \
	exif.h \
	iapplication.h \
	messageboxes.h \
	mvplugininterface.h \
	packagemanager.h \
	paths.h \
	plugin.h \
	pluginevents.h \
	pluginmanager.h \
	preferencesdialog.h \
	processutil.h \
	scriptutil.h \
	settings.h \
	simplefunctions.h \
	simpletypes.h \
	stringutil.h \
	version.h \
    mainwindow.h \
    progressbardialog.h \
    jsapi/jsapi_application.h \
    jsapi/jsapi_console.h \
    jsapi/jsapi_fileinfo.h \
    jsapi/jsapi_imaging.h \
    jsapi/jsapi_input.h \
    jsapi/jsapi_plugin.h \
    jsapi/jsapi_ui.h \
    jsapi/jsapi_system.h \
    batchdialog.h

SOURCES += main.cpp \
	action.cpp \
	actionlistitemwidget.cpp \
	actionthread.cpp \
	application.cpp \
	consolewidget.cpp \
	exif.cpp \
	messageboxes.cpp \
	packagemanager.cpp \
	paths.cpp \
	plugin.cpp \
	pluginmanager.cpp \
	preferencesdialog.cpp \
	processutil.cpp \
	settings.cpp \
	scriptutil.cpp \
	stringutil.cpp \
	version.cpp \
    mainwindow.cpp \
    progressbardialog.cpp \
    jsapi/jsapi_application.cpp \
    jsapi/jsapi_console.cpp \
    jsapi/jsapi_fileinfo.cpp \
    jsapi/jsapi_imaging.cpp \
    jsapi/jsapi_input.cpp \
    jsapi/jsapi_plugin.cpp \
    jsapi/jsapi_ui.cpp \
    jsapi/jsapi_system.cpp \
    batchdialog.cpp

RESOURCES += resources.qrc

macx {
	INCLUDEPATH += "/usr/local/Cellar/freeimage/3.15.4/include"
	LIBS += /usr/local/Cellar/freeimage/3.15.4/lib/libfreeimage.dylib
}

unix {
	INCLUDEPATH += /usr/include
	LIBS += /usr/lib/libfreeimage.so
}

FORMS += \
	preferencesdialog.ui \
    mainwindow.ui \
    progressbardialog.ui \
    batchdialog.ui
