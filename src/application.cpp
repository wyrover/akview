#include <QCommandLineParser>
#include <QDebug>
#include <QFileDialog>
#include <QFileOpenEvent>
#include <QMessageBox>
#include <QProcess>
#include <QUrl>

#include "application.h"
#include "constants.h"
#include "paths.h"
#include "settings.h"
#include "version.h"

// TODO: add version info to About box
// TODO: JpegTools: put focus on select radiobutton
// TODO: UI to change shortcuts
// TODO: Exif properties plugin Ctrl + I

namespace mv {

Application::Application(int &argc, char **argv, int applicationFlags) : QApplication(argc, argv, applicationFlags) {
	settings_ = NULL;
	preferencesDialog_ = NULL;

	Application::setOrganizationName(VER_COMPANYNAME_STR);
	Application::setOrganizationDomain(VER_DOMAIN_STR);
	Application::setApplicationName(APPLICATION_TITLE);
}

Application::~Application() {
	onExit();
}

void Application::initialize() {
	QCommandLineParser args;
	args.addPositionalArgument("file", tr("File to open."));
	args.process(*this);

	engine_ = new QQmlApplicationEngine();
	engine_->load(QUrl(QStringLiteral("qrc:///main.qml")));

	setWindowTitle(APPLICATION_TITLE);

	QObject* win = qmlApplicationWindow();
	Settings settings;
	QVariant v;
	settings.beginGroup("applicationWindow");
	v = settings.value("width");
	if (!v.isNull()) win->setProperty("width", v);
	v = settings.value("height");
	if (!v.isNull()) win->setProperty("height", v);
	v = settings.value("x");
	if (!v.isNull()) win->setProperty("x", v);
	v = settings.value("y");
	if (!v.isNull()) win->setProperty("y", v);
	settings.endGroup();

	Paths paths;

	pluginManager_ = new PluginManager(dynamic_cast<IApplication*>(this));
	pluginManager_->loadPlugins(paths.pluginFolder());

	QObject::connect(this->qmlRootObject(), SIGNAL(keypressed(int, const QString&, int)), this, SLOT(mainWindow_keypressed(int, const QString&, int)));
	QObject::connect(this->qmlRootObject(), SIGNAL(actionTriggered(const QString&)), this, SLOT(mainWindow_actionTriggered(const QString&)));

	QStringList filePaths = args.positionalArguments();
	if (filePaths.size() > 0) setSource(filePaths[0]);

	win->setProperty("visible", true);
}

void Application::setWindowTitle(const QString &title) {
	QString prefix;
#ifdef MV_DEBUG
	prefix = "** DEBUG ** ";
#endif // MV_DEBUG
	qmlApplicationWindow()->setProperty("title", prefix + title + " (" + version::number() + ")");
}

void Application::showPreferencesDialog() {
	if (!preferencesDialog_) {
		preferencesDialog_ = new PreferencesDialog(NULL);
		preferencesDialog_->setModal(true);
	}
	preferencesDialog_->exec();
}

PluginManager *Application::pluginManager() const {
	return pluginManager_;
}

ActionVector Application::actions() const {
	ActionVector output;
	PluginVector plugins = pluginManager()->plugins();
	for (int i = 0; i < plugins.size(); i++) {
		Plugin* plugin = plugins[i];
		ActionVector pluginActions = plugin->actions();
		output.insert(output.end(), pluginActions.begin(), pluginActions.end());
	}
	return output;
}

bool Application::event(QEvent *event) {
	switch (event->type()) {

		case QEvent::FileOpen:

			setSource(static_cast<QFileOpenEvent *>(event)->file());
			return true;

		default:

			return QApplication::event(event);

	}
}

Application* Application::instance() {
	Application* application = static_cast<Application*>(QGuiApplication::instance());
	return application;
}

QString Application::source() const {
	return source_;
}

void Application::setSource(const QString &source) {
	if (source == source_) return;
	source_ = source;
	onMediaSourceChange();
}

QObject* Application::qmlRootObject() const {
	return engine_->rootObjects().first();
}

QObject* Application::qmlImage() const {
	return qmlRootObject()->findChild<QObject*>("image");
}

QObject* Application::qmlApplicationWindow() const {
	return qmlRootObject();
}

void Application::mainWindow_keypressed(int key, const QString& text, int modifiers) {
	KeypressedEvent event;
	event.keyCode = key;
	event.text = text;
	event.modifiers = modifiers;

	if (event.keyCode == Qt::Key_O && event.modifiers == Qt::ControlModifier) {
		QString filter;
		QStringList extensions = supportedFileExtensions();
		for (int i = 0; i < extensions.size(); i++) {
			QString e = extensions[i];
			if (filter != "") filter += " ";
			filter += "*." + e;
		}
		Settings settings;
		QString lastDir = settings.value("lastOpenFileDirectory").toString();
		QString filePath = QFileDialog::getOpenFileName(NULL, tr("Open File"), lastDir, tr("Supported Files (%1)").arg(filter));
		if (filePath != "") {
			setSource(filePath);
			settings.setValue("lastOpenFileDirectory", QVariant(QFileInfo(filePath).absolutePath()));
		}
		return;
	}

	if (event.keyCode == Qt::Key_W && event.modifiers == Qt::ControlModifier) {
		quit();
		return;
	}

	if (event.keyCode == Qt::Key_Left) {
		previousSource();
		return;
	}

	if (event.keyCode == Qt::Key_Right) {
		nextSource();
		return;
	}

	// Keyboard events that are not handled by the application are sent to the plugins
	pluginManager_->onKeypressed(event);
}

void Application::mainWindow_actionTriggered(const QString &name) {
	if (name == "about") {
		QMessageBox::about(NULL, tr("About %1").arg(APPLICATION_TITLE), tr("%1 %2").arg(APPLICATION_TITLE).arg(version::number()));
		return;
	}

	if (name == "preferences") {
		showPreferencesDialog();
		return;
	}
}

void Application::onMediaSourceChange() {
	qmlImage()->setProperty("source", source_ == "" ? QUrl("") : QUrl("file://" + source_));
	setWindowTitle(QFileInfo(source_).fileName());
}

void Application::onExit() {
	QObject* win = qmlApplicationWindow();
	Settings settings;

	settings.beginGroup("applicationWindow");
	settings.setValue("width", win->property("width"));
	settings.setValue("height", win->property("height"));
	settings.setValue("x", win->property("x"));
	settings.setValue("y", win->property("y"));
	settings.endGroup();
}

QStringList Application::supportedFileExtensions() const {
	QStringList output;
	output << "jpg" << "jpeg" << "png" << "gif" << "bmp" << "tif" << "tiff";
	return output;
}

void Application::setSourceIndex(int index) {
	QStringList sources = this->sources();
	if (!sources.size()) return;

	QString source = sources[index];
	sourceIndex_ = index;

	setSource(source);
}

void Application::nextSource() {
	int index = sourceIndex();
	QStringList sources = this->sources();
	index++;
	if (index >= sources.size()) index = 0;
	setSourceIndex(index);
}

void Application::previousSource() {
	int index = sourceIndex();
	QStringList sources = this->sources();
	index--;
	if (index < 0) index = sources.size() - 1;
	setSourceIndex(index);
}

int Application::sourceIndex() const {
	QString source = this->source();

	if (QFileInfo(source).dir().absolutePath() != sourceDir_) {
		// Current dir has changed - reload source list
		sourceIndex_ = -1;
		sources_.clear();
	}

	QStringList sources = this->sources();
	if (!sources.size()) return -1;

	source = QFileInfo(source).fileName();

	// Check if the index we have is correct
	if (sourceIndex_ >= 0 && sourceIndex_ < sources.size() && sources[sourceIndex_] == source) return sourceIndex_;

	// If it's not correct, try to get it from the current source and source list
	sourceIndex_ = -1;

	for (int i = 0; i < sources.size(); i++) {
		if (QFileInfo(sources[i]).fileName() == source) {
			sourceIndex_ = i;
			break;
		}
	}
	return sourceIndex_;
}

void Application::refreshSources() {
	sources_.clear();
	sourceIndex_ = -1;
}

void Application::reloadSource() const {
	QString currentSource = qmlImage()->property("source").toString();
	qmlImage()->setProperty("source", "");
	qmlImage()->setProperty("source", currentSource);
}

void Application::exifClearOrientation(const QString &filePath) {
	JheadHandler handler(filePath);
	handler.clearOrientation();
}

bool Application::runAppleScript(const QString &script) {
	QStringList scriptArgs;
	scriptArgs << QLatin1String("-e") << script;
	int exitCode = QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);

	if (exitCode == -2) {
		qWarning() << "runAppleScript: the process could not be started:" << script;
		return false;
	}

	if (exitCode == -1) {
		qWarning() << "runAppleScript: the process crashed:" << script;
		return false;
	}

	if (exitCode > 0) {
		qWarning() << "runAppleScript: the process exited with error code " << exitCode;
		return false;
	}

	return true;
}

Settings *Application::settings() const {
	if (settings_) return settings_;
	settings_ = new Settings();
	return settings_;
}

QStringList Application::sources() const {
	if (sources_.length()) return sources_;

	QStringList supportedFileExtensions = this->supportedFileExtensions();
	sourceIndex_ = -1;
	QString source = this->source();

	QDir dir = QFileInfo(source).dir();
	sourceDir_ = dir.absolutePath();
	QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::LocaleAware);
	for (int i = 0; i < files.size(); i++) {
		if (!supportedFileExtensions.contains(files[i].suffix().toLower())) continue;
		sources_.append(files[i].absoluteFilePath());
	}

	return sources_;
}

}
