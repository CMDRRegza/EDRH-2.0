import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: splashRoot
    
    anchors.fill: parent
    color: "#000000"
    
    property alias progressValue: progressBar.value
    property alias statusText: statusLabel.text
    property bool isComplete: false
    
    signal splashComplete()
    
    // Background with space image
    Rectangle {
        anchors.fill: parent
        color: "#0a0a0a"
        
        // Animated background gradient
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1a1a2e" }
                GradientStop { position: 0.5; color: "#16213e" }
                GradientStop { position: 1.0; color: "#0a0a0a" }
            }
            
            // Pulse animation
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: !splashRoot.isComplete
                NumberAnimation { to: 0.8; duration: 2000; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.0; duration: 2000; easing.type: Easing.InOutQuad }
            }
        }
        
        // Floating particles/stars
        Repeater {
            model: 50
            Rectangle {
                width: Math.random() * 3 + 1
                height: width
                radius: width / 2
                color: Qt.rgba(1, 1, 1, Math.random() * 0.8 + 0.2)
                x: parent ? Math.random() * parent.width : Math.random() * 600
                y: parent ? Math.random() * parent.height : Math.random() * 400
                
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation { 
                        to: Math.random() * 0.5
                        duration: (Math.random() * 2000) + 1000
                        easing.type: Easing.InOutQuad 
                    }
                    NumberAnimation { 
                        to: Math.random() * 0.8 + 0.2
                        duration: (Math.random() * 2000) + 1000
                        easing.type: Easing.InOutQuad 
                    }
                }
            }
        }
    }
    
    // Main content
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width * 0.6
        spacing: 40
        
        // Logo and title section
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 20
            
            // EDRH Logo (text-based for now)
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: logoText.width + 40
                height: logoText.height + 30
                color: "transparent"
                border.color: "#FF7F50"
                border.width: 3
                radius: 15
                
                Text {
                    id: logoText
                    anchors.centerIn: parent
                    text: "EDRH"
                    font.pixelSize: 48
                    font.bold: true
                    font.family: "Arial Black"
                    color: "#FF7F50"
                }
                
                // Glow effect (Qt6 compatible)
                Rectangle {
                    anchors.centerIn: logoText
                    width: logoText.width + 4
                    height: logoText.height + 4
                    color: "transparent"
                    border.color: "#FF7F50"
                    border.width: 1
                    radius: 2
                    opacity: 0.3
                    z: -1
                }
                
                // Scale animation
                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    running: !splashRoot.isComplete
                    NumberAnimation { to: 1.05; duration: 1500; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 1.0; duration: 1500; easing.type: Easing.InOutQuad }
                }
            }
            
            // Application title
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Elite Dangerous Records Helper"
                font.pixelSize: 20
                font.bold: true
                color: "#FFFFFF"
                opacity: 0.9
            }
            
            // Version
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "v1.4.0-qt"
                font.pixelSize: 14
                color: "#B0B0B0"
                opacity: 0.8
            }
        }
        
        // Loading section
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 15
            
            // Progress bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 6
                color: "#2a2a2a"
                radius: 3
                border.width: 1
                border.color: "#444444"
                
                Rectangle {
                    id: progressBar
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 1
                    radius: 2
                    color: "#FF7F50"
                    
                    property real value: 0.0
                    width: parent.width * Math.min(value, 1.0)
                    
                    Behavior on width {
                        NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
                    }
                    
                    // Shine effect
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.3) }
                            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.1) }
                            GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.0) }
                        }
                    }
                }
            }
            
            // Status text
            Text {
                id: statusLabel
                Layout.alignment: Qt.AlignHCenter
                text: "Initializing..."
                font.pixelSize: 14
                color: "#CCCCCC"
                
                // Fade in/out animation for text changes
                Behavior on text {
                    SequentialAnimation {
                        NumberAnimation { target: statusLabel; property: "opacity"; to: 0.5; duration: 150 }
                        NumberAnimation { target: statusLabel; property: "opacity"; to: 1.0; duration: 150 }
                    }
                }
            }
            
            // Spinning loading indicator
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 32
                height: 32
                color: "transparent"
                visible: !splashRoot.isComplete
                
                Rectangle {
                    width: 4
                    height: 12
                    color: "#FF7F50"
                    radius: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    transformOrigin: Item.Bottom
                    
                    RotationAnimation on rotation {
                        from: 0
                        to: 360
                        duration: 1000
                        loops: Animation.Infinite
                        running: !splashRoot.isComplete
                    }
                }
                
                Repeater {
                    model: 8
                    Rectangle {
                        width: 3
                        height: 8
                        color: "#FF7F50"
                        radius: 1.5
                        opacity: 0.3 + (index * 0.1)
                        anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                        anchors.top: parent ? parent.top : undefined
                        transformOrigin: Item.Bottom
                        rotation: index * 45
                    }
                }
            }
        }
        
        // Credits
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Loading journal data and connecting to database..."
            font.pixelSize: 12
            color: "#888888"
            opacity: 0.7
        }
    }
    
    // Fade out animation when complete
    NumberAnimation {
        id: fadeOutAnimation
        target: splashRoot
        property: "opacity"
        to: 0.0
        duration: 500
        easing.type: Easing.InOutQuad
        
        onFinished: {
            splashRoot.visible = false
            splashRoot.splashComplete()
        }
    }
    
    // Public methods
    function updateProgress(value, message) {
        progressValue = value
        if (message !== undefined) {
            statusText = message
        }
    }
    
    function complete() {
        isComplete = true
        statusText = "Ready!"
        progressValue = 1.0
        
        // Wait a moment then fade out
        Qt.callLater(function() {
            fadeOutTimer.start()
        })
    }
    
    Timer {
        id: fadeOutTimer
        interval: 800
        onTriggered: fadeOutAnimation.start()
    }
    
    // Real initialization - no auto-complete
    Component.onCompleted: {
        // Start with initial state
        updateProgress(0.0, "Initializing...")
    }
} 