#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QJsonObject>
#include <QListWidgetItem>

#include "action.h"
#include "mvplugininterface.h"
#include "pluginevents.h"
#include "simpletypes.h"

namespace mv {

class ActionListWidgetItem: public QListWidgetItem {

public:

	ActionListWidgetItem(Action* action);
	void setShortcut(const QKeySequence& ks);
	void setShortcutIsOverridden(bool v);
	bool shortcutIsOverridden() const;
	QKeySequence shortcut() const;
	Action* action() const;
	void updateDisplay();

private:

	Action* action_;
	QKeySequence shortcut_;
	bool shortcutIsOverridden_;

};

class Plugin {

public:

	Plugin(IApplication* application, const QString &pluginFilePath);
	bool loadMetadata();
	bool loadInterface();
	MvPluginInterface* interface() const;
	QString errorMessage() const;
	QString description() const;
	QString version() const;
	QString compatibilityMinVersion() const;
	QString compatibilityMaxVersion() const;
	ActionVector actions() const;
	bool supports(const KeypressedEvent& event) const;
	bool interfaceLoaded() const;
	Action *findAction(const KeypressedEvent& event) const;
	Action *findAction(const QString& name) const;

private:

	QString metadataFilePath(const QString &pluginFilePath) const;
	QString errorMessage_;
	MvPluginInterface* interface_;
	IApplication* application_;
	QString pluginFilePath_;
	QJsonObject metadata_;
	ActionVector actions_;

};

typedef std::vector<Plugin*> PluginVector;

class PluginManager {

public:

	PluginManager(IApplication* application);
	bool loadPlugin(const QString& filePath);
	void loadPlugins(const QString& folderPath);
	PluginVector plugins() const;
	void onKeypressed(const KeypressedEvent& event);
	void onAction(const QString& actionName);

private:

	PluginVector plugins_;
	IApplication* application_;

};

}

#endif // PLUGINMANAGER_H
