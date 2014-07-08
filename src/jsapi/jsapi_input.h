#ifndef MV_JSAPI_INPUT_H
#define MV_JSAPI_INPUT_H

#include <QObject>
#include <QScriptEngine>
#include <QScriptValue>
#include <QRect>

namespace jsapi {

class Input : public QObject {

	Q_OBJECT
	Q_PROPERTY(QString filePath READ filePath)
	Q_PROPERTY(QString escapedFilePath READ escapedFilePath)
	Q_PROPERTY(QStringList filePaths READ filePaths)
	Q_PROPERTY(QString escapedFilePaths READ escapedFilePaths)
	Q_PROPERTY(QScriptValue selectionRect READ selectionRect)

public:

	Input(QScriptEngine* engine, const QStringList& filePaths, const QRect& selectionRect);

public slots:

	QString filePath() const;
	QString escapedFilePath() const;
	QStringList filePaths() const;
	QString escapedFilePaths() const;
	QScriptValue selectionRect() const;

private:

	QString escapePath(const QString& path) const;

	QScriptEngine* engine_;
	QStringList filePaths_;
	QRect selectionRect_;

};

}

#endif