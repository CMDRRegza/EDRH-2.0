import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Qt.labs.platform 1.1 as Platform

ApplicationWindow {
    id: popupWindow
    
    property string systemName: ""
    property string category: ""
    property var systemData: ({})
    property var systemInfo: null
    property bool isClaimedByUser: false
    property string systemDescription: ""
    property var uploadedImages: []  // Store multiple uploaded images
    property bool canEdit: {
        // Can only edit if system is claimed by current commander
        if (!edrhController || !edrhController.commanderName) return false
        
        // Use the isClaimedByUser property which is already calculated
        return isClaimedByUser
    }
    
    width: 1000
    height: 900
    minimumWidth: 900
    minimumHeight: 800
    
    // Center the popup on screen
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    
    title: "POI: " + systemName
    modality: Qt.ApplicationModal
    flags: Qt.Window | Qt.WindowCloseButtonHint | Qt.WindowTitleHint
    
    Material.theme: Material.Dark
    Material.primary: "#FF7F50"
    Material.accent: "#FF7F50"
    Material.background: "#0a0a0a"
    Material.foreground: "#FFFFFF"
    
    background: Rectangle {
        color: "#0a0a0a"
    }
    
    Component.onCompleted: {
        if (systemName) {
            edrhController.getSystemInformationRobust(systemName)
        }
        updateSystemDisplay()
        checkClaimStatus()
        
        // Note: Preset images are loaded automatically on startup and refreshed periodically
        // No need to reload them every time popup opens
    }
    
    Connections {
        target: edrhController
        function onSystemInformationReceived(sysName, sysInfo) {
            if (sysName === systemName) {
                systemInfo = sysInfo
                updateSystemDisplay()
            }
        }
        
        function onEdsmSystemDataReceived(sysName, sysData) {
            if (sysName === systemName) {
                // Update system data with EDSM information
                systemData = sysData
                updateSystemDisplay()
            }
        }
    }
    
    // Connection to imageLoader for preset image updates
    property int imageRefreshTrigger: 0
    
    Connections {
        target: typeof imageLoader !== 'undefined' ? imageLoader : null
        function onPresetImagesFromDatabaseLoaded(presetImages) {
            // Trigger image refresh when new preset images are loaded
            imageRefreshTrigger++
        }
        
        function onPresetImageLoaded(category, imagePath) {
            // Trigger image refresh if it's for our current category
            if (category === popupWindow.category) {
                imageRefreshTrigger++
            }
        }
    }
    
    // Multi-image file dialog with imgbb upload support
    Platform.FileDialog {
        id: multiImageDialog
        title: "Select Images for " + systemName
        nameFilters: [
            "Image files (*.png *.jpg *.jpeg *.bmp *.gif *.webp)",
            "All files (*)"
        ]
        fileMode: Platform.FileDialog.OpenFiles  // Allow multiple file selection
        
        onAccepted: {
            console.log("Multiple images selected:", files.length, "files")
            uploadProgressText.text = "Uploading " + files.length + " images to imgbb..."
            uploadProgressPopup.open()
            
            // Upload each file to imgbb
            var uploadedCount = 0
            for (var i = 0; i < files.length; i++) {
                var fileUrl = files[i].toString()
                console.log("Uploading image:", fileUrl)
                
                // Convert file URL to local path
                var localPath = fileUrl.replace("file:///", "")
                
                // Call the imgbb upload function
                uploadImageToImgbb(localPath, function(imgbbUrl) {
                    if (imgbbUrl) {
                        uploadedImages.push(imgbbUrl)
                        uploadedCount++
                        uploadProgressText.text = "Uploaded " + uploadedCount + " of " + files.length + " images..."
                        
                        if (uploadedCount === files.length) {
                            uploadProgressPopup.close()
                            // Trigger refresh of images view
                            imagesRepeater.model = getAllImages()
                        }
                    } else {
                        uploadProgressText.text = "Failed to upload image " + (uploadedCount + 1)
                    }
                })
            }
        }
    }
    
    // Upload progress popup
    Popup {
        id: uploadProgressPopup
        parent: popupWindow.contentItem
        anchors.centerIn: parent
        width: 400
        height: 150
        modal: true
        closePolicy: Popup.NoAutoClose
        
        background: Rectangle {
            color: "#141414"
            radius: 15
            border.width: 2
            border.color: "#FF7F50"
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15
            
            Text {
                text: "Uploading Images"
                font.pixelSize: 18
                font.bold: true
                color: "#FF7F50"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            
            Text {
                id: uploadProgressText
                text: "Preparing upload..."
                font.pixelSize: 14
                color: "#FFFFFF"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: uploadProgressPopup.visible
            }
        }
    }
    
    // JavaScript function to upload to imgbb
    function uploadImageToImgbb(filePath, callback) {
        var xhr = new XMLHttpRequest()
        var apiKey = "8df93308e43e8a90de4b3a1219f07956"
        
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        if (response.success && response.data && response.data.url) {
                            console.log("Image uploaded successfully:", response.data.url)
                            callback(response.data.url)
                        } else {
                            console.error("Invalid imgbb response:", xhr.responseText)
                            callback(null)
                        }
                    } catch (e) {
                        console.error("Failed to parse imgbb response:", e)
                        callback(null)
                    }
                } else {
                    console.error("Upload failed with status:", xhr.status)
                    callback(null)
                }
            }
        }
        
        // Read file and convert to base64
        var fileReader = new XMLHttpRequest()
        fileReader.open("GET", "file:///" + filePath, true)
        fileReader.responseType = "blob"
        
        fileReader.onload = function() {
            var reader = new FileReader()
            reader.onloadend = function() {
                // Extract base64 data
                var base64Data = reader.result.split(',')[1]
                
                // Prepare upload
                xhr.open("POST", "https://api.imgbb.com/1/upload", true)
                xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded")
                
                var params = "key=" + apiKey + "&image=" + encodeURIComponent(base64Data)
                xhr.send(params)
            }
            reader.readAsDataURL(fileReader.response)
        }
        
        fileReader.send()
    }
    
    // Copy popup - INSIDE the SystemViewPopup with proper z-order
    // Note: Copy popup is triggered directly from copy buttons, not via signal connection
    
    Popup {
        id: copyPopup
        parent: popupWindow.contentItem  // Parent to the popup content, not main window
        anchors.centerIn: parent
        width: 300
        height: 150
        modal: false  // Don't block interaction with parent popup
        z: 9999  // Very high z-order to appear above everything in this popup
        
        property string messageText: ""
        
        background: Rectangle {
            color: "#141414"
            radius: 15
            border.width: 2
            border.color: "#FF7F50"
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15
            
            Text {
                text: "Copied"
                font.pixelSize: 18
                font.bold: true
                color: "#FF7F50"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            
            Text {
                text: copyPopup.messageText
                font.pixelSize: 14
                color: "#FFFFFF"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            
            Button {
                text: "OK"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 80
                
                background: Rectangle {
                    color: parent.pressed ? "#FF9068" : "#FF7F50"
                    radius: 8
                }
                
                contentItem: Text {
                    text: parent.text
                    font.bold: true
                    color: "#000000"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: copyPopup.close()
            }
        }
        
        // Auto-close after 2 seconds
        Timer {
            id: autoCloseTimer
            interval: 2000
            running: copyPopup.opened
            onTriggered: copyPopup.close()
        }
    }

    
    function updateSystemDisplay() {
        if (systemInfo && systemInfo.hasInformation) {
            systemInfoText.text = systemInfo.system_info || "No additional system information available"
            // Don't set system description from systemInfo
            poiDescEditor.text = systemInfo.poi_description || ""
        } else {
            systemInfoText.text = "Basic system information displayed above. No additional details available in database."
            // Keep descriptions empty
            poiDescEditor.text = ""
        }
        checkClaimStatus()
    }
    
    function checkClaimStatus() {
        if (edrhController && edrhController.isSystemClaimedByCurrentCommander) {
            isClaimedByUser = edrhController.isSystemClaimedByCurrentCommander(systemName)
            console.log("System claim status for", systemName, ":", isClaimedByUser)
        } else {
            isClaimedByUser = false
        }
        // Also check from systemData
        if (systemData && systemData.claimed && systemData.claimedBy === edrhController.commanderName) {
            isClaimedByUser = true
        }
    }
    
    function updateClaimStatus() {
        console.log("Claim status updated for", systemName, ":", isClaimedByUser)
    }
    
    function calculateDistance(x, y, z) {
        // Show actual coordinates instead of hiding them
        if (x === undefined || y === undefined || z === undefined) {
            return "Distance: Unknown (no coordinates)"
        }
        var distance = Math.sqrt(x*x + y*y + z*z)
        return distance.toFixed(2) + " LY"
    }
    
    function getCoordinatesText() {
        // Priority 1: Check journal data from current location
        if (edrhController && systemName === edrhController.currentSystem) {
            var x = edrhController.commanderX
            var y = edrhController.commanderY
            var z = edrhController.commanderZ
            if (x !== undefined && y !== undefined && z !== undefined && (x !== 0 || y !== 0 || z !== 0)) {
                return "X: " + x.toFixed(2) + ", Y: " + y.toFixed(2) + ", Z: " + z.toFixed(2) + " (Journal)"
            }
        }
        
        // Priority 2: Check systemData (which has coordinates from the system list)
        if (systemData && typeof systemData === 'object') {
            var x = systemData.x
            var y = systemData.y  
            var z = systemData.z
            if (x !== undefined && y !== undefined && z !== undefined) {
                return "X: " + x.toFixed(2) + ", Y: " + y.toFixed(2) + ", Z: " + z.toFixed(2)
            }
        }
        
        // Priority 3: Fallback to systemInfo if systemData doesn't have coordinates
        if (systemInfo && typeof systemInfo === 'object') {
            var x = systemInfo.x
            var y = systemInfo.y  
            var z = systemInfo.z
            if (x !== undefined && y !== undefined && z !== undefined) {
                return "X: " + x.toFixed(2) + ", Y: " + y.toFixed(2) + ", Z: " + z.toFixed(2)
            }
        }
        
        // Priority 4: Try to get from EDSM
        edrhController.getEDSMSystemData(systemName)
        
        // Show that coordinates are missing
        return "Coordinates: Fetching from EDSM..."
    }
    
    function getDistanceText() {
        // Always calculate distance from Sol (0,0,0) - ignore pre-calculated distance from current position
        // First check systemData for coordinates
        if (systemData && typeof systemData === 'object') {
            var x = systemData.x
            var y = systemData.y  
            var z = systemData.z
            if (x !== undefined && y !== undefined && z !== undefined) {
                return calculateDistance(x, y, z)
            }
        }
        
        // Fallback to systemInfo if systemData doesn't have coordinates
        if (systemInfo && typeof systemInfo === 'object') {
            var x = systemInfo.x
            var y = systemInfo.y  
            var z = systemInfo.z
            if (x !== undefined && y !== undefined && z !== undefined) {
                return calculateDistance(x, y, z)
            }
        }
        
        // Be honest about why distance isn't available
        return "Distance: Unknown (coordinates missing)"
    }
    
    function getCategoriesText() {
        // Check for multiple categories
        if (systemData && systemData.categoryList && systemData.categoryList.length > 0) {
            // Remove duplicates and join
            var uniqueCategories = []
            for (var i = 0; i < systemData.categoryList.length; i++) {
                if (uniqueCategories.indexOf(systemData.categoryList[i]) === -1) {
                    uniqueCategories.push(systemData.categoryList[i])
                }
            }
            return uniqueCategories.join(", ")
        }
        // Fallback to single category
        return category || "Unknown Category"
    }
    
    function getCategoryColor() {
        var categoryColors = {
            "Colliding Rings": "#24499f",
            "Alternative Carousel": "#9b59b6",
            "Extreme close proximity to ring": "#24499f",
            "PVP (tilted moon)": "#ff4444",
            "PVP (black hole gas giant moons)": "#ff4444",
            "PVP (close binary)": "#ff4444",
            "BarycentreWD+ Atmospheric Landable": "#44ff88"
        }
        return categoryColors[category] || "#666666"
    }
    
    function getCategoryImageUrl() {
        console.log("getCategoryImageUrl called for category:", category)
        console.log("imageLoader available:", typeof imageLoader !== 'undefined' && imageLoader)
        
        // Use the imageLoader system to get preset images from database
        if (typeof imageLoader !== 'undefined' && imageLoader && imageLoader.hasPresetImage(category)) {
            var fileUrl = imageLoader.getPresetImageFileUrl(category)
            console.log("Found preset image file URL:", fileUrl)
            if (fileUrl && fileUrl !== "") {
                console.log("Using cached image:", fileUrl)
                return fileUrl
            } else {
                // Start loading the image and return the direct URL for now
                console.log("Loading preset image for category:", category)
                imageLoader.loadPresetImage(category)
                var url = imageLoader.getPresetImageUrl(category)
                console.log("Returning direct URL:", url)
                return url
            }
        } else {
            console.log("No preset image found for category:", category)
            console.log("imageLoader.hasPresetImage result:", typeof imageLoader !== 'undefined' && imageLoader ? imageLoader.hasPresetImage(category) : "imageLoader not available")
        }
        
        var fallbackUrl = getGenericSpaceImage()
        console.log("Using fallback image:", fallbackUrl)
        return fallbackUrl
    }
    
    function getGenericSpaceImage() {
        // Return a fallback space image for unknown categories
        return "https://images.unsplash.com/photo-1446776877081-d282a0f896e2?w=800&h=600&fit=crop"
    }
    
    function getAllImages() {
        var images = []
        
        // First add user uploaded images
        for (var i = 0; i < uploadedImages.length; i++) {
            // Check if it's an imgbb URL or local file
            var imageUrl = uploadedImages[i]
            if (imageUrl.startsWith("http")) {
                // Already uploaded to imgbb
                images.push({
                    category: "User Upload " + (i + 1),
                    imageUrl: imageUrl,
                    isUserImage: true
                })
            } else {
                // Local file - should not happen with new upload system
                console.warn("Local file in uploadedImages:", imageUrl)
            }
        }
        
        // Then add category preset images
        var categories = []
        if (systemData && systemData.categoryList && systemData.categoryList.length > 0) {
            categories = systemData.categoryList
        } else if (category) {
            categories = [category]
        }
        
        var addedCategories = []
        for (var i = 0; i < categories.length; i++) {
            if (addedCategories.indexOf(categories[i]) === -1) {
                addedCategories.push(categories[i])
                
                var imageUrl = ""
                if (typeof imageLoader !== 'undefined' && imageLoader && imageLoader.hasPresetImage(categories[i])) {
                    var fileUrl = imageLoader.getPresetImageFileUrl(categories[i])
                    if (fileUrl && fileUrl !== "") {
                        imageUrl = fileUrl
                    } else {
                        imageLoader.loadPresetImage(categories[i])
                        imageUrl = imageLoader.getPresetImageUrl(categories[i])
                    }
                } else {
                    imageUrl = getGenericSpaceImage()
                }
                
                images.push({
                    category: categories[i],
                    imageUrl: imageUrl,
                    isUserImage: false
                })
            }
        }
        
        if (images.length === 0) {
            images.push({
                category: "Unknown Category",
                imageUrl: getGenericSpaceImage(),
                isUserImage: false
            })
        }
        
        return images
    }
    

    
    // Main layout with fixed bottom buttons
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 0
        
        // Content area (no scroll here)
        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20
            

                
                // Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    color: "#141414"
                    border.color: "#FF7F50"
                    border.width: 2
                    radius: 15
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 15
                        
                        Rectangle {
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 80
                            color: "#0a0a0a"
                            radius: 8
                            
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width * 0.8
                                height: parent.height * 0.8
                                color: getCategoryColor()
                                radius: 6
                            }
                        }
                        
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 8
                            
                            Text {
                                text: systemName
                                font.pixelSize: 24
                                font.bold: true
                                color: "#FF7F50"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        edrhController.copyToClipboard(systemName)
                                        copyPopup.messageText = "Copied: " + systemName
                                        copyPopup.open()
                                    }
                                    onEntered: parent.color = "#FFD700"
                                    onExited: parent.color = "#FF7F50"
                                }
                            }
                            
                            Text {
                                text: category || "Unknown Category"
                                font.pixelSize: 14
                                color: "#B0B0B0"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
                
                // Tab bar with equal width tabs
                TabBar {
                    id: tabBar
                    Layout.fillWidth: true
                    
                    TabButton {
                        text: "System Info"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    
                    TabButton {
                        text: "Images"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    
                    TabButton {
                        text: "Edit Info"
                        font.pixelSize: 14
                        font.bold: true
                        visible: canEdit
                    }
                }
                
                // Clean StackLayout implementation
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: tabBar.currentIndex
                    
                    // System Info tab - NO SCROLLING
                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 15
                            
                            // System details grid with equal spacing
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: 10
                                columnSpacing: 20
                                
                                // Row 1 - Equal width columns
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 60
                                    color: "#1f1f1f"
                                    radius: 8
                                    
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 15
                                        
                                        Text {
                                            text: "System Name:"
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: "#FF7F50"
                                            Layout.preferredWidth: 100
                                        }
                                        
                                        Text {
                                            text: systemName
                                            font.pixelSize: 14
                                            color: "#FFFFFF"
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                                
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 60
                                    color: "#1f1f1f"
                                    radius: 8
                                    
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 15
                                        
                                        Text {
                                            text: "Distance from Sol:"
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: "#FF7F50"
                                            Layout.preferredWidth: 130
                                        }
                                        
                                        Text {
                                            text: getDistanceText()
                                            font.pixelSize: 14
                                            color: "#FFFFFF"
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                                
                                // Row 2 - Coordinates spanning both columns
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 60
                                    Layout.columnSpan: 2
                                    color: "#1f1f1f"
                                    radius: 8
                                    
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 15
                                        
                                        Text {
                                            text: "Coordinates:"
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: "#FF7F50"
                                            Layout.preferredWidth: 120
                                        }
                                        
                                        Text {
                                            text: getCoordinatesText()
                                            font.pixelSize: 14
                                            color: "#FFFFFF"
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                                
                                // Row 3 - Categories spanning both columns
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 60
                                    Layout.columnSpan: 2
                                    color: "#1f1f1f"
                                    radius: 8
                                    
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 15
                                        
                                        Text {
                                            text: "Categories:"
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: "#FF7F50"
                                            Layout.preferredWidth: 120
                                        }
                                        
                                        Text {
                                            text: getCategoriesText()
                                            font.pixelSize: 14
                                            color: "#FFFFFF"
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                            
                            // System Info - fills remaining space
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: "#1f1f1f"
                                radius: 8
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 10
                                    
                                    Text {
                                    text: "System Information:"
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "#FF7F50"
                                        bottomPadding: 5
                                        }
                                    
                                    ScrollView {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                        clip: true
                                        
                                        Text {
                                            id: systemInfoText
                                            width: parent.width
                                            text: "Loading system information..."
                                            font.pixelSize: 13
                                            font.family: "Consolas, Courier New, monospace"
                                            color: "#FFFFFF"
                                            wrapMode: Text.WordWrap
                                            lineHeight: 1.3
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // Images tab
                    Rectangle {
                        color: "#1f1f1f"
                        
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 20
                            
                            Text {
                                text: "System Images"
                                font.pixelSize: 16
                                font.bold: true
                                color: "#FFFFFF"
                            }
                            
                            // Horizontal scroll for multiple images
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOn
                                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                                clip: true
                                
                                Row {
                                    spacing: 15
                                    height: parent.height
                                    
                                    // Get all images including user uploads
                                    Repeater {
                                        id: imagesRepeater
                                        model: getAllImages()
                                        
                                        delegate: Rectangle {
                                            width: 600
                                            height: parent.height - 20
                                            color: "#0a0a0a"
                                            radius: 8
                                            border.width: 2
                                            border.color: "#444444"
                                            
                                            Column {
                                                anchors.fill: parent
                                                anchors.margins: 10
                                                spacing: 10
                                                
                                                Text {
                                                    text: modelData.category
                                                    font.pixelSize: 14
                                                    font.bold: true
                                                    color: "#FF7F50"
                                                    elide: Text.ElideRight
                                                    width: parent.width
                                                }
                                            
                                            // Only disable main scroll when actively editing
                                            onActiveFocusChanged: {
                                                mainScrollView.interactive = !activeFocus
                                            }
                                                
                                                Image {
                                                    width: parent.width
                                                    height: parent.height - 30
                                                    source: modelData.imageUrl
                                                    fillMode: Image.PreserveAspectFit
                                                    
                                                    Rectangle {
                                                        anchors.left: parent.left
                                                        anchors.right: parent.right
                                                        anchors.bottom: parent.bottom
                                                        height: 40
                                                        color: "#000000"
                                                        opacity: 0.8
                                                        
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: systemName
                                                            font.pixelSize: 12
                                                            font.bold: true
                                                            color: "#FFFFFF"
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
                    
                    // Edit Info tab
                    Flickable {
                        id: mainScrollView
                        // Remove anchors.fill since this is in a StackLayout
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: width
                        contentHeight: editInfoColumn.implicitHeight + 40
                        boundsBehavior: Flickable.StopAtBounds
                        
                        ScrollBar.vertical: ScrollBar {
                            active: true
                            policy: ScrollBar.AsNeeded
                        }
                        
                        // Click outside any text area to unfocus
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                // Remove focus from any active text editor
                                systemInfoEditor.focus = false
                                systemDescEditor.focus = false
                                poiDescEditor.focus = false
                                mainScrollView.forceActiveFocus()
                            }
                            z: -1  // Behind all content
                        }
                        
                        ColumnLayout {
                            id: editInfoColumn
                            width: Math.max(parent.width, 800)
                            spacing: 15
                            anchors.margins: 20
                            
                            Text {
                                text: canEdit ? "Edit System Information" : "System Information (Read Only)"
                                font.pixelSize: 18
                                font.bold: true
                                color: canEdit ? "#FF7F50" : "#888888"
                                Layout.fillWidth: true
                            }
                            
                            // Show claim status
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 60
                                color: canEdit ? "#224422" : (systemData && systemData.claimed ? "#442222" : "#444422")
                                radius: 8
                                border.width: 1
                                border.color: canEdit ? "#338833" : (systemData && systemData.claimed ? "#883333" : "#888833")
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (systemData && systemData.claimed) {
                                            if (isClaimedByUser) {
                                                return "You have claimed this system and can edit it."
                                            } else {
                                                return "This system is claimed by: " + (systemData.claimedBy || "another commander")
                                            }
                                        } else {
                                            return "This system is unclaimed. Claim it to add information."
                                        }
                                    }
                                    font.pixelSize: 14
                                    color: canEdit ? "#AAFFAA" : (systemData && systemData.claimed ? "#FFAAAA" : "#FFFFAA")
                                    wrapMode: Text.WordWrap
                                    width: parent.width - 40
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                            

                            
                            // Editable System Info (for claimed systems)
                            Rectangle {
                                visible: canEdit
                                Layout.fillWidth: true
                                Layout.preferredHeight: 300  // Good size for editing
                                color: "#1f1f1f"
                                radius: 8
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 10
                                    
                                    Text {
                                        text: "Edit System Information:"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#FF7F50"
                                    }
                                    
                                    ScrollView {
                                        id: systemInfoScrollView
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                        clip: true
                                        background: Rectangle { color: "transparent" }
                                        
                                        // Custom wheel handling
                                        function handleWheel(wheel) {
                                            if (!systemInfoEditor.activeFocus) {
                                                // Forward to main scroll when not focused
                                                mainScrollView.flick(0, wheel.angleDelta.y * 3)
                                                return true
                                            }
                                            return false // Let ScrollView handle it
                                        }
                                        
                                        TextArea {
                                            id: systemInfoEditor
                                            width: parent.width
                                            placeholderText: "Add detailed system information here..."
                                            text: systemInfo && systemInfo.system_info ? systemInfo.system_info : ""
                                            wrapMode: TextArea.Wrap
                                            selectByMouse: true
                                            font.pixelSize: 13
                                            font.family: "Consolas, Courier New, monospace"
                                            color: "#FFFFFF"
                                            placeholderTextColor: "#888888"
                                            leftPadding: 12
                                            rightPadding: 12
                                            topPadding: 12
                                            bottomPadding: 12
                                            
                                            background: Rectangle {
                                                color: "#0a0a0a"
                                                radius: 4
                                                border.width: systemInfoEditor.activeFocus ? 2 : 1
                                                border.color: systemInfoEditor.activeFocus ? "#FF7F50" : "#444444"
                                                
                                                Behavior on border.color {
                                                    ColorAnimation { duration: 200 }
                                                }
                                                Behavior on border.width {
                                                    NumberAnimation { duration: 200 }
                                                }
                                                
                                                // Click anywhere in background to focus
                                                MouseArea {
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.NoButton
                                                    onPressed: function(mouse) {
                                                        systemInfoEditor.forceActiveFocus()
                                                        mouse.accepted = false
                                                    }
                                                    onWheel: function(wheel) {
                                                        if (systemInfoScrollView.handleWheel(wheel)) {
                                                            wheel.accepted = true
                                                        } else {
                                                            wheel.accepted = false
                                                        }
                                                    }
                                                }
                                            }
                                            
                                            // Enable inner scroll when focused, disable when not focused
                                            onActiveFocusChanged: {
                                                mainScrollView.interactive = !activeFocus
                                            }
                                        }
                                    }
                                    
                                    Button {
                                        text: "Save System Info (Coming Soon)"
                                        Layout.alignment: Qt.AlignRight
                                        Layout.preferredWidth: 200
                                        Layout.preferredHeight: 35
                                        enabled: false // Temporarily disabled
                                        opacity: 0.5
                                        
                                        background: Rectangle {
                                            color: parent.pressed ? "#FF9068" : "#FF7F50"
                                            radius: 8
                                        }
                                        
                                        contentItem: Text {
                                            text: parent.text
                                            font.pixelSize: 12
                                            font.bold: true
                                            color: "#000000"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        
                                        onClicked: {
                                            console.log("Save system info feature is temporarily disabled")
                                            // TODO: Implement system info saving when database schema is ready
                                        }
                                    }
                                }
                            }
                            
                            // POI Status Section
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                color: "#1f1f1f"
                                radius: 8
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 10
                                    
                                    Text {
                                        text: canEdit ? "POI Status:" : "POI Status: (View Only)"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#FF7F50"
                                    }
                                    
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 15
                                        
                                        Button {
                                            text: "Mark as POI"
                                            Layout.preferredWidth: 120
                                            Layout.preferredHeight: 40
                                            enabled: canEdit
                                            visible: canEdit
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? "#4ade80" : "#22c55e"
                                                radius: 8
                                                border.width: 1
                                                border.color: "#16a34a"
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
                                                console.log("Marking system as POI:", systemName)
                                                edrhController.markSystemAsPOI(systemName, "POI")
                                            }
                                        }
                                        
                                        Button {
                                            text: "Mark as Potential POI"
                                            Layout.preferredWidth: 160
                                            Layout.preferredHeight: 40
                                            enabled: canEdit
                                            visible: canEdit
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? "#fb923c" : "#f97316"
                                                radius: 8
                                                border.width: 1
                                                border.color: "#ea580c"
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
                                                console.log("Marking system as Potential POI:", systemName)
                                                edrhController.markSystemAsPOI(systemName, "Potential POI")
                                            }
                                        }
                                        
                                        Button {
                                            text: "Remove POI Status"
                                            Layout.preferredWidth: 140
                                            Layout.preferredHeight: 40
                                            enabled: canEdit
                                            visible: canEdit
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? "#f87171" : "#ef4444"
                                                radius: 8
                                                border.width: 1
                                                border.color: "#dc2626"
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
                                                console.log("Removing POI status from system:", systemName)
                                                edrhController.removePOIStatus(systemName)
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // System Description Editor
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 250
                                color: "#1f1f1f"
                                radius: 8
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 10
                                    
                                    Text {
                                        text: "System Description:"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#FF7F50"
                                    }
                                    
                                    Rectangle {
                                        id: systemDescContainer
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        color: "#0a0a0a"
                                        radius: 4
                                        border.width: systemDescEditor.activeFocus ? 2 : 1
                                        border.color: systemDescEditor.activeFocus ? "#FF7F50" : "#444444"
                                        
                                        Behavior on border.color {
                                            ColorAnimation { duration: 200 }
                                        }
                                        Behavior on border.width {
                                            NumberAnimation { duration: 200 }
                                        }
                                        
                                        MouseArea {
                                            anchors.fill: parent
                                            acceptedButtons: Qt.LeftButton
                                            onClicked: systemDescEditor.forceActiveFocus()
                                            // Don't handle wheel events - let them pass through
                                        }
                                        
                                        // Separate MouseArea just for wheel events
                                        MouseArea {
                                            anchors.fill: parent
                                            acceptedButtons: Qt.NoButton
                                            onWheel: function(wheel) {
                                                if (!systemDescEditor.activeFocus) {
                                                    mainScrollView.flick(0, wheel.angleDelta.y * 3)
                                                    wheel.accepted = true
                                                } else {
                                                    // Let the ScrollView handle it when focused
                                                    wheel.accepted = false
                                                }
                                            }
                                        }
                                        
                                        ScrollView {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                            clip: true
                                            background: Rectangle { color: "transparent" }
                                            
                                            TextArea {
                                                id: systemDescEditor
                                                width: parent.width
                                                placeholderText: canEdit ? "Add system description..." : "No description available"
                                                text: systemDescription || ""
                                                wrapMode: TextArea.Wrap
                                                selectByMouse: true
                                                enabled: canEdit
                                                readOnly: !canEdit
                                                font.pixelSize: 13
                                                font.family: "Consolas, Courier New, monospace"
                                                color: "#FFFFFF"
                                                placeholderTextColor: "#888888"
                                                background: Rectangle { color: "transparent" }
                                            
                                            // Enable inner scroll when focused, disable when not focused
                                            onActiveFocusChanged: {
                                                mainScrollView.interactive = !activeFocus
                                            }
                                        }
                                    }
                                }
                                    
                                    Button {
                                        text: "Save Notes (Coming Soon)"
                                        Layout.alignment: Qt.AlignRight
                                        Layout.preferredWidth: 160
                                        Layout.preferredHeight: 35
                                        enabled: false // Temporarily disabled
                                        visible: canEdit
                                        opacity: 0.5
                                        
                                        background: Rectangle {
                                            color: parent.pressed ? "#FF9068" : "#FF7F50"
                                            radius: 8
                                        }
                                        
                                        contentItem: Text {
                                            text: parent.text
                                            font.pixelSize: 12
                                            font.bold: true
                                            color: "#000000"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        
                                        onClicked: {
                                            console.log("Save notes feature is temporarily disabled")
                                            // TODO: Implement notes saving when database schema is ready
                                            // edrhController.saveSystemDescription(systemName, descriptionEditor.text)
                                        }
                                    }
                                }
                            }
                            
                            // POI Description Editor
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 250
                                color: "#1f1f1f"
                                radius: 8
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 10
                                    
                                    Text {
                                        text: "POI Description:"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#FF7F50"
                                    }
                                    
                                    Rectangle {
                                        id: poiDescContainer
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        color: "#0a0a0a"
                                        radius: 4
                                        border.width: poiDescEditor.activeFocus ? 2 : 1
                                        border.color: poiDescEditor.activeFocus ? "#FF7F50" : "#444444"
                                        
                                        Behavior on border.color {
                                            ColorAnimation { duration: 200 }
                                        }
                                        Behavior on border.width {
                                            NumberAnimation { duration: 200 }
                                        }
                                        
                                        MouseArea {
                                            anchors.fill: parent
                                            acceptedButtons: Qt.LeftButton
                                            onClicked: poiDescEditor.forceActiveFocus()
                                            // Don't handle wheel events - let them pass through
                                        }
                                        
                                        // Separate MouseArea just for wheel events
                                        MouseArea {
                                            anchors.fill: parent
                                            acceptedButtons: Qt.NoButton
                                            onWheel: function(wheel) {
                                                if (!poiDescEditor.activeFocus) {
                                                    mainScrollView.flick(0, wheel.angleDelta.y * 3)
                                                    wheel.accepted = true
                                                } else {
                                                    // Let the ScrollView handle it when focused
                                                    wheel.accepted = false
                                                }
                                            }
                                        }
                                        
                                        ScrollView {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                            clip: true
                                            background: Rectangle { color: "transparent" }
                                            
                                            TextArea {
                                                id: poiDescEditor
                                                width: parent.width
                                                placeholderText: canEdit ? "Add POI-specific description..." : "No POI description available"
                                                text: ""
                                                wrapMode: TextArea.Wrap
                                                selectByMouse: true
                                                enabled: canEdit
                                                readOnly: !canEdit
                                                font.pixelSize: 13
                                                font.family: "Consolas, Courier New, monospace"
                                                color: "#FFFFFF"
                                                placeholderTextColor: "#888888"
                                                background: Rectangle { color: "transparent" }
                                            
                                            // Enable inner scroll when focused, disable when not focused
                                            onActiveFocusChanged: {
                                                mainScrollView.interactive = !activeFocus
                                            }
                                        }
                                    }
                                }
                                    
                                    Button {
                                        text: "Save POI Description (Coming Soon)"
                                        Layout.alignment: Qt.AlignRight
                                        Layout.preferredWidth: 220
                                        Layout.preferredHeight: 35
                                        enabled: false
                                        visible: canEdit
                                        opacity: 0.5
                                        
                                        background: Rectangle {
                                            color: parent.pressed ? "#FF9068" : "#FF7F50"
                                            radius: 8
                                        }
                                        
                                        contentItem: Text {
                                            text: parent.text
                                            font.pixelSize: 12
                                            font.bold: true
                                            color: "#000000"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        
                                        onClicked: {
                                            console.log("Save POI description feature is temporarily disabled")
                                        }
                                    }
                                }
                            }
                            
                            // Image Upload Section
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                color: "#1f1f1f"
                                radius: 8
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 10
                                    
                                    Text {
                                        text: "Upload Images:"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#FF7F50"
                                    }
                                    
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 15
                                        
                                        Button {
                                            text: "Choose Images"
                                            Layout.preferredWidth: 120
                                            Layout.preferredHeight: 40
                                            enabled: canEdit
                                            visible: canEdit
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? "#a855f7" : "#9333ea"
                                                radius: 8
                                                border.width: 1
                                                border.color: "#7c3aed"
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
                                                console.log("Opening multi-image picker for system:", systemName)
                                                multiImageDialog.open()
                                            }
                                        }
                                        
                                        Text {
                                            text: "Select multiple screenshots to display for this system"
                                            font.pixelSize: 12
                                            color: "#888888"
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
        }
        
        // Fixed bottom buttons
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: "#0a0a0a"
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 15
                
                Button {
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 45
                    text: {
                        if (edrhController.hasExistingClaim && !isClaimedByUser) {
                            return "Release Current Claim First"
                        } else if (isClaimedByUser) {
                            return "Unclaim System"
                        } else {
                            return "Claim System"
                        }
                    }
                    font.pixelSize: 14
                    font.bold: true
                    
                    enabled: {
                        // Can only click if:
                        // 1. System is claimed by us (to unclaim)
                        // 2. System is not claimed AND we don't have another claim
                        if (isClaimedByUser) {
                            return true
                        } else {
                            return !edrhController.hasExistingClaim
                        }
                    }
                    
                    background: Rectangle {
                        color: {
                            if (!parent.enabled) {
                                return "#666666"  // Gray when disabled
                            } else if (isClaimedByUser) {
                                return parent.pressed ? "#b91c1c" : "#dc3545"  // Red for unclaim
                            } else {
                                return parent.pressed ? "#16a34a" : "#28a745"  // Green for claim
                            }
                        }
                        radius: 8
                        border.color: {
                            if (!parent.enabled) {
                                return "#444444"
                            } else if (isClaimedByUser) {
                                return "#c82333"
                            } else {
                                return "#218838"
                            }
                        }
                        border.width: 1
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : "#AAAAAA"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        console.log("Claim button clicked for system:", systemName, "isClaimedByUser:", isClaimedByUser)
                        if (isClaimedByUser) {
                            console.log("Unclaiming system:", systemName)
                            edrhController.unclaimSystem(systemName)
                        } else if (!edrhController.hasExistingClaim) {
                            console.log("Claiming system:", systemName)
                            edrhController.claimSystem(systemName)
                        }
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                Button {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 45
                    text: "Close"
                    font.pixelSize: 14
                    font.bold: true
                    
                    background: Rectangle {
                        color: "#FF7F50"
                        radius: 8
                        border.color: "#FF5722"
                        border.width: 1
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "#000000"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: popupWindow.close()
                }
            }
        }
    }
}