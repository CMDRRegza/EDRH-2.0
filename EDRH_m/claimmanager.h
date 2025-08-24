#ifndef CLAIMMANAGER_H
#define CLAIMMANAGER_H

#include <QObject>
#include <QString>
#include <QDebug>
#include <QJsonArray>
#include <QHash>
#include <QDateTime>

class SupabaseClient;

class ClaimManager : public QObject
{
    Q_OBJECT

public:
    explicit ClaimManager(QObject *parent = nullptr);
    
    void initialize(SupabaseClient *supabaseClient, const QString &commanderName);
    
    Q_INVOKABLE bool canClaimSystem(const QString &systemName);
    Q_INVOKABLE bool claimSystem(const QString &systemName);
    Q_INVOKABLE bool unclaimSystem(const QString &systemName);
    
    Q_INVOKABLE QString getCurrentClaim() const;
    Q_INVOKABLE bool hasActiveClaim() const;
    Q_INVOKABLE int getClaimCount() const;
    Q_INVOKABLE bool isSystemCompleted(const QString &systemName);
    Q_INVOKABLE QString getSystemClaimedBy(const QString &systemName);
    Q_INVOKABLE bool isSystemClaimedByUser(const QString &systemName);
    Q_INVOKABLE void refreshClaimData();
    Q_INVOKABLE QString getClaimStatusDebug(const QString &systemName);
    Q_INVOKABLE void applyLocalClaim(const QString &systemName);
    Q_INVOKABLE void applyLocalUnclaim(const QString &systemName);

signals:
    void claimStatusChanged(const QString &systemName, bool isClaimed, const QString &claimedBy);
    void systemDoneStatusChanged(const QString &systemName, bool isDone);
    void currentClaimChanged(const QString &systemName);
    void claimError(const QString &message);

private slots:
    void onDatabaseOperationComplete(const QString &operation, bool success, const QString &data);
    void onTakenSystemsReceived(const QJsonArray &taken);

private:
    SupabaseClient *m_supabaseClient;
    QString m_commanderName;
    QString m_currentClaim;
    QJsonArray m_takenSystemsData;
    struct PendingOverride { bool claimed; qint64 expireAtMs; };
    QHash<QString, PendingOverride> m_pendingOverrides;
    void pruneExpiredOverrides();
    
    bool queryCurrentClaim();
    bool querySystemStatus(const QString &systemName, QString &claimedBy, bool &isDone);
    bool validateClaimOperation(const QString &operation);
    void logClaimOperation(const QString &operation, const QString &systemName, bool success);
};

#endif // CLAIMMANAGER_H