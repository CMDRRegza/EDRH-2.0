#include "supabaseclient.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>
#include <QtMath>
#include <QDateTime>
#include <QSet>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <algorithm>
#include <cmath>

SupabaseClient::SupabaseClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_lastAuthFailureTime(0)
    , m_consecutiveAuthFailures(0)
    , m_syncInProgress(false)
    , m_webhookConfigLoaded(false)
{
    // Connect network manager signals - QNetworkAccessManager::finished passes the reply as parameter
    connect(m_networkManager, &QNetworkAccessManager::finished, 
            this, &SupabaseClient::handleNetworkReply);
    qDebug() << "SupabaseClient: Connected to QNetworkAccessManager::finished with queued connection";
    
    // Initialize sync state file path
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    m_syncStateFile = QDir(appDataPath).filePath("database_sync_state.json");
    loadSyncState();
}

void SupabaseClient::markSystemAsEdited(const QString &systemName)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for markSystemAsEdited";
        return;
    }
    
    if (m_currentCommander.isEmpty() || m_currentCommander == "Unknown") {
        qDebug() << "No commander context for markSystemAsEdited";
        return;
    }
    
    qDebug() << "Marking system as edited:" << systemName << "by commander:" << m_currentCommander;
    
    // Update the edited flag in taken table
    QJsonObject updateData;
    updateData["edited"] = true;
    
    QString endpoint = QString("taken?system=eq.%1&by_cmdr=eq.%2")
                      .arg(QUrl::toPercentEncoding(systemName))
                      .arg(QUrl::toPercentEncoding(m_currentCommander));
    
    QNetworkRequest request = createRequest(endpoint);
    request.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
    
    QJsonDocument doc(updateData);
    QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "PATCH", doc.toJson());
    
    if (reply) {
        reply->setProperty("operation", "UPDATE:system_edited");
        reply->setProperty("systemName", systemName);
        qDebug() << "PATCH request sent to mark" << systemName << "as edited";
    } else {
        qDebug() << "Failed to create PATCH request for edited flag";
    }
}

void SupabaseClient::getCurrentCommanderSystems()
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getCurrentCommanderSystems";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (m_currentCommander.isEmpty() || m_currentCommander == "Unknown") {
        qDebug() << "No commander context for getCurrentCommanderSystems";
        // Emit empty array
        emit takenSystemsReceived(QJsonArray());
        return;
    }
    
    qDebug() << "Fetching claimed systems for current commander:" << m_currentCommander;
    
    // Filter by current commander using the by_cmdr column
    // IMPORTANT: Explicitly include the 'edited' column in the select
    QString endpoint = QString("taken?select=id,system,by_cmdr,visited,done,edited&by_cmdr=eq.%1&order=id.desc")
                      .arg(QUrl::toPercentEncoding(m_currentCommander));
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:current_commander_taken");
        qDebug() << "getCurrentCommanderSystems: Request sent for commander" << m_currentCommander << "with edited flag";
    } else {
        qDebug() << "getCurrentCommanderSystems: Failed to create network request!";
        emit networkError("Failed to create current commander systems request");
    }
}

void SupabaseClient::getSystemInformationFromCategory(const QString &systemName, const QString &category)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getSystemInformationFromCategory";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (category.isEmpty() || category == "Unknown") {
        // No category provided - emit minimal system info
        QJsonObject systemInfo;
        systemInfo["system"] = systemName;
        systemInfo["category"] = "Unknown";
        systemInfo["system_info"] = "No additional system information available";
        emit systemInformationReceived(systemName, systemInfo);
        return;
    }
    
    qDebug() << "Fetching system information from category table:" << category << "for system:" << systemName;
    
    // The Python version shows that Supabase accepts table names with spaces directly.
    // The issue is Qt's QUrl normalization. Let's build the request manually.
    
    // Encode components manually
    QString encodedCategory = QString::fromUtf8(QUrl::toPercentEncoding(category));
    QString encodedSystemName = QString::fromUtf8(QUrl::toPercentEncoding(systemName));
    
    // Build the full URL as a string
    // IMPORTANT: Different category tables use different column capitalizations!
    // Some use "System" (uppercase) and others use "system" (lowercase)
    
    // First, try with lowercase 'system' (most common)
    QString fullUrl = QString("%1/rest/v1/%2?select=*&system=eq.%3")
        .arg(m_supabaseUrl, encodedCategory, encodedSystemName);
    
    // Store both URL variations
    QString uppercaseUrl = QString("%1/rest/v1/%2?select=*&System=eq.%3")
        .arg(m_supabaseUrl, encodedCategory, encodedSystemName);
    
    // Create request without using QUrl constructor to avoid normalization
    QNetworkRequest request;
    
    // HACK: Set the URL using setRawHeader to bypass Qt's URL normalization
    // We'll construct the entire HTTP request manually
    request.setRawHeader("Host", "nxrvrnnaxxykwaugkxnw.supabase.co");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("apikey", m_supabaseKey.toUtf8());
    request.setRawHeader("Authorization", ("Bearer " + m_supabaseKey).toUtf8());
    request.setRawHeader("Prefer", "return=representation");
    
    // Use QUrl with the encoded string, but force it to not decode
    QUrl url = QUrl::fromEncoded(fullUrl.toUtf8(), QUrl::StrictMode);
    request.setUrl(url);
    
    qDebug() << "DEBUG: Encoded URL being sent:" << fullUrl;
    qDebug() << "DEBUG: QUrl toString:" << url.toString();
    qDebug() << "DEBUG: QUrl toEncoded:" << url.toEncoded();
    
    // Add commander header for RLS policies
    if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
        request.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
        qDebug() << "Added x-commander-name header:" << m_currentCommander;
    }
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    
    if (reply) {
        reply->setProperty("operation", "GET:system_information_category");
        reply->setProperty("systemName", systemName);
        reply->setProperty("category", category);
        reply->setProperty("fallbackUrl", uppercaseUrl);  // Store the uppercase URL as fallback
        reply->setProperty("triedLowercase", true);
        qDebug() << "getSystemInformationFromCategory: Request sent for" << systemName;
    } else {
        qDebug() << "getSystemInformationFromCategory: Failed to create network request!";
        emit networkError("Failed to create category system information request");
    }
}

QString SupabaseClient::formatCategoryTableData(const QJsonObject &categoryData, const QString &category) const
{
    if (categoryData.isEmpty()) {
        return "No additional system information available";
    }
    
    QStringList formattedInfo;
    
    // Add category as header
    formattedInfo << QString("Category: %1").arg(category);
    formattedInfo << ""; // Empty line
    
    // Process different types of data based on common column patterns
    QJsonObject::const_iterator it;
    for (it = categoryData.begin(); it != categoryData.end(); ++it) {
        QString key = it.key();
        QJsonValue value = it.value();
        
        // Only skip 'id' column as requested by user
        if (key.toLower() == "id") {
            continue;
        }
        
        // Handle null or empty values by showing them as "Not Available"
        if (value.isNull() || (value.isString() && value.toString().isEmpty())) {
            // Still show the field but indicate no data - use same formatting as below
            QString formattedKey = key;
            formattedKey = formattedKey.replace("_", " ");
            
            // Handle specific capitalization patterns
            formattedKey = formattedKey.replace(" km", " (km)").replace(" KM", " (km)")
                          .replace(" LS", " (LS)").replace(" AU", " (AU)")
                          .replace(" days", " (days)")
                          .replace("Bio Signals", "Bio Signals")
                          .replace("Outer Ring", "Outer Ring")
                          .replace("Planet Type", "Planet Type")
                          .replace("Moon Type", "Moon Type")
                          .replace("Axial Tilt", "Axial Tilt")
                          .replace("Orbital Period", "Orbital Period");
                          
            // Capitalize first letter of each word
            QStringList words = formattedKey.split(' ');
            for (int i = 0; i < words.size(); ++i) {
                if (!words[i].isEmpty()) {
                    words[i][0] = words[i][0].toUpper();
                }
            }
            formattedKey = words.join(' ');
            
            formattedInfo << QString("%1: Not Available").arg(formattedKey);
            continue;
        }
        
        // Format the key name to be more readable
        QString formattedKey = key;
        formattedKey = formattedKey.replace("_", " ");
        
        // Handle specific capitalization patterns
        formattedKey = formattedKey.replace(" km", " (km)").replace(" KM", " (km)")
                      .replace(" LS", " (LS)").replace(" AU", " (AU)")
                      .replace(" days", " (days)")
                      .replace("Bio Signals", "Bio Signals")
                      .replace("Outer Ring", "Outer Ring")
                      .replace("Planet Type", "Planet Type")
                      .replace("Moon Type", "Moon Type")
                      .replace("Axial Tilt", "Axial Tilt")
                      .replace("Orbital Period", "Orbital Period");
                      
        // Capitalize first letter of each word
        QStringList words = formattedKey.split(' ');
        for (int i = 0; i < words.size(); ++i) {
            if (!words[i].isEmpty()) {
                words[i][0] = words[i][0].toUpper();
            }
        }
        formattedKey = words.join(' ');
        
        // Handle different value types
        QString formattedValue;
        if (value.isDouble()) {
            double numValue = value.toDouble();
            // Format numbers appropriately based on field names
            if (key.contains("Distance", Qt::CaseInsensitive) && key.contains("LS", Qt::CaseInsensitive)) {
                formattedValue = QString("%1 LS").arg(numValue, 0, 'f', 2);
            } else if (key.contains("Distance", Qt::CaseInsensitive) && key.contains("AU", Qt::CaseInsensitive)) {
                formattedValue = QString("%1 AU").arg(numValue, 0, 'f', 6);
            } else if (key.contains("Distance", Qt::CaseInsensitive) && key.contains("km", Qt::CaseInsensitive)) {
                formattedValue = QString("%1 km").arg(numValue, 0, 'f', 2);
            } else if (key.contains("Radius", Qt::CaseInsensitive) && key.contains("km", Qt::CaseInsensitive)) {
                formattedValue = QString("%1 km").arg(numValue, 0, 'f', 0);
            } else if (key.contains("Inclination", Qt::CaseInsensitive) || key.contains("Axial_Tilt", Qt::CaseInsensitive)) {
                formattedValue = QString("%1°").arg(numValue, 0, 'f', 1);
            } else if (key.contains("Period", Qt::CaseInsensitive) && key.contains("days", Qt::CaseInsensitive)) {
                formattedValue = QString("%1 days").arg(numValue, 0, 'f', 2);
            } else if (key.contains("Eccentricity", Qt::CaseInsensitive)) {
                formattedValue = QString::number(numValue, 'f', 6);
            } else if (key.toLower() == "x" || key.toLower() == "y" || key.toLower() == "z") {
                // Format coordinates with proper precision
                formattedValue = QString::number(numValue, 'f', 2);
            } else if (key.contains("Ring", Qt::CaseInsensitive) && key.contains("km", Qt::CaseInsensitive)) {
                formattedValue = QString("%1 km").arg(numValue, 0, 'f', 0);
            } else if (key.contains("Outer", Qt::CaseInsensitive) && key.contains("Radius", Qt::CaseInsensitive)) {
                // Format OuterRadius in km with commas for readability
                if (numValue > 1000000) {
                    formattedValue = QString("%1 km").arg(numValue / 1000000.0, 0, 'f', 2) + "M"; // Convert to millions
                } else if (numValue > 1000) {
                    formattedValue = QString("%1 km").arg(numValue / 1000.0, 0, 'f', 1) + "K"; // Convert to thousands  
                } else {
                    formattedValue = QString("%1 km").arg(numValue, 0, 'f', 0);
                }
            } else if (key.contains("SMA", Qt::CaseInsensitive) || key.contains("SemiMajor", Qt::CaseInsensitive)) {
                // Format SemiMajorAxis in AU with proper precision
                formattedValue = QString("%1 AU").arg(numValue, 0, 'f', 6);
            } else if (key.contains("Inclination", Qt::CaseInsensitive)) {
                // Format Inclination in degrees
                formattedValue = QString("%1°").arg(numValue, 0, 'f', 2);
            } else {
                // General numeric formatting - NO MORE SCIENTIFIC NOTATION
                if (numValue > 1000000) {
                    // Format large numbers with readable units instead of scientific notation
                    if (numValue > 1000000000) {
                        formattedValue = QString("%1B").arg(numValue / 1000000000.0, 0, 'f', 1); // Billions
                    } else {
                        formattedValue = QString("%1M").arg(numValue / 1000000.0, 0, 'f', 1); // Millions
                    }
                } else if (numValue > 1000) {
                    formattedValue = QString("%1K").arg(numValue / 1000.0, 0, 'f', 1); // Thousands
                } else {
                    formattedValue = QString::number(numValue, 'f', 3);
                }
            }
        } else if (value.isString()) {
            formattedValue = value.toString();
        } else {
            formattedValue = QString::number(value.toInt());
        }
        
        formattedInfo << QString("%1: %2").arg(formattedKey, formattedValue);
    }
    
    return formattedInfo.join("\n");
}

void SupabaseClient::configure(const QString &url, const QString &key)
{
    m_supabaseUrl = url;
    m_supabaseKey = key;
    
    qDebug() << "SupabaseClient configured:";
    qDebug() << "  URL:" << m_supabaseUrl;
    qDebug() << "  Key configured:" << !m_supabaseKey.isEmpty();
}

QNetworkRequest SupabaseClient::createRequest(const QString &endpoint)
{
    QUrl url(m_supabaseUrl + "/rest/v1/" + endpoint);
    QNetworkRequest request(url);
    
    // Set required Supabase headers
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("apikey", m_supabaseKey.toUtf8());
    request.setRawHeader("Authorization", ("Bearer " + m_supabaseKey).toUtf8());
    request.setRawHeader("Prefer", "return=representation");
    // Improve connection robustness
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false); // avoid HTTP/2 flakiness
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    // Add commander header for RLS policies
    if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
        request.setRawHeader("x-commander-name", m_currentCommander.trimmed().toUtf8());
        qDebug() << "Added x-commander-name header:" << m_currentCommander;
    }
    
    return request;
}

void SupabaseClient::makeRequest(const QString &method, const QString &endpoint, const QJsonObject &data)
{
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = nullptr;
    
    if (method == "GET") {
        reply = m_networkManager->get(request);
    } else if (method == "POST") {
        QJsonDocument doc(data);
        reply = m_networkManager->post(request, doc.toJson());
    } else if (method == "PATCH") {
        QJsonDocument doc(data);
        reply = m_networkManager->put(request, doc.toJson());
    } else if (method == "DELETE") {
        reply = m_networkManager->deleteResource(request);
    }
    
    if (reply) {
        // Store operation type for reply handling
        reply->setProperty("operation", method + ":" + endpoint);
    }
}

void SupabaseClient::setCommanderContext(const QString &commanderName)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for setCommanderContext";
        return;
    }
 
    if (commanderName.isEmpty() || commanderName == "Unknown") {
        qDebug() << "Cannot set commander context with empty or unknown commander name";
        return;
    }
    
    // Store the commander name for use in subsequent requests
    m_currentCommander = commanderName;
    qDebug() << "Commander context stored locally:" << commanderName;
    
    // DISABLE RPC call until we fix the database security properly
    qDebug() << "RPC call disabled - will implement proper security later";
}

void SupabaseClient::resetAuthFailures()
{
    qDebug() << "Resetting authentication failure state. Previous failures:" << m_consecutiveAuthFailures;
    m_consecutiveAuthFailures = 0;
    m_lastAuthFailureTime = 0;
    qDebug() << "Authentication failure state reset. Ready to retry requests.";
}

void SupabaseClient::getSystems()
{
    // Debug logging reduced for performance - only log issues
    
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "❌ Supabase not configured for getSystems";
        emit networkError("Supabase not configured");
        return;
    }
    
    // Check if we should skip due to recent authentication failures
    if (shouldSkipRequestDueToAuthFailure()) {
        qDebug() << "❌ Skipping getSystems() due to authentication failure cooldown";
        return;
    }
    
    qDebug() << "Fetching systems from systems table...";
    
    // Query the real table: systems (same as getSystemsNear)
    // Order by category first, then by system name for consistent display  
    QNetworkRequest request = createRequest("systems?select=systems,category,x,y,z&order=category.asc,systems.asc");
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:systems");
        qDebug() << "✅ getSystems: Request sent successfully, operation tagged as GET:systems";
        qDebug() << "Request URL:" << reply->request().url().toString();
    } else {
        qDebug() << "❌ getSystems: Failed to create network request!";
    }
}

void SupabaseClient::getTakenSystems()
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getTakenSystems";
        emit networkError("Supabase not configured");
        return;
    }
    
    qDebug() << "Fetching ALL claimed systems from taken table...";
    
    // Fetch ALL taken systems so UI can determine claim status correctly
    QString endpoint = "taken?select=*&order=id.desc";
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:taken");
        qDebug() << "getTakenSystems: Request sent for ALL taken systems";
    } else {
        qDebug() << "getTakenSystems: Failed to create network request!";
        emit networkError("Failed to create taken systems request");
    }
}



void SupabaseClient::getSystemDetails(const QString &systemName, const QString &category)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getSystemDetails";
        emit networkError("Supabase not configured");
        return;
    }
    
    qDebug() << "Fetching system details for" << systemName << "from category table:" << category;
    
    // Query the specific category table for detailed system data
    // Use double quotes to handle table names with spaces
    QString encodedTableName = QString("\"%1\"").arg(category);
    QString endpoint = QString("%1?select=*&System=eq.%2").arg(encodedTableName, systemName);
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:system_details");
        reply->setProperty("systemName", systemName);
        reply->setProperty("category", category);
        qDebug() << "getSystemDetails: Request sent for system" << systemName;
    } else {
        qDebug() << "getSystemDetails: Failed to create network request!";
        emit networkError("Failed to create system details request");
    }
}

void SupabaseClient::getSystemDetailsRobust(const QString &systemName)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getSystemDetailsRobust";
        emit networkError("Supabase not configured");
        return;
    }
    
    qDebug() << "Fetching category for system" << systemName << "from systems table";
    
    // First, find the category for this system
    QString endpoint = QString("systems?select=category,x,y,z&systems=eq.%1").arg(systemName);
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:system_category_lookup");
        reply->setProperty("systemName", systemName);
        qDebug() << "getSystemDetailsRobust: Looking up category for system" << systemName;
    } else {
        qDebug() << "getSystemDetailsRobust: Failed to create category lookup request!";
        emit networkError("Failed to create category lookup request");
    }
}

void SupabaseClient::getSystemDetailsWithCapitalizationHandling(const QString &systemName, const QString &category)
{
    qDebug() << "Trying to get system details for" << systemName << "in category" << category;
    // Start with capital "System" (most common)
    getSystemDetailsWithFieldName(systemName, category, "System");
}

