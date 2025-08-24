import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    
    color: Theme.cardBgColor
    border.color: Theme.borderColor
    border.width: 1
    radius: 15
    Layout.fillWidth: true
    Layout.preferredHeight: 180
    Layout.maximumHeight: 180
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 8
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Text {
                text: "◎"
                font.pixelSize: 10
                color: Theme.warningColor
            }
            
            Text {
                text: "NEAREST UNCLAIMED"
                font.pixelSize: 11
                font.bold: true
                color: Theme.textMuted
            }
            
            // Spacer
            Item { Layout.fillWidth: true }
            
            // System count
            Text {
                text: "(" + edrhController.nearestSystems.length + " systems)"
                font.pixelSize: 10
                color: Theme.textMuted
                font.italic: true
            }
        }
        
        // Current unclaimed system display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            Layout.maximumHeight: 70
            color: Theme.secondaryBgColor
            radius: 8
            clip: true
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                
                // Top row with system name and distance
                RowLayout {
                    Layout.fillWidth: true
                    
                    Text {
                        id: systemNameText
                        text: edrhController.currentUnclaimedSystemName
                        font.pixelSize: 12
                        font.bold: true
                        color: Theme.textColor
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                        maximumLineCount: 1
                    }
                    
                    Text {
                        id: distanceText
                        text: edrhController.nearestDistanceText
                        font.pixelSize: 10
                        font.bold: true
                        color: Theme.accentColor
                        Layout.alignment: Qt.AlignRight
                    }
                }
                
                // Category
                Text {
                    id: categoryText
                    text: edrhController.nearestCategoryText
                    font.pixelSize: 9
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    maximumLineCount: 1
                }
                
                // POI status (compact)
                Text {
                    text: "• Potential Discovery"
                    font.pixelSize: 8
                    color: Theme.warningColor
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }
        
        // Navigation controls
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            
            // Previous button
            Button {
                text: "←"
                Layout.preferredWidth: 50
                Layout.preferredHeight: 30
                
                background: Rectangle {
                    color: parent.pressed ? Theme.tertiaryBgColor : Theme.secondaryBgColor
                    border.color: Theme.borderColor
                    border.width: 1
                    radius: 8
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 16
                    font.bold: true
                    color: parent.parent.enabled ? Theme.textColor : Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: edrhController.prevUnclaimed()
            }
            
            // Counter
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: edrhController.unclaimedTotal > 0 ? 
                      (edrhController.unclaimedIndex + 1) + "/" + edrhController.unclaimedTotal : "0/0"
                font.pixelSize: 12
                color: Theme.textMuted
            }
            
            // Next button
            Button {
                text: "→"
                Layout.preferredWidth: 50
                Layout.preferredHeight: 30
                
                background: Rectangle {
                    color: parent.pressed ? Theme.tertiaryBgColor : Theme.secondaryBgColor
                    border.color: Theme.borderColor
                    border.width: 1
                    radius: 8
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 16
                    font.bold: true
                    color: parent.parent.enabled ? Theme.textColor : Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: edrhController.nextUnclaimed()
            }
            
            // Spacer
            Item { Layout.fillWidth: true }
            
            // View button
            Button {
                text: "View"
                Layout.preferredWidth: 50
                Layout.preferredHeight: 30
                
                background: Rectangle {
                    color: parent.pressed ? Theme.accentHover : Theme.accentColor
                    radius: 8
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 12
                    font.bold: true
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    if (edrhController.currentUnclaimedSystemName !== "None") {
                        edrhController.viewSystem(edrhController.currentUnclaimedSystemName)
                    }
                }
            }
        }
    }
} 