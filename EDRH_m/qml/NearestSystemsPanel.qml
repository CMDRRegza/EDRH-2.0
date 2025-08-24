import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    
    // Filter Manager instance
    FilterManager {
        id: filterManager
        
        onFiltersChanged: {
            // Apply filters to system list and reset pagination
            resetPagination()
            updateFilteredSystems()
        }
    }
    
    // Listen for filter change requests from controller
    Connections {
        target: edrhController
        function onRequestFilterChange(filterType) {
            filterManager.setSystemTypeFilter(filterType)
            // Also update the ComboBox to reflect the change
            for (var i = 0; i < filterManager.systemTypeFilters.length; i++) {
                if (filterManager.systemTypeFilters[i] === filterType) {
                    systemTypeFilter.currentIndex = i
                    break
                }
            }
        }
    }
    
    // Filtered systems model
    property var filteredSystemsModel: []
    
    // Pagination properties - smaller initial load to encourage Load More usage
    property int systemsPerPage: 20
    property int currentlyShowing: 20
    property var displayedSystemsModel: []
    
    // Update filtered systems when source data or filters change
    function updateFilteredSystems() {
        var filtered = []
        var sourceData = edrhController.nearestSystems || []
        
        for (var i = 0; i < sourceData.length; i++) {
            var system = sourceData[i]
            if (filterManager.matchesFilter(system)) {
                filtered.push(system)
            }
        }
        
        filteredSystemsModel = filtered
        updateDisplayedSystems()
    }
    
    // Update displayed systems based on pagination
    function updateDisplayedSystems() {
        var displayed = []
        var maxToShow = Math.min(currentlyShowing, filteredSystemsModel.length)
        
        for (var i = 0; i < maxToShow; i++) {
            displayed.push(filteredSystemsModel[i])
        }
        
        displayedSystemsModel = displayed
    }
    
    // Show more systems
    function showMoreSystems() {
        // Save current scroll position
        var oldContentY = systemsList.contentY
        var oldCount = displayedSystemsModel.length
        
        currentlyShowing = Math.min(currentlyShowing + systemsPerPage, filteredSystemsModel.length)
        updateDisplayedSystems()
        
        // Restore scroll position after adding new items
        Qt.callLater(function() {
            // Maintain scroll position or scroll to just above the new items
            var newItemsAdded = displayedSystemsModel.length - oldCount
            if (newItemsAdded > 0) {
                // Keep current view position, don't jump to top
                systemsList.contentY = oldContentY
            }
        })
    }
    
    // Reset pagination when filters change
    function resetPagination() {
        currentlyShowing = systemsPerPage
        updateDisplayedSystems()
    }
    
    // Watch for changes in source data
    Connections {
        target: edrhController
        function onNearestSystemsChanged() {
            resetPagination()
            updateFilteredSystems()
        }
    }
    
    // Initialize filtered systems and pagination
    Component.onCompleted: {
        updateFilteredSystems()
    }
    
    color: Theme.cardBgColor
    border.color: Theme.borderColor
    border.width: 2
    radius: 15
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 25  // Increased from 20 to 25
        spacing: 15  // Increased from 10 to 15
        
        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 125  // Increased from 110 to 125
            color: Theme.secondaryBgColor
            radius: 12
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 30  // Increased from 25 to 30
                spacing: 18  // Increased from 15 to 18
                
                // Title
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10  // Added spacing
                    
                    Text {
                        text: "◈"
                        font.pixelSize: 16  // Increased from 14 to 16
                        color: Theme.accentColor
                    }
                    
                    Text {
                        text: "NEAREST SYSTEMS"
                        font.pixelSize: 20  // Increased from 18 to 20
                        font.bold: true
                        color: Theme.textColor
                    }
                }
                
                // Filters row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12  // Increased from 10 to 12
                    
                    Text {
                        text: "FILTERS"
                        font.pixelSize: 12  // Increased from 11 to 12
                        font.bold: true
                        color: Theme.textMuted
                    }
                    
                    // Category filter button
                    Button {
                        id: categoryFilterBtn
                        text: filterManager.getSelectedCategoriesText()
                        Layout.preferredWidth: 260  // Increased from 250 to 260
                        Layout.preferredHeight: 36  // Increased from 32 to 36
                        
                        background: Rectangle {
                            color: parent.pressed ? Theme.tertiaryBgColor : "#363636"
                            border.color: Theme.borderColor
                            border.width: 1
                            radius: 8
                        }
                        
                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 13  // Increased from 12 to 13
                            font.bold: true
                            color: Theme.textColor
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12  // Increased from 10 to 12
                        }
                        
                        onClicked: {
                            categoryDropdown.visible = !categoryDropdown.visible
                        }
                    }
                    
                    // System type filter
                    ComboBox {
                        id: systemTypeFilter
                        Layout.preferredWidth: 170  // Increased from 160 to 170
                        Layout.preferredHeight: 36  // Increased from 32 to 36
                        
                        model: filterManager.systemTypeFilters
                        currentIndex: 0
                        
                        background: Rectangle {
                            color: Theme.tertiaryBgColor
                            border.color: Theme.borderColor
                            border.width: 1
                            radius: 8
                        }
                        
                        contentItem: Text {
                            text: parent.currentText
                            font.pixelSize: 13  // Increased from 12 to 13
                            font.bold: true
                            color: Theme.textColor
                            leftPadding: 12  // Increased from 10 to 12
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        popup: Popup {
                            y: parent.height - 1
                            width: parent.width
                            implicitHeight: contentItem.implicitHeight
                            padding: 1
                            
                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: systemTypeFilter.delegateModel
                                currentIndex: systemTypeFilter.highlightedIndex
                                
                                ScrollIndicator.vertical: ScrollIndicator { }
                            }
                            
                            background: Rectangle {
                                color: Theme.secondaryBgColor
                                border.color: Theme.borderColor
                                border.width: 1
                                radius: 8
                            }
                        }
                        
                        delegate: ItemDelegate {
                            width: systemTypeFilter.width
                            contentItem: Text {
                                text: modelData
                                color: Theme.textColor
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                            }
                            highlighted: systemTypeFilter.highlightedIndex === index
                            
                            background: Rectangle {
                                color: parent.highlighted ? Theme.tertiaryBgColor : "transparent"
                            }
                        }
                        
                        onCurrentTextChanged: {
                            filterManager.setSystemTypeFilter(currentText)
                        }
                    }
                }
            }
        }
        
        // Category dropdown (hidden by default)
        Rectangle {
            id: categoryDropdown
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 200 : 0
            visible: false
            color: Theme.secondaryBgColor
            radius: 8
            border.width: 1
            border.color: Theme.borderColor
            
            ScrollView {
                anchors.fill: parent
                anchors.margins: 10
                
                Column {
                    width: parent.width
                    spacing: 2
                    
                    Repeater {
                        model: filterManager.availableCategories
                        
                        delegate: Rectangle {
                            readonly property bool isSeparator: modelData === "--- Richard's Stuff ---"
                            
                            width: parent.width
                            height: isSeparator ? 35 : 30
                            color: !isSeparator && categoryMouseArea.containsMouse ? Theme.tertiaryBgColor : "transparent"
                            radius: 4
                            
                            // Separator styling for Richard's Stuff
                            Rectangle {
                                visible: isSeparator
                                anchors.centerIn: parent
                                width: parent.width - 20
                                height: 1
                                color: Theme.borderColor
                            }
                            
                            Row {
                                visible: !isSeparator
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 10
                                spacing: 10
                                
                                Rectangle {
                                    width: 16
                                    height: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: "transparent"
                                    border.color: Theme.accentColor
                                    border.width: 2
                                    radius: 2
                                    
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 8
                                        height: 8
                                        color: Theme.accentColor
                                        radius: 1
                                        visible: filterManager.selectedCategories.includes(modelData)
                                    }
                                }
                                
                                Text {
                                    text: modelData
                                    color: Theme.textColor
                                    font.pixelSize: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            
                            // Separator text styling for Richard's Stuff
                            Text {
                                visible: isSeparator
                                anchors.centerIn: parent
                                text: modelData
                                color: Theme.textMuted
                                font.pixelSize: 11
                                font.bold: true
                                font.italic: true
                            }
                            
                            MouseArea {
                                id: categoryMouseArea
                                anchors.fill: parent
                                hoverEnabled: !isSeparator
                                enabled: !isSeparator
                                
                                onClicked: {
                                    if (!isSeparator) {
                                        filterManager.toggleCategory(modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Systems list
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // Ensure content is clipped to prevent overflow
            clip: true
            
            // Set scroll bar policies
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            
            background: Rectangle {
                color: "#1a1a1a"
                radius: 10
            }
            
            ListView {
                id: systemsList
                anchors.fill: parent
                anchors.margins: 8  // Increased from 5 to 8
                spacing: 8  // Increased from 5 to 8
                
                // Enable clipping for the ListView as well
                clip: true
                
                // Improve scrolling behavior
                boundsBehavior: Flickable.StopAtBounds
                
                model: displayedSystemsModel
                
                delegate: SystemCard {
                    width: systemsList.width - 16  // Adjusted for increased margins
                    systemData: modelData
                    
                    onSystemClicked: function(systemName) {
                        edrhController.viewSystem(systemName)
                    }
                    
                    onSystemNameCopyRequested: function(systemName) {
                        edrhController.copyToClipboard(systemName)
                    }
                }
                
                // Footer with Load More button at bottom of scroll
                footer: Item {
                    width: systemsList.width
                    height: currentlyShowing < filteredSystemsModel.length ? 100 : 50
                    
                    // Systems count info
                    Text {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 10
                        text: "Showing " + displayedSystemsModel.length + " of " + filteredSystemsModel.length + " systems"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                    }
                    
                    // Load More button
                    Button {
                        id: footerShowMoreBtn
                        anchors.centerIn: parent
                        anchors.topMargin: 20
                        text: "Load " + Math.min(systemsPerPage, filteredSystemsModel.length - currentlyShowing) + " More Systems"
                        visible: currentlyShowing < filteredSystemsModel.length
                        enabled: currentlyShowing < filteredSystemsModel.length
                        
                        width: 200
                        height: 40
                        
                        background: Rectangle {
                            color: parent.pressed ? Theme.accentHover : Theme.accentColor
                            radius: 8
                            border.width: 1
                            border.color: Qt.lighter(Theme.accentColor, 1.2)
                        }
                        
                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 13
                            font.bold: true
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        onClicked: showMoreSystems()
                    }
                }
                
                // Show loading or empty state
                Label {
                    anchors.centerIn: parent
                    visible: systemsList.count === 0
                    text: "Loading systems..."
                    color: Theme.textSecondary
                    font.pixelSize: 16  // Increased from 14 to 16
                }
            }
        }
    }
} 