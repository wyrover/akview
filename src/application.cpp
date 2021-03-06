#include "application.h"
#include "batchdialog.h"
#include "constants.h"
#include "exif.h"
#include "paths.h"
#include "settings.h"
#include "simplefunctions.h"
#include "stringutil.h"
#include "version.h"

namespace mv {

QStringList queuedMessages_;
QMutex myMessageHandlerMutex_;

void myMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
	QMutexLocker locker(&myMessageHandlerMutex_);

	QString txt;
	switch (type) {
		case QtDebugMsg: txt = QString("%1").arg(msg); break;
		case QtWarningMsg: txt = QString("Warning: %1").arg(msg); break;
		case QtCriticalMsg: txt = QString("Critical: %1").arg(msg); break;
		case QtFatalMsg: txt = QString("Fatal: %1").arg(msg); break;
	}

	if (!Application::instance()->mainWindow()) {
		queuedMessages_ << txt;
	} else {
		// The console is populated this way so that log can work from
		// any thread.
		QMetaObject::invokeMethod(Application::instance()->mainWindow(), "consoleLog", Qt::QueuedConnection, Q_ARG(QString, txt));
	}

#ifdef QT_DEBUG
	QFile outFile(QDir::homePath() + "/mv.log");
	outFile.open(QIODevice::WriteOnly | QIODevice::Append);
	QTextStream ts(&outFile);
	ts << txt << endl;
#endif
}

Application::Application(int &argc, char **argv, int applicationFlags) : QApplication(argc, argv, applicationFlags) {
	qInstallMessageHandler(myMessageHandler);

	packageManager_ = NULL;
	mainWindow_ = NULL;
	settings_ = NULL;
	preferencesDialog_ = NULL;
	menuBar_ = NULL;
	preloadTimer_ = NULL;
	browsingDirection_ = Forward;

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

	Settings settings;

	preloadTimer_ = new QTimer(this);
	preloadTimer_->setInterval(100);
	preloadTimer_->setSingleShot(true);
	connect(preloadTimer_, SIGNAL(timeout()), this, SLOT(preloadTimer_timeout()));

	mainWindow_ = new MainWindow();

	#ifdef Q_OS_MAC
	setQuitOnLastWindowClosed(false);
	#endif

	setWindowTitle(APPLICATION_TITLE);
	loadWindowGeometry();

	Paths paths;

	pluginManager_ = new PluginManager();
	pluginManager_->loadPlugins(paths.pluginFolder());

	connect(mainWindow_, SIGNAL(keypressed(QKeyEvent*)), this, SLOT(mainWindow_keypressed(QKeyEvent*)));
	connect(mainWindow_, SIGNAL(closed()), this, SLOT(mainWindow_closed()));
	connect(&fsWatcher_, SIGNAL(fileChanged(const QString&)), this, SLOT(fsWatcher_fileChanged(const QString&)));

	QStringList filePaths = args.positionalArguments();
	if (filePaths.size() > 0) setSource(filePaths[0]);

	setupActions();

	mainWindow_->setStatusItem("dimensions", "");
	mainWindow_->setStatusItem("counter", "");
	mainWindow_->setStatusItem("zoom", "");

	refreshStatusBar();

	mainWindow_->toolbar()->addAction(actionById("zoom_out"));
	mainWindow_->toolbar()->addAction(actionById("zoom_in"));
	mainWindow_->toolbar()->addAction(actionById("rotate"));
	mainWindow_->toolbar()->addAction(actionById("previous"));
	mainWindow_->toolbar()->addAction(actionById("next"));

	refreshMenu();

	mainWindow_->showStatusBar(settings.value("showStatusBar").toBool());
	mainWindow_->showToolbar(settings.value("showToolbar").toBool());
	mainWindow_->show();

#if defined(QT_DEBUG) && !defined(AK_IS_DEBUGRELEASE)
	mainWindow_->showConsole(true);
#endif

	for (int i = 0; i < queuedMessages_.size(); i++) {
		QMetaObject::invokeMethod(mainWindow(), "consoleLog", Qt::QueuedConnection, Q_ARG(QString, queuedMessages_[i]));
	}
	queuedMessages_.clear();
}

