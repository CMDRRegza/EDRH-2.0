#include "claimmanager.h"
#include "supabaseclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

ClaimManager::ClaimManager(QObject *parent) 
    : QObject(parent)
    , m_supabaseClient(nullptr)
{
    qDebug() << "ClaimManager: Initialized";
}

void ClaimManager::initialize(SupabaseClient *supabaseClient, const QString &commanderName)
{
    m_supabaseClient = supabaseClient;
    m_commanderName = commanderName;
    
    qDebug() << "ClaimManager: Initialized for commander" << commanderName;
    
    // Connect to Supabase signals
    if (m_supabaseClient) {
        connect(m_supabaseClient, &SupabaseClient::takenSystemsReceived,
                this, &ClaimManager::onTakenSystemsReceived);
    }
    
    // Query current claim on initialization
    queryCurrentClaim();
}

bool ClaimManager::canClaimSystem(const QString &systemName)
{
    qDebug() << "\n=== ClaimManager::canClaimSystem ===" << systemName;
    qDebug() << "ClaimManager: Current claim:" << m_currentClaim;
    qDebug() << "ClaimManager: Commander:" << m_commanderName;
    qDebug() << "ClaimManager: Taken systems data size:" << m_takenSystemsData.size();
    
    // Rule 1: Can't claim if we already have an active claim
    if (hasActiveClaim()) {
        qDebug() << "CANNOT CLAIM: Already have active claim:" << m_currentClaim;
        return false;
    }
    
    // Rule 2: Can't claim if system is already claimed by someone else
    QString claimedBy;
    bool isDone;
    if (querySystemStatus(systemName, claimedBy, isDone)) {
        qDebug() << "System status - ClaimedBy:" << claimedBy << "IsDone:" << isDone;
        if (!claimedBy.isEmpty() && claimedBy != m_commanderName) {
            qDebug() << "CANNOT CLAIM: System claimed by" << claimedBy;
            return false;
        }
    }
    
    // Rule 3: Can claim if it's unclaimed or our own completed system
    if (claimedBy.isEmpty()) {
        qDebug() << "CAN CLAIM: System is unclaimed";
        return true;
    }
    
    if (claimedBy == m_commanderName && isDone) {
        qDebug() << "CAN CLAIM: Our completed system";
        return true;
    }
    
    qDebug() << "CANNOT CLAIM: System status unclear";
    return false;
}