void SupabaseClient::getSystemDetailsWithFieldName(const QString &systemName, const QString &category, const QString &fieldName)
{
    qDebug() << "Querying category table" << category << "for system" << systemName << "using field" << fieldName;
    
    // Query the specific category table for detailed system data with specified field name
    // Use double quotes to handle table names with spaces
    QString encodedTableName = QString("\"%1\"").arg(category);
    QString endpoint = QString("%1?select=*&%2=eq.%3").arg(encodedTableName, fieldName, systemName);
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:system_details_robust");
        reply->setProperty("systemName", systemName);
        reply->setProperty("category", category);
        reply->setProperty("fieldName", fieldName);
        qDebug() << "getSystemDetailsWithFieldName: Request sent for system" << systemName << "using field" << fieldName;
    } else {
        qDebug() << "getSystemDetailsWithFieldName: Failed to create network request!";
        emit networkError("Failed to create system details request");
    }
}

void SupabaseClient::getPOISystems(const QString &commanderName)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getPOISystems";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (shouldSkipRequestDueToAuthFailure()) {
        return;
    }
    
    // Set commander context for RLS policies before accessing POI data
    if (!commanderName.isEmpty() && commanderName != "Unknown") {
        setCommanderContext(commanderName);
    }
    
    qDebug() << "Fetching POI systems from system_information table...";
    
    // Query systems with POI status from system_information table
    QString endpoint = "system_information?select=system,potential_or_poi,discoverer,submitter,name&order=system.asc";
    
    QNetworkRequest request = createRequest(endpoint);
    
    // Keep headers for future write operations, but they're not needed for reading
    if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
        request.setRawHeader("X-Commander", m_currentCommander.toUtf8());
        qDebug() << "Added commander header for future write operations:" << m_currentCommander;
    }
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:pois");
        qDebug() << "getPOISystems: Request sent, operation tagged as GET:pois";
    } else {
        qDebug() << "getPOISystems: Failed to create network request!";
        emit networkError("Failed to create POI request");
    }
}

void SupabaseClient::getPresetImages(bool includeRichard)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getPresetImages";
        emit networkError("Supabase not configured");
        return;
    }
    
    qDebug() << "Fetching preset images from Supabase, includeRichard:" << includeRichard;
    
    QString endpoint = "preset_images?select=*";
    if (!includeRichard) {
        // Filter out Richard images unless explicitly requested
        // Richard=true means hidden in special subsection, Richard=false/null means visible
        endpoint += "&or=(Richard.is.null,Richard.eq.false)";
    }
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:preset_images");
        qDebug() << "getPresetImages: Request sent, operation tagged as GET:preset_images";
    } else {
        qDebug() << "getPresetImages: Failed to create network request!";
        emit networkError("Failed to create preset images request");
    }
}

void SupabaseClient::getCategories()
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getCategories";
        emit networkError("Supabase not configured");
        return;
    }
    
    // Check if we should skip due to recent authentication failures
    if (shouldSkipRequestDueToAuthFailure()) {
        // Skip silently during cooldown period
        return;
    }
    
    qDebug() << "Fetching categories from systems table...";
    
    // Get distinct categories from systems table, ordered alphabetically
    QNetworkRequest request = createRequest("systems?select=category&order=category.asc");
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:categories_systems");
        qDebug() << "getCategories: Request sent for systems categories, operation tagged as GET:categories_systems";
    } else {
        qDebug() << "getCategories: Failed to create network request!";
        emit networkError("Failed to create categories request");
    }
}

void SupabaseClient::getRichardCategories()
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getRichardCategories";
        emit networkError("Supabase not configured");
        return;
    }
    
    qDebug() << "Fetching Richard categories from Supabase preset_images table...";
    
    // Get Richard categories from preset_images table where Richard=true
    QNetworkRequest request = createRequest("preset_images?select=category&Richard=eq.true");
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:categories_richard");
        qDebug() << "getRichardCategories: Request sent, operation tagged as GET:categories_richard";
    } else {
        qDebug() << "getRichardCategories: Failed to create network request!";
        emit networkError("Failed to create Richard categories request");
    }
}

void SupabaseClient::getSystemsNear(double x, double y, double z, int limit)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getSystemsNear";
        emit networkError("Supabase not configured");
        return;
    }
    
    // Check if we should skip due to recent authentication failures
    if (shouldSkipRequestDueToAuthFailure()) {
        // Skip silently during cooldown period
        return;
    }
    
    qDebug() << "Fetching systems near coordinates:" << x << y << z << "with limit:" << limit;
    
    // Use PostgREST/Supabase to find systems within a reasonable range
    // We'll fetch systems and calculate distances client-side for now
    // In production, you'd use a proper spatial query
    QString endpoint = QString("systems?select=systems,category,x,y,z&limit=%1&order=systems.asc").arg(limit);
    
    QNetworkRequest request = createRequest(endpoint);
    
    // Store the reference coordinates for distance calculation in reply processing
    request.setRawHeader("X-Reference-X", QString::number(x, 'f', 6).toUtf8());
    request.setRawHeader("X-Reference-Y", QString::number(y, 'f', 6).toUtf8());
    request.setRawHeader("X-Reference-Z", QString::number(z, 'f', 6).toUtf8());
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:systems_near");
        reply->setProperty("refX", x);
        reply->setProperty("refY", y);
        reply->setProperty("refZ", z);
        // Reduce logging frequency for this common operation
        if (m_consecutiveAuthFailures == 0) {
            qDebug() << "getSystemsNear: Request sent, operation tagged as GET:systems_near";
        }
    } else {
        qDebug() << "getSystemsNear: Failed to create network request!";
        emit networkError("Failed to create systems near request");
    }
}

void SupabaseClient::getSystemInformation(const QString &systemName, const QString &category)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getSystemInformation";
        emit networkError("Supabase not configured");
        return;
    }
    
    qDebug() << "Getting system information for" << systemName << "category:" << category;
    
    // First try to get custom system information from system_information table
    QString endpoint = QString("system_information?system=eq.%1&select=*")
                      .arg(QUrl::toPercentEncoding(systemName));
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:system_information_primary");
        reply->setProperty("systemName", systemName);
        reply->setProperty("fallbackCategory", category);
        qDebug() << "getSystemInformation: Primary request sent for" << systemName;
    } else {
        qDebug() << "getSystemInformation: Failed to create network request!";
        emit networkError("Failed to create system information request");
    }
}

void SupabaseClient::getSystemInformationFromDB(const QString &systemName)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getSystemInformationFromDB";
        emit networkError("Supabase not configured");
        return;
    }
    
    qDebug() << "Getting system information from database for" << systemName;
    qDebug() << "=== POI DATABASE QUERY DEBUG ===";
    qDebug() << "Including POI fields: discoverer, submitter, potential_or_poi";
    
    // Query system_information table with all fields including POI data
    // CRITICAL: Explicitly include POI fields in SELECT for debugging
    QString endpoint = QString("system_information?system=eq.%1&select=id,system,description,x,y,z,images,name,additional_images,system_info,category,image_title,additional_image_titles,discoverer,submitter,potential_or_poi")
                      .arg(QUrl::toPercentEncoding(systemName));
    
    qDebug() << "POI Query endpoint:" << endpoint;
    qDebug() << "POI fields included: potential_or_poi, discoverer, submitter";
    
    QNetworkRequest request = createRequest(endpoint);
    
    // CRITICAL FIX: Remove commander headers for public access to POI data
    // POI information should be visible to all users, not just system owners
    qDebug() << "POI Query: No commander headers added for public POI access";
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:system_information_db");
        reply->setProperty("systemName", systemName);
        qDebug() << "getSystemInformationFromDB: Request sent for" << systemName << "with POI fields";
        qDebug() << "Full endpoint:" << endpoint;
        qDebug() << "=== END POI DATABASE QUERY DEBUG ===";
    } else {
        qDebug() << "getSystemInformationFromDB: Failed to create network request!";
        qDebug() << "=== END POI DATABASE QUERY DEBUG ===";
        emit networkError("Failed to create system information DB request");
    }
}

void SupabaseClient::claimSystem(const QString &systemName, const QString &commander)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for claimSystem";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (systemName.isEmpty() || commander.isEmpty()) {
        emit systemClaimed(systemName, false);
        return;
    }
    
    // Ensure commander context is set
    if (m_currentCommander != commander) {
        setCommanderContext(commander);
    }
    
    qDebug() << "Claiming system" << systemName << "for commander" << commander;
    
    // Check if commander has visited this system in journal logs
    bool hasVisited = checkSystemVisitedInJournal(systemName, commander);
    qDebug() << "System" << systemName << "visited status:" << (hasVisited ? "YES" : "NO");
    
    // FIRST: Check if ANY record exists for this system (to prevent stealing claims)
    QString checkEndpoint = QString("taken?system=eq.%1")
                           .arg(QUrl::toPercentEncoding(systemName));
    
    QNetworkRequest checkRequest = createRequest(checkEndpoint);
    checkRequest.setRawHeader("X-Commander", commander.toUtf8());
    checkRequest.setRawHeader("x-commander-name", commander.toUtf8());
    
    QNetworkReply *checkReply = m_networkManager->get(checkRequest);
    
    if (checkReply) {
        checkReply->setProperty("operation", "CHECK:existing_claim");
        checkReply->setProperty("systemName", systemName);
        checkReply->setProperty("commander", commander);
        checkReply->setProperty("hasVisited", hasVisited);
        qDebug() << "Checking if system" << systemName << "is already claimed by anyone";
    }
}

void SupabaseClient::unclaimSystem(const QString &systemName)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for unclaimSystem";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (m_currentCommander.isEmpty() || m_currentCommander == "Unknown") {
        qDebug() << "No commander context for unclaim";
        emit systemUnclaimed(systemName, false);
        return;
    }
    
    QString commanderTrimmed = m_currentCommander.trimmed();
    qDebug() << "Releasing claim for" << systemName << "commander" << commanderTrimmed;

    // Rely on RLS to restrict UPDATE to the current commander's row; do not rely on by_cmdr filter in URL
    QString patchEndpoint = QString("taken?system=eq.%1")
                            .arg(QUrl::toPercentEncoding(systemName));
    QNetworkRequest patchReq = createRequest(patchEndpoint);
    patchReq.setRawHeader("Prefer", "return=representation");
    patchReq.setRawHeader("X-Commander", commanderTrimmed.toUtf8());
    patchReq.setRawHeader("x-commander-name", commanderTrimmed.toUtf8());
    QJsonObject body; body["by_cmdr"] = QStringLiteral("empty");
    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    qDebug() << "unclaimSystem PATCH endpoint:" << patchEndpoint;
    qDebug() << "unclaimSystem headers x-commander-name:" << commanderTrimmed;
    qDebug() << "unclaimSystem payload:" << QString::fromUtf8(payload);
    QNetworkReply *patchReply = m_networkManager->sendCustomRequest(patchReq, "PATCH", payload);
    if (patchReply) {
        patchReply->setProperty("operation", "UNCLAIM:mark_empty");
        patchReply->setProperty("systemName", systemName);
        qDebug() << "unclaimSystem: PATCH mark empty sent for" << systemName;
    } else {
        qDebug() << "unclaimSystem: Failed to create PATCH mark empty request!";
        emit systemUnclaimed(systemName, false);
    }
}

void SupabaseClient::markSystemVisited(const QString &systemName, const QString &commander)
{
    qDebug() << "Marking system visited:" << systemName << "by commander:" << commander;
    
    QJsonObject data;
    data["system"] = systemName;
    data["commander"] = commander;
    data["visited_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    makeRequest("POST", "visited_systems", data);
}

void SupabaseClient::updateSystemStatus(const QString &systemName, bool visited, bool done)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for updateSystemStatus";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (m_currentCommander.isEmpty() || m_currentCommander == "Unknown") {
        qDebug() << "No commander context for updateSystemStatus";
        emit systemStatusUpdated(systemName, false);
        return;
    }
    
    qDebug() << "Updating system status:" << systemName << "visited:" << visited << "done:" << done;
    
    // Simple update - just change the values
    QJsonObject updateData;
    updateData["visited"] = visited;
    updateData["done"] = done;
    
    // Use correct column names: system=eq. and by_cmdr=eq.
    QString endpoint = QString("taken?system=eq.%1&by_cmdr=eq.%2")
                      .arg(QUrl::toPercentEncoding(systemName))
                      .arg(QUrl::toPercentEncoding(m_currentCommander));
    
    qDebug() << "updateSystemStatus: Full endpoint:" << endpoint;
    qDebug() << "updateSystemStatus: Commander:" << m_currentCommander;
    qDebug() << "updateSystemStatus: Update data:" << QJsonDocument(updateData).toJson(QJsonDocument::Compact);
    
    QNetworkRequest request = createRequest(endpoint);
    request.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
    
    QJsonDocument doc(updateData);
    QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "PATCH", doc.toJson());
    
    if (reply) {
        reply->setProperty("operation", "UPDATE:system_status");
        reply->setProperty("systemName", systemName);
        reply->setProperty("visited", visited);
        reply->setProperty("done", done);
        qDebug() << "PATCH request sent to update" << systemName;
    } else {
        qDebug() << "Failed to create update request";
        emit systemStatusUpdated(systemName, false);
    }
}

void SupabaseClient::updateSystemVisited(const QString &systemName, bool visited)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for updateSystemVisited";
        return;
    }
    
    if (m_currentCommander.isEmpty() || m_currentCommander == "Unknown") {
        qDebug() << "No commander context for updateSystemVisited";
        return;
    }
    
    qDebug() << "=== UPDATE VISITED DEBUG ===";
    qDebug() << "System:" << systemName;
    qDebug() << "Visited:" << visited;
    qDebug() << "Commander:" << m_currentCommander;
    
    // Only update the visited field
    QJsonObject updateData;
    updateData["visited"] = visited;
    
    // FIXED: Use both system and commander filters to ensure RLS policy works
    QString endpoint = QString("taken?system=eq.%1&by_cmdr=eq.%2")
                      .arg(QUrl::toPercentEncoding(systemName))
                      .arg(QUrl::toPercentEncoding(m_currentCommander));
    
    qDebug() << "updateSystemVisited: Full endpoint:" << endpoint;
    qDebug() << "updateSystemVisited: Commander:" << m_currentCommander;
    qDebug() << "Update data:" << QJsonDocument(updateData).toJson(QJsonDocument::Compact);
    
    QNetworkRequest request = createRequest(endpoint);
    request.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
    
    // Try additional header formats that might work better with Supabase RLS
    request.setRawHeader("X-Commander-Name", m_currentCommander.toUtf8());
    request.setRawHeader("commander-name", m_currentCommander.toUtf8());
    
    // Add debug headers
    qDebug() << "Request headers:";
    const auto headers = request.rawHeaderList();
    for (const auto &header : headers) {
        qDebug() << "  " << header << ":" << request.rawHeader(header);
    }
    
    QJsonDocument doc(updateData);
    QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "PATCH", doc.toJson());
    
    if (reply) {
        reply->setProperty("operation", "UPDATE:system_visited");
        reply->setProperty("systemName", systemName);
        reply->setProperty("visited", visited);
        qDebug() << "PATCH request sent to update visited status for" << systemName;
    } else {
        qDebug() << "Failed to create PATCH request for visited update";
        emit systemStatusUpdated(systemName, false);
    }
    qDebug() << "=== END UPDATE VISITED DEBUG ===";
}

void SupabaseClient::updateSystemDone(const QString &systemName, bool done)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for updateSystemDone";
        return;
    }
    
    if (m_currentCommander.isEmpty() || m_currentCommander == "Unknown") {
        qDebug() << "No commander context for updateSystemDone";
        return;
    }
    
    qDebug() << "Updating done status:" << systemName << "done:" << done;
    
    // Only update the done field
    QJsonObject updateData;
    updateData["done"] = done;
    
    QString endpoint = QString("taken?system=eq.%1&by_cmdr=eq.%2")
                      .arg(QUrl::toPercentEncoding(systemName))
                      .arg(QUrl::toPercentEncoding(m_currentCommander));
    
    QNetworkRequest request = createRequest(endpoint);
    request.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
    
    QJsonDocument doc(updateData);
    QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "PATCH", doc.toJson());
    
    if (reply) {
        reply->setProperty("operation", "UPDATE:system_done");
        reply->setProperty("systemName", systemName);
        reply->setProperty("done", done);
        qDebug() << "PATCH request sent to update done status for" << systemName;
    } else {
        qDebug() << "Failed to create PATCH request for done update";
        emit systemStatusUpdated(systemName, false);
    }
}

// Removed duplicate implementation - keeping the correct one that uses 'systems' and 'claimed_by' fields

void SupabaseClient::checkAdminStatus(const QString &commander)
{
    qDebug() << "Checking admin status for commander:" << commander;
    makeRequest("GET", "admin_access?select=id&commander=eq." + commander);
}

void SupabaseClient::testAdminAccess(const QString &serviceKey)
{
    if (serviceKey.isEmpty()) {
        emit adminAccessTestComplete(false);
        return;
    }
    
    qDebug() << "Testing admin access with service key...";
    
    // Create a request with the service key to test access to admin_access table
    QString endpoint = "admin_access?select=id&limit=1";
    QUrl url(m_supabaseUrl + "/rest/v1/" + endpoint);
    QNetworkRequest request(url);
    
    // Use service key instead of regular key for this test
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("apikey", serviceKey.toUtf8());
    request.setRawHeader("Authorization", ("Bearer " + serviceKey).toUtf8());
    request.setRawHeader("Prefer", "return=representation");
    
    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("operation", "TEST:admin_access");
    
    qDebug() << "testAdminAccess: Request sent with service key";
}

void SupabaseClient::getWebhookConfig()
{
    if (!isConfigured()) {
        qWarning() << "SupabaseClient not configured for getWebhookConfig";
        return;
    }
    
    qDebug() << "Loading webhook configuration from app_config table...";
    
    // Get Discord webhook URL from secure app_config table
    QString endpoint = "app_config?select=config_value&config_key=eq.discord_webhook_url";
    QNetworkRequest request = createRequest(endpoint);
    
    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("operation", "GET:webhook_config");
    
    qDebug() << "getWebhookConfig: Request sent";
}

