#include "journalmonitor.h"
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonParseError>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

JournalMonitor::JournalMonitor(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_updateTimer(new QTimer(this))
    , m_isMonitoring(false)
    , m_lastFileSize(0)
{
    // Set up timer for periodic checks
    m_updateTimer->setInterval(1000); // Check every second
    m_updateTimer->setSingleShot(false);
    
    // Connect signals
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, 
            this, &JournalMonitor::onFileChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, 
            this, &JournalMonitor::onDirectoryChanged);
    connect(m_updateTimer, &QTimer::timeout, 
            this, &JournalMonitor::checkForUpdates);
}

void JournalMonitor::setJournalPath(const QString &path)
{
    if (m_journalPath != path) {
        m_journalPath = path;
        emit journalPathChanged();
        
        if (m_isMonitoring) {
            stopMonitoring();
            startMonitoring();
        }
    }
}

void JournalMonitor::startMonitoring()
{
    if (m_journalPath.isEmpty()) {
        QString autoDetected = autoDetectJournalFolder();
        if (!autoDetected.isEmpty()) {
            setJournalPath(autoDetected);
        } else {
            emit journalError("No journal folder found");
            return;
        }
    }
    
    if (!QDir(m_journalPath).exists()) {
        emit journalError("Journal folder does not exist: " + m_journalPath);
        return;
    }
    
    // Watch the journal directory
    if (!m_fileWatcher->directories().contains(m_journalPath)) {
        m_fileWatcher->addPath(m_journalPath);
    }
    
    // Find and process the latest journal file
    updateCurrentJournalFile();
    
    if (!m_currentJournalFile.isEmpty()) {
        processJournalFile(m_currentJournalFile);
        
        // Watch the current journal file
        if (!m_fileWatcher->files().contains(m_currentJournalFile)) {
            m_fileWatcher->addPath(m_currentJournalFile);
        }
    }
    
    m_isMonitoring = true;
    m_updateTimer->start();
    emit isMonitoringChanged();
    
    qDebug() << "Journal monitoring started for:" << m_journalPath;
}

void JournalMonitor::stopMonitoring()
{
    m_updateTimer->stop();
    m_fileWatcher->removePaths(m_fileWatcher->files());
    m_fileWatcher->removePaths(m_fileWatcher->directories());
    
    m_isMonitoring = false;
    emit isMonitoringChanged();
    
    qDebug() << "Journal monitoring stopped";
}

bool JournalMonitor::analyzeJournalFolder(const QString &folderPath)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        return false;
    }
    
    QStringList journalFiles = findJournalFiles(folderPath);
    if (journalFiles.isEmpty()) {
        return false;
    }
    
    // Find the latest journal with valid data
    QString latestJournal = findLatestJournalWithFSDJump(folderPath);
    if (!latestJournal.isEmpty()) {
        setJournalPath(folderPath);
        return true;
    }
    
    return false;
}

QString JournalMonitor::autoDetectJournalFolder()
{
    // Try standard Elite Dangerous locations
    QStringList possiblePaths = {
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + 
            "/Frontier Developments/Elite Dangerous",
        QDir::homePath() + "/Saved Games/Frontier Developments/Elite Dangerous",
        "C:/Users/" + qgetenv("USERNAME") + "/Saved Games/Frontier Developments/Elite Dangerous"
    };
    
    for (const QString &path : possiblePaths) {
        if (analyzeJournalFolder(path)) {
            qDebug() << "Auto-detected journal folder:" << path;
            return path;
        }
    }
    
    qDebug() << "Failed to auto-detect journal folder";
    return QString();
}

QString JournalMonitor::getLatestJournalFile()
{
    if (m_journalPath.isEmpty()) {
        return QString();
    }
    
    return findLatestJournalWithFSDJump(m_journalPath);
}

void JournalMonitor::onFileChanged(const QString &path)
{
    if (path == m_currentJournalFile) {
        QFileInfo fileInfo(path);
        if (fileInfo.size() > m_lastFileSize) {
            qDebug() << "Journal file changed, processing updates...";
            processJournalFile(path);
            m_lastFileSize = fileInfo.size();
        }
    }
}

void JournalMonitor::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path)
    // Check if a new journal file was created
    updateCurrentJournalFile();
}

