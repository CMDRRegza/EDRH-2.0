#include "edrhcontroller.h"
#include "exceptionmanager.h"
#include <QUrl>
#include "journalmonitor.h"
#include "configmanager.h"
#include "claimmanager.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QDesktopServices>
#include <algorithm>
#include <QUrl>
#include <cmath>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStringList>

EDRHController::EDRHController(QObject *parent)
    : QObject(parent)
    , m_supabaseClient(nullptr)
    , m_journalMonitor(nullptr)
    , m_configManager(nullptr)
    , m_claimManager(nullptr)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_commanderName("Unknown")
    , m_currentSystem("Unknown")
    , m_selectedCategory("All Categories")
    , m_jumpCount(0)
    , m_sessionTime("00:00:00")
    , m_mapWindowActive(false)

    , m_nearestDistanceText("N/A")
    , m_nearestCategoryText("Unknown")
    , m_sessionJumpTrackingActive(false)
    , m_commanderX(0.0)
    , m_commanderY(0.0)
    , m_commanderZ(0.0)
    , m_hasValidPosition(false)
    , m_sessionTimer(new QTimer(this))
    , m_journalTimer(new QTimer(this))
    , m_sessionStartTime(QDateTime::currentMSecsSinceEpoch())
    , m_unclaimedIndex(0)
    , m_suppressMainAppNotifications(false)

{
    // Initialize galaxy map data
    m_visibleSystemsCount = 0;
    m_galaxyMapLoading = false;
    m_commanderPosition = QVariantMap();
    m_galaxyMapFilters = QVariantMap();
    
    // Initialize timers
    connect(m_sessionTimer, &QTimer::timeout, this, &EDRHController::updateSessionTime);
    connect(m_journalTimer, &QTimer::timeout, this, &EDRHController::processJournalUpdate);
    
    // Start session timer (update every second)
    m_sessionTimer->start(1000);
    
    // Initialize data
    initializeData();
}

void EDRHController::setSupabaseClient(SupabaseClient *client)
{
    m_supabaseClient = client;
    
    if (m_supabaseClient) {
        // Connect Supabase signals to our slots
        connect(m_supabaseClient, &SupabaseClient::systemsReceived,
                this, &EDRHController::handleSystemsReceived);
        connect(m_supabaseClient, &SupabaseClient::nearestSystemsReceived,
                this, &EDRHController::handleNearestSystemsReceived);
        connect(m_supabaseClient, &SupabaseClient::takenSystemsReceived,
                this, &EDRHController::handleTakenSystemsReceived);
        connect(m_supabaseClient, &SupabaseClient::categoriesReceived,
                this, &EDRHController::handleCategoriesReceived);
        connect(m_supabaseClient, &SupabaseClient::poisReceived,
                this, &EDRHController::handlePOISReceived);
        connect(m_supabaseClient, &SupabaseClient::poiDataForMergeReceived,
                this, &EDRHController::handlePOIDataForMerge);
        connect(m_supabaseClient, &SupabaseClient::systemInformationReceived,
                this, &EDRHController::handleSystemInformationReceived);
        connect(m_supabaseClient, &SupabaseClient::allCommanderLocationsReceived,
                this, &EDRHController::handleAllCommanderLocationsReceived);
        connect(m_supabaseClient, &SupabaseClient::systemImageSet,
                this, &EDRHController::systemImageSet);
        connect(m_supabaseClient, &SupabaseClient::bulkSystemImagesLoaded,
                this, &EDRHController::handleBulkSystemImagesLoaded);
        connect(m_supabaseClient, &SupabaseClient::systemClaimed,
                this, [this](const QString &systemName, bool success) {
                    if (success) {
                        // Only show main app notification if not suppressed
                        if (!m_suppressMainAppNotifications) {
                            emit showMessage("System Claimed", QString("Successfully claimed %1").arg(systemName));
                        }
                        
                        // EMIT SIGNAL TO QML for immediate UI updates
                        // Mark optimistic current claim without triggering another network request
                        if (m_claimManager) {
                            QMetaObject::invokeMethod(m_claimManager, "applyLocalClaim", Qt::DirectConnection,
                                                      Q_ARG(QString, systemName));
                        }
                        emit systemClaimed(systemName, success);
                        
                        emit systemUpdated();
                        // Refresh taken systems to update claim status immediately
                        m_supabaseClient->getTakenSystems();
                        // Also refresh nearest systems
                        updateNearestSystems();
                    } else {
                        // Always show error messages
                        emit showError("Claim Failed", QString("Failed to claim %1").arg(systemName));
                        
                        // EMIT SIGNAL TO QML even for failures
                        emit systemClaimed(systemName, success);
                    }
                });
        connect(m_supabaseClient, &SupabaseClient::systemUnclaimed,
                this, [this](const QString &systemName, bool success) {
                    qDebug() << "SUPABASE UNCLAIM RESPONSE:" << systemName << "success:" << success;
                    
                    if (success) {
                        // Only show main app notification if not suppressed
                        if (!m_suppressMainAppNotifications) {
                            emit showMessage("System Unclaimed", QString("Successfully unclaimed %1").arg(systemName));
                        }
                        
                        // CRITICAL FIX: Always emit signal for server confirmation, but don't update local state again
                        // The local state was already updated optimistically in unclaimSystem()
                        qDebug() << "Server confirmed unclaim, emitting confirmation signal to QML";
                        
                        // ClaimManager handles all state management now
                        
                        // ALWAYS emit signal to QML for server confirmation (popup needs to know)
                        emit systemUnclaimed(systemName, success);
                        
                        emit systemUpdated();
                        
                        // ClaimManager handles state management
                        
                        // CRITICAL FIX: Refresh taken systems to ensure consistency
                        // Use a small delay to allow database to update
                        QTimer::singleShot(500, this, [this]() {
                            qDebug() << "DELAYED REFRESH: Updating taken systems after unclaim";
                            if (m_supabaseClient) {
                                // Only refresh if no popup is currently open to avoid interference
                                if (!m_suppressMainAppNotifications) {
                                    m_supabaseClient->getTakenSystems();
                                } else {
                                    qDebug() << "DELAYED REFRESH: Skipping because popup is open";
                                }
                            }
                        });
                        
                        // Also refresh nearest systems
                        updateNearestSystems();
                    } else {
                        // FAILURE CASE: Restore local state if server operation failed
                        qWarning() << "Server unclaim failed for" << systemName << "- restoring local state";
                        
                        // ClaimManager handles error recovery automatically
                        
                        // Always show error messages
                        emit showError("Unclaim Failed", QString("Failed to unclaim %1").arg(systemName));
                        
                        // EMIT SIGNAL TO QML even for failures (but with corrected state)
                        emit systemUnclaimed(systemName, success);
                        
                        // Refresh data to get current server state
                        if (m_supabaseClient) {
                            m_supabaseClient->getTakenSystems();
                        }
                    }
                });
        connect(m_supabaseClient, &SupabaseClient::systemMarkedVisited,
                this, [this](const QString &systemName, bool success) {
                    if (success) {
                        emit showMessage("System Visited", QString("Successfully marked %1 as visited").arg(systemName));
                        emit systemUpdated();
                    } else {
                        emit showError("Visit Mark Failed", QString("Failed to mark %1 as visited").arg(systemName));
                    }
                });
        connect(m_supabaseClient, &SupabaseClient::systemStatusUpdated,
                this, [this](const QString &systemName, bool success) {
                    if (success) {
                        qDebug() << "System status successfully updated for" << systemName;
                        if (!m_suppressMainAppNotifications) {
                            emit showMessage("Status Updated", QString("System status updated for %1").arg(systemName));
                        }
                        // CRITICAL FIX: Update both caches immediately to fix claim detection
                        // Update main cache
                        for (int i = 0; i < m_allTakenSystemsData.size(); ++i) {
                            QJsonObject takenSystem = m_allTakenSystemsData[i].toObject();
                            if (takenSystem.value("system").toString() == systemName) {
                                takenSystem["done"] = true;  // Mark as done in local cache
                                m_allTakenSystemsData[i] = takenSystem;  // Update the array element
                                qDebug() << "CACHE FIX: Updated main cache for" << systemName << "to done=true";
                                break;
                            }
                        }
                        
                        // Session cache removed - keeping only main cache update for simplicity
                        
                        // Refresh data to show updated status
                        emit systemUpdated();
                        if (m_supabaseClient) {
                            m_supabaseClient->getTakenSystems();
                            // CRITICAL FIX: Also refresh nearest systems to update Done badges
                            updateNearestSystems();
                        }
                    } else {
                        qDebug() << "Failed to update system status for" << systemName;
                        emit showError("Update Failed", QString("Failed to update status for %1").arg(systemName));
                    }
                });
        connect(m_supabaseClient, &SupabaseClient::networkError,
                this, &EDRHController::handleSupabaseError);
        
        // TODO: Connect image upload completion signal when SupabaseClient implements it
        // connect(m_supabaseClient, &SupabaseClient::imageUploadCompleted,
        //         this, [this](const QString &systemName, const QString &imageUrl, bool success) {
        //             qDebug() << "Image upload completed for" << systemName << "URL:" << imageUrl << "Success:" << success;
        //             
        //             if (success) {
        //                 if (!m_suppressMainAppNotifications) {
        //                     emit showMessage("Image Uploaded", QString("Successfully uploaded image for %1").arg(systemName));
        //                 }
        //             } else {
        //                 emit showError("Upload Failed", QString("Failed to upload image for %1").arg(systemName));
        //             }
        //             
        //             // EMIT SIGNAL TO QML for immediate UI updates
        //             emit systemImageSet(systemName, imageUrl, success);
        //         });
        
        qDebug() << "EDRHController connected to SupabaseClient";
        
        // Initialize ClaimManager if we have it
        if (m_claimManager && !m_commanderName.isEmpty() && m_commanderName != "Unknown") {
            m_claimManager->initialize(m_supabaseClient, m_commanderName);
            qDebug() << "ClaimManager initialized with SupabaseClient";
        }
    }
}

