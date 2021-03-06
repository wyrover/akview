#include "constants.h"
#include "paths.h"

namespace mv {

Paths::Paths() {

}

QString Paths::applicationFolder() const {
	return qApp->applicationDirPath();
}

QString Paths::pluginFolder() const {
	QString appFilePath = qApp->applicationFilePath();
	QDir dir = QFileInfo(appFilePath).dir();

#ifdef Q_OS_LINUX
	return QDir::homePath() + "/.config/akview/plugins";
#endif

#ifdef QT_DEBUG
	return QDir::homePath() + "/.config/akview/plugins";
#endif // QT_DEBUG

#ifdef MV_DEBUG
	if (appFilePath.indexOf(".app/Contents/MacOS/") >= 0) {
		dir.cdUp();
		dir.cdUp();
		dir.cdUp();
	}

	dir.cdUp();
	dir.cd("plugins/release");
#else
	if (appFilePath.indexOf(".app/Contents/MacOS/") >= 0) {
		dir.cdUp();
		dir.cd("PlugIns/multiviewer");
	}
#endif // MV_DEBUG

	return dir.absolutePath();
}

}
