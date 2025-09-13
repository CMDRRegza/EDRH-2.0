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
                        source: "qrc:/EDRH/assets/E47CDFX.png"  // Use correct qrc path as primary
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        
                        // Fallback sources if main source fails
                        property var imageSources: [
                            "qrc:/EDRH/assets/E47CDFX.png",  // Correct qrc path first
                            "qrc:/assets/E47CDFX.png",       // Alternative qrc path
                            "assets/E47CDFX.png"             // Local path last
                        ]
                        property int currentSourceIndex: 0
                        
                        onStatusChanged: {
                            console.log("Galaxy image status changed:", status, "for source:", source)
                            if (status === Image.Error && currentSourceIndex < imageSources.length - 1) {
                                currentSourceIndex++
                                console.log("Trying fallback image source:", imageSources[currentSourceIndex])
                                source = imageSources[currentSourceIndex]
                            } else if (status === Image.Ready) {
                                console.log("Galaxy image loaded successfully from:", source)
                            }
                        }
                        
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
            Layout.maximumHeight: 360    // Add maximum height constraint to prevent stretching
            Layout.minimumHeight: 280    // Add minimum height to prevent collapsing too much
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
                        Layout.maximumHeight: 75    // Prevent stretching
                        Layout.minimumHeight: 60    // Prevent collapsing
                        title: "APP UPTIME"
                        value: edrhController.sessionTime
                        icon: ""
                        accentColor: Theme.infoColor
                    }
                    
                    // Jump count stat
                    StatCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 75  // Increased from 50 to 75
                        Layout.maximumHeight: 75    // Prevent stretching
                        Layout.minimumHeight: 60    // Prevent collapsing
                        title: "JUMPS THIS SESSION"
                        value: edrhController.jumpCount.toString()
                        icon: ""
                        accentColor: Theme.warningColor
                    }
                    
                    // Map status
                    StatCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 75  // Increased from 50 to 75
                        Layout.maximumHeight: 75    // Prevent stretching
                        Layout.minimumHeight: 60    // Prevent collapsing
                        title: "MAP STATUS"
                        value: edrhController.mapWindowActive ? "Active" : "Inactive"
                        icon: ""
                        accentColor: edrhController.mapWindowActive ? Theme.successColor : Theme.dangerColor
                    }
                    
                    // Claims stat
                    StatCard {
                        id: claimsStatCard
                        Layout.fillWidth: true
                        Layout.preferredHeight: 75  // Increased from 50 to 75
                        Layout.maximumHeight: 75    // Prevent stretching
                        Layout.minimumHeight: 60    // Prevent collapsing
                        title: "YOUR CLAIMS"
                        value: edrhController.claimManager ? edrhController.claimManager.getClaimCount().toString() : "0"
                        icon: ""
                        accentColor: Theme.warningColor
                        
                        // Update when claim status changes
                        Connections {
                            target: edrhController.claimManager
                            function onClaimStatusChanged(systemName, isClaimed, claimedBy) {
                                console.log("UI DEBUG: claimStatusChanged signal received - systemName:", systemName, "isClaimed:", isClaimed)
                                claimsStatCard.updateClaimsCounter()
                            }
                        }
                        
                        // Also listen for commander changes
                        Connections {
                            target: edrhController
                            function onCommanderNameChanged() {
                                console.log("UI DEBUG: Commander changed, refreshing claims count")
                                claimsStatCard.updateClaimsCounter()
                            }
                        }
                        
                        // Function to update claims counter with proper validation
                        function updateClaimsCounter() {
                            console.log("UI DEBUG: updateClaimsCounter called")
                            console.log("UI DEBUG: edrhController available:", edrhController ? "YES" : "NO")
                            console.log("UI DEBUG: claimManager available:", edrhController.claimManager ? "YES" : "NO")
                            
                            if (edrhController && edrhController.claimManager) {
                                var newCount = edrhController.claimManager.getClaimCount()
                                console.log("UI DEBUG: getClaimCount() returned:", newCount)
                                claimsStatCard.value = newCount.toString()
                                console.log("UI DEBUG: Updated claimsStatCard.value to:", claimsStatCard.value)
                                
                                // Stop the retry timer once we get data
                                if (retryTimer.running) {
                                    console.log("UI DEBUG: Stopping retry timer - data received")
                                    retryTimer.stop()
                                }
                            } else {
                                console.log("UI DEBUG: Cannot update - claimManager not available")
                            }
                        }
                        
                        // Force update on component completion
                        Component.onCompleted: {
                            console.log("UI DEBUG: GalaxyActionsCard completed - setting up claim counter")
                            console.log("UI DEBUG: edrhController:", edrhController ? "available" : "null")
                            console.log("UI DEBUG: claimManager:", (edrhController && edrhController.claimManager) ? "available" : "null")
                            
                            // Try immediate update first
                            updateClaimsCounter()
                            
                            // Start retry timer as backup
                            retryTimer.start()
                        }
                        
                        // Retry timer with timeout and data validation
                        Timer {
                            id: retryTimer
                            interval: 200  // Increased interval to reduce spam
                            repeat: true
                            property int attempts: 0
                            
                            onTriggered: {
                                attempts++
                                console.log("UI DEBUG: Retry timer attempt", attempts, "- checking ClaimManager and data")
                                
                                if (edrhController && edrhController.claimManager) {
                                    var currentCount = edrhController.claimManager.getClaimCount()
                                    console.log("UI DEBUG: ClaimManager available! Current count:", currentCount)
                                    
                                    // Update the display regardless of count (could legitimately be 0)
                                    claimsStatCard.value = currentCount.toString()
                                    console.log("UI DEBUG: Updated claimsStatCard.value to:", claimsStatCard.value)
                                    
                                    // Stop timer after getting ANY result or after reasonable timeout
                                    if (currentCount > 0 || attempts >= 50) {  // 10 seconds max
                                        console.log("UI DEBUG: Stopping retry timer - count:", currentCount, "attempts:", attempts)
                                        retryTimer.stop()
                                    }
                                } else {
                                    console.log("UI DEBUG: ClaimManager still not available, attempt:", attempts)
                                    
                                    // Timeout after too many attempts
                                    if (attempts >= 50) {  // 10 seconds max
                                        console.log("UI DEBUG: Stopping retry timer - timeout reached")
                                        retryTimer.stop()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
} 