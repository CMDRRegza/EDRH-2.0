#ifndef SUPABASECLIENT_H
#define SUPABASECLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>

class SupabaseClient : public QObject
{
    Q_OBJECT

public:
    struct SystemData {
        QString name;
        QString category;
        double x, y, z;
        double distance;
    };

    explicit SupabaseClient(QObject *parent = nullptr);
    
    // Configuration
    Q_INVOKABLE void configure(const QString &url, const QString &key);
    Q_INVOKABLE bool isConfigured() const { return !m_supabaseUrl.isEmpty() && !m_supabaseKey.isEmpty(); }
    Q_INVOKABLE void resetAuthFailures(); // Reset authentication failure state
    
    // Commander context for RLS policies
    Q_INVOKABLE void setCommanderContext(const QString &commanderName);
    
    // Database operations
    Q_INVOKABLE void getSystems();
    Q_INVOKABLE void getTakenSystems();
    Q_INVOKABLE void getCurrentCommanderSystems(); // Get only current commander's systems
    Q_INVOKABLE void getPOISystems(const QString &commanderName = "");
    Q_INVOKABLE void getCategories();
    Q_INVOKABLE void getRichardCategories();
    Q_INVOKABLE void getPresetImages(bool includeRichard = false);
    Q_INVOKABLE void getSystemsNear(double x, double y, double z, int limit = 50);
    Q_INVOKABLE void getSystemInformation(const QString &systemName, const QString &category = "");
    Q_INVOKABLE void getSystemInformationFromDB(const QString &systemName);
    Q_INVOKABLE void getSystemInformationFromCategory(const QString &systemName, const QString &category);
    Q_INVOKABLE void claimSystem(const QString &systemName, const QString &commander);
    Q_INVOKABLE void unclaimSystem(const QString &systemName);
    Q_INVOKABLE void markSystemVisited(const QString &systemName, const QString &commander);
    Q_INVOKABLE void updateSystemStatus(const QString &systemName, bool visited, bool done);
    Q_INVOKABLE void updateSystemVisited(const QString &systemName, bool visited);
    Q_INVOKABLE void updateSystemDone(const QString &systemName, bool done);
    Q_INVOKABLE void markSystemAsEdited(const QString &systemName);
    Q_INVOKABLE void checkAdminStatus(const QString &commander);
    Q_INVOKABLE void testAdminAccess(const QString &serviceKey);
    Q_INVOKABLE void getSystemDetails(const QString &systemName, const QString &category);
    Q_INVOKABLE void getSystemDetailsRobust(const QString &systemName);
    Q_INVOKABLE void triggerWebhook(const QString &eventType, const QVariantMap &data);
    Q_INVOKABLE void getWebhookConfig();
    Q_INVOKABLE void markSystemAsPOI(const QString &systemName, const QString &poiType, const QString &commander);
    Q_INVOKABLE void removePOIStatus(const QString &systemName, const QString &commander);
    Q_INVOKABLE void updateSystemPOIStatus(const QString &systemName, const QString &poiType, const QString &discoverer, const QString &submitter);
    Q_INVOKABLE void saveSystemDescription(const QString &systemName, const QString &description);
    Q_INVOKABLE void saveSystemInformation(const QString &systemName, const QVariantMap &information);
    Q_INVOKABLE void updateSystemImages(const QString &systemName, const QString &primaryImage, 
                                       const QString &primaryTitle, const QString &additionalImages, 
                                       const QString &additionalTitles);
    Q_INVOKABLE void loadSystemImagesForSystems(const QStringList &systemNames);
    Q_INVOKABLE void getTakenSystemForCommander(const QString &systemName, const QString &commanderName);
    Q_INVOKABLE void getAllCommanderLocations();
    Q_INVOKABLE void updateCommanderLocation(const QString &commanderName, double x, double y, double z, const QString &systemName);
    Q_INVOKABLE void uploadImageToImgbb(const QString &filePath, const QString &systemName);
    Q_INVOKABLE void testImgbbApiKey();
    Q_INVOKABLE void getPresetImageForCategory(const QString &category);
    Q_INVOKABLE void usePresetImageForSystem(const QString &systemName, const QString &category);
    Q_INVOKABLE QString getSystemImageUrl(const QString &systemName, const QString &category);
    Q_INVOKABLE void saveImageToDatabase(const QString &systemName, const QString &imageUrl);
    Q_INVOKABLE bool checkSystemVisitedInJournal(const QString &systemName, const QString &commanderName);

    // Smart database sync functions
    Q_INVOKABLE void initializeDatabaseSync();
    Q_INVOKABLE void checkForDatabaseUpdates();
    Q_INVOKABLE bool isFirstRun() const;
    Q_INVOKABLE void performIncrementalSync();
    
private:
    void getSystemDetailsWithCapitalizationHandling(const QString &systemName, const QString &category);
    void getSystemDetailsWithFieldName(const QString &systemName, const QString &category, const QString &fieldName);
    
