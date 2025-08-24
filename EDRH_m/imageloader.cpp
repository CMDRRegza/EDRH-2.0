#include "imageloader.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QBuffer>
#include <QDebug>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

ImageLoader::ImageLoader(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    initializeCache();
    initializePresetImages();
}

void ImageLoader::initializeCache()
{
    // Create cache directory
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_cacheDir = QDir(appDataDir).filePath("image_cache");
    
    QDir dir;
    if (!dir.exists(m_cacheDir)) {
        dir.mkpath(m_cacheDir);
        qDebug() << "Created image cache directory:" << m_cacheDir;
    }
}

void ImageLoader::initializePresetImages()
{
    // Initialize preset images based on categories (matching Python version)
    m_presetImages.clear();
    
    // Space/cosmic backgrounds for different categories
    m_presetImages["Wolf-Rayet Star"] = "https://images.unsplash.com/photo-1446776653964-20c1d3a81b06?w=800&h=600&fit=crop";
    m_presetImages["High Metal Content World"] = "https://images.unsplash.com/photo-1614730321146-b6fa6a46bcb4?w=800&h=600&fit=crop";
    m_presetImages["Scenic"] = "https://images.unsplash.com/photo-1502134249126-9f3755a50d78?w=800&h=600&fit=crop";
    m_presetImages["Binary Planets"] = "https://images.unsplash.com/photo-1512138664757-360e0aad5132?w=800&h=600&fit=crop";
    m_presetImages["Potential POI"] = "https://images.unsplash.com/photo-1547036967-23d11aacaee0?w=800&h=600&fit=crop";
    m_presetImages["POI"] = "https://images.unsplash.com/photo-1543722530-d2c3201371e7?w=800&h=600&fit=crop";
    m_presetImages["Extreme close proximity to ring"] = "https://images.unsplash.com/photo-1573588028698-f4759befb09a?w=800&h=600&fit=crop";
    m_presetImages["Very cool"] = "https://images.unsplash.com/photo-1446776877081-d282a0f896e2?w=800&h=600&fit=crop";
    m_presetImages["Great system."] = "https://images.unsplash.com/photo-1542621334-a254cf47733d?w=800&h=600&fit=crop";
    m_presetImages["Pretty cool system"] = "https://images.unsplash.com/photo-1581833971358-2c8b550f87b3?w=800&h=600&fit=crop";
    
    // Default fallback image
    m_presetImages["default"] = "https://images.unsplash.com/photo-1446776877081-d282a0f896e2?w=800&h=600&fit=crop";
    
    qDebug() << "Initialized" << m_presetImages.size() << "preset images";
}

void ImageLoader::loadImage(const QString &url, const QString &systemName)
{
    if (url.isEmpty()) {
        emit imageLoadFailed(url, "Empty URL provided");
        return;
    }
    
    // Normalize Imgur URLs to direct image links
    QString normalizedUrl = normalizeImgurUrl(url);
    
    // Check if image is already cached
    if (isImageCached(normalizedUrl)) {
        QString cachedPath = getCachedImagePath(normalizedUrl);
        qDebug() << "Image found in cache:" << cachedPath;
        emit imageLoaded(normalizedUrl, cachedPath);
        return;
    }
    
    // Download the image
    QNetworkRequest request{QUrl(normalizedUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "EDRH/1.4.0-qt");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    // Force HTTP/1.1 for hosts that misbehave with HTTP/2 (e.g., some CDN edges)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    // Follow safe redirects automatically (ImgBB/Imgur sometimes redirect to CDN URLs)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    QNetworkReply *reply = m_networkManager->get(request);
    // Implement manual timeout (Qt 6.9 doesn't have QNetworkReply::setTransferTimeout)
    QTimer *timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(15000);
    QObject::connect(timeoutTimer, &QTimer::timeout, reply, [this, reply]() {
        QString pendingUrl = m_pendingDownloads.value(reply);
        qWarning() << "Image download timed out:" << pendingUrl;
        reply->abort();
    });
    timeoutTimer->start();
    m_pendingDownloads[reply] = normalizedUrl;
    
    connect(reply, &QNetworkReply::finished, this, &ImageLoader::handleImageDownloaded);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &ImageLoader::handleNetworkError);
    
    qDebug() << "Started downloading image:" << normalizedUrl;
}

void ImageLoader::loadPresetImage(const QString &category)
{
    QString url = getPresetImageUrl(category);
    if (!url.isEmpty()) {
        loadImage(url, category);
    } else {
        emit imageLoadFailed(category, "No preset image found for category");
    }
}

