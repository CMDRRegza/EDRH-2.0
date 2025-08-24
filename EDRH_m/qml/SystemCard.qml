import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    
    property var systemData: ({})
    property bool isDestroyed: false  // Track destruction state
    
    signal systemClicked(string systemName)
    signal systemNameCopyRequested(string systemName)
    
    height: 130  // Increased height from 120 to 130 for better visual impact
    color: "transparent"
    radius: 15
    border.width: 2
    border.color: "#333333"
    clip: true  // Prevent content from overflowing outside the card
    
    // Extract data with safety checks
    readonly property string systemName: (systemData && typeof systemData === 'object' && systemData.name) ? systemData.name : "Unknown System"
    readonly property string category: (systemData && typeof systemData === 'object' && systemData.category) ? systemData.category : "Wolf-Rayet Star"
    readonly property string distance: (systemData && typeof systemData === 'object' && systemData.distance) ? systemData.distance : "N/A"
    readonly property string poi: {
        if (systemData && typeof systemData === 'object') {
            // Check for potential_or_poi field first (this is what's actually in the database)
            if (systemData.potential_or_poi && systemData.potential_or_poi.trim() !== "") {
                return systemData.potential_or_poi
            }
            // Fallback to poi field for backwards compatibility
            if (systemData.poi && systemData.poi.trim() !== "") {
                return systemData.poi
            }
        }
        return ""
    }
    readonly property bool isDone: (systemData && typeof systemData === 'object' && systemData.done) ? systemData.done : false
    readonly property bool isClaimed: (function() {
        if (!(systemData && typeof systemData === 'object')) return false
        if (!systemData.claimed) return false
        var by = systemData.claimedBy || ""
        if (by.toLowerCase && by.toLowerCase() === "empty") return false
        return true
    })()
    readonly property string claimedBy: (function() {
        if (!(systemData && typeof systemData === 'object')) return ""
        var by = systemData.claimedBy || ""
        if (by.toLowerCase && by.toLowerCase() === "empty") return ""
        return by
    })()
    readonly property bool isClaimedByUser: isClaimed && claimedBy === (edrhController ? edrhController.commanderName : "")
    readonly property string categoryColor: {
        if (systemData && typeof systemData === 'object' && systemData.categoryColor && typeof systemData.categoryColor === 'string' && systemData.categoryColor !== "") {
            return systemData.categoryColor
        }
        var fallbackColor = getCategoryColor()
        return (fallbackColor && typeof fallbackColor === 'string' && fallbackColor !== "") ? fallbackColor : "#FF7F50"
    }
    readonly property string backgroundImage: getBackgroundImage()
    
    // Image loading states
    property bool isLoadingImage: false
    property bool imageLoadFailed: false
    property string cachedImageUrl: ""
    property bool usingFallback: false
    
    // Background image layer
    Rectangle {
        id: backgroundContainer
        anchors.fill: parent
        radius: parent.radius
        color: "#1a1a1a"
        clip: true
        
        // Background image with smart caching
        Image {
            id: bgImage
            anchors.fill: parent
            source: ""  // Will be set explicitly to avoid binding loops
            fillMode: Image.PreserveAspectCrop
            opacity: status === Image.Ready ? 0.6 : 0
            asynchronous: true
            cache: false  // Disabled since we're using our own caching
            
            // Enhanced loading state management
            onStatusChanged: {
                if (status === Image.Ready) {
                    root.imageLoadFailed = false
                    fallbackGradient.visible = false
                } else if (status === Image.Error) {
                    root.imageLoadFailed = true
                    fallbackGradient.visible = true
                    console.warn("SystemCard: Image failed to load for", root.systemName)
                } else if (status === Image.Loading) {
                    root.isLoadingImage = true
                } else if (status === Image.Null) {
                    root.isLoadingImage = false
                }
            }
            
            // Smooth loading animation
            Behavior on opacity {
                NumberAnimation { duration: 300 }
            }
        }
        
        // Fallback gradient background
        Rectangle {
            id: fallbackGradient
            anchors.fill: parent
            visible: false
            gradient: Gradient {
                GradientStop { 
                    position: 0.0
                    color: root.categoryColor
                }
                GradientStop { 
                    position: 1.0
                    color: Qt.darker(root.categoryColor, 2.0)
                }
            }
            opacity: 0.4
        }
        
        // Dark overlay for text readability
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.5
            radius: parent.radius
        }
        
        // Subtle noise texture effect
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            opacity: 0.1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.3; color: "#FFFFFF" }
                GradientStop { position: 0.7; color: "transparent" }
                GradientStop { position: 1.0; color: "#FFFFFF" }
            }
        }
    }
    
    // Connect to ImageLoader for preset image caching only
    Connections {
        target: imageLoader
        enabled: root.visible && !root.isDestroyed  // CRITICAL FIX: Disable when destroyed
        
        function onImageLoaded(url, imagePath) {
            // CRITICAL FIX: Early return if component is being destroyed
            if (!root || !root.visible || root.isDestroyed) {
                console.log("SystemCard: Ignoring imageLoaded signal - component destroyed or invisible")
                return
            }
            
            // Only update if this is still the current background image AND it's a preset image
            var isUploadedImage = (systemData && systemData.images && systemData.images.trim() !== "" && 
                                 url === systemData.images)
            
            if (url === root.backgroundImage && root.systemName && !isUploadedImage) {
                console.log("SystemCard: Cached preset image loaded for", root.systemName, "- URL:", imagePath)
                root.cachedImageUrl = imagePath
                
                // CRITICAL FIX: Defer image source update to prevent mass simultaneous updates
                Qt.callLater(function() {
                    if (root && !root.isDestroyed && bgImage) {
                        // Use the properly encoded cached path directly
                        bgImage.source = imagePath
                        root.imageLoadFailed = false
                    }
                })
            }
        }
        
        function onPresetImagesReady() {
            // When preset images are loaded, check if we need to update our image
            if (!root || root.isDestroyed) return
            
            console.log("SystemCard: Preset images ready signal received for", root.systemName, 
                       "- Using fallback:", root.usingFallback)
            
            // Update if we're currently using a fallback
            if (root.usingFallback) {
                console.log("SystemCard: Updating image source for", root.systemName)
                root.usingFallback = false  // Reset flag
                Qt.callLater(updateImageSource)
            }
        }
        
        function onImageLoadFailed(url, error) {
            // CRITICAL FIX: Early return if component is being destroyed
            if (!root || !root.visible || root.isDestroyed) {
                console.log("SystemCard: Ignoring imageLoadFailed signal - component destroyed or invisible")
                return
            }
            
            // Only update if this is still the current background image AND it's a preset image
            var isUploadedImage = (systemData && systemData.images && systemData.images.trim() !== "" && 
                                 url === systemData.images)
            
            if (url === root.backgroundImage && root.systemName && !isUploadedImage) {
                console.warn("SystemCard: Failed to cache preset image for", root.systemName, ":", error)
                root.imageLoadFailed = true
                fallbackGradient.visible = true
                if (root.cachedImageUrl && root.cachedImageUrl.trim() !== "") {
                    console.log("SystemCard: Falling back to cached file URL for", root.systemName)
                    bgImage.source = root.cachedImageUrl
                }
            }
        }
    }
    
    // Trigger image loading when backgroundImage changes
    onBackgroundImageChanged: {
        updateImageSource()
    }
    
    // Component lifecycle handlers
    Component.onCompleted: {
        // Defer image update to ensure all properties are ready
        Qt.callLater(updateImageSource)
    }
    
    Component.onDestruction: {
        // CRITICAL FIX: Mark as destroyed to prevent signal processing
        root.isDestroyed = true
        console.log("SystemCard: Component destroyed for", root.systemName)
    }
    
    // Listen for when preset images are ready
    Connections {
        target: imageLoader
        
        function onPresetImagesReady() {
            console.log("SystemCard: Preset images are ready, usingFallback:", root.usingFallback)
            // Always re-evaluate image source when presets become available
            // This fixes the race condition where some cards check before presets are loaded
            Qt.callLater(updateImageSource)
            root.usingFallback = false
        }
    }

    // Listen for bulk system image updates (uploaded ImgBB primaries) from controller
    Connections {
        target: edrhController
        function onSystemImagesUpdated(systemNames) {
            if (!root || root.isDestroyed) return
            if (!systemNames) return
            try {
                // QStringList comes in as a JS array in QML
                if (systemNames.indexOf(root.systemName) !== -1) {
                    console.log("SystemCard: systemImagesUpdated for", root.systemName)
                    Qt.callLater(updateImageSource)
                }
            } catch (e) {
                console.warn("SystemCard: error handling systemImagesUpdated:", e)
            }
        }
    }
    
    // Update the image source - separate function to avoid binding loops
    function updateImageSource() {
        // Recompute background image each call to avoid stale binding when
        // ImageLoader's internal preset map changes without a QML-notifiable property
        var bg = getBackgroundImage();

        if (bg && bg.trim() !== "") {
            // Reset cached URL when background image changes
            root.cachedImageUrl = ""
            
            // Check if this is an uploaded image (ImgBB URL) - skip caching for these
            var isUploadedImage = (systemData && systemData.images && systemData.images.trim() !== "" && 
                                 bg === systemData.images)
            
            if (isUploadedImage) {
                // Uploaded images bypass caching - use direct URL
                console.log("SystemCard: Using direct uploaded image for", root.systemName)
                bgImage.source = bg
            } else {
                // Only cache preset images from database
                var cachedPath = imageLoader.getCachedImageFileUrl(bg)
                if (cachedPath && cachedPath.trim() !== "") {
                    root.cachedImageUrl = cachedPath
                    // Use the properly encoded URL directly
                    bgImage.source = cachedPath
                    console.log("SystemCard: Using cached preset image for", root.systemName, "- URL:", cachedPath)
                } else {
                    // Show direct URL while caching
                    bgImage.source = bg
                    // Request download and cache (only for preset images)
                    imageLoader.loadImage(bg, root.systemName)
                    console.log("SystemCard: Requesting preset image cache for", root.systemName, ":", bg)
                }
            }
        } else {
            // Clear image if no background image
            root.cachedImageUrl = ""
            bgImage.source = ""
        }
    }
    
    // Main content overlay
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        radius: parent.radius
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 20  // Increased from 18 to 20
            spacing: 18  // Increased from 15 to 18
            
            // Left side - System info
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8  // Increased from 6 to 8
                
                // System name (clickable)
                Text {
                    id: systemNameText
                    text: root.systemName
                    font.pixelSize: 22  // Increased from 20 to 22
                    font.bold: true
                    color: "#FFFFFF"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    
                    // Drop shadow for better readability
                    style: Text.Outline
                    styleColor: "#000000"
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        
                        onEntered: {
                            parent.color = "#FF7F50"
                            parent.scale = 1.02
                        }
                        onExited: {
                            parent.color = "#FFFFFF"
                            parent.scale = 1.0
                        }
                        onClicked: root.systemNameCopyRequested(root.systemName)
                    }
                    
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                    
                    Behavior on scale {
                        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                    }
                }
                
                // Category
                Text {
                    text: root.category
                    font.pixelSize: 13  // Increased from 12 to 13
                    color: "#E0E0E0"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    opacity: 0.9
                    
                    style: Text.Outline
                    styleColor: "#000000"
                }
                
                // Badges row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10  // Increased from 8 to 10
                    
                    // POI badge
                    Rectangle {
                        visible: root.poi !== ""
                        width: poiText.width + 16
                        height: poiText.height + 8
                        color: root.poi === "POI" ? "#48bb78" : "#f6ad55"
                        radius: 6
                        border.width: 1
                        border.color: Qt.lighter(color, 1.3)
                        
                        // Glow effect (Qt6 compatible)
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 2
                            height: parent.height + 2
                            color: "transparent"
                            border.color: parent.color
                            border.width: 1
                            radius: parent.radius + 1
                            opacity: 0.4
                            z: -1
                        }
                        
                        Text {
                            id: poiText
                            anchors.centerIn: parent
                            text: root.poi === "POI" ? "POI" : "POTENTIAL"
                            font.pixelSize: 10
                            font.bold: true
                            color: "#FFFFFF"
                        }
                    }
                    
                    // Done badge
                    Rectangle {
                        visible: root.isDone
                        width: doneText.width + 16
                        height: doneText.height + 8
                        color: "#9f7aea"
                        radius: 6
                        border.width: 1
                        border.color: Qt.lighter(color, 1.3)
                        
                        // Glow effect (Qt6 compatible)
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 2
                            height: parent.height + 2
                            color: "transparent"
                            border.color: parent.color
                            border.width: 1
                            radius: parent.radius + 1
                            opacity: 0.4
                            z: -1
                        }
                        
                        Text {
                            id: doneText
                            anchors.centerIn: parent
                            text: "DONE"
                            font.pixelSize: 10
                            font.bold: true
                            color: "#FFFFFF"
                        }
                    }
                    
                    // Claimed badge
                    Rectangle {
                        visible: root.isClaimed
                        width: claimedText.width + 16
                        height: claimedText.height + 8
                        color: root.isClaimedByUser ? "#4299e1" : "#e53e3e"  // Blue for user's claims, red for others
                        radius: 6
                        border.width: 1
                        border.color: Qt.lighter(color, 1.3)
                        
                        // Glow effect (Qt6 compatible)
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 2
                            height: parent.height + 2
                            color: "transparent"
                            border.color: parent.color
                            border.width: 1
                            radius: parent.radius + 1
                            opacity: 0.4
                            z: -1
                        }
                        
                        Text {
                            id: claimedText
                            text: root.isClaimedByUser ? "CLAIMED" : "TAKEN"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 11
                            anchors.centerIn: parent
                        }
                    }
                    
                    // Spacer
                    Item { Layout.fillWidth: true }
                }
            }
            
            // Right side - Distance and action button
            ColumnLayout {
                Layout.preferredWidth: 140  // Increased from 130 to 140
                Layout.fillHeight: true
                spacing: 15  // Increased from 12 to 15
                
                // Distance badge
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: distanceText.width + 24  // Increased from 20 to 24
                    height: distanceText.height + 16  // Increased from 12 to 16
                    color: "#1a1a1a"
                    radius: 8
                    border.width: 2
                    border.color: "#FF7F50"
                    opacity: 0.9
                    
                    // Background glow (Qt6 compatible)
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width + 4
                        height: parent.height + 4
                        color: "transparent"
                        border.color: "#FF7F50"
                        border.width: 1
                        radius: parent.radius + 2
                        opacity: 0.5
                        z: -1
                    }
                    
                    Text {
                        id: distanceText
                        anchors.centerIn: parent
                        text: root.distance
                        font.pixelSize: 13  // Increased from 12 to 13
                        font.bold: true
                        color: "#FFFFFF"
                    }
                }
                
                // View button
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: "View"
                    width: 80  // Increased from 70 to 80
                    height: 36  // Increased from 32 to 36
                    
                    background: Rectangle {
                        id: btnBg
                        color: parent.pressed ? "#0ea5c4" : (parent.hovered ? "#17a2b8" : "#138496")
                        radius: 8
                        border.width: 1
                        border.color: Qt.lighter(btnBg.color, 1.2)
                        
                        // Subtle gradient - fixed color reference
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.lighter(btnBg.color, 1.1) }
                            GradientStop { position: 1.0; color: btnBg.color }
                        }
                        
                        // Button glow on hover (Qt6 compatible)
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + (parent.parent.hovered ? 4 : 0)
                            height: parent.height + (parent.parent.hovered ? 4 : 0)
                            color: "transparent"
                            border.color: parent.color
                            border.width: parent.parent.hovered ? 1 : 0
                            radius: parent.radius + 2
                            opacity: parent.parent.hovered ? 0.6 : 0
                            z: -1
                            
                            Behavior on width {
                                NumberAnimation { duration: 200 }
                            }
                            Behavior on height {
                                NumberAnimation { duration: 200 }
                            }
                            Behavior on opacity {
                                NumberAnimation { duration: 200 }
                            }
                        }
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14  // Increased from 13 to 14
                        font.bold: true
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: root.systemClicked(root.systemName)
                }
            }
        }
    }
    
    // Card hover effect
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
        
        onEntered: {
            root.border.color = "#FF7F50"
            root.border.width = 3
            root.scale = 1.02
            hoverGlow.visible = true
        }
        onExited: {
            root.border.color = "#333333"
            root.border.width = 2
            root.scale = 1.0
            hoverGlow.visible = false
        }
        
        onPressed: function(mouse) { mouse.accepted = false }
        onReleased: function(mouse) { mouse.accepted = false }
        onClicked: function(mouse) { mouse.accepted = false }
    }
    
    // Hover glow effect (Qt6 compatible)
    Rectangle {
        id: hoverGlow
        anchors.centerIn: parent
        width: parent.width + 6
        height: parent.height + 6
        color: "transparent"
        border.color: "#FF7F50"
        border.width: 2
        radius: parent.radius + 3
        opacity: 0.6
        visible: false
        z: -1
        
        Behavior on visible {
            NumberAnimation { duration: 200 }
        }
    }
    
    // Smooth animations
    Behavior on scale {
        NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
    }
    
    Behavior on border.color {
        ColorAnimation { duration: 200 }
    }
    
    Behavior on border.width {
        NumberAnimation { duration: 200 }
    }
    
    // Helper functions
    function getBackgroundImage() {
        // PRIORITY 1: Check for uploaded images first (ImgBB URLs from user uploads)
        // Try controller cache first, then systemData
        if (edrhController && edrhController.getPrimaryImageForSystem) {
            var cachedPrimary = edrhController.getPrimaryImageForSystem(root.systemName)
            if (cachedPrimary && cachedPrimary.trim() !== "") {
                console.log("SystemCard: Using controller-cached uploaded image for", root.systemName, ":", cachedPrimary)
                return cachedPrimary
            }
        }
        if (systemData && systemData.images && systemData.images.trim() !== "") {
            console.log("SystemCard: Using uploaded image for", root.systemName, ":", systemData.images)
            return systemData.images
        }
        
        // PRIORITY 2: Check for database-driven preset image for this category
        if (imageLoader && root.category) {
            var hasPreset = imageLoader.hasPresetImage(root.category)
            console.log("SystemCard:", root.systemName, "- Category:", root.category, "- Has preset:", hasPreset)
            
            if (hasPreset) {
                var presetUrl = imageLoader.getPresetImageUrl(root.category)
                if (presetUrl && presetUrl.trim() !== "") {
                    console.log("SystemCard: Using database preset image for category", root.category, ":", presetUrl)
                    root.usingFallback = false  // We found an image, not using fallback
                    return presetUrl
                }
            } else {
                // If no preset found but we have a valid category, it might not be loaded yet
                // Return empty string to trigger fallback, and wait for onPresetImagesReady signal
                console.log("SystemCard: No preset image found yet for", root.systemName, "- will check again when ready")
                root.usingFallback = true
                return ""
            }
        }
        
        // PRIORITY 4: Final fallback to default image
        if (imageLoader) {
            var defaultUrl = imageLoader.getPresetImageUrl("default")
            if (defaultUrl && defaultUrl.trim() !== "") {
                console.log("SystemCard: Using default preset image for", root.systemName)
                return defaultUrl
            }
        }
        
        // PRIORITY 5: Emergency fallback if ImageLoader fails
        console.log("SystemCard: Using emergency fallback (gradient) for", root.systemName)
        root.usingFallback = true
        return ""  // Will trigger gradient fallback
    }
    
    function getCategoryColor() {
        // Enhanced safety checks for category property
        var categoryValue = ""
        try {
            // Check if root exists and has category property
            if (typeof root !== 'undefined' && root !== null && 'category' in root) {
                categoryValue = root.category || ""
            }
        } catch (e) {
            console.warn("Error accessing category:", e)
            return "#FF7F50"  // Default accent color
        }
        
        // Additional safety checks
        if (typeof categoryValue !== "string" || categoryValue === "" || categoryValue === null || categoryValue === undefined) {
            return "#FF7F50"  // Default accent color
        }
        
        // Generate deterministic color from category string
        return generateHashColor(categoryValue);
    }
    
    // Deterministic hash-based color generation
    function generateHashColor(str) {
        // Simple hash function for consistent results
        var hash = 0;
        for (var i = 0; i < str.length; i++) {
            var character = str.charAt(i);
            hash = ((hash << 5) - hash) + character.charCodeAt(0);
            hash = hash & hash; // Convert to 32-bit integer
        }
        
        // Convert hash to positive number
        hash = Math.abs(hash);
        
        // Generate HSL values for better color distribution
        var hue = hash % 360; // 0-359 degrees
        var saturation = 65 + (hash % 25); // 65-89% for vibrant colors
        var lightness = 45 + (hash % 20); // 45-64% for good contrast
        
        // Convert HSL to RGB
        var rgb = hslToRgb(hue / 360, saturation / 100, lightness / 100);
        
        // Convert to hex
        var hex = "#" + 
            ("0" + Math.round(rgb[0] * 255).toString(16)).slice(-2) +
            ("0" + Math.round(rgb[1] * 255).toString(16)).slice(-2) +
            ("0" + Math.round(rgb[2] * 255).toString(16)).slice(-2);
            
        return hex;
    }
    
    // HSL to RGB conversion helper
    function hslToRgb(h, s, l) {
        var r, g, b;
        
        if (s === 0) {
            r = g = b = l; // achromatic
        } else {
            function hue2rgb(p, q, t) {
                if (t < 0) t += 1;
                if (t > 1) t -= 1;
                if (t < 1/6) return p + (q - p) * 6 * t;
                if (t < 1/2) return q;
                if (t < 2/3) return p + (q - p) * (2/3 - t) * 6;
                return p;
            }
            
            var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
            var p = 2 * l - q;
            r = hue2rgb(p, q, h + 1/3);
            g = hue2rgb(p, q, h);
            b = hue2rgb(p, q, h - 1/3);
        }
        
        return [r, g, b];
    }
} 