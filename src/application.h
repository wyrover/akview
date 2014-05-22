#ifndef APPLICATION_H
#define APPLICATION_H

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QUrl>
#include "pluginmanager.h"

class Application : public QGuiApplication, IApplication {
	Q_OBJECT

public:

	explicit Application(int &argc, char **argv, int applicationFlags = ApplicationFlags);
	static Application* instance();
	void initialize();

protected:

	void onImageSourceChange();
	QObject* qmlRootObject() const;
	QObject* qmlImage() const;

private:

	QUrl imageSource_;
	QQmlApplicationEngine* engine_;
	PluginManager* pluginManager_;

public slots:

	void mainWindow_keypressed(int key);
	void mainWindow_sourceSelected(QString source);
	QUrl imageSource() const;
	void setImageSource(const QUrl& source);
	void setImageSource(const QString& source);

};

#endif // APPLICATION_H