QString ImageLoader::getCachedImagePath(const QString &url)
{
    if (m_imageCache.contains(url)) {
        return m_imageCache[url];
    }
    
    QString fileName = generateCacheFileName(url);
    QString filePath = QDir(m_cacheDir).filePath(fileName);
    
    if (QFileInfo::exists(filePath)) {
        m_imageCache[url] = filePath;
        return filePath;
    }
    
    return QString();
}

QString ImageLoader::getCachedImageFileUrl(const QString &url)
{
    QString cachedPath = getCachedImagePath(url);
    if (cachedPath.isEmpty()) {
        return QString();
    }
    
    // CRITICAL FIX: Properly encode the URL to handle spaces and special characters
    QUrl fileUrl = QUrl::fromLocalFile(cachedPath);
    
    // Use toEncoded() and convert back to QString - this ensures proper URL encoding
    QByteArray encodedUrl = fileUrl.toEncoded();
    QString fileUrlString = QString::fromUtf8(encodedUrl);
    
    // Debug output to check URL conversion
    qDebug() << "ImageLoader: Converting cached path" << cachedPath;
    qDebug() << "ImageLoader: Generated encoded URL:" << fileUrlString;
    
    // Additional validation - ensure it starts with file://
    if (!fileUrlString.startsWith("file://")) {
        qWarning() << "ImageLoader: URL doesn't start with file://, manual construction needed";
        
        // Manual construction with proper encoding
        QString normalizedPath = cachedPath;
        normalizedPath.replace('\\', '/');
        
        // URL encode the path components that have spaces
        QUrl manualUrl;
        manualUrl.setScheme("file");
        manualUrl.setPath(normalizedPath);
        fileUrlString = QString::fromUtf8(manualUrl.toEncoded());
        
        qDebug() << "ImageLoader: Manually constructed URL:" << fileUrlString;
    }
    
    return fileUrlString;
}

bool ImageLoader::hasPresetImage(const QString &category)
{
    return m_presetImages.contains(category);
}

QString ImageLoader::getPresetImageUrl(const QString &category)
{
    if (m_presetImages.contains(category)) {
        return m_presetImages[category];
    }
    
    // Return default image if category not found
    return m_presetImages.value("default", "");
}

QString ImageLoader::getPresetImageFileUrl(const QString &category)
{
    QString url = getPresetImageUrl(category);
    if (url.isEmpty()) {
        return QString();
    }
    
    return getCachedImageFileUrl(url);
}

void ImageLoader::clearCache()
{
    QDir cacheDir(m_cacheDir);
    cacheDir.removeRecursively();
    m_imageCache.clear();
    initializeCache();
    emit cacheCleared();
    qDebug() << "Image cache cleared";
}

void ImageLoader::preloadCommonImages()
{
    // Preload the most common preset images
    QStringList commonCategories = {
        "Wolf-Rayet Star", "High Metal Content World", "Scenic", 
        "Binary Planets", "POI", "default"
    };
    
    for (const QString &category : commonCategories) {
        if (hasPresetImage(category)) {
            loadPresetImage(category);
        }
    }
    
    qDebug() << "Started preloading" << commonCategories.size() << "common images";
}

void ImageLoader::loadPresetImagesFromDatabase(bool includeRichard)
{
    // This would typically connect to a SupabaseClient instance
    // For now, we'll emit a signal that can be connected to fetch from database
    qDebug() << "Requesting preset images from database, includeRichard:" << includeRichard;
    
    // In a real implementation, this would be connected to SupabaseClient
    // emit requestPresetImages(includeRichard);
}

void ImageLoader::handlePresetImagesReceived(const QJsonArray &presetImages)
{
    qDebug() << "Processing" << presetImages.size() << "preset images from database";
    
    // Clear existing preset mapping
    m_presetImages.clear();
    
    // Process each preset image from database
    for (const QJsonValue &value : presetImages) {
        QJsonObject preset = value.toObject();
        QString category = preset["category"].toString();
        QString imageUrl = preset["image"].toString();
        bool isRichard = preset["richard"].toBool();
        
        if (!category.isEmpty() && !imageUrl.isEmpty()) {
            m_presetImages[category] = imageUrl;
            qDebug() << "Mapped category" << category << "to image URL" << imageUrl << "(Richard:" << isRichard << ")";
            
            // CRITICAL FIX: Don't preload all images simultaneously during startup
            // This prevents the mass URL corruption bug. Images will be loaded on-demand.
            // loadImage(imageUrl, category);
        }
    }
    
    qDebug() << "Loaded" << m_presetImages.size() << "preset image mappings";
    emit presetImagesFromDatabaseLoaded(presetImages);
    emit presetImagesReady();  // Notify that preset images are ready to use
}