void EDRHController::setJournalMonitor(JournalMonitor *monitor)
{
    m_journalMonitor = monitor;
    
    if (m_journalMonitor) {
        // Connect journal monitor signals
        connect(m_journalMonitor, &JournalMonitor::commanderDetected,
                this, [this](const QString &commander) {
                    qDebug() << "Journal monitor commander detected signal received:" << commander;
                    
                    qDebug() << "Commander detected from journal monitor:" << commander;
                    setCommanderName(commander);
                    
                    // Trigger webhook for program login
                    if (m_supabaseClient) {
                        QVariantMap webhookData;
                        webhookData["commander"] = commander;
                        webhookData["action"] = "Program Login";
                        webhookData["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                        m_supabaseClient->triggerWebhook("program_login", webhookData);
                    }
                });
        connect(m_journalMonitor, &JournalMonitor::commanderNameChanged,
                this, [this]() {
                    if (m_journalMonitor) {
                        setCommanderName(m_journalMonitor->commanderName());
                    }
                });
        connect(m_journalMonitor, &JournalMonitor::currentSystemChanged,
                this, [this]() {
                    if (m_journalMonitor) {
                        setCurrentSystem(m_journalMonitor->currentSystem());
                    }
                });
        connect(m_journalMonitor, &JournalMonitor::fsdJumpDetected,
                this, [this](const QString &system, const QJsonObject &jumpData) {
                    setCurrentSystem(system);
                    
                    // Extract coordinates from jump data for distance calculations
                    if (jumpData.contains("StarPos")) {
                        QJsonArray starPos = jumpData.value("StarPos").toArray();
                        if (starPos.size() >= 3) {
                            bool wasValidBefore = m_hasValidPosition;
                            m_commanderX = starPos[0].toDouble();
                            m_commanderY = starPos[1].toDouble();
                            m_commanderZ = starPos[2].toDouble();
                            m_hasValidPosition = true;
                            qDebug() << "Updated commander position from FSD jump:" << m_commanderX << m_commanderY << m_commanderZ;
                            
                            // Update commander location in database
                            updateCommanderLocation();
                            
                            // **PERFORMANCE FIX**: Only recalculate if we have session tracking active
                            // During journal initialization, we get many historical jumps that spam the database
                            if (m_sessionJumpTrackingActive && !m_nearestSystems.isEmpty()) {
                                if (!wasValidBefore) {
                                qDebug() << "First valid position detected, recalculating distances for" << m_nearestSystems.size() << "systems";
                                } else {
                                    qDebug() << "Position updated from FSD jump, recalculating distances for" << m_nearestSystems.size() << "systems";
                                }
                                QJsonArray systemsArray;
                                for (const QVariant &systemVariant : m_nearestSystems) {
                                    QVariantMap systemMap = systemVariant.toMap();
                                    QJsonObject systemObj;
                                    systemObj["name"] = systemMap.value("name").toString();
                                    systemObj["category"] = systemMap.value("category").toString();
                                    systemObj["x"] = systemMap.value("x").toDouble();
                                    systemObj["y"] = systemMap.value("y").toDouble();
                                    systemObj["z"] = systemMap.value("z").toDouble();
                                    systemObj["poi"] = systemMap.value("poi").toString();
                                    systemObj["done"] = systemMap.value("done").toBool();
                                    systemObj["claimed"] = systemMap.value("claimed").toBool();
                                    systemObj["claimedBy"] = systemMap.value("claimedBy").toString();
                                    systemsArray.append(systemObj);
                                }
                                handleSystemsReceived(systemsArray);
                            } else if (!m_sessionJumpTrackingActive) {
                                qDebug() << "Skipping distance recalculation during journal initialization";
                            }
                        }
                    }
                    
                    // Only count jumps after initial journal processing is complete
                    if (m_sessionJumpTrackingActive) {
                        m_jumpCount++;
                        emit jumpCountChanged();
                        qDebug() << "Session jump count:" << m_jumpCount;
                    } else {
                        qDebug() << "Ignoring jump during initial journal processing";
                    }
                });
        
        connect(m_journalMonitor, &JournalMonitor::carrierJumpDetected,
                this, [this](const QString &system, const QJsonObject &jumpData) {
                    setCurrentSystem(system);
                    
                    // Extract coordinates from jump data for distance calculations
                    if (jumpData.contains("StarPos")) {
                        QJsonArray starPos = jumpData.value("StarPos").toArray();
                        if (starPos.size() >= 3) {
                            m_commanderX = starPos[0].toDouble();
                            m_commanderY = starPos[1].toDouble();
                            m_commanderZ = starPos[2].toDouble();
                            m_hasValidPosition = true;
                            qDebug() << "Updated commander position from Carrier jump:" << m_commanderX << m_commanderY << m_commanderZ;
                            
                            // Update commander location in database
                            updateCommanderLocation();
                            
                            // **PERFORMANCE FIX**: Only recalculate if we have session tracking active
                            // During journal initialization, we get many historical jumps that spam the database
                            if (m_sessionJumpTrackingActive && !m_nearestSystems.isEmpty()) {
                                qDebug() << "Position updated from carrier jump, recalculating distances for" << m_nearestSystems.size() << "systems";
                                QJsonArray systemsArray;
                                for (const QVariant &systemVariant : m_nearestSystems) {
                                    QVariantMap systemMap = systemVariant.toMap();
                                    QJsonObject systemObj;
                                    systemObj["name"] = systemMap.value("name").toString();
                                    systemObj["category"] = systemMap.value("category").toString();
                                    systemObj["x"] = systemMap.value("x").toDouble();
                                    systemObj["y"] = systemMap.value("y").toDouble();
                                    systemObj["z"] = systemMap.value("z").toDouble();
                                    systemObj["poi"] = systemMap.value("poi").toString();
                                    systemObj["done"] = systemMap.value("done").toBool();
                                    systemObj["claimed"] = systemMap.value("claimed").toBool();
                                    systemObj["claimedBy"] = systemMap.value("claimedBy").toString();
                                    systemsArray.append(systemObj);
                                }
                                handleSystemsReceived(systemsArray);
                            } else if (!m_sessionJumpTrackingActive) {
                                qDebug() << "Skipping distance recalculation during journal initialization";
                            }
                        }
                    }
                    
                    // Only count carrier jumps after initial journal processing is complete
                    if (m_sessionJumpTrackingActive) {
                        m_jumpCount++;
                        emit jumpCountChanged();
                        qDebug() << "Session jump count (carrier):" << m_jumpCount;
                    } else {
                        qDebug() << "Ignoring carrier jump during initial journal processing";
                    }
                });
        
        qDebug() << "EDRHController connected to JournalMonitor";
        
        // Start session jump tracking after a short delay to allow initial journal processing
        QTimer::singleShot(3000, this, [this]() {
            m_sessionJumpTrackingActive = true;
            qDebug() << "Session jump tracking activated";
        });
    }
}

void EDRHController::setConfigManager(ConfigManager *configManager)
{
    m_configManager = configManager;
    
    if (m_configManager) {
        // Connect to admin status changes
        connect(m_configManager, &ConfigManager::isAdminChanged,
                this, &EDRHController::isAdminChanged);
        
        qDebug() << "EDRHController connected to ConfigManager, admin status:" << m_configManager->isAdmin();
    }
}

void EDRHController::setClaimManager(ClaimManager *claimManager)
{
    m_claimManager = claimManager;
    
    if (m_claimManager && m_supabaseClient) {
        // Initialize ClaimManager with SupabaseClient and commander name
        m_claimManager->initialize(m_supabaseClient, m_commanderName);
        
        // ClaimManager handles all claim state internally
        
        connect(m_claimManager, &ClaimManager::claimError,
                this, [this](const QString &error) {
                    emit showError("Claim Error", error);
                });
        
        qDebug() << "EDRHController connected to ClaimManager";
    }
}

bool EDRHController::isAdmin() const
{
    return m_configManager ? m_configManager->isAdmin() : false;
}

void EDRHController::setCommanderName(const QString &name)
{
    if (m_commanderName != name) {
        m_commanderName = name;
        emit commanderNameChanged();
        
        // Re-initialize ClaimManager with new commander
        if (m_claimManager && m_supabaseClient && !name.isEmpty() && name != "Unknown") {
            m_claimManager->initialize(m_supabaseClient, name);
            qDebug() << "ClaimManager re-initialized for commander:" << name;
        }
    }
}

void EDRHController::setCurrentSystem(const QString &system)
{
    if (m_currentSystem != system) {
        m_currentSystem = system;
        emit currentSystemChanged();
        
        // **PERFORMANCE FIX**: Only update systems if we have a valid position and session tracking is active
        // During journal initialization, this gets called many times unnecessarily
        if (m_hasValidPosition && m_sessionJumpTrackingActive) {
            // Update nearest systems when current system changes
            updateNearestSystems();
            updateUnclaimedSystems();
        } else {
            qDebug() << "Skipping system update - position not valid or session tracking not active";
        }
    }
}

void EDRHController::setSelectedCategory(const QString &category)
{
    if (m_selectedCategory != category) {
        m_selectedCategory = category;
        emit selectedCategoryChanged();
        
        // Update systems list based on new category filter
        updateNearestSystems();
        updateUnclaimedSystems();
    }
}

// UI Actions
void EDRHController::refreshData()
{
    qDebug() << "Refreshing data from Supabase...";
    
    // Check journal verification before allowing data access
    qDebug() << "Journal verification check - configManager:" << (m_configManager ? "available" : "null");
    if (m_configManager) {
        qDebug() << "Journal verified status:" << m_configManager->journalVerified();
    }
    
    if (m_configManager && !m_configManager->journalVerified()) {
        qWarning() << "Data access blocked - journal not verified";
        emit showError("Access Denied", "Journal verification required for data access.");
        return;
    }
    
    if (m_supabaseClient && m_supabaseClient->isConfigured()) {
        // Auto-reset auth failures to ensure data can load
        if (m_supabaseClient->isInAuthFailureCooldown()) {
            qDebug() << "Auto-resetting auth failures for data refresh...";
            m_supabaseClient->resetAuthFailures();
        }
        
        // Load real data from Supabase - use updateNearestSystems to maintain distance sorting
        updateNearestSystems(); // This intelligently chooses between getSystems and getSystemsNear
        m_supabaseClient->getTakenSystems();
        qDebug() << "Requesting POI systems data for commander:" << m_commanderName;
        m_supabaseClient->getPOISystems(m_commanderName); // Load POI systems with commander context
        m_supabaseClient->getCategories(); // Load categories dynamically
        
        // Refresh ClaimManager data
        if (m_claimManager) {
            m_claimManager->refreshClaimData();
        }
    } else {
        qDebug() << "SupabaseClient not yet configured, waiting for async initialization...";
        // Don't load sample data - wait for real data
    }
    
    loadCategories();
    emit systemUpdated();
}

void EDRHController::viewCurrentSystem()
{
    if (m_currentSystem != "Unknown") {
        viewSystem(m_currentSystem);
    } else {
        emit showMessage("No System", "No system currently detected!");
    }
}

void EDRHController::createPOI()
{
    if (m_currentSystem == "Unknown") {
        emit showMessage("No System", "No system currently detected!");
        return;
    }
    
    qDebug() << "Creating POI for system:" << m_currentSystem;
    // TODO: Implement POI creation logic
    emit showMessage("POI Created", QString("POI created for %1").arg(m_currentSystem));
}

void EDRHController::openMap()
{
    qDebug() << "Opening galaxy map...";
    m_mapWindowActive = true;
    emit mapWindowActiveChanged();
    // TODO: Implement map window opening
}

void EDRHController::claimSystem(const QString &systemName)
{
    qDebug() << "EDRHController::claimSystem forwarding to ClaimManager";
    
    if (!m_claimManager) {
        emit showError("System Error", "Claim manager not initialized");
        return;
    }
    
    // Check journal verification before allowing claim
    if (m_configManager && !m_configManager->journalVerified()) {
        qWarning() << "Claim denied - journal not verified";
        emit showError("Access Denied", 
                       "Journal verification required to claim systems.\n\n"
                       "Recheck your journals?");
        return;
    }
    
    // Let ClaimManager handle the claim
    m_claimManager->claimSystem(systemName);
    
    // Only show immediate feedback if not suppressed
    if (!m_suppressMainAppNotifications) {
        emit showMessage("Claiming System", QString("Claiming %1 for %2...").arg(systemName, m_commanderName));
    }
    qDebug() << "Claim request sent to SupabaseClient";
}

void EDRHController::openGalaxyMap()
{
    qDebug() << "Opening built-in galaxy map...";
    
    // Emit signal to open the built-in galaxy map window
    emit openGalaxyMapWindow();
    
    // Removed annoying popup message
}

void EDRHController::viewSystem(const QString &systemName)
{
    auto timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    qDebug() << "🔥 [" << timestamp << "] C++ EDRHController::viewSystem CALLED for:" << systemName;
    
    if (systemName.isEmpty() || systemName == "Unknown" || systemName == "Unknown System") {
        emit showError("Invalid System", "Please select a valid system to view.");
        return;
    }
    
    // Note: System name copying is available by clicking the system name text
    // No automatic copying when viewing system details
    
    // Find the system data in our nearest systems list
    QVariantMap systemData;
    for (const QVariant &systemVariant : m_nearestSystems) {
        QVariantMap system = systemVariant.toMap();
        if (system.value("name").toString() == systemName) {
            systemData = system;
            break;
        }
    }
    
    // If not found in nearest systems, try to get data from database
    if (systemData.isEmpty()) {
        qDebug() << "System not found in nearest systems, requesting from database:" << systemName;
        
        // Don't show error popup for current system - it's expected to not be in our database
        bool isCurrentSystem = (systemName == m_currentSystem);
        
        // Request detailed system information from database (but don't show errors for current system)
        if (m_supabaseClient && m_supabaseClient->isConfigured() && !isCurrentSystem) {
            m_supabaseClient->getSystemDetailsRobust(systemName);
        }
        
        // Always try EDSM API as a fallback for systems not in our database
        getSystemFromEDSM(systemName);
        
        // Create basic system data while waiting for database/EDSM response
        systemData["name"] = systemName;
        systemData["category"] = isCurrentSystem ? "Current System" : "Unknown";
        systemData["distance"] = "Unknown";  // Be honest about unknown distance
        systemData["poi"] = "";
        // DON'T set claimed/claimedBy/done here - let ClaimManager determine this
        // systemData["claimed"] = false;  // REMOVED - was overriding actual claim data
        // systemData["claimedBy"] = "";    // REMOVED - was overriding actual claim data
        // systemData["done"] = false;      // REMOVED - was overriding actual claim data
        // Don't set x/y/z as undefined - let the popup handle missing coordinates properly
    }
    
    // Show the internal system popup instead of opening EDSM
    qDebug() << "🚀 [" << timestamp << "] C++ EMITTING showSystemPopup signal for:" << systemName;
    emit showSystemPopup(systemName, systemData);
    qDebug() << "✅ [" << timestamp << "] C++ showSystemPopup signal EMITTED for:" << systemName;
    emit navigationRequested(systemName);
}

void EDRHController::viewYourSystems()
{
    qDebug() << "Viewing your systems (claims and done systems)...";
    
    if (m_commanderName == "Unknown") {
        emit showError("No Commander", "Commander name not detected. Please check journal monitoring.");
        return;
    }
    
    // Request the UI to apply "Your Claims" filter
    emit requestFilterChange("Your Claims");
    
    // Show a brief confirmation message
    emit showMessage("Filter Applied", QString("Showing your claimed systems, %1!").arg(m_commanderName));
}

void EDRHController::copyToClipboard(const QString &text)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);
    emit showMessage("Copied", QString("Copied: %1").arg(text));
}

