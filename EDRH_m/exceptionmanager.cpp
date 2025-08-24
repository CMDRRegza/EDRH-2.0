#include "exceptionmanager.h"
#include <QFile>
#include <QDebug>
#include <QCoreApplication>

QMutex ExceptionManager::s_mutex;
ExceptionManager* ExceptionManager::s_instance = nullptr;

ExceptionManager::ExceptionManager(QObject* parent)
    : QObject(parent)
{
    setupLogFile();
}

ExceptionManager::~ExceptionManager()
{
}

ExceptionManager& ExceptionManager::instance()
{
    QMutexLocker locker(&s_mutex);
    if (!s_instance) {
        s_instance = new ExceptionManager();
    }
    return *s_instance;
}

void ExceptionManager::setupLogFile()
{
    m_logFilePath = getLogFilePath();
    
    // Create directory if it doesn't exist
    QDir logDir = QFileInfo(m_logFilePath).dir();
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    // Log startup message
    logInfo("ExceptionManager", QString("Logging initialized - Log file: %1").arg(m_logFilePath));
}

QString ExceptionManager::getLogFilePath()
{
    // Get application directory
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Create logs subdirectory
    QString logsDir = appDir + "/logs";
    QDir dir;
    if (!dir.exists(logsDir)) {
        dir.mkpath(logsDir);
    }
    
    // Create log filename with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd");
    return QString("%1/edrh_errors_%2.log").arg(logsDir, timestamp);
}

void ExceptionManager::logError(const QString& context, const QString& message, ErrorLevel level)
{
    QString logEntry = formatLogEntry(context, message, level);
    writeToFile(logEntry);
    
    // Also output to debug console
    qDebug() << logEntry;
    
    // Emit signal for any UI components that might want to listen
    emit errorOccurred(message, level);
}

void ExceptionManager::logException(const QString& context, const std::exception& e, ErrorLevel level)
{
    QString message = QString("Exception: %1").arg(e.what());
    logError(context, message, level);
}

void ExceptionManager::logInfo(const QString& context, const QString& message)
{
    logError(context, message, ErrorLevel::INFO);
}

void ExceptionManager::logWarning(const QString& context, const QString& message)
{
    logError(context, message, ErrorLevel::WARNING);
}

void ExceptionManager::logCritical(const QString& context, const QString& message)
{
    logError(context, message, ErrorLevel::CRITICAL);
}

QString ExceptionManager::formatLogEntry(const QString& context, const QString& message, ErrorLevel level)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = errorLevelToString(level);
    
    return QString("[%1] %2 | %3 | %4")
        .arg(timestamp, levelStr, context, message);
}

QString ExceptionManager::errorLevelToString(ErrorLevel level)
{
    switch (level) {
        case ErrorLevel::INFO: return "INFO";
        case ErrorLevel::WARNING: return "WARN";
        case ErrorLevel::ERROR: return "ERROR";
        case ErrorLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void ExceptionManager::writeToFile(const QString& logEntry)
{
    QMutexLocker locker(&m_fileMutex);
    
    QFile file(m_logFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream stream(&file);
        stream << logEntry << Qt::endl;
    }
}