void JournalMonitor::checkForUpdates()
{
    if (!m_currentJournalFile.isEmpty()) {
        QFileInfo fileInfo(m_currentJournalFile);
        if (fileInfo.exists() && fileInfo.size() > m_lastFileSize) {
            processJournalFile(m_currentJournalFile);
            m_lastFileSize = fileInfo.size();
        }
    }
    
    // Check for new journal files
    QString latestJournal = getLatestJournalFile();
    if (!latestJournal.isEmpty() && latestJournal != m_currentJournalFile) {
        qDebug() << "New journal file detected:" << latestJournal;
        updateCurrentJournalFile();
    }
}

void JournalMonitor::processJournalFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit journalError("Failed to open journal file: " + filePath);
        return;
    }
    
    QTextStream stream(&file);
    
    // If we're processing incrementally, seek to the last position
    if (m_lastFileSize > 0) {
        stream.seek(m_lastFileSize);
    }
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            processJournalLine(line);
        }
    }
    
    file.close();
}

void JournalMonitor::processJournalLine(const QString &line)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        return; // Skip invalid JSON lines
    }
    
    QJsonObject entry = doc.object();
    QString event = entry.value("event").toString();
    
    if (event == "Commander") {
        extractCommanderName(entry);
    } else if (event == "FSDJump") {
        processFSDJump(entry);
    } else if (event == "CarrierJump") {
        processCarrierJump(entry);
    } else if (event == "Location") {
        processLocation(entry);
    } else if (event == "LoadGame") {
        extractCommanderName(entry);
    }
}

void JournalMonitor::extractCommanderName(const QJsonObject &entry)
{
    QString commander = entry.value("Name").toString();
    if (!commander.isEmpty() && commander != m_commanderName) {
        m_commanderName = commander;
        emit commanderNameChanged();
        emit commanderDetected(commander);
        qDebug() << "Commander detected:" << commander;
    }
}

void JournalMonitor::processFSDJump(const QJsonObject &entry)
{
    QString system = entry.value("StarSystem").toString();
    if (!system.isEmpty() && system != m_currentSystem) {
        m_currentSystem = system;
        m_lastJumpData = entry;
        emit currentSystemChanged();
        emit fsdJumpDetected(system, entry);
        qDebug() << "FSD Jump to:" << system;
    }
}

void JournalMonitor::processCarrierJump(const QJsonObject &entry)
{
    QString system = entry.value("StarSystem").toString();
    if (!system.isEmpty() && system != m_currentSystem) {
        m_currentSystem = system;
        m_lastJumpData = entry;
        emit currentSystemChanged();
        emit carrierJumpDetected(system, entry);
        qDebug() << "Carrier Jump to:" << system;
    }
}

void JournalMonitor::processLocation(const QJsonObject &entry)
{
    QString system = entry.value("StarSystem").toString();
    if (!system.isEmpty() && system != m_currentSystem) {
        m_currentSystem = system;
        m_lastJumpData = entry;
        emit currentSystemChanged();
        
        // Extract coordinates from Location event just like FSD jumps
        if (entry.contains("StarPos")) {
            emit fsdJumpDetected(system, entry);
        }
        
        qDebug() << "Location update:" << system;
    }
}

void JournalMonitor::updateCurrentJournalFile()
{
    QString latestJournal = getLatestJournalFile();
    if (!latestJournal.isEmpty() && latestJournal != m_currentJournalFile) {
        // Stop watching old file
        if (!m_currentJournalFile.isEmpty()) {
            m_fileWatcher->removePath(m_currentJournalFile);
        }
        
        m_currentJournalFile = latestJournal;
        m_lastFileSize = 0; // Reset to process entire file
        
        // Start watching new file
        m_fileWatcher->addPath(m_currentJournalFile);
        
        qDebug() << "Updated current journal file to:" << m_currentJournalFile;
    }
}

QStringList JournalMonitor::findJournalFiles(const QString &directory)
{
    QDir dir(directory);
    QStringList filters;
    filters << "Journal.*.log";
    
    QStringList journalFiles = dir.entryList(filters, QDir::Files, QDir::Time);
    
    // Return full paths
    QStringList fullPaths;
    for (const QString &file : journalFiles) {
        fullPaths << dir.absoluteFilePath(file);
    }
    
    return fullPaths;
}