void SupabaseClient::triggerWebhook(const QString &eventType, const QVariantMap &data)
{
    if (!m_webhookConfigLoaded || m_webhookUrl.isEmpty()) {
        qDebug() << "Webhook not configured, loading config first...";
        // Connect to config received signal and retry
        connect(this, &SupabaseClient::webhookConfigReceived, this, [this, eventType, data]() {
            disconnect(this, &SupabaseClient::webhookConfigReceived, this, nullptr);
            triggerWebhook(eventType, data);
        });
        getWebhookConfig();
        return;
    }
    
    qDebug() << "Triggering webhook for event:" << eventType;
    qDebug() << "Webhook data:" << QJsonDocument::fromVariant(QVariantMap(data)).toJson(QJsonDocument::Compact);
    
    // Prepare Discord webhook payload
    QJsonObject webhookPayload;
    QJsonObject embed;
    
    // Configure embed based on event type
    if (eventType == "poi_upload") {
        embed["title"] = "📍 POI Upload";
        embed["color"] = 16776960; // Gold
        embed["description"] = QString("**%1** marked **%2** as **%3**")
                              .arg(data.value("commander").toString())
                              .arg(data.value("system").toString())
                              .arg(data.value("poi_type").toString());
                              
    } else if (eventType == "admin_action") {
        embed["title"] = "⚡ Admin Action";
        embed["color"] = 16711680; // Red
        embed["description"] = QString("Admin **%1** performed: **%2**")
                              .arg(data.value("admin").toString())
                              .arg(data.value("action").toString());
                              
    } else if (eventType == "program_login") {
        embed["title"] = "🟢 Program Login";
        embed["color"] = 65280; // Green
        embed["description"] = QString("**%1** logged into EDRH")
                              .arg(data.value("commander").toString());
    } else {
        // Generic event
        embed["title"] = QString("📡 EDRH Event: %1").arg(eventType);
        embed["color"] = 3447003;
        embed["description"] = data.value("message", "Event triggered").toString();
    }
    
    // Add timestamp in proper Discord format (ISO 8601 with milliseconds)
    embed["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    embed["footer"] = QJsonObject{{"text", "EDRH - Elite Dangerous Records Helper"}};
    
    webhookPayload["embeds"] = QJsonArray{embed};
    
    // Send webhook request
    QNetworkRequest webhookRequest{QUrl(m_webhookUrl)};
    webhookRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(webhookPayload);
    qDebug() << "Final webhook payload:" << doc.toJson(QJsonDocument::Compact);
    
    QNetworkReply *reply = m_networkManager->post(webhookRequest, doc.toJson());
    reply->setProperty("operation", "POST:webhook:" + eventType);
    
    qDebug() << "Webhook request sent for event:" << eventType;
}

void SupabaseClient::getAllCommanderLocations()
{
    if (!isConfigured()) {
        qWarning() << "SupabaseClient not configured for getAllCommanderLocations";
        return;
    }
    
    qDebug() << "Fetching all commander locations from commanders table...";
    
    // Get all commanders (note: current schema doesn't have location fields yet)
    // For now, return basic commander info - location tracking would need schema update
    QString endpoint = "commanders?select=name,blocked,first_seen";
    QNetworkRequest request = createRequest(endpoint);
    
    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("operation", "GET:all_commanders");
    
    qDebug() << "getAllCommanderLocations: Request sent, operation tagged as GET:all_commanders";
}

void SupabaseClient::updateCommanderLocation(const QString &commanderName, double x, double y, double z, const QString &systemName)
{
    if (!isConfigured()) {
        qWarning() << "SupabaseClient not configured for updateCommanderLocation";
        return;
    }
    
    if (commanderName.isEmpty()) {
        qWarning() << "Cannot update commander location: commander name is empty";
        return;
    }
    
    qDebug() << "Commander location update requested for:" << commanderName << "at" << x << y << z << "in system:" << systemName;
    
    // NOTE: Current database schema doesn't have location fields (current_x, current_y, current_z)
    // For now, just log the location data locally
    // TODO: Add location fields to commanders table or create separate commander_locations table
    
    qDebug() << "Location tracking not yet implemented in database schema. Commander position logged locally only.";
    
    // For now, emit success to prevent errors in UI
    emit commanderLocationUpdated(commanderName, true);
}

double SupabaseClient::calculateDistance(double x1, double y1, double z1, double x2, double y2, double z2)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    double dz = z2 - z1;
    return qSqrt(dx*dx + dy*dy + dz*dz);
}

bool SupabaseClient::shouldSkipRequestDueToAuthFailure()
{
    if (m_consecutiveAuthFailures >= MAX_AUTH_FAILURES_TO_LOG) {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        qint64 timeSinceLastFailure = currentTime - m_lastAuthFailureTime;
        
        if (timeSinceLastFailure < AUTH_FAILURE_COOLDOWN_MS) {
            // Still in cooldown period, skip request silently
            return true;
        } else {
            // Cooldown expired, reset counters
            m_consecutiveAuthFailures = 0;
            return false;
        }
    }
    
    return false;
}

bool SupabaseClient::isInAuthFailureCooldown() const
{
    if (m_consecutiveAuthFailures >= MAX_AUTH_FAILURES_TO_LOG) {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        qint64 timeSinceLastFailure = currentTime - m_lastAuthFailureTime;
        return timeSinceLastFailure < AUTH_FAILURE_COOLDOWN_MS;
    }
    return false;
}

void SupabaseClient::mergePOIDataIntoSystems(QJsonArray &systemsArray)
{
    // CRITICAL FIX: Synchronously add POI data to systems for public visibility
    qDebug() << "*** MERGE POI FUNCTION CALLED ***";
    qDebug() << "mergePOIDataIntoSystems: Processing" << systemsArray.size() << "systems";
    
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for POI merging";
        return;
    }
    
    // Build a list of system names to query
    QStringList systemNames;
    for (int i = 0; i < systemsArray.size(); i++) {
        QJsonObject system = systemsArray[i].toObject();
        QString systemName = system["name"].toString();
        if (!systemName.isEmpty()) {
            systemNames.append(systemName);
        }
    }
    
    if (systemNames.isEmpty()) {
        qDebug() << "No systems to query for POI data";
        return;
    }
    
    qDebug() << "Querying POI data for" << systemNames.size() << "systems";
    
    // Build a query to get POI data for the specific systems
    QStringList systemQueries;
    for (const QString &systemName : systemNames) {
        systemQueries.append(QString("system.eq.%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(systemName))));
    }
    
    QString systemFilter = systemQueries.join(",");
    QString endpoint = QString("system_information?select=system,potential_or_poi,discoverer,submitter&or=(%1)").arg(systemFilter);
    
    QNetworkRequest request = createRequest(endpoint);
    
    // IMPORTANT: Remove commander headers for public POI access
    request.setRawHeader("x-commander-name", "");
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:poi_data_for_merge");
        reply->setProperty("systemsCount", systemsArray.size());
        // Store the systems array so we can merge when the response comes back
        QJsonDocument systemsDoc(systemsArray);
        reply->setProperty("systemsToMerge", systemsDoc.toJson());
        qDebug() << "mergePOIDataIntoSystems: POI data request sent for" << systemNames.size() << "systems";
    } else {
        qDebug() << "mergePOIDataIntoSystems: Failed to create POI request";
    }
}

