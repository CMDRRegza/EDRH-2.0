import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Dialogs

Window {
    id: confirmWindow
    
    property string dialogTitle: ""
    property string message: ""
    property string commanderName: ""
    property string journalPath: ""
    
    // Make it a proper modal window
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.WindowStaysOnTopHint
    
    width: 650
    height: 650
    
    // Center on screen
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    
    title: "EDRH - Commander Verification"
    visible: true
    color: "#0a0a0a"  // Match splash screen background
    
    // Start with hidden state for animation
    opacity: 0.0
    
    // Custom signal for accepted/rejected
    signal accepted()
    signal rejected()
    signal journalPathChangeRequested(string newPath)
    
    // Force window to front when shown
    onVisibleChanged: {
        if (visible) {
            console.log("ConfirmationDialog: Window is now visible, requesting activation")
            requestActivate()
            raise()
        }
    }
    
    // Center the window properly after creation
    Component.onCompleted: {
        console.log("ConfirmationDialog: Window completed - visible:", visible, "x:", x, "y:", y)
        requestActivate()
        raise()
        
        // Start entrance animation
        entranceAnimation.start()
    }
    
    // Smooth entrance animation
    NumberAnimation {
        id: entranceAnimation
        target: confirmWindow
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: 300
        easing.type: Easing.OutQuart
    }
    
    // File dialog for changing journal path
    FolderDialog {
        id: folderDialog
        title: "Select Elite Dangerous Journal Folder"
        currentFolder: "file:///" + confirmWindow.journalPath.replace(/\\/g, "/")
        
        onAccepted: {
            let newPath = selectedFolder.toString().replace("file:///", "").replace(/\//g, "\\")
            confirmWindow.journalPathChangeRequested(newPath)
        }
    }
    
    // Main container - no border, match splash screen
    Rectangle {
        anchors.fill: parent
        color: "#0a0a0a"
        
        // Animated background gradient like splash screen
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1a1a2e" }
                GradientStop { position: 0.5; color: "#16213e" }
                GradientStop { position: 1.0; color: "#0a0a0a" }
            }
            opacity: 0.9
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 30
            spacing: 20
            
            // Header section - match splash screen style
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12
                
                // Title with border like splash screen logo
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: titleText.width + 30
                    height: titleText.height + 20
                    color: "transparent"
                    border.color: "#FF7F50"
                    border.width: 2
                    radius: 10
                    
                    Text {
                        id: titleText
                        anchors.centerIn: parent
                        text: "Commander Verification"
                        color: "#FF7F50"
                        font.pixelSize: 18
                        font.bold: true
                        font.family: "Arial"
                    }
                }
                
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Multiple commanders detected in your journal files"
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    opacity: 0.9
                }
            }
            
            // Detailed information section
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 15
                
                // Commander info
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    color: "#2a2a2a"
                    radius: 10
                    border.color: "#444444"
                    border.width: 1
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 15
                        spacing: 8
                        
                        Text {
                            text: "Detected Commanders:"
                            color: "#FF7F50"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        
                        Text {
                            text: confirmWindow.message
                            color: "#CCCCCC"
                            font.pixelSize: 13
                            font.family: "Arial"
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                
                // Journal location info
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    color: "#2a2a2a"
                    radius: 10
                    border.color: "#444444"
                    border.width: 1
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 15
                        spacing: 10
                        
                        RowLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: "Journal Location:"
                                color: "#FF7F50"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            
                            Item { Layout.fillWidth: true }
                            
                            Rectangle {
                                Layout.preferredWidth: 80
                                Layout.preferredHeight: 25
                                radius: 5
                                color: changeButton.containsMouse ? "#FF9068" : "#FF7F50"
                                border.color: "#FF5722"
                                border.width: 1
                                
                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: "Change"
                                    color: "#000000"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                                
                                MouseArea {
                                    id: changeButton
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    
                                    onClicked: {
                                        folderDialog.open()
                                    }
                                }
                            }
                        }
                        
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            color: "#1a1a1a"
                            radius: 6
                            border.color: "#333333"
                            border.width: 1
                            
                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 10
                                clip: true
                                
                                Text {
                                    width: parent.width
                                    text: confirmWindow.journalPath || "Not set"
                                    color: "#B0B0B0"
                                    font.pixelSize: 11
                                    font.family: "Consolas, Monaco, monospace"
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
                
                // System information
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                    color: "#2a2a2a"
                    radius: 10
                    border.color: "#444444"
                    border.width: 1
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 15
                        spacing: 8
                        
                        Text {
                            text: "System Information:"
                            color: "#FF7F50"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        
                        Text {
                            Layout.fillWidth: true
                            text: "This information will be used to verify your journal access and track your progress in Elite Dangerous. Different Windows user accounts will have different journal paths."
                            color: "#B0B0B0"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
            
            Item { Layout.fillHeight: true }
            
            // Warning section
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Please verify this information is correct before proceeding"
                color: "#B0B0B0"
                font.pixelSize: 12
                opacity: 0.8
            }
            
            // Button section - properly sized
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 60
                Layout.bottomMargin: 20
                spacing: 30
                
                // Continue button - match splash screen colors
                Rectangle {
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 45
                    radius: 8
                    color: acceptMouse.containsMouse ? "#FF9068" : "#FF7F50"
                    border.color: "#FF5722"
                    border.width: 1
                    
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "Yes, Continue"
                        color: "#000000"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    
                    MouseArea {
                        id: acceptMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        
                        onClicked: {
                            confirmWindow.accepted()
                        }
                    }
                }
                
                // Exit button - match splash screen style
                Rectangle {
                    Layout.preferredWidth: 160
                    Layout.preferredHeight: 45
                    radius: 8
                    color: rejectMouse.containsMouse ? "#555555" : "#444444"
                    border.color: "#666666"
                    border.width: 1
                    
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "No, Exit"
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    
                    MouseArea {
                        id: rejectMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        
                        onClicked: {
                            confirmWindow.rejected()
                        }
                    }
                }
            }
        }
    }
}