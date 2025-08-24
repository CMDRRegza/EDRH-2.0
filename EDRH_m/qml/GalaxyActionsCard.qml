import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    
    color: Theme.cardBgColor
    border.color: Theme.borderColor
    border.width: 2
    radius: 15
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 25  // Increased from 20 to 25
        spacing: 20  // Increased from 15 to 20
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Text {
                text: "GALAXY ACTIONS"
                font.pixelSize: 15  // Increased from 13 to 15
                font.bold: true
                color: Theme.accentColor
            }
            
            Item { Layout.fillWidth: true }
        }
        
        // Action buttons
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12  // Increased from 10 to 12
            
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 80  // Increased significantly to show more of the galaxy map
                text: "Open Galaxy Map"
                
                background: Rectangle {
                    color: parent.pressed ? "#1a1a1a" : "#0f0f0f"
                    radius: 8
                    border.width: 1
                    border.color: parent.pressed ? "#444444" : "#333333"
                    
                    // Use the user's galaxy background image
                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: "file:///C:/Users/Admin/Documents/GitHub/Project/EDRH_m/assets/E47CDFX.png"
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        
                        // Dark overlay for better text readability
                        Rectangle {
                            anchors.fill: parent
                            color: "#60000000"
                            radius: parent.parent.radius - 1
                        }
                    }
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 16  // Increased font size for bigger button
                    font.bold: true
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    style: Text.Outline
                    styleColor: "#000000"  // Strong text outline for readability over galaxy image
                }
                
                onClicked: {
                    // Prefer the controller signal that main.qml listens to, ensures window exists
                    if (edrhController) {
                        edrhController.openGalaxyMap()
                    }
                }
            }
        }
        
        // Statistics Section
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 360  // Increased from 300 to 360 for better statistics layout
            color: Theme.secondaryBgColor
            radius: 15
            border.width: 1
            border.color: Theme.borderColor
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20  // Increased from 15 to 20
                spacing: 15  // Increased from 12 to 15
                
                // Stats Header
                RowLayout {
                    Layout.fillWidth: true
                    
                    Text {
                        text: "STATISTICS"
                        font.pixelSize: 16  // Increased from 14 to 16
                        font.bold: true
                        color: Theme.accentColor
                    }
                    
                    Item { Layout.fillWidth: true }
                }
                
                // Stats grid
                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: 2
                    rowSpacing: 12  // Increased from 8 to 12
                    columnSpacing: 12  // Increased from 8 to 12
                    
                    // Session time stat
                    StatCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 75  // Increased from 50 to 75 for better text layout
                        title: "APP UPTIME"
                        value: edrhController.sessionTime
                        icon: ""
                        accentColor: Theme.infoColor
                    }
                    
                    // Jump count stat
                    StatCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 75  // Increased from 50 to 75
                        title: "JUMPS THIS SESSION"
                        value: edrhController.jumpCount.toString()
                        icon: ""
                        accentColor: Theme.warningColor
                    }
                    
                    // Map status
                    StatCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 75  // Increased from 50 to 75
                        title: "MAP STATUS"
                        value: edrhController.mapWindowActive ? "Active" : "Inactive"
                        icon: ""
                        accentColor: edrhController.mapWindowActive ? Theme.successColor : Theme.dangerColor
                    }
                    
                    // Claims stat
                    StatCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 75  // Increased from 50 to 75
                        title: "YOUR CLAIMS"
                        value: edrhController.claimManager ? edrhController.claimManager.getClaimCount().toString() : "0"
                        icon: ""
                        accentColor: Theme.warningColor
                    }
                }
            }
        }
    }
} 