void SupabaseClient::handleNetworkReply(QNetworkReply *reply)
{
    if (!reply) {
        qDebug() << "handleNetworkReply: No reply object passed as parameter!";
        return;
    }
    
    QString operation = reply->property("operation").toString();
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    // Add specific debug for ImgBB operations
    if (operation.startsWith("IMGBB:")) {
        qDebug() << "=== IMGBB REPLY RECEIVED ===";
        qDebug() << "Operation:" << operation;
        qDebug() << "HTTP Status:" << httpStatus;
        qDebug() << "Reply URL:" << reply->url().toString();
        qDebug() << "Reply error:" << reply->error();
        qDebug() << "Reply error string:" << reply->errorString();
        qDebug() << "Reply finished:" << reply->isFinished();
        qDebug() << "Reply data size:" << reply->bytesAvailable();
        qDebug() << "=== END IMGBB REPLY RECEIVED ===";
    }
    
    // Only log details for successful requests or first few failures
    if (reply->error() == QNetworkReply::NoError || m_consecutiveAuthFailures <= MAX_AUTH_FAILURES_TO_LOG) {
        qDebug() << "handleNetworkReply: Processing operation:" << operation << "HTTP Status:" << httpStatus;
    }
    
    bool success = false;
    QJsonObject response;
    
    // Special handling for webhook operations - they return HTTP 204 with no body on success
    if (operation.startsWith("POST:webhook:")) {
        if (reply->error() == QNetworkReply::NoError && 
            (httpStatus == 200 || httpStatus == 204)) {
            success = true;
            response["message"] = "Webhook sent successfully";
        } else {
            response = parseReply(reply, success);
        }
    } else if (operation.startsWith("IMGBB:upload")) {
        // ImgBB returns 200 with JSON response on success
        if (reply->error() == QNetworkReply::NoError && httpStatus == 200) {
            success = true;
            // Don't parse here, let the IMGBB handler below read the raw response
        }
    } else {
        response = parseReply(reply, success);
    }
    
    // Ensure reply is cleaned up at the end
    reply->deleteLater();
    
    if (!success) {
        QString error = response.value("message").toString("Network error");
        
        // Check if this is an authentication error
        bool isAuthError = error.contains("authentication", Qt::CaseInsensitive) || 
                          error.contains("unauthorized", Qt::CaseInsensitive) ||
                          error.contains("Host requires authentication", Qt::CaseInsensitive) ||
                          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401;
        
        if (isAuthError) {
            // Special handling for admin access test - this is expected to fail for non-admins
            if (operation.startsWith("TEST:admin_access")) {
                qDebug() << "Admin access test failed - user does not have admin privileges";
                emit adminAccessTestComplete(false);
                return; // Don't count this as a regular auth failure
            }
            
            m_consecutiveAuthFailures++;
            m_lastAuthFailureTime = QDateTime::currentMSecsSinceEpoch();
            
            // Only log the first few authentication failures to avoid spam
            if (m_consecutiveAuthFailures <= MAX_AUTH_FAILURES_TO_LOG) {
                qWarning() << "Supabase authentication failed for operation:" << operation << "-" << error;
                if (m_consecutiveAuthFailures == MAX_AUTH_FAILURES_TO_LOG) {
                    qWarning() << "Authentication failed" << MAX_AUTH_FAILURES_TO_LOG << "times. Entering cooldown mode for" << (AUTH_FAILURE_COOLDOWN_MS/1000) << "seconds.";
                }
                emit networkError(error);
            }
            // After MAX_AUTH_FAILURES_TO_LOG, we stop logging and emitting signals to reduce spam
        } else {
            // Reset auth failure counter for non-auth errors
            m_consecutiveAuthFailures = 0;
            
            // Check if this is a POI-related error (non-critical for core app functionality)
            bool isPOIError = operation.startsWith("GET:pois") || operation.startsWith("POST:pois") || operation.startsWith("DELETE:pois");
            bool isWebhookError = operation.startsWith("POST:webhook:");
            bool isUpdateError = operation.startsWith("UPDATE:system");
            
            if (isUpdateError) {
                // Handle system status update failures
                QString systemName = reply->property("systemName").toString();
                qDebug() << "System status update failed for" << systemName << "-" << error;
                emit systemStatusUpdated(systemName, false);
                // Don't show popup for update failures, let the UI handle it
            } else if (isPOIError) {
                // POI errors are non-critical - log but don't show scary popup
                qDebug() << "POI operation failed (non-critical):" << operation << "-" << error;
                qDebug() << "App will continue to work normally without POI functionality";
                
                // Emit empty POI data so app continues to work
                if (operation.startsWith("GET:pois")) {
                    emit poisReceived(QJsonArray());
                }
                // Don't emit networkError for POI operations to avoid popup
            } else if (isWebhookError) {
                // Webhook errors are non-critical but should be logged
                QString eventType = operation.split(":").last();
                qDebug() << "Webhook failed for event:" << eventType << "-" << error;
                qDebug() << "Webhook HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                qDebug() << "Webhook response body:" << reply->readAll();
                emit webhookTriggered(false, eventType);
                // Don't emit networkError for webhook operations
            } else {
                // Special handling for category table queries - check for column name issues BEFORE showing error
                if (operation == "GET:system_information_category" && httpStatus == 400) {
                    QByteArray responseBody = reply->readAll();
                    qDebug() << "Category table 400 error - checking if it's a column name issue...";
                    qDebug() << "Response:" << QString::fromUtf8(responseBody);
                    
                    // Check if this is a column name capitalization issue
                    if (responseBody.contains("column") && 
                        (responseBody.contains("system") || responseBody.contains("System"))) {
                        
                        bool triedLowercase = reply->property("triedLowercase").toBool();
                        QString fallbackUrl = reply->property("fallbackUrl").toString();
                        
                        if (triedLowercase && !fallbackUrl.isEmpty()) {
                            qDebug() << "Column name issue detected - retrying with uppercase 'System'";
                            
                            // Extract the original properties
                            QString systemName = reply->property("systemName").toString();
                            QString category = reply->property("category").toString();
                            
                            // Create a new request with uppercase System
                            QUrl url = QUrl::fromEncoded(fallbackUrl.toUtf8(), QUrl::StrictMode);
                            QNetworkRequest request(url);
                            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                            request.setRawHeader("apikey", m_supabaseKey.toUtf8());
                            request.setRawHeader("Authorization", ("Bearer " + m_supabaseKey).toUtf8());
                            request.setRawHeader("Prefer", "return=representation");
                            
                            if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
                                request.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
                            }
                            
                            QNetworkReply *retryReply = m_networkManager->get(request);
                            if (retryReply) {
                                retryReply->setProperty("operation", "GET:system_information_category");
                                retryReply->setProperty("systemName", systemName);
                                retryReply->setProperty("category", category);
                                retryReply->setProperty("triedLowercase", false); // Don't retry again
                                qDebug() << "Retry request sent for" << systemName;
                            }
                            
                            reply->deleteLater();
                            return; // Don't emit any error - we're retrying
                        }
                    }
                }
                
                // For all other errors, show the popup
                qWarning() << "Supabase request failed:" << operation << "-" << error;
                emit networkError(error);
            }
        }
        
        return;
    } else {
        // Reset auth failure counter on successful request
        if (m_consecutiveAuthFailures > 0) {
            qDebug() << "Authentication recovered. Resetting failure counter.";
            m_consecutiveAuthFailures = 0;
        }
        
        qDebug() << "Supabase request successful for operation:" << operation;
    }
    
    // Handle different operation types
    if (operation.startsWith("GET:systems_near")) {
        QJsonArray systems = response.value("data").toArray();
        double centerX = reply->property("refX").toDouble();
        double centerY = reply->property("refY").toDouble();
        double centerZ = reply->property("refZ").toDouble();
        // Calculate distances and emit nearestSystemsReceived signal
        QJsonArray systemsWithDistance;
        for (const QJsonValue &value : systems) {
            QJsonObject system = value.toObject();
            double systemX = system.value("x").toDouble();
            double systemY = system.value("y").toDouble();
            double systemZ = system.value("z").toDouble();
            
            double distance = calculateDistance(centerX, centerY, centerZ, systemX, systemY, systemZ);
            
            // Transform system data for UI
            QJsonObject transformed;
            transformed["name"] = system.value("systems").toString();
            transformed["category"] = system.value("category").toString();
            transformed["distance"] = QString("%1 LY").arg(distance, 0, 'f', 1);
            transformed["x"] = systemX;
            transformed["y"] = systemY;
            transformed["z"] = systemZ;
            transformed["poi"] = "";
            transformed["claimed"] = false;
            transformed["done"] = false;
            transformed["claimedBy"] = "";
            transformed["distanceValue"] = distance;
            
            systemsWithDistance.append(transformed);
        }
        
        // Sort by distance
        QVariantList systemsList;
        for (const QJsonValue &value : systemsWithDistance) {
            systemsList.append(value.toObject().toVariantMap());
        }
        
        std::sort(systemsList.begin(), systemsList.end(), [](const QVariant &a, const QVariant &b) {
            QVariantMap mapA = a.toMap();
            QVariantMap mapB = b.toMap();
            return mapA["distanceValue"].toDouble() < mapB["distanceValue"].toDouble();
        });
        
        // Convert back to QJsonArray and remove distanceValue field
        QJsonArray sortedSystems;
        for (const QVariant &variant : systemsList) {
            QVariantMap map = variant.toMap();
            map.remove("distanceValue");
            sortedSystems.append(QJsonObject::fromVariantMap(map));
        }
        
        emit nearestSystemsReceived(sortedSystems);
        
    } else if (operation.startsWith("GET:systems")) {
        QJsonArray systems = response.value("data").toArray();
        
        if (systems.isEmpty()) {
            qDebug() << "❌ No systems in database response! HTTP Status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        } else {
            qDebug() << "✅ Received" << systems.size() << "systems from database";
        }
        
        // If no data array, try direct array response (Supabase sometimes returns array directly)
        if (systems.isEmpty() && response.isEmpty()) {
            qDebug() << "No data field found, checking if response is direct array";
            // Response might be direct array
            systems = QJsonArray(); // Will handle this case below
        }
        
        // Transform database format to match our UI expectations
        QJsonArray transformedSystems;
        for (const QJsonValue &value : systems) {
            QJsonObject system = value.toObject();
            QString category = system["category"].toString();
            
            // Filter out test/debug categories
            if (category == "TEST_CATEGORY" || category.startsWith("test_")) {
                continue;
            }
            
            QJsonObject transformed;
            
            // Map database fields to expected UI fields
            transformed["name"] = system["systems"];  // systems → name
            transformed["category"] = system["category"];
            
            // Coordinates ARE available in systems table!
            double x = system["x"].toDouble();
            double y = system["y"].toDouble();
            double z = system["z"].toDouble();
            transformed["x"] = x;
            transformed["y"] = y;
            transformed["z"] = z;
            
            // Calculate distance from Sol (0,0,0)
            double distance = std::sqrt(x*x + y*y + z*z);
            transformed["distance"] = QString::number(distance, 'f', 1) + " LY";
            
            // Add default values for fields we don't have yet
            transformed["poi"] = "";  // Will be populated from system_information
            transformed["claimed"] = false;  // Will implement claims tracking later
            transformed["done"] = false;
            transformed["claimedBy"] = "";
            
            transformedSystems.append(transformed);
        }
        
        qDebug() << "Transformed" << transformedSystems.size() << "systems for UI";
        if (transformedSystems.size() > 0) {
            // Sample system loaded
        }
        
        // CRITICAL FIX: Add POI data merging functionality
        mergePOIDataIntoSystems(transformedSystems);
        
        // Emit systems data (POI data will be merged synchronously)
        emit systemsReceived(transformedSystems);
        
    } else if (operation.startsWith("GET:taken_system_specific")) {
        QString systemName = reply->property("systemName").toString();
        QString commanderName = reply->property("commanderName").toString();
        QJsonArray takenData = response.value("data").toArray();
        
        qDebug() << "Got taken_system_specific response for" << systemName << "commander" << commanderName;
        qDebug() << "Response data:" << QJsonDocument(takenData).toJson(QJsonDocument::Compact);
        
        // Emit this as a single-item array for compatibility with existing handler
        emit takenSystemsReceived(takenData);
        
    } else if (operation.startsWith("GET:taken")) {
        QJsonArray taken = response.value("data").toArray();
        qDebug() << "Emitting takenSystemsReceived with" << taken.size() << "items (ALL systems)";
        emit takenSystemsReceived(taken);
        
    } else if (operation.startsWith("GET:current_commander_taken")) {
        QJsonArray taken = response.value("data").toArray();
        qDebug() << "Emitting takenSystemsReceived with" << taken.size() << "items (current commander only)";
        emit takenSystemsReceived(taken);
        
    } else if (operation.startsWith("GET:categories_systems")) {
        qDebug() << "Processing GET:categories_systems response";
        QJsonArray categoriesData = response.value("data").toArray();
        qDebug() << "Systems categories data size:" << categoriesData.size();
        
        // Extract unique categories and add "All Categories" at the beginning
        QStringList systemsCategories;
        systemsCategories << "All Categories"; // Always first
        
        QSet<QString> categorySet;
        for (const QJsonValue &value : categoriesData) {
            QJsonObject categoryObj = value.toObject();
            QString category = categoryObj.value("category").toString().trimmed();
            // Filter out test/debug categories
            if (!category.isEmpty() && !categorySet.contains(category) && 
                category != "TEST_CATEGORY" && !category.startsWith("test_")) {
                categorySet.insert(category);
                systemsCategories << category;
            }
        }
        
        // Store systems categories temporarily and fetch Richard categories
        m_pendingSystemsCategories = systemsCategories;
        
        // Now fetch Richard categories from preset_images table
        getRichardCategories();
        
    } else if (operation.startsWith("GET:categories_richard")) {
        qDebug() << "Processing GET:categories_richard response";
        QJsonArray richardData = response.value("data").toArray();
        qDebug() << "Richard categories data size:" << richardData.size();
        
        // Extract Richard categories (show ALL Richard categories, don't deduplicate)
        QStringList richardCategories;
        QSet<QString> richardSet;
        
        for (const QJsonValue &value : richardData) {
            QJsonObject categoryObj = value.toObject();
            QString category = categoryObj.value("category").toString().trimmed();
            if (!category.isEmpty() && !richardSet.contains(category)) {
                richardSet.insert(category);
                richardCategories << category;
            }
        }
        
        // Combine systems categories with Richard categories (remove duplicates)
        QStringList allCategories;
        
        // Add systems categories but exclude ones that will appear in Richard's section
        for (const QString &category : m_pendingSystemsCategories) {
            if (!richardSet.contains(category)) {
                allCategories << category;
            }
        }
        
        // Add separator and Richard categories if we have any (always show Richard's section)
        if (!richardCategories.isEmpty()) {
            allCategories << "--- Richard's Stuff ---";
            allCategories << richardCategories;
        }
        
        // Convert to JSON array for consistent signal interface
        QJsonArray categoriesArray;
        for (const QString &category : allCategories) {
            categoriesArray.append(category);
        }
        
        qDebug() << "Emitting categoriesReceived with" << categoriesArray.size() << "total categories (systems + Richard)";
        emit categoriesReceived(categoriesArray);
        
    } else if (operation.startsWith("GET:systems_near")) {
        qDebug() << "Processing GET:systems_near response";
        QJsonArray systemsData = response.value("data").toArray();
        qDebug() << "Systems near data size:" << systemsData.size();
        
        // Get reference coordinates from reply properties
        double refX = reply->property("refX").toDouble();
        double refY = reply->property("refY").toDouble();
        double refZ = reply->property("refZ").toDouble();
        
        qDebug() << "Reference coordinates:" << refX << refY << refZ;
        
        // Calculate distances and sort by distance
        QJsonArray systemsWithDistance;
        for (const QJsonValue &value : systemsData) {
            QJsonObject system = value.toObject();
            double systemX = system.value("x").toDouble();
            double systemY = system.value("y").toDouble();
            double systemZ = system.value("z").toDouble();
            
            double distance = calculateDistance(refX, refY, refZ, systemX, systemY, systemZ);
            
            // Transform system data for UI (similar to getSystems)
            QJsonObject transformed;
            transformed["name"] = system.value("systems").toString();
            transformed["category"] = system.value("category").toString();
            transformed["distance"] = QString("%1 LY").arg(distance, 0, 'f', 1);
            transformed["x"] = systemX;
            transformed["y"] = systemY;
            transformed["z"] = systemZ;
            transformed["poi"] = "";  // Will be filled from POI table join later
            transformed["claimed"] = false;  // Will be filled from taken table join later
            transformed["done"] = false;  // TODO: done status not available in systems table
            transformed["claimedBy"] = "";
            transformed["distanceValue"] = distance;  // For sorting
            
            systemsWithDistance.append(transformed);
        }
        
        // Sort by distance (convert to QVariantList for easier sorting)
        QVariantList systemsList;
        for (const QJsonValue &value : systemsWithDistance) {
            systemsList.append(value.toObject().toVariantMap());
        }
        
        std::sort(systemsList.begin(), systemsList.end(), [](const QVariant &a, const QVariant &b) {
            QVariantMap mapA = a.toMap();
            QVariantMap mapB = b.toMap();
            return mapA["distanceValue"].toDouble() < mapB["distanceValue"].toDouble();
        });
        
        // Convert back to QJsonArray and remove distanceValue field
        QJsonArray sortedSystems;
        for (const QVariant &variant : systemsList) {
            QVariantMap map = variant.toMap();
            map.remove("distanceValue");  // Remove sorting helper field
            sortedSystems.append(QJsonObject::fromVariantMap(map));
        }
        
        qDebug() << "*** MERGING POI DATA DIRECTLY INTO SYSTEMS ***";
        
        // Directly query and merge POI data synchronously
        QStringList systemNames;
        for (int i = 0; i < sortedSystems.size(); i++) {
            QJsonObject system = sortedSystems[i].toObject();
            QString systemName = system["name"].toString();
            if (!systemName.isEmpty()) {
                systemNames.append(systemName);
            }
        }
        
        if (!systemNames.isEmpty()) {
            // Build query for POI data
            QStringList systemQueries;
            for (const QString &systemName : systemNames) {
                systemQueries.append(QString("system.eq.%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(systemName))));
            }
            
            QString systemFilter = systemQueries.join(",");
            QString endpoint = QString("system_information?select=system,potential_or_poi&or=(%1)").arg(systemFilter);
            
            qDebug() << "Querying POI data for systems_near:" << endpoint;
            
            QNetworkRequest request = createRequest(endpoint);
            request.setRawHeader("x-commander-name", ""); // Remove commander headers for public access
            
            QNetworkReply *reply = m_networkManager->get(request);
            
            if (reply) {
                reply->setProperty("operation", "GET:poi_for_systems_near");
                // Store the sorted systems so we can merge and emit them when POI data arrives
                QJsonDocument systemsDoc(sortedSystems);
                reply->setProperty("sortedSystems", systemsDoc.toJson());
                qDebug() << "POI data request sent for systems_near";
                return; // Don't emit systems yet, wait for POI data
            }
        }
        
        qDebug() << "*** EMITTING NEAREST SYSTEMS RECEIVED (NO POI MERGE) ***";
        qDebug() << "Emitting nearestSystemsReceived with" << sortedSystems.size() << "sorted systems";
        emit nearestSystemsReceived(sortedSystems);
        
    } else if (operation.startsWith("GET:preset_images")) {
        QJsonArray presetImages = response.value("data").toArray();
        
        // Store count for future change detection
        QJsonObject tables = m_syncState.value("tables").toObject();
        tables["preset_images_count"] = presetImages.size();
        m_syncState["tables"] = tables;
        saveSyncState();
        
        emit presetImagesReceived(presetImages);
        
    } else if (operation.startsWith("GET:system_information_primary")) {
        QString systemName = reply->property("systemName").toString();
        QString fallbackCategory = reply->property("fallbackCategory").toString();
        QJsonArray systemInfoArray = response.value("data").toArray();
        
        qDebug() << "Got system_information_primary response for" << systemName;
        
        // Check if we got data and if system_info field is not empty
        bool hasCustomInfo = false;
        QJsonObject customInfo;
        
        if (!systemInfoArray.isEmpty()) {
            customInfo = systemInfoArray.first().toObject();
            QString systemInfo = customInfo.value("system_info").toString();
            
            // Check if system_info field exists and is not empty
            if (!systemInfo.isEmpty() && systemInfo.trimmed() != "") {
                hasCustomInfo = true;
                qDebug() << "Found custom system information:" << systemInfo.left(100) + "...";
            }
        }
        
        if (hasCustomInfo) {
            // Use the custom system information
            customInfo["hasInformation"] = true;
            emit systemInformationReceived(systemName, customInfo);
        } else {
            // Fall back to category table data
            qDebug() << "No custom system_info found, falling back to category table:" << fallbackCategory;
            getSystemInformationFromCategory(systemName, fallbackCategory);
        }
        
    } else if (operation.startsWith("GET:system_information_category")) {
        QString systemName = reply->property("systemName").toString();
        QString category = reply->property("category").toString();
        QJsonArray categoryDataArray = response.value("data").toArray();
        
        qDebug() << "Got system_information_category response for" << systemName << "category:" << category;
        
        QJsonObject formattedSystemInfo;
        
        if (!categoryDataArray.isEmpty()) {
            QJsonObject rawCategoryData = categoryDataArray.first().toObject();
            
            // Use the new formatCategoryTableData method to create readable text
            QString formattedText = formatCategoryTableData(rawCategoryData, category);
            
            formattedSystemInfo["hasInformation"] = true;
            formattedSystemInfo["system_info"] = formattedText;
            formattedSystemInfo["category"] = category;
            formattedSystemInfo["system"] = systemName;
            formattedSystemInfo["raw_data"] = rawCategoryData;
            
            qDebug() << "Formatted category data into readable system information";
        } else {
            // No data found in category table either
            formattedSystemInfo["hasInformation"] = false;
            formattedSystemInfo["system_info"] = "No additional system information available";
            formattedSystemInfo["category"] = category;
            formattedSystemInfo["system"] = systemName;
            
            qDebug() << "No data found in category table for" << systemName;
        }
        
        emit systemInformationReceived(systemName, formattedSystemInfo);
        
    } else if (operation.startsWith("GET:system_information_db")) {
        QString systemName = reply->property("systemName").toString();
        QJsonArray systemInfoArray = response.value("data").toArray();
        
        qDebug() << "Got system_information_db response for" << systemName;
        qDebug() << "Response data:" << QJsonDocument(systemInfoArray).toJson(QJsonDocument::Compact);
        
        QJsonObject formattedSystemInfo;
        if (!systemInfoArray.isEmpty()) {
            QJsonObject rawData = systemInfoArray.first().toObject();
            formattedSystemInfo = rawData;  // Use raw data directly
            formattedSystemInfo["hasInformation"] = true;
            qDebug() << "Found system_information data:" << QJsonDocument(rawData).toJson(QJsonDocument::Compact);
        } else {
            formattedSystemInfo["hasInformation"] = false;
            qDebug() << "No system_information data found for" << systemName;
        }
        
        emit systemInformationReceived(systemName, formattedSystemInfo);
        
    } else if (operation.startsWith("GET:system_details")) {
        QString systemName = reply->property("systemName").toString();
        QString category = reply->property("category").toString();
        
        QJsonArray systemDetailsArray = response.value("data").toArray();
        QJsonObject systemDetails;
        
        if (!systemDetailsArray.isEmpty()) {
            systemDetails = systemDetailsArray.first().toObject();
            qDebug() << "System details retrieved for" << systemName << "in category" << category;
        } else {
            qDebug() << "No system details found for" << systemName << "in category" << category;
        }
        
        // Emit as system information for now (same signal interface)
        emit systemInformationReceived(systemName, systemDetails);
        
    } else if (operation.startsWith("GET:system_category_lookup")) {
        QString systemName = reply->property("systemName").toString();
        QJsonArray systemData = response.value("data").toArray();
        
        if (!systemData.isEmpty()) {
            QJsonObject systemInfo = systemData.first().toObject();
            QString category = systemInfo.value("category").toString();
            double x = systemInfo.value("x").toDouble();
            double y = systemInfo.value("y").toDouble();
            double z = systemInfo.value("z").toDouble();
            
            qDebug() << "Found system" << systemName << "in category" << category << "at coordinates" << x << y << z;
            
            // Now query the category-specific table for detailed information
            qDebug() << "Querying category table" << category << "for detailed system information";
            getSystemInformation(systemName, category);
        } else {
            qDebug() << "No system found for" << systemName;
            emit networkError(QString("System %1 not found in database").arg(systemName));
        }
        
    } else if (operation.startsWith("GET:system_details_robust")) {
        QString systemName = reply->property("systemName").toString();
        QString category = reply->property("category").toString();
        QString fieldName = reply->property("fieldName").toString();
        
        QJsonArray systemDetailsArray = response.value("data").toArray();
        
        if (!systemDetailsArray.isEmpty()) {
            QJsonObject systemDetails = systemDetailsArray.first().toObject();
            qDebug() << "System details retrieved for" << systemName << "using field" << fieldName;
            emit systemInformationReceived(systemName, systemDetails);
        } else {
            // Try the other capitalization if this one failed
            if (fieldName == "System") {
                qDebug() << "Trying lowercase 'system' field for" << systemName;
                getSystemDetailsWithFieldName(systemName, category, "system");
            } else {
                qDebug() << "No system details found for" << systemName << "in category" << category;
                emit networkError(QString("System details not found for %1").arg(systemName));
            }
        }
        
    } else if (operation.startsWith("GET:admin_access")) {
        QJsonArray adminData = response.value("data").toArray();
        bool isAdmin = !adminData.isEmpty();
        emit adminStatusReceived(isAdmin);
        
    } else if (operation.startsWith("GET:taken")) {
        QJsonArray takenData = response.value("data").toArray();
        qDebug() << "Received" << takenData.size() << "taken systems from Supabase";
        emit takenSystemsReceived(takenData);
        
    } else if (operation.startsWith("GET:poi_data_for_merge")) {
        QJsonArray poiData = response.value("data").toArray();
        int systemsCount = reply->property("systemsCount").toInt();
        QByteArray systemsJson = reply->property("systemsToMerge").toByteArray();
        
        qDebug() << "Received" << poiData.size() << "POI records for merging with" << systemsCount << "systems";
        
        // Parse the stored systems array
        QJsonDocument systemsDoc = QJsonDocument::fromJson(systemsJson);
        QJsonArray systemsArray = systemsDoc.array();
        
        // Merge POI data into the systems
        for (int i = 0; i < systemsArray.size(); i++) {
            QJsonObject system = systemsArray[i].toObject();
            QString systemName = system["name"].toString();
            
            // Look for this system in POI data
            for (const QJsonValue &poiValue : poiData) {
                QJsonObject poiSystem = poiValue.toObject();
                QString poiSystemName = poiSystem.value("system").toString();
                
                if (systemName == poiSystemName) {
                    QString potentialOrPoi = poiSystem.value("potential_or_poi").toString();
                    if (!potentialOrPoi.isEmpty()) {
                        system["poi"] = potentialOrPoi;
                        system["potential_or_poi"] = potentialOrPoi;
                        systemsArray[i] = system;
                        qDebug() << "Merged POI data for" << systemName << ":" << potentialOrPoi;
                    }
                    break;
                }
            }
        }
        
        // Store updated POI data for future use
        m_pendingPOIData = poiData;
        
        // Emit signal with the merged systems data and cache it
        m_cachedNearestSystems = systemsArray;
        emit poiDataForMergeReceived(systemsArray);
        
    } else if (operation.startsWith("GET:poi_for_systems_near")) {
        QJsonArray poiData = response.value("data").toArray();
        QByteArray systemsJson = reply->property("sortedSystems").toByteArray();
        
        qDebug() << "Received" << poiData.size() << "POI records for systems_near merge";
        
        // Parse the stored systems array
        QJsonDocument systemsDoc = QJsonDocument::fromJson(systemsJson);
        QJsonArray sortedSystems = systemsDoc.array();
        
        // Merge POI data into the systems
        for (int i = 0; i < sortedSystems.size(); i++) {
            QJsonObject system = sortedSystems[i].toObject();
            QString systemName = system["name"].toString();
            
            // Look for this system in POI data
            for (const QJsonValue &poiValue : poiData) {
                QJsonObject poiSystem = poiValue.toObject();
                QString poiSystemName = poiSystem.value("system").toString();
                
                if (systemName == poiSystemName) {
                    QString potentialOrPoi = poiSystem.value("potential_or_poi").toString();
                    if (!potentialOrPoi.isEmpty()) {
                        system["poi"] = potentialOrPoi;
                        system["potential_or_poi"] = potentialOrPoi;
                        sortedSystems[i] = system;
                        qDebug() << "*** MERGED POI FOR SYSTEMS_NEAR:" << systemName << "=" << potentialOrPoi << "***";
                    }
                    break;
                }
            }
        }
        
        qDebug() << "*** EMITTING NEAREST SYSTEMS WITH POI DATA MERGED ***";
        m_cachedNearestSystems = sortedSystems;
        emit nearestSystemsReceived(sortedSystems);
        
    } else if (operation.startsWith("GET:pois")) {
        QJsonArray poisData = response.value("data").toArray();
        qDebug() << "Received" << poisData.size() << "POI systems from Supabase";
        
        // Store POI data for future merge operations
        m_pendingPOIData = poisData;
        qDebug() << "Stored" << poisData.size() << "POI records in cache for merging";
        
        emit poisReceived(poisData);
        
    } else if (operation.startsWith("RPC:claim_system")) {
        QString systemName = reply->property("systemName").toString();
        QString commander = reply->property("commander").toString();
        
        QJsonObject result = response;
        bool success = result["success"].toBool();
        QString message = result["message"].toString();
        QString errorMsg = result["error"].toString();
        
        if (success) {
            qDebug() << "System claimed successfully:" << systemName;
            emit systemClaimed(systemName, true);
            
            // Refresh data to show the new claim
            getTakenSystems();
        } else {
            qDebug() << "Failed to claim system:" << errorMsg;
            emit systemClaimed(systemName, false);
            emit networkError(errorMsg);
        }
        
    } else if (operation.startsWith("RPC:release_claim")) {
        QString systemName = reply->property("systemName").toString();
        
        QJsonObject result = response;
        bool success = result["success"].toBool();
        QString message = result["message"].toString();
        QString errorMsg = result["error"].toString();
        
        if (success) {
            qDebug() << "Claim released successfully";
            emit systemUnclaimed(systemName, true);
            
            // Refresh data to show the claim is gone
            getTakenSystems();
        } else {
            qDebug() << "Failed to release claim:" << errorMsg;
            emit systemUnclaimed(systemName, false);
            emit networkError(errorMsg);
        }
        
    } else if (operation.startsWith("POST:taken")) {
        QString systemName = reply->property("systemName").toString();
        QString commander = reply->property("commander").toString();
        qDebug() << "System" << systemName << "successfully claimed by" << commander;
        emit systemClaimed(systemName, true);
        // Ensure all clients get fresh claim data immediately
        QTimer::singleShot(0, this, [this]() { getTakenSystems(); });
        
    } else if (operation.startsWith("DELETE:taken")) {
        QString systemName = reply->property("systemName").toString();
        qDebug() << "System" << systemName << "successfully unclaimed";
        emit systemUnclaimed(systemName, true);
        // Ensure all clients get fresh claim data immediately
        QTimer::singleShot(0, this, [this]() { getTakenSystems(); });
        
    } else if (operation.startsWith("UPSERT:system_information_poi_set")) {
        QString systemName = reply->property("systemName").toString();
        QString poiType = reply->property("poiType").toString();
        qDebug() << "system_information updated for" << systemName << "poi=" << poiType;
        emit requestCompleted("markSystemAsPOI", true, QString("System marked as %1").arg(poiType));
        // Proactively refresh POI merge after marking
        if (!m_cachedNearestSystems.isEmpty()) {
            QJsonArray systemsArray = m_cachedNearestSystems; // copy
            fetchAndMergePOIData(systemsArray);
        }
        // Force a direct POI query for the single system to remove any cache ambiguity
        QJsonArray single;
        QJsonObject obj; obj["name"] = systemName; single.append(obj);
        fetchAndMergePOIData(single);
        
    } else if (operation.startsWith("UPDATE:system_information_poi_clear")) {
        QString systemName = reply->property("systemName").toString();
        qDebug() << "POI field cleared in system_information for" << systemName;
        // Trigger POI re-merge to drop badge quickly
        if (!m_cachedNearestSystems.isEmpty()) {
            QJsonArray systemsArray = m_cachedNearestSystems; // copy
            fetchAndMergePOIData(systemsArray);
        }
        QJsonArray single;
        QJsonObject obj; obj["name"] = systemName; single.append(obj);
        fetchAndMergePOIData(single);

    } else if (operation.startsWith("DELETE:pois")) {
        QString systemName = reply->property("systemName").toString();
        qDebug() << "POI status removed from system" << systemName;
        emit requestCompleted("removePOIStatus", true, "POI status removed");
        
    } else if (operation.startsWith("GET:all_commanders")) {
        QJsonArray commanderData = response.value("data").toArray();
        qDebug() << "Received" << commanderData.size() << "commander locations from Supabase";
        emit allCommanderLocationsReceived(commanderData);
        
    } else if (operation.startsWith("PATCH:commander_location")) {
        QString commanderName = operation.split(":").last();
        qDebug() << "Commander location successfully updated for" << commanderName;
        emit commanderLocationUpdated(commanderName, true);
        
    } else if (operation.startsWith("UPDATE:system_visited")) {
        QString systemName = reply->property("systemName").toString();
        bool visited = reply->property("visited").toBool();
        
        // Debug the actual response from database
        QJsonArray responseData = response.value("data").toArray();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        
        qDebug() << "=== VISITED UPDATE RESPONSE DEBUG ===";
        qDebug() << "HTTP Status:" << httpStatus;
        qDebug() << "Response data size:" << responseData.size();
        qDebug() << "Response data:" << QJsonDocument(responseData).toJson(QJsonDocument::Compact);
        qDebug() << "Full response:" << QJsonDocument(response).toJson(QJsonDocument::Compact);
        qDebug() << "Request was for system:" << systemName << "visited:" << visited;
        
        // FIXED: Check if update was successful based on HTTP status and response
        bool updateSuccess = (httpStatus == 200 || httpStatus == 204) && reply->error() == QNetworkReply::NoError;
        qDebug() << "Update success determined as:" << updateSuccess;
        qDebug() << "=== END VISITED UPDATE RESPONSE DEBUG ===";
        
        if (updateSuccess) {
            qDebug() << "System visited status updated successfully for" << systemName << "to" << visited;
            emit systemStatusUpdated(systemName, true);
            
            // Refresh taken systems data to update UI
            QTimer::singleShot(500, this, [this]() {
                getTakenSystems();
            });
        } else {
            qDebug() << "System visited status update failed for" << systemName;
            emit systemStatusUpdated(systemName, false);
        }
        
    } else if (operation.startsWith("UPDATE:system_done")) {
        QString systemName = reply->property("systemName").toString();
        bool done = reply->property("done").toBool();
        
        // Check success similar to visited update
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool updateSuccess = (httpStatus == 200 || httpStatus == 204) && reply->error() == QNetworkReply::NoError;
        
        if (updateSuccess) {
            qDebug() << "System done status updated successfully for" << systemName << "to" << done;
            emit systemStatusUpdated(systemName, true);
            
            // Refresh taken systems data to update UI
            QTimer::singleShot(500, this, [this]() {
                getTakenSystems();
            });
        } else {
            qDebug() << "System done status update failed for" << systemName;
            emit systemStatusUpdated(systemName, false);
        }
        
    } else if (operation.startsWith("UPDATE:system_edited")) {
        QString systemName = reply->property("systemName").toString();
        
        // Check success
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool updateSuccess = (httpStatus == 200 || httpStatus == 204) && reply->error() == QNetworkReply::NoError;
        
        if (updateSuccess) {
            qDebug() << "System marked as edited successfully for" << systemName;
            
            // Refresh taken systems data to update UI with edited flag
            QTimer::singleShot(300, this, [this]() {
                getTakenSystems();
            });
        } else {
            qDebug() << "Failed to mark system as edited for" << systemName;
        }
        
    } else if (operation.startsWith("TEST:admin_access")) {
        qDebug() << "Admin access test successful - user has admin privileges";
        emit adminAccessTestComplete(true);
        
    } else if (operation.startsWith("GET:webhook_config")) {
        QJsonArray configData = response.value("data").toArray();
        if (!configData.isEmpty()) {
            QString webhookUrl = configData[0].toObject().value("config_value").toString();
            m_webhookUrl = webhookUrl;
            m_webhookConfigLoaded = true;
            qDebug() << "Webhook configuration loaded successfully";
            emit webhookConfigReceived(webhookUrl);
        } else {
            qWarning() << "No webhook configuration found in app_config table";
            emit webhookConfigReceived("");
        }
        
    } else if (operation.startsWith("POST:webhook:")) {
        QString eventType = operation.split(":").last();
        qDebug() << "Webhook successfully sent for event:" << eventType;
        emit webhookTriggered(true, eventType);
        
    } else if (operation.startsWith("CHECK:system_info_exists")) {
        QString systemName = reply->property("systemName").toString();
        QVariantMap information = reply->property("information").toMap();
        QJsonArray existingData = response.value("data").toArray();
        
        // Map QML field names to database column names
        QJsonObject data;
        data["system"] = systemName;  // Required field
        data["system_info"] = information.value("system_info", "").toString();  // FIXED: correct field name
        data["description"] = information.value("description", "").toString(); 
        data["name"] = information.value("name", "").toString();
        data["images"] = information.value("images", "").toString();
        
        // *** CRITICAL FIX: Add the missing POI fields ***
        data["discoverer"] = information.value("discoverer", "").toString();
        data["submitter"] = information.value("submitter", "").toString();
        data["potential_or_poi"] = information.value("potential_or_poi", "").toString();
        
        qDebug() << "=== HELLO my name is burger - DATABASE OPERATION ===";
        qDebug() << "About to save data:" << data;
        qDebug() << "Operation type:" << (existingData.isEmpty() ? "INSERT" : "UPDATE");
        qDebug() << "=== END HELLO my name is burger - DATABASE OPERATION ===";
        
        QNetworkRequest dataRequest;
        QNetworkReply *dataReply = nullptr;
        
        if (existingData.isEmpty()) {
            // Record doesn't exist - INSERT
            qDebug() << "System_information record doesn't exist for" << systemName << "- inserting new record";
            dataRequest = createRequest("system_information");
            
            QJsonDocument doc(data);
            dataReply = m_networkManager->post(dataRequest, doc.toJson());
            
            if (dataReply) {
                dataReply->setProperty("operation", "INSERT:system_information");
                dataReply->setProperty("systemName", systemName);
            }
        } else {
            // Record exists - UPDATE
            qDebug() << "System_information record exists for" << systemName << "- updating existing record";
            QString updateEndpoint = QString("system_information?system=eq.%1")
                                    .arg(QUrl::toPercentEncoding(systemName));
            dataRequest = createRequest(updateEndpoint);
            
            // Remove the system field from update data (can't update primary key)
            data.remove("system");
            
            QJsonDocument doc(data);
            dataReply = m_networkManager->sendCustomRequest(dataRequest, "PATCH", doc.toJson());
            
            if (dataReply) {
                dataReply->setProperty("operation", "UPDATE:system_information");
                dataReply->setProperty("systemName", systemName);
            }
        }
        
        // Add commander headers for RLS policies
        if (dataReply && !m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
            dataRequest.setRawHeader("X-Commander", m_currentCommander.toUtf8());
            dataRequest.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
        }
        
    } else if (operation.startsWith("INSERT:system_information") || operation.startsWith("UPDATE:system_information")) {
        QString systemName = reply->property("systemName").toString();
        QString operationType = operation.startsWith("INSERT") ? "created" : "updated";
        
        qDebug() << "=== HELLO my name is burger - DATABASE SUCCESS ===";
        qDebug() << "System information" << operationType << "successfully for" << systemName;
        qDebug() << "=== END HELLO my name is burger - DATABASE SUCCESS ===";
        
        emit requestCompleted("saveSystemInformation", true, QString("System information %1").arg(operationType));
        
        // If potential_or_poi changed via save, immediately re-merge POI data so badges update without restart
        if (!m_cachedNearestSystems.isEmpty()) {
            QJsonArray systemsArray = m_cachedNearestSystems; // copy
            fetchAndMergePOIData(systemsArray);
        }
        QJsonArray single;
        QJsonObject obj; obj["name"] = systemName; single.append(obj);
        fetchAndMergePOIData(single);
        
    } else if (operation.startsWith("SET:commander_context")) {
        qDebug() << "Commander context set successfully";
        emit requestCompleted("setCommanderContext", true, "Commander context updated");
        
    } else if (operation.startsWith("IMGBB:upload")) {
        QString systemName = reply->property("systemName").toString();
        QString filePath = reply->property("filePath").toString();
        
        qDebug() << "=== IMGBB RESPONSE DEBUG START ===";
        qDebug() << "System:" << systemName;
        qDebug() << "File:" << filePath;
        
        // Get all response details
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QNetworkReply::NetworkError networkError = reply->error();
        QString errorString = reply->errorString();
        QByteArray responseBody = reply->readAll();
        
        qDebug() << "HTTP Status Code:" << httpStatus;
        qDebug() << "Network Error:" << networkError;
        qDebug() << "Error String:" << errorString;
        qDebug() << "Response Body Length:" << responseBody.length();
        qDebug() << "Response Body Content:" << responseBody;
        
        // Log all response headers
        qDebug() << "Response Headers:";
        const QList<QNetworkReply::RawHeaderPair> headers = reply->rawHeaderPairs();
        for (const QNetworkReply::RawHeaderPair &header : headers) {
            qDebug() << "  " << header.first << ":" << header.second;
        }
        
        if (success && networkError == QNetworkReply::NoError) {
            // Try to parse the response
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);
            
            if (parseError.error != QJsonParseError::NoError) {
                qDebug() << "JSON Parse Error:" << parseError.errorString();
                qDebug() << "Raw response:" << responseBody;
                emit requestCompleted("uploadImageToImgbb", false, "Invalid JSON response from imgbb");
            } else {
                QJsonObject jsonResponse = doc.object();
                qDebug() << "Parsed JSON response:" << doc.toJson(QJsonDocument::Compact);
                
                // Check if imgbb returned success
                bool imgbbSuccess = jsonResponse.value("success").toBool();
                if (!imgbbSuccess) {
                    QJsonObject error = jsonResponse.value("error").toObject();
                    QString errorMsg = error.value("message").toString("Unknown imgbb error");
                    qDebug() << "ImgBB returned success=false:" << errorMsg;
                    emit requestCompleted("uploadImageToImgbb", false, QString("ImgBB Error: %1").arg(errorMsg));
                } else {
                    // Success! Extract image URLs
                    QJsonObject data = jsonResponse.value("data").toObject();
                    QString imageUrl = data.value("url").toString();
                    QString displayUrl = data.value("display_url").toString();
                    QString viewUrl = data.value("url_viewer").toString();
                    
                    QString finalUrl = displayUrl.isEmpty() ? imageUrl : displayUrl;
                    
                    if (!finalUrl.isEmpty()) {
                        qDebug() << "SUCCESS! Image uploaded to:" << finalUrl;
                        
                        // Store as user override for this system
                        m_systemImageOverrides[systemName] = finalUrl;
                        
                        // SAVE TO DATABASE: Update system_information table
                        saveImageToDatabase(systemName, finalUrl);
                        
                        emit requestCompleted("uploadImageToImgbb", true, QString("Image uploaded: %1").arg(finalUrl));
                        emit systemImageSet(systemName, finalUrl, true);
                    } else {
                        qDebug() << "No usable image URL in successful response";
                        emit requestCompleted("uploadImageToImgbb", false, "No image URL in response");
                    }
                }
            }
        } else {
            // Upload failed - provide detailed error
            QString detailedError = QString("Upload failed - HTTP %1: %2").arg(httpStatus).arg(errorString);
            
            // Log raw error details
            qDebug() << "IMGBB UPLOAD FAILED:";
            qDebug() << "  HTTP Status:" << httpStatus;
            qDebug() << "  Network Error Code:" << networkError;
            qDebug() << "  Error String:" << errorString;
            qDebug() << "  Response Body Empty:" << responseBody.isEmpty();
            qDebug() << "  Response Body Size:" << responseBody.size();
            if (!responseBody.isEmpty()) {
                qDebug() << "  Response Body (first 500 chars):" << responseBody.left(500);
            }
            
            if (httpStatus == 400) {
                if (responseBody.isEmpty()) {
                    detailedError = "HTTP 400 with empty response - likely invalid API key or malformed request";
                } else if (responseBody.contains("API key") || responseBody.contains("api_key")) {
                    detailedError = "Invalid API key - get a new one from https://api.imgbb.com/";
                } else {
                    detailedError = QString("Bad request: %1").arg(QString::fromUtf8(responseBody));
                }
            } else if (httpStatus == 429) {
                detailedError = "Rate limit exceeded - try again later";
            } else if (httpStatus == 403) {
                detailedError = "Access forbidden - check API key permissions";
            } else if (httpStatus == 0 && errorString.contains("server replied:")) {
                // This is the case we're seeing - Qt couldn't parse the response
                detailedError = "Server response could not be parsed - possible encoding issue or network proxy";
            } else if (responseBody.isEmpty()) {
                detailedError = QString("Empty response with HTTP %1 - possible network/DNS issue").arg(httpStatus);
            }
            
            qDebug() << "DETAILED ERROR:" << detailedError;
            emit requestCompleted("uploadImageToImgbb", false, detailedError);
        }
        
        qDebug() << "=== IMGBB RESPONSE DEBUG END ===";
        
    } else if (operation.startsWith("GET:preset_image_category")) {
        QString category = reply->property("category").toString();
        QJsonArray presetData = response.value("data").toArray();
        
        if (!presetData.isEmpty()) {
            QJsonObject presetImage = presetData.first().toObject();
            QString imageUrl = presetImage.value("image_url").toString();
            QString altUrl = presetImage.value("image_link").toString(); // Fallback field name
            
            if (imageUrl.isEmpty() && !altUrl.isEmpty()) {
                imageUrl = altUrl;
            }
            
            if (!imageUrl.isEmpty()) {
                // Cache the result
                m_categoryImageCache[category] = imageUrl;
                
                // Check if we have a pending request for this category
                if (m_pendingPresetRequests.contains(category)) {
                    QString systemName = m_pendingPresetRequests.take(category);
                    qDebug() << "Found preset image for category" << category << "system" << systemName << ":" << imageUrl;
                    emit presetImageFound(systemName, imageUrl, category);
                }
                
                qDebug() << "Cached preset image for category" << category << ":" << imageUrl;
            } else {
                qDebug() << "No image URL found in preset image data for category:" << category;
                
                if (m_pendingPresetRequests.contains(category)) {
                    QString systemName = m_pendingPresetRequests.take(category);
                    emit systemImageSet(systemName, "", false);
                }
            }
        } else {
            qDebug() << "No preset image found for category:" << category;
            
            if (m_pendingPresetRequests.contains(category)) {
                QString systemName = m_pendingPresetRequests.take(category);
                emit systemImageSet(systemName, "", false);
            }
        }
        
    } else if (operation.startsWith("CHECK:preset_images_count")) {
        // Handle count check for incremental sync
        QJsonArray responseData = response.value("data").toArray();
        int currentCount = responseData.size();
        
        qDebug() << "Preset images count check response size:" << currentCount;
        qDebug() << "Raw response data:" << QJsonDocument(responseData).toJson(QJsonDocument::Compact);
        
        // Compare with stored count
        QJsonObject tables = m_syncState.value("tables").toObject();
        int storedCount = tables.value("preset_images_count").toInt(0);
        
        // If current count seems wrong (too low), skip the check to avoid false positives
        if (currentCount < 10 && storedCount > 10) {
            qWarning() << "Count check returned suspiciously low count (" << currentCount << "), skipping check to avoid false positive";
            finalizeDatabaseSync(true, 0);
        } else if (currentCount != storedCount) {
            qDebug() << "Preset images count changed:" << storedCount << "->" << currentCount;
            // Download updated preset images
            getPresetImages(true);
            
            // Update stored count
            tables["preset_images_count"] = currentCount;
            m_syncState["tables"] = tables;
            finalizeDatabaseSync(true, abs(currentCount - storedCount));
        } else {
            qDebug() << "No changes detected in preset images";
            
            // Still load preset images if this is the first time or if ImageLoader doesn't have them
            if (storedCount == 0 || currentCount > 0) {
                qDebug() << "Loading preset images from database (initial load or refresh)";
                getPresetImages(true);
            }
            
            finalizeDatabaseSync(true, 0);
        }
    
    // Authentication operation handlers
    } else if (operation == "security_check") {
        QString commanderName = reply->property("commander_name").toString();
        QJsonArray data = response.value("data").toArray();
        
        if (data.isEmpty()) {
            // Commander not found in security table - add as new user
            qDebug() << "[DEBUG]" << commanderName << "not found in security table - will add as new user";
            
            // For now, treat as legitimate new user (full implementation would check journal for alts)
            QStringList allCommanders;
            allCommanders << commanderName;
            handleNewCommander(commanderName, allCommanders);
            emit authenticationComplete(true, "New user added successfully");
        } else {
            // Commander found in security table
            QJsonObject commanderData = data.first().toObject();
            bool isBlocked = commanderData.value("blocked").toBool();
            QString notes = commanderData.value("notes").toString();
            
            if (isBlocked) {
                qDebug() << "[DEBUG] User" << commanderName << "is blocked - denying access";
                logLoginEvent(commanderName, false, "blocked_attempt");
                emit authenticationComplete(false, "You are unauthenticated. Speak to the plugin owner in Discord to gain access.");
            } else {
                qDebug() << "[DEBUG] User" << commanderName << "authenticated - allowing access";
                logLoginEvent(commanderName, false, "login");
                emit authenticationComplete(true, "Authentication successful");
            }
        }
        
    } else if (operation == "add_new_commander") {
        QString commanderName = reply->property("commander_name").toString();
        QJsonArray data = response.value("data").toArray();
        
        if (!data.isEmpty()) {
            qDebug() << "[DEBUG] Successfully added" << commanderName << "to security table";
            logLoginEvent(commanderName, false, "new_user");
        } else {
            qDebug() << "[ERROR] Failed to add" << commanderName << "to security table";
            emit authenticationComplete(false, "Failed to add user to security table");
        }
        
    } else if (operation == "check_banned_alt") {
        QString commanderName = reply->property("commander_name").toString();
        QString altCommander = reply->property("alt_commander").toString();
        QJsonArray data = response.value("data").toArray();
        
        if (!data.isEmpty()) {
            // Found a banned alt commander - block the new commander
            qDebug() << "[WARNING] RENAME DETECTED!" << altCommander << "(blocked) renamed to" << commanderName;
            
            // Update the new commander to blocked status
            QJsonObject updateData;
            updateData["blocked"] = true;
            updateData["notes"] = QString("⚠️ SUSPICIOUS: Blocked commanders in same journal: %1").arg(altCommander);
            
            QString endpoint = QString("security?name=eq.%1").arg(commanderName);
            QNetworkRequest request = createRequest(endpoint);
            QJsonDocument doc(updateData);
            QNetworkReply *updateReply = m_networkManager->sendCustomRequest(request, "PATCH", doc.toJson());
            updateReply->setProperty("operation", "block_renamed_commander");
            updateReply->setProperty("commander_name", commanderName);
            updateReply->setProperty("alt_commander", altCommander);
            
            logLoginEvent(commanderName, false, "rename_attempt", QString("%1 renamed to %2").arg(altCommander, commanderName));
            emit authenticationComplete(false, QString("Rename detected! %1 is banned.\nSpeak to the plugin owner in Discord to gain access.").arg(altCommander));
        }
        
    } else if (operation == "block_renamed_commander") {
        QString commanderName = reply->property("commander_name").toString();
        QString altCommander = reply->property("alt_commander").toString();
        qDebug() << "[SECURITY] Blocked" << commanderName << "due to rename from banned commander" << altCommander;
        
    } else if (operation == "log_login_event") {
        // Login event logged successfully
        QJsonArray data = response.value("data").toArray();
        if (!data.isEmpty()) {
            qDebug() << "[DEBUG] Login event logged successfully";
        }
    } else if (operation.startsWith("CHECK:system_info_for_image")) {
        QString systemName = reply->property("systemName").toString();
        QString imageUrl = reply->property("imageUrl").toString();
        QJsonArray systemInfoData = response.value("data").toArray();
        
        if (!systemInfoData.isEmpty()) {
            // System exists - update it with the new image
            QJsonObject existingData = systemInfoData.first().toObject();
            QString existingImages = existingData.value("images").toString();
            QString existingAdditionalImages = existingData.value("additional_images").toString();
            
            QJsonObject updateData;
            
            if (existingImages.isEmpty()) {
                // No primary image yet - set this as primary
                updateData["images"] = imageUrl;
                qDebug() << "Setting as primary image for system:" << systemName;
            } else {
                // Primary image exists - add to additional_images array
                QJsonArray additionalImages;
                
                // Parse existing additional_images if they exist
                if (!existingAdditionalImages.isEmpty()) {
                    QJsonParseError parseError;
                    QJsonDocument doc = QJsonDocument::fromJson(existingAdditionalImages.toUtf8(), &parseError);
                    if (parseError.error == QJsonParseError::NoError && doc.isArray()) {
                        additionalImages = doc.array();
                    }
                }
                
                // Add new image to additional_images array
                additionalImages.append(imageUrl);
                updateData["additional_images"] = QString::fromUtf8(QJsonDocument(additionalImages).toJson(QJsonDocument::Compact));
                qDebug() << "Adding to additional images for system:" << systemName;
            }
            
            // Update the existing record
            QString updateEndpoint = QString("system_information?system=eq.%1").arg(QUrl::toPercentEncoding(systemName));
            QNetworkRequest updateRequest = createRequest(updateEndpoint);
            
            // Add commander headers to update request
            if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
                updateRequest.setRawHeader("X-Commander", m_currentCommander.toUtf8());
                updateRequest.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
            } else {
                updateRequest.setRawHeader("X-Commander", "Regza");
                updateRequest.setRawHeader("x-commander-name", "Regza");
            }
            
            QJsonDocument updateDoc(updateData);
            QNetworkReply *updateReply = m_networkManager->sendCustomRequest(updateRequest, "PATCH", updateDoc.toJson());
            
            if (updateReply) {
                updateReply->setProperty("operation", "UPDATE:system_info_image");
                updateReply->setProperty("systemName", systemName);
                updateReply->setProperty("imageUrl", imageUrl);
                qDebug() << "Updating existing system_information record with image";
            }
        } else {
            // System doesn't exist - create new record
            QJsonObject newData;
            newData["system"] = systemName;
            newData["images"] = imageUrl;
            newData["system_info"] = QString("System images uploaded by user.");
            newData["submitter"] = m_currentCommander.isEmpty() ? "Regza" : m_currentCommander;
            newData["potential_or_poi"] = "Potential POI";
            
            QNetworkRequest insertRequest = createRequest("system_information");
            
            // Add commander headers to insert request
            if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
                insertRequest.setRawHeader("X-Commander", m_currentCommander.toUtf8());
                insertRequest.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
            } else {
                insertRequest.setRawHeader("X-Commander", "Regza");
                insertRequest.setRawHeader("x-commander-name", "Regza");
            }
            
            QJsonDocument insertDoc(newData);
            QNetworkReply *insertReply = m_networkManager->post(insertRequest, insertDoc.toJson());
            
            if (insertReply) {
                insertReply->setProperty("operation", "INSERT:system_info_image");
                insertReply->setProperty("systemName", systemName);
                insertReply->setProperty("imageUrl", imageUrl);
                qDebug() << "Creating new system_information record with image";
            }
        }
        
    } else if (operation.startsWith("UPDATE:system_info_image") || operation.startsWith("INSERT:system_info_image")) {
        QString systemName = reply->property("systemName").toString();
        QString imageUrl = reply->property("imageUrl").toString();
        QString operationType = operation.startsWith("UPDATE") ? "updated" : "created";
        
        qDebug() << "System information" << operationType << "successfully for" << systemName << "with image" << imageUrl;
        
    } else if (operation.startsWith("GET:bulk_system_images")) {
        QJsonArray imagesData = response.value("data").toArray();
        QJsonObject systemImages;
        
        // Convert array to object keyed by system name
        for (const QJsonValue &value : imagesData) {
            QJsonObject imageInfo = value.toObject();
            QString systemName = imageInfo.value("system").toString();
            QString imageUrl = imageInfo.value("images").toString();
            
            if (!systemName.isEmpty() && !imageUrl.isEmpty()) {
                systemImages[systemName] = imageUrl;
            }
        }
        
        qDebug() << "Loaded images for" << systemImages.size() << "systems";
        emit bulkSystemImagesLoaded(systemImages);
        
    } else if (operation.startsWith("CHECK:existing_claim")) {
        QString systemName = reply->property("systemName").toString();
        QString commander = reply->property("commander").toString();
        bool hasVisited = reply->property("hasVisited").toBool();
        QJsonArray existingData = response.value("data").toArray();
        
        if (!existingData.isEmpty()) {
            // Check the current owner of the single system row
            QJsonObject existingClaim = existingData[0].toObject();
            QString claimedBy = existingClaim.value("by_cmdr").toString();
            bool isDone = existingClaim.value("done").toBool();

            // Treat placeholder 'empty' (or blank) as unclaimed
            if (claimedBy.isEmpty() || claimedBy.compare("empty", Qt::CaseInsensitive) == 0) {
                qDebug() << "System" << systemName << "row is unclaimed ('empty') - proceeding to claim";
            } else if (claimedBy == commander) {
                qDebug() << "Claim already exists for" << systemName << "by" << commander << "- not creating duplicate";
                emit systemClaimed(systemName, true); // Already claimed by this user
                return;
            } else if (isDone) {
                qDebug() << "Existing claim is marked as done by" << claimedBy << "- allowing new claim";
            } else {
                qDebug() << "System is actively claimed by another commander - blocking claim (claimedBy:" << claimedBy << ")";
                emit systemClaimed(systemName, false);
                emit networkError(QString("System '%1' is already claimed by %2. You cannot claim systems that belong to other commanders unless they are marked as 'Done'.")
                                .arg(systemName, claimedBy));
                return;
            }
        }
        
        // If we reach here, either no claim exists OR existing claim is done - create new claim
        qDebug() << "Creating new claim for" << systemName;
        
        // Use UPSERT to prevent duplicates - this is the fix!
        QJsonObject claimData;
        claimData["system"] = systemName;
        claimData["by_cmdr"] = commander;
        claimData["visited"] = hasVisited;
        claimData["done"] = false;
        
        // Add resolution header to ignore conflicts (UPSERT behavior)
        // IMPORTANT: PostgREST requires on_conflict when using resolution=ignore-duplicates
        QString upsertEndpoint = "taken?on_conflict=system";
        QNetworkRequest claimRequest = createRequest(upsertEndpoint);
        // Merge into the existing row for this system if it already exists (unique on system)
        claimRequest.setRawHeader("Prefer", "return=representation,resolution=merge-duplicates");
        claimRequest.setRawHeader("X-Commander", commander.toUtf8());
        claimRequest.setRawHeader("x-commander-name", commander.toUtf8());
        
        QJsonDocument claimDoc(claimData);
        QNetworkReply *claimReply = m_networkManager->post(claimRequest, claimDoc.toJson());
        
        if (claimReply) {
            claimReply->setProperty("operation", "POST:taken");
            claimReply->setProperty("systemName", systemName);
            claimReply->setProperty("commander", commander);
            qDebug() << "UPSERT claim request" << upsertEndpoint
                     << "payload:" << QString::fromUtf8(claimDoc.toJson(QJsonDocument::Compact));
            qDebug() << "Creating new claim record for" << systemName << "with duplicate prevention";
        }
        
        // REMOVED: automatic system_information creation to prevent unnecessary duplicates
        // Let users create system_information manually if needed
        
    } else if (operation.startsWith("CHECK:records_before_update")) {
        QString systemName = reply->property("systemName").toString();
        QJsonArray records = response.value("data").toArray();
        
        qDebug() << "=== RECORDS CHECK RESULT ===";
        qDebug() << "System:" << systemName;
        qDebug() << "Records found:" << records.size();
        
        for (const QJsonValue &value : records) {
            QJsonObject record = value.toObject();
            qDebug() << "Record:" << record;
        }
        qDebug() << "=== END RECORDS CHECK ===";
        
    } else if (operation.startsWith("UNCLAIM:mark_empty")) {
        QString systemName = reply->property("systemName").toString();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray responseBytes = reply->readAll();
        qDebug() << "unclaimSystem: HTTP" << httpStatus << "response:" << responseBytes;
        if (httpStatus >= 200 && httpStatus < 300) {
            // Treat any 2xx as success. Some configurations may return 200 with an empty array
            // even when the UPDATE succeeded due to policy/visibility nuances.
            qDebug() << "unclaimSystem: by_cmdr set to 'empty' for" << systemName;
            emit systemUnclaimed(systemName, true);
            QTimer::singleShot(0, this, [this]() { getTakenSystems(); });
        } else {
            qDebug() << "unclaimSystem: PATCH mark empty failed for" << systemName << "HTTP" << httpStatus;
            emit systemUnclaimed(systemName, false);
        }
    } else if (operation.startsWith("CHECK:system_info_exists_on_claim")) {
        QString systemName = reply->property("systemName").toString();
        QString commander = reply->property("commander").toString();
        QJsonArray existingSystemInfo = response.value("data").toArray();
        
        if (existingSystemInfo.isEmpty()) {
            // REMOVED: DO NOT AUTO-CREATE system_information records
            // This was causing the duplicates!
            qDebug() << "No system_information record exists for" << systemName << "- but NOT creating one automatically";
            qDebug() << "Users can create system_information records manually if needed";
        } else {
            qDebug() << "System_information record already exists for" << systemName << "- skipping creation";
        }
        
    } else if (operation.startsWith("IMGBB:test")) {
        
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray responseBody = reply->readAll();
        
        qDebug() << "HTTP Status:" << httpStatus;
        qDebug() << "Response Body:" << responseBody;
        
        if (httpStatus == 200 && !responseBody.isEmpty()) {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);
            
            if (parseError.error == QJsonParseError::NoError) {
                QJsonObject jsonResponse = doc.object();
                bool imgbbSuccess = jsonResponse.value("success").toBool();
                
                if (imgbbSuccess) {
                    qDebug() << "✅ API KEY IS WORKING! ImgBB test upload successful!";
                    qDebug() << "Your API key is valid and ImgBB is accessible.";
                } else {
                    qDebug() << "❌ API key test failed - ImgBB returned success=false";
                    QJsonObject error = jsonResponse.value("error").toObject();
                    qDebug() << "Error:" << error.value("message").toString();
                }
            } else {
                qDebug() << "❌ Invalid JSON response from ImgBB";
            }
        } else if (httpStatus == 400) {
            qDebug() << "❌ HTTP 400 - Your API key is likely invalid or expired";
            qDebug() << "Get a new API key from https://api.imgbb.com/";
        } else {
            qDebug() << "❌ Unexpected response - HTTP" << httpStatus;
            qDebug() << "Response:" << responseBody;
        }
        
        qDebug() << "=== IMGBB API TEST COMPLETE ===";
    }
    
    emit requestCompleted(operation, true, "Success");
    reply->deleteLater();
}

