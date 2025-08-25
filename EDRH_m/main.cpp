#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QDir>
#include <QtQml>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include "edrhcontroller.h"
#include "configmanager.h"
#include "supabaseclient.h"
#include "imageloader.h"
#include "journalmonitor.h"
#include "galaxymaprenderer.h"
#include "claimmanager.h"
#include "exceptionmanager.h"

// Custom message handler for timestamped logging
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    static QFile debugFile;
    static QTextStream stream;
    static bool initialized = false;
    
    if (!initialized) {
        // Create log directory
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(logDir);
        
        // Create timestamped log file
        QString fileName = QString("debug_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        debugFile.setFileName(QDir(logDir).filePath(fileName));
        debugFile.open(QIODevice::WriteOnly | QIODevice::Append);
        stream.setDevice(&debugFile);
        initialized = true;
        
        // Log the file location to console
        fprintf(stderr, "[LOG] Debug log created at: %s\n", debugFile.fileName().toLocal8Bit().constData());
    }
    
    // Format message with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString typeStr;
    switch (type) {
        case QtDebugMsg: typeStr = "DEBUG"; break;
        case QtWarningMsg: typeStr = "WARN"; break;
        case QtCriticalMsg: typeStr = "CRITICAL"; break;
        case QtFatalMsg: typeStr = "FATAL"; break;
        case QtInfoMsg: typeStr = "INFO"; break;
    }
    
    QString formattedMsg = QString("%1 | %2 | %3").arg(timestamp, typeStr, msg);
    
    // Write to file
    stream << formattedMsg << Qt::endl;
    stream.flush();
    
    // Also output to console for Qt Creator
    fprintf(stderr, "%s\n", formattedMsg.toLocal8Bit().constData());
}

int main(int argc, char *argv[])
{
    try {
        // Fix for XMLHttpRequest local file access
        qputenv("QML_XHR_ALLOW_FILE_READ", "1");
        
        QGuiApplication app(argc, argv);
        
        // Initialize ExceptionManager singleton
        ExceptionManager& exceptionManager = ExceptionManager::instance();
        Q_UNUSED(exceptionManager);
        
        // Install custom message handler for timestamped logging
        qInstallMessageHandler(messageHandler);
        
        qDebug() << "=== EDRH APPLICATION STARTED ===";
        qDebug() << "Application Version:" << "0.9.5";
        qDebug() << "Qt Version:" << QT_VERSION_STR;
    
    // Set application properties
    app.setApplicationName("EDRH - Elite Dangerous Records Helper");
    app.setApplicationVersion("0.9.5");
    app.setOrganizationName("EDRH");
    
    // Set the application icon
    app.setWindowIcon(QIcon(":/assets/icon.ico"));
    
    // Set Material style for modern UI
    QQuickStyle::setStyle("Material");
    
    // Create the config manager, supabase client, image loader, journal monitor and controller
    // BUT DO NOT initialize them yet - let QML trigger initialization
    ConfigManager configManager;
    SupabaseClient supabaseClient;
    ImageLoader imageLoader;
    JournalMonitor journalMonitor;
    ClaimManager claimManager;
    EDRHController controller;
    
    // Set up the configuration connections BEFORE loading QML
    // This ensures the signal connection is ready when QML calls loadConfig()
    QObject::connect(&configManager, &ConfigManager::configLoaded, [&]() {
        qDebug() << "Config loaded signal received, configuring components...";
        
        // Configure Supabase client
        supabaseClient.configure(configManager.supabaseUrl(), configManager.supabaseKey());
        controller.setSupabaseClient(&supabaseClient);
        controller.setConfigManager(&configManager);
        controller.setClaimManager(&claimManager);
        
        // Test admin access if service key is provided
        if (!configManager.adminServiceKey().isEmpty()) {
            // Connect to admin access test result
            QObject::connect(&supabaseClient, &SupabaseClient::adminAccessTestComplete,
                            &configManager, &ConfigManager::setAdminStatus);
            
            // Test admin access with the service key
            supabaseClient.testAdminAccess(configManager.adminServiceKey());
        }
        
        // Set up journal monitoring
        journalMonitor.setJournalPath(configManager.journalPath());
        controller.setJournalMonitor(&journalMonitor);
        
        // Connect SupabaseClient to ImageLoader for preset images
        QObject::connect(&supabaseClient, &SupabaseClient::presetImagesReceived,
                        &imageLoader, &ImageLoader::handlePresetImagesReceived);
        
        // Start monitoring
        journalMonitor.startMonitoring();
        
        // Start preloading common images
        imageLoader.preloadCommonImages();
        
        // Initialize smart database sync system
        supabaseClient.initializeDatabaseSync();
        
        // NOW refresh data since SupabaseClient is configured
        controller.refreshData();
        
        qDebug() << "All components configured successfully";
    });
    
    // Create QML engine immediately to show splash screen
    QQmlApplicationEngine engine;
    
    // Register custom QML types
    qmlRegisterType<GalaxyMapRenderer>("EDRH.Components", 1, 0, "GalaxyMapRenderer");
    
    // Register objects with QML BEFORE loading
    engine.rootContext()->setContextProperty("configManager", &configManager);
    engine.rootContext()->setContextProperty("supabaseClient", &supabaseClient);
    engine.rootContext()->setContextProperty("imageLoader", &imageLoader);
    engine.rootContext()->setContextProperty("journalMonitor", &journalMonitor);
    engine.rootContext()->setContextProperty("edrhController", &controller);
    
    // Set the QML import path for qt6_add_qml_module
    engine.addImportPath("qrc:/");
    
    // Load the main QML file immediately to show splash screen
    const QUrl url(QStringLiteral("qrc:/EDRH/qml/main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    
    engine.load(url);
    
    qDebug() << "=== QML ENGINE LOADED SUCCESSFULLY ===";
    qDebug() << "Starting Qt application event loop...";
    
        int result = app.exec();
        
        qDebug() << "=== EDRH APPLICATION ENDING ===" << "Exit code:" << result;
        return result;
        
    } catch (const std::exception& e) {
        LOG_CRITICAL("main", QString("Application crashed with exception: %1").arg(e.what()));
        return -1;
    } catch (...) {
        LOG_CRITICAL("main", "Application crashed with unknown exception");
        return -2;
    }
} 