void EDRHController::showAdminPanel()
{
    if (!isAdmin()) {
        emit showError("Access Denied", "Admin privileges required");
        return;
    }
    
    qDebug() << "Opening admin panel...";
    // TODO: Implement admin panel
}

// Navigation
void EDRHController::prevUnclaimed()
{
    if (!m_unclaimedSystems.isEmpty() && m_unclaimedIndex > 0) {
        m_unclaimedIndex--;
        emit unclaimedIndexChanged();
        emit currentUnclaimedSystemNameChanged();
        updateUnclaimedSystems();
    }
}

void EDRHController::nextUnclaimed()
{
    if (!m_unclaimedSystems.isEmpty() && m_unclaimedIndex < m_unclaimedSystems.size() - 1) {
        m_unclaimedIndex++;
        emit unclaimedIndexChanged();
        emit currentUnclaimedSystemNameChanged();
        updateUnclaimedSystems();
    }
}

QString EDRHController::currentUnclaimedSystemName() const
{
    if (!m_unclaimedSystems.isEmpty() && m_unclaimedIndex < m_unclaimedSystems.size()) {
        QVariantMap system = m_unclaimedSystems[m_unclaimedIndex].toMap();
        return system.value("name").toString();
    }
    return "None";
}

void EDRHController::viewClosest()
{
    if (!m_unclaimedSystems.isEmpty()) {
        QVariantMap system = m_unclaimedSystems[m_unclaimedIndex].toMap();
        viewSystem(system["name"].toString());
    }
}

void EDRHController::claimClosest()
{
    if (!m_unclaimedSystems.isEmpty()) {
        QVariantMap system = m_unclaimedSystems[m_unclaimedIndex].toMap();
        claimSystem(system["name"].toString());
    }
}

// System management
void EDRHController::markSystemVisited(const QString &systemName)
{
    if (m_commanderName == "Unknown") {
        emit showError("No Commander", "Commander name not detected. Please check journal monitoring.");
        return;
    }
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    qDebug() << "Marking system as visited:" << systemName << "by commander:" << m_commanderName;
    
    // Use SupabaseClient to mark system as visited
    m_supabaseClient->markSystemVisited(systemName, m_commanderName);
    
    // Show immediate feedback
    emit showMessage("Marking Visited", QString("Marking %1 as visited...").arg(systemName));
}

void EDRHController::markSystemDone(const QString &systemName)
{
    qDebug() << "Marking system as done:" << systemName;
    
    // TODO: Implement proper "done" status tracking in SupabaseClient
    // For now, just show a message
    emit showMessage("Marked Done", QString("Marked %1 as done (local only)").arg(systemName));
    emit systemUpdated();
}

void EDRHController::unclaimSystem(const QString &systemName)
{
    qDebug() << "EDRHController::unclaimSystem forwarding to ClaimManager";
    
    if (!m_claimManager) {
        emit showError("System Error", "Claim manager not initialized");
        return;
    }
    
    // Let ClaimManager handle the unclaim completely
    m_claimManager->unclaimSystem(systemName);
    
    // Only show immediate feedback if not suppressed
    if (!m_suppressMainAppNotifications) {
        emit showMessage("Unclaiming System", QString("Unclaiming %1...").arg(systemName));
    }
}



void EDRHController::getSystemInformation(const QString &systemName, const QString &category)
{
    if (m_supabaseClient) {
        m_supabaseClient->getSystemInformation(systemName, category);
    } else {
        emit showError("Database Error", "Unable to get system information: database not connected");
    }
}

void EDRHController::getSystemInformationRobust(const QString &systemName)
{
    qDebug() << "Getting robust system information with coordinates for:" << systemName;
    if (m_supabaseClient) {
        m_supabaseClient->getSystemDetailsRobust(systemName);
    } else {
        emit showError("Database Error", "Unable to get system information: database not connected");
    }
}



bool EDRHController::isSystemClaimable(const QString &systemName) const
{
    qDebug() << "EDRHController::isSystemClaimable forwarding to ClaimManager";
    
    if (!m_claimManager) {
        qDebug() << "ClaimManager not available";
        return false;
    }
    
    return m_claimManager->canClaimSystem(systemName);
}

void EDRHController::getSystemFromEDSM(const QString &systemName)
{
    if (systemName.isEmpty()) {
        qDebug() << "EDSM: Empty system name provided";
        return;
    }
    
    qDebug() << "Fetching system data from EDSM API for:" << systemName;
    
    // Construct EDSM API URL
    QString url = QString("https://www.edsm.net/api-v1/system?systemName=%1&showCoordinates=1&showInformation=1&showPrimaryStar=1")
                      .arg(QString(systemName).replace(' ', '+'));
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EDRH/1.4.0-qt");
    
    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("systemName", systemName);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QString systemName = reply->property("systemName").toString();
        
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "EDSM API error for" << systemName << ":" << reply->errorString();
            reply->deleteLater();
            return;
        }
        
        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            qDebug() << "EDSM JSON parse error:" << parseError.errorString();
            reply->deleteLater();
            return;
        }
        
        QJsonObject systemData = doc.object();
        
        if (systemData.isEmpty() || systemData.value("name").toString().isEmpty()) {
            qDebug() << "EDSM: System not found:" << systemName;
            reply->deleteLater();
            return;
        }
        
        // Convert EDSM data to our format
        QVariantMap mappedData;
        mappedData["name"] = systemData.value("name").toString();
        mappedData["category"] = "External System";  // EDSM doesn't provide categories
        
        // Extract coordinates
        if (systemData.contains("coords")) {
            QJsonObject coords = systemData.value("coords").toObject();
            mappedData["x"] = coords.value("x").toDouble();
            mappedData["y"] = coords.value("y").toDouble(); 
            mappedData["z"] = coords.value("z").toDouble();
            
            // Calculate distance if we have commander position
            if (m_hasValidPosition) {
                double dx = coords.value("x").toDouble() - m_commanderX;
                double dy = coords.value("y").toDouble() - m_commanderY;
                double dz = coords.value("z").toDouble() - m_commanderZ;
                double distance = sqrt(dx*dx + dy*dy + dz*dz);
                mappedData["distance"] = QString::number(distance, 'f', 2) + " LY";
            } else {
                mappedData["distance"] = "Unknown";
            }
        } else {
            mappedData["x"] = 0.0;
            mappedData["y"] = 0.0;
            mappedData["z"] = 0.0;
            mappedData["distance"] = "Unknown";
        }
        
        // Extract system information
        if (systemData.contains("information")) {
            QJsonObject info = systemData.value("information").toObject();
            mappedData["allegiance"] = info.value("allegiance").toString();
            mappedData["government"] = info.value("government").toString();
            mappedData["economy"] = info.value("economy").toString();
            mappedData["population"] = info.value("population").toInt();
        }
        
        // Extract primary star information
        if (systemData.contains("primaryStar")) {
            QJsonObject star = systemData.value("primaryStar").toObject();
            mappedData["primaryStarType"] = star.value("type").toString();
            mappedData["isScoopable"] = star.value("isScoopable").toBool();
        }
        
        mappedData["source"] = "EDSM";
        mappedData["poi"] = "";
        mappedData["done"] = false;
        mappedData["claimed"] = false;
        mappedData["claimedBy"] = "";
        
        qDebug() << "EDSM: Successfully fetched data for" << systemName;
        emit edsmSystemDataReceived(systemName, mappedData);
        
        reply->deleteLater();
    });
}

