import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Qt.labs.platform 1.1
import EDRH 1.0

ApplicationWindow {
    id: mainWindow
    
    // Responsive window sizing
    width: Math.max(1200, Screen.width * 0.7)
    height: Math.max(800, Screen.height * 0.8)
    minimumWidth: 1000
    minimumHeight: 700
    
    visible: true  // Changed to true to fix window not showing
    title: "EDRH - Elite Dangerous Records Helper " + edrhController.appVersion
    
    // Material theme configuration
    Material.theme: Material.Dark
    Material.primary: "#FF7F50"  // ACCENT_COLOR
    Material.accent: "#FF7F50"
    Material.background: "#0a0a0a"  // MAIN_BG_COLOR
    Material.foreground: "#FFFFFF"  // TEXT_COLOR
    
    // Custom colors matching the Python version
    readonly property color mainBgColor: "#0a0a0a"
    readonly property color cardBgColor: "#141414"
    readonly property color secondaryBgColor: "#1f1f1f"
    readonly property color tertiaryBgColor: "#2a2a2a"
    readonly property color accentColor: "#FF7F50"
    readonly property color accentHover: "#FF9068"
    readonly property color successColor: "#4ECDC4"
    readonly property color dangerColor: "#E74C3C"
    readonly property color warningColor: "#F39C12"
    readonly property color infoColor: "#3498DB"
    readonly property color textColor: "#FFFFFF"
    readonly property color textSecondary: "#B0B0B0"
    readonly property color textMuted: "#808080"
    readonly property color borderColor: "#2a2a2a"
    
    background: Rectangle {
        color: mainBgColor
        
        // Beautiful space background like Icarus Terminal
        Rectangle {
            anchors.fill: parent
            color: "#0a0a0a"
            
            // Animated nebula-like gradient background
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#1a1a2e" }
                    GradientStop { position: 0.3; color: "#16213e" }
                    GradientStop { position: 0.7; color: "#0f172a" }
                    GradientStop { position: 1.0; color: "#0a0a0a" }
                }
                opacity: 0.6
                
                // Subtle pulsing animation
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: true
                    NumberAnimation { to: 0.4; duration: 8000; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 0.6; duration: 8000; easing.type: Easing.InOutQuad }
                }
            }
            
            // Distant stars
            Repeater {
                model: 150
                Rectangle {
                    width: Math.random() * 2 + 0.5
                    height: width
                    radius: width / 2
                    color: Qt.rgba(1, 1, 1, Math.random() * 0.8 + 0.2)
                    x: Math.random() * (parent ? parent.width : 1200)
                    y: Math.random() * (parent ? parent.height : 800)
                    
                    // Subtle twinkling
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        NumberAnimation { 
                            to: Math.random() * 0.3 + 0.2
                            duration: (Math.random() * 3000) + 2000
                            easing.type: Easing.InOutQuad 
                        }
                        NumberAnimation { 
                            to: Math.random() * 0.9 + 0.1
                            duration: (Math.random() * 3000) + 2000
                            easing.type: Easing.InOutQuad 
                        }
                    }
                }
            }
            
            // Larger accent stars
            Repeater {
                model: 20
                Rectangle {
                    width: Math.random() * 4 + 2
                    height: width
                    radius: width / 2
                    color: "#FF7F50"
                    opacity: Math.random() * 0.6 + 0.2
                    x: Math.random() * (parent ? parent.width : 1200)
                    y: Math.random() * (parent ? parent.height : 800)
                    
                    // Gentle pulsing
                    SequentialAnimation on scale {
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.3; duration: 4000; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 4000; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }
    }
    
    // Application state management
    property bool splashComplete: false
    property bool appReady: false
    
    // Splash Screen Overlay (separate window)
    Window {
        id: splashWindow
        width: 600
        height: 400
        visible: !splashComplete
        flags: Qt.SplashScreen | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint
        color: "#000000"
        modality: Qt.ApplicationModal
        
        // Center the splash screen on screen
        x: (Screen.width - width) / 2
        y: (Screen.height - height) / 2
        
        // Ensure it stays on top and appears immediately
        Component.onCompleted: {
            show()
            raise()
            requestActivate()
        }
        
        SplashScreen {
            id: splashScreen
            anchors.fill: parent
            
            Component.onCompleted: {
                // Connect authentication signals from SupabaseClient
                console.log("QML: Connecting authentication signals...")
                supabaseClient.authenticationComplete.connect(authenticationComplete)
                supabaseClient.confirmationRequired.connect(showConfirmationDialog)
                console.log("QML: Signals connected, starting initialization...")
                
                // Start the authentication initialization process
                initializeApplication()
            }
            
            onSplashComplete: {
                console.log("Splash complete signal received!")
                mainWindow.splashComplete = true
                mainWindow.visible = true
                mainWindow.show()  // Force show
                mainWindow.raise()  // Bring to front
                mainWindow.requestActivate()  // Make active
                mainContent.visible = true
                mainContent.opacity = 1.0
                
                // Don't close splash window immediately
                Qt.callLater(function() {
                    try {
                        if (splashWindow) {
                            splashWindow.close()
                            splashWindow.destroy()
                        }
                    } catch (e) {
                        console.warn("Error closing splash window:", e)
                    }
                })
            }
        }
    }
    
            // Main content area (hidden during splash)
        Item {
            id: mainContent
            anchors.fill: parent
            visible: splashComplete
            opacity: splashComplete ? 1.0 : 0.0
        
        Behavior on opacity {
            NumberAnimation { duration: 500; easing.type: Easing.InOutQuad }
        }
        
        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            color: "transparent"
            
            // Tab view container - transparent to show space background
            Rectangle {
                id: tabContainer
                anchors.fill: parent
                color: "transparent"
                radius: 15
                border.width: 2
                border.color: borderColor
                
                TabBar {
                    id: tabBar
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 50
                    
                    background: Rectangle {
                        color: secondaryBgColor
                        radius: 15
                    }
                    
                    TabButton {
                        text: "Main"
                        font.pixelSize: 16
                        font.bold: true
                        
                        background: Rectangle {
                            color: parent.checked ? accentColor : tertiaryBgColor
                            radius: 10
                            border.width: 1
                            border.color: borderColor
                        }
                        
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: parent.checked ? "#000000" : textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    

                }
                
                // Tab content stack
                StackLayout {
                    id: stackLayout
                    anchors.top: tabBar.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 10
                    currentIndex: tabBar.currentIndex
                    

                    
                    // Main Tab
                    Item {
                        id: mainTab
                        
                        RowLayout {
                            anchors.fill: parent
                            spacing: 15
                            
                            // Left Panel - Expanded Commander Dashboard
                            ColumnLayout {
                                Layout.preferredWidth: 500  // Increased from 450 to 500
                                Layout.fillHeight: true
                                spacing: 20  // Increased from 15 to 20
                                
                                // Commander Info Card (increased size so text doesn't hit edges)
                                CommanderInfoCard {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 280  // Increased from 250 to 280
                                    Layout.maximumHeight: 280
                                }
                                
                                // Quick Actions Card
                                QuickActionsCard {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 120  // Increased from 100 to 120
                                }
                                
                                // Galaxy Actions Card (includes statistics)
                                GalaxyActionsCard {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.minimumHeight: 480  // Increased from 420 to 480 for better statistics layout
                                }
                            }
                            
                            // Right Panel - Nearest Systems
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumWidth: 520  // Increased from 500 to 520
                                spacing: 20  // Increased from 15 to 20
                                
                                // Nearest Unclaimed Card  
                                NearestUnclaimedCard {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 220  // Increased from 200 to 220
                                }
                                
                                // Nearest Systems Panel
                                NearestSystemsPanel {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.minimumHeight: 450  // Increased from 400 to 450
                                }
                            }
                        }
                    }
                }
            }
            
            // Version label
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: versionText.implicitWidth + 20
                height: versionText.implicitHeight + 8
                color: secondaryBgColor
                radius: 6
                
                Text {
                    id: versionText
                    anchors.centerIn: parent
                    text: edrhController.appVersion
                    color: textMuted
                    font.pixelSize: 10
                    font.bold: true
                }
            }
            
            // Admin indicator (if admin)
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 20
                width: adminText.width + 24
                height: adminText.height + 12
                color: dangerColor
                radius: 8
                visible: edrhController.isAdmin
                
                Text {
                    id: adminText
                    anchors.centerIn: parent
                    text: "ADMIN MODE"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.bold: true
                }
            }
            
            // Refresh button
            Button {
                anchors.top: parent.top
                anchors.right: edrhController.isAdmin ? parent.right : parent.right
                anchors.margins: 20
                anchors.rightMargin: edrhController.isAdmin ? 140 : 20
                
                text: "Refresh"
                font.pixelSize: 12
                font.bold: true
                
                background: Rectangle {
                    color: parent.pressed ? tertiaryBgColor : secondaryBgColor
                    border.width: 1
                    border.color: borderColor
                    radius: 8
                }
                
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: edrhController.refreshData()
            }
        }
    }
    
    // Galaxy Map Window (opens when button clicked)
    GalaxyMapWindow {
        id: galaxyMapWindow
        visible: false
    }
    
    // Connect EDRHController signal to open galaxy map window
    Connections {
        target: edrhController
        function onOpenGalaxyMapWindow() {
            console.log("Opening galaxy map window from EDRHController signal")
            galaxyMapWindow.show()
            galaxyMapWindow.raise()
            galaxyMapWindow.requestActivate()
        }
    }
    
    // Message popup (for showing notifications)
    Popup {
        id: messagePopup
        parent: Overlay.overlay  // Ensure it's in the overlay
        x: (mainWindow.width - width) / 2  // Use mainWindow width
        y: Math.max(50, (mainWindow.height - height) / 2 - 100)  // Center with offset
        width: 400
        height: 200
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        z: 1000  // Ensure it appears above other popups
        
        property alias title: titleText.text
        property alias message: messageText.text
        
        background: Rectangle {
            color: cardBgColor
            radius: 15
            border.width: 2
            border.color: accentColor
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            
            Text {
                id: titleText
                font.pixelSize: 18
                font.bold: true
                color: accentColor
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            
            Text {
                id: messageText
                font.pixelSize: 14
                color: textColor
                Layout.fillWidth: true
                Layout.fillHeight: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            
            Button {
                text: "OK"
                Layout.alignment: Qt.AlignHCenter
                
                background: Rectangle {
                    color: parent.pressed ? accentHover : accentColor
                    radius: 8
                }
                
                contentItem: Text {
                    text: parent.text
                    font.bold: true
                    color: "#000000"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    messagePopup.close()
                }
            }
        }
    }
    
    // Connect to controller signals
    Connections {
        target: edrhController
        
        function onShowMessage(title, message) {
            // Don't show copy messages in main window - SystemViewPopup handles them directly
            if (title === "Copied") {
                return
            }
            messagePopup.title = title
            messagePopup.message = message
            messagePopup.open()
        }
        
        function onShowError(title, message) {
            // Suppress "Database Error" dialogs - they often occur during normal operation
            // and shouldn't block the user interface (especially during image uploads)
            if (title === "Database Error") {
                console.warn("Suppressed Database Error dialog:", message)
                return
            }
            
            messagePopup.title = title
            messagePopup.message = message
            messagePopup.open()
        }
        
        // REMOVED: Old onShowSystemPopup handler - replaced with new debounced version below
        // function onShowSystemPopup(systemName, systemData) { showSystemView(systemName, systemData) }
        
        function onOpenGalaxyMapWindow() {
            galaxyMapWindow.show()
            galaxyMapWindow.raise()
            galaxyMapWindow.requestActivate()
        }
        
        function onDatabaseDownloadProgress(progress, status) {
            if (splashScreen && !mainWindow.splashComplete) {
                splashScreen.updateProgress(progress, status)
            }
        }
        
        function onDatabaseDownloadComplete() {
            if (splashScreen && !mainWindow.splashComplete) {
                splashScreen.complete()
            }
        }
    }
    
    // Connection to SupabaseClient for database sync
    Connections {
        target: supabaseClient
        
        function onDatabaseSyncStatusChanged(status) {
            if (splashScreen && !mainWindow.splashComplete) {
                splashScreen.statusText = status
            }
            console.log("Database sync status:", status)
        }
        
        function onDatabaseSyncProgress(current, total, operation) {
            if (splashScreen && !mainWindow.splashComplete) {
                splashScreen.progressValue = current / total
                splashScreen.statusText = operation + " (" + current + "/" + total + ")"
            }
        }
        
        function onDatabaseSyncComplete(isFirstRun, changesDetected) {
            console.log("Database sync complete. First run:", isFirstRun, "Changes:", changesDetected)
            
            if (splashScreen && !mainWindow.splashComplete) {
                if (isFirstRun) {
                    splashScreen.statusText = "Database downloaded successfully"
                } else {
                    splashScreen.statusText = changesDetected > 0 ? 
                        changesDetected + " updates downloaded" : 
                        "Database up to date"
                }
                splashScreen.progressValue = 1.0
                
                // Give user time to see the completion message
                Qt.callLater(function() {
                    splashScreen.complete()
                })
            }
            
            // Start periodic checking timer
            databaseSyncTimer.running = true
        }
    }
    
    // REMOVED: Old system view popup management - replaced with new debounced implementation below
    // property var currentSystemPopup: null
    
    // REMOVED: Old showSystemView function - completely replaced with new debounced onShowSystemPopup below

    // Application initialization with commander detection FIRST
    function initializeApplication() {
        // Step 1: Detect commanders BEFORE loading config (to prevent normal startup)
        splashScreen.updateProgress(0.1, "Scanning journal files...")
        
        // Try to get journal path
        let journalPath = journalMonitor.autoDetectJournalFolder()
        if (!journalPath) {
            // Show error - no journal found
            authenticationComplete(false, "No Elite Dangerous journal files found. Please ensure the game has been run at least once.")
            return
        }
        
        splashScreen.updateProgress(0.2, "Detecting commanders...")
        
        // This will scan ALL journal files and emit confirmationRequired
        // which will show the confirmation dialog
        // NOTE: configManager.loadConfig() will only be called AFTER user confirms
        supabaseClient.detectCommanderRenames(journalPath)
    }
    
    // Handle authentication results during splash screen
    function authenticationComplete(success, message) {
        // Stop the timeout timer since authentication completed
        authTimeoutTimer.stop()
        
        if (!success) {
            // Authentication failed - show error and exit
            splashScreen.updateProgress(1.0, "Authentication failed!")
            
            // Show error dialog
            Qt.callLater(function() {
                showAuthErrorDialog(message)
            })
        } else {
            // Authentication successful - NOW start database operations
            console.log("QML: Authentication successful, starting database operations")
            splashScreen.updateProgress(0.7, "Loading database...")
            
            // Start the actual database refresh that was originally in splash screen
            edrhController.refreshData()
            
            // Use a timer to delay the next steps
            delayTimer1.start()
        }
    }
    
    // Show authentication error dialog
    function showAuthErrorDialog(message) {
        // Create a simple dialog using Qt.createComponent
        let component = Qt.createComponent("qrc:/EDRH/qml/AuthErrorDialog.qml")
        if (component.status === Component.Ready) {
            let dialog = component.createObject(mainWindow, {
                "errorMessage": message
            })
            
            // Connect to dialog signal
            dialog.closed.connect(function() {
                try {
                    if (dialog) {
                        dialog.destroy()
                    }
                } catch (e) {
                    console.warn("Error destroying auth error dialog:", e)
                }
                Qt.quit() // Exit application
            })
            
            dialog.open()
        } else {
            // Fallback - just exit
            console.error("Authentication failed:", message)
            Qt.quit()
        }
    }
    
    // User confirmation dialog for commander detection
    function showConfirmationDialog(title, message, commanderName) {
        console.log("QML: showConfirmationDialog called with:", title, message, commanderName)
        // Update splash screen to show waiting for confirmation
        splashScreen.updateProgress(0.5, "Waiting for user confirmation...")
        
        let component = Qt.createComponent("qrc:/EDRH/qml/ConfirmationDialog.qml")
        console.log("QML: Component status:", component.status, "Ready =", Component.Ready)
        if (component.status === Component.Ready) {
            let dialog = component.createObject(null, {  // No parent needed for Window
                "dialogTitle": title,
                "message": message,
                "commanderName": commanderName,
                "journalPath": configManager.journalPath
            })
            
            if (!dialog) {
                console.error("QML: Failed to create dialog object!")
                authenticationComplete(false, "Failed to create confirmation dialog")
                return
            }
            
            console.log("QML: Dialog window created successfully, showing...")
            console.log("QML: Dialog position - x:", dialog.x, "y:", dialog.y, "width:", dialog.width, "height:", dialog.height)
            
            // Connect to dialog signals
            dialog.accepted.connect(function() {
                // User confirmed - now start normal application initialization
                try {
                    if (dialog) {
                        dialog.close()
                        dialog.destroy()
                    }
                } catch (e) {
                    console.warn("Error closing accepted dialog:", e)
                }
                splashScreen.updateProgress(0.3, "Loading configuration...")
                
                // NOW load config which will trigger normal C++ startup
                configManager.loadConfig()
                
                // User confirmed commander detection, so enable journal access for this session
                console.log("QML: User confirmed commander detection, setting journal as verified")
                configManager.setJournalVerified(true)
                
                // Skip authentication for now and proceed directly
                Qt.callLater(function() {
                    splashScreen.updateProgress(0.6, "Loading application...")
                    // Skip security check and proceed directly to app startup
                    authenticationComplete(true, "Authentication skipped")
                })
            })
            
            dialog.rejected.connect(function() {
                // User rejected - exit application immediately
                console.log("User selected 'No, Exit' - exiting application...")
                try {
                    if (dialog) {
                        dialog.close()
                        dialog.destroy()
                    }
                } catch (e) {
                    console.warn("Error closing rejected dialog:", e)
                }
                // Exit immediately without showing error dialog
                Qt.quit()
            })
            
            dialog.journalPathChangeRequested.connect(function(newPath) {
                // User wants to change journal path
                console.log("QML: Journal path change requested:", newPath)
                configManager.setJournalPath(newPath)
                configManager.saveConfig()
                dialog.journalPath = newPath
                console.log("QML: Journal path updated to:", newPath)
            })
            
            console.log("QML: About to show dialog window...")
            
            // Show the window (no need to call open() for Window)
            dialog.show()
            dialog.requestActivate()
            dialog.raise()
            
            console.log("QML: dialog.show() called, dialog visible:", dialog.visible)
        } else {
            // Fallback if dialog component fails to load
            console.error("QML: Failed to load confirmation dialog component!")
            console.error("QML: Component status:", component.status)
            console.error("QML: Component error:", component.errorString())
            authenticationComplete(false, "Failed to load confirmation dialog")
        }
    }
    
    // Timer definitions were duplicated - removed duplicates
    
    Timer {
        id: readyTimer
        interval: 800  // Show splash for 0.8 seconds after ready
        onTriggered: {
            console.log("QML: readyTimer triggered, completing splash screen")
            appReady = true
            if (splashScreen) {
                console.log("QML: Calling splashScreen.complete()")
                splashScreen.complete()
            }
            console.log("QML: Splash screen completion triggered")
        }
    }
    
    // Timeout timer for authentication - proceed even if security check fails
    Timer {
        id: authTimeoutTimer
        interval: 5000  // 5 seconds timeout
        onTriggered: {
            console.log("QML: Authentication timeout - proceeding with app startup")
            authenticationComplete(true, "Authentication timed out, proceeding anyway")
        }
    }
    
    // Smart database sync timer - checks for changes every minute
    Timer {
        id: databaseSyncTimer
        interval: 60000  // 1 minute
        repeat: true
        running: false  // Will start after initial sync complete
        
        onTriggered: {
            // Only check for updates if supabase is configured and not already syncing
            if (typeof supabaseClient !== 'undefined' && supabaseClient && supabaseClient.isConfigured()) {
                console.log("Checking for database changes (periodic check)")
                supabaseClient.checkForDatabaseUpdates()
            }
        }
    }
    
    // Delay timers for authentication flow
    Timer {
        id: delayTimer1
        interval: 100
        repeat: false
        onTriggered: {
            console.log("QML: First timer reached")
            splashScreen.updateProgress(0.9, "Loading system data...")
            delayTimer2.start()
        }
    }
    
    Timer {
        id: delayTimer2
        interval: 500
        repeat: false
        onTriggered: {
            console.log("QML: Second timer reached, about to show main window")
            splashScreen.updateProgress(1.0, "Ready!")
            console.log("QML: Starting readyTimer")
            readyTimer.start()
        }
    }
    
    // **FIX: Image Picker Component for safe file selection**
    ImagePicker {
        id: imagePicker
        
        onImageSelected: (systemName, fileName) => {
            console.log("Image selected for", systemName, ":", fileName)
            edrhController.handleImageSelected(systemName, fileName)
        }
        
        onCancelled: {
            console.log("Image selection cancelled")
        }
    }
    
    // SystemViewPopup debounce protection
    property string lastSystemPopupRequest: ""
    property bool systemPopupInProgress: false
    
    Timer {
        id: systemPopupDebounceTimer
        interval: 200 // 200ms debounce for popup creation (reduced from 500ms)
        repeat: false
        onTriggered: {
            var timestamp = new Date().toISOString().substr(11, 12)
            console.log("⏰ [" + timestamp + "] SystemPopup debounce timer TRIGGERED - clearing protection")
            console.log("⏰ [" + timestamp + "] Previous state: inProgress=" + systemPopupInProgress + ", lastRequest='" + lastSystemPopupRequest + "'")
            systemPopupInProgress = false
            lastSystemPopupRequest = ""
            console.log("⏰ [" + timestamp + "] Debounce protection CLEARED")
        }
    }
    
    // **FIX: Connection to handle image picker requests**
    Connections {
        target: edrhController
        function onRequestImagePicker(systemName) {
            console.log("QML: Received image picker request for system:", systemName)
            imagePicker.openForSystem(systemName)
        }
        
            // Handle system popup requests from galaxy map right-click and other sources
    function onShowSystemPopup(systemName, systemData) {
        var timestamp = new Date().toISOString().substr(11, 12)
        console.log("🔥 [" + timestamp + "] QML: RECEIVED showSystemPopup request for:", systemName)
        console.log("🔥 [" + timestamp + "] Current debounce state: inProgress=" + systemPopupInProgress + ", lastRequest='" + lastSystemPopupRequest + "'")
        
        // DEBOUNCE: Prevent multiple popups for same system
        if (systemPopupInProgress) {
            console.log("🚫 [" + timestamp + "] SystemViewPopup creation BLOCKED - already in progress")
            return
        }
        
        if (lastSystemPopupRequest === systemName) {
            console.log("🚫 [" + timestamp + "] SystemViewPopup creation BLOCKED - duplicate request for:", systemName)
            return
        }
        
        // Set debounce protection
        console.log("🔒 [" + timestamp + "] SETTING debounce protection for:", systemName)
        systemPopupInProgress = true
        lastSystemPopupRequest = systemName
        systemPopupDebounceTimer.restart()
        
        console.log("✅ [" + timestamp + "] CREATING SystemViewPopup for:", systemName)
            
            // Create and show SystemViewPopup with proper parenting to prevent garbage collection
            var component = Qt.createComponent("qrc:/EDRH/qml/SystemViewPopup.qml")
            if (component.status === Component.Ready) {
                var popup = component.createObject(mainWindow, {  // ← FIX: Parent to mainWindow for stability
                    "systemName": systemName,
                    "systemData": systemData || {}
                })
                
                if (popup) {
                    console.log("QML: SystemViewPopup created successfully for:", systemName)
                    
                    // Connect to close signal to reset debounce when user manually closes
                    popup.closing.connect(function() {
                        console.log("SystemViewPopup manually closed by user")
                        systemPopupInProgress = false
                        lastSystemPopupRequest = ""
                        // Don't restart the timer - user action should clear immediately
                    })
                    
                    popup.show()
                    popup.requestActivate()
                    popup.raise()
                } else {
                    console.error("QML: Failed to create SystemViewPopup object")
                    // Reset debounce on failure
                    systemPopupInProgress = false
                    lastSystemPopupRequest = ""
                }
            } else {
                console.error("QML: Failed to load SystemViewPopup component:", component.errorString())
                // Reset debounce on failure
                systemPopupInProgress = false
                lastSystemPopupRequest = ""
            }
        }
    }
    
    // **NEW: Connection to handle preset images**
    Connections {
        target: supabaseClient
        function onPresetImageFound(systemName, imageUrl, category) {
            console.log("Preset image found for", systemName, ":", imageUrl)
            // This will be handled by SystemViewPopup when implemented
        }
        
        function onSystemImageSet(systemName, imageUrl, success) {
            console.log("System image set for", systemName, ":", imageUrl, "success:", success)
            // This will be handled by SystemViewPopup when implemented
        }
    }
}