QString JournalMonitor::findLatestJournalWithFSDJump(const QString &directory)
{
    QStringList journalFiles = findJournalFiles(directory);
    
    // Check files in reverse chronological order (newest first)
    for (const QString &filePath : journalFiles) {
        if (hasValidJournalData(filePath)) {
            return filePath;
        }
    }
    
    return QString();
}

bool JournalMonitor::hasValidJournalData(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    int lineCount = 0;
    bool hasJump = false;
    bool hasCommander = false;
    
    while (!stream.atEnd() && lineCount < 100) { // Check first 100 lines
        QString line = stream.readLine().trimmed();
        lineCount++;
        
        if (line.contains("\"event\":\"FSDJump\"") || line.contains("\"event\":\"CarrierJump\"")) {
            hasJump = true;
        }
        if (line.contains("\"event\":\"Commander\"") || line.contains("\"event\":\"LoadGame\"")) {
            hasCommander = true;
        }
        
        if (hasJump && hasCommander) {
            file.close();
            return true;
        }
    }
    
    file.close();
    return hasJump || hasCommander; // Accept if we have at least one useful event
}

QString JournalMonitor::extractCommanderFromPath(const QString &filePath)
{
    // Try to extract commander name from file path patterns
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.baseName();
    
    // Elite Dangerous journal files often contain timestamps
    // This is a fallback method if we can't parse the actual file
    QRegularExpression regex("Journal\\.([0-9T\\-\\.]+)");
    QRegularExpressionMatch match = regex.match(fileName);
    
    if (match.hasMatch()) {
        // Return a placeholder - actual commander name should come from file content
        return "Unknown";
    }
    
    return "Unknown";
}

int JournalMonitor::countTotalJumps() const
{
    if (m_journalPath.isEmpty()) {
        qDebug() << "No journal path set for jump counting";
        return 0;
    }
    
    QDir journalDir(m_journalPath);
    if (!journalDir.exists()) {
        qDebug() << "Journal directory does not exist:" << m_journalPath;
        return 0;
    }
    
    // Get all journal files
    QStringList filters;
    filters << "Journal.*.log";
    QFileInfoList journalFiles = journalDir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
    
    int totalJumps = 0;
    
    for (const QFileInfo &fileInfo : journalFiles) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Could not open journal file for jump counting:" << fileInfo.absoluteFilePath();
            continue;
        }
        
        QTextStream stream(&file);
        QString line;
        while (stream.readLineInto(&line)) {
            // Count FSDJump and CarrierJump events
            if (line.contains("\"event\":\"FSDJump\"") || line.contains("\"event\":\"CarrierJump\"")) {
                totalJumps++;
            }
        }
        
        file.close();
    }
    
    // Total jumps counted
    return totalJumps;
} 