void ImageLoader::handleImageDownloaded()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString url = m_pendingDownloads.value(reply);
    m_pendingDownloads.remove(reply);
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        
        if (!data.isEmpty()) {
            saveImageToCache(url, data);
            QString cachedPath = getCachedImagePath(url);
            emit imageLoaded(url, cachedPath);
            qDebug() << "Image downloaded and cached:" << url;
        } else {
            emit imageLoadFailed(url, "Empty image data received");
        }
    } else {
        // Retry once with a sanitized URL (strip query params) if extension missing
        QString err = reply->errorString();
        qDebug() << "Failed to download image:" << url << err;
        QUrl qurl(url);
        bool hasExt = QFileInfo(qurl.path()).suffix().length() > 0;
        if (!hasExt) {
            QUrl retry = qurl;
            retry.setQuery(QString());
            QString retryUrl = retry.toString();
            if (retryUrl != url) {
                qDebug() << "Retrying image without query params:" << retryUrl;
                loadImage(retryUrl, QString());
            }
        }
        emit imageLoadFailed(url, err);
    }
    
    reply->deleteLater();
}

void ImageLoader::handleNetworkError(QNetworkReply::NetworkError error)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString url = m_pendingDownloads.value(reply);
    qDebug() << "Network error downloading image:" << url << error;

    // Retry once on transient errors
    if (error == QNetworkReply::TimeoutError ||
        error == QNetworkReply::TemporaryNetworkFailureError ||
        error == QNetworkReply::ProxyTimeoutError ||
        error == QNetworkReply::UnknownNetworkError) {
        if (!url.isEmpty()) {
            qDebug() << "Retrying image download after transient error:" << url;
            loadImage(url, QString());
            reply->deleteLater();
            return;
        }
    }

    emit imageLoadFailed(url, QString("Network error: %1").arg(error));
}

bool ImageLoader::isImageCached(const QString &url)
{
    QString fileName = generateCacheFileName(url);
    QString filePath = QDir(m_cacheDir).filePath(fileName);
    return QFileInfo::exists(filePath);
}

QString ImageLoader::generateCacheFileName(const QString &url)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(url.toUtf8());
    QString hashStr = hash.result().toHex();
    
    // Extract file extension from URL
    QUrl qurl(url);
    QString path = qurl.path();
    QString extension = QFileInfo(path).suffix();
    if (extension.isEmpty()) {
        extension = "jpg"; // Default extension
    }
    
    return QString("%1.%2").arg(hashStr, extension);
}

void ImageLoader::saveImageToCache(const QString &url, const QByteArray &data)
{
    QString fileName = generateCacheFileName(url);
    QString filePath = QDir(m_cacheDir).filePath(fileName);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
        m_imageCache[url] = filePath;
        qDebug() << "Image saved to cache:" << filePath;
    } else {
        qDebug() << "Failed to save image to cache:" << filePath;
    }
}

QString ImageLoader::normalizeImgurUrl(const QString &url)
{
    // Handle Imgur URLs to ensure we get direct image links
    if (url.contains("imgur.com")) {
        // If it's already a direct image URL (i.imgur.com), keep it as is
        if (url.contains("i.imgur.com") && (url.endsWith(".jpg") || url.endsWith(".jpeg") || 
            url.endsWith(".png") || url.endsWith(".gif") || url.endsWith(".webp"))) {
            return url;
        }
        
        // Convert imgur.com/xxxxx to i.imgur.com/xxxxx.jpg
        if (url.contains("imgur.com/") && !url.contains("i.imgur.com")) {
            QString imageId = url;
            // Extract the image ID (everything after the last slash)
            int lastSlash = imageId.lastIndexOf('/');
            if (lastSlash != -1) {
                imageId = imageId.mid(lastSlash + 1);
                
                // Remove any query parameters or file extensions
                int queryStart = imageId.indexOf('?');
                if (queryStart != -1) {
                    imageId = imageId.left(queryStart);
                }
                int dotPos = imageId.indexOf('.');
                if (dotPos != -1) {
                    imageId = imageId.left(dotPos);
                }
                
                // Return direct image URL
                return QString("https://i.imgur.com/%1.jpg").arg(imageId);
            }
        }
    }
    
    // For non-Imgur URLs or if parsing fails, return original URL
    return url;
} 