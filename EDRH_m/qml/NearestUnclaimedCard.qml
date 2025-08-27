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
                font.pixelSize: 14  // Increased from 10 to 14
                color: Theme.warningColor
            }
            
            Text {
                text: "NEAREST UNCLAIMED"
                font.pixelSize: 14  // Increased from 11 to 14
                font.bold: true
                color: Theme.textMuted
            }
            
            // Spacer
            Item { Layout.fillWidth: true }
            
            // System count
            Text {
                text: "(" + edrhController.nearestSystems.length + " systems)"
                font.pixelSize: 12  // Increased from 10 to 12
                color: Theme.textMuted
                font.italic: true
            }
        }
        
        // Current unclaimed system display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90  // Increased from 70 to 90 for larger text
            Layout.maximumHeight: 90
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
                        font.pixelSize: 16  // Increased from 12 to 16
                        font.bold: true
                        color: Theme.textColor
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                        maximumLineCount: 1
                        style: Text.Outline  // Add text outline for better visibility
                        styleColor: "#000000"
                    }
                    
                    Text {
                        id: distanceText
                        text: edrhController.nearestDistanceText
                        font.pixelSize: 13  // Increased from 10 to 13
                        font.bold: true
                        color: Theme.accentColor
                        Layout.alignment: Qt.AlignRight
                        style: Text.Outline  // Add text outline for better visibility
                        styleColor: "#000000"
                    }
                }
                
                // Category
                Text {
                    id: categoryText
                    text: edrhController.nearestCategoryText
                    font.pixelSize: 12  // Increased from 9 to 12
                    font.bold: true  // Make it bold for better readability
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    maximumLineCount: 1
                }
                
                // POI status (dynamic based on actual data)
                Text {
                    text: {
                        // Get current unclaimed system data to determine POI status
                        var systems = edrhController.unclaimedSystems || []
                        if (systems.length > 0 && edrhController.unclaimedIndex < systems.length) {
                            var currentSystem = systems[edrhController.unclaimedIndex]
                            if (currentSystem && typeof currentSystem === 'object') {
                                var poi = currentSystem.potential_or_poi || currentSystem.poi || ""
                                if (poi === "POI") return "• Confirmed POI"
                                if (poi === "Potential POI") return "• Potential POI"
                                
                                // Check if it's claimed/done
                                if (currentSystem.done) return "• System Done"
                                if (currentSystem.claimed) return "• System Claimed"
                                
                                // Default for unclaimed systems
                                return "• Unclaimed System"
                            }
                        }
                        return "• No System Selected"
                    }
                    font.pixelSize: 11  // Increased from 8 to 11
                    font.bold: true  // Make it bold
                    color: {
                        var systems = edrhController.unclaimedSystems || []
                        if (systems.length > 0 && edrhController.unclaimedIndex < systems.length) {
                            var currentSystem = systems[edrhController.unclaimedIndex]
                            if (currentSystem && typeof currentSystem === 'object') {
                                var poi = currentSystem.potential_or_poi || currentSystem.poi || ""
                                if (poi === "POI") return "#48bb78"  // Green for confirmed POI
                                if (poi === "Potential POI") return "#f6ad55"  // Orange for potential POI
                                if (currentSystem.done) return "#AAFFAA"  // Light green for done
                                if (currentSystem.claimed) return "#FF6B6B"  // Red for claimed
                            }
                        }
                        return Theme.warningColor  // Default color
                    }
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    style: Text.Outline  // Add text outline for better visibility
                    styleColor: "#000000"
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
                font.pixelSize: 14  // Increased from 12 to 14
                font.bold: true  // Make it bold for better visibility
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