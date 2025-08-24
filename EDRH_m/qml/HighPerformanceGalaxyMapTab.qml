import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import EDRH.Components 1.0
import EDRH 1.0

Item {
    id: root
    
    // Navigation properties
    property real zoomLevel: 1.0
    property point panOffset: Qt.point(0, 0)
    property bool isLoading: false
    
    // Right-click debounce
    property bool rightClickInProgress: false
    
    Component.onCompleted: {
        console.log("Galaxy Map: Initializing...")
        
        // Connect to real-time updates with safety checks
        try {
            if (edrhController && edrhController.galaxyMapSystemsChanged) {
                edrhController.galaxyMapSystemsChanged.connect(updateSystemsData)
            }
            
            // Load galaxy map data immediately if controller is ready
            if (edrhController && typeof edrhController.loadGalaxyMapData === 'function') {
                edrhController.loadGalaxyMapData()
            }
            
            // Also try refreshData if no systems are available
            if (edrhController && (!edrhController.galaxyMapSystems || edrhController.galaxyMapSystems.length === 0)) {
                if (typeof edrhController.refreshData === 'function') {
                    edrhController.refreshData()
                }
            }
            
            // Add delay to ensure all components are initialized
            timer.setTimeout(function() {
                updateSystemsData()
            }, 100)
            
            // Force initial update after a longer delay to ensure controller is ready
            timer.setTimeout(function() {
                updateSystemsData()
            }, 500)
        } catch (error) {
            console.log("Galaxy Map initialization error:", error)
        }
    }
    
    // Simple timer for delayed execution
    Timer {
        id: timer
        
        function setTimeout(callback, delay) {
            timer.interval = delay
            timer.repeat = false
            timer.triggered.connect(callback)
            timer.start()
        }
    }
    
    function updateSystemsData() {
        console.log("=== updateSystemsData called ===")
        console.log("  edrhController available:", typeof edrhController !== 'undefined')
        if (edrhController) {
            console.log("  galaxyMapSystems count:", edrhController.galaxyMapSystems ? edrhController.galaxyMapSystems.length : 0)
        }
        updateFilteredSystems()
    }
    
    // Filter Manager instance (same as in NearestSystemsPanel)  
    FilterManager {
        id: filterManager
        
        onFiltersChanged: {
            updateFilteredSystems()
        }

    }
    
    // Filtered systems for the galaxy map
    property var filteredGalaxyMapSystems: []
    
    // Timer for retrying star renderer updates with size checking
    Timer {
        id: retryTimer
        interval: 200
        repeat: true
        running: false
        property int attempts: 0
        onTriggered: {
            attempts++
            
            // Only retry if we have both data AND proper dimensions
            if (filteredGalaxyMapSystems.length > 0 && galaxyRenderer.width > 0 && galaxyRenderer.height > 0) {
                try {
                    galaxyRenderer.starSystems = filteredGalaxyMapSystems
                    stop() // Stop retrying if successful
                } catch (e) {
                    if (attempts >= 20) {
                        stop()
                    }
                }
            } else {
                if (filteredGalaxyMapSystems.length === 0 || attempts >= 20) {
                    stop()
                }
            }
        }
    }
    
    // Additional force timer to ensure data gets set
    Timer {
        id: forceTimer
        interval: 500
        repeat: true
        running: true
        property int attempts: 0
        onTriggered: {
            attempts++
            if (filteredGalaxyMapSystems.length > 0 && galaxyRenderer.width > 0 && galaxyRenderer.height > 0) {
                // Force reload by clearing and setting again
                var currentSystems = galaxyRenderer.starSystems
                galaxyRenderer.starSystems = []
                galaxyRenderer.starSystems = filteredGalaxyMapSystems
                stop()
            } else if (attempts >= 40) { // 20 seconds max
                stop()
            }
        }
    }
    
    // Force update when filtered systems change
    onFilteredGalaxyMapSystemsChanged: {
        // Only start retry if we have data AND renderer has proper size
        if (filteredGalaxyMapSystems.length > 0) {
            // Force immediate update if component is ready
            if (galaxyRenderer.width > 0 && galaxyRenderer.height > 0) {
                galaxyRenderer.starSystems = filteredGalaxyMapSystems
            } else {
                // Start retry timer with reset attempts
                retryTimer.attempts = 0
                retryTimer.restart()
            }
        }
    }
    

    
    function updateFilteredSystems() {
        try {
            console.log("=== updateFilteredSystems called ===")
            var filtered = []
            var sourceData = edrhController.galaxyMapSystems || []
            
            console.log("  Source data count:", sourceData.length)
            
            if (!sourceData || sourceData.length === 0) {
                console.log("  No source data - setting empty filtered list")
                root.filteredGalaxyMapSystems = filtered
                return
            }
            
            // Check filter states
            console.log("  Filter states:")
            console.log("    Unclaimed:", showUnclaimedCheck.checked)
            console.log("    Your Claims:", showYourClaimsCheck.checked)
            console.log("    Others Claims:", showOthersClaimsCheck.checked)
            console.log("    Potential POIs:", showPotentialPOIsCheck.checked)
            console.log("    POIs:", showPOIsCheck.checked)
            console.log("    Done Systems:", showDoneSystemsCheck.checked)
            
            // Apply claim status filters FIRST (like v1.4incomplete.py)
            for (var i = 0; i < sourceData.length; i++) {
                var system = sourceData[i]
                
                if (!system) continue  // Skip null/undefined systems
            
            // Check claim status filters (checkboxes) - like v1.4incomplete.py
            var claimed = system.claimed || false
            var claimedBy = system.claimedBy || ""
            var poi = system.poi || ""
            var currentCommander = edrhController.commanderName || "Regza"
            
            // If ANY checkboxes are checked, apply filtering. If NONE are checked, show NOTHING.
            var anyClaimFilterActive = showUnclaimedCheck.checked || showYourClaimsCheck.checked || 
                                      showOthersClaimsCheck.checked || showPotentialPOIsCheck.checked ||
                                      showPOIsCheck.checked || showDoneSystemsCheck.checked
            
            if (!anyClaimFilterActive) {
                // If no claim filters are active, skip all systems (show nothing by default)
                continue
            }
            
            // Check which filters pass
            var passesClaimFilter = false
            
            if (showUnclaimedCheck.checked && !claimed) {
                passesClaimFilter = true
            }
            if (showYourClaimsCheck.checked && claimed && claimedBy === currentCommander) {
                passesClaimFilter = true
            }
            if (showOthersClaimsCheck.checked && claimed && claimedBy !== currentCommander) {
                passesClaimFilter = true
            }
            if (showPotentialPOIsCheck.checked && poi === "Potential POI") {
                passesClaimFilter = true
            }
            if (showPOIsCheck.checked && poi === "POI") {
                passesClaimFilter = true
            }
            if (showDoneSystemsCheck.checked && (system.status === "Done" || system.done === true)) {
                passesClaimFilter = true
            }
            
            // If claim filters are active but this system doesn't pass, skip it
            if (!passesClaimFilter) {
                continue
            }
            
            // Then check category filters using FilterManager
            var matchesCategory = filterManager.matchesFilter(system)
            
            if (matchesCategory) {
                // Additional check to exclude Richard's categories
                var systemCategory = system.category || ""
                var richardCategories = ["PVP (tilted moon)", "PVP (black hole gas giant moons)", "PVP (close binary)"]
                var isRichardSystem = false
                
                for (var j = 0; j < richardCategories.length; j++) {
                    if (systemCategory === richardCategories[j]) {
                        isRichardSystem = true
                        break
                    }
                }
                
                if (!isRichardSystem) {
                    filtered.push(system)
                    if (filtered.length <= 5) {  // Only log first 5 systems
                        console.log("      ADDED system:", system.name, "claimed:", system.claimed)
                    }
                }
            } else if (filtered.length === 0) {
                // If we haven't added any systems yet, log why this one was rejected
                console.log("    System " + (system.name || "Unknown") + " REJECTED by category filter")
            }
            }
            
            console.log("  FILTERED RESULT:", filtered.length, "systems passed filters")
            if (filtered.length > 0) {
                console.log("    Sample system:", JSON.stringify(filtered[0]))
            }
            
            // Set filtered systems directly
            root.filteredGalaxyMapSystems = filtered
            
            // Update the star renderer safely
            if (galaxyRenderer && typeof galaxyRenderer.starSystems !== "undefined") {
                var previousCount = galaxyRenderer.starSystems ? galaxyRenderer.starSystems.length : -1
                galaxyRenderer.starSystems = root.filteredGalaxyMapSystems
                
                console.log("  Updated renderer: previousCount=" + previousCount + ", newCount=" + filtered.length)
                console.log("  Renderer size:" + galaxyRenderer.width + "x" + galaxyRenderer.height)
            } else {
                console.log("  WARNING: galaxyRenderer not available or starSystems property undefined")
            }
            
        } catch (error) {
            console.log("Error in updateFilteredSystems:", error)
            // Set empty array on error to prevent crashes
            root.filteredGalaxyMapSystems = []
            if (galaxyRenderer && typeof galaxyRenderer.starSystems !== "undefined") {
                galaxyRenderer.starSystems = []
            }
        }
    }
    
    // Debounce timer for right-click
    Timer {
        id: rightClickDebounceTimer
        interval: 300 // 300ms debounce
        repeat: false
        onTriggered: {
            var timestamp = new Date().toISOString().substr(11, 12)
            console.log("⏰ [" + timestamp + "] QML Right-click debounce timer TRIGGERED - clearing protection")
            root.rightClickInProgress = false
            console.log("⏰ [" + timestamp + "] QML Right-click debounce protection CLEARED")
        }
    }
    
    // Right-click handler to open SystemViewPopup for clicked star
    function handleRightClick(mouseX, mouseY) {
        var timestamp = new Date().toISOString().substr(11, 12)
        
        // Debounce rapid right-clicks
        if (root.rightClickInProgress) {
            console.log("⏸️ [" + timestamp + "] QML Right-click DEBOUNCED")
            return
        }
        
        console.log("🖱️ [" + timestamp + "] QML Right-click RECEIVED at:", mouseX, mouseY, "(zoom:", root.zoomLevel + ", pan:", root.panOffset.x + "," + root.panOffset.y + ")")
        
        if (!root.filteredGalaxyMapSystems || root.filteredGalaxyMapSystems.length === 0) {
            console.log("❌ [" + timestamp + "] No systems available - count:", root.filteredGalaxyMapSystems ? root.filteredGalaxyMapSystems.length : "null")
            return
        }
        
        // Find the closest system to the click position
        var clickedSystem = findSystemAtPosition(mouseX, mouseY)
        if (clickedSystem) {
            console.log("✅ [" + timestamp + "] Found system:", clickedSystem.name, "at", clickedSystem.x + "," + clickedSystem.z)
            
            // SET DEBOUNCE BEFORE making the call to prevent double-clicks
            root.rightClickInProgress = true
            rightClickDebounceTimer.restart()
            console.log("🔒 [" + timestamp + "] QML Right-click debounce SET for:", clickedSystem.name)
            
            // Open the system popup using the same pattern as SystemCard
            if (edrhController && typeof edrhController.viewSystem === 'function') {
                console.log("🚀 [" + timestamp + "] QML CALLING edrhController.viewSystem() for:", clickedSystem.name)
                edrhController.viewSystem(clickedSystem.name)
                console.log("🚀 [" + timestamp + "] QML edrhController.viewSystem() CALL COMPLETED for:", clickedSystem.name)
            } else {
                console.log("❌ [" + timestamp + "] edrhController.viewSystem not available")
                // Reset debounce if call failed
                root.rightClickInProgress = false
                rightClickDebounceTimer.stop()
            }
        } else {
            console.log("❌ [" + timestamp + "] No system found at click position")
            // Don't set debounce if no system was found - allow immediate retry
        }
    }
    
    // Find system closest to screen coordinates
    function findSystemAtPosition(mouseX, mouseY) {
        if (!root.filteredGalaxyMapSystems || root.filteredGalaxyMapSystems.length === 0 || 
            !galaxyRenderer || galaxyRenderer.width <= 0 || galaxyRenderer.height <= 0) {
            console.log("❌ Prerequisites failed for findSystemAtPosition")
            return null
        }
        
        var closestSystem = null
        var minDistance = Number.MAX_VALUE
        var clickThreshold = 30 // Reasonable threshold for clicking
        var checkedSystems = 0
        var validSystems = 0
        
        // Check systems within threshold (reduced logging)
        
        // Convert galaxy coordinates to screen coordinates and find closest
        for (var i = 0; i < root.filteredGalaxyMapSystems.length; i++) {
            var system = root.filteredGalaxyMapSystems[i]
            checkedSystems++
            
            if (!system) {
                if (i < 3) console.log("System", i, "is null")
                continue
            }
            
            // Check if system has valid coordinates - try different possible field names
            var systemX = system.x || system.galactic_x || system.coord_x
            var systemZ = system.z || system.galactic_z || system.coord_z
            
            if (typeof systemX === 'undefined' || typeof systemZ === 'undefined') {
                if (i < 3) console.log("System", i, "(", system.name, ") missing coordinates. Available fields:", Object.keys(system))
                continue
            }
            
            validSystems++
            
            // Convert 3D galaxy coordinates to 2D screen coordinates
            var screenPos = galaxyCoordinatesToScreen(systemX, systemZ)
            if (!screenPos) {
                if (i < 3) console.log("Failed to convert coordinates for", system.name)
                continue
            }
            
            // Calculate distance from mouse click to star position
            var dx = mouseX - screenPos.x
            var dy = mouseY - screenPos.y
            var distance = Math.sqrt(dx * dx + dy * dy)
            
            // Only log very close systems or first few for debugging
            if (distance < clickThreshold || i < 3) {
                console.log("System", system.name, "at screen pos", screenPos.x.toFixed(1) + "," + screenPos.y.toFixed(1), "distance:", distance.toFixed(1))
            }
            
            if (distance < minDistance && distance < clickThreshold) {
                minDistance = distance
                closestSystem = system
            }
        }
        
        // Only log result if we found a system
        if (closestSystem) {
            console.log("🎯 Found system:", closestSystem.name, "distance:", minDistance.toFixed(1) + "px")
        }
        
        return closestSystem
    }
    
    // Convert galaxy coordinates to screen coordinates - MATCHES C++ GalaxyMapRenderer exactly
    function galaxyCoordinatesToScreen(galaxyX, galaxyZ) {
        if (!galaxyRenderer || galaxyRenderer.width <= 0 || galaxyRenderer.height <= 0) {
            console.log("❌ galaxyCoordinatesToScreen: Invalid renderer")
            return null
        }
        
        // Validate input coordinates
        if (typeof galaxyX !== 'number' || typeof galaxyZ !== 'number') {
            console.log("❌ galaxyCoordinatesToScreen: Invalid coordinates", galaxyX, galaxyZ)
            return null
        }
        
        // EXACT same constants as C++ GalaxyMapRenderer
        var LY_PER_PIXEL = 40.0
        var ORIG_OFF_X = 1124.0
        var ORIG_OFF_Y = 1749.0
        
        // Step 1: Transform to pixel coordinates (same as C++)
        var px = ORIG_OFF_X + galaxyX / LY_PER_PIXEL
        var py = ORIG_OFF_Y - galaxyZ / LY_PER_PIXEL  // CRITICAL: Minus for Z coordinate!
        
        // Step 2: Python scaling logic (EXACTLY like C++ which mimics Python v1.4incomplete.py)
        var base_full_width = 2250.0
        var base_full_height = 2250.0
        var base_med_width = 800.0
        var zoom = 1.5  // Default zoom to make galaxy bigger
        
        // Calculate resized dimensions
        var resized_width = base_med_width * zoom
        var resized_height = base_med_width * zoom  // Square image
        
        // Calculate scale factor
        var scale = resized_width / base_full_width
        
        var canvas_width = galaxyRenderer.width
        var canvas_height = galaxyRenderer.height
        
        // Use the calculated scale for positioning
        var image_width = resized_width
        var image_height = resized_height
        
        // Center the scaled image in the window
        var x0 = (canvas_width - image_width) / 2.0
        var y0 = (canvas_height - image_height) / 2.0
        
        // Apply final transformation - this gives us the base star position
        var baseX = x0 + px * scale
        var baseY = y0 + py * scale
        
        // Now apply the current zoom and pan transformations (same as C++ paint method)
        var centerX = canvas_width / 2.0
        var centerY = canvas_height / 2.0
        
        // Apply the same transformation as C++ paint()
        // 1. Translate to center
        var transformedX = baseX - centerX
        var transformedY = baseY - centerY
        
        // 2. Apply zoom
        transformedX *= root.zoomLevel
        transformedY *= root.zoomLevel
        
        // 3. Apply pan offset
        transformedX += root.panOffset.x
        transformedY += root.panOffset.y
        
        // 4. Translate back to screen coordinates
        var screenX = transformedX + centerX
        var screenY = transformedY + centerY
        
        // Debug logging for Sol
        if (Math.abs(galaxyX) < 0.1 && Math.abs(galaxyZ) < 0.1) {
            console.log("Sol (0,0) -> px,py:", px.toFixed(1) + "," + py.toFixed(1), "-> base:", baseX.toFixed(1) + "," + baseY.toFixed(1), "-> screen:", screenX.toFixed(1) + "," + screenY.toFixed(1))
        }
        
        return Qt.point(screenX, screenY)
    }
    

    
    // Galaxy map with high-performance C++ renderer
    GalaxyMapRenderer {
        id: galaxyRenderer
        anchors.fill: parent
        // Fallback explicit dimensions if anchoring fails
        width: parent.width > 0 ? parent.width : 1200
        height: parent.height > 0 ? parent.height : 800
        
        zoomLevel: root.zoomLevel
        panOffset: root.panOffset
        backgroundImage: "assets/E47CDFX.png"
        commanderPosition: Qt.point((edrhController && edrhController.commanderX) ? edrhController.commanderX : 0, 
                                   (edrhController && edrhController.commanderZ) ? edrhController.commanderZ : 0)
        
        // Use default star visibility - filtering is done by claim status, not star type
        showMainSequence: true
        showNeutronStars: true
        showWhiteDwarfs: true
        showBlackHoles: true
        
        // Tooltips enabled with proper positioning and scaling
        tooltipsEnabled: true
        
        // Commander location properties
        showAllCommanders: showAllCommandersCheck.checked
        isAdminMode: edrhController.isAdmin || false
        allCommanderLocations: edrhController.allCommanderLocations || []
        
        // Connect to filtered star data from EDRH controller
        starSystems: root.filteredGalaxyMapSystems || []
        
        // Listen for component completion and size changes
        Component.onCompleted: {
            // Force update when component is ready
            retryDataUpdate()
            
            // Also retry after a short delay to handle timing issues
            Qt.callLater(function() {
                retryDataUpdate()
            })
        }
        
        // Listen for new galaxy map data and immediately update
        Connections {
            target: edrhController
            function onGalaxyMapSystemsChanged() {
                Qt.callLater(updateFilteredSystems)
            }
        }
        
        // Retry when component gets proper dimensions
        onWidthChanged: {
            if (width > 0 && height > 0 && root.filteredGalaxyMapSystems.length > 0) {
                // Force reload since component now has proper size
                var currentSystems = starSystems
                starSystems = []  // Clear first
                starSystems = currentSystems  // Then set again to force loadRealStarSystems()
                retryTimer.stop()
            }
        }
        
        onHeightChanged: {
            if (width > 0 && height > 0 && root.filteredGalaxyMapSystems.length > 0) {
                // Force reload since component now has proper size
                var currentSystems = starSystems
                starSystems = []  // Clear first
                starSystems = currentSystems  // Then set again to force loadRealStarSystems()
                retryTimer.stop()
            }
        }
        
        function retryDataUpdate() {
            try {
                if (root.filteredGalaxyMapSystems && root.filteredGalaxyMapSystems.length > 0 && width > 0 && height > 0) {
                    starSystems = root.filteredGalaxyMapSystems
                    // Stop any running retry timer since we succeeded
                    if (retryTimer) retryTimer.stop()
                } else if (root.filteredGalaxyMapSystems && root.filteredGalaxyMapSystems.length > 0) {
                    // Don't start retry timer immediately - wait for proper sizing
                    if (width > 0 && height > 0) {
                        if (retryTimer) {
                            retryTimer.attempts = 0
                            retryTimer.restart()
                        }
                    }
                }
            } catch (error) {
                console.log("Error in retryDataUpdate:", error)
            }
        }
        
        onSystemRightClicked: (systemName, x, y, z) => {
            // DISABLED: This conflicts with our QML right-click handler above
            // The QML MouseArea right-click handler takes precedence to avoid duplication
            console.log("🚫 C++ systemRightClicked signal ignored to prevent duplication")
            console.log("   System:", systemName, "- handled by QML MouseArea instead")
        }
        
        // Mouse interaction for pan and zoom
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            
            property point lastPanPoint
            property bool isPanning: false
            
            onPressed: (mouse) => {
                if (mouse.button === Qt.LeftButton) {
                    lastPanPoint = Qt.point(mouse.x, mouse.y)
                    isPanning = true
                } else if (mouse.button === Qt.RightButton) {
                    // Right-click to open system popup
                    handleRightClick(mouse.x, mouse.y)
                }
            }
            
            onPositionChanged: (mouse) => {
                if (isPanning && (mouse.buttons & Qt.LeftButton)) {
                    var delta = Qt.point(mouse.x - lastPanPoint.x, mouse.y - lastPanPoint.y)
                    root.panOffset = Qt.point(root.panOffset.x + delta.x, root.panOffset.y + delta.y)
                    lastPanPoint = Qt.point(mouse.x, mouse.y)
                }
            }
            
            onReleased: (mouse) => {
                if (mouse.button === Qt.LeftButton) {
                    isPanning = false
                }
            }
            
            onWheel: (wheel) => {
                // Mouse-centered zoom with stability improvements for high zoom levels
                var zoomFactor = wheel.angleDelta.y > 0 ? 1.15 : 1.0/1.15
                var newZoomLevel = Math.max(0.1, Math.min(20.0, root.zoomLevel * zoomFactor))
                
                // Skip update if zoom didn't actually change (prevents precision issues)
                if (Math.abs(newZoomLevel - root.zoomLevel) < 0.001) {
                    return
                }
                
                // Get mouse position
                var mouseX = wheel.x
                var mouseY = wheel.y
                
                // Add bounds checking for stability at high zoom levels
                if (mouseX < 0 || mouseX > width || mouseY < 0 || mouseY > height) {
                    root.zoomLevel = newZoomLevel
                    return
                }
                
                // Apply EXACT inverse transformation sequence as in C++ paint():
                // 1. translate(width/2, height/2)
                // 2. translate(panOffset)  
                // 3. scale(zoom)
                // 4. translate(-width/2, -height/2)
                
                // Step 1: Move mouse to origin relative to center
                var step1X = mouseX - width / 2.0
                var step1Y = mouseY - height / 2.0
                
                // Step 2: Remove pan offset
                var step2X = step1X - root.panOffset.x
                var step2Y = step1Y - root.panOffset.y
                
                // Step 3: Remove zoom (inverse scale) with precision check
                if (root.zoomLevel < 0.001) {
                    root.zoomLevel = newZoomLevel
                    return
                }
                var worldX = step2X / root.zoomLevel
                var worldY = step2Y / root.zoomLevel
                
                // Clamp world coordinates to prevent excessive values at high zoom
                var maxWorldCoord = 100000
                worldX = Math.max(-maxWorldCoord, Math.min(maxWorldCoord, worldX))
                worldY = Math.max(-maxWorldCoord, Math.min(maxWorldCoord, worldY))
                
                // Apply zoom change
                root.zoomLevel = newZoomLevel
                
                // Now transform world coordinates back to screen with new zoom
                // Step 3: Apply new zoom
                var newStep2X = worldX * root.zoomLevel
                var newStep2Y = worldY * root.zoomLevel
                
                // Step 2: Calculate new pan offset with bounds checking
                var newPanX = step1X - newStep2X
                var newPanY = step1Y - newStep2Y
                
                // Clamp pan offset to prevent excessive values
                var maxPan = 50000
                newPanX = Math.max(-maxPan, Math.min(maxPan, newPanX))
                newPanY = Math.max(-maxPan, Math.min(maxPan, newPanY))
                
                root.panOffset = Qt.point(newPanX, newPanY)
            }
        }
        
        // Keyboard navigation
        Keys.onPressed: (event) => {
            var panSpeed = 20
            
            switch (event.key) {
                case Qt.Key_Left:
                    root.panOffset = Qt.point(root.panOffset.x + panSpeed, root.panOffset.y)
                    event.accepted = true
                    break
                case Qt.Key_Right:
                    root.panOffset = Qt.point(root.panOffset.x - panSpeed, root.panOffset.y)
                    event.accepted = true
                    break
                case Qt.Key_Up:
                    root.panOffset = Qt.point(root.panOffset.x, root.panOffset.y + panSpeed)
                    event.accepted = true
                    break
                case Qt.Key_Down:
                    root.panOffset = Qt.point(root.panOffset.x, root.panOffset.y - panSpeed)
                    event.accepted = true
                    break
                // Keyboard zoom removed - use mouse wheel for proper mouse-centered zoom
                case Qt.Key_Space:
                case Qt.Key_Home:
                    root.zoomLevel = 1.0
                    root.panOffset = Qt.point(0, 0)
                    event.accepted = true
                    break
            }
        }
        
        focus: true
    }
    
    // Collapsible Filter Panel - Compact Design
    Rectangle {
        id: galaxyFilterPanel
        width: filterPanelExpanded ? 240 : 35
        height: filterPanelExpanded ? 500 : 35
        x: 10
        y: 10
        color: Qt.rgba(0.05, 0.05, 0.1, 0.95)
        radius: 8
        border.width: 1
        border.color: "#FF7F50"
        
        property bool filterPanelExpanded: true  // Start expanded for debugging
        
        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        
        // Toggle Button (always visible)
        Rectangle {
            id: toggleButton
            width: 30
            height: 30
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 2
            color: toggleMouseArea.pressed ? "#FF9068" : (toggleMouseArea.containsMouse ? "#FF7F50" : "#444444")
            radius: 6
            border.width: 1
            border.color: "#FF7F50"
            
            Text {
                anchors.centerIn: parent
                text: galaxyFilterPanel.filterPanelExpanded ? "X" : "="
                color: "#FFFFFF"
                font.pixelSize: 12
                font.bold: true
            }
            
            MouseArea {
                id: toggleMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    galaxyFilterPanel.filterPanelExpanded = !galaxyFilterPanel.filterPanelExpanded
                }
            }
        }
        
        // Filter Content (only visible when expanded)
        Column {
            id: filterContent
            anchors.fill: parent
            anchors.margins: 8
            anchors.topMargin: 38
            spacing: 6
            visible: galaxyFilterPanel.filterPanelExpanded
            
            // Compact Header
            Text {
                text: "FILTERS"
                color: "#FF7F50"
                font.bold: true
                font.pixelSize: 14
            }
            
            Rectangle { width: parent.width; height: 1; color: "#FF7F50"; opacity: 0.3 }
            
            // Compact Claim Status Filters
            Text {
                text: "Claim Status:"
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 12
            }
            
            // Individual checkbox controls (safer than component for compatibility)
            Row {
                id: showUnclaimedCheck
                property bool checked: true  // Default to showing unclaimed systems
                spacing: 8
                height: 20
                
                Rectangle {
                    width: 14
                    height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    border.color: "#FF7F50"
                    border.width: 1
                    color: parent.checked ? "#FF7F50" : "transparent"
                    
                    Text {
                        text: "v"
                        color: "#000000"
                        anchors.centerIn: parent
                        visible: parent.parent.checked
                        font.pixelSize: 10
                        font.bold: true
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
                
                Text {
                    text: "Unclaimed"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
            }
            
            Row {
                id: showYourClaimsCheck
                property bool checked: true  // Default to showing your claims too
                spacing: 8
                height: 20
                
                Rectangle {
                    width: 14
                    height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    border.color: "#FF7F50"
                    border.width: 1
                    color: parent.checked ? "#FF7F50" : "transparent"
                    
                    Text {
                        text: "v"
                        color: "#000000"
                        anchors.centerIn: parent
                        visible: parent.parent.checked
                        font.pixelSize: 10
                        font.bold: true
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
                
                Text {
                    text: "Your Claims"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
            }
            
            Row {
                id: showOthersClaimsCheck
                property bool checked: false
                spacing: 8
                height: 20
                
                Rectangle {
                    width: 14
                    height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    border.color: "#FF7F50"
                    border.width: 1
                    color: parent.checked ? "#FF7F50" : "transparent"
                    
                    Text {
                        text: "✓"
                        color: "#000000"
                        anchors.centerIn: parent
                        visible: parent.parent.checked
                        font.pixelSize: 10
                        font.bold: true
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
                
                Text {
                    text: "Others' Claims"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
            }
            
            Row {
                id: showPotentialPOIsCheck
                property bool checked: false
                spacing: 8
                height: 20
                
                Rectangle {
                    width: 14
                    height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    border.color: "#FF7F50"
                    border.width: 1
                    color: parent.checked ? "#FF7F50" : "transparent"
                    
                    Text {
                        text: "✓"
                        color: "#000000"
                        anchors.centerIn: parent
                        visible: parent.parent.checked
                        font.pixelSize: 10
                        font.bold: true
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
                
                Text {
                    text: "Potential POIs"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
            }
            
            Row {
                id: showPOIsCheck
                property bool checked: false
                spacing: 8
                height: 20
                
                Rectangle {
                    width: 14
                    height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    border.color: "#FF7F50"
                    border.width: 1
                    color: parent.checked ? "#FF7F50" : "transparent"
                    
                    Text {
                        text: "✓"
                        color: "#000000"
                        anchors.centerIn: parent
                        visible: parent.parent.checked
                        font.pixelSize: 10
                        font.bold: true
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
                
                Text {
                    text: "POIs"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
            }
            
            Row {
                id: showDoneSystemsCheck
                property bool checked: false
                spacing: 8
                height: 20
                
                Rectangle {
                    width: 14
                    height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    border.color: "#FF7F50"
                    border.width: 1
                    color: parent.checked ? "#FF7F50" : "transparent"
                    
                    Text {
                        text: "✓"
                        color: "#000000"
                        anchors.centerIn: parent
                        visible: parent.parent.checked
                        font.pixelSize: 10
                        font.bold: true
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
                
                Text {
                    text: "Done Systems"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                            updateFilteredSystems()
                        }
                    }
                }
            }
            
            Rectangle { width: parent.width; height: 1; color: "#555555"; opacity: 0.5 }
            
            // Admin Mode Controls
            Text {
                text: "Admin Features:"
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 12
                visible: edrhController.isAdmin || false
            }
            
            Row {
                id: showAllCommandersCheck
                property bool checked: false
                spacing: 8
                height: 20
                visible: edrhController.isAdmin || false
                
                Rectangle {
                    width: 14
                    height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    border.color: "#FFA500"
                    border.width: 1
                    color: parent.checked ? "#FFA500" : "transparent"
                    
                    Text {
                        text: "✓"
                        color: "#000000"
                        anchors.centerIn: parent
                        visible: parent.parent.checked
                        font.pixelSize: 10
                        font.bold: true
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                        }
                    }
                }
                
                Text {
                    text: "Show All Commander Locations"
                    color: "#FFA500"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            parent.parent.checked = !parent.parent.checked
                        }
                    }
                }
            }
            
            Rectangle { width: parent.width; height: 1; color: "#555555"; opacity: 0.5 }
            
            // Compact Categories Filter
            Text {
                text: "Categories:"
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 12
            }
            
            // Compact category filter button
            Rectangle {
                id: categoryFilterBtn
                width: parent.width
                height: 24
                color: categoryMouseArea.pressed ? "#404040" : "#363636"
                border.color: "#FF7F50"
                border.width: 1
                radius: 4
                
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: filterManager.getSelectedCategoriesText()
                    font.pixelSize: 10
                    color: "#FFFFFF"
                    elide: Text.ElideRight
                    width: parent.width - 24
                }
                
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: categoryDropdown.visible ? "^" : "v"
                    font.pixelSize: 10
                    color: "#FF7F50"
                }
                
                MouseArea {
                    id: categoryMouseArea
                    anchors.fill: parent
                    onClicked: {
                        categoryDropdown.visible = !categoryDropdown.visible
                    }
                }
            }
            
            // Compact category dropdown
            Rectangle {
                id: categoryDropdown
                width: parent.width
                height: visible ? 120 : 0
                visible: false
                color: "#2A2A2A"
                radius: 4
                border.width: 1
                border.color: "#FF7F50"
                
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    
                    Column {
                        width: parent.width
                        spacing: 1
                        
                        Repeater {
                            model: filterManager.availableCategories
                            
                            delegate: Rectangle {
                                readonly property bool isSeparator: modelData === "--- Richard's Stuff ---"
                                
                                width: parent.width
                                height: isSeparator ? 16 : 18
                                color: !isSeparator && categoryItemMouseArea.containsMouse ? "#505050" : "transparent"
                                radius: 2
                                
                                Row {
                                    visible: !isSeparator
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 4
                                    spacing: 4
                                    
                                    Rectangle {
                                        width: 10
                                        height: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "transparent"
                                        border.color: "#FF7F50"
                                        border.width: 1
                                        radius: 1
                                        
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 4
                                            height: 4
                                            color: "#FF7F50"
                                            radius: 1
                                            visible: filterManager.selectedCategories.includes(modelData)
                                        }
                                    }
                                    
                                    Text {
                                        text: modelData
                                        color: "#FFFFFF"
                                        font.pixelSize: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        elide: Text.ElideRight
                                        width: categoryDropdown.width - 20
                                    }
                                }
                                
                                // Separator
                                Rectangle {
                                    visible: isSeparator
                                    anchors.centerIn: parent
                                    width: parent.width - 8
                                    height: 1
                                    color: "#FF7F50"
                                    opacity: 0.5
                                }
                                
                                MouseArea {
                                    id: categoryItemMouseArea
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
            
            // Debug info and system count display removed per user request
            

        }
    }

} 