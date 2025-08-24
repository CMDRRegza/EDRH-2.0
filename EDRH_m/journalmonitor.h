#ifndef JOURNALMONITOR_H
#define JOURNALMONITOR_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>
#include <QDir>

class JournalMonitor : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString commanderName READ commanderName NOTIFY commanderNameChanged)
    Q_PROPERTY(QString currentSystem READ currentSystem NOTIFY currentSystemChanged)
    Q_PROPERTY(QString journalPath READ journalPath WRITE setJournalPath NOTIFY journalPathChanged)
    Q_PROPERTY(bool isMonitoring READ isMonitoring NOTIFY isMonitoringChanged)

public:
    explicit JournalMonitor(QObject *parent = nullptr);
    
    // Property getters
    QString commanderName() const { return m_commanderName; }
    QString currentSystem() const { return m_currentSystem; }
    QString journalPath() const { return m_journalPath; }
    bool isMonitoring() const { return m_isMonitoring; }
    
    // Property setters
    void setJournalPath(const QString &path);
    
    // Public methods
    Q_INVOKABLE void startMonitoring();
    Q_INVOKABLE void stopMonitoring();
    Q_INVOKABLE bool analyzeJournalFolder(const QString &folderPath);
    Q_INVOKABLE QString autoDetectJournalFolder();
    Q_INVOKABLE QString getLatestJournalFile();
    Q_INVOKABLE int countTotalJumps() const;
    
    // Commander extraction - following v1.4incomplete.py pattern
    Q_INVOKABLE QString extractCommanderFromJournal(const QString &journalFilePath = "");

signals:
    void commanderNameChanged();
    void commanderDetected(const QString &commanderName);
    void currentSystemChanged();
    void journalPathChanged();
    void isMonitoringChanged();
    void journalUpdated();
    void fsdJumpDetected(const QString &system, const QJsonObject &jumpData);
    void carrierJumpDetected(const QString &system, const QJsonObject &jumpData);
    void journalError(const QString &error);

private slots:
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);
    void checkForUpdates();

private:
    QFileSystemWatcher *m_fileWatcher;
    QTimer *m_updateTimer;
    QString m_journalPath;
    QString m_commanderName;
    QString m_currentSystem;
    QString m_currentJournalFile;
    bool m_isMonitoring;
    qint64 m_lastFileSize;
    QJsonObject m_lastJumpData;
    
    // Helper methods
    void processJournalFile(const QString &filePath);
    void processJournalLine(const QString &line);
    void extractCommanderName(const QJsonObject &entry);
    void processFSDJump(const QJsonObject &entry);
    void processCarrierJump(const QJsonObject &entry);
    void processLocation(const QJsonObject &entry);
    void updateCurrentJournalFile();
    QStringList findJournalFiles(const QString &directory);
    QString findLatestJournalWithFSDJump(const QString &directory);
    bool hasValidJournalData(const QString &filePath);
    QString extractCommanderFromPath(const QString &filePath);
};

#endif // JOURNALMONITOR_H 