void EDRHController::markSystemAsPOI(const QString &systemName, const QString &poiType)
{
    qDebug() << "EDRHController::markSystemAsPOI called with:" << systemName << "type:" << poiType;
    
    // Check journal verification before allowing POI operations
    if (m_configManager && !m_configManager->journalVerified()) {
        qWarning() << "POI operation blocked - journal not verified";
        emit showError("Access Denied", "Journal verification required for POI operations.");
        return;
    }
    
    if (m_commanderName == "Unknown") {
        qDebug() << "POI marking failed: No commander name detected";
        emit showError("No Commander", "Commander name not detected. Please check journal monitoring.");
        return;
    }
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "POI marking failed: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    qDebug() << "Marking system as" << poiType << ":" << systemName << "by commander:" << m_commanderName;
    
    // Use SupabaseClient to mark the system as POI
    m_supabaseClient->markSystemAsPOI(systemName, poiType, m_commanderName);
    
    // Trigger webhook for POI upload
    if (m_supabaseClient) {
        QVariantMap webhookData;
        webhookData["commander"] = m_commanderName;
        webhookData["system"] = systemName;
        webhookData["poi_type"] = poiType;
        webhookData["action"] = "POI Upload";
        m_supabaseClient->triggerWebhook("poi_upload", webhookData);
    }
    
    // Show immediate feedback
    emit showMessage("Marking POI", QString("Marking %1 as %2...").arg(systemName, poiType));

    // Optimistic UI update: set POI locally so badge updates immediately
    bool updated = false;
    for (int i = 0; i < m_nearestSystems.size(); ++i) {
        QVariantMap s = m_nearestSystems[i].toMap();
        if (s.value("name").toString() == systemName) {
            s["poi"] = poiType;
            s["potential_or_poi"] = poiType;
            m_nearestSystems[i] = s;
            updated = true;
            break;
        }
    }
    if (updated) {
        m_poiSystemStatus[systemName] = poiType;
        qDebug() << "Optimistic POI set for" << systemName << ":" << poiType;
        emit nearestSystemsChanged();
    }
}

