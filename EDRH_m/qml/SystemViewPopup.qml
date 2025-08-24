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
    onUploadedImagesChanged: {/* quiet */}
    property bool handlingOwnNotifications: true  // Flag to indicate this popup handles its own notifications
    
    // Edit Info properties
    property string poiName: ""
    property string poiDescription: ""
    property string customSystemInfo: ""
    property bool isVisited: false
    property bool isDone: false
    property var allImages: []  // Combined user + preset images
    
    // Signal to force UI updates when claim status changes
    signal claimStatusChanged()
    signal forceUIRefresh()  // Force immediate UI refresh
    // Cooldown after claim/unclaim to avoid racing DB updates
    property bool claimCheckCooldown: false
    Timer {
        id: claimCooldownTimer
        interval: 600
        repeat: false
        onTriggered: claimCheckCooldown = false
    }
    
    property bool canEdit: {
        // Can only edit if system is claimed by current commander
        if (!edrhController || !edrhController.commanderName) return false
        
        // Use the isClaimedByUser property which is already calculated  
        // This will automatically update when isClaimedByUser changes
        return isClaimedByUser
    }
    
    // Additional connections for SupabaseClient signals
    Connections {
        target: (edrhController && edrhController.supabaseClient) ? edrhController.supabaseClient : null
        enabled: target !== null
        
        function onSystemStatusUpdated(sysName, success) {
            if (sysName === systemName && popupWindow.visible) {
                if (success) {
                    loadSystemData()
                } else {
                    console.warn("SystemViewPopup: update failed")
                }
            }
        }
        
        function onTakenSystemsReceived(takenSystems) {
            if (!popupWindow.visible || !systemName) return
            
            // minimal logging
            
            // Find our system in the data
            for (var i = 0; i < takenSystems.length; i++) {
                var takenSystem = takenSystems[i]
                
                if (takenSystem.system === systemName && takenSystem.by_cmdr === edrhController.commanderName) {
                    // keep essential match only
                    
                    // Update UI with database values - handle string/boolean conversion
                    isVisited = (takenSystem.visited === true || takenSystem.visited === "true")
                    isDone = (takenSystem.done === true || takenSystem.done === "true")
                    
                    // CRITICAL FIX: Update systemData with the edited flag from database
                    if (!systemData || typeof systemData !== 'object') {
                    systemData = {}
                    }
                    
                    // Convert string "true"/"false" to actual boolean
                    var editedValue = takenSystem.edited
                    if (typeof editedValue === 'string') {
                        systemData.edited = (editedValue.toLowerCase() === 'true')
                } else {
                    systemData.edited = (editedValue === true)
                }
                
                systemData.claimed = true
                systemData.claimedBy = edrhController.commanderName
                
                // updated edited/visited/done extracted
                    
                    // Trigger display update with the edited flag
                    updateSystemDisplay()
                    return
                }
            }
            
            // Not found in taken systems - set defaults
            // default values when not found
            isVisited = false
            isDone = false
            if (!systemData || typeof systemData !== 'object') {
                systemData = {}
            }
            systemData.edited = false
            systemData.claimed = false
            systemData.claimedBy = ""
        }
    }

    // Refresh claim state when ClaimManager emits claimStatusChanged
    Connections {
        target: (edrhController && edrhController.claimManager) ? edrhController.claimManager : null
        enabled: target !== null
        function onClaimStatusChanged(changedSystemName, isClaimed, claimedBy) {
            if (!popupWindow.visible) return
            if (!systemData || !systemData.name) return
            if (changedSystemName !== systemData.name) return
            var prev = isClaimedByUser
            isClaimedByUser = (isClaimed && claimedBy === edrhController.commanderName)
            if (prev !== isClaimedByUser) {
                console.log("SystemViewPopup: claimManager update -> isClaimedByUser:", isClaimedByUser)
                claimStatusChanged()
            }
        }
    }
    
    // JavaScript function to upload to imgbb using Qt.btoa for base64 encoding
    function uploadImageToImgbb(filePath, callback) {
        // For Qt, we need to use the SupabaseClient to handle file uploads
        // The original web-based approach won't work in Qt QML
        // Qt-based upload handled by SupabaseClient
        
        // Use the existing SupabaseClient upload method
        if (edrhController && edrhController.supabaseClient) {
            edrhController.uploadImageToImgbb(filePath, systemName)
        } else {
            console.error("No SupabaseClient available for upload")
            callback(null)
        }
    }
    
    function calculateDistance(x, y, z) {
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
        
        // Priority 2: Check systemData
        if (systemData && typeof systemData === 'object') {
            var x = systemData.x
            var y = systemData.y  
            var z = systemData.z
            if (x !== undefined && y !== undefined && z !== undefined) {
                return "X: " + x.toFixed(2) + ", Y: " + y.toFixed(2) + ", Z: " + z.toFixed(2)
            }
        }
        
        // Priority 3: Fallback to systemInfo
        if (systemInfo && typeof systemInfo === 'object') {
            var x = systemInfo.x
            var y = systemInfo.y  
            var z = systemInfo.z
            if (x !== undefined && y !== undefined && z !== undefined) {
                return "X: " + x.toFixed(2) + ", Y: " + y.toFixed(2) + ", Z: " + z.toFixed(2)
            }
        }
        
        // Try to get from EDSM
        edrhController.getEDSMSystemData(systemName)
        return "Coordinates: Fetching from EDSM..."
    }
    
    function getDistanceText() {
        // Calculate distance from Sol (0,0,0)
        if (systemData && typeof systemData === 'object') {
            var x = systemData.x
            var y = systemData.y  
            var z = systemData.z
            if (x !== undefined && y !== undefined && z !== undefined) {
                return calculateDistance(x, y, z)
            }
        }
        
        if (systemInfo && typeof systemInfo === 'object') {
            var x = systemInfo.x
            var y = systemInfo.y  
            var z = systemInfo.z
            if (x !== undefined && y !== undefined && z !== undefined) {
                return calculateDistance(x, y, z)
            }
        }
        
        return "Distance: Unknown (coordinates missing)"
    }
    
    function getCategoriesText() {
        if (systemData && systemData.categoryList && systemData.categoryList.length > 0) {
            var uniqueCategories = []
            for (var i = 0; i < systemData.categoryList.length; i++) {
                if (uniqueCategories.indexOf(systemData.categoryList[i]) === -1) {
                    uniqueCategories.push(systemData.categoryList[i])
                }
            }
            
            // If multiple categories, show combined name at top
            if (uniqueCategories.length >= 2) {
                // Create a combined category name from the individual categories
                return uniqueCategories.join(" and ")
            }
            
            return uniqueCategories.join(", ")
        }
        return category || "Unknown Category"
    }
    
    function getAllImages() {
        var images = []
        
        // First add user uploaded images
        for (var i = 0; i < uploadedImages.length; i++) {
            var imageUrl = uploadedImages[i]
            if (imageUrl.startsWith("http")) {
                images.push({
                    category: "User Upload " + (i + 1),
                    imageUrl: imageUrl,
                    isUserImage: true
                })
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
                
                var imageUrl = getPresetImageForCategory(categories[i])
                
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
                imageUrl: getPresetImageForCategory("Unknown"),
                isUserImage: false
            })
        }
        
        allImages = images
        return images
    }
    
    function getPresetImageForCategory(cat) {
        // Use DB-driven preset image URL via SupabaseClient cache
        if (edrhController && edrhController.supabaseClient) {
            var url = edrhController.supabaseClient.getSystemImageUrl(systemName, cat)
            return url || ""
        }
        return ""
    }
    
    function loadSystemData() {
        // Refresh fresh system data from DB
        
        if (!edrhController) {
            console.error("No edrhController available")
            return
        }
        
        if (!edrhController.supabaseClient) {
            console.error("No supabaseClient available")
            return
        }
        
        // targeted refresh for claim state
        try {
            edrhController.supabaseClient.getTakenSystemForCommander(systemName, edrhController.commanderName)
            // requested
        } catch (error) {
            console.error("SystemViewPopup: Error calling getTakenSystemForCommander:", error)
        }
        
        // CRITICAL FIX: Only call getSystemInformationFromDB first
        // We'll decide whether to load default data AFTER we know the edited status
        // robust details from DB first
        try {
            edrhController.supabaseClient.getSystemInformationFromDB(systemName)
            // requested
        } catch (error) {
            console.error("SystemViewPopup: Error calling getSystemInformationFromDB:", error)
        }
        
        // DON'T call getSystemInformation here - we'll call it conditionally after checking edited flag
        // NOTE: getSystemInformationFromDB above now includes POI fields for all users
    }
    
    function checkIfNeedDefaultData() {
        // decide whether default data is needed
        
        // Only load default data if edited flag is false/undefined AND we don't have default data yet
        var needsDefaultData = false
        
        if (!systemData || !systemData.hasOwnProperty('edited') || systemData.edited !== true) {
            // not edited => maybe need default
            
            if (!systemInfo || !systemInfo.hasOwnProperty('system_info')) {
                // load from category table
                needsDefaultData = true
            } else {
                // already have default
            }
        } else {
            // edited => skip default
        }
        
        if (needsDefaultData && category) {
            // load default by category
            try {
                edrhController.supabaseClient.getSystemInformation(systemName, category)
            } catch (error) {
                console.error("Error loading default system info:", error)
            }
        }
        
        // end decision
    }
    
    function saveEditInfoData() {
        if (!isClaimedByUser) return
        
        // saving edit info data
        
        // Prepare POI data if POI checkbox is checked
        var poiData = {}
        if (poiCheckbox.checked) {
            var poiType = poiTypeCombo.currentIndex === 0 ? "Potential POI" : "POI"
            poiData = {
                potential_or_poi: poiType,
                discoverer: discovererField.text || "",
                submitter: submitterField.text || ""
            }
            // include POI data
        } else {
            // Clear POI status
            poiData = {
                potential_or_poi: "",
                discoverer: "",
                submitter: ""
            }
            // clearing POI status
        }
        
        // Process images correctly - extract primary image and additional images
        var primaryImageUrl = ""
        var additionalImageUrls = []
        
        for (var i = 0; i < uploadedImages.length; i++) {
            if (uploadedImages[i].isPrimary) {
                primaryImageUrl = uploadedImages[i].url
            } else {
                additionalImageUrls.push(uploadedImages[i].url)
            }
        }
        
        var dataToSave = {
            name: poiName,  // maps to 'name' column
            description: poiDescription,  // maps to 'description' column  
            system_info: customSystemInfo,  // maps to 'system_info' column
            images: primaryImageUrl,  // PRIMARY IMAGE ONLY - single URL string
            additional_images: JSON.stringify(additionalImageUrls),  // Additional images as JSON array
            potential_or_poi: poiData.potential_or_poi,
            discoverer: poiData.discoverer,
            submitter: poiData.submitter
        }
        
        // send to C++
        
        // Save to system_information table with POI data
        edrhController.saveSystemInformation(systemName, dataToSave)
        
        // Update visited/done status in taken table
        edrhController.updateSystemStatus(systemName, isVisited, isDone)
        
        // CRITICAL: Mark as edited in taken table (this triggers smart fallback logic)
        edrhController.markSystemAsEdited(systemName)
        
        // Update local systemData to reflect edited status immediately
        if (!systemData || typeof systemData !== 'object') {
            systemData = {}
        }
        systemData.edited = true
        systemData.claimed = true
        systemData.claimedBy = edrhController.commanderName
        
        // saved; refresh System Info tab
        
        // NOW call updateSystemDisplay to refresh the System Info tab immediately
        updateSystemDisplay()
        
        // saved
    }
    
    function makeImagePrimary(index) {
        if (index < 0 || index >= uploadedImages.length) return
        if (uploadedImages[index].isPrimary) return // Already primary
        
        // set image primary
        
        // Find current primary and unset it
        for (var i = 0; i < uploadedImages.length; i++) {
            if (uploadedImages[i].isPrimary) {
                uploadedImages[i].isPrimary = false
                break
            }
        }
        
        // Set new primary
        uploadedImages[index].isPrimary = true
        
        // Update models
        imagesRepeater.model = uploadedImages.length
        imageManagerRepeater.model = uploadedImages.length
        
        // Force property update to trigger bindings
        uploadedImagesChanged()
        
        // Force repeater updates
        if (imageTitlesRepeater) {
            imageTitlesRepeater.model = uploadedImages.length
        }
        
        // Save to database
        saveImagesToDatabase()
    }
    
    function deleteImage(index) {
        if (index < 0 || index >= uploadedImages.length) return
        
        var imageToDelete = uploadedImages[index]
        var wasPrimary = imageToDelete.isPrimary
        // delete image
        
        // Remove from array
        uploadedImages.splice(index, 1)
        
        // If we deleted the primary image and there are still images left,
        // make the first one primary
        if (wasPrimary && uploadedImages.length > 0) {
            uploadedImages[0].isPrimary = true
            // promote first image to primary and sync
        }
        
        // Update model
        imagesRepeater.model = uploadedImages.length
        imageManagerRepeater.model = uploadedImages.length > 0 ? uploadedImages : []
        
        // Force property update to trigger bindings
        uploadedImagesChanged()
        
        // Force repeater updates
        if (imageTitlesRepeater) {
            imageTitlesRepeater.model = uploadedImages.length
        }
        
        // Save updated images to database
        saveImagesToDatabase()
    }
    
    function updateImageTitle(index, newTitle) {
        if (index < 0 || index >= uploadedImages.length) return
        
        // Title is already updated in the array by the TextInput
        // image title updated
        
        // Save to database after a delay to avoid too many updates while typing
        if (updateTitleTimer) updateTitleTimer.stop()
        updateTitleTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 1000; repeat: false }', popupWindow)
        updateTitleTimer.triggered.connect(function() { saveImagesToDatabase() })
        updateTitleTimer.start()
    }
    
    function saveImagesToDatabase() {
        if (!systemName) return
        
        // save images to database
        
        // Prepare the data
        var primaryImage = ""
        var primaryTitle = ""
        var additionalImages = []
        var additionalTitles = {}
        
        for (var i = 0; i < uploadedImages.length; i++) {
            // iterate images
            if (uploadedImages[i].isPrimary) {
                primaryImage = uploadedImages[i].url
                primaryTitle = uploadedImages[i].title || ""
                // set primary
            } else {
                additionalImages.push(uploadedImages[i].url)
                additionalTitles[additionalImages.length - 1] = uploadedImages[i].title || ""
            }
        }
        
        // Safety check - ensure we have a valid primary image
        if (!primaryImage && uploadedImages.length > 0) {
            console.warn("No primary image found though images exist")
            // Try to set the first image as primary
            if (uploadedImages[0] && uploadedImages[0].url) {
                primaryImage = uploadedImages[0].url
                primaryTitle = uploadedImages[0].title || ""
                // auto-fix primary
            }
        }
        
        // Critical validation - ensure primaryImage is a string, not an object
        if (typeof primaryImage !== "string") {
            console.warn("primaryImage not a string; fixing")
            if (primaryImage && primaryImage.url) {
                primaryImage = primaryImage.url
                // fixed
            } else {
                primaryImage = ""
                // reset primary
            }
        }
        
        // Validate additional images are strings
        for (var i = 0; i < additionalImages.length; i++) {
            if (typeof additionalImages[i] !== "string") {
                console.warn("additionalImages[" + i + "] not a string; skipping")
                if (additionalImages[i] && additionalImages[i].url) {
                    additionalImages[i] = additionalImages[i].url
                } else {
                    additionalImages.splice(i, 1)
                    i--
                }
            }
        }
        
        // Smart validation - only reject actual corruption, allow valid empty saves
        if (primaryImage && primaryImage.toString().includes("[object Object]")) {
            console.warn("About to save empty primaryImage")
            primaryImage = ""  // Clean the corruption but continue with save
        }
        
        // Check additional images for corruption but don't block save
        for (var i = 0; i < additionalImages.length; i++) {
            if (additionalImages[i] && additionalImages[i].toString().includes("[object Object]")) {
                console.warn("Skipping corrupted additionalImages[" + i + "]")
                additionalImages.splice(i, 1)  // Remove corrupted entry
                i--
            }
        }
        
        // after validation cleanup
        
        // Debug log what we're sending
        // saving images now
        
        // Call backend to save
        edrhController.updateSystemImages(systemName, primaryImage, primaryTitle, 
                                        JSON.stringify(additionalImages), 
                                        JSON.stringify(additionalTitles))
    }
    
    property var updateTitleTimer: null
    
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
        // component created for system
        
        // Load fresh data from database
        loadSystemData()
        
        updateSystemDisplay()
        
        // Check claim status
        Qt.callLater(function() {
            if (popupWindow.visible) {
                checkClaimStatus()
            }
        })
        
        // Suppress main app notifications
        edrhController.suppressMainAppNotifications = true
    }
    
    Component.onDestruction: {
        // Use a timer to delay re-enabling notifications to avoid race condition
        suppressionTimer.start()
    }
    
    // Timer to close upload progress popup
    Timer {
        id: uploadDelayTimer
        interval: 3000  // 3 seconds
        onTriggered: {
            uploadProgressPopup.close()
        }
    }
    
    // Timer to delay re-enabling main app notifications
    Timer {
        id: suppressionTimer
        interval: 2000  // 2 second delay to ensure server response is processed
        onTriggered: {
            if (edrhController) {
                console.log("SystemViewPopup: Timer re-enabling main app notifications")
                edrhController.suppressMainAppNotifications = false
            }
        }
    }
    
    // Copy popup - INSIDE the SystemViewPopup with proper z-order
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
            // multiple images selected
            uploadProgressText.text = "Uploading " + files.length + " images to imgbb..."
            uploadProgressPopup.open()
            
            // Upload each file using the SupabaseClient method
            var uploadedCount = 0
            for (var i = 0; i < files.length; i++) {
                var fileUrl = files[i].toString()
                // uploading image
                
                // Convert file URL to local path (remove file:/// prefix)
                var localPath = fileUrl.replace("file:///", "")
                
                // Use the SupabaseClient upload method directly
                if (edrhController) {
                    edrhController.uploadImageToImgbb(localPath, systemName)
                    uploadedCount++
                    uploadProgressText.text = "Uploading image " + uploadedCount + " of " + files.length + "..."
                } else {
                    console.error("No edrhController available for upload")
                    uploadProgressText.text = "Upload failed - no controller available"
                }
            }
            
            // For now, close the progress popup after a delay using Qt Timer
            // TODO: Connect to actual upload completion signals
            uploadDelayTimer.start()
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
    
    Connections {
        target: edrhController
        enabled: true  // ENABLE connections to handle notifications in popup
        
        function onSystemInformationReceived(sysName, sysInfo) {
            if (sysName === systemName) {
                console.log("SystemViewPopup: Received system information for", sysName)
                console.log("SystemViewPopup: hasInformation:", sysInfo ? sysInfo.hasInformation : "undefined")
                console.log("SystemViewPopup: Response type check - hasOwnProperty('id'):", sysInfo ? sysInfo.hasOwnProperty('id') : "no sysInfo")
                console.log("SystemViewPopup: system_info content:", sysInfo ? (sysInfo.system_info || "EMPTY") : "NO SYSINFO")
                
                // Determine if this is custom data from system_information table vs default data from category table
                var isCustomData = sysInfo && (sysInfo.hasOwnProperty('id') || (sysInfo.hasOwnProperty('system') && !sysInfo.hasOwnProperty('raw_data')))
                var isDefaultData = sysInfo && sysInfo.hasOwnProperty('raw_data') && sysInfo.hasInformation
                
                console.log("SystemViewPopup: isCustomData:", isCustomData, "isDefaultData:", isDefaultData)
                
                if (isCustomData) {
                    console.log("SystemViewPopup: Processing CUSTOM data from system_information table")
                    // Store the system info object
                    systemInfo = sysInfo
                    
                    // Load POI data from system_information using correct column names
                    if (sysInfo.name) {
                        poiName = sysInfo.name
                        console.log("Loaded POI name:", poiName)
                    }
                    if (sysInfo.description) {
                        poiDescription = sysInfo.description
                        console.log("Loaded POI description:", poiDescription)
                    }
                    
                    // Load POI status and related fields
                    if (sysInfo.potential_or_poi) {
                        var poiStatus = sysInfo.potential_or_poi
                        poiCheckbox.checked = (poiStatus === "Potential POI" || poiStatus === "POI")
                        if (poiCheckbox.checked) {
                            poiTypeCombo.currentIndex = (poiStatus === "POI") ? 1 : 0
                        }
                        console.log("Loaded POI status:", poiStatus, "checkbox:", poiCheckbox.checked)
                    } else {
                        poiCheckbox.checked = false
                    }
                    
                    if (sysInfo.discoverer) {
                        discovererField.text = sysInfo.discoverer
                        console.log("Loaded discoverer:", sysInfo.discoverer)
                    }
                    
                    if (sysInfo.submitter) {
                        submitterField.text = sysInfo.submitter
                        console.log("Loaded submitter:", sysInfo.submitter)
                    }
                    
                    // Load custom system info - this is the user's edited content
                    if (sysInfo.system_info !== undefined) {
                        customSystemInfo = sysInfo.system_info || ""
                        console.log("Loaded CUSTOM system info from database:", customSystemInfo)
                    }
                    
                    // Load images from database
                    uploadedImages = []  // Clear existing images
                    if (sysInfo.images && sysInfo.images.trim() !== "") {
                        // Check for corrupted data
                        if (sysInfo.images.includes("[object Object]")) {
                            console.log("WARNING: Corrupted primary image data detected:", sysInfo.images)
                            console.log("Skipping corrupted primary image - will preserve valid additional images")
                            // Don't add the corrupted primary image, but continue loading additional images
                        } else {
                            uploadedImages.push({
                                url: sysInfo.images,
                                title: sysInfo.image_title || "",
                                isPrimary: true
                            })
                            console.log("Loaded primary image:", sysInfo.images)
                            console.log("After loading primary image, uploadedImages.length:", uploadedImages.length)
                        }
                    }
                    
                    // Load additional images
                    if (sysInfo.additional_images) {
                        try {
                            var additionalImgs = JSON.parse(sysInfo.additional_images)
                            var additionalTitles = {}
                            
                            // Parse additional_image_titles properly
                            if (sysInfo.additional_image_titles) {
                                if (typeof sysInfo.additional_image_titles === "string") {
                                    // If it's a string, try to parse it
                                    if (sysInfo.additional_image_titles === "[object Object]") {
                                        // This means it was saved incorrectly, default to empty
                                        additionalTitles = {}
                                        console.log("Found corrupted additional_image_titles, using empty object")
                                    } else {
                                        try {
                                            additionalTitles = JSON.parse(sysInfo.additional_image_titles)
                                        } catch (e) {
                                            console.log("Failed to parse additional_image_titles string:", e)
                                            additionalTitles = {}
                                        }
                                    }
                                } else if (typeof sysInfo.additional_image_titles === "object") {
                                    // Already an object, use as-is
                                    additionalTitles = sysInfo.additional_image_titles
                                }
                            }
                            
                            if (Array.isArray(additionalImgs)) {
                                var hasCorruption = false
                                for (var i = 0; i < additionalImgs.length; i++) {
                                    // Check for corrupted URLs
                                    if (additionalImgs[i] && additionalImgs[i].toString().includes("[object Object]")) {
                                        console.log("WARNING: Corrupted additional image URL detected:", additionalImgs[i])
                                        hasCorruption = true
                                    } else {
                                        uploadedImages.push({
                                            url: additionalImgs[i],
                                            title: additionalTitles[i.toString()] || additionalTitles[i] || "",
                                            isPrimary: false
                                        })
                                    }
                                }
                                
                                if (hasCorruption) {
                                    console.log("Found corrupted additional images - they were skipped during loading")
                                    console.log("Valid images preserved:", uploadedImages.length)
                                    // Note: Corrupted images are automatically excluded from uploadedImages above
                                    // No need to trigger a database save that would clear all images
                                }
                                
                                console.log("Loaded", uploadedImages.length, "additional images (after cleanup)")
                                console.log("Additional titles:", JSON.stringify(additionalTitles))
                            }
                        } catch (e) {
                            console.log("Failed to parse additional_images:", e)
                        }
                    }
                    
                    // If we have images but no primary (due to corruption), make the first one primary
                    if (uploadedImages.length > 0) {
                        var hasPrimary = false
                        for (var j = 0; j < uploadedImages.length; j++) {
                            if (uploadedImages[j].isPrimary) {
                                hasPrimary = true
                                break
                            }
                        }
                        if (!hasPrimary) {
                            console.log("No primary image found, promoting first image to primary")
                            uploadedImages[0].isPrimary = true
                        }
                    }
                    
                    // Update image displays
                    imagesRepeater.model = uploadedImages.length
                    imageManagerRepeater.model = uploadedImages.length > 0 ? uploadedImages : []
                    
                    // Force property update to trigger bindings
                    uploadedImagesChanged()
                    console.log("After loading all images, uploadedImages.length:", uploadedImages.length)
                    
                    // Force repeater updates
                    if (imageTitlesRepeater) {
                        imageTitlesRepeater.model = uploadedImages.length
                    }
                    
                    // Check if we need default fallback data
                    checkIfNeedDefaultData()
                    
                } else if (isDefaultData) {
                    console.log("SystemViewPopup: Processing DEFAULT data from category table")
                    // Store the default system info separately, don't overwrite customSystemInfo
                    systemInfo = sysInfo
                    console.log("Stored default system info for fallback use")
                    
                } else {
                    console.log("SystemViewPopup: Unknown data type, storing as systemInfo")
                    systemInfo = sysInfo
                    
                    // Check if we need default fallback data
                    checkIfNeedDefaultData()
                }
                
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
        
        // Handle claim success/failure notifications - show in popup overlay
        function onSystemClaimed(sysName, success) {
            if (sysName === systemName) {
                console.log("SystemViewPopup: Received systemClaimed signal - showing popup notification")
                if (success) {
                    claimPopup.messageText = "Successfully claimed " + systemName + "!"
                    claimPopup.isSuccess = true
                    
                    // Update local state immediately
                    isClaimedByUser = true
                    
                    // Force immediate UI update
                    popupWindow.canEdit = true
                    
                    // Update systemData to reflect claimed status
                    if (systemData && typeof systemData === 'object') {
                        systemData.claimed = true
                        systemData.claimedBy = edrhController.commanderName
                    }
                    
                    // Start short cooldown before any re-check runs
                    claimCheckCooldown = true
                    claimCooldownTimer.restart()
                    console.log("SystemViewPopup: Local state updated after claim; cooldown started")
                } else {
                    claimPopup.messageText = "Failed to claim " + systemName
                    claimPopup.isSuccess = false
                }
                claimPopup.open()  // Show the popup notification
            }
        }
        
        function onSystemUnclaimed(sysName, success) {
            if (sysName === systemName) {
                console.log("SystemViewPopup: Received systemUnclaimed signal - success:", success)
                console.log("SystemViewPopup: Popup visible before processing:", popupWindow.visible)
                
                if (success) {
                    claimPopup.messageText = "Successfully released claim on " + systemName + "!"
                    claimPopup.isSuccess = true
                    
                    // Update local state immediately
                    isClaimedByUser = false
                    
                    // Update systemData to reflect unclaimed status
                    if (systemData && typeof systemData === 'object') {
                        systemData.claimed = false
                        systemData.claimedBy = ""
                    }
                    
                    // Start short cooldown before any re-check runs
                    claimCheckCooldown = true
                    claimCooldownTimer.restart()
                    console.log("SystemViewPopup: Local state updated after successful unclaim; cooldown started")
                } else {
                    // FAILURE CASE: Don't update local state, server will restore it
                    claimPopup.messageText = "Failed to release claim on " + systemName + ": Server says no claim found"
                    claimPopup.isSuccess = false
                    console.log("SystemViewPopup: Unclaim failed, keeping current state")
                    
                    // Don't update isClaimedByUser or systemData - let the server response handle restoration
                }
                
                console.log("SystemViewPopup: About to show claim popup, main popup visible:", popupWindow.visible)
                claimPopup.open()  // Show the popup notification
                
                console.log("SystemViewPopup: After showing claim popup, main popup visible:", popupWindow.visible)
            }
        }
        
        function onSystemImageSet(sysName, imageUrl, success) {
            console.log("=== onSystemImageSet CALLED ===")
            console.log("sysName:", sysName, "systemName:", systemName)
            console.log("imageUrl:", imageUrl)
            console.log("success:", success)
            console.log("Condition check:", sysName === systemName && success && imageUrl)
            
            if (sysName === systemName && success && imageUrl) {
                console.log("Image uploaded and saved for", sysName, ":", imageUrl)
                
                // If no images exist, this becomes the primary image
                if (uploadedImages.length === 0) {
                    uploadedImages.push({
                        url: imageUrl,
                        title: "",
                        isPrimary: true
                    })
                } else {
                    // Otherwise add as additional image
                    uploadedImages.push({
                        url: imageUrl,
                        title: "",
                        isPrimary: false
                    })
                }
                
                // Update both image displays
                imagesRepeater.model = uploadedImages.length
                imageManagerRepeater.model = uploadedImages.length > 0 ? uploadedImages : []
                
                // Force property update to trigger bindings
                uploadedImagesChanged()
                console.log("After adding new image, uploadedImages.length:", uploadedImages.length)
                console.log("uploadedImages contents:", JSON.stringify(uploadedImages))
                
                // Force repeater updates
                if (imageTitlesRepeater) {
                    imageTitlesRepeater.model = uploadedImages.length
                }
                
                // Close the upload progress popup
                uploadProgressPopup.close()
            }
        }
        
        // Listen for general system updates (like when taken systems are refreshed)
        function onSystemUpdated() {
            // Only refresh claim status if popup is still open to avoid interference
            if (popupWindow.visible) {
                console.log("SystemViewPopup: System update notification received, refreshing claim status")
                // Add a small delay to ensure the popup doesn't close due to rapid updates
                Qt.callLater(function() {
                    if (popupWindow.visible) {  // Double-check it's still visible
                        // CRITICAL FIX: Only call checkClaimStatus ONCE to prevent oscillation
                        checkClaimStatus()
                        // Don't call additional UI refresh signals - checkClaimStatus handles that
                    }
                })
            } else {
                console.log("SystemViewPopup: Ignoring system update - popup not visible")
            }
        }
    }
    
    // Claim notification popup - PROPER Z-ORDER FIX
    Popup {
        id: claimPopup
        parent: Overlay.overlay  // Use the application's main overlay instead
        anchors.centerIn: parent
        width: 350
        height: 180
        modal: true  // Make it truly modal
        z: 100000  // Extremely high z-order
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        property string messageText: ""
        property bool isSuccess: true
        
        background: Rectangle {
            color: "#141414"
            radius: 15
            border.width: 2
            border.color: claimPopup.isSuccess ? "#22c55e" : "#ef4444"
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15
            
            Text {
                text: claimPopup.isSuccess ? "Success" : "Error"
                font.pixelSize: 18
                font.bold: true
                color: claimPopup.isSuccess ? "#22c55e" : "#ef4444"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            
            Text {
                text: claimPopup.messageText
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
                    color: parent.pressed ? (claimPopup.isSuccess ? "#16a34a" : "#dc2626") : (claimPopup.isSuccess ? "#22c55e" : "#ef4444")
                    radius: 8
                }
                
                contentItem: Text {
                    text: parent.text
                    font.bold: true
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: claimPopup.close()
            }
        }
        
        // Auto-close after 3 seconds
        Timer {
            id: claimAutoCloseTimer
            interval: 3000
            running: claimPopup.opened
            onTriggered: claimPopup.close()
        }
    }
    
    // POI Help popup
    Popup {
        id: poiHelpPopup
        parent: popupWindow.contentItem
        anchors.centerIn: parent
        width: 500
        height: 300
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        property string helpText: ""
        
        background: Rectangle {
            color: "#141414"
            radius: 15
            border.width: 2
            border.color: "#FF7F50"
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 20
            
            Text {
                text: "POI Information"
                font.pixelSize: 18
                font.bold: true
                color: "#FF7F50"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                clip: true
                contentWidth: availableWidth
                
                Text {
                    width: poiHelpPopup.width - 40  // Account for popup margins
                    text: poiHelpPopup.helpText
                    font.pixelSize: 14
                    color: "#FFFFFF"
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
            }
            
            Button {
                text: "Got it"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 100
                Layout.preferredHeight: 35
                
                background: Rectangle {
                    color: parent.pressed ? "#FF9068" : "#FF7F50"
                    radius: 8
                }
                
                contentItem: Text {
                    text: parent.text
                    font.bold: true
                    font.pixelSize: 14
                    color: "#000000"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: poiHelpPopup.close()
            }
        }
    }
    
    function updateSystemDisplay() {
        var displayText = ""
        
        // Check if we have multiple categories with system information
        if (systemData && systemData.categoryList && systemData.categoryList.length >= 2) {
            var uniqueCategories = []
            for (var i = 0; i < systemData.categoryList.length; i++) {
                if (uniqueCategories.indexOf(systemData.categoryList[i]) === -1) {
                    uniqueCategories.push(systemData.categoryList[i])
                }
            }
            
            // Build system info text for each category
            for (var j = 0; j < uniqueCategories.length; j++) {
                if (j > 0) displayText += "\n\n" // Add spacing between categories
                
                displayText += "=== " + uniqueCategories[j] + " ===\n"
                
                // Add category-specific information (you can expand this based on your data structure)
                displayText += "Category: " + uniqueCategories[j] + "\n"
                displayText += "System: " + systemName + "\n"
                
                // Add any additional category-specific details here
                if (systemInfo && systemInfo.system_info) {
                    displayText += "Details: " + systemInfo.system_info + "\n"
                }
            }
            
            systemInfoText.text = displayText
        } else {
            // SMART FALLBACK LOGIC - THIS IS THE KEY FIX!
            var useCustomInfo = false
            var editedFlag = false
            
            console.log("=== HELLO my name is burger - FALLBACK LOGIC ===")
            console.log("systemData:", JSON.stringify(systemData))
            console.log("systemInfo:", JSON.stringify(systemInfo))
            console.log("customSystemInfo variable:", customSystemInfo)
            
            // Check if the system has been edited by the current user
            // First check systemData for the edited flag
            if (systemData && systemData.hasOwnProperty('edited')) {
                editedFlag = systemData.edited === true
                console.log("Found edited flag in systemData:", editedFlag, "(raw value:", systemData.edited, ")")
            } else {
                console.log("No edited flag found in systemData")
            }
            console.log("=== END HELLO my name is burger - FALLBACK LOGIC ===")
            
            // RULE: If edited flag is true, use custom info (even if empty)
            if (editedFlag) {
                useCustomInfo = true
                console.log("System marked as edited - using custom system info")
                
                // Use the current customSystemInfo variable instead of systemInfo.system_info
                var currentCustomInfo = customSystemInfo || ""
                console.log("Current customSystemInfo variable:", currentCustomInfo)
                
                if (currentCustomInfo === "") {
                    systemInfoText.text = "(Custom system information cleared by user)"
                } else {
                    systemInfoText.text = currentCustomInfo
                }
                console.log("Using custom system info:", currentCustomInfo)
            } else {
                // RULE: If edited flag is false or not set, use default info
                console.log("System not edited - using default system info")
                
                // Check if we have default system info from the category/database
                if (systemInfo && systemInfo.hasInformation && systemInfo.system_info) {
                    systemInfoText.text = systemInfo.system_info
                    console.log("Using default system info from database")
                } else {
                    systemInfoText.text = "Basic system information displayed above. No additional details available in database."
                    console.log("No default system info available")
                }
            }
            
            // Load POI data from system_information for editing
            if (systemInfo) {
                if (systemInfo.name) {
                    poiName = systemInfo.name || ""
                }
                if (systemInfo.description) {
                    poiDescription = systemInfo.description || ""
                }
                
                // For the edit area, populate with appropriate content
                if (useCustomInfo && systemInfo.hasOwnProperty('system_info')) {
                    customSystemInfo = systemInfo.system_info || ""
                } else if (!useCustomInfo && systemInfoText.text !== "Basic system information displayed above. No additional details available in database.") {
                    // Pre-populate with default info for editing
                    customSystemInfo = systemInfoText.text
                } else {
                    customSystemInfo = ""
                }
            }
        }
        
        console.log("=== HELLO my name is burger - DISPLAY COMPLETE ===")
        console.log("updateSystemDisplay completed - edited flag:", editedFlag, "useCustomInfo:", useCustomInfo)
        console.log("Final systemInfoText.text:", systemInfoText.text)
        console.log("=== END HELLO my name is burger - DISPLAY COMPLETE ===")
    }
    
    // Timer to debounce checkClaimStatus calls and prevent oscillation
    Timer {
        id: claimStatusDebounceTimer
        interval: 100  // 100ms debounce
        onTriggered: {
            performClaimStatusCheck()
        }
    }
    
    // The actual claim status check logic
    function performClaimStatusCheck() {
        var oldClaimStatus = isClaimedByUser
        
        // Use ClaimManager as the single source of truth for claim status
        if (edrhController && edrhController.claimManager) {
            isClaimedByUser = edrhController.claimManager.isSystemClaimedByUser(systemName)
            var claimedBy = edrhController.claimManager.getSystemClaimedBy(systemName)
            var isCompleted = edrhController.claimManager.isSystemCompleted(systemName)
        } else {
            isClaimedByUser = false
        }
        
        // If claim status changed, emit signal to force UI update but DON'T close popup
        if (oldClaimStatus !== isClaimedByUser) {
            console.warn("Claim status changed:", systemName, "->", isClaimedByUser)
            claimStatusChanged()
        }
    }
    
    // Debounced version that prevents oscillation
    function checkClaimStatus() {
        if (claimCheckCooldown) {
            return
        }
        claimStatusDebounceTimer.stop()
        claimStatusDebounceTimer.start()
    }
    
    // Main layout
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 0
        
        // Content area
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
                            color: "#FF7F50"
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
            
            // Tab bar
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
                    visible: canEdit && isClaimedByUser  // Only show if system is claimed by user
                }
            }
            
            // Tab content
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: tabBar.currentIndex
                
                // Update display when switching to System Info tab
                onCurrentIndexChanged: {
                    if (currentIndex === 0) {  // System Info tab
                        console.log("Switched to System Info tab - updating display")
                        updateSystemDisplay()
                    }
                }
                
                // System Info tab
                Item {
                    Flickable {
                        id: systemInfoFlickable
                        anchors.fill: parent
                        anchors.margins: 20
                        contentHeight: systemInfoContent.height
                        contentWidth: width
                        clip: true
                        
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }
                        
                        ColumnLayout {
                            id: systemInfoContent
                            width: parent.width
                            spacing: 20
                        
                        // System details grid with equal spacing
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 15
                            columnSpacing: 30
                            
                            // Row 1 - Equal width columns
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 80
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
                                        
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                edrhController.copyToClipboard(systemName)
                                                copyPopup.messageText = "Copied: " + systemName
                                                copyPopup.open()
                                            }
                                            onEntered: parent.color = "#FFD700"
                                            onExited: parent.color = "#FFFFFF"
                                        }
                                    }
                                }
                            }
                            
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 80
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
                                Layout.preferredHeight: 80
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
                                Layout.preferredHeight: 80
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
                            
                            // Row 4 - POI Status Display (visible to all users)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: poiStatusVisible ? 180 : 0
                                Layout.columnSpan: 2
                                color: "#1f1f1f"
                                radius: 8
                                visible: poiStatusVisible
                                
                                property bool poiStatusVisible: {
                                    console.log("=== POI VISIBILITY DEBUG ===")
                                    console.log("systemInfo exists:", systemInfo ? "yes" : "no")
                                    console.log("systemName being checked:", systemName)
                                    
                                    // CRITICAL FIX: Check multiple data sources for POI information
                                    var sources = [systemInfo, systemData]
                                    var foundPOI = false
                                    var foundDiscoverer = false
                                    var foundSubmitter = false
                                    
                                    for (var i = 0; i < sources.length; i++) {
                                        var source = sources[i]
                                        if (source) {
                                            console.log("Checking source", i, ":", JSON.stringify(source))
                                            
                                            if (source.potential_or_poi && source.potential_or_poi.trim() !== "") {
                                                foundPOI = true
                                                console.log("Found POI in source", i, ":", source.potential_or_poi)
                                            }
                                            if (source.discoverer && source.discoverer.trim() !== "") {
                                                foundDiscoverer = true
                                                console.log("Found discoverer in source", i, ":", source.discoverer)
                                            }
                                            if (source.submitter && source.submitter.trim() !== "") {
                                                foundSubmitter = true
                                                console.log("Found submitter in source", i, ":", source.submitter)
                                            }
                                        }
                                    }
                                    
                                    // ALSO check the POI form fields if they're filled (for Edit Info tab)
                                    if (poiCheckbox && poiCheckbox.checked) {
                                        foundPOI = true
                                        console.log("Found POI from checkbox")
                                        if (discovererField && discovererField.text && discovererField.text.trim() !== "") {
                                            foundDiscoverer = true
                                        }
                                        if (submitterField && submitterField.text && submitterField.text.trim() !== "") {
                                            foundSubmitter = true
                                        }
                                    }
                                    
                                    var result = foundPOI || foundDiscoverer || foundSubmitter
                                    console.log("POI visibility calculation: foundPOI:", foundPOI, "foundDiscoverer:", foundDiscoverer, "foundSubmitter:", foundSubmitter)
                                    console.log("POI visible result:", result)
                                    console.log("=== END POI VISIBILITY DEBUG ===")
                                    
                                    return result
                                }
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 10
                                    
                                    RowLayout {
                                        Layout.fillWidth: true
                                        
                                        Rectangle {
                                            Layout.preferredWidth: 8
                                            Layout.preferredHeight: 8
                                            radius: 4
                                            color: {
                                                var poiType = ""
                                                if (systemInfo && systemInfo.potential_or_poi) {
                                                    poiType = systemInfo.potential_or_poi
                                                } else if (systemData && systemData.potential_or_poi) {
                                                    poiType = systemData.potential_or_poi
                                                }
                                                return poiType === "POI" ? "#22c55e" : "#f59e0b"
                                            }
                                        }
                                        
                                        Text {
                                            text: {
                                                var poiType = ""
                                                // Check multiple sources for POI type
                                                if (systemInfo && systemInfo.potential_or_poi) {
                                                    poiType = systemInfo.potential_or_poi
                                                } else if (systemData && systemData.potential_or_poi) {
                                                    poiType = systemData.potential_or_poi
                                                }
                                                return "Point of Interest" + (poiType ? " (" + poiType + ")" : "")
                                            }
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: "#FF7F50"
                                        }
                                    }
                                    
                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        columnSpacing: 20
                                        rowSpacing: 5
                                        
                                        Text {
                                            text: "Discoverer:"
                                            font.pixelSize: 12
                                            color: "#AAAAAA"
                                        }
                                        
                                        Text {
                                            text: {
                                                var discoverer = "Unknown"
                                                if (systemInfo && systemInfo.discoverer) {
                                                    discoverer = systemInfo.discoverer
                                                } else if (systemData && systemData.discoverer) {
                                                    discoverer = systemData.discoverer
                                                }
                                                return discoverer
                                            }
                                            font.pixelSize: 12
                                            color: "#FFFFFF"
                                            Layout.fillWidth: true
                                        }
                                        
                                        Text {
                                            text: "Submitter:"
                                            font.pixelSize: 12
                                            color: "#AAAAAA"
                                        }
                                        
                                        Text {
                                            text: {
                                                var submitter = "Unknown"
                                                if (systemInfo && systemInfo.submitter) {
                                                    submitter = systemInfo.submitter
                                                } else if (systemData && systemData.submitter) {
                                                    submitter = systemData.submitter
                                                }
                                                return submitter
                                            }
                                            font.pixelSize: 12
                                            color: "#FFFFFF"
                                            Layout.fillWidth: true
                                        }
                                        
                                        Text {
                                            text: "POI Name:"
                                            font.pixelSize: 12
                                            color: "#AAAAAA"
                                            visible: poiName && poiName.trim() !== ""
                                        }
                                        
                                        Text {
                                            text: poiName || "Not specified"
                                            font.pixelSize: 12
                                            color: "#FFFFFF"
                                            Layout.fillWidth: true
                                            visible: poiName && poiName.trim() !== ""
                                        }
                                        
                                        Text {
                                            text: "POI Description:"
                                            font.pixelSize: 12
                                            color: "#AAAAAA"
                                            Layout.columnSpan: 2
                                            visible: poiDescription && poiDescription.trim() !== ""
                                        }
                                        
                                        Text {
                                            id: poiDescText
                                            Layout.fillWidth: true
                                            Layout.columnSpan: 2
                                            text: poiDescription || "No description available"
                                            font.pixelSize: 12
                                            color: "#FFFFFF"
                                            wrapMode: Text.WordWrap
                                            visible: poiDescription && poiDescription.trim() !== ""
                                        }
                                    }
                                }
                            }
                            
                            // Row 5 - Status Information spanning both columns
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 100
                                Layout.columnSpan: 2
                                color: "#1f1f1f"
                                radius: 8
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    spacing: 10
                                    
                                    Text {
                                        text: "Status Information:"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#FF7F50"
                                    }
                                    
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 20
                                        
                                        // Claim Status
                                        Rectangle {
                                            id: claimStatusChip
                                            Layout.preferredWidth: Math.max(150, claimStatusText.implicitWidth + 20)
                                            Layout.preferredHeight: 30
                                            color: {
                                                if (isClaimedByUser) {
                                                    return "#2d2d2d"  // Neutral for your claim (button handles green)
                                                } else if (systemData && systemData.claimed) {
                                                    return "#442222"  // Red for others' claims
                                                } else {
                                                    return "#444422"  // Yellow for unclaimed
                                                }
                                            }
                                            radius: 6
                                            border.width: 1
                                            border.color: {
                                                if (isClaimedByUser) {
                                                    return "#888888" // Neutral border
                                                } else if (systemData && systemData.claimed) {
                                                    return "#883333"
                                                } else {
                                                    return "#888833"
                                                }
                                            }
                                            
                                            clip: true
                                            Text {
                                                anchors.centerIn: parent
                                                id: claimStatusText
                                                text: {
                                                    if (isClaimedByUser) {
                                                        return "✓ This system is all yours"
                        } else if (systemData && systemData.claimed) {
                            var by = systemData.claimedBy || "Unknown"
                            if (by.toLowerCase && by.toLowerCase() === "empty") {
                                return "Unclaimed"
                            }
                            return "Claimed by " + by
                                                    } else {
                                                        return "Unclaimed"
                                                    }
                                                }
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideRight
                                                color: {
                                                    if (isClaimedByUser) {
                                                        return "#22c55e" // green
                                                    } else if (systemData && systemData.claimed) {
                                                        return "#FFAAAA"
                                                    } else {
                                                        return "#FFFFAA"
                                                    }
                                                }
                                            }
                                        }
                                        
                                        // Visit Status
                                        Rectangle {
                                            id: visitStatusChip
                                            Layout.preferredWidth: Math.max(100, visitStatusText.implicitWidth + 20)
                                            Layout.preferredHeight: 30
                                            color: isVisited ? "#224422" : "#442222"
                                            radius: 6
                                            border.width: 1
                                            border.color: isVisited ? "#338833" : "#883333"
                                            clip: true
                                            Text {
                                                anchors.centerIn: parent
                                                id: visitStatusText
                                                text: isVisited ? "✓ Visited" : "Not Visited"
                                                font.pixelSize: 11
                                                font.bold: true
                                                color: isVisited ? "#AAFFAA" : "#FFAAAA"
                                            }
                                        }
                                        
                                        // Completion Status
                                        Rectangle {
                                            id: doneStatusChip
                                            Layout.preferredWidth: Math.max(100, doneStatusText.implicitWidth + 20)
                                            Layout.preferredHeight: 30
                                            color: isDone ? "#224422" : "#442222"
                                            radius: 6
                                            border.width: 1
                                            border.color: isDone ? "#338833" : "#883333"
                                            clip: true
                                            Text {
                                                anchors.centerIn: parent
                                                id: doneStatusText
                                                text: isDone ? "✓ Completed" : "In Progress"
                                                font.pixelSize: 11
                                                font.bold: true
                                                color: isDone ? "#AAFFAA" : "#FFAAAA"
                                            }
                                        }
                                        
                                        Item { Layout.fillWidth: true }
                                    }
                                }
                            }
                        }
                        
                        // System Information Display
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(300, systemInfoColumn.implicitHeight)
                            Layout.minimumHeight: 300
                            color: "#1f1f1f"
                            radius: 8
                            
                            ColumnLayout {
                                id: systemInfoColumn
                                anchors.fill: parent
                                anchors.margins: 15
                                spacing: 10
                                Layout.alignment: Qt.AlignTop
                                
                                Text {
                                    text: "System Information:"
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "#FF7F50"
                                    bottomPadding: 5
                                }
                                
                                Text {
                                    id: systemInfoText
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                                    text: "Loading system information..."
                                    font.pixelSize: 13
                                    font.family: "Consolas, Courier New, monospace"
                                    color: "#FFFFFF"
                                    wrapMode: Text.WordWrap
                                    textFormat: Text.PlainText
                                    verticalAlignment: Text.AlignTop
                                }
                            }
                        }
                        }  // End ColumnLayout
                    }  // End Flickable
                }
                
                // Images tab - aaaaaaaaaaaaaaaaaaaaaa
                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 20
                        
                        Text {
                            text: "System Images"
                            font.pixelSize: 18
                            font.bold: true
                            color: "#FF7F50"
                        }
                        
                        // Image gallery area
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 400
                            color: "#1f1f1f"
                            radius: 8
                            
                            ScrollView {
                                id: imageScrollView
                                anchors.fill: parent
                                anchors.margins: 10
                                clip: true
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOn
                                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                                
                                // Custom wheel handler for horizontal scrolling
                                WheelHandler {
                                    target: null
                                    onWheel: {
                                        // Convert vertical wheel to horizontal scroll
                                        var delta = event.angleDelta.y / 120 * 50 // 50 pixels per wheel notch
                                        imageScrollView.ScrollBar.horizontal.position -= delta / imageScrollView.contentWidth
                                        
                                        // Clamp to valid range
                                        if (imageScrollView.ScrollBar.horizontal.position < 0) {
                                            imageScrollView.ScrollBar.horizontal.position = 0
                                        } else if (imageScrollView.ScrollBar.horizontal.position > 1 - imageScrollView.ScrollBar.horizontal.size) {
                                            imageScrollView.ScrollBar.horizontal.position = 1 - imageScrollView.ScrollBar.horizontal.size
                                        }
                                    }
                                }
                                
                                Row {
                                    id: imageRow
                                    spacing: 15
                                    height: 380
                                    
                                    // User uploaded images
                                    Repeater {
                                        id: imagesRepeater
                                        model: uploadedImages.length
                                        
                                        Rectangle {
                                            width: 500
                                            height: 380  // FIXED HEIGHT
                                            color: "#0a0a0a"
                                            radius: 8
                                            border.width: 2
                                            border.color: "#FF7F50"
                                            
                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 10
                                                spacing: 10
                                                
                                                Text {
                                                    text: uploadedImages[index].isPrimary ? "Primary Image" : "User Upload " + (index + 1)
                                                    font.pixelSize: 14
                                                    font.bold: true
                                                    color: uploadedImages[index].isPrimary ? "#FFD700" : "#FF7F50"
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 20
                                                }
                                                
                                                Image {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 300  // Reduced to make room for title
                                                    source: uploadedImages[index].url
                                                    fillMode: Image.PreserveAspectFit
                                                    asynchronous: true
                                                    
                                                    Rectangle {
                                                        anchors.centerIn: parent
                                                        width: 60
                                                        height: 60
                                                        color: "transparent"
                                                        visible: parent.status === Image.Loading
                                                        
                                                        BusyIndicator {
                                                            anchors.centerIn: parent
                                                            running: parent.visible
                                                            width: 40
                                                            height: 40
                                                        }
                                                    }
                                                }
                                                
                                                Text {
                                                    text: uploadedImages[index].title || "No title"
                                                    font.pixelSize: 14
                                                    font.italic: !uploadedImages[index].title
                                                    color: uploadedImages[index].title ? "#FFFFFF" : "#888888"
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 20
                                                    horizontalAlignment: Text.AlignHCenter
                                                    elide: Text.ElideRight
                                                }
                                                
                                                Text {
                                                    text: systemName
                                                    font.pixelSize: 12
                                                    color: "#AAAAAA"
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 20
                                                    horizontalAlignment: Text.AlignHCenter
                                                }
                                            }
                                        }
                                    }
                                    
                                    // Only show preset if no user images
                                    Rectangle {
                                        visible: uploadedImages.length === 0
                                        width: 500
                                        height: 380  // FIXED HEIGHT
                                        color: "#0a0a0a"
                                        radius: 8
                                        border.width: 2
                                        border.color: "#444444"
                                        
                                        Component.onCompleted: {
                                            console.log("Preset image visibility check - uploadedImages.length:", uploadedImages.length)
                                            console.log("Preset image visible:", visible)
                                        }
                                        
                                        Connections {
                                            target: popupWindow
                                            function onUploadedImagesChanged() {
                                                console.log("uploadedImages changed, new length:", uploadedImages.length)
                                            }
                                        }
                                        
                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 10
                                            
                                            Text {
                                                text: category || "Space System"
                                                font.pixelSize: 14
                                                font.bold: true
                                                color: "#888888"
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 20
                                            }
                                            
                                            Image {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 320  // FIXED HEIGHT - THIS IS KEY!
                                            source: {
                                                var url = getPresetImageForCategory(category)
                                                if (url && url.length > 0) return url
                                                return "" // let outer logic/caching handle no-image case
                                            }
                                                fillMode: Image.PreserveAspectFit
                                                asynchronous: true
                                                
                                                onStatusChanged: {
                                                    if (status === Image.Error) {
                                                        console.log("Image failed to load, source was:", source)
                                                    } else if (status === Image.Ready) {
                                                        console.log("Image loaded successfully:", source)
                                                    }
                                                }
                                                
                                                Rectangle {
                                                    anchors.centerIn: parent
                                                    width: 60
                                                    height: 60
                                                    color: "transparent"
                                                    visible: parent.status === Image.Loading
                                                    
                                                    BusyIndicator {
                                                        anchors.centerIn: parent
                                                        running: parent.visible
                                                        width: 40
                                                        height: 40
                                                    }
                                                }
                                            }
                                            
                                            Text {
                                                text: systemName + " (Preset)"
                                                font.pixelSize: 12
                                                color: "#888888"
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 20
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Info text at bottom
                        Text {
                            text: uploadedImages.length > 0 ? 
                                  ("Showing " + uploadedImages.length + " user uploaded image(s)") :
                                  "No user images uploaded. Showing preset image only."
                            font.pixelSize: 12
                            color: "#888888"
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignCenter
                        }
                    }
                }
                
                // Edit Info tab - COMPREHENSIVE EDIT INTERFACE
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    contentWidth: width

                    ColumnLayout {
                        id: editInfoColumn
                        width: parent ? parent.width : 0
                        spacing: 15
                        anchors.margins: 20

                        Text {
                            text: canEdit ? "Edit System Information" : "System Information (Read Only)"
                            font.pixelSize: 18
                            font.bold: true
                            color: canEdit ? "#FF7F50" : "#888888"
                            Layout.fillWidth: true
                        }
                        
                        // Save button - only visible in Edit Info tab when user can edit
                        Button {
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 45
                            Layout.alignment: Qt.AlignHCenter
                            text: "Save Changes"
                            font.pixelSize: 14
                            font.bold: true
                            visible: canEdit && isClaimedByUser
                            
                            background: Rectangle {
                                color: parent.pressed ? "#16a34a" : "#22c55e"
                                radius: 8
                                border.color: "#16a34a"
                                border.width: 1
                            }
                            
                            contentItem: Text {
                                text: parent.text
                                color: "#FFFFFF"
                                font: parent.font
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            onClicked: {
                                console.log("Save button clicked - saving system information")
                                console.log("isClaimedByUser:", isClaimedByUser)
                                console.log("canEdit:", canEdit)
                                
                                // Only save if user can edit (i.e., they own the system)
                                if (canEdit && isClaimedByUser) {
                                    console.log("Saving system info via save button...")
                                    saveEditInfoData()
                                    
                                    // Show a brief feedback message
                                    copyPopup.messageText = "System information saved!"
                                    copyPopup.open()
                                } else {
                                    console.log("Cannot save - user doesn't own system or cannot edit")
                                    copyPopup.messageText = "Cannot save: You must claim this system first"
                                    copyPopup.open()
                                }
                            }
                        }

                        // Status indicators row
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            
                            // Claim status
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 50
                                color: isClaimedByUser ? "#2d2d2d" : "#2d2d14"  // Neutral when yours; claim button shows green
                                radius: 8
                                border.width: 1
                                border.color: isClaimedByUser ? "#888888" : "#a3e635"  // Neutral border when yours
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: isClaimedByUser ? "✓ This system is all yours" : "Unclaimed"
                                    color: isClaimedByUser ? "#22c55e" : "#eab308"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }
                            
                            // Visited status
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 50
                                color: isVisited ? "#224422" : "#442222"
                                radius: 8
                                border.width: 1
                                border.color: isVisited ? "#338833" : "#883333"
                                
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: isClaimedByUser
                                    onClicked: {
                                        // Toggle the visited status immediately in UI
                                        isVisited = !isVisited
                                        
                                        console.log("VISITED CLICK: Setting visited to:", isVisited, "for system:", systemName)
                                        console.log("NOT auto-saving - waiting for Save button")
                                        
                                        // DO NOT send update to database here - only Save button should save
                                    }
                                }
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: isVisited ? "✓ Visited" : "Not Visited"
                                    color: isVisited ? "#AAFFAA" : "#FFAAAA"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }
                            
                            // Done status
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 50
                                color: isDone ? "#224422" : "#442222"
                                radius: 8
                                border.width: 1
                                border.color: isDone ? "#338833" : "#883333"
                                
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: isClaimedByUser
                                    onClicked: {
                                        // Toggle the done status immediately in UI
                                        isDone = !isDone
                                        
                                        console.log("DONE CLICK: Setting done to:", isDone, "for system:", systemName)
                                        console.log("NOT auto-saving - waiting for Save button")
                                        
                                        // DO NOT send update to database here - only Save button should save
                                    }
                                }
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: isDone ? "✓ Done" : "In Progress"
                                    color: isDone ? "#AAFFAA" : "#FFAAAA"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }
                        }
                        
                        // Show claim status text
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            color: {
                                if (isClaimedByUser && systemData && systemData.claimed) {
                                    return "#224422"  // Green for your claim
                                } else if (systemData && systemData.claimed && !isClaimedByUser) {
                                    return "#442222"  // Red for others' claims
                                } else {
                                    return "#444422"  // Yellow for unclaimed
                                }
                            }
                            radius: 8
                            border.width: 1
                            border.color: {
                                if (isClaimedByUser && systemData && systemData.claimed) {
                                    return "#338833"  // Green border for your claim
                                } else if (systemData && systemData.claimed && !isClaimedByUser) {
                                    return "#883333"  // Red border for others' claims
                                } else {
                                    return "#888833"  // Yellow border for unclaimed
                                }
                            }

                            Connections {
                                target: popupWindow
                                function onForceUIRefresh() {
                                    console.log("Rectangle: UI refresh triggered")
                                }
                                function onClaimStatusChanged() {
                                    console.log("Rectangle: Claim status changed")
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    if (isClaimedByUser) {
                                        return "This claim is yours - you can edit all information below."
                                    } else if (systemData && systemData.claimed) {
                                        return "This system is claimed by: " + (systemData.claimedBy || "another commander")
                                    } else {
                                        return "This system is unclaimed. Claim it to add information."
                                    }
                                }
                                font.pixelSize: 14
                                color: {
                                    if (isClaimedByUser && systemData && systemData.claimed) {
                                        return "#AAFFAA"  // Green for your claim
                                    } else if (systemData && systemData.claimed && !isClaimedByUser) {
                                        return "#FFAAAA"  // Red for others' claims
                                    } else {
                                        return "#FFFFAA"  // Yellow for unclaimed
                                    }
                                }
                                wrapMode: Text.WordWrap
                                width: parent.width - 40
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        // POI Information Section
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: poiCheckbox.checked ? 420 : 60
                            color: "#1f1f1f"
                            radius: 8
                            
                            Behavior on Layout.preferredHeight {
                                NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 15
                                spacing: 10
                                
                                // POI Toggle Row
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    
                                    CheckBox {
                                        id: poiCheckbox
                                        enabled: isClaimedByUser
                                        checked: false  // Will be set based on database data
                                        
                                        onCheckedChanged: {
                                            if (!checked) {
                                                // Clear POI data when unchecked
                                                poiTypeCombo.currentIndex = 0
                                                poiName = ""
                                                poiDescription = ""
                                                discovererField.text = ""
                                                submitterField.text = ""
                                            }
                                        }
                                    }
                                    
                                    Text {
                                        text: "Mark as Point of Interest (POI)"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: poiCheckbox.checked ? "#FF7F50" : "#888888"
                                        Layout.fillWidth: true
                                    }
                                }

                                    // POI Configuration (only visible when checked)
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 30  // Indent POI fields to align with checkbox
                                        spacing: 10
                                        visible: poiCheckbox.checked
                                        opacity: poiCheckbox.checked ? 1.0 : 0.0
                                    
                                    Behavior on opacity {
                                        NumberAnimation { duration: 200 }
                                    }
                                    
                                    // POI Type Selection
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 5
                                        Layout.alignment: Qt.AlignVCenter
                                        
                                        Text {
                                            text: "POI Type:"
                                            font.pixelSize: 12
                                            color: "#FFFFFF"
                                            Layout.preferredWidth: 80
                                            Layout.alignment: Qt.AlignVCenter
                                        }
                                        
                                        ComboBox {
                                            id: poiTypeCombo
                                            Layout.fillWidth: false
                                            Layout.preferredWidth: 150
                                            Layout.preferredHeight: 30
                                            Layout.alignment: Qt.AlignVCenter
                                            enabled: isClaimedByUser
                                            
                                            model: ["Potential POI", "POI"]
                                            
                                            background: Rectangle {
                                                color: "#0a0a0a"
                                                border.color: "#444444"
                                                border.width: 1
                                                radius: 4
                                            }
                                            
                                            contentItem: Text {
                                                text: parent.currentText
                                                color: "#FFFFFF"
                                                font.pixelSize: 12
                                                leftPadding: 8
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                        
                                        Rectangle {
                                            Layout.preferredWidth: 25
                                            Layout.preferredHeight: 25
                                            Layout.alignment: Qt.AlignVCenter
                                            color: "#444444"
                                            radius: 12.5
                                            border.color: "#666666"
                                            border.width: 1
                                            
                                            Text {
                                                anchors.centerIn: parent
                                                text: "?"
                                                font.pixelSize: 14
                                                font.bold: true
                                                color: "#FFFFFF"
                                            }
                                            
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    poiHelpPopup.helpText = poiTypeCombo.currentIndex === 0 ? 
                                                        "Potential: Systems that are deemed by the person submitting interesting enough and is being prepared to get accepted on gec. Keep in mind this system is currently unfinished, and directly marking it as POI is fine aswell. and it being reliant on GEC is only temporary." :
                                                        "POIs: These are systems that have been accepted in the GEC and are official POIs. Keep in mind this system is currently unfinished, and it being reliant on GEC is only temporary."
                                                    poiHelpPopup.open()
                                                }
                                            }
                                        }
                                        
                                        Item {
                                            Layout.fillWidth: true  // Push everything to the left
                                        }
                                    }
                                    
                                    // Discoverer Field
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10
                                        
                                        Text {
                                            text: "Discoverer:"
                                            font.pixelSize: 12
                                            color: "#FFFFFF"
                                            Layout.preferredWidth: 80
                                        }
                                        
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 30
                                            color: "#0a0a0a"
                                            radius: 4
                                            border.width: 1
                                            border.color: "#444444"
                                            
                                            TextInput {
                                                id: discovererField
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                font.pixelSize: 12
                                                color: "#FFFFFF"
                                                enabled: isClaimedByUser
                                                selectByMouse: true
                                                
                                                // Prevent wheel events from being captured when not focused
                                                WheelHandler {
                                                    enabled: !parent.activeFocus
                                                    target: null
                                                    onWheel: function(event) { event.accepted = false }
                                                }
                                                
                                                Text {
                                                    visible: parent.text.length === 0
                                                    text: "Who discovered this?"
                                                    color: "#888888"
                                                    font: parent.font
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }
                                        }
                                    }
                                    
                                    // Submitter Field
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10
                                        
                                        Text {
                                            text: "Submitter:"
                                            font.pixelSize: 12
                                            color: "#FFFFFF"
                                            Layout.preferredWidth: 80
                                        }
                                        
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 30
                                            color: "#0a0a0a"
                                            radius: 4
                                            border.width: 1
                                            border.color: "#444444"
                                            
                                            TextInput {
                                                id: submitterField
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                font.pixelSize: 12
                                                color: "#FFFFFF"
                                                enabled: isClaimedByUser
                                                selectByMouse: true
                                                
                                                // Prevent wheel events from being captured when not focused
                                                WheelHandler {
                                                    enabled: !parent.activeFocus
                                                    target: null
                                                    onWheel: function(event) { event.accepted = false }
                                                }
                                                
                                                Text {
                                                    visible: parent.text.length === 0
                                                    text: "Who is submitting this POI?"
                                                    color: "#888888"
                                                    font: parent.font
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }
                                        }
                                    }
                                }

                                Text {
                                    text: "POI Information:"
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "#FF7F50"
                                    visible: !poiCheckbox.checked
                                }

                                // POI Name
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 35
                                    color: "#0a0a0a"
                                    radius: 4
                                    border.width: poiNameEditor.activeFocus ? 2 : 1
                                    border.color: poiNameEditor.activeFocus ? "#FF7F50" : "#444444"

                                    TextInput {
                                        id: poiNameEditor
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        property string customPlaceholderText: isClaimedByUser ? "Enter POI name..." : "No POI name set"
                                        text: poiName
                                        enabled: isClaimedByUser
                                        readOnly: !isClaimedByUser
                                        
                                        // Prevent wheel events from being captured when not focused
                                        WheelHandler {
                                            enabled: !parent.activeFocus
                                            target: null
                                            onWheel: event.accepted = false
                                        }
                                        font.pixelSize: 13
                                        color: "#FFFFFF"
                                        selectByMouse: true
                                        
                                        onTextChanged: {
                                            poiName = text
                                            if (isClaimedByUser) {
                                                console.log("POI Name changed - NOT auto-saving, waiting for Save button")
                                                // DO NOT call saveEditInfoData() here
                                            }
                                        }

                                        Text {
                                            visible: parent.text.length === 0
                                            text: parent.customPlaceholderText
                                            color: "#888888"
                                            font: parent.font
                                            anchors.fill: parent
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }

                                // POI Description
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.minimumHeight: 100
                                    color: "#0a0a0a"
                                    radius: 4
                                    border.width: poiDescEditor.activeFocus ? 2 : 1
                                    border.color: poiDescEditor.activeFocus ? "#FF7F50" : "#444444"

                                    ScrollView {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                        clip: true

                                        TextArea {
                                            id: poiDescEditor
                                            width: parent ? parent.width : 0
                                            property string customPlaceholderText: isClaimedByUser ? "Describe this POI..." : "No POI description available"
                                            text: poiDescription
                                            wrapMode: TextArea.Wrap
                                            selectByMouse: true
                                            enabled: isClaimedByUser
                                            readOnly: !isClaimedByUser
                                            font.pixelSize: 13
                                            color: "#FFFFFF"
                                            background: Rectangle { color: "transparent" }
                                            
                                            // Prevent wheel events from being captured when not focused
                                            WheelHandler {
                                                enabled: !parent.activeFocus
                                                target: null
                                                onWheel: event.accepted = false
                                            }
                                            
                                            onTextChanged: {
                                                poiDescription = text
                                                if (isClaimedByUser) {
                                                    console.log("POI Description changed - NOT auto-saving, waiting for Save button")
                                                    // DO NOT call saveEditInfoData() here
                                                }
                                            }

                                            Text {
                                                visible: parent.text.length === 0
                                                text: parent.customPlaceholderText
                                                color: "#888888"
                                                font: parent.font
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                verticalAlignment: Text.AlignTop
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Custom System Information Section
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 200
                            color: "#1f1f1f"
                            radius: 8
                            
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 15
                                spacing: 10
                                
                                Text {
                                    text: "Custom System Information:"
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "#FF7F50"
                                }
                                
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#0a0a0a"
                                    radius: 4
                                    border.width: customSystemInfoEditor.activeFocus ? 2 : 1
                                    border.color: customSystemInfoEditor.activeFocus ? "#FF7F50" : "#444444"
                                    
                                    ScrollView {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                        clip: true
                                        
                                        TextArea {
                                            id: customSystemInfoEditor
                                            width: parent ? parent.width : 0
                                            property string customPlaceholderText: isClaimedByUser ? "Add custom system information..." : "No custom information available"
                                            text: customSystemInfo
                                            wrapMode: TextArea.Wrap
                                            selectByMouse: true
                                            enabled: isClaimedByUser
                                            readOnly: !isClaimedByUser
                                            font.pixelSize: 13
                                            color: "#FFFFFF"
                                            background: Rectangle { color: "transparent" }
                                            
                                            // Prevent wheel events from being captured when not focused
                                            WheelHandler {
                                                enabled: !parent.activeFocus
                                                target: null
                                                onWheel: event.accepted = false
                                            }
                                            
                                            onTextChanged: {
                                                customSystemInfo = text
                                                if (isClaimedByUser) {
                                                    console.log("=== HELLO my name is burger - TYPING DETECTED ===")
                                                    console.log("Text changed to:", text)
                                                    console.log("customSystemInfo updated to:", customSystemInfo)
                                                    console.log("=== END HELLO my name is burger - TYPING DETECTED ===")
                                                    
                                                    // Mark as edited immediately but DON'T call updateSystemDisplay yet
                                                    if (systemData && typeof systemData === 'object') {
                                                        systemData.edited = true
                                                    }
                                                    
                                                    // Save the data but delay display update
                                                    // DO NOT call saveEditInfoData() here - only Save button should save
                                                }
                                            }
                                            
                                            Text {
                                                visible: parent.text.length === 0
                                                text: parent.customPlaceholderText
                                                color: "#888888"
                                                font: parent.font
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                verticalAlignment: Text.AlignTop
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Image Management Section (ONLY in Edit Info tab)
                        Rectangle {
                            visible: isClaimedByUser
                            Layout.fillWidth: true
                            Layout.preferredHeight: imageManagementColumn.implicitHeight + 30
                            color: "#1f1f1f"
                            radius: 8

                            ColumnLayout {
                                id: imageManagementColumn
                                anchors.fill: parent
                                anchors.margins: 15
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    
                                    Text {
                                        text: "Manage System Images:"
                                        font.pixelSize: 14
                                        font.bold: true
                                        color: "#FF7F50"
                                        Layout.fillWidth: true
                                    }
                                    
                                    Text {
                                        text: {
                                            // Force binding update
                                            var count = uploadedImages ? uploadedImages.length : 0
                                            return count + " images total"
                                        }
                                        font.pixelSize: 12
                                        color: "#888888"
                                    }
                                }

                                // Upload button
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 15

                                    Button {
                                        text: "Upload Images"
                                        Layout.preferredWidth: 120
                                        Layout.preferredHeight: 35
                                        enabled: isClaimedByUser

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
                                            console.log("Opening image picker for system:", systemName)
                                            multiImageDialog.open()
                                        }
                                    }

                                    Text {
                                        text: "Upload screenshots (PNG, JPG, GIF, WebP)"
                                        font.pixelSize: 12
                                        color: "#888888"
                                        Layout.fillWidth: true
                                    }
                                }

                                // Images dock - compact horizontal view
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 120
                                    color: "#0a0a0a"
                                    radius: 4
                                    border.width: 1
                                    border.color: "#444444"
                                    
                                    ScrollView {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                                        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                                        clip: true
                                        
                                        Row {
                                            spacing: 8
                                            height: 100
                                            
                                            // Preset image (only show if no uploaded images)
                                            Rectangle {
                                                visible: uploadedImages.length === 0
                                                width: 100
                                                height: 100
                                                color: "#1a1a1a"
                                                radius: 4
                                                border.width: 2
                                                border.color: "#FF7F50"
                                                
                                                Image {
                                                    anchors.fill: parent
                                                    anchors.margins: 4
                                                    source: getPresetImageForCategory(category)
                                                    fillMode: Image.PreserveAspectCrop
                                                    asynchronous: true
                                                }
                                                
                                                Text {
                                                    anchors.bottom: parent.bottom
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    anchors.bottomMargin: 2
                                                    text: "Preset"
                                                    font.pixelSize: 8
                                                    color: uploadedImages.length > 0 ? "#888888" : "#FF7F50"
                                                    font.bold: true
                                                }
                                                
                                                // Delete button only if user has uploaded images
                                                Button {
                                                    visible: uploadedImages.length > 0 && isClaimedByUser
                                                    anchors.top: parent.top
                                                    anchors.right: parent.right
                                                    anchors.margins: 2
                                                    width: 20
                                                    height: 20
                                                    text: "×"
                                                    
                                                    background: Rectangle {
                                                        color: "#ef4444"
                                                        radius: 10
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
                                                        // This makes preset image primary again
                                                        console.log("Making preset image primary")
                                                    }
                                                }
                                            }
                                            
                                            // User uploaded images
                                            Repeater {
                                                id: imageManagerRepeater
                                                model: uploadedImages.length
                                                
                                                delegate: Rectangle {
                                                    width: 100
                                                    height: 100
                                                    color: "#1a1a1a"
                                                    radius: 4
                                                    border.width: 2
                                                    border.color: uploadedImages[index].isPrimary ? "#FF7F50" : "#666666"
                                                    clip: true  // Prevent children from rendering outside
                                                    
                                                    Image {
                                                        anchors.fill: parent
                                                        anchors.margins: 4
                                                        source: uploadedImages[index].url
                                                        fillMode: Image.PreserveAspectCrop
                                                        asynchronous: true
                                                    }
                                                    
                                                    Rectangle {
                                                        anchors.bottom: parent.bottom
                                                        anchors.horizontalCenter: parent.horizontalCenter
                                                        anchors.bottomMargin: 4
                                                        width: Math.min(parent.width - 8, labelText.implicitWidth + 8)
                                                        height: 16
                                                        color: uploadedImages[index].isPrimary ? "#FFD700" : "#FF7F50"
                                                        radius: 8
                                                        
                                                        Text {
                                                            id: labelText
                                                            anchors.centerIn: parent
                                                            text: uploadedImages[index].isPrimary ? "Primary" : "Image " + (index + 1)
                                                            font.pixelSize: 10
                                                            color: "#000000"
                                                            font.bold: true
                                                        }
                                                    }
                                                    
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        anchors.margins: 20 // Leave space for delete button
                                                        onClicked: {
                                                            if (!uploadedImages[index].isPrimary) {
                                                                makeImagePrimary(index)
                                                            }
                                                        }
                                                    }
                                                    
                                                    // Delete button
                                                    Rectangle {
                                                        visible: isClaimedByUser
                                                        anchors.top: parent.top
                                                        anchors.right: parent.right
                                                        anchors.topMargin: 4
                                                        anchors.rightMargin: 4
                                                        width: 20
                                                        height: 20
                                                        color: "#ef4444"
                                                        radius: 10
                                                        border.color: "#FFFFFF"
                                                        border.width: 1
                                                        z: 10
                                                        
                                                        Text {
                                                            anchors.centerIn: parent
                                                            anchors.verticalCenterOffset: -1  // Slight adjustment for visual centering
                                                            text: "✕"  // Using a different X character
                                                            font.pixelSize: 12
                                                            font.family: "Arial"
                                                            color: "#FFFFFF"
                                                        }
                                                        
                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                console.log("Deleting image:", uploadedImages[index].url)
                                                                deleteImage(index)
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            
                                            // Plus button to add more images
                                            Rectangle {
                                                visible: isClaimedByUser
                                                width: 100
                                                height: 100
                                                color: "#1a1a1a"
                                                radius: 4
                                                border.width: 2
                                                border.color: "#666666"
                                                
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "+"
                                                    font.pixelSize: 32
                                                    color: "#666666"
                                                    font.bold: true
                                                }
                                                
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: multiImageDialog.open()
                                                }
                                            }
                                        }
                                    }
                                }
                                
                                // Image titles editing section
                                Rectangle {
                                    id: imageTitlesSection
                                    visible: {
                                        var hasImages = uploadedImages && uploadedImages.length > 0
                                        console.log("Image titles section visibility check - uploadedImages.length:", uploadedImages ? uploadedImages.length : "null", "visible:", hasImages)
                                        return hasImages
                                    }
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: visible ? (titlesColumn.implicitHeight + 20) : 0
                                    color: "#0a0a0a"
                                    radius: 4
                                    border.width: 1
                                    border.color: "#444444"
                                    
                                    Component.onCompleted: {
                                        console.log("Image titles section created, uploadedImages.length:", uploadedImages.length)
                                        console.log("uploadedImages:", JSON.stringify(uploadedImages))
                                    }
                                    
                                    Connections {
                                        target: popupWindow
                                        function onUploadedImagesChanged() {
                                            console.log("Image titles section detected uploadedImages change, new length:", uploadedImages.length)
                                        }
                                    }
                                    
                                    ColumnLayout {
                                        id: titlesColumn
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 10
                                        
                                        Text {
                                            text: "Image Titles"
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: "#FF7F50"
                                        }
                                        
                                        Repeater {
                                            id: imageTitlesRepeater
                                            model: uploadedImages ? uploadedImages.length : 0
                                            
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 10
                                                
                                                Text {
                                                    text: (uploadedImages[index] && uploadedImages[index].isPrimary) ? "Primary:" : "Image " + (index + 1) + ":"
                                                    font.pixelSize: 12
                                                    color: "#FFFFFF"
                                                    Layout.preferredWidth: 60
                                                }
                                                
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 25
                                                    color: "#1a1a1a"
                                                    radius: 4
                                                    border.width: 1
                                                    border.color: titleInput2.activeFocus ? "#FF7F50" : "#444444"
                                                    
                                                    TextInput {
                                                        id: titleInput2
                                                        anchors.fill: parent
                                                        anchors.margins: 6
                                                        text: (uploadedImages[index] && uploadedImages[index].title) ? uploadedImages[index].title : ""
                                                        font.pixelSize: 12
                                                        color: "#FFFFFF"
                                                        selectByMouse: true
                                                        verticalAlignment: Text.AlignVCenter
                                                        
                                                        // Prevent wheel events from being captured when not focused
                                                        WheelHandler {
                                                            enabled: !parent.activeFocus
                                                            target: null
                                                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                                            onWheel: (wheel)=> { wheel.accepted = false }
                                                        }
                                                        
                                                        onTextChanged: {
                                                            uploadedImages[index].title = text
                                                            updateImageTitle(index, text)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Add some bottom padding so the last element isn't cut off
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 50
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
                    // Auto-size width to fit full label (no ellipsis)
                    Layout.preferredWidth: Math.max(180, claimButtonMeasure.implicitWidth + 24)
                    Layout.preferredHeight: 45
                    property bool isCheckingClaim: false
                    property bool cachedCanClaim: {
                        // Use ClaimManager for clean claim logic
                        // Force re-evaluation by depending on reactive properties
                        var dummy1 = edrhController.claimManager.currentClaim
                        // Use claimManager state instead of removed controller property
                        var dummy2 = edrhController && edrhController.claimManager ? edrhController.claimManager.hasActiveClaim() : false
                        var dummy3 = isClaimedByUser
                        
                        if (isClaimedByUser) {
                            return true  // Can always unclaim your own
                        } else {
                            // Use ClaimManager's canClaimSystem method
                            return edrhController.claimManager.canClaimSystem(systemData.name || "")
                        }
                    }
                    
                    text: {
                        if (isCheckingClaim) {
                            return "Checking Claims..."
                        } else if (isClaimedByUser) {
                            return "Unclaim System"
                        } else if (systemData && systemData.claimed && systemData.claimedBy && systemData.claimedBy !== edrhController.commanderName) {
                            return "You cannot claim this system"
                        } else if (!cachedCanClaim && edrhController.claimManager && edrhController.claimManager.hasActiveClaim()) {
                            return "Complete your current claim first"
                        } else {
                            return "Claim System"
                        }
                    }
                    font.pixelSize: 14
                    font.bold: true
                    
                    // Force immediate update of button properties
                    Connections {
                        target: popupWindow
                        function onForceUIRefresh() {
                            console.log("Button: Forcing UI refresh, isClaimedByUser:", isClaimedByUser)
                        }
                    }
                    
                    enabled: {
                        if (isCheckingClaim) {
                            return false
                        } else if (isClaimedByUser) {
                            return systemData && systemData.claimed && systemData.claimedBy === edrhController.commanderName
                        } else if (systemData && systemData.claimed && systemData.claimedBy && systemData.claimedBy.toLowerCase && systemData.claimedBy.toLowerCase() !== "empty" && systemData.claimedBy !== edrhController.commanderName) {
                            return false
                        } else {
                            return cachedCanClaim
                        }
                    }
                    
                    // Force button state refresh when claim status changes
                    Connections {
                        target: popupWindow
                        function onClaimStatusChanged() {
                            // Force re-evaluation of enabled state
                            console.log("Button: Forcing enabled state refresh")
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
                        anchors.fill: parent
                        anchors.margins: 0
                    }
                    
                    // Hidden measurer to compute ideal width
                    Text {
                        id: claimButtonMeasure
                        text: parent.text
                        font: parent.font
                        visible: false
                    }

                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : "#AAAAAA"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.fill: parent
                        wrapMode: Text.NoWrap
                        maximumLineCount: 1
                    }
                    
                    onClicked: {
                        console.log("Claim button clicked for system:", systemName, "isClaimedByUser:", isClaimedByUser)
                        console.log("SystemData claim status:", systemData ? systemData.claimed : "no systemData")
                        console.log("SystemData claimedBy:", systemData ? systemData.claimedBy : "no systemData")
                        
                        // Double-check claim status before proceeding
                        if (isClaimedByUser) {
                            // Safety check: ensure we actually own this system
                            if (systemData && systemData.claimed && systemData.claimedBy === edrhController.commanderName) {
                                console.log("Proceeding with unclaim - all checks passed")
                                
                                // Apply local unclaim immediately to avoid race flicker, then send server request
                                if (edrhController && edrhController.claimManager) {
                                    edrhController.claimManager.applyLocalUnclaim(systemName)
                                }
                                edrhController.claimManager.unclaimSystem(systemName)
                                
                                // Re-enable notifications and close the popup immediately (boot out)
                                edrhController.suppressMainAppNotifications = false
                                popupWindow.close()
                            } else {
                                console.log("Cannot unclaim - system data shows we don't own it")
                                claimPopup.messageText = "Cannot unclaim: System is not claimed by you"
                                claimPopup.isSuccess = false
                                claimPopup.open()
                            }
                        } else if (!(edrhController && edrhController.claimManager && edrhController.claimManager.hasActiveClaim())) {
                            console.log("Proceeding with claim")
                            // Optimistic local transition to avoid race flicker
                            if (edrhController && edrhController.claimManager) {
                                edrhController.claimManager.applyLocalClaim(systemName)
                            }
                            edrhController.claimManager.claimSystem(systemName)
                            
                            // Re-enable notifications and close the popup immediately (boot out)
                            edrhController.suppressMainAppNotifications = false
                            popupWindow.close()
                        } else {
                            console.log("Cannot claim - user has existing claim")
                            claimPopup.messageText = "Cannot claim: You already have a system claimed"
                            claimPopup.isSuccess = false
                            claimPopup.open()
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