void Application::preloadTimer_timeout() {
	QString p = browsingDirection_ == Backward ? previousSourcePath() : nextSourcePath();
	if (p == "") return;
	mainWindow_->loadSource(p);
}

void Application::fsWatcher_fileChanged(const QString& path) {
	if (path == source_) {
		if (!QFileInfo::exists(path)) {
			// File has been deleted
			int sourceIndex = this->sourceIndex();
			refreshSources();
			QStringList sources = this->sources();
			if (sources.size() == 0) {
				setSource("");
			} else {
				if (sourceIndex < 0) {
					// Shouldn't happen but sometime does. In that case, reset
					// it to 0. Behavior won't be right but better than crashing.
					qWarning() << "Application::fsWatcher_fileChanged: sourceIndex was" << sourceIndex;
					sourceIndex = 0;
				}
				if (sourceIndex >= sources.size()) sourceIndex = 0;
				setSource(sources[sourceIndex]);
			}
		} else {
			reloadSource();
		}
	}
}

void Application::refreshMenu(const QString& actionId) {
	if (actionId == "") {
		ActionVector actions = this->actions();
		for (unsigned int i = 0; i < actions.size(); i++) {
			Action* action = actions[i];
			if (action->id() == "") continue; // Safety to avoid infinite loops
			refreshMenu(action->id());
		}
	}

	Action* action = actionById(actionId);
	if (actionId == "undo") action->setEnabled(undoVector_.size() > 0);
}

void Application::pushUndoState() {
	QFile file(source());

	if (!file.open(QIODevice::ReadOnly)) {
		qWarning() << "Could not save undo information - could not open" << source();
		return;
	}

	Settings settings;
	int undoSize = settings.value("undoSize").toInt();
	while (undoVector_.size() > undoSize - 1) undoVector_.removeFirst();
	undoVector_ << file.readAll();
	refreshMenu("undo");
}

void Application::popUndoState() {
	undoVector_.removeLast();
	refreshMenu("undo");
}

void Application::undo() {
	if (undoVector_.size() <= 0) return;

	QFile file(source());
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qWarning() << "Could not read undo information - could not open" << source();
		return;
	}

	QByteArray content = undoVector_[undoVector_.size() - 1];
	qint64 writtenBytes = file.write(content);
	if (writtenBytes != content.size()) {
		qWarning() << "Restored" << writtenBytes << "bytes out of" << content.size();
		return;
	}

	popUndoState();
}

PackageManager* Application::packageManager() const {
	if (packageManager_) return packageManager_;
	packageManager_ = new PackageManager();
	return packageManager_;
}

MainWindow* Application::mainWindow() const {
	return mainWindow_;
}