bool ClaimManager::claimSystem(const QString &systemName)
{
    qDebug() << "\n=== ClaimManager::claimSystem ===" << systemName;
    // Do not hard-block here; let the server enforce uniqueness to avoid client-side races
    
    if (!m_supabaseClient) {
        emit claimError("Database client not initialized");
        return false;
    }
    
    // Prepare claim data
    QJsonObject data;
    data["system"] = systemName;
    data["by_cmdr"] = m_commanderName;
    data["done"] = false;
    data["date_taken"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    
    // Send to database regardless of local snapshot; server RLS/constraints are authoritative
    m_supabaseClient->claimSystem(systemName, m_commanderName);
    
    // Update local state immediately
    m_currentClaim = systemName;
    emit currentClaimChanged(systemName);
    emit claimStatusChanged(systemName, true, m_commanderName);
    
    logClaimOperation("CLAIM", systemName, true);
    return true;
}

bool ClaimManager::unclaimSystem(const QString &systemName)
{
    qDebug() << "\n=== ClaimManager::unclaimSystem ===" << systemName;
    
    // Allow unclaiming ANY system claimed by this commander (active or completed)
    // Only guard against unclaiming systems not owned by the commander
    QString claimedBy;
    bool isDone = false;
    querySystemStatus(systemName, claimedBy, isDone);
    // Only block if we know for sure it's owned by someone else
    if (!claimedBy.isEmpty() && claimedBy != m_commanderName) {
        emit claimError("Cannot unclaim: System is not claimed by you");
        return false;
    }
    
    if (!m_supabaseClient) {
        emit claimError("Database client not initialized");
        return false;
    }
    
    // Delete from database
    m_supabaseClient->unclaimSystem(systemName);
    
    // Update local state immediately
    if (m_currentClaim == systemName) {
        m_currentClaim.clear();
        emit currentClaimChanged("");
    }
    emit claimStatusChanged(systemName, false, "");
    
    logClaimOperation("UNCLAIM", systemName, true);
    return true;
}

QString ClaimManager::getCurrentClaim() const
{
    return m_currentClaim;
}

bool ClaimManager::hasActiveClaim() const
{
    return !m_currentClaim.isEmpty();
}

int ClaimManager::getClaimCount() const
{
    int count = 0;
    for (const QJsonValue &value : m_takenSystemsData) {
        QJsonObject system = value.toObject();
        QString commander = system["by_cmdr"].toString();
        if (commander == m_commanderName) {
            count++;
        }
    }
    qDebug() << "ClaimManager: getClaimCount() returning" << count << "total claims for" << m_commanderName;
    return count;
}

bool ClaimManager::isSystemCompleted(const QString &systemName)
{
    QString claimedBy;
    bool isDone;
    
    if (querySystemStatus(systemName, claimedBy, isDone)) {
        return isDone && claimedBy == m_commanderName;
    }
    
    return false;
}

QString ClaimManager::getSystemClaimedBy(const QString &systemName)
{
    pruneExpiredOverrides();
    if (m_pendingOverrides.contains(systemName)) {
        const PendingOverride &o = m_pendingOverrides.value(systemName);
        return o.claimed ? m_commanderName : QString();
    }
    // If we have an optimistic current claim, treat it as ours immediately
    if (!m_currentClaim.isEmpty() && systemName == m_currentClaim) {
        return m_commanderName;
    }
    
    QString claimedBy;
    bool isDone;
    
    querySystemStatus(systemName, claimedBy, isDone);
    return claimedBy;
}

bool ClaimManager::isSystemClaimedByUser(const QString &systemName)
{
    pruneExpiredOverrides();
    if (m_pendingOverrides.contains(systemName)) {
        bool val = m_pendingOverrides.value(systemName).claimed;
        qDebug() << "ClaimManager: isSystemClaimedByUser(" << systemName << ") via override:" << val;
        return val;
    }
    // Honor local optimistic state immediately after a claim
    if (!m_currentClaim.isEmpty() && systemName == m_currentClaim) {
        qDebug() << "ClaimManager: isSystemClaimedByUser(" << systemName << ") -> true via m_currentClaim";
        return true;
    }
    
    QString claimedBy = getSystemClaimedBy(systemName);
    bool result = (claimedBy == m_commanderName);
    qDebug() << "ClaimManager: isSystemClaimedByUser(" << systemName << ") claimedBy:" << claimedBy << "commander:" << m_commanderName << "result:" << result;
    return result;
}

void ClaimManager::refreshClaimData()
{
    qDebug() << "ClaimManager: Refreshing claim data";
    
    if (!m_supabaseClient) {
        qDebug() << "ClaimManager: No SupabaseClient available";
        return;
    }
    
    // Request fresh data from database
    m_supabaseClient->getTakenSystems();
    
    qDebug() << "ClaimManager: Requested taken systems from database";
}

QString ClaimManager::getClaimStatusDebug(const QString &systemName)
{
    QString status;
    QString claimedBy;
    bool isDone;
    
    if (querySystemStatus(systemName, claimedBy, isDone)) {
        if (claimedBy.isEmpty()) {
            status = "UNCLAIMED";
        } else if (claimedBy == m_commanderName) {
            if (isDone) {
                status = "YOURS_DONE";
            } else if (systemName == m_currentClaim) {
                status = "YOURS_ACTIVE";
            } else {
                status = "YOURS_ORPHANED"; // Shouldn't happen
            }
        } else {
            status = QString("CLAIMED_BY_%1").arg(claimedBy);
        }
    } else {
        status = "UNKNOWN";
    }
    
    return QString("%1 [Current: %2]").arg(status).arg(m_currentClaim);
}

void ClaimManager::onDatabaseOperationComplete(const QString &operation, bool success, const QString &data)
{
    qDebug() << "ClaimManager: Database operation" << operation << "success:" << success;
    
    if (!success) {
        emit claimError("Database operation failed: " + operation);
        return;
    }
    
    // Handle specific operations
    if (operation == "getTakenSystems") {
        // Re-query our current claim after data refresh
        queryCurrentClaim();
    }
}

bool ClaimManager::queryCurrentClaim()
{
    qDebug() << "ClaimManager: Querying current claim for" << m_commanderName;
    qDebug() << "ClaimManager: takenSystemsData size:" << m_takenSystemsData.size();
    
    QString foundClaim;
    
    // Find our active (not done) claim
    for (const QJsonValue &value : m_takenSystemsData) {
        QJsonObject system = value.toObject();
        QString commander = system["by_cmdr"].toString();
        bool done = system["done"].toBool();
        QString systemName = system["system"].toString();
        
        qDebug() << "ClaimManager: Found system:" << systemName << "by:" << commander << "done:" << done;
        
        if (commander == m_commanderName && !done) {
            foundClaim = systemName;
            qDebug() << "ClaimManager: Found active claim:" << systemName;
            break;
        }
    }
    
    qDebug() << "ClaimManager: Search complete, foundClaim:" << foundClaim;
    
    // Update if changed
    if (m_currentClaim != foundClaim) {
        m_currentClaim = foundClaim;
        emit currentClaimChanged(m_currentClaim);
        qDebug() << "ClaimManager: Current claim updated to:" << m_currentClaim;
    } else {
        qDebug() << "ClaimManager: No change in current claim:" << m_currentClaim;
    }
    
    return true;
}

bool ClaimManager::querySystemStatus(const QString &systemName, QString &claimedBy, bool &isDone)
{
    claimedBy.clear();
    isDone = false;
    
    // Find the system in our local data
    for (const QJsonValue &value : m_takenSystemsData) {
        QJsonObject system = value.toObject();
        if (system["system"].toString() == systemName) {
            claimedBy = system["by_cmdr"].toString();
            // Normalize special placeholder to represent unclaimed state
            if (claimedBy.compare("empty", Qt::CaseInsensitive) == 0) {
                claimedBy.clear();
            }
            isDone = system["done"].toBool();
            return true;
        }
    }
    
    // System not in taken list = unclaimed
    return true;
}

bool ClaimManager::validateClaimOperation(const QString &operation)
{
    // Add any additional validation logic here
    return true;
}

void ClaimManager::onTakenSystemsReceived(const QJsonArray &taken)
{
    qDebug() << "ClaimManager: Received" << taken.size() << "taken systems";
    
    // CRITICAL FIX: Only update for complete datasets, not single-system queries
    // Single-system queries (0-1 items) are from SystemViewPopup and should not corrupt complete data
    if (taken.size() > 10) {  // Complete dataset has ~396 systems, single queries have 0-1
        qDebug() << "ClaimManager: Complete dataset detected - updating claim data";
        
        m_takenSystemsData = taken;
        
        // Re-query our current claim with fresh data
        queryCurrentClaim();
    } else {
        qDebug() << "ClaimManager: Single-system query detected - ignoring to prevent data corruption";
        qDebug() << "ClaimManager: Preserving existing dataset of" << m_takenSystemsData.size() << "systems";
        // Don't update m_takenSystemsData or queryCurrentClaim - preserve complete dataset
    }
}

void ClaimManager::logClaimOperation(const QString &operation, const QString &systemName, bool success)
{
    qDebug() << QString("[CLAIM_LOG] %1: %2 - %3")
                .arg(operation)
                .arg(systemName)
                .arg(success ? "SUCCESS" : "FAILED");
}

void ClaimManager::applyLocalClaim(const QString &systemName)
{
    if (systemName.isEmpty()) return;
    m_currentClaim = systemName;
    emit currentClaimChanged(systemName);
    emit claimStatusChanged(systemName, true, m_commanderName);
    // Set pending override for 2 seconds to mask server latency
    PendingOverride o{true, QDateTime::currentMSecsSinceEpoch() + 2000};
    m_pendingOverrides.insert(systemName, o);
}

void ClaimManager::applyLocalUnclaim(const QString &systemName)
{
    if (systemName.isEmpty()) return;
    if (m_currentClaim == systemName) {
        m_currentClaim.clear();
        emit currentClaimChanged("");
    }
    emit claimStatusChanged(systemName, false, "");
    PendingOverride o{false, QDateTime::currentMSecsSinceEpoch() + 2000};
    m_pendingOverrides.insert(systemName, o);
}

void ClaimManager::pruneExpiredOverrides()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<QString> toRemove;
    for (auto it = m_pendingOverrides.constBegin(); it != m_pendingOverrides.constEnd(); ++it) {
        if (it.value().expireAtMs <= now) toRemove.append(it.key());
    }
    for (const QString &k : toRemove) m_pendingOverrides.remove(k);
}