void EDRHController::verifyJournal()
{
    qDebug() << "EDRHController::verifyJournal called";
    
    // Only allow admin to verify journals
    if (!isAdmin()) {
        qWarning() << "Journal verification attempted by non-admin user";
        emit showError("Access Denied", "Only administrators can verify journal access.");
        return;
    }
    
    if (!m_configManager) {
        qWarning() << "ConfigManager not available for journal verification";
        emit showError("Error", "Configuration manager not available.");
        return;
    }
    
    // Set journal as verified
    m_configManager->setJournalVerified(true);
    m_configManager->saveConfig();
    
    qDebug() << "Journal verification enabled by admin";
    emit showMessage("Journal Verified", "Journal access has been verified and enabled.");
    
    // Trigger admin action webhook
    if (m_supabaseClient) {
        QVariantMap webhookData;
        webhookData["admin"] = m_commanderName;
        webhookData["action"] = "Journal Verification Enabled";
        webhookData["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        m_supabaseClient->triggerWebhook("admin_action", webhookData);
    }
}

void EDRHController::revokeJournalVerification()
{
    qDebug() << "EDRHController::revokeJournalVerification called";
    
    // Only allow admin to revoke journal verification
    if (!isAdmin()) {
        qWarning() << "Journal verification revocation attempted by non-admin user";
        emit showError("Access Denied", "Only administrators can revoke journal verification.");
        return;
    }
    
    if (!m_configManager) {
        qWarning() << "ConfigManager not available for journal verification revocation";
        emit showError("Error", "Configuration manager not available.");
        return;
    }
    
    // Set journal as not verified
    m_configManager->setJournalVerified(false);
    m_configManager->saveConfig();
    
    qDebug() << "Journal verification revoked by admin";
    emit showMessage("Journal Verification Revoked", "Journal access has been revoked. Commander will need re-verification.");
    
    // Trigger admin action webhook
    if (m_supabaseClient) {
        QVariantMap webhookData;
        webhookData["admin"] = m_commanderName;
        webhookData["action"] = "Journal Verification Revoked";
        webhookData["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        m_supabaseClient->triggerWebhook("admin_action", webhookData);
    }
}

void EDRHController::removePOIStatus(const QString &systemName)
{
    qDebug() << "EDRHController::removePOIStatus called with:" << systemName;
    
    if (m_commanderName == "Unknown") {
        emit showError("No Commander", "Commander name not detected. Please check journal monitoring.");
        return;
    }
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "POI removal failed: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    qDebug() << "Removing POI status for system:" << systemName << "by commander:" << m_commanderName;
    
    // Use SupabaseClient to remove POI status with commander context
    m_supabaseClient->removePOIStatus(systemName, m_commanderName);
    
    // Show immediate feedback
    emit showMessage("Removing POI", QString("Removing POI status from %1...").arg(systemName));

    // Optimistic UI update: clear POI locally so badge disappears immediately
    bool changed = false;
    for (int i = 0; i < m_nearestSystems.size(); ++i) {
        QVariantMap s = m_nearestSystems[i].toMap();
        if (s.value("name").toString() == systemName) {
            if (s.value("poi").toString() != "") {
                s["poi"] = "";
                s["potential_or_poi"] = "";
                m_nearestSystems[i] = s;
                changed = true;
            }
            break;
        }
    }
    if (changed) {
        m_poiSystemStatus.remove(systemName);
        qDebug() << "Optimistic POI cleared for" << systemName;
        emit nearestSystemsChanged();
    }
}

void EDRHController::saveSystemDescription(const QString &systemName, const QString &description)
{
    qDebug() << "EDRHController::saveSystemDescription called with:" << systemName;
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "Description save failed: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    qDebug() << "Saving description for system:" << systemName;
    
    // Use SupabaseClient to save description
    m_supabaseClient->saveSystemDescription(systemName, description);
    
    // Show immediate feedback
    emit showMessage("Saving Description", QString("Saving description for %1...").arg(systemName));
}

void EDRHController::openImagePicker(const QString &systemName)
{
    qDebug() << "EDRHController::openImagePicker called with:" << systemName;
    
    // **FIX: QWidget crash** - Signal QML to open file dialog instead of using QFileDialog directly
    // QFileDialog requires QApplication, but QML apps use QGuiApplication
    // Let QML handle the file dialog using Qt.labs.platform.FileDialog
    emit requestImagePicker(systemName);
    
    qDebug() << "Image picker request sent to QML for system:" << systemName;
}

// New function to handle file selection from QML
void EDRHController::handleImageSelected(const QString &systemName, const QString &fileName)
{
    qDebug() << "EDRHController::handleImageSelected called with:" << systemName << "file:" << fileName;
    
    if (fileName.isEmpty()) {
        qDebug() << "No file selected for image upload";
        return;
    }
    
    // Convert QML file URL to local path
    QString localPath = QUrl(fileName).toLocalFile();
    if (localPath.isEmpty()) {
        localPath = fileName; // Fallback if not a URL
    }
    
    qDebug() << "Selected image file:" << localPath;
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "Image upload failed: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    // Upload image to imgbb
    m_supabaseClient->uploadImageToImgbb(localPath, systemName);
    
    // Show immediate feedback
    emit showMessage("Uploading Image", QString("Uploading image for %1...").arg(systemName));
}

// Category management
void EDRHController::updateCategoryFilter(const QStringList &categories)
{
    qDebug() << "Updating category filter:" << categories;
    // TODO: Implement category filtering
    updateNearestSystems();
    updateUnclaimedSystems();
}

void EDRHController::resetCategoryFilter()
{
    setSelectedCategory("All Categories");
}

// Journal monitoring
void EDRHController::startJournalMonitoring()
{
    qDebug() << "Starting journal monitoring...";
    m_journalTimer->start(1000); // Check every second
    // TODO: Implement journal monitoring
}

void EDRHController::stopJournalMonitoring()
{
    qDebug() << "Stopping journal monitoring...";
    m_journalTimer->stop();
}

void EDRHController::selectJournalFolder()
{
    qDebug() << "Selecting journal folder...";
    // TODO: Implement folder selection dialog
}

// Private slots
void EDRHController::updateSessionTime()
{
    formatSessionTime();
}

void EDRHController::processJournalUpdate()
{
    // TODO: Implement journal file processing
    // This would read the latest journal entries and update the current system/commander
}

// Private methods
void EDRHController::initializeData()
{
    // Load categories first
    loadCategories();
    
    // Don't load sample data during initialization - let real data load asynchronously
    // when Supabase gets configured later
    
    // Load journal data to get current system
    loadJournalData();
}

void EDRHController::loadCategories()
{
    // Request categories from Supabase instead of using hardcoded list
    if (m_supabaseClient && m_supabaseClient->isConfigured()) {
        qDebug() << "Requesting categories from Supabase...";
        m_supabaseClient->getCategories();
    } else {
        qDebug() << "Supabase not configured, using fallback categories";
        // Fallback categories for when Supabase is not configured
        QVariantList categories;
        categories << "All Categories"
                   << "Wolf-Rayet Star"
                   << "High Metal Content World"
                   << "Scenic"
                   << "Binary Planets"
                   << "Potential POI"
                   << "POI";
        
        m_availableCategories = categories;
        emit availableCategoriesChanged();
    }
}

void EDRHController::updateNearestSystems()
{
    if (!m_supabaseClient) {
        qDebug() << "No SupabaseClient available for updateNearestSystems";
        return;
    }
    
    if (!m_supabaseClient->isConfigured()) {
        qDebug() << "SupabaseClient not yet configured, skipping updateNearestSystems";
        return;
    }
    
    // Check if we're in authentication failure cooldown to avoid spam
    if (m_supabaseClient->isInAuthFailureCooldown()) {
        // Skip silently during authentication issues
        return;
    }
    
    // **PERFORMANCE FIX**: Implement request throttling to prevent excessive database calls
    static QDateTime lastRequestTime;
    static QString lastRequestPosition;
    
    QString currentPosition = QString("%1,%2,%3").arg(m_commanderX).arg(m_commanderY).arg(m_commanderZ);
    QDateTime now = QDateTime::currentDateTime();
    
    // Don't make request if position hasn't changed significantly
    if (lastRequestPosition == currentPosition && lastRequestTime.addSecs(2) > now) {
        qDebug() << "Skipping redundant database request - same position and within 2 seconds of last request";
        return;
    }
    
    lastRequestPosition = currentPosition;
    lastRequestTime = now;
    
    // Use distance-based queries when we have valid commander position
    if (m_hasValidPosition) {
        qDebug() << "Using commander position for distance-based system sorting:" << m_commanderX << m_commanderY << m_commanderZ;
        // Get nearby systems with distance calculation from current position
        m_supabaseClient->getSystemsNear(m_commanderX, m_commanderY, m_commanderZ, 2000); // Reasonable limit for performance
    } else {
        qDebug() << "No valid commander position, fetching all systems without distance sorting";
        // Fallback to general systems list if no position available
        m_supabaseClient->getSystems();
    }
}

void EDRHController::updateUnclaimedSystems()
{
    QVariantList unclaimedSystems;
    
    // Filter nearest systems for unclaimed ones
    for (const QVariant &systemVariant : m_nearestSystems) {
        QVariantMap system = systemVariant.toMap();
        bool claimed = system.value("claimed", false).toBool();
        
        if (!claimed) {
            unclaimedSystems.append(system);
        }
    }
    
    // Sort by distance (they should already be sorted, but make sure)
    std::sort(unclaimedSystems.begin(), unclaimedSystems.end(), 
              [](const QVariant &a, const QVariant &b) {
                  QVariantMap systemA = a.toMap();
                  QVariantMap systemB = b.toMap();
                  
                  // Extract numeric distance for comparison
                  QString distA = systemA.value("distance").toString();
                  QString distB = systemB.value("distance").toString();
                  
                  // Remove " LY" suffix and convert to double
                  distA.remove(" LY");
                  distB.remove(" LY");
                  
                  return distA.toDouble() < distB.toDouble();
              });
    
    m_unclaimedSystems = unclaimedSystems;
    emit unclaimedSystemsChanged();
    emit unclaimedTotalChanged();
    emit currentUnclaimedSystemNameChanged();
    
    // Update nearest unclaimed display
    if (!unclaimedSystems.isEmpty() && m_unclaimedIndex < unclaimedSystems.size()) {
        QVariantMap nearest = unclaimedSystems[m_unclaimedIndex].toMap();
        m_nearestDistanceText = nearest["distance"].toString();
        m_nearestCategoryText = nearest["category"].toString();
        emit nearestDistanceTextChanged();
        emit nearestCategoryTextChanged();
    } else {
        // No unclaimed systems found
        m_nearestDistanceText = "N/A";
        m_nearestCategoryText = "No unclaimed systems nearby";
        emit nearestDistanceTextChanged();
        emit nearestCategoryTextChanged();
    }
}

void EDRHController::formatSessionTime()
{
    qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - m_sessionStartTime) / 1000;
    int hours = elapsed / 3600;
    int minutes = (elapsed % 3600) / 60;
    int seconds = elapsed % 60;
    
    m_sessionTime = QString("%1:%2:%3")
                        .arg(hours, 2, 10, QChar('0'))
                        .arg(minutes, 2, 10, QChar('0'))
                        .arg(seconds, 2, 10, QChar('0'));
    
    emit sessionTimeChanged();
}

void EDRHController::updateTotalJumpCount()
{
    // This function is deprecated - we now track session jumps only
    // Session jumps are incremented directly in FSD/Carrier jump signal handlers
    qDebug() << "updateTotalJumpCount() called but ignored - using session-only tracking";
}

// Placeholder implementations for database and file operations
bool EDRHController::connectToDatabase()
{
    qDebug() << "Connecting to database...";
    
    if (!m_supabaseClient) {
        qWarning() << "No Supabase client available";
        return false;
    }
    
    if (!m_supabaseClient->isConfigured()) {
        qWarning() << "Supabase client not configured";
        return false;
    }
    
    qDebug() << "Database connection ready";
    return true;
}

bool EDRHController::loadJournalData()
{
    qDebug() << "Loading journal data...";
    
    // Journal data is now handled by JournalMonitor
    // If monitor is available and has data, use it
    if (m_journalMonitor) {
        QString commanderName = m_journalMonitor->commanderName();
        QString currentSystem = m_journalMonitor->currentSystem();
        
        qDebug() << "Journal monitor data - Commander:" << commanderName << "System:" << currentSystem;
        
        if (!commanderName.isEmpty() && commanderName != "Unknown") {
            qDebug() << "Setting commander name from journal:" << commanderName;
            setCommanderName(commanderName);
        } else {
            qDebug() << "Commander name from journal is empty or Unknown, keeping current:" << m_commanderName;
        }
        if (!currentSystem.isEmpty()) {
            setCurrentSystem(currentSystem);
        } else {
            setCurrentSystem("Sol");  // Default fallback
        }
    } else {
        setCurrentSystem("Sol");  // Default fallback
    }
    
    return true;
}

void EDRHController::updateCommanderLocation()
{
    if (!m_supabaseClient || m_commanderName.isEmpty() || !m_hasValidPosition) {
        return;
    }
    
    qDebug() << "Updating commander location in database:" << m_commanderName << "at" 
             << m_commanderX << m_commanderY << m_commanderZ << "in system:" << m_currentSystem;
    
    // Update commander location in the database
    m_supabaseClient->updateCommanderLocation(m_commanderName, m_commanderX, m_commanderY, m_commanderZ, m_currentSystem);
}

// Supabase response handlers
void EDRHController::handleSystemsReceived(const QJsonArray &systems)
{
    ExceptionManager::instance().safeCatch("EDRHController::handleSystemsReceived", [&]() {
        qDebug() << "Received" << systems.size() << "systems from Supabase";
    
    QVariantList systemsList;
    for (const QJsonValue &value : systems) {
        QJsonObject system = value.toObject();
        
        QVariantMap systemMap;
        // Data is already transformed in SupabaseClient, so use 'name' not 'systems'
        systemMap["name"] = system.value("name").toString();
        
        // Parse and format categories for multi-category support
        QString rawCategory = system.value("category").toString();
        QStringList categoryList = parseCategories(rawCategory);
        systemMap["category"] = formatCategoriesForDisplay(categoryList);
        systemMap["categoryList"] = QVariant::fromValue(categoryList); // Store raw list for filtering
        systemMap["categoryColor"] = getCategoryColorForMulti(categoryList);
        
        // Calculate distance from current position if we have valid coordinates
        double x = system.value("x").toDouble();
        double y = system.value("y").toDouble();
        double z = system.value("z").toDouble();
        
        if (m_hasValidPosition && x != 0.0 && y != 0.0 && z != 0.0) {
            // Calculate distance from current commander position
            double distance = qSqrt(qPow(x - m_commanderX, 2) + qPow(y - m_commanderY, 2) + qPow(z - m_commanderZ, 2));
            systemMap["distance"] = QString::number(distance, 'f', 1) + " LY";
        } else {
            // No position or coordinates available
            systemMap["distance"] = "N/A";
        }
        
        systemMap["poi"] = system.value("poi").toString();
        systemMap["done"] = system.value("done").toBool();
        
        // Determine claim status based on taken systems data
        QString systemName = systemMap["name"].toString();
        bool isClaimed = m_takenSystems.contains(systemName);
        QString claimedBy = "";
        
        if (isClaimed) {
            // Find who claimed this system from the taken systems data
            for (const QJsonValue &takenValue : m_allTakenSystemsData) {
                QJsonObject takenSystem = takenValue.toObject();
                if (takenSystem.value("system").toString() == systemName) {
                    claimedBy = takenSystem.value("by_cmdr").toString();  // Fixed: use correct field name
                    if (claimedBy.compare("empty", Qt::CaseInsensitive) == 0) {
                        // Normalize placeholder to unclaimed
                        isClaimed = false;
                        claimedBy.clear();
                    }
                    break;
                }
            }
        }
        
        systemMap["claimed"] = isClaimed;
        systemMap["claimedBy"] = claimedBy;
        systemMap["x"] = x;
        systemMap["y"] = y;
        systemMap["z"] = z;
        
        systemsList.append(systemMap);
    }
    
    // Sort by distance when we have position data
    if (m_hasValidPosition) {
        std::sort(systemsList.begin(), systemsList.end(), [](const QVariant &a, const QVariant &b) {
            QVariantMap systemA = a.toMap();
            QVariantMap systemB = b.toMap();
            
            QString distanceStrA = systemA.value("distance").toString();
            QString distanceStrB = systemB.value("distance").toString();
            
            // Handle N/A distances
            if (distanceStrA == "N/A") return false;
            if (distanceStrB == "N/A") return true;
            
            // Extract numeric values from distance strings
            distanceStrA.replace(" LY", "").replace("LY", "").replace(",", "");
            distanceStrB.replace(" LY", "").replace("LY", "").replace(",", "");
            
            bool okA, okB;
            double distanceA = distanceStrA.toDouble(&okA);
            double distanceB = distanceStrB.toDouble(&okB);
            
            if (!okA && !okB) return false;
            if (!okA) return false;
            if (!okB) return true;
            
            return distanceA < distanceB;
        });
    }
    
    m_nearestSystems = systemsList;
    emit nearestSystemsChanged();
    
    // Also update galaxy map systems with the real database data
    m_galaxyMapSystems = systemsList;
    m_visibleSystemsCount = systemsList.size();
    emit galaxyMapSystemsChanged();
    emit visibleSystemsCountChanged();
    
    // Set loading to false now that we have data
    if (m_galaxyMapLoading) {
        m_galaxyMapLoading = false;
        emit galaxyMapLoadingChanged();
    }
    
        qDebug() << "Updated galaxy map with" << systemsList.size() << "real systems from database";
    });
}

void EDRHController::handleNearestSystemsReceived(const QJsonArray &systems)
{
    ExceptionManager::instance().safeCatch("EDRHController::handleNearestSystemsReceived", [&]() {
        qDebug() << "Received" << systems.size() << "systems from Supabase";
    
    // Step 1: Group systems by name and combine categories
    QMap<QString, QVariantMap> systemsMap;

    // Preserve important per-system UI state across refreshes to avoid "forgetting"
    // images/POI/claim badges while other async feeds (POI, taken, images) arrive.
    QMap<QString, QVariantMap> previousByName;
    for (const QVariant &item : m_nearestSystems) {
        const QVariantMap prev = item.toMap();
        const QString prevName = prev.value("name").toString();
        if (!prevName.isEmpty()) {
            previousByName.insert(prevName, prev);
        }
    }
    
    for (const QJsonValue &value : systems) {
        QJsonObject system = value.toObject();
        QString systemName = system.value("name").toString();
        
        if (systemName.isEmpty()) continue;
        
        // Parse this system's category
        QString rawCategory = system.value("category").toString();
        
        if (systemsMap.contains(systemName)) {
            // System already exists, add category to existing entry
            QVariantMap existingSystem = systemsMap[systemName];
            QStringList existingCategories = existingSystem["categoryList"].toStringList();
            
            // Add new category if not already present
            if (!rawCategory.isEmpty() && !existingCategories.contains(rawCategory)) {
                existingCategories.append(rawCategory);
                existingSystem["categoryList"] = existingCategories;
                
                // Update display category
                existingSystem["category"] = formatCategoriesForDisplay(existingCategories);
                existingSystem["categoryColor"] = getCategoryColorForMulti(existingCategories);
                
                systemsMap[systemName] = existingSystem;
                // Debug: Combined categories for system
            }
        } else {
            // New system, create entry
            QVariantMap systemMap;
            systemMap["name"] = systemName;
            
            // Initialize category list
            QStringList categoryList;
            if (!rawCategory.isEmpty()) {
                categoryList.append(rawCategory);
            }
            systemMap["categoryList"] = categoryList;
            systemMap["category"] = formatCategoriesForDisplay(categoryList);
            systemMap["categoryColor"] = getCategoryColorForMulti(categoryList);
            
            // Distance is already formatted as string in transformed data
            systemMap["distance"] = system.value("distance").toString();
            
            // Get POI status from loaded POI data instead of hardcoding
            QString poiStatus = "";
            
            // Check if this system is in the detailed POI systems data
            for (const QString &poiSystemName : m_poiSystems) {
                if (poiSystemName == systemName) {
                    qDebug() << "System" << systemName << "found in POI systems, determining actual status...";
                    
                    // Try to determine the actual POI type from available data
                    // We need to check the system category or other indicators
                    
                    // Get the actual POI status from our stored map
                    if (m_poiSystemStatus.contains(systemName)) {
                        poiStatus = m_poiSystemStatus[systemName];
                        qDebug() << "Found actual POI status for" << systemName << ":" << poiStatus;
                    } else {
                        // Fallback if not found in map
                        poiStatus = "POI";
                        qDebug() << "No POI status found for" << systemName << ", defaulting to POI";
                    }
                    
                    qDebug() << "Set POI status for" << systemName << "to:" << poiStatus;
                    break;
                }
            }
            
            // Additional fallback logic
            if (poiStatus.isEmpty()) {
                if (rawCategory.contains("Potential POI", Qt::CaseInsensitive)) {
                    poiStatus = "Potential POI";
                }
            }
            
            // If we still do not have a POI status, preserve the previous one (avoids flicker)
            if (poiStatus.isEmpty() && previousByName.contains(systemName)) {
                const QVariantMap prev = previousByName.value(systemName);
                const QString prevPoi = prev.value("poi").toString();
                if (!prevPoi.isEmpty()) poiStatus = prevPoi;
            }
            systemMap["poi"] = poiStatus;
            
            // Determine claim status and done status based on complete taken systems data
            bool isClaimed = false;
            bool isDone = false;
            QString claimedBy = "";
            for (const QJsonValue &takenValue : m_allTakenSystemsData) {
                const QJsonObject takenSystem = takenValue.toObject();
                if (takenSystem.value("system").toString() == systemName) {
                    isClaimed = true;
                    claimedBy = takenSystem.value("by_cmdr").toString();
                    isDone = takenSystem.value("done").toBool();
                    if (claimedBy.compare("empty", Qt::CaseInsensitive) == 0) {
                        isClaimed = false;
                        claimedBy.clear();
                    }
                    break;
                }
            }
            
            systemMap["done"] = isDone;
            
            systemMap["claimed"] = isClaimed;
            systemMap["claimedBy"] = claimedBy;

            // Preserve uploaded/preset image URL if we already had it from a prior bulk load
            if (previousByName.contains(systemName)) {
                const QVariantMap prev = previousByName.value(systemName);
                const QString prevImages = prev.value("images").toString();
                if (!prevImages.isEmpty()) {
                    systemMap["images"] = prevImages;
                }
            }
            
            // Store coordinates for popup display
            systemMap["x"] = system.value("x").toDouble();
            systemMap["y"] = system.value("y").toDouble();
            systemMap["z"] = system.value("z").toDouble();
            
            systemsMap[systemName] = systemMap;
        }
    }
    
    // Step 2: Convert map back to list for UI
    QVariantList systemsList;
    for (auto it = systemsMap.begin(); it != systemsMap.end(); ++it) {
        systemsList.append(it.value());
    }
    
    // Step 3: Sort systems by distance (closest first)
    std::sort(systemsList.begin(), systemsList.end(), [](const QVariant &a, const QVariant &b) {
        QVariantMap systemA = a.toMap();
        QVariantMap systemB = b.toMap();
        
        QString distanceStrA = systemA.value("distance").toString();
        QString distanceStrB = systemB.value("distance").toString();
        
        // Extract numeric values from distance strings (remove "LY" suffix)
        distanceStrA.replace(" LY", "").replace("LY", "").replace(",", "");
        distanceStrB.replace(" LY", "").replace("LY", "").replace(",", "");
        
        bool okA, okB;
        double distanceA = distanceStrA.toDouble(&okA);
        double distanceB = distanceStrB.toDouble(&okB);
        
        // If conversion failed, put those systems at the end
        if (!okA && !okB) return false;
        if (!okA) return false;
        if (!okB) return true;
        
        return distanceA < distanceB;
    });
    
    qDebug() << "Processed and combined" << systemsList.size() << "unique systems from" << systems.size() << "database entries";
    m_nearestSystems = systemsList;
    emit nearestSystemsChanged();
    
    // Load images for visible systems
    if (!systemsList.isEmpty() && m_supabaseClient) {
        QStringList systemNames;
        // Get first 100 systems for image loading (to avoid overloading)
        int count = qMin(100, systemsList.size());
        for (int i = 0; i < count; i++) {
            QVariantMap system = systemsList[i].toMap();
            systemNames.append(system["name"].toString());
        }
        
        if (!systemNames.isEmpty()) {
            qDebug() << "Loading images for" << systemNames.size() << "visible systems";
            m_supabaseClient->loadSystemImagesForSystems(systemNames);
        }
    }
    
    // Update unclaimed systems based on the new nearest systems data
    updateUnclaimedSystems();
    
    // CRITICAL FIX: Also populate galaxy map systems with the same data
    m_galaxyMapSystems = systemsList;
    m_visibleSystemsCount = systemsList.size();
    emit galaxyMapSystemsChanged();
    emit visibleSystemsCountChanged();
    
    // Set loading to false now that we have data (for galaxy map)
    if (m_galaxyMapLoading) {
        m_galaxyMapLoading = false;
        emit galaxyMapLoadingChanged();
        qDebug() << "Galaxy map loading completed via handleNearestSystemsReceived - found" << systemsList.size() << "systems";
    }
    });
}

void EDRHController::handleTakenSystemsReceived(const QJsonArray &taken)
{
    qDebug() << "Received" << taken.size() << "taken systems from Supabase";
    
    // CRITICAL FIX: Only update ClaimManager for complete datasets, not single-system queries
    // Single-system queries (0-1 items) are from SystemViewPopup and should not corrupt ClaimManager's complete data
    if (taken.size() > 10) {  // Complete dataset has 396 systems, single queries have 0-1
        qDebug() << "Complete dataset detected - updating ClaimManager and UI";
        
        // Store complete taken systems data for UI display ONLY
        // Actual claim management is handled by ClaimManager
        m_allTakenSystemsData = taken;
        
        // Update nearest systems display to show claimed status
        updateNearestSystemsWithClaimData();
        
        // Nudge cards to refresh claim badges without rebuilding list
        // Gather the systems present to minimize work
        QStringList names;
        names.reserve(m_nearestSystems.size());
        for (const QVariant &v : m_nearestSystems) {
            const QVariantMap s = v.toMap();
            const QString n = s.value("name").toString();
            if (!n.isEmpty()) names.append(n);
        }
        if (!names.isEmpty()) emit systemImagesUpdated(names); // reuse lightweight signal
    } else {
        qDebug() << "Single-system query detected - skipping ClaimManager update to prevent data corruption";
        // SystemViewPopup will handle this data directly, don't corrupt ClaimManager's complete dataset
    }
}

void EDRHController::updateNearestSystemsWithClaimData()
{
    // Update the nearest systems list with claim data from m_allTakenSystemsData
    for (int i = 0; i < m_nearestSystems.size(); ++i) {
        QVariantMap system = m_nearestSystems[i].toMap();
        QString systemName = system["name"].toString();
        
        // Find this system in taken data
        bool isClaimed = false;
        QString claimedBy;
        bool isDone = false;
        
        for (const QJsonValue &value : m_allTakenSystemsData) {
            QJsonObject takenSystem = value.toObject();
            if (takenSystem["system"].toString() == systemName) {
                isClaimed = true;
                claimedBy = takenSystem["by_cmdr"].toString();
                isDone = takenSystem["done"].toBool();
                if (claimedBy.compare("empty", Qt::CaseInsensitive) == 0) {
                    isClaimed = false;
                    claimedBy.clear();
                }
                break;
            }
        }
        
        // Update system data
        system["claimed"] = isClaimed;
        system["claimedBy"] = claimedBy;
        system["done"] = isDone;
        
        m_nearestSystems[i] = system;
    }
    
    emit nearestSystemsChanged();
}



void EDRHController::handlePOISReceived(const QJsonArray &pois)
{
    qDebug() << "Received" << pois.size() << "POI systems from Supabase";
    
    // Store POI system names for lookup (from system_information table)
    m_poiSystems.clear();
    for (const QJsonValue &value : pois) {
        QJsonObject poiSystem = value.toObject();
        QString systemName = poiSystem.value("system").toString();
        QString potentialOrPoi = poiSystem.value("potential_or_poi").toString();
        
        if (!systemName.isEmpty() && !potentialOrPoi.isEmpty()) {
            // Add all systems with POI status (both "POI" and "Potential POI")
            m_poiSystems.append(systemName);
            // Store the actual POI status for each system
            m_poiSystemStatus[systemName] = potentialOrPoi;
            qDebug() << "Stored POI status for" << systemName << ":" << potentialOrPoi;
        }
    }
    
    qDebug() << "Loaded" << m_poiSystems.size() << "POI systems";
}

void EDRHController::handlePOIDataForMerge(const QJsonArray &poiData)
{
    qDebug() << "=== POI DATA MERGE START ===";
    qDebug() << "Incoming array size:" << poiData.size();
    qDebug() << "Current systems count:" << m_nearestSystems.size();

    if (poiData.isEmpty() || m_nearestSystems.isEmpty()) {
        qDebug() << "Nothing to merge (empty inputs)";
        qDebug() << "=== POI DATA MERGE END ===";
        return;
    }

    // Determine payload shape: either a list of POI rows ({ system, potential_or_poi, ... })
    // or a list of systems with merged POI ({ name, potential_or_poi, ... })
    QJsonObject first = poiData.first().toObject();
    bool payloadIsPoiRows = first.contains("system");
    bool payloadIsSystems = first.contains("name");

    bool systemsUpdated = false;

    if (payloadIsPoiRows) {
        qDebug() << "POI merge payload detected as POI rows";
        for (int i = 0; i < m_nearestSystems.size(); i++) {
            QVariantMap system = m_nearestSystems[i].toMap();
            QString systemName = system.value("name").toString();
            for (const QJsonValue &poiValue : poiData) {
                QJsonObject poiSystem = poiValue.toObject();
                if (systemName == poiSystem.value("system").toString()) {
                    QString potentialOrPoi = poiSystem.value("potential_or_poi").toString();
                    // Set if present; if empty, clear
                    if (!potentialOrPoi.isEmpty()) {
                        system["poi"] = potentialOrPoi;
                        system["potential_or_poi"] = potentialOrPoi;
                        m_poiSystemStatus[systemName] = potentialOrPoi;
                    } else {
                        system.remove("poi");
                        system.remove("potential_or_poi");
                        m_poiSystemStatus.remove(systemName);
                    }
                    m_nearestSystems[i] = system;
                    systemsUpdated = true;
                    qDebug() << "Updated system" << systemName << "with POI rows payload ->" << (potentialOrPoi.isEmpty() ? "CLEARED" : potentialOrPoi);
                    break;
                }
            }
        }
    } else if (payloadIsSystems) {
        qDebug() << "POI merge payload detected as systems with merged POI";
        // The payload contains exactly the systems we asked to refresh.
        // For each provided system, set or clear its POI value accordingly.
        for (const QJsonValue &val : poiData) {
            QJsonObject sys = val.toObject();
            QString name = sys.value("name").toString();
            QString potential = sys.value("potential_or_poi").toString();
            if (name.isEmpty()) continue;
            for (int i = 0; i < m_nearestSystems.size(); i++) {
                QVariantMap system = m_nearestSystems[i].toMap();
                if (system.value("name").toString() == name) {
                    if (!potential.isEmpty()) {
                        system["poi"] = potential;
                        system["potential_or_poi"] = potential;
                        m_poiSystemStatus[name] = potential;
                    } else {
                        system.remove("poi");
                        system.remove("potential_or_poi");
                        m_poiSystemStatus.remove(name);
                    }
                    m_nearestSystems[i] = system;
                    systemsUpdated = true;
                    qDebug() << "Updated system" << name << "with POI status (systems payload):" << (potential.isEmpty() ? "CLEARED" : potential);
                    break;
                }
            }
        }
    } else {
        qDebug() << "Unknown POI merge payload shape; skipping";
    }

    if (systemsUpdated) {
        qDebug() << "POI data merged, emitting nearestSystemsChanged signal";
        
        // CRITICAL FIX: Also update galaxy map systems with the same POI data
        // since filtering uses galaxyMapSystems, not nearestSystems
        m_galaxyMapSystems = m_nearestSystems;
        m_visibleSystemsCount = m_nearestSystems.size();
        
        emit nearestSystemsChanged();
        emit galaxyMapSystemsChanged();
        emit visibleSystemsCountChanged();
        
        // Also update unclaimed systems since POI data affects them too
        updateUnclaimedSystems();
        
        qDebug() << "Galaxy map systems also updated with POI data. Systems count:" << m_galaxyMapSystems.size();
    } else {
        qDebug() << "No systems were updated with POI data";
    }
    qDebug() << "=== POI DATA MERGE END ===";
}

void EDRHController::handleSupabaseError(const QString &error)
{
    qWarning() << "Supabase error:" << error;
    
    // Don't show error popups for expected "not found" scenarios
    if (!error.contains("not found") && !error.contains("No rows found")) {
    emit showError("Database Error", "Failed to connect to database: " + error);
    }
    
    // Fallback to sample data
    updateNearestSystems();
    updateUnclaimedSystems();
}

void EDRHController::handleCategoriesReceived(const QJsonArray &categories)
{
    qDebug() << "Received" << categories.size() << "categories from Supabase";
    
    QVariantList categoriesList;
    for (const QJsonValue &value : categories) {
        QString category = value.toString();
        if (!category.isEmpty()) {
            categoriesList.append(category);
        }
    }
    
    qDebug() << "Processed categories:" << categoriesList;
    m_availableCategories = categoriesList;
    emit availableCategoriesChanged();
}

void EDRHController::handleSystemInformationReceived(const QString &systemName, const QJsonObject &systemInfo)
{
    // Convert QJsonObject to QVariantMap for QML
    QVariantMap systemInfoMap = systemInfo.toVariantMap();
    
    // Add some additional metadata
    systemInfoMap["systemName"] = systemName;
    systemInfoMap["hasInformation"] = !systemInfo.isEmpty();
    
    // Emit signal for QML to handle
    emit systemInformationReceived(systemName, systemInfoMap);
}

void EDRHController::handleAllCommanderLocationsReceived(const QJsonArray &locations)
{
    QVariantList locationList;
    
    for (const auto &locationValue : locations) {
        QJsonObject location = locationValue.toObject();
        QVariantMap locationMap;
        
        locationMap["name"] = location.value("name").toString();
        // NOTE: Database schema doesn't have location fields yet, using placeholder values
        locationMap["x"] = 0.0; // location.value("current_x").toDouble();
        locationMap["y"] = 0.0; // location.value("current_y").toDouble(); 
        locationMap["z"] = 0.0; // location.value("current_z").toDouble();
        locationMap["system"] = "Unknown"; // location.value("current_system").toString();
        locationMap["lastSeen"] = location.value("first_seen").toString();
        locationMap["currentCommander"] = m_commanderName; // For filtering out self
        
        locationList.append(locationMap);
    }
    
    m_allCommanderLocations = locationList;
    emit allCommanderLocationsChanged();
    
    qDebug() << "Updated commander list, received" << locationList.size() << "commanders (location tracking not yet implemented in schema)";
}

// Multi-category parsing and formatting implementation (from v1.4incomplete.py specification)
QStringList EDRHController::parseCategories(const QString &categoryString)
{
    if (categoryString.isEmpty()) {
        return QStringList();
    }
    
    // Split by semicolon and clean up whitespace
    QStringList categories = categoryString.split(';', Qt::SkipEmptyParts);
    QStringList cleanedCategories;
    
    for (const QString &category : categories) {
        QString cleaned = category.trimmed();
        if (!cleaned.isEmpty()) {
            cleanedCategories.append(cleaned);
        }
    }
    
    return cleanedCategories;
}

QString EDRHController::formatCategoriesForDisplay(const QStringList &categories)
{
    if (categories.isEmpty()) {
        return "Unknown";
    }
    
    if (categories.size() == 1) {
        return categories.first();
    }
    
    // Join multiple categories with bullet separator (as in Python version)
    return categories.join(" • ");
}

QString EDRHController::getCategoryColorForMulti(const QStringList &categories)
{
    if (categories.isEmpty()) {
        return "#888888"; // Default gray
    }
    
    // Priority color mapping based on v1.4incomplete.py
    // Check for highest priority categories first
    for (const QString &category : categories) {
        if (category == "POI") return "#FF6B6B"; // Red for POI
        if (category == "Binary Planets") return "#4ECDC4"; // Teal for binary planets
        if (category == "Wolf-Rayet Star") return "#FFE66D"; // Yellow for Wolf-Rayet
        if (category == "Scenic") return "#95E1D3"; // Light green for scenic
        if (category.startsWith("PVP")) return "#FF7F50"; // Orange for PVP systems
        if (category.contains("Richard")) return "#9B59B6"; // Purple for Richard systems
    }
    
    // Default colors for common categories
    QString firstCategory = categories.first().toLower();
    if (firstCategory.contains("metal")) return "#B0BEC5"; // Gray for metal content
    if (firstCategory.contains("water")) return "#64B5F6"; // Blue for water worlds
    if (firstCategory.contains("earth")) return "#81C784"; // Green for earth-likes
    
    return "#888888"; // Default gray
}

void EDRHController::startFullDatabaseDownload()
{
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        emit databaseDownloadProgress(0.0, "Database not configured");
        return;
    }
    
    qDebug() << "Starting full database download...";
    
    // Emit initial progress
    emit databaseDownloadProgress(0.1, "Connecting to database...");
    
    // Start downloading all database components sequentially with progress tracking
    // 1. Download all systems (40% of progress)
    emit databaseDownloadProgress(0.2, "Downloading system database...");
    m_supabaseClient->getSystems();
    
    // 2. Download taken systems (20% of progress) 
    emit databaseDownloadProgress(0.5, "Downloading claimed systems...");
    m_supabaseClient->getTakenSystems();
    
    // 3. Download POI systems (20% of progress)
    emit databaseDownloadProgress(0.7, "Downloading POI database...");
    m_supabaseClient->getPOISystems(m_commanderName);
    
    // 4. Download categories (10% of progress)
    emit databaseDownloadProgress(0.8, "Downloading categories...");
    m_supabaseClient->getCategories();
    
    // 5. Download preset images (10% of progress)
    emit databaseDownloadProgress(0.9, "Loading preset images...");
    m_supabaseClient->getPresetImages(true);
    
    // Complete after short delay to allow all requests to process
    QTimer::singleShot(2000, this, [this]() {
        emit databaseDownloadProgress(1.0, "Database download complete!");
        emit databaseDownloadComplete();
    });
}

