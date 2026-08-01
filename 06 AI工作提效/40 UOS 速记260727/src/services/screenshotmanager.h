#ifndef SCREENSHOTMANAGER_H
#define SCREENSHOTMANAGER_H

#include <QObject>
#include <QString>
#include <QPixmap>

class ScreenshotManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ScreenshotManager)

public:
    explicit ScreenshotManager(QObject *parent = nullptr);

    void captureScreen();
    void captureRegion();

    static QString defaultScreenshotDir();

signals:
    void screenshotTaken(const QString &filePath);
    void screenshotFailed(const QString &errorMessage);

private:
    void savePixmap(const QPixmap &pixmap);
    QString generateFileName() const;
};

#endif // SCREENSHOTMANAGER_H