void Application::setupActions() {
	menus_["File"] = new QMenu(tr("File"));
	menus_["Edit"] = new QMenu(tr("Edit"));
	menus_["View"] = new QMenu(tr("View"));
	menus_["Tools"] = new QMenu(tr("Tools"));
	menus_["Plugins"] = new QMenu(tr("Plugins"));
	menus_["Help"] = new QMenu(tr("Help"));

	QStringList menuOrder;
	menuOrder << "File" << "Edit" << "View" << "Tools" << "Plugins" << "Help";

	Action* action = NULL;

	// ===============================================================================================
	// FILE
	// ===============================================================================================

	createAction("open_file", tr("Open a file..."), "File", QKeySequence(Qt::CTRL + Qt::Key_O));
	createAction("batch_operation", tr("Batch operation..."), "File", QKeySequence(Qt::CTRL + Qt::Key_B));
	#ifdef Q_OS_MAC
	createAction("close_window", tr("Close window"), "File", QKeySequence(Qt::CTRL + Qt::Key_W));
	#else
	createAction("quit", tr("Quit"), "File", QKeySequence(Qt::CTRL + Qt::Key_Q));
	#endif

	// ===============================================================================================
	// EDIT
	// ===============================================================================================

	createAction("undo", tr("Undo"), "Edit", QKeySequence("Ctrl+Z"));

	// ===============================================================================================
	// VIEW
	// ===============================================================================================

	action = createAction("next", tr("Next"), "View", QKeySequence(Qt::Key_Right), QKeySequence("Num+Right"));
	action->setIcon(QIcon(":/icon_next.png"));
	action = createAction("previous", tr("Previous"), "View", QKeySequence(Qt::Key_Left), QKeySequence("Num+Left"));
	action->setIcon(QIcon(QPixmap(":/icon_next.png").transformed(QTransform().scale(-1, 1))));
	action = createAction("zoom_in", tr("Zoom In"), "View", QKeySequence(Qt::Key_Plus));
	action->setIcon(QIcon(":/icon_zoom_in.png"));
	action = createAction("zoom_out", tr("Zoom Out"), "View", QKeySequence(Qt::Key_Minus));
	action->setIcon(QIcon(":/icon_zoom_out.png"));
	action = createAction("rotate", tr("Rotate"), "View", QKeySequence(Qt::Key_R));
	action->setIcon(QIcon(":/icon_rotate.png"));
	createAction("toggle_console", tr("Toggle console"), "View", QKeySequence(Qt::Key_F12));
	createAction("toggle_status_bar", tr("Toggle status bar"), "View");
	createAction("toggle_toolbar", tr("Toggle tool bar"), "View");

	// ===============================================================================================
	// OTHER
	// ===============================================================================================

	createAction("close_console", tr("Close console"), "", QKeySequence(Qt::Key_Escape));
	createAction("about", tr("About"), "Help");
	createAction("preferences", tr("Preferences"), "Tools");

	PluginVector plugins = pluginManager()->plugins();
	for (unsigned int i = 0; i < plugins.size(); i++) {
		Plugin* plugin = plugins[i];
		for (unsigned int j = 0; j < plugin->actions().size(); j++) {
			Action* action = plugin->actions()[j];
			registerAction("Plugins", action);
		}
	}

	menuBar_ = mainWindow_->menubar();

	for (int i = 0; i < menuOrder.size(); i++) {
		menuBar_->addMenu(menus_[menuOrder[i]]);
	}

	refreshActionShortcuts();
}

void Application::refreshActionShortcuts() {
	Settings settings;
	settings.beginGroup("shortcuts");

	ActionVector actions = this->actions();
	for (unsigned int i = 0; i < actions.size(); i++) {
		Action* action = actions[i];
		if (settings.contains(action->id())) {
			QString shortcutString = settings.value(action->id()).toString();
			QKeySequence kv(shortcutString);
			action->setShortcut(kv);
		} else {
			action->restoreDefaultShortcut();
		}
	}

	settings.endGroup();
}

Action* Application::actionById(const QString& actionId) const {
	for (unsigned int i = 0; i < builtinActions_.size(); i++) {
		Action* action = builtinActions_[i];
		if (action->id() == actionId) return action;
	}

	PluginVector plugins = pluginManager()->plugins();
	for (unsigned int i = 0; i < plugins.size(); i++) {
		Plugin* plugin = plugins[i];
		for (unsigned int j = 0; j < plugin->actions().size(); j++) {
			Action* action = plugin->actions()[j];
			if (action->id() == actionId) return action;
		}
	}

	return NULL;
}

Action* Application::createAction(const QString& name, const QString& text, const QString& menu, const QKeySequence& shortcut1, const QKeySequence& shortcut2) {
	Action* action = new Action();
	action->setId(name);
	action->setText(text);

	QList<QKeySequence> shortcuts;
	if (!shortcut1.isEmpty()) {
		shortcuts << shortcut1;
		if (!shortcut2.isEmpty()) {
			shortcuts << shortcut2;
		}
	}

	action->setShortcuts(shortcuts);
	action->setDefaultShortcuts(shortcuts);

	builtinActions_.push_back(action);

	registerAction(menu, action);

	return action;
}