void EDRHController::resetDatabaseAuthFailures()
{
    qDebug() << "resetDatabaseAuthFailures called from QML";
    if (m_supabaseClient) {
        m_supabaseClient->resetAuthFailures();
        
        // Try to reload data after resetting auth failures
        if (m_supabaseClient->isConfigured()) {
            qDebug() << "Retrying data load after resetting auth failures...";
            loadGalaxyMapData();
        }
    } else {
        qDebug() << "No SupabaseClient available for resetDatabaseAuthFailures";
    }
}

// Galaxy Map Functions Implementation
void EDRHController::loadGalaxyMapData()
{
    ExceptionManager::instance().safeCatch("EDRHController::loadGalaxyMapData", [&]() {
        if (!m_supabaseClient) {
            qDebug() << "No Supabase client available for galaxy map";
            return;
        }
    
    m_galaxyMapLoading = true;
    emit galaxyMapLoadingChanged();
    
    qDebug() << "Loading galaxy map data...";
    qDebug() << "Current galaxyMapSystems count before loading:" << m_galaxyMapSystems.size();
    
    // Clear existing data
    m_galaxyMapSystems.clear();
    emit galaxyMapSystemsChanged(); // Emit to clear the QML side too
    
    // Load all necessary data for galaxy map
    qDebug() << "Calling getSystems() for galaxy map...";
    
    // Automatically reset auth failures if we're in cooldown to ensure data loads
    if (m_supabaseClient->isInAuthFailureCooldown()) {
        qDebug() << "Auto-resetting auth failure cooldown for galaxy map...";
        m_supabaseClient->resetAuthFailures();
    }
    
    // Check if we have any systems loaded at all, if not, do a full refresh
    if (m_nearestSystems.isEmpty()) {
        qDebug() << "No systems loaded yet, doing full data refresh first...";
        refreshData(); // This will call getSystems() and load all the data
    } else {
        qDebug() << "Systems already loaded (" << m_nearestSystems.size() << "), using existing data for galaxy map";
        // Use existing data for galaxy map
        m_galaxyMapSystems = m_nearestSystems;
        emit galaxyMapSystemsChanged();
        
        // Set loading to false since we have data
        m_galaxyMapLoading = false;
        emit galaxyMapLoadingChanged();
    }
    
    // Also refresh taken systems and POI data
    m_supabaseClient->getTakenSystems();
    m_supabaseClient->getPOISystems(m_commanderName);
    
    // Load commander locations for admin mode
    if (isAdmin()) {
        m_supabaseClient->getAllCommanderLocations();
    }
    
    // Update commander position from current coordinates
    if (m_hasValidPosition) {
        m_commanderPosition.clear();
        m_commanderPosition["x"] = m_commanderX;
        m_commanderPosition["y"] = m_commanderY;
        m_commanderPosition["z"] = m_commanderZ;
        m_commanderPosition["system"] = m_currentSystem;
        emit commanderPositionChanged();
    }
    
    // Note: Don't set loading to false here - wait for actual data to come back
    // The loading flag will be set to false when data handlers complete
    });
}

