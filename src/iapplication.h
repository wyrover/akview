#ifndef IAPPLICATION_H
#define IAPPLICATION_H

class IApplication {

public slots:

	virtual QUrl imageSource() const = 0;
	virtual void setImageSource(const QString& source) = 0;
	virtual void setImageSource(const QUrl& source) = 0;
	virtual QStringList supportedFileExtensions() = 0;

};

#endif // IAPPLICATION_H
