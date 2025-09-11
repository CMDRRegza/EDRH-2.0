#ifndef EDRHCONTROLLER_H
#define EDRHCONTROLLER_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QJsonArray>
#include <QNetworkAccessManager>

// Forward declarations
class JournalMonitor;
class ConfigManager;
class ClaimManager;

// Include complete definition for use in Q_PROPERTY
#include "supabaseclient.h"

class EDRHController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    // Properties exposed to QML
    Q_PROPERTY(QString commanderName READ commanderName WRITE setCommanderName NOTIFY commanderNameChanged)
    Q_PROPERTY(QString currentSystem READ currentSystem WRITE setCurrentSystem NOTIFY currentSystemChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(bool isAdmin READ isAdmin NOTIFY isAdminChanged)
    Q_PROPERTY(QVariantList nearestSystems READ nearestSystems NOTIFY nearestSystemsChanged)
    Q_PROPERTY(QVariantList unclaimedSystems READ unclaimedSystems NOTIFY unclaimedSystemsChanged)
    Q_PROPERTY(QString selectedCategory READ selectedCategory WRITE setSelectedCategory NOTIFY selectedCategoryChanged)
    Q_PROPERTY(QVariantList availableCategories READ availableCategories NOTIFY availableCategoriesChanged)
    Q_PROPERTY(int jumpCount READ jumpCount NOTIFY jumpCountChanged)
    Q_PROPERTY(QString sessionTime READ sessionTime NOTIFY sessionTimeChanged)
    Q_PROPERTY(bool mapWindowActive READ mapWindowActive NOTIFY mapWindowActiveChanged)

    Q_PROPERTY(QString nearestDistanceText READ nearestDistanceText NOTIFY nearestDistanceTextChanged)
    Q_PROPERTY(QString nearestCategoryText READ nearestCategoryText NOTIFY nearestCategoryTextChanged)
    Q_PROPERTY(int unclaimedIndex READ unclaimedIndex NOTIFY unclaimedIndexChanged)
    Q_PROPERTY(int unclaimedTotal READ unclaimedTotal NOTIFY unclaimedTotalChanged)
    Q_PROPERTY(QString currentUnclaimedSystemName READ currentUnclaimedSystemName NOTIFY currentUnclaimedSystemNameChanged)

    Q_PROPERTY(bool suppressMainAppNotifications READ suppressMainAppNotifications WRITE setSuppressMainAppNotifications NOTIFY suppressMainAppNotificationsChanged)
    Q_PROPERTY(SupabaseClient* supabaseClient READ supabaseClient CONSTANT)
    Q_PROPERTY(ClaimManager* claimManager READ claimManager CONSTANT)
    
    // Galaxy Map Properties
    Q_PROPERTY(QVariantList galaxyMapSystems READ galaxyMapSystems NOTIFY galaxyMapSystemsChanged)
    Q_PROPERTY(QVariantMap commanderPosition READ commanderPosition NOTIFY commanderPositionChanged)
    Q_PROPERTY(int visibleSystemsCount READ visibleSystemsCount NOTIFY visibleSystemsCountChanged)
    Q_PROPERTY(bool galaxyMapLoading READ galaxyMapLoading NOTIFY galaxyMapLoadingChanged)
    Q_PROPERTY(QVariantList allCommanderLocations READ allCommanderLocations NOTIFY allCommanderLocationsChanged)
    Q_PROPERTY(double commanderX READ commanderX NOTIFY commanderPositionChanged)
    Q_PROPERTY(double commanderZ READ commanderZ NOTIFY commanderPositionChanged)
    Q_PROPERTY(QStringList allDetectedCommanders READ getAllDetectedCommanders NOTIFY allDetectedCommandersChanged)
    
public:
    explicit EDRHController(QObject *parent = nullptr);
    
    // Set the Supabase client and journal monitor references
    void setSupabaseClient(SupabaseClient *client);
    void setConfigManager(ConfigManager *configManager);
    void setJournalMonitor(JournalMonitor *monitor);
    void setClaimManager(ClaimManager *claimManager);
    
    // Property getters
    QString commanderName() const { return m_commanderName; }
    QString currentSystem() const { return m_currentSystem; }
    QString appVersion() const { return "0.9.5"; }
    bool isAdmin() const;
    QVariantList nearestSystems() const { return m_nearestSystems; }
    QVariantList unclaimedSystems() const { return m_unclaimedSystems; }
    QString selectedCategory() const { return m_selectedCategory; }
    QVariantList availableCategories() const { return m_availableCategories; }
    int jumpCount() const { return m_jumpCount; }
    QString sessionTime() const { return m_sessionTime; }
    bool mapWindowActive() const { return m_mapWindowActive; }
    
    // Galaxy Map getters
    QVariantList galaxyMapSystems() const { return m_galaxyMapSystems; }
    QVariantMap commanderPosition() const { return m_commanderPosition; }
    int visibleSystemsCount() const { return m_visibleSystemsCount; }
    bool galaxyMapLoading() const { return m_galaxyMapLoading; }
    QVariantList allCommanderLocations() const { return m_allCommanderLocations; }
    double commanderX() const { return m_commanderX; }
    double commanderZ() const { return m_commanderZ; }

    QString nearestDistanceText() const { return m_nearestDistanceText; }
    QString nearestCategoryText() const { return m_nearestCategoryText; }
    int unclaimedIndex() const { return m_unclaimedIndex; }
    int unclaimedTotal() const { return m_unclaimedSystems.size(); }
    QString currentUnclaimedSystemName() const;

    bool suppressMainAppNotifications() const { return m_suppressMainAppNotifications; }
    SupabaseClient* supabaseClient() const { return m_supabaseClient; }
    ClaimManager* claimManager() const { return m_claimManager; }
    
    // Property setters
    void setCommanderName(const QString &name);
    void setCurrentSystem(const QString &system);
    void setSelectedCategory(const QString &category);
    void setSuppressMainAppNotifications(bool suppress);
    
public slots:
    // UI Actions
    void refreshData();
    void viewCurrentSystem();
    void createPOI();
    void openMap();
    void openGalaxyMap();
    void claimSystem(const QString &systemName);
    void viewSystem(const QString &systemName);
    void viewYourSystems();
    void requestSettingsDialog();
    void setForcedCommander(const QString &commander);
    
    // Commander list access
    Q_INVOKABLE QStringList getAllDetectedCommanders() const;

    void copyToClipboard(const QString &text);
    void showAdminPanel();
    
    // Navigation
    void prevUnclaimed();
    void nextUnclaimed();
    void viewClosest();
    void claimClosest();
    
    // System management
    void markSystemVisited(const QString &systemName);
    void markSystemDone(const QString &systemName);
    void unclaimSystem(const QString &systemName);
    void getSystemInformation(const QString &systemName, const QString &category = "");
    void getSystemInformationRobust(const QString &systemName);

    Q_INVOKABLE bool isSystemClaimable(const QString &systemName) const;
    Q_INVOKABLE void getSystemFromEDSM(const QString &systemName);
    Q_INVOKABLE void getEDSMSystemData(const QString &systemName);
    Q_INVOKABLE void markSystemAsPOI(const QString &systemName, const QString &poiType);
    Q_INVOKABLE void removePOIStatus(const QString &systemName);
    Q_INVOKABLE void verifyJournal(); // Admin function to verify journal access
    Q_INVOKABLE void revokeJournalVerification(); // Admin function to revoke journal access
    Q_INVOKABLE void saveSystemDescription(const QString &systemName, const QString &description);
    Q_INVOKABLE void openImagePicker(const QString &systemName);
    Q_INVOKABLE void handleImageSelected(const QString &systemName, const QString &fileName);
    Q_INVOKABLE void uploadImageToImgbb(const QString &filePath, const QString &systemName);
    Q_INVOKABLE bool checkSystemVisitedInJournal(const QString &systemName) const;
    Q_INVOKABLE void saveSystemInformation(const QString &systemName, const QVariantMap &information);
    Q_INVOKABLE void updateSystemStatus(const QString &systemName, bool visited, bool done);
    Q_INVOKABLE void updateSystemVisited(const QString &systemName, bool visited);
    Q_INVOKABLE void updateSystemDone(const QString &systemName, bool done);
    Q_INVOKABLE void markSystemAsEdited(const QString &systemName);
    Q_INVOKABLE void updateSystemImages(const QString &systemName, const QString &primaryImage, 
                                       const QString &primaryTitle, const QString &additionalImages, 
                                       const QString &additionalTitles);
    // Query helpers for QML
    Q_INVOKABLE QString getPrimaryImageForSystem(const QString &systemName) const;

    // Galaxy Map Functions
    Q_INVOKABLE void loadGalaxyMapData();
    Q_INVOKABLE void updateGalaxyMapFilters(const QVariantMap &filters);
    Q_INVOKABLE void focusOnSystem(const QString &systemName);
    Q_INVOKABLE QVariantMap getSystemsInRegion(double minX, double maxX, double minZ, double maxZ);
    
    // Category management
    void updateCategoryFilter(const QStringList &categories);
    void resetCategoryFilter();
    
    // Journal monitoring
    void startJournalMonitoring();
    void stopJournalMonitoring();
    void selectJournalFolder();
    
    // Database operations
    Q_INVOKABLE void startFullDatabaseDownload();
    Q_INVOKABLE void resetDatabaseAuthFailures(); // Reset database authentication failures
    
signals:
    // Property change signals
    void commanderNameChanged();
    void currentSystemChanged();
    void isAdminChanged();
    void nearestSystemsChanged();
    void unclaimedSystemsChanged();
    void selectedCategoryChanged();
    void availableCategoriesChanged();
    void jumpCountChanged();
    void sessionTimeChanged();
    void mapWindowActiveChanged();

    void nearestDistanceTextChanged();
    void nearestCategoryTextChanged();
    void unclaimedIndexChanged();
    void unclaimedTotalChanged();
    void currentUnclaimedSystemNameChanged();

    void suppressMainAppNotificationsChanged();
    
    // Galaxy Map signals
    void galaxyMapSystemsChanged();
    void commanderPositionChanged();
    void visibleSystemsCountChanged();
    void galaxyMapLoadingChanged();
    void allCommanderLocationsChanged();
    
    // Action signals
    void showMessage(const QString &title, const QString &message);
    void showError(const QString &title, const QString &message);
    void showSettingsDialog();
    void allDetectedCommandersChanged();
    void navigationRequested(const QString &systemName);
    void systemUpdated();
    void showSystemPopup(const QString &systemName, const QVariantMap &systemData);
    void openGalaxyMapWindow();
    void systemInformationReceived(const QString &systemName, const QVariantMap &systemInfo);
    void requestFilterChange(const QString &systemTypeFilter);
    void edsmSystemDataReceived(const QString &systemName, const QVariantMap &systemData);
    
    // System claim/unclaim signals for immediate UI updates
    void systemClaimed(const QString &systemName, bool success);
    void systemUnclaimed(const QString &systemName, bool success);
    void systemImageSet(const QString &systemName, const QString &imageUrl, bool success);
    // Bulk signal to notify cards their image may have changed without rebuilding the list
    void systemImagesUpdated(const QStringList &systemNames);

    
    // Database download progress signals
    void databaseDownloadProgress(double progress, const QString &status);
    void databaseDownloadComplete();
    
    // **NEW SIGNAL FOR IMAGE PICKER FIX**
    void requestImagePicker(const QString &systemName);

private slots:
    void updateSessionTime();
    void processJournalUpdate();
    
    // Supabase response handlers
    void handleSystemsReceived(const QJsonArray &systems);
    void handleNearestSystemsReceived(const QJsonArray &systems);
    void handleTakenSystemsReceived(const QJsonArray &taken);
    void handleCategoriesReceived(const QJsonArray &categories);
    void handlePOISReceived(const QJsonArray &pois);
    void handlePOIDataForMerge(const QJsonArray &poiData);
    void handleSystemInformationReceived(const QString &systemName, const QJsonObject &systemInfo);
    void handleAllCommanderLocationsReceived(const QJsonArray &locations);
    void handleBulkSystemImagesLoaded(const QJsonObject &systemImages);
    void handleSupabaseError(const QString &error);
    
private:
    // External dependencies
    SupabaseClient *m_supabaseClient;
    JournalMonitor *m_journalMonitor;
    ConfigManager *m_configManager;
    ClaimManager *m_claimManager;
    QNetworkAccessManager *m_networkManager;
    
    // Internal data
    QString m_commanderName;
    QString m_currentSystem;
    QVariantList m_nearestSystems;
    QVariantList m_unclaimedSystems;
    QString m_selectedCategory;
    QVariantList m_availableCategories;
    QStringList m_poiSystems; // Store POI system names
    QMap<QString, QString> m_poiSystemStatus; // Store actual POI status (POI/Potential POI) for each system
    QStringList m_takenSystems; // Store all taken system names

    QJsonArray m_allTakenSystemsData; // Store complete taken systems data for detailed queries
    // Fast lookup for uploaded primary images by system name (ImgBB URLs)
    QMap<QString, QString> m_systemImages;
    int m_jumpCount;
    QString m_sessionTime;
    bool m_mapWindowActive;

    QString m_nearestDistanceText;
    QString m_nearestCategoryText;
    bool m_sessionJumpTrackingActive; // Only count jumps after initial journal processing

    bool m_suppressMainAppNotifications; // Suppress notifications when SystemViewPopup is open
    bool m_dataRefreshMode = false; // Prevent fake claim events during data rebuilds
    

    
    // Commander position data for distance calculations
    double m_commanderX;
    double m_commanderY;
    double m_commanderZ;
    bool m_hasValidPosition;
    
    // Internal utilities
    QTimer *m_sessionTimer;
    QTimer *m_journalTimer;
    qint64 m_sessionStartTime;
    int m_unclaimedIndex;
    
    // Helper methods
    void initializeData();
    void loadCategories();
    void updateNearestSystems();
    void updateUnclaimedSystems();
    void updateNearestSystemsWithClaimData();
    void formatSessionTime();
    void updateTotalJumpCount();
    
    // Multi-category parsing and formatting (from v1.4incomplete.py specification)
    QStringList parseCategories(const QString &categoryString);
    QString formatCategoriesForDisplay(const QStringList &categories);
    QString getCategoryColorForMulti(const QStringList &categories);
    
    // Database and file operations (placeholder for now)
    bool connectToDatabase();
    bool loadJournalData();
    void updateCommanderLocation();
    
    // Galaxy Map helper functions
    QString determineSystemType(const QVariantMap &systemData);
    double calculateSystemImportance(const QVariantMap &systemData);
    bool isPotentialPOI(const QString &category);
    bool isSystemCompleted(const QString &systemName) const;
    
    // Galaxy Map data
    QVariantList m_galaxyMapSystems;
    QVariantMap m_commanderPosition;
    int m_visibleSystemsCount;
    QVariantList m_allCommanderLocations;
    bool m_galaxyMapLoading;
    QVariantMap m_galaxyMapFilters;
    
    // Forced commander settings
    bool m_forcedCommanderEnabled;
    QString m_forcedCommanderName;
};

#endif // EDRHCONTROLLER_H 