void EDRHController::updateGalaxyMapFilters(const QVariantMap &filters)
{
    m_galaxyMapFilters = filters;
    
    // Process the systems data based on new filters
    QVariantList filteredSystems;
    
    for (const auto &system : m_nearestSystems) {
        QVariantMap systemMap = system.toMap();
        QString systemType = determineSystemType(systemMap);
        
        // Check if this system type is enabled in filters
        if (filters.contains(systemType) && filters[systemType].toMap()["enabled"].toBool()) {
            // Add type information for rendering
            systemMap["type"] = systemType;
            systemMap["importance"] = calculateSystemImportance(systemMap);
            filteredSystems.append(systemMap);
        }
    }
    
    m_galaxyMapSystems = filteredSystems;
    m_visibleSystemsCount = filteredSystems.size();
    
    emit galaxyMapSystemsChanged();
    emit visibleSystemsCountChanged();
}

void EDRHController::focusOnSystem(const QString &systemName)
{
    if (systemName.isEmpty()) {
        return;
    }
    
    qDebug() << "Focusing galaxy map on system:" << systemName;
    
    // Find the system in our data
    for (const auto &system : m_galaxyMapSystems) {
        QVariantMap systemMap = system.toMap();
        if (systemMap["name"].toString() == systemName) {
            // Emit signal to update map focus
            emit showSystemPopup(systemName, systemMap);
            break;
        }
    }
}