QJsonObject SupabaseClient::parseReply(QNetworkReply *reply, bool &success)
{
    success = false;
    QJsonObject result;
    
    if (reply->error() != QNetworkReply::NoError) {
        result["message"] = reply->errorString();
        return result;
    }
    
    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        result["message"] = parseError.errorString();
        return result;
    }
    
    success = true;
    
    // For Supabase, the response is usually a JSON array directly
    if (doc.isArray()) {
        result["data"] = doc.array();
    } else {
        result = doc.object();
    }
    
    return result;
}

void SupabaseClient::processSystemsReply(const QJsonArray &data)
{
    qDebug() << "Received" << data.size() << "systems from database";
    
    // Transform database format to match our UI expectations
    QJsonArray transformedSystems;
    for (const QJsonValue &value : data) {
        QJsonObject system = value.toObject();
        QJsonObject transformed;
        
        // Map database fields to expected UI fields
        transformed["name"] = system["systems"];  // systems → name
        transformed["category"] = system["category"];
        transformed["x"] = system["x"];
        transformed["y"] = system["y"];
        transformed["z"] = system["z"];
        
        // Calculate distance from Sol (0,0,0) for now
        double x = system["x"].toDouble();
        double y = system["y"].toDouble(); 
        double z = system["z"].toDouble();
        double distance = std::sqrt(x*x + y*y + z*z);
        transformed["distance"] = QString::number(distance, 'f', 1) + " LY";
        
        // Add default values for fields we don't have yet
        transformed["poi"] = "";  // Will be filled from POI table join later
        transformed["claimed"] = false;  // Will be filled from taken table join later
        transformed["done"] = false;
        transformed["claimedBy"] = "";
        
        transformedSystems.append(transformed);
    }
    
    qDebug() << "Transformed" << transformedSystems.size() << "systems for UI";
    emit systemsReceived(transformedSystems);
}

// Database sync implementation
void SupabaseClient::loadSyncState()
{
    QFile file(m_syncStateFile);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error == QJsonParseError::NoError) {
            m_syncState = doc.object();
        } else {
            qDebug() << "Failed to parse sync state file:" << error.errorString();
            m_syncState = QJsonObject(); // Reset to empty
        }
        file.close();
    } else {
        // First run - create empty sync state
        m_syncState = QJsonObject();
        m_syncState["version"] = "1.0";
        m_syncState["first_run"] = true;
        m_syncState["last_sync"] = "";
        m_syncState["tables"] = QJsonObject();
    }
}

void SupabaseClient::saveSyncState()
{
    QFile file(m_syncStateFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(m_syncState);
        file.write(doc.toJson());
        file.close();
    } else {
        qDebug() << "Failed to save sync state file:" << file.errorString();
    }
}

bool SupabaseClient::isFirstRun() const
{
    return m_syncState.value("first_run").toBool(true);
}

void SupabaseClient::initializeDatabaseSync()
{
    if (m_syncInProgress) {
        qDebug() << "Database sync already in progress";
        return;
    }
    
    qDebug() << "Initializing database sync...";
    m_syncInProgress = true;
    
    if (isFirstRun()) {
        emit databaseSyncStatusChanged("Downloading Database");
        qDebug() << "First run detected - performing full database download";
        performFullSync();
    } else {
        emit databaseSyncStatusChanged("Checking Database for changes");
        qDebug() << "Checking for database updates since last sync";
        checkForDatabaseUpdates();
    }
}

void SupabaseClient::checkForDatabaseUpdates()
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for database updates check";
        finalizeDatabaseSync(false, 0);
        return;
    }
    
    QString lastSync = m_syncState.value("last_sync").toString();
    qDebug() << "Checking for updates since:" << lastSync;
    
    // Check each table for updates (simplified approach)
    // In a real implementation, you'd query with timestamps
    
    // For now, we'll check if preset_images table has changes
    // by comparing count or using a simple heuristic
    emit databaseSyncProgress(1, 3, "Checking preset images");
    
    // Start with checking preset images count by selecting id only (lightweight)
    QString endpoint = "preset_images?select=id";
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "CHECK:preset_images_count");
        qDebug() << "Checking preset images count for changes";
    }
}

void SupabaseClient::performIncrementalSync()
{
    qDebug() << "Performing incremental database sync";
    
    // Get only new/updated records since last sync
    QString lastSync = m_syncState.value("last_sync").toString();
    
    if (lastSync.isEmpty()) {
        // No previous sync, do full sync
        performFullSync();
        return;
    }
    
    // Check for changes in each critical table
    emit databaseSyncProgress(1, 4, "Syncing preset images");
    getPresetImages(true);
    
    // Note: In a full implementation, you'd check updated_at timestamps
    // and only fetch records newer than lastSync
}

void SupabaseClient::performFullSync()
{
    qDebug() << "Performing full database sync";
    
    // Download all essential data
    emit databaseSyncProgress(1, 4, "Downloading preset images");
    getPresetImages(true);
    
    emit databaseSyncProgress(2, 4, "Downloading categories");
    getCategories();
    
    emit databaseSyncProgress(3, 4, "Downloading system data");
    // Note: We'll use a smaller subset for initial load
    
    emit databaseSyncProgress(4, 4, "Finalizing");
    
    // Mark as no longer first run
    m_syncState["first_run"] = false;
    saveSyncState();
}

void SupabaseClient::finalizeDatabaseSync(bool success, int changesDetected)
{
    m_syncInProgress = false;
    
    if (success) {
        // Update sync timestamp
        m_syncState["last_sync"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        saveSyncState();
        
        QString message = isFirstRun() ? 
            QString("Database downloaded successfully") :
            QString("Database updated - %1 changes detected").arg(changesDetected);
        
        emit databaseSyncStatusChanged(message);
        qDebug() << message;
    } else {
        emit databaseSyncStatusChanged("Database sync failed - using cached data");
        qDebug() << "Database sync failed";
    }
    
    emit databaseSyncComplete(isFirstRun(), changesDetected);
} 

void SupabaseClient::markSystemAsPOI(const QString &systemName, const QString &poiType, const QString &commander)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for markSystemAsPOI";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (systemName.isEmpty() || poiType.isEmpty() || commander.isEmpty()) {
        emit requestCompleted("markSystemAsPOI", false, "Invalid parameters");
        return;
    }
    
    qDebug() << "Marking system" << systemName << "as" << poiType << "by commander" << commander;
    
    // Set commander context for RLS policies
    setCommanderContext(commander);
    
    // Set potential_or_poi in system_information for immediate card sourcing.
    // Use upsert semantics so missing rows are created with minimal data.
    {
        QString upsertEndpoint = "system_information?on_conflict=system";
        QNetworkRequest upsertReq = createRequest(upsertEndpoint);
        upsertReq.setRawHeader("Prefer", "return=representation,resolution=merge-duplicates");
        if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
            upsertReq.setRawHeader("X-Commander", m_currentCommander.toUtf8());
            upsertReq.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
        }
        QJsonObject upsert;
        upsert["system"] = systemName;
        upsert["potential_or_poi"] = poiType;
        upsert["discoverer"] = commander;
        upsert["submitter"] = commander;
        QNetworkReply *upsertReply = m_networkManager->post(upsertReq, QJsonDocument(upsert).toJson());
        if (upsertReply) {
            upsertReply->setProperty("operation", "UPSERT:system_information_poi_set");
            upsertReply->setProperty("systemName", systemName);
            upsertReply->setProperty("poiType", poiType);
            qDebug() << "UPSERT system_information potential_or_poi for" << systemName << "=" << poiType;
        }
    }
}

void SupabaseClient::removePOIStatus(const QString &systemName, const QString &commander)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for removePOIStatus";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (systemName.isEmpty() || commander.isEmpty()) {
        emit requestCompleted("removePOIStatus", false, "Invalid parameters");
        return;
    }
    
    qDebug() << "Removing POI status for system" << systemName << "by commander" << commander;
    
    // Set commander context for RLS policies
    setCommanderContext(commander);
    
    // Clear potential_or_poi in system_information (primary source used by cards)
    {
        QString clearEndpoint = QString("system_information?system=eq.%1").arg(QUrl::toPercentEncoding(systemName));
        QNetworkRequest clearReq = createRequest(clearEndpoint);
        if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
            clearReq.setRawHeader("X-Commander", m_currentCommander.toUtf8());
            clearReq.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
        }
        QJsonObject patchObj; patchObj["potential_or_poi"] = QJsonValue::Null;
        QNetworkReply *clearReply = m_networkManager->sendCustomRequest(clearReq, "PATCH", QJsonDocument(patchObj).toJson());
        if (clearReply) {
            clearReply->setProperty("operation", "UPDATE:system_information_poi_clear");
            clearReply->setProperty("systemName", systemName);
            qDebug() << "PATCH system_information set potential_or_poi = null for" << systemName;
        }
    }

    // Also clean up any row from the auxiliary pois table if present
    QString endpoint = QString("pois?system=eq.%1").arg(QUrl::toPercentEncoding(systemName));
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->deleteResource(request);
    
    if (reply) {
        reply->setProperty("operation", "DELETE:pois");
        reply->setProperty("systemName", systemName);
        qDebug() << "removePOIStatus: Request sent, operation tagged as DELETE:pois";
    } else {
        qDebug() << "removePOIStatus: Failed to create network request!";
        emit requestCompleted("removePOIStatus", false, "Network request failed");
    }
}

void SupabaseClient::saveSystemDescription(const QString &systemName, const QString &description)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for saveSystemDescription";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (systemName.isEmpty()) {
        emit requestCompleted("saveSystemDescription", false, "Invalid system name");
        return;
    }
    
    qDebug() << "Saving description for system" << systemName;
    
    QJsonObject descData;
    descData["system"] = systemName;
    descData["description"] = description;
    // Add commander directly to the data so database can access it
    descData["commander"] = m_currentCommander;
    
    QNetworkRequest request = createRequest("system_information");
    
    // Add commander context for RLS policy
    if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
        request.setRawHeader("X-Commander", m_currentCommander.toUtf8());
        qDebug() << "Added commander header for system description save:" << m_currentCommander;
    }
    
    QJsonDocument doc(descData);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    
    if (reply) {
        reply->setProperty("operation", "POST:system_information");
        reply->setProperty("systemName", systemName);
        qDebug() << "saveSystemDescription: Request sent, operation tagged as POST:system_information";
    } else {
        qDebug() << "saveSystemDescription: Failed to create network request!";
        emit requestCompleted("saveSystemDescription", false, "Network request failed");
    }
}
void SupabaseClient::testImgbbApiKey()
{
    qDebug() << "=== TESTING IMGBB API KEY ===";
    
    // Create a small test image (1x1 pixel red PNG)
    QByteArray testImageData = QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg=="
    );
    
    QString base64Image = testImageData.toBase64();
    QString apiKey = "8df93308e43e8a90de4b3a1219f07956";
    
    QUrl imgbbUrl("https://api.imgbb.com/1/upload");
    QNetworkRequest request(imgbbUrl);
    // Force HTTP/1.1; some CDN edges return HTTP/2 protocol errors on POST
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader, "EDRH-Test/1.0");
    
    QUrlQuery postData;
    postData.addQueryItem("key", apiKey);
    postData.addQueryItem("image", base64Image);
    postData.addQueryItem("name", "api_test");
    
    QByteArray formData = postData.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply *reply = m_networkManager->post(request, formData);
    if (reply) {
        // Manual timeout for Qt 6.9
        QTimer *timeoutTimer = new QTimer(reply);
        timeoutTimer->setSingleShot(true);
        timeoutTimer->setInterval(30000);
        QObject::connect(timeoutTimer, &QTimer::timeout, reply, [reply]() {
            qWarning() << "IMGBB upload timed out";
            reply->abort();
        });
        timeoutTimer->start();
    }
    
    if (reply) {
        reply->setProperty("operation", "IMGBB:test");
        qDebug() << "ImgBB API test request sent";
    }
}

void SupabaseClient::getPresetImageForCategory(const QString &category)
{
    if (category.isEmpty()) {
        emit systemImageSet("", "", false);
        return;
    }
    
    // Check cache first
    if (m_categoryImageCache.contains(category)) {
        QString cachedUrl = m_categoryImageCache.value(category);
        qDebug() << "Using cached preset image for category" << category << ":" << cachedUrl;
        // We don't know which system requested this, so just cache it
        return;
    }
    
    qDebug() << "Fetching preset image for category:" << category;
    
    // Query preset_images table for this category
    QString endpoint = QString("preset_images?select=*&category=eq.%1&limit=1").arg(category);
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:preset_image_category");
        reply->setProperty("category", category);
    }
}

void SupabaseClient::usePresetImageForSystem(const QString &systemName, const QString &category)
{
    if (systemName.isEmpty() || category.isEmpty()) {
        emit systemImageSet(systemName, "", false);
        return;
    }
    
    // Check if we already have this category's image cached
    if (m_categoryImageCache.contains(category)) {
        QString imageUrl = m_categoryImageCache.value(category);
        qDebug() << "Using cached preset image for system" << systemName << "category" << category << ":" << imageUrl;
        emit presetImageFound(systemName, imageUrl, category);
        emit systemImageSet(systemName, imageUrl, true);
        return;
    }
    
    // Store the pending request so we know which system to update when we get the result
    m_pendingPresetRequests[category] = systemName;
    
    // Fetch the preset image for this category
    getPresetImageForCategory(category);
}

QString SupabaseClient::getSystemImageUrl(const QString &systemName, const QString &category)
{
    // Check for user override first
    if (m_systemImageOverrides.contains(systemName)) {
        return m_systemImageOverrides.value(systemName);
    }
    
    // Check category cache
    if (!category.isEmpty() && m_categoryImageCache.contains(category)) {
        return m_categoryImageCache.value(category);
    }
    
    return QString(); // Empty string if no image found
}

// Add this function to your supabaseclient.cpp file - REPLACE the existing broken one
void SupabaseClient::uploadImageToImgbb(const QString &filePath, const QString &systemName)
{
    startImgbbUpload(filePath, systemName, 1);
}

void SupabaseClient::startImgbbUpload(const QString &filePath, const QString &systemName, int attempt)
{
    if (filePath.isEmpty() || systemName.isEmpty()) {
        emit requestCompleted("uploadImageToImgbb", false, "Invalid parameters");
        return;
    }

    // Strip the file:// prefix if present
    QString cleanFilePath = filePath;
    if (cleanFilePath.startsWith("file:///")) {
        cleanFilePath = cleanFilePath.mid(8); // Remove "file:///"
    } else if (cleanFilePath.startsWith("file://")) {
        cleanFilePath = cleanFilePath.mid(7); // Remove "file://"
    }

    qDebug() << "=== IMGBB UPLOAD DEBUG START ===";
    qDebug() << "Original file path:" << filePath;
    qDebug() << "Cleaned file path:" << cleanFilePath;
    qDebug() << "System name:" << systemName;

    // Read the image file
    QFile file(cleanFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QString error = QString("Failed to read file: %1").arg(file.errorString());
        qDebug() << "FILE READ ERROR:" << error;
        qDebug() << "File exists:" << QFileInfo::exists(cleanFilePath);
        qDebug() << "File permissions:" << QFileInfo(cleanFilePath).permissions();
        emit requestCompleted("uploadImageToImgbb", false, error);
        return;
    }

    QByteArray imageData = file.readAll();
    file.close();

    if (imageData.isEmpty()) {
        qDebug() << "ERROR: Image file is empty";
        emit requestCompleted("uploadImageToImgbb", false, "Image file is empty");
        return;
    }

    // Check file size (ImgBB has a 32MB limit)
    const qint64 maxSize = 32 * 1024 * 1024; // 32MB
    qDebug() << "Image file size:" << imageData.size() << "bytes (max:" << maxSize << ")";

    if (imageData.size() > maxSize) {
        emit requestCompleted("uploadImageToImgbb", false, "Image file too large (max 32MB)");
        return;
    }

    // Encode image to base64
    QString base64Image = imageData.toBase64();
    qDebug() << "Base64 encoding complete. Length:" << base64Image.length();

    // Your ImgBB API key (you should store this in config, not hardcode it)
    QString apiKey = "8df93308e43e8a90de4b3a1219f07956";
    qDebug() << "Using API key (first 10 chars):" << apiKey.left(10) + "...";

    // === THE CRITICAL FIX IS HERE ===
    // Create the ImgBB request properly with optimizations
    QUrl imgbbUrl("https://api.imgbb.com/1/upload");
    QNetworkRequest request(imgbbUrl);
    // OPTIMIZATION: Allow HTTP/2 for better performance
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    // OPTIMIZATION: Keep alive connections for faster subsequent uploads
    request.setRawHeader("Connection", "keep-alive");
    // OPTIMIZATION: Prefer cached responses
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);

    // CRITICAL: Set the correct Content-Type for form data
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader, "EDRH-ImageUploader/1.0");
    request.setRawHeader("Accept", "application/json");
    
    // Timeout handled at QNAM level; QNetworkRequest::setTransferTimeout not available on Qt 6.9

    // Build form data manually
    // Don't use QUrlQuery as it might be encoding incorrectly
    QString formDataString = QString("key=%1&image=%2")
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(apiKey)))
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(base64Image)));
    
    QByteArray formData = formDataString.toUtf8();
    
    // Additional debug logging
    qDebug() << "First 200 chars of form data:" << formDataString.left(200);

    qDebug() << "Form data size:" << formData.size() << "bytes";
    qDebug() << "Sending request to ImgBB...";

    // Send the POST request
    QNetworkReply *reply = m_networkManager->post(request, formData);

    if (reply) {
        reply->setProperty("operation", "IMGBB:upload");
        reply->setProperty("systemName", systemName);
        reply->setProperty("filePath", filePath);
        reply->setProperty("attempt", attempt);
        
        // Add debug tracking for reply lifecycle
        qDebug() << "=== IMGBB REPLY LIFECYCLE DEBUG ===";
        qDebug() << "Reply created successfully, pointer:" << reply;
        qDebug() << "Reply URL:" << reply->url().toString();
        qDebug() << "Reply operation:" << reply->property("operation").toString();
        
        // Connect to additional debugging signals
        connect(reply, &QNetworkReply::errorOccurred, [=](QNetworkReply::NetworkError error) {
            qDebug() << "IMGBB REPLY ERROR:" << error << "Message:" << reply->errorString();
        });
        
        connect(reply, &QNetworkReply::sslErrors, [=](const QList<QSslError> &errors) {
            qDebug() << "IMGBB SSL ERRORS:" << errors.size();
            for (const auto& err : errors) {
                qDebug() << "SSL Error:" << err.errorString();
            }
        });
        
        connect(reply, &QNetworkReply::downloadProgress, [=](qint64 received, qint64 total) {
            qDebug() << "IMGBB Download progress:" << received << "/" << total;
        });
        
        connect(reply, &QNetworkReply::uploadProgress, [=](qint64 sent, qint64 total) {
            if (sent % 1000000 == 0 || sent == total) { // Log every MB or at completion
                qDebug() << "IMGBB Upload progress:" << sent << "/" << total;
                if (sent == total) {
                    qDebug() << "IMGBB Upload completed, waiting for server response...";
                }
            }
        });
        
        // Add timeout detection and proper error handling
        QTimer::singleShot(30000, reply, [=]() {
            if (!reply->isFinished()) {
                qDebug() << "IMGBB TIMEOUT - No response after 30 seconds, aborting";
                qDebug() << "Reply state: finished=" << reply->isFinished() << " error=" << reply->error();
                reply->abort();
                
                // Notify UI of failure through requestCompleted signal
                qDebug() << "Emitting requestCompleted with timeout error";
                emit requestCompleted("uploadImageToImgbb", false, "ImgBB timeout: No response after 30 seconds. Server may be overloaded, try again.");
            }
        });
        
        qDebug() << "Request sent to imgbb API";
        qDebug() << "=== END IMGBB REPLY LIFECYCLE DEBUG ===";
        // One-shot retry on protocol failure/timeouts
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            QString op = reply->property("operation").toString();
            if (!op.startsWith("IMGBB:upload")) return;
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QNetworkReply::NetworkError nerr = reply->error();
            int attemptProp = reply->property("attempt").toInt();
            if ((nerr == QNetworkReply::ProtocolFailure || httpStatus == 0 || nerr == QNetworkReply::TimeoutError) && attemptProp < IMGBB_MAX_ATTEMPTS) {
                QString system = reply->property("systemName").toString();
                QString path = reply->property("filePath").toString();
                qWarning() << "IMGBB upload failed, retrying with fresh connection. Attempt" << (attemptProp+1);
                QTimer::singleShot(200, this, [this, path, system, attemptProp]() {
                    startImgbbUpload(path, system, attemptProp + 1);
                });
            }
        });
    } else {
        qDebug() << "Failed to create ImgBB request!";
        emit requestCompleted("uploadImageToImgbb", false, "Failed to create network request");
    }

    qDebug() << "=== IMGBB UPLOAD DEBUG END ===";
}

// Authentication Methods - following v1.4incomplete.py pattern
void SupabaseClient::performSecurityCheck(const QString &commanderName)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for security check";
        emit authenticationComplete(false, "Supabase not configured");
        return;
    }
    
    if (commanderName.isEmpty() || commanderName == "Unknown") {
        qDebug() << "Cannot perform security check with empty or unknown commander name";
        emit authenticationComplete(false, "Invalid commander name");
        return;
    }
    
    qDebug() << "[DEBUG] Security check for:" << commanderName;
    
    // Query security table for this commander
    QString endpoint = QString("security?name=eq.%1&select=name,blocked,notes").arg(commanderName);
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("operation", "security_check");
    reply->setProperty("commander_name", commanderName);
}

void SupabaseClient::handleNewCommander(const QString &commanderName, const QStringList &allCommanders)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        return;
    }
    
    qDebug() << "[DEBUG]" << commanderName << "not in security table - adding as new user";
    
    bool hasBannedCommanders = false;
    QStringList blockedCommanders;
    QString renameInfo;
    
    // Check if any other commanders in journal are banned
    if (allCommanders.size() > 1) {
        for (const QString &otherCmdr : allCommanders) {
            if (otherCmdr != commanderName) {
                // Query if this other commander is blocked
                QString endpoint = QString("security?name=eq.%1&blocked=eq.true&select=name").arg(otherCmdr);
                QNetworkRequest request = createRequest(endpoint);
                QNetworkReply *reply = m_networkManager->get(request);
                reply->setProperty("operation", "check_banned_alt");
                reply->setProperty("commander_name", commanderName);
                reply->setProperty("alt_commander", otherCmdr);
            }
        }
        
        if (blockedCommanders.isEmpty()) {
            renameInfo = QString("Multiple commanders detected (legitimate alts): %1").arg(allCommanders.join(", "));
        }
    }
    
    // Add new commander to security table (will be updated after checking alts)
    QJsonObject securityData;
    securityData["name"] = commanderName;
    securityData["blocked"] = hasBannedCommanders;
    securityData["first_seen"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!renameInfo.isEmpty()) {
        securityData["notes"] = renameInfo;
    }
    
    QNetworkRequest request = createRequest("security");
    QJsonDocument doc(securityData);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    reply->setProperty("operation", "add_new_commander");
    reply->setProperty("commander_name", commanderName);
}

