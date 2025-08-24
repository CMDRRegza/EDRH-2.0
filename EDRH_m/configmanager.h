#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

class ConfigManager : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString commanderName READ commanderName NOTIFY commanderNameChanged)
    Q_PROPERTY(QString journalPath READ journalPath NOTIFY journalPathChanged)
    Q_PROPERTY(QString currentSystem READ currentSystem NOTIFY currentSystemChanged)
    Q_PROPERTY(QString supabaseUrl READ supabaseUrl NOTIFY supabaseUrlChanged)
    Q_PROPERTY(QString supabaseKey READ supabaseKey NOTIFY supabaseKeyChanged)
    Q_PROPERTY(bool supabaseAuthConfirmation READ supabaseAuthConfirmation NOTIFY supabaseAuthConfirmationChanged)
    Q_PROPERTY(bool journalVerified READ journalVerified NOTIFY journalVerifiedChanged)
    Q_PROPERTY(bool isAdmin READ isAdmin NOTIFY isAdminChanged)

public:
    explicit ConfigManager(QObject *parent = nullptr);
    
    // Load configuration from config.json
    Q_INVOKABLE bool loadConfig();
    Q_INVOKABLE bool saveConfig();
    
    // Property getters
    QString commanderName() const { return m_commanderName; }
    QString journalPath() const { return m_journalPath; }
    QString currentSystem() const { return m_currentSystem; }
    QString supabaseUrl() const { return m_supabaseUrl; }
    QString supabaseKey() const { return m_supabaseKey; }
    bool supabaseAuthConfirmation() const { return m_supabaseAuthConfirmation; }
    bool journalVerified() const { return m_journalVerified; }
    bool isAdmin() const { return m_isAdmin; }
    QString adminServiceKey() const { return m_adminServiceKey; }
    
    // Method to set admin status after successful table access
    void setAdminStatus(bool isAdmin);
    
    // Property setters
    void setCommanderName(const QString &name);
    void setJournalPath(const QString &path);
    void setCurrentSystem(const QString &system);
    Q_INVOKABLE void setJournalVerified(bool verified);

signals:
    void commanderNameChanged();
    void journalPathChanged();
    void currentSystemChanged();
    void supabaseUrlChanged();
    void supabaseKeyChanged();
    void supabaseAuthConfirmationChanged();
    void journalVerifiedChanged();
    void isAdminChanged();
    void configLoaded();
    void configError(const QString &error);

private:
    // Configuration data
    QString m_commanderName;
    QString m_journalPath;
    QString m_currentSystem;
    QString m_supabaseUrl;
    QString m_supabaseKey;
    bool m_supabaseAuthConfirmation;
    bool m_journalVerified;
    bool m_isAdmin;
    QString m_adminServiceKey; // Stores service key for admin operations
    
    // Internal data
    QJsonObject m_configData;
    QString m_configFilePath;
    
    // Helper methods
    void initializeDefaults();
    QString getConfigPath() const;
};

#endif // CONFIGMANAGER_H 