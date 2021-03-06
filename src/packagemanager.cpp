#include "packagemanager.h"

#include "application.h"
#include "processutil.h"

namespace mv {

Manager::Manager(int id, const QString& name, const QString& url, const QString& command, const QStringList& installCommand) {
	id_ = id;
	name_ = name;
	url_ = url;
	command_ = command;
	installCommand_ = installCommand;
}

QStringList Manager::installCommand() const {
	return installCommand_;
}

QString Manager::name() const {
	return name_;
}

QString Manager::url() const {
	return url_;
}

int Manager::id() const {
	return id_;
}

QString Manager::command() const {
	return command_;
}


PackageManager::PackageManager() {
	progressBarDialog_ = NULL;
	installProcess_ = NULL;
	installProcessEnv_ = NULL;
	managers_.push_back(new Manager(1, "Homebrew", "http://brew.sh", "brew", QStringList() << "brew" << "install" << "__PACKAGE__"));
	// TODO: check that gksudo is installed
	managers_.push_back(new Manager(2, "APT", "https://wiki.debian.org/Apt", "apt-get", QStringList() << "gksudo" << "--" << "apt-get" << "-q" << "-y" << "install" << "__PACKAGE__"));
	selectedManagerId_ = 0;
}

PackageManager::~PackageManager() {

}

void PackageManager::progressBarDialog_rejected() {
	delete progressBarDialog_;
	progressBarDialog_ = NULL;

	disconnect(installProcess_, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(process_finished(int, QProcess::ExitStatus)));
	installProcess_->terminate();
	installProcess_->waitForFinished(2000);
	installProcess_->kill();

	delete installProcess_;
	installProcess_ = NULL;
	delete installProcessEnv_;
	installProcessEnv_ = NULL;
}

bool PackageManager::installationInProgress() const {
	return progressBarDialog_ != NULL;
}

Manager* PackageManager::managerById(int id) const {
	for (unsigned int i = 0; i < managers_.size(); i++) {
		Manager* m = managers_[i];
		if (m->id() == id) return m;
	}
	return NULL;
}

Manager* PackageManager::selectedManager() const {
	if (selectedManagerId_ <= 0) {
		for (unsigned int i = 0; i < managers_.size(); i++) {
			if (managerIsInstalled(managers_[i]->id())) {
				selectedManagerId_ = managers_[i]->id();
				qDebug() << "Auto selected package manager:" << managers_[i]->name();
				break;
			}
		}

		if (selectedManagerId_ <= 0) {
			qWarning() << "Could not auto-select package manager.";
			return NULL;
		}
	}

	return managerById(selectedManagerId_);
}

bool PackageManager::managerIsInstalled(int managerId) const {
	Manager* manager = managerById(managerId);
	QString command = manager->command();

	if (installedCommands_.contains(command)) return true;
	bool ok = processutil::commandIsAvailable(command);
	if (ok) installedCommands_.push_back(command);
	return ok;
}

bool PackageManager::commandIsInstalled(const QString& command) {
	if (installedCommands_.contains(command)) return true;
	bool ok = processutil::commandIsAvailable(command);
	if (ok) installedCommands_.push_back(command);
	return ok;
}

void PackageManager::process_readyReadStandardError() {
	QProcess* process = dynamic_cast<QProcess*>(sender());
	qDebug() << qPrintable(QString(process->readAllStandardError()).trimmed());
}

void PackageManager::process_readyReadStandardOutput() {
	QProcess* process = dynamic_cast<QProcess*>(sender());
	qDebug() << qPrintable(QString(process->readAllStandardOutput()).trimmed());
}

void PackageManager::process_finished(int, QProcess::ExitStatus) {
	delete progressBarDialog_;
	progressBarDialog_ = NULL;
	delete installProcess_;
	installProcess_ = NULL;
	delete installProcessEnv_;
	installProcessEnv_ = NULL;

	emit installationDone();
}

void PackageManager::install(const QStringList& packages) {
	if (installationInProgress()) {
		qWarning() << "An installation is already in progress.";
		return;
	}

	Manager* manager = selectedManager();
	if (!manager) return;

	QStringList command = manager->installCommand();
	QStringList temp;
	for (int i = 0; i < command.size(); i++) {
		if (command[i] == "__PACKAGE__") {
			for (int j = 0; j < packages.size(); j++) {
				temp.push_back(packages[j]);
			}
		} else {
			temp.push_back(command[i]);
		}
	}
	command = temp;

	qDebug() << qPrintable("$ " + command.join(" "));

	QProcessEnvironment sysEnv = QProcessEnvironment::systemEnvironment();
	installProcessEnv_ = new QProcessEnvironment(sysEnv);
	installProcessEnv_->insert("DEBIAN_FRONTEND", "noninteractive"); // NOT working

	installProcess_ = new QProcess();
	installProcess_->setProcessEnvironment(*installProcessEnv_);
	connect(installProcess_, SIGNAL(readyReadStandardError()), this, SLOT(process_readyReadStandardError()));
	connect(installProcess_, SIGNAL(readyReadStandardOutput()), this, SLOT(process_readyReadStandardOutput()));
	connect(installProcess_, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(process_finished(int, QProcess::ExitStatus)));
	installProcess_->start(command[0], command.mid(1));

	if (!progressBarDialog_) {
		progressBarDialog_ = new ProgressBarDialog(Application::instance()->mainWindow());
		progressBarDialog_->setModal(true);
	}

	connect(progressBarDialog_, SIGNAL(rejected()), this, SLOT(progressBarDialog_rejected()));
	progressBarDialog_->show();
}

void PackageManager::install(const QString& package) {
	install(QStringList() << package);
}

}