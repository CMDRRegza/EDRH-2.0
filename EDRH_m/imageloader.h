#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QUrl>
#include <QHash>
#include <QTimer>
#include <QQmlEngine>
#include <QJsonArray>

class ImageLoader : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
public:
    explicit ImageLoader(QObject *parent = nullptr);
    
    // Load image from URL with caching
    Q_INVOKABLE void loadImage(const QString &url, const QString &systemName = "");
    Q_INVOKABLE void loadPresetImage(const QString &category);
    Q_INVOKABLE void loadPresetImagesFromDatabase(bool includeRichard = false);
    Q_INVOKABLE QString getCachedImagePath(const QString &url);
    Q_INVOKABLE QString getCachedImageFileUrl(const QString &url);
    Q_INVOKABLE bool hasPresetImage(const QString &category);
    Q_INVOKABLE QString getPresetImageUrl(const QString &category);
    Q_INVOKABLE QString getPresetImageFileUrl(const QString &category);
    
    // Clear cache
    Q_INVOKABLE void clearCache();
    
    // Preload common images
    Q_INVOKABLE void preloadCommonImages();

public slots:
    // Handle preset images from database
    void handlePresetImagesReceived(const QJsonArray &presetImages);

signals:
    void imageLoaded(const QString &url, const QString &imagePath);
    void imageLoadFailed(const QString &url, const QString &error);
    void presetImageLoaded(const QString &category, const QString &imagePath);
    void presetImagesFromDatabaseLoaded(const QJsonArray &presetImages);
    void presetImagesReady();  // Emitted when preset images are loaded and ready to use
    void cacheCleared();

private slots:
    void handleImageDownloaded();
    void handleNetworkError(QNetworkReply::NetworkError error);

private:
    QNetworkAccessManager *m_networkManager;
    QHash<QString, QString> m_imageCache;  // URL -> local file path
    QHash<QString, QString> m_presetImages; // Category -> preset URL
    QHash<QNetworkReply*, QString> m_pendingDownloads; // Reply -> URL
    QString m_cacheDir;
    
    void initializeCache();
    void initializePresetImages();
    QString generateCacheFileName(const QString &url);
    void saveImageToCache(const QString &url, const QByteArray &data);
    bool isImageCached(const QString &url);
    QString normalizeImgurUrl(const QString &url);
};

#endif // IMAGELOADER_H 