QVariantMap EDRHController::getSystemsInRegion(double minX, double maxX, double minZ, double maxZ)
{
    QVariantMap result;
    QVariantList systemsInRegion;
    int totalCount = 0;
    
    for (const auto &system : m_galaxyMapSystems) {
        QVariantMap systemMap = system.toMap();
        double x = systemMap["x"].toDouble();
        double z = systemMap["z"].toDouble();
        
        if (x >= minX && x <= maxX && z >= minZ && z <= maxZ) {
            systemsInRegion.append(systemMap);
            totalCount++;
        }
    }
    
    result["systems"] = systemsInRegion;
    result["count"] = totalCount;
    result["bounds"] = QVariantMap{
        {"minX", minX}, {"maxX", maxX},
        {"minZ", minZ}, {"maxZ", maxZ}
    };
    
    return result;
}

// Helper function to determine system type for galaxy map
QString EDRHController::determineSystemType(const QVariantMap &systemData)
{
    QString systemName = systemData["name"].toString();
    
    // Check if system is claimed by current user
    if (m_claimManager && m_claimManager->getSystemClaimedBy(systemName) == m_commanderName) {
        return "yourClaims";
    }
    
    // Check if system is claimed by others
    if (m_takenSystems.contains(systemName)) {
        return "othersClaims";
    }
    
    // Check if system is a POI
    if (m_poiSystems.contains(systemName)) {
        // Further distinguish between POI types if needed
        return "POIs";
    }
    
    // Check for potential POI (implement logic based on your criteria)
    QString category = systemData["category"].toString();
    if (isPotentialPOI(category)) {
        return "potentialPOIs";
    }
    
    // Check if system is completed
    // (implement based on your completion criteria)
    if (isSystemCompleted(systemName)) {
        return "completed";
    }
    
    // Default to unclaimed
    return "unclaimed";
}

// Helper function to calculate system importance for LOD
double EDRHController::calculateSystemImportance(const QVariantMap &systemData)
{
    double importance = 0.5; // Base importance
    
    QString systemType = systemData["type"].toString();
    QString category = systemData["category"].toString();
    
    // Higher importance for user's systems
    if (systemType == "yourClaims") {
        importance += 0.4;
    }
    
    // Higher importance for POIs
    if (systemType == "POIs") {
        importance += 0.3;
    }
    
    // Higher importance for rare categories
    if (category.contains("Legend") || category.contains("Pendulum")) {
        importance += 0.2;
    }
    
    return qBound(0.0, importance, 1.0);
}

// Helper function to check if system is potential POI
bool EDRHController::isPotentialPOI(const QString &category)
{
    // Define criteria for potential POI systems
    static const QStringList potentialCategories = {
        "Close binary to star",
        "Extreme close proximity to ring",
        "Pendulum",
        "The Legend"
    };
    
    return potentialCategories.contains(category);
}

// Helper function to check if system is completed - SIMPLE AND RELIABLE
bool EDRHController::isSystemCompleted(const QString &systemName) const
{
    qDebug() << "COMPLETION CHECK: Checking if system" << systemName << "is completed";
    
    // Simple, reliable check - just look through the current data
    // If we can't find it or data is empty, assume not completed (safe default)
    for (const auto &value : m_allTakenSystemsData) {
        QJsonObject takenSystem = value.toObject();
        QString system = takenSystem.value("system").toString();
        
        if (system == systemName) {
            bool done = takenSystem.value("done").toBool();
            qDebug() << "COMPLETION CHECK: Found" << systemName << "-> done:" << done;
            return done;
        }
    }
    
    qDebug() << "COMPLETION CHECK:" << systemName << "not found -> not completed";
    return false;  // Safe default: if we can't find it, assume not done
}

void EDRHController::uploadImageToImgbb(const QString &filePath, const QString &systemName)
{
    if (!m_supabaseClient) {
        emit showError("Upload Error", "Service not connected");
        // Emit failure signal to QML
        emit systemImageSet(systemName, "", false);
        return;
    }
    
    qDebug() << "EDRHController: Forwarding image upload request to SupabaseClient";
    qDebug() << "  File:" << filePath;
    qDebug() << "  System:" << systemName;
    
    // Forward the request to SupabaseClient which handles the ImgBB upload
    m_supabaseClient->uploadImageToImgbb(filePath, systemName);
    // SupabaseClient will emit systemImageSet signal when upload completes
}

void EDRHController::setSuppressMainAppNotifications(bool suppress)
{
    if (m_suppressMainAppNotifications != suppress) {
        m_suppressMainAppNotifications = suppress;
        qDebug() << "EDRHController: Suppress main app notifications set to:" << suppress;
        emit suppressMainAppNotificationsChanged();
    }
}

bool EDRHController::checkSystemVisitedInJournal(const QString &systemName) const
{
    qDebug() << "EDRHController::checkSystemVisitedInJournal called for:" << systemName;
    
    // Check if journal monitor is available
    if (!m_journalMonitor) {
        qDebug() << "No journal monitor available, returning false";
        return false;
    }
    
    // For now, return false as a placeholder. This function should
    // check the journal monitor's visited systems data when that functionality
    // is implemented in the JournalMonitor class.
    // 
    // Future implementation should call something like:
    // return m_journalMonitor->isSystemVisited(systemName);
    
    qDebug() << "Journal visit check not yet implemented, returning false";
    return false;
}

void EDRHController::saveSystemInformation(const QString &systemName, const QVariantMap &information)
{
    qDebug() << "EDRHController::saveSystemInformation called for:" << systemName;
    qDebug() << "Information to save:" << information;
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "Cannot save system information: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    if (m_commanderName.isEmpty() || m_commanderName == "Unknown") {
        qDebug() << "Cannot save system information: No commander name";
        emit showError("No Commander", "Commander name not detected.");
        return;
    }
    
    // Set commander context and call the saveSystemInformation method
    m_supabaseClient->setCommanderContext(m_commanderName);
    m_supabaseClient->saveSystemInformation(systemName, information);
    
    qDebug() << "System information save request sent for" << systemName;
}

void EDRHController::updateSystemStatus(const QString &systemName, bool visited, bool done)
{
    qDebug() << "EDRHController::updateSystemStatus called for:" << systemName;
    qDebug() << "Visited:" << visited << "Done:" << done;
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "Cannot update system status: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    if (m_commanderName.isEmpty() || m_commanderName == "Unknown") {
        qDebug() << "Cannot update system status: No commander name";
        emit showError("No Commander", "Commander name not detected.");
        return;
    }
    
    // Set commander context and call the updateSystemStatus method
    m_supabaseClient->setCommanderContext(m_commanderName);
    m_supabaseClient->updateSystemStatus(systemName, visited, done);
    
    qDebug() << "System status update request sent for" << systemName;
}

void EDRHController::getEDSMSystemData(const QString &systemName)
{
    // This is an alias for getSystemFromEDSM to maintain compatibility with QML
    getSystemFromEDSM(systemName);
}

void EDRHController::updateSystemVisited(const QString &systemName, bool visited)
{
    qDebug() << "EDRHController::updateSystemVisited called for:" << systemName << "visited:" << visited;
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "Cannot update system visited: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    if (m_commanderName.isEmpty() || m_commanderName == "Unknown") {
        qDebug() << "Cannot update system visited: No commander name";
        emit showError("No Commander", "Commander name not detected.");
        return;
    }
    
    // Set commander context and call the updateSystemVisited method
    m_supabaseClient->setCommanderContext(m_commanderName);
    m_supabaseClient->updateSystemVisited(systemName, visited);
    
    qDebug() << "System visited update request sent for" << systemName;
}

void EDRHController::updateSystemDone(const QString &systemName, bool done)
{
    qDebug() << "EDRHController::updateSystemDone called for:" << systemName << "done:" << done;
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "Cannot update system done: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    
    if (m_commanderName.isEmpty() || m_commanderName == "Unknown") {
        qDebug() << "Cannot update system done: No commander name";
        emit showError("No Commander", "Commander name not detected.");
        return;
    }
    
    // Set commander context and call the updateSystemDone method
    m_supabaseClient->setCommanderContext(m_commanderName);
    m_supabaseClient->updateSystemDone(systemName, done);
    
    qDebug() << "System done update request sent for" << systemName;
}

void EDRHController::updateSystemImages(const QString &systemName, const QString &primaryImage, 
                                       const QString &primaryTitle, const QString &additionalImages, 
                                       const QString &additionalTitles)
{
    qDebug() << "EDRHController::updateSystemImages called for:" << systemName;
    qDebug() << "Primary image:" << primaryImage;
    qDebug() << "Primary title:" << primaryTitle;
    qDebug() << "Additional images:" << additionalImages;
    qDebug() << "Additional titles:" << additionalTitles;
    
    // Defensive check for corrupted data
    if (primaryImage.contains("[object Object]")) {
        qDebug() << "ERROR: Primary image contains [object Object] - data corruption detected!";
        qDebug() << "This usually means an array was passed instead of a string URL";
        emit showError("Image Error", "Image data is corrupted. Please re-upload your images.");
        return;
    }
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "Cannot update system images: Supabase not configured";
        emit showError("Database Error", "Database not configured or unavailable.");
        return;
    }
    
    if (m_commanderName.isEmpty() || m_commanderName == "Unknown") {
        qDebug() << "Cannot update system images: No commander name";
        emit showError("No Commander", "Commander name not detected.");
        return;
    }
    
    // Set commander context
    m_supabaseClient->setCommanderContext(m_commanderName);
    
    // Call Supabase client to update the images
    m_supabaseClient->updateSystemImages(systemName, primaryImage, primaryTitle, 
                                       additionalImages, additionalTitles);
    
    qDebug() << "System images update request sent for" << systemName;
}

void EDRHController::handleBulkSystemImagesLoaded(const QJsonObject &systemImages)
{
    qDebug() << "Received bulk system images for" << systemImages.size() << "systems";
    
    // Update nearest systems with image data
    bool updated = false;
    QStringList updatedSystems;
    for (int i = 0; i < m_nearestSystems.size(); i++) {
        QVariantMap system = m_nearestSystems[i].toMap();
        QString systemName = system["name"].toString();
        
        if (systemImages.contains(systemName)) {
            system["images"] = systemImages[systemName].toString();
            m_systemImages[systemName] = system["images"].toString();
            m_nearestSystems[i] = system;
            updated = true;
            updatedSystems.append(systemName);
        }
    }
    
    if (updated) {
        qDebug() << "Updated system data with images - NOT emitting nearestSystemsChanged to preserve component state";
        // CRITICAL FIX: Don't emit nearestSystemsChanged for image updates only
        // This was causing SystemCard components to be destroyed/recreated, 
        // which corrupted claim status and caused the 2-system bug
        // Image updates are handled automatically by property bindings
        emit systemImagesUpdated(updatedSystems);
    }
}

QString EDRHController::getPrimaryImageForSystem(const QString &systemName) const
{
    return m_systemImages.value(systemName);
}

void EDRHController::markSystemAsEdited(const QString &systemName)
{
    qDebug() << "EDRHController::markSystemAsEdited called for:" << systemName;
    
    if (!m_supabaseClient || !m_supabaseClient->isConfigured()) {
        qDebug() << "Cannot mark system as edited: Supabase not configured";
        return;
    }
    
    if (m_commanderName.isEmpty() || m_commanderName == "Unknown") {
        qDebug() << "Cannot mark system as edited: No commander name";
        return;
    }
    
    // Set commander context and mark as edited
    m_supabaseClient->setCommanderContext(m_commanderName);
    m_supabaseClient->markSystemAsEdited(systemName);
    
    qDebug() << "System marked as edited request sent for" << systemName;
} 