void Application::registerAction(const QString& menuName, Action* action) {
	connect(action, SIGNAL(triggered()), this, SLOT(mainWindow_actionTriggered()));

	if (menuName != "") {
		if (menus_.find(menuName) != menus_.end()) {
			menus_[menuName]->addAction(action);
		} else {
			QMenu* menuObject = new QMenu(menuName);
			menuObject->addAction(action);
			menus_[menuName] = menuObject;
		}
	}
}

void Application::setWindowTitle(const QString &title) {
	QString prefix;

#ifdef MV_DEBUG
	prefix = "** DEBUG ** ";
#endif // MV_DEBUG

#ifdef QT_DEBUG
	prefix = "** DEBUG ** ";
#endif // QT_DEBUG

	mainWindow_->setWindowTitle(prefix + title);
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
	output.insert(output.end(), builtinActions_.begin(), builtinActions_.end());

	PluginVector plugins = pluginManager()->plugins();
	for (unsigned int i = 0; i < plugins.size(); i++) {
		Plugin* plugin = plugins[i];
		ActionVector pluginActions = plugin->actions();
		output.insert(output.end(), pluginActions.begin(), pluginActions.end());
	}

	return output;
}

bool Application::actionShortcutIsOverridden(const QString& actionName) const {
	Settings settings;
	QVariant v = settings.value("shortcuts/" + actionName);
	return !v.isNull();
}

QString Application::shortcutAction(const QKeySequence& shortcut) const {
	Settings settings;
	settings.beginGroup("shortcuts");
	QStringList keys = settings.childKeys();
	QStringList noopActions;

	for (int i = 0; i < keys.size(); i++) {
		QString key = keys[i];
		QString value = settings.value(key).toString();
		QKeySequence kv(value);
		if (kv.matches(shortcut) == QKeySequence::ExactMatch) {
			return key;
		} else if (value == "") {
			noopActions << key;
		}
	}

	ActionVector actions = this->actions();
	for (unsigned int i = 0; i < actions.size(); i++) {
		Action* a = actions[i];
		if (a->supports(shortcut)) {
			// Now also check if the shortcut has been overridden by a blank shortcut (which means
			// this action cannot be started via a shortcut)
			if (noopActions.contains(a->id())) return "";
			return a->id();
		}
	}

	settings.endGroup();

	return "";
}

QKeySequence Application::actionShortcut(const QString &actionName) const {
	ActionVector actions = this->actions();
	Action* action = NULL;
	for (unsigned int i = 0; i < actions.size(); i++) {
		Action* a = actions[i];
		if (a->id() == actionName) {
			action = a;
			break;
		}
	}

	if (!action) {
		QKeySequence output;
		return output;
	}

	Settings settings;
	QVariant v = settings.value("shortcuts/" + actionName);
	if (v.isNull()) return action->shortcut();

	QKeySequence output(v.toString());
	return output;
}

