#ifndef EXCEPTIONMANAGER_H
#define EXCEPTIONMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QMutex>

class ExceptionManager : public QObject
{
    Q_OBJECT

public:
    enum class ErrorLevel {
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    static ExceptionManager& instance();
    
    // Main logging functions
    void logError(const QString& context, const QString& message, ErrorLevel level = ErrorLevel::ERROR);
    void logException(const QString& context, const std::exception& e, ErrorLevel level = ErrorLevel::ERROR);
    void logInfo(const QString& context, const QString& message);
    void logWarning(const QString& context, const QString& message);
    void logCritical(const QString& context, const QString& message);
    
    // Try-catch wrapper functions
    template<typename Func>
    void safeCatch(const QString& context, Func&& func) {
        try {
            func();
        } catch (const std::exception& e) {
            logException(context, e);
        } catch (...) {
            logError(context, "Unknown exception caught", ErrorLevel::CRITICAL);
        }
    }
    
    template<typename Func, typename ReturnType>
    ReturnType safeCatchWithReturn(const QString& context, Func&& func, const ReturnType& defaultReturn) {
        try {
            return func();
        } catch (const std::exception& e) {
            logException(context, e);
            return defaultReturn;
        } catch (...) {
            logError(context, "Unknown exception caught", ErrorLevel::CRITICAL);
            return defaultReturn;
        }
    }

signals:
    void errorOccurred(const QString& message, ErrorLevel level);

private:
    ExceptionManager(QObject* parent = nullptr);
    ~ExceptionManager();
    
    QString getLogFilePath();
    QString formatLogEntry(const QString& context, const QString& message, ErrorLevel level);
    QString errorLevelToString(ErrorLevel level);
    
    void writeToFile(const QString& logEntry);
    void setupLogFile();
    
    static QMutex s_mutex;
    static ExceptionManager* s_instance;
    
    QString m_logFilePath;
    QMutex m_fileMutex;
};

// Convenience macros
#define SAFE_CATCH(context, code) \
    ExceptionManager::instance().safeCatch(context, [&]() { code; })

#define SAFE_CATCH_RETURN(context, code, defaultValue) \
    ExceptionManager::instance().safeCatchWithReturn(context, [&]() { return code; }, defaultValue)

#define LOG_ERROR(context, message) \
    ExceptionManager::instance().logError(context, message)

#define LOG_WARNING(context, message) \
    ExceptionManager::instance().logWarning(context, message)

#define LOG_INFO(context, message) \
    ExceptionManager::instance().logInfo(context, message)

#define LOG_CRITICAL(context, message) \
    ExceptionManager::instance().logCritical(context, message)

#endif // EXCEPTIONMANAGER_H
