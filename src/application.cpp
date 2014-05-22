#include "application.h"
#include <QDebug>
#include <QUrl>

Application::Application(int &argc, char **argv, int applicationFlags) : QGuiApplication(argc, argv, applicationFlags) {

}

void Application::initialize() {
	engine_ = new QQmlApplicationEngine();
	engine_->load(QUrl(QStringLiteral("qrc:///main.qml")));

#ifdef QT_DEBUG
	QPluginLoader pluginLoader("/Users/laurent/Docs/PROGS/C++/mv/plugins/build-MvBrowserPlugin-Qt_5_2_1-Debug/libechoplugin_debug.dylib");
#else
	QPluginLoader pluginLoader("/Users/laurent/Docs/PROGS/C++/mv/plugins/build-MvBrowserPlugin-Qt_5_2_1-Release/libechoplugin.dylib");
#endif

	QObject *plugin = pluginLoader.instance();
	if (plugin) {
		MvPluginInterface *mvPlugin;
		mvPlugin = qobject_cast<MvPluginInterface *>(plugin);
		mvPlugin->onInitialize(dynamic_cast<IApplication*>(this));
		plugins_.push_back(mvPlugin);
		//qDebug() << mvPlugin->description();
	}

	setImageSource(QUrl("file:///Users/laurent/Desktop/CH_12_05_2014.jpg"));

	QObject::connect(this->qmlRootObject(), SIGNAL(keypressed(int)), this, SLOT(mainWindow_keypressed(int)));
	QObject::connect(this->qmlRootObject(), SIGNAL(sourceSelected(QString)), this, SLOT(mainWindow_sourceSelected(QString)));
}

Application* Application::instance() {
	Application* application = static_cast<Application*>(QGuiApplication::instance());
	return application;
}

QUrl Application::imageSource() const {
	return imageSource_;
}

void Application::setImageSource(const QUrl &source) {
	if (source == imageSource_) return;
	imageSource_ = source;
	onImageSourceChange();
}

void Application::setImageSource(const QString &source) {
	// TODO: handle URLs
	if (source.left(7) == "file://") {
		this->setImageSource(QUrl(source));
	} else {
		this->setImageSource(QUrl("file://" + source));
	}
}

QObject* Application::qmlRootObject() const {
	return engine_->rootObjects().first();
}

QObject* Application::qmlImage() const {
	return qmlRootObject()->findChild<QObject*>("image");
}

void Application::mainWindow_keypressed(int key) {
	for (unsigned int i = 0; i < plugins_.size(); i++) {
		MvPluginInterface* plugin = plugins_[i];
		KeypressedEvent event;
		event.keyCode = key;
		plugin->onKeypressed(event);
	}
}

void Application::mainWindow_sourceSelected(QString source) {
	this->setImageSource(source);
}

void Application::onImageSourceChange() {
	qmlImage()->setProperty("source", imageSource_);
}