bool Application::event(QEvent *event) {
	switch (event->type()) {

		case QEvent::FileOpen: {

			QString filePath = static_cast<QFileOpenEvent*>(event)->file();
			browsingDirection_ = Forward;

			if (QFileInfo(filePath).isDir()) {
				QStringList sources = this->sources(filePath);
				if (!sources.size()) return true;
				setSource(sources[0]);
			} else {
				setSource(filePath);
			}
			return true;

		}

		default: {

			return QApplication::event(event);

		}

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
	onSourceChange();
}

void Application::closeWindowCleanup() {
	setSource("");
	mainWindow_->clearSourceAndCache();
}

void Application::execAction(const QString& actionName, const QStringList& filePaths) {
	if (actionName == "") return;

	if (actionName == "open_file") {
		Settings settings;
		QString lastDir = settings.value("lastOpenFileDirectory").toString();
		QString filePath = QFileDialog::getOpenFileName(NULL, tr("Open File"), lastDir, supportedFilesFilter());
		if (filePath != "") {
			browsingDirection_ = Forward;
			setSource(filePath);
			settings.setValue("lastOpenFileDirectory", QVariant(QFileInfo(filePath).absolutePath()));
		}
		return;
	}

	if (actionName == "close_window") {
		if (mainWindow_ && !mainWindow_->isHidden()) {
			closeWindowCleanup();
			mainWindow_->hide();
		}
		return;
	}

	if (actionName == "quit") {
		quit();
		return;
	}

	if (actionName == "batch_operation") {
		BatchDialog dialog(mainWindow());
		dialog.setModal(true);
		dialog.exec();
		return;
	}

	if (actionName == "previous") {
		previousSource();
		return;
	}

	if (actionName == "next") {
		nextSource();
		return;
	}

	if (actionName == "zoom_in") {
		int previous = mainWindow_->zoomIndex();
		mainWindow_->zoomIn();
		if (previous != mainWindow_->zoomIndex()) onZoomChange();
		return;
	}

	if (actionName == "zoom_out") {
		int previous = mainWindow_->zoomIndex();
		mainWindow_->zoomOut();
		if (previous != mainWindow_->zoomIndex()) onZoomChange();
		return;
	}

	if (actionName == "rotate") {
		mainWindow_->setRotation(mainWindow_->rotation() + 90);
		return;
	}

	if (actionName == "close_console") {
		mainWindow_->showConsole(false);
		return;
	}

	if (actionName == "toggle_console") {
		mainWindow_->toggleConsole();
		return;
	}

	if (actionName == "toggle_status_bar") {
		mainWindow_->toggleStatusBar();
		Settings settings;
		settings.setValue("showStatusBar", mainWindow_->statusBarShown());
		return;
	}

	if (actionName == "toggle_toolbar") {
		mainWindow_->toggleToolbar();
		Settings settings;
		settings.setValue("showToolbar", mainWindow_->toolbarShown());
		return;
	}

	if (actionName == "about") {
		QMessageBox::about(NULL, tr("About %1").arg(APPLICATION_TITLE), tr("%1 %2").arg(APPLICATION_TITLE).arg(version::number()));
		return;
	}

	if (actionName == "preferences") {
		showPreferencesDialog();
		return;
	}

	if (actionName == "undo") {
		undo();
		return;
	}

	pluginManager_->execAction(actionName, filePaths);
}

void Application::mainWindow_keypressed(QKeyEvent* event) {
	QKeySequence ks(event->modifiers() + event->key());
	QString actionName = shortcutAction(ks);
	execAction(actionName, QStringList() << source());
}

void Application::mainWindow_closed() {
	closeWindowCleanup();
}

void Application::mainWindow_actionTriggered() {
	Action* action = dynamic_cast<Action*>(sender());
	QString name = action->id();
	execAction(name, QStringList() << source());
}

void Application::onZoomChange() {
	mainWindow_->setStatusItem("zoom", QString("Zoom: %1%").arg(round(mainWindow_->zoom() * 100.0)));
}

void Application::onSourceChange() {
	undoVector_.clear();
	preloadTimer_->stop();

	if (fsWatcher_.files().size()) fsWatcher_.removePaths(fsWatcher_.files());
	if (source_ != "") fsWatcher_.addPath(source_);

	if (mainWindow_->isHidden()) mainWindow_->show();
	mainWindow_->resetZoom();
	//Exif exif(source_);
	// mainWindow_->setRotation(360 - exif.rotation());
	mainWindow_->setSource(source_);
	setWindowTitle(QFileInfo(source_).fileName());
	refreshStatusBar();

	preloadTimer_->start();
}

void Application::refreshStatusBar() {
	int sourceIndex = this->sourceIndex();
	QString counter = sourceIndex >= 0 ? QString("#%1/%2").arg(sourceIndex + 1).arg(sources().size()) : "#-/-";
	mainWindow_->setStatusItem("counter", counter);

	QPixmap* pixmap = mainWindow_->pixmap();
	QString sizeString = pixmap ? QString("%1x%2").arg(pixmap->width()).arg(pixmap->height()) : "";
	mainWindow_->setStatusItem("dimensions", sizeString);

	onZoomChange();
}

void Application::saveWindowGeometry() {
	if (!mainWindow_) return;

	Settings settings;
	settings.beginGroup("applicationWindow");
	settings.setValue("width", mainWindow_->size().width());
	settings.setValue("height", mainWindow_->size().height());
	settings.setValue("x", mainWindow_->x());
	settings.setValue("y", mainWindow_->y());
	settings.endGroup();
}

void Application::loadWindowGeometry() {
	Settings settings;
	QVariant v;
	int windowX = 0;
	int windowY = 0;
	int windowWidth = 800;
	int windowHeight = 600;
	settings.beginGroup("applicationWindow");
	v = settings.value("width");
	if (!v.isNull()) windowWidth = v.toInt();
	v = settings.value("height");
	if (!v.isNull()) windowHeight = v.toInt();
	v = settings.value("x");
	if (!v.isNull()) windowX = v.toInt();
	v = settings.value("y");
	if (!v.isNull()) windowY = v.toInt();
	settings.endGroup();

	mainWindow_->move(windowX, windowY);
	mainWindow_->resize(windowWidth, windowHeight);
}

void Application::onExit() {
	saveWindowGeometry();
}

QStringList Application::supportedFileExtensions() const {
	QStringList output;
	output << "jpg" << "jpeg" << "png" << "gif" << "bmp" << "tif" << "tiff";
	return output;
}

QString Application::supportedFilesFilter() const {
	QString filter;
	QStringList extensions = supportedFileExtensions();
	for (int i = 0; i < extensions.size(); i++) {
		QString e = extensions[i];
		if (filter != "") filter += " ";
		filter += "*." + e;
	}
	return tr("Supported Files (%1)").arg(filter);
}

bool Application::isSupportedFileExtension(const QString& extension) const {
	QStringList extensions = supportedFileExtensions();
	return extensions.contains(extension.toLower());
}

bool Application::isSupportedFile(const QString& filePath) const {
	return isSupportedFileExtension(QFileInfo(filePath).suffix());
}

void Application::playLoopAnimation() {
	mainWindow_->doLoopAnimation();
}

void Application::setSourceIndex(int index) {
	QStringList sources = this->sources();
	if (!sources.size()) return;

	QString source = sources[index];
	sourceIndex_ = index;

	setSource(source);
}

QString Application::nextSourcePath() const {
	QStringList sources = this->sources();
	if (!sources.size()) return "";
	int index = sourceIndex();
	index++;
	if (index >= sources.size()) index = 0;
	return sources[index];
}

QString Application::previousSourcePath() const {
	QStringList sources = this->sources();
	if (!sources.size()) return "";
	int index = sourceIndex();
	index--;
	if (index < 0) index = sources.size() - 1;
	return sources[index];
}

void Application::nextSource() {
	int index = sourceIndex();
	QStringList sources = this->sources();
	index++;
	if (index >= sources.size()) {
		index = 0;
		playLoopAnimation();
	}
	browsingDirection_ = Forward;
	setSourceIndex(index);
}

void Application::previousSource() {
	int index = sourceIndex();
	QStringList sources = this->sources();
	index--;
	if (index < 0) {
		index = sources.size() - 1;
		playLoopAnimation();
	}
	browsingDirection_ = Backward;
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

void Application::reloadSource() {
	mainWindow_->reloadSource();
	refreshStatusBar();
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
	return sources(source());
}

QStringList Application::sources(const QString& filePath) const {
	if (sources_.length()) return sources_;

	QStringList supportedFileExtensions = this->supportedFileExtensions();
	sourceIndex_ = -1;

	QFileInfo fileInfo(filePath);
	QDir dir;
	if (fileInfo.isDir()) {
		dir.setPath(filePath);
	} else {
		dir = fileInfo.dir();
	}

	sourceDir_ = dir.absolutePath();
	QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::LocaleAware);
	for (int i = 0; i < files.size(); i++) {
		if (!supportedFileExtensions.contains(files[i].suffix().toLower())) continue;
		sources_.append(files[i].absoluteFilePath());
	}

	std::sort(sources_.begin(), sources_.end(), stringutil::NaturalSortCompare());

	return sources_;
}

}