    // Database sync helper functions
    void loadSyncState();
    void saveSyncState();
    void performFullSync();
    void finalizeDatabaseSync(bool success, int changesDetected);
    
public:
    // Format system information from category table data into readable text
    QString formatCategoryTableData(const QJsonObject &categoryData, const QString &category) const;
    
    // Utility methods
    Q_INVOKABLE double calculateDistance(double x1, double y1, double z1, double x2, double y2, double z2);
    Q_INVOKABLE bool isInAuthFailureCooldown() const;

public slots:
    // Authentication methods - automatic security system like v1.4incomplete.py  
    void performSecurityCheck(const QString &commanderName);
    void handleNewCommander(const QString &commanderName, const QStringList &allCommanders);
    void detectCommanderRenames(const QString &journalPath);
    void logLoginEvent(const QString &commanderName, bool isAdmin, const QString &eventType, const QString &details = "");
    bool isCommanderBlocked(const QString &commanderName);
    void addCommanderToSecurity(const QString &commanderName, bool blocked, const QString &notes = "", const QString &journalPath = "");

signals:
    void systemsReceived(const QJsonArray &systems);
    void takenSystemsReceived(const QJsonArray &taken);
    void poisReceived(const QJsonArray &pois);
    void categoriesReceived(const QJsonArray &categories);
    void presetImagesReceived(const QJsonArray &presetImages);
    void nearestSystemsReceived(const QJsonArray &systems);
    void systemInformationReceived(const QString &systemName, const QJsonObject &systemInfo);
    void systemClaimed(const QString &systemName, bool success);
    void systemUnclaimed(const QString &systemName, bool success);
    void systemMarkedVisited(const QString &systemName, bool success);
    void systemStatusUpdated(const QString &systemName, bool success);
    void adminStatusReceived(bool isAdmin);
    void adminAccessTestComplete(bool hasAccess);
    void presetImageFound(const QString &systemName, const QString &imageUrl, const QString &category);
    void systemImageSet(const QString &systemName, const QString &imageUrl, bool success);
    void bulkSystemImagesLoaded(const QJsonObject &systemImages);
    void allCommanderLocationsReceived(const QJsonArray &locations);
    void commanderLocationUpdated(const QString &commanderName, bool success);
    void webhookConfigReceived(const QString &webhookUrl);
    void webhookTriggered(bool success, const QString &eventType);
    void poiDataForMergeReceived(const QJsonArray &poiData);

    void networkError(const QString &error);
    void requestCompleted(const QString &operation, bool success, const QString &message);
    
    // Database sync signals
    void databaseSyncStatusChanged(const QString &status);
    void databaseSyncProgress(int current, int total, const QString &operation);
    void databaseSyncComplete(bool isFirstRun, int changesDetected);

    // Authentication signals
    void securityCheckComplete(const QString &commanderName, bool isBlocked, const QString &reason = "");
    void authenticationRequired();
    void authenticationComplete(bool success, const QString &message = "");
    void confirmationRequired(const QString &title, const QString &message, const QString &commanderName);

private slots:
    void handleNetworkReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_supabaseUrl;
    QString m_supabaseKey;
    QString m_currentCommander;
    QStringList m_detectedCommanders;  // Store current commander for RLS policies
    QStringList m_pendingSystemsCategories; // Store systems categories while loading Richard categories
    // Cache of the most recent nearest systems payload for quick POI re-merge
    QJsonArray m_cachedNearestSystems;
    
    // Error handling and rate limiting
    qint64 m_lastAuthFailureTime;
    int m_consecutiveAuthFailures;
    static const int MAX_AUTH_FAILURES_TO_LOG = 3;
    static const qint64 AUTH_FAILURE_COOLDOWN_MS = 30000; // 30 seconds
    
    // Database sync state
    QString m_syncStateFile;
    QJsonObject m_syncState;
    bool m_syncInProgress;
    
    // Webhook configuration (loaded securely from database)
    QString m_webhookUrl;
    bool m_webhookConfigLoaded;
    
    // Preset image system
    QMap<QString, QString> m_categoryImageCache; // category -> image URL
    QMap<QString, QString> m_systemImageOverrides; // system -> custom image URL
    QMap<QString, QString> m_pendingPresetRequests; // category -> systemName
    
    // POI data for merging
    QJsonArray m_pendingPOIData;
    
    // Helper methods
    QNetworkRequest createRequest(const QString &endpoint);
    void makeRequest(const QString &method, const QString &endpoint, const QJsonObject &data = QJsonObject());
    QJsonObject parseReply(QNetworkReply *reply, bool &success);
    bool shouldSkipRequestDueToAuthFailure();
    void fetchAndMergePOIData(QJsonArray &systemsArray);
    void mergePOIDataIntoSystems(QJsonArray &systemsArray);
    void processSystemsReply(const QJsonArray &data);
    // ImgBB helper
    void startImgbbUpload(const QString &filePath, const QString &systemName, int attempt = 1);
    static const int IMGBB_MAX_ATTEMPTS = 2;

};

// Declare this type for Qt's meta-object system
Q_DECLARE_METATYPE(SupabaseClient*)

#endif // SUPABASECLIENT_H 