QString JournalMonitor::extractCommanderFromJournal(const QString &journalFilePath)
{
    QString filePath = journalFilePath;
    if (filePath.isEmpty()) {
        filePath = m_currentJournalFile;
    }
    
    if (filePath.isEmpty()) {
        filePath = getLatestJournalFile();
    }
    
    if (filePath.isEmpty()) {
        qDebug() << "[WARNING] No journal file available for commander extraction";
        return "Unknown";
    }
    
    QString currentJournalName = QFileInfo(filePath).fileName();
    qDebug() << "[DEBUG] Starting commander extraction from:" << currentJournalName;
    
    // First, scan the CURRENT journal from BOTTOM to TOP (latest entries first)
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[ERROR] Could not open journal file:" << filePath;
        return "Unknown";
    }
    
    QTextStream stream(&file);
    QStringList lines;
    while (!stream.atEnd()) {
        lines.append(stream.readLine());
    }
    file.close();
    
    qDebug() << "[DEBUG] Current journal has" << lines.size() << "lines";
    
    // Read from bottom to top (latest entries first)
    for (int i = lines.size() - 1; i >= 0; i--) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) {
            continue;
        }
        
        // Look for LoadGame or Commander events
        if (line.contains("\"event\":\"LoadGame\"") || line.contains("\"event\":\"Commander\"")) {
            qDebug() << "[DEBUG] Found potential commander event at line" << (i+1) << ":" << line.left(100) << "...";
            
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
            
            if (parseError.error == QJsonParseError::NoError) {
                QJsonObject data = doc.object();
                QString commander;
                
                if (data.value("event").toString() == "LoadGame") {
                    commander = data.value("Commander").toString();
                    if (!commander.isEmpty()) {
                        qDebug() << "[DEBUG] ✓ FOUND LoadGame commander in current journal:" << commander;
                        emit commanderDetected(commander);
                        return commander;
                    }
                } else if (data.value("event").toString() == "Commander") {
                    commander = data.value("Name").toString();
                    if (!commander.isEmpty()) {
                        qDebug() << "[DEBUG] ✓ FOUND Commander event in current journal:" << commander;
                        emit commanderDetected(commander);
                        return commander;
                    }
                }
            } else {
                // Fallback to regex if JSON parsing fails
                QRegularExpression loadGameRegex("\"Commander\"\\s*:\\s*\"([^\"]+)\"");
                QRegularExpression commanderRegex("\"Name\"\\s*:\\s*\"([^\"]+)\"");
                
                QRegularExpressionMatch match;
                if (line.contains("\"event\":\"LoadGame\"")) {
                    match = loadGameRegex.match(line);
                    if (match.hasMatch()) {
                        QString commander = match.captured(1);
                        qDebug() << "[DEBUG] ✓ FOUND commander via LoadGame regex in current journal:" << commander;
                        emit commanderDetected(commander);
                        return commander;
                    }
                } else if (line.contains("\"event\":\"Commander\"")) {
                    match = commanderRegex.match(line);
                    if (match.hasMatch()) {
                        QString commander = match.captured(1);
                        qDebug() << "[DEBUG] ✓ FOUND commander via Commander regex in current journal:" << commander;
                        emit commanderDetected(commander);
                        return commander;
                    }
                }
            }
        }
    }
    
    // If NO commander found in current journal, check the most recent journals
    qDebug() << "[WARNING] No commander found in current journal" << currentJournalName << ", checking recent journals...";
    
    QStringList allJournals = findJournalFiles(m_journalPath);
    
    qDebug() << "[DEBUG] Found" << allJournals.size() << "total journals to check";
    
    // Check the most recent journals (excluding current one we already checked)
    for (int i = 0; i < qMin(10, allJournals.size()); i++) {
        QString journalPath = allJournals[i];
        QString journalName = QFileInfo(journalPath).fileName();
        
        if (journalPath == filePath) {
            qDebug() << "[DEBUG] Skipping current journal:" << journalName;
            continue;
        }
        
        qDebug() << "[DEBUG] Checking recent journal:" << journalName;
        
        QFile recentFile(journalPath);
        if (!recentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "[DEBUG] Error reading recent journal" << journalName;
            continue;
        }
        
        QTextStream recentStream(&recentFile);
        QStringList recentLines;
        while (!recentStream.atEnd()) {
            recentLines.append(recentStream.readLine());
        }
        recentFile.close();
        
        // Read from bottom to top (latest entries first)
        for (int j = recentLines.size() - 1; j >= 0; j--) {
            QString line = recentLines[j].trimmed();
            if (line.isEmpty()) {
                continue;
            }
            
            if (line.contains("\"event\":\"LoadGame\"") && line.contains("\"Commander\"")) {
                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
                
                if (parseError.error == QJsonParseError::NoError) {
                    QJsonObject data = doc.object();
                    QString commander = data.value("Commander").toString();
                    if (!commander.isEmpty()) {
                        qDebug() << "[DEBUG] ✓ FOUND commander" << commander << "in recent journal:" << journalName;
                        emit commanderDetected(commander);
                        return commander;
                    }
                } else {
                    // Fallback regex
                    QRegularExpression regex("\"Commander\"\\s*:\\s*\"([^\"]+)\"");
                    QRegularExpressionMatch match = regex.match(line);
                    if (match.hasMatch()) {
                        QString commander = match.captured(1);
                        qDebug() << "[DEBUG] ✓ FOUND commander via regex" << commander << "in recent journal:" << journalName;
                        emit commanderDetected(commander);
                        return commander;
                    }
                }
            }
        }
    }
    
    qDebug() << "[WARNING] No commander found in any recent journals";
    return "Unknown";
} 