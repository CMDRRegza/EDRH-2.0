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
        anchors.margins: 30  // Increased from 25 to 30
        spacing: 25  // Increased from 20 to 25
        
        // Commander Section
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90  // Increased from 80 to 90
            color: "transparent"
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 12  // Increased from 8 to 12
                
                // Commander title row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8  // Added spacing
                    
                    Text {
                        text: "⬢"
                        font.pixelSize: 12  // Increased from 10 to 12
                        color: Theme.accentColor
                    }
                    
                    Text {
                        text: "COMMANDER"
                        font.pixelSize: 12  // Increased from 11 to 12
                        font.bold: true
                        color: Theme.textMuted
                    }
                }
                
                // Commander name
                Text {
                    id: commanderName
                    text: "CMDR: " + edrhController.commanderName
                    font.pixelSize: 26  // Increased from 24 to 26
                    font.bold: true
                    color: Theme.textColor
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap  // Added word wrap for long names
                }
            }
        }
        
        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 2
            color: Theme.secondaryBgColor
            radius: 1
        }
        
        // Current System Section
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90  // Changed from 110 to 90 to match Commander section
            color: "transparent"
            clip: true
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 12  // Increased from 8 to 12
                
                // System title row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8  // Added spacing
                    
                    Text {
                        text: "◈"
                        font.pixelSize: 12  // Increased from 10 to 12
                        color: Theme.successColor
                    }
                    
                    Text {
                        text: "CURRENT SYSTEM"
                        font.pixelSize: 12  // Increased from 11 to 12
                        font.bold: true
                        color: Theme.textMuted
                    }
                }
                
                // System name (clickable)
                Text {
                    id: systemName
                    text: edrhController.currentSystem !== "Unknown" ? edrhController.currentSystem : "Scanning journals..."
                    font.pixelSize: 26  // Increased from 24 to 26
                    font.bold: true
                    color: Theme.textColor
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    wrapMode: Text.WordWrap  // Added word wrap for long system names
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        
                        onEntered: parent.color = Theme.accentColor
                        onExited: parent.color = Theme.textColor
                        onClicked: {
                            if (edrhController.currentSystem !== "Unknown") {
                                edrhController.viewSystem(edrhController.currentSystem)
                            }
                        }
                    }
                }
                
                // Removed jump count display as it's unnecessary for the commander info card
                // Jump stats are still available in the Galaxy Map tab for those who need them
            }
        }
        
        // Spacer to fill remaining space
        Item {
            Layout.fillHeight: true
        }
    }
} 