void SupabaseClient::detectCommanderRenames(const QString &journalPath)
{
    QStringList allCommanders;
    QStringList switchUserEvents;
    
    qDebug() << "[DEBUG] Scanning journal files for all commanders in:" << journalPath;
    
    QDir journalDir(journalPath);
    if (!journalDir.exists()) {
        qDebug() << "[ERROR] Journal directory does not exist:" << journalPath;
        emit confirmationRequired("Commander Detection", "No journal directory found", "Unknown");
        return;
    }
    
    // Get all journal files, sorted by date (newest first)
    QStringList filters;
    filters << "Journal.*.log";
    QFileInfoList journalFiles = journalDir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Time);
    
    qDebug() << "[DEBUG] Found" << journalFiles.size() << "journal files to scan";
    
    QMap<QString, QJsonObject> commanderData; // commander name -> {fid, first_seen, sessions}
    
    // Scan through journals (limit to recent ones for performance)
    for (int i = 0; i < qMin(50, journalFiles.size()); i++) {
        const QFileInfo &fileInfo = journalFiles[i];
        QString journalFile = fileInfo.absoluteFilePath();
        QString fileName = fileInfo.fileName();
        
        qDebug() << "[DEBUG] Scanning journal:" << fileName;
        
        QFile file(journalFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "[DEBUG] Error reading" << fileName;
            continue;
        }
        
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty()) continue;
            
            // Look for SwitchUser events
            if (line.contains("\"event\":\"SwitchUser\"")) {
                switchUserEvents.append(fileName + ": SwitchUser event");
                qDebug() << "[DEBUG] Found SwitchUser event in" << fileName;
            }
            
            // Look for LoadGame events with Commander info
            if (line.contains("\"event\":\"LoadGame\"") && line.contains("\"Commander\"")) {
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
                
                if (parseError.error == QJsonParseError::NoError) {
                    QJsonObject data = doc.object();
                    QString commanderName = data.value("Commander").toString();
                    QString fid = data.value("FID").toString();
                    
                    if (!commanderName.isEmpty()) {
                        if (!commanderData.contains(commanderName)) {
                            qDebug() << "[DEBUG] Found commander:" << commanderName << "in" << fileName;
                            allCommanders.append(commanderName);
                            
                            QJsonObject cmdrInfo;
                            cmdrInfo["fid"] = fid;
                            cmdrInfo["first_seen"] = fileName;
                            cmdrInfo["sessions"] = 1;
                            commanderData[commanderName] = cmdrInfo;
                        } else {
                            // Increment session count
                            QJsonObject cmdrInfo = commanderData[commanderName];
                            cmdrInfo["sessions"] = cmdrInfo["sessions"].toInt() + 1;
                            commanderData[commanderName] = cmdrInfo;
                        }
                    }
                }
            }
            
            // Also check Commander events
            if (line.contains("\"event\":\"Commander\"") && line.contains("\"Name\"")) {
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
                
                if (parseError.error == QJsonParseError::NoError) {
                    QJsonObject data = doc.object();
                    QString commanderName = data.value("Name").toString();
                    
                    if (!commanderName.isEmpty() && !allCommanders.contains(commanderName)) {
                        qDebug() << "[DEBUG] Found commander via Commander event:" << commanderName << "in" << fileName;
                        allCommanders.append(commanderName);
                        
                        QJsonObject cmdrInfo;
                        cmdrInfo["fid"] = "Unknown";
                        cmdrInfo["first_seen"] = fileName;
                        cmdrInfo["sessions"] = 1;
                        commanderData[commanderName] = cmdrInfo;
                    }
                }
            }
        }
        
        file.close();
    }
    
    // Remove duplicates and prepare display message
    allCommanders.removeDuplicates();
    
    QString message;
    QString currentCommander;
    
    if (allCommanders.isEmpty()) {
        message = "No commanders found in journal files";
        currentCommander = "Unknown";
    } else if (allCommanders.size() == 1) {
        currentCommander = allCommanders.first();
        message = QString("Single commander detected: %1").arg(currentCommander);
    } else {
        // For multiple commanders, the first one found (from most recent journal) is current
        currentCommander = allCommanders.first();
        message = QString("Multiple commanders detected: %1\n").arg(allCommanders.join(", "));
        message += QString("Current commander: %1\n").arg(currentCommander);
        
        if (!switchUserEvents.isEmpty()) {
            message += QString("Found %1 account switch events - likely legitimate alts\n").arg(switchUserEvents.size());
        }
        
        // Show unique FIDs info
        QStringList uniqueFids;
        for (const QString &cmdr : allCommanders) {
            QString fid = commanderData[cmdr]["fid"].toString();
            if (!fid.isEmpty() && fid != "Unknown" && !uniqueFids.contains(fid)) {
                uniqueFids.append(fid);
            }
        }
        
        if (uniqueFids.size() > 1) {
            message += QString("Multiple unique FIDs found (%1) - these are legitimate alt accounts").arg(uniqueFids.size());
        } else if (uniqueFids.size() == 1) {
            message += "Same FID detected - likely same player with multiple saves";
        }
    }
    
    qDebug() << "[INFO] Commander detection complete:" << allCommanders.join(", ");
    qDebug() << "[INFO] Current commander:" << currentCommander;
    
    // Store the detected commanders for use in authentication
    m_detectedCommanders = allCommanders;
    m_currentCommander = currentCommander;
    
    // Emit confirmation required with all the info
    qDebug() << "[DEBUG] About to emit confirmationRequired signal with message:" << message;
    emit confirmationRequired("Commander Detection", message, currentCommander);
    qDebug() << "[DEBUG] confirmationRequired signal emitted";
}

void SupabaseClient::logLoginEvent(const QString &commanderName, bool isAdmin, const QString &eventType, const QString &details)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        return;
    }
    
    QJsonObject loginData;
    loginData["commander"] = commanderName;
    loginData["is_admin"] = isAdmin;
    loginData["login_time"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    loginData["app_version"] = "0.9.5";
    loginData["event_type"] = eventType;
    if (!details.isEmpty()) {
        loginData["details"] = details;
    }
    loginData["webhook_id"] = "https://discord.com/api/webhooks/1386234211928903681/uQB4XGehER9Bq4kRtJvcPuZq5nFeaQzlcjyVPVLrsaFwITpd9tYdEzL7AqkBBts6sdV2";
    
    QNetworkRequest request = createRequest("login_events");
    QJsonDocument doc(loginData);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    reply->setProperty("operation", "log_login_event");
    
    qDebug() << "[DEBUG] Logged" << eventType << "event for" << commanderName;
}

bool SupabaseClient::isCommanderBlocked(const QString &commanderName)
{
    // This will be set after security check completes
    // For now return false, actual check happens in performSecurityCheck
    return false;
}

void SupabaseClient::addCommanderToSecurity(const QString &commanderName, bool blocked, const QString &notes, const QString &journalPath)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        return;
    }
    
    QJsonObject securityData;
    securityData["name"] = commanderName;
    securityData["blocked"] = blocked;
    securityData["first_seen"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!notes.isEmpty()) {
        securityData["notes"] = notes;
    }
    if (!journalPath.isEmpty()) {
        securityData["journal_path"] = journalPath;
    }
    
    QNetworkRequest request = createRequest("security");
    QJsonDocument doc(securityData);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    reply->setProperty("operation", "add_to_security");
    reply->setProperty("commander_name", commanderName);
    
    if (blocked) {
        qDebug() << "[SECURITY] Added" << commanderName << "to security table as BLOCKED";
    } else {
        qDebug() << "[DEBUG] Added" << commanderName << "to security table as ALLOWED";
    }
}

void SupabaseClient::saveImageToDatabase(const QString &systemName, const QString &imageUrl)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for saveImageToDatabase";
        return;
    }
    
    if (systemName.isEmpty() || imageUrl.isEmpty()) {
        qDebug() << "Invalid parameters for saveImageToDatabase";
        return;
    }
    
    qDebug() << "Saving image to database - System:" << systemName << "URL:" << imageUrl;
    
    // First, check if system exists in system_information table
    QString checkEndpoint = QString("system_information?select=id,images,additional_images&system=eq.%1").arg(QUrl::toPercentEncoding(systemName));
    QNetworkRequest checkRequest = createRequest(checkEndpoint);
    
    // Add commander header for RLS policies
    if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
        checkRequest.setRawHeader("X-Commander", m_currentCommander.toUtf8());
        checkRequest.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
        qDebug() << "Added commander headers for image save:" << m_currentCommander;
    } else {
        // Use default commander if none set
        checkRequest.setRawHeader("X-Commander", "Regza");
        checkRequest.setRawHeader("x-commander-name", "Regza");
        qDebug() << "Using default commander 'Regza' for image save";
    }
    
    QNetworkReply *checkReply = m_networkManager->get(checkRequest);
    
    if (checkReply) {
        checkReply->setProperty("operation", "CHECK:system_info_for_image");
        checkReply->setProperty("systemName", systemName);
        checkReply->setProperty("imageUrl", imageUrl);
        qDebug() << "Checking if system exists in system_information table";
    }
}

void SupabaseClient::updateSystemImages(const QString &systemName, const QString &primaryImage, 
                                       const QString &primaryTitle, const QString &additionalImages, 
                                       const QString &additionalTitles)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for updateSystemImages";
        return;
    }
    
    if (systemName.isEmpty()) {
        qDebug() << "Invalid system name for updateSystemImages";
        return;
    }
    
    qDebug() << "Updating images for system:" << systemName;
    
    // Smart validation - clean corruption but don't block valid saves
    if (primaryImage.contains("[object Object]")) {
        qDebug() << "WARNING: Cleaning corrupted primaryImage:" << primaryImage;
        // Don't return/block - just clean the corruption and continue
    }
    
    if (additionalImages.contains("[object Object]")) {
        qDebug() << "WARNING: Cleaning corrupted additionalImages:" << additionalImages;
        // Don't return/block - just log and continue
    }
    
    qDebug() << "UPLOAD DEBUG: Backend processing save for system:" << systemName;
    
    // Debug: Log what we're actually saving
    qDebug() << "=== BACKEND VALIDATION PASSED ===";
    qDebug() << "primaryImage value:" << primaryImage;
    
    qDebug() << "Input validation passed, proceeding with database update";
    qDebug() << "primaryImage:" << primaryImage;
    qDebug() << "additionalImages:" << additionalImages;
    
    // Prepare update data
    QJsonObject updateData;
    updateData["images"] = primaryImage;
    updateData["image_title"] = primaryTitle;
    updateData["additional_images"] = additionalImages;
    
    // Parse additional_image_titles - it should be a JSON object string
    if (!additionalTitles.isEmpty()) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(additionalTitles.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            updateData["additional_image_titles"] = doc.object();
        } else {
            qDebug() << "Failed to parse additional_image_titles:" << parseError.errorString();
            qDebug() << "Raw additionalTitles string:" << additionalTitles;
            // Fallback to null if parsing fails
            updateData["additional_image_titles"] = QJsonValue::Null;
        }
    } else {
        updateData["additional_image_titles"] = QJsonValue::Null;
    }
    
    // Update the system_information table
    QString endpoint = QString("system_information?system=eq.%1").arg(QUrl::toPercentEncoding(systemName));
    QNetworkRequest request = createRequest(endpoint);
    
    // Add commander header for RLS policies
    if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
        request.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
    }
    
    QJsonDocument doc(updateData);
    QByteArray jsonData = doc.toJson();
    
    qDebug() << "UPDATE:system_images payload:" << QString::fromUtf8(jsonData);
    
    QNetworkReply *reply = m_networkManager->sendCustomRequest(request, "PATCH", jsonData);
    
    if (reply) {
        reply->setProperty("operation", "UPDATE:system_images");
        reply->setProperty("systemName", systemName);
        qDebug() << "System images update request sent";
    }
}

void SupabaseClient::loadSystemImagesForSystems(const QStringList &systemNames)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for loadSystemImagesForSystems";
        return;
    }
    
    if (systemNames.isEmpty()) {
        qDebug() << "No systems to load images for";
        return;
    }
    
    qDebug() << "Loading images for" << systemNames.size() << "systems";
    
    // Build filter for multiple systems
    QStringList systemFilters;
    for (const QString &name : systemNames) {
        systemFilters.append(QString("system.eq.%1").arg(QUrl::toPercentEncoding(name)));
    }
    
    // Query system_information for all these systems
    QString endpoint = QString("system_information?select=system,images&or=(%1)").arg(systemFilters.join(","));
    QNetworkRequest request = createRequest(endpoint);
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:bulk_system_images");
        qDebug() << "Bulk system images request sent";
    }
}

bool SupabaseClient::checkSystemVisitedInJournal(const QString &systemName, const QString &commanderName)
{
    if (systemName.isEmpty() || commanderName.isEmpty()) {
        qDebug() << "Invalid parameters for journal visit check";
        return false;
    }
    
    qDebug() << "Checking if" << commanderName << "visited" << systemName << "in journal logs";
    
    // Get the journal directory (this should be configurable, but for now use default)
    QString journalPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Frontier Developments/Elite Dangerous";
    QDir journalDir(journalPath);
    
    if (!journalDir.exists()) {
        qDebug() << "Journal directory does not exist:" << journalPath;
        return false;
    }
    
    // Get all journal files, sorted by date (newest first)
    QStringList filters;
    filters << "Journal.*.log";
    QFileInfoList journalFiles = journalDir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Time);
    
    qDebug() << "Scanning" << journalFiles.size() << "journal files for visit to" << systemName;
    
    // Scan through recent journals (limit to last 20 files for performance)
    for (int i = 0; i < qMin(20, journalFiles.size()); i++) {
        const QFileInfo &fileInfo = journalFiles[i];
        QString journalFile = fileInfo.absoluteFilePath();
        
        QFile file(journalFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty()) continue;
            
            // Check for FSDJump or CarrierJump events to this system
            if ((line.contains("\"event\":\"FSDJump\"") || line.contains("\"event\":\"CarrierJump\"")) && 
                line.contains(QString("\"%1\"").arg(systemName))) {
                
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
                
                if (parseError.error == QJsonParseError::NoError) {
                    QJsonObject data = doc.object();
                    QString jumpSystem = data.value("StarSystem").toString();
                    QString commander = data.value("Commander").toString();
                    
                    // Check if this is the right system and commander
                    if (jumpSystem == systemName && 
                        (commander.isEmpty() || commander == commanderName)) {
                        qDebug() << "Found visit to" << systemName << "by" << commanderName << "in journal";
                        file.close();
                        return true;
                    }
                }
            }
        }
        
        file.close();
    }
    
    qDebug() << "No visit found for" << systemName << "by" << commanderName << "in recent journals";
    return false;
}

void SupabaseClient::getTakenSystemForCommander(const QString &systemName, const QString &commanderName)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for getTakenSystemForCommander";
        emit networkError("Supabase not configured");
        return;
    }
    
    if (systemName.isEmpty() || commanderName.isEmpty()) {
        qDebug() << "Empty parameters for getTakenSystemForCommander";
        return;
    }
    
    qDebug() << "Querying taken table for system:" << systemName << "commander:" << commanderName;
    
    // Query the taken table for this specific system and commander
    // IMPORTANT: Explicitly include the 'edited' column in the select
    QString endpoint = QString("taken?system=eq.%1&by_cmdr=eq.%2&select=id,system,by_cmdr,visited,done,edited")
                      .arg(QUrl::toPercentEncoding(systemName))
                      .arg(QUrl::toPercentEncoding(commanderName));
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:taken_system_specific");
        reply->setProperty("systemName", systemName);
        reply->setProperty("commanderName", commanderName);
        qDebug() << "getTakenSystemForCommander: Request sent for" << systemName << "with edited flag";
    } else {
        qDebug() << "getTakenSystemForCommander: Failed to create network request!";
        emit networkError("Failed to create taken system request");
    }
}



void SupabaseClient::fetchAndMergePOIData(QJsonArray &systemsArray)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for fetchAndMergePOIData";
        return;
    }
    
    if (systemsArray.isEmpty()) {
        qDebug() << "No systems to fetch POI data for";
        return;
    }
    
    qDebug() << "Fetching POI data for" << systemsArray.size() << "systems";
    
    // Fetch all POI data from system_information table
    QString endpoint = "system_information?select=system,potential_or_poi,discoverer,submitter";
    
    QNetworkRequest request = createRequest(endpoint);
    // Don't add commander headers - POI data should be public
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    if (reply) {
        reply->setProperty("operation", "GET:poi_data_for_merge");
        reply->setProperty("systemsCount", systemsArray.size());
        // Store the target systems we want to merge back into
        QJsonDocument systemsDoc(systemsArray);
        reply->setProperty("systemsToMerge", systemsDoc.toJson());
        qDebug() << "fetchAndMergePOIData: Request sent for POI data";
    } else {
        qDebug() << "fetchAndMergePOIData: Failed to create network request!";
    }
}

void SupabaseClient::saveSystemInformation(const QString &systemName, const QVariantMap &information)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for saveSystemInformation";
        emit requestCompleted("saveSystemInformation", false, "Database not configured");
        return;
    }
    
    if (systemName.isEmpty()) {
        qDebug() << "Invalid system name for saveSystemInformation";
        emit requestCompleted("saveSystemInformation", false, "System name cannot be empty");
        return;
    }
    
    qDebug() << "=== HELLO my name is burger - C++ SAVE START ===";
    qDebug() << "Saving system information for" << systemName;
    qDebug() << "Information:" << information;
    qDebug() << "=== END HELLO my name is burger - C++ SAVE START ===";
    
    // First check if record exists
    QString checkEndpoint = QString("system_information?system=eq.%1&select=id")
                            .arg(QUrl::toPercentEncoding(systemName));
    
    QNetworkRequest checkRequest = createRequest(checkEndpoint);
    
    // Add commander headers for RLS policies
    if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
        checkRequest.setRawHeader("X-Commander", m_currentCommander.toUtf8());
        checkRequest.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
    }
    
    QNetworkReply *checkReply = m_networkManager->get(checkRequest);
    
    if (checkReply) {
        checkReply->setProperty("operation", "CHECK:system_info_exists");
        checkReply->setProperty("systemName", systemName);
        checkReply->setProperty("information", information);
        qDebug() << "Checking if system_information exists for" << systemName;
    } else {
        qDebug() << "Failed to create check request!";
        emit requestCompleted("saveSystemInformation", false, "Failed to create check request");
    }
}

void SupabaseClient::updateSystemPOIStatus(const QString &systemName, const QString &poiType, const QString &discoverer, const QString &submitter)
{
    if (m_supabaseUrl.isEmpty() || m_supabaseKey.isEmpty()) {
        qDebug() << "Supabase not configured for updateSystemPOIStatus";
        emit requestCompleted("updateSystemPOIStatus", false, "Database not configured");
        return;
    }
    
    if (systemName.isEmpty()) {
        qDebug() << "Invalid system name for updateSystemPOIStatus";
        emit requestCompleted("updateSystemPOIStatus", false, "System name cannot be empty");
        return;
    }
    
    qDebug() << "Updating POI status for" << systemName << "type:" << poiType << "discoverer:" << discoverer << "submitter:" << submitter;
    
    // Check if system_information record exists first
    QString checkEndpoint = QString("system_information?system=eq.%1&select=id")
                            .arg(QUrl::toPercentEncoding(systemName));
    
    QNetworkRequest checkRequest = createRequest(checkEndpoint);
    
    // Add commander headers for RLS policies
    if (!m_currentCommander.isEmpty() && m_currentCommander != "Unknown") {
        checkRequest.setRawHeader("X-Commander", m_currentCommander.toUtf8());
        checkRequest.setRawHeader("x-commander-name", m_currentCommander.toUtf8());
    }
    
    QNetworkReply *checkReply = m_networkManager->get(checkRequest);
    
    if (checkReply) {
        checkReply->setProperty("operation", "CHECK:poi_system_exists");
        checkReply->setProperty("systemName", systemName);
        checkReply->setProperty("poiType", poiType);
        checkReply->setProperty("discoverer", discoverer);
        checkReply->setProperty("submitter", submitter);
        qDebug() << "Checking if system_information exists for POI update:" << systemName;
    } else {
        qDebug() << "Failed to create POI check request!";
        emit requestCompleted("updateSystemPOIStatus", false, "Failed to create check request");
    }
}
