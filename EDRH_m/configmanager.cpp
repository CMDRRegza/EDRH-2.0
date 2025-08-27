#include "configmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QCoreApplication>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_supabaseAuthConfirmation(false)
    , m_journalVerified(false)
    , m_isAdmin(false)
{
    initializeDefaults();
    m_configFilePath = getConfigPath();
    
    // Don't load config automatically - let QML control timing for splash screen
    // loadConfig() will be called when needed from QML
}

void ConfigManager::initializeDefaults()
{
    m_commanderName = "Unknown";
    m_journalPath = getDefaultJournalPath(); // Use auto-detection instead of hardcoded path
    m_currentSystem = "Unknown";
    m_supabaseUrl = "";
    m_supabaseKey = "";
    m_supabaseAuthConfirmation = false;
    m_journalVerified = false;
    m_isAdmin = false;
    m_adminServiceKey = "";
}

QString ConfigManager::getConfigPath() const
{
    // Get the directory where the application is running
    QString appDir = QCoreApplication::applicationDirPath();
    return QDir(appDir).filePath("config.json");
}

QString ConfigManager::getDefaultJournalPath() const
{
    // Try standard Elite Dangerous locations for any user (same as journalmonitor.cpp)
    QStringList possiblePaths = {
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Frontier Developments/Elite Dangerous",
        QDir::homePath() + "/Saved Games/Frontier Developments/Elite Dangerous",
        "C:/Users/" + qgetenv("USERNAME") + "/Saved Games/Frontier Developments/Elite Dangerous"
    };
    
    // Return the first path that exists
    for (const QString &path : possiblePaths) {
        QDir dir(path);
        if (dir.exists()) {
            qDebug() << "Auto-detected journal path:" << path;
            return path;
        }
    }
    
    // If none exist, return the most common path as fallback (but don't hardcode "Admin")
    QString fallbackPath = QDir::homePath() + "/Saved Games/Frontier Developments/Elite Dangerous";
    qDebug() << "No existing journal paths found, using fallback:" << fallbackPath;
    return fallbackPath;
}

bool ConfigManager::loadConfig()
{
    qDebug() << "Loading config from:" << m_configFilePath;
    
    QFile file(m_configFilePath);
    if (!file.exists()) {
        qWarning() << "Config file not found:" << m_configFilePath;
        emit configError("Config file not found");
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open config file:" << file.errorString();
        emit configError("Cannot open config file: " + file.errorString());
        return false;
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        emit configError("JSON parse error: " + parseError.errorString());
        return false;
    }
    
    m_configData = doc.object();
    
    // Load configuration values
    m_commanderName = m_configData.value("commander_name").toString("Unknown");
    
    // Handle journal path with auto-detection and persistence
    QString configJournalPath = m_configData.value("journal_path").toString();
    if (configJournalPath.isEmpty()) {
        // Auto-detect and save the path for future use
        m_journalPath = getDefaultJournalPath();
        qDebug() << "Auto-detected journal path (will be saved):" << m_journalPath;
        
        // Save the auto-detected path back to config for persistence
        m_configData["journal_path"] = m_journalPath;
        
        // Write the updated config immediately so the path persists
        QJsonDocument doc(m_configData);
        QFile file(m_configFilePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
            qDebug() << "Auto-detected journal path saved to config.json";
        }
    } else {
        m_journalPath = configJournalPath;
    }
    
    m_currentSystem = m_configData.value("current_journal").toString("Unknown");
    m_supabaseUrl = m_configData.value("supabase_url").toString();
    m_supabaseKey = m_configData.value("supabase_key").toString();
    m_supabaseAuthConfirmation = m_configData.value("supabase_auth_confirmation").toBool(false);
    m_journalVerified = m_configData.value("journal_verified").toBool(false);
    
    qDebug() << "Config loaded - journal_verified:" << m_journalVerified;
    
    // Check if user is admin by testing service key access to hidden admin_access table
    if (m_configData.contains("supabase_auth_confirmation")) {
        QJsonValue authValue = m_configData.value("supabase_auth_confirmation");
        if (authValue.isString() && !authValue.toString().isEmpty()) {
            // Try to use the service key to access admin-only table
            QString serviceKey = authValue.toString();
            // Admin verification will be done via network call to admin_access table
            // If successful, m_isAdmin will be set to true and service key will be used
            m_isAdmin = false; // Will be set to true after successful admin table access
            m_adminServiceKey = serviceKey; // Store for admin operations
        }
    }
    
    qDebug() << "Config loaded successfully:";
    qDebug() << "  Commander:" << m_commanderName;
    qDebug() << "  Journal Path:" << m_journalPath;
    qDebug() << "  Current System:" << m_currentSystem;
    qDebug() << "  Supabase URL:" << m_supabaseUrl;
    qDebug() << "  Journal Verified:" << m_journalVerified;
    qDebug() << "  Is Admin:" << m_isAdmin;
    
    // Emit signals for property changes
    emit commanderNameChanged();
    emit journalPathChanged();
    emit currentSystemChanged();
    emit supabaseUrlChanged();
    emit supabaseKeyChanged();
    emit supabaseAuthConfirmationChanged();
    emit journalVerifiedChanged();
    emit isAdminChanged();
    emit configLoaded();
    
    return true;
}

bool ConfigManager::saveConfig()
{
    // Update config data
    m_configData["commander_name"] = m_commanderName;
    m_configData["journal_path"] = m_journalPath;
    m_configData["current_journal"] = m_currentSystem;
    m_configData["journal_verified"] = m_journalVerified;
    
    QJsonDocument doc(m_configData);
    
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot save config file:" << file.errorString();
        emit configError("Cannot save config file: " + file.errorString());
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    qDebug() << "Config saved successfully";
    return true;
}

void ConfigManager::setCommanderName(const QString &name)
{
    if (m_commanderName != name) {
        m_commanderName = name;
        emit commanderNameChanged();
    }
}

void ConfigManager::setJournalPath(const QString &path)
{
    if (m_journalPath != path) {
        m_journalPath = path;
        qDebug() << "Setting journal path to:" << path;
        qDebug() << "Config file path for save:" << m_configFilePath;
        
        // Save config immediately to persist the change
        bool saveSuccess = saveConfig();
        qDebug() << "Config save result for journal_path update:" << (saveSuccess ? "SUCCESS" : "FAILED");
        
        emit journalPathChanged();
        qDebug() << "Journal path set to:" << path << "and saved to config";
    } else {
        qDebug() << "Journal path already set to:" << path << ", no change needed";
    }
}

void ConfigManager::setCurrentSystem(const QString &system)
{
    if (m_currentSystem != system) {
        m_currentSystem = system;
        emit currentSystemChanged();
    }
}

void ConfigManager::setJournalVerified(bool verified)
{
    if (m_journalVerified != verified) {
        m_journalVerified = verified;
        qDebug() << "Setting journal verification to:" << verified << "from" << !verified;
        qDebug() << "Config file path for save:" << m_configFilePath;
        
        // Save config immediately to persist the change
        bool saveSuccess = saveConfig();
        qDebug() << "Config save result for journal_verified update:" << (saveSuccess ? "SUCCESS" : "FAILED");
        
        emit journalVerifiedChanged();
        qDebug() << "Journal verification set to:" << verified << "and saved to config";
    } else {
        qDebug() << "Journal verification already set to:" << verified << ", no change needed";
    }
}

void ConfigManager::setAdminStatus(bool isAdmin)
{
    if (m_isAdmin != isAdmin) {
        m_isAdmin = isAdmin;
        emit isAdminChanged();
    }
} 