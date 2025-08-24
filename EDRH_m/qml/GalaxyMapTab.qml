import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Shapes 1.15

// High-Performance Galaxy Map with Modern UI
Rectangle {
    id: root
    color: "#000000"  // Pure black
    
    Component.onCompleted: {
        console.log("=== GALAXY MAP TAB LOADED ===")
        console.log("Size:", width, "x", height)
        console.log("Loading state:", isLoading)
        
        // Connect to real database systems from EDRHController
        if (typeof edrhController !== 'undefined') {
            console.log("Connecting to EDRH Controller for real system data")
            updateSystemsFromController()
            
            // Listen for changes to galaxy map systems
            edrhController.galaxyMapSystemsChanged.connect(updateSystemsFromController)
        } else {
            console.log("Warning: EDRHController not available - using test data")
        }
    }
    
    function updateSystemsFromController() {
        if (typeof edrhController !== 'undefined' && edrhController.galaxyMapSystems) {
            console.log("Updating galaxy map with", edrhController.galaxyMapSystems.length, "real systems from database")
            
            // Convert systems data for the canvas
            mapCanvas.allSystems = edrhController.galaxyMapSystems
            mapCanvas.requestRedraw()
            
            // Update loading state
            root.isLoading = edrhController.galaxyMapLoading
        }
    }
    
    property alias mapCanvas: mapCanvas
    property real zoomLevel: 1.0
    property point panOffset: Qt.point(0, 0)
    property bool isLoading: false
    
    // Performance settings
    property int maxVisibleSystems: 1000  // Limit visible systems for performance
    
    // Map constants from Python version
    readonly property real lyPerPixel: 40.0
    readonly property real origOffsetX: 1124
    readonly property real origOffsetY: 1749
    readonly property int dotRadius: 5
    
    // System type filters
    property var systemFilters: ({
        "unclaimed": { enabled: false, color: "#4A90E2", count: 0 },
        "yourClaims": { enabled: true, color: "#7ED321", count: 0 },
        "othersClaims": { enabled: true, color: "#F5A623", count: 0 },
        "potentialPOIs": { enabled: true, color: "#F8E71C", count: 0 },
        "POIs": { enabled: true, color: "#FFD700", count: 0 },
        "completed": { enabled: true, color: "#9013FE", count: 0 },
        "commander": { enabled: true, color: "#D0021B", count: 1 }
    })
    
    // Galaxy Map Filter Panel
    Rectangle {
        id: filterPanel
        width: 280
        height: 320
        x: 20
        y: 20
        color: Qt.rgba(0.05, 0.05, 0.1, 0.95)
        radius: 12
        border.width: 2
        border.color: "#FF7F50"
        
        Column {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 8
            
            // Header
            Text {
                text: "🌌 ELITE DANGEROUS GALAXY"
                color: "#FF7F50"
                font.bold: true
                font.pixelSize: 16
            }
            
            Rectangle { width: parent.width; height: 1; color: "#FF7F50"; opacity: 0.3 }
            
            // System Type Filters
            Text {
                text: "System Types:"
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 12
            }
            
            Row {
                spacing: 10
                
                Rectangle {
                    width: 80; height: 25
                    color: "#FF0000"
                    radius: 4
                    border.width: 1
                    border.color: "#FFFFFF"
                    
                    Text {
                        anchors.centerIn: parent
                        text: "Test"
                        color: "#FFFFFF"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
                
                Rectangle {
                    width: 80; height: 25
                    color: "#00FF00"
                    radius: 4
                    border.width: 1
                    border.color: "#FFFFFF"
                    
                    Text {
                        anchors.centerIn: parent
                        text: "Center"
                        color: "#000000"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
                
                Rectangle {
                    width: 80; height: 25
                    color: "#0080FF"
                    radius: 4
                    border.width: 1
                    border.color: "#FFFFFF"
                    
                    Text {
                        anchors.centerIn: parent
                        text: "Corners"
                        color: "#FFFFFF"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
            
            Rectangle { width: parent.width; height: 1; color: "#555555"; opacity: 0.5 }
            
            // Performance Info
            Text {
                text: "Performance:"
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 12
            }
            
            Text {
                text: "FPS: " + (mapCanvas.fps || 0) + " | Dots: " + (mapCanvas.visibleDots || 0)
                color: mapCanvas.fps > 20 ? "#7ED321" : "#D0021B"
                font.pixelSize: 11
                font.bold: true
            }
            
            Text {
                text: "Zoom: " + root.zoomLevel.toFixed(2) + "x"
                color: "#FFFFFF"
                font.pixelSize: 11
            }
            
            Text {
                text: "Background: " + (galaxyBackground.visible ? "Image" : "Generated")
                color: galaxyBackground.visible ? "#7ED321" : "#FFA500"
                font.pixelSize: 11
            }
            
            Text {
                text: "Navigation: Unified Transform System"
                color: "#7ED321"
                font.pixelSize: 10
            }
            
            Rectangle { width: parent.width; height: 1; color: "#555555"; opacity: 0.5 }
            
            // Controls
            Text {
                text: "Controls:"
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 12
            }
            
            Text {
                text: "🖱️ Left drag = Pan"
                color: "#7ED321"
                font.pixelSize: 10
            }
            
            Text {
                text: "🖱️ Mouse wheel = Zoom"
                color: "#7ED321"
                font.pixelSize: 10
            }
            
            Text {
                text: "🖱️ Right click = Reset"
                color: "#7ED321"
                font.pixelSize: 10
            }
            
            Text {
                text: "⌨️ Arrow keys = Pan"
                color: "#7ED321"
                font.pixelSize: 10
            }
            
            Text {
                text: "⌨️ +/- = Zoom | Space = Reset"
                color: "#7ED321"
                font.pixelSize: 10
            }
        }
    }

    
    // Main Map Canvas - High Performance
    Item {
        id: mapContainer
        anchors.fill: parent
        
        // Galaxy background with proper scaling
        Rectangle {
            id: backgroundRect
            anchors.fill: parent
            color: "#000000"  // Fallback black background
            
            // Try to load the galaxy image first (multiple fallback sources)
            Image {
                id: galaxyBackground
                anchors.fill: parent
                source: "qrc:/assets/E47CDFX.png"  // Try resource first
                fillMode: Image.PreserveAspectCrop
                smooth: true
                cache: false
                
                // Try multiple source paths if first fails
                property var imageSources: [
                    "qrc:/assets/E47CDFX.png",
                    "file:///C:/Users/Admin/Downloads/EDRH_redo/EDRH_m/assets/E47CDFX.png",
                    "assets/E47CDFX.png",
                    "file:assets/E47CDFX.png"
                ]
                property int currentSourceIndex: 0
                
                // Transform background with navigation like dots
                transform: [
                    Translate {
                        x: root.panOffset.x
                        y: root.panOffset.y
                    },
                    Scale {
                        xScale: root.zoomLevel
                        yScale: root.zoomLevel
                        origin.x: width / 2
                        origin.y: height / 2
                    }
                ]
                
                // Try multiple sources if image fails to load
                onStatusChanged: {
                    if (status === Image.Error) {
                        currentSourceIndex++
                        if (currentSourceIndex < imageSources.length) {
                            source = imageSources[currentSourceIndex]
                        } else {
                            // All image sources failed, use programmatic background
                            galaxyBackground.visible = false
                            galaxyCanvas.visible = true
                        }
                    } else if (status === Image.Ready) {
                        // Image loaded successfully
                        galaxyCanvas.visible = false
                    }
                }
            }
            
            // Fallback: Programmatic galaxy background
            Canvas {
                id: galaxyCanvas
                anchors.fill: parent
                visible: false  // Hidden by default, shown if image fails
                
                // Transform programmatic background same as image
                transform: [
                    Translate {
                        x: root.panOffset.x
                        y: root.panOffset.y
                    },
                    Scale {
                        xScale: root.zoomLevel
                        yScale: root.zoomLevel
                        origin.x: width / 2
                        origin.y: height / 2
                    }
                ]
                
                // Redraw when visible or window resizes
                property bool needsRedraw: visible
                onNeedsRedrawChanged: if (needsRedraw) requestPaint()
                onWidthChanged: if (visible) requestPaint()
                onHeightChanged: if (visible) requestPaint()
                
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    
                    // Create a radial gradient for galaxy center
                    var gradient = ctx.createRadialGradient(width/2, height/2, 0, width/2, height/2, Math.max(width, height)/2)
                    gradient.addColorStop(0, "#4a5d7a")      // Bright center
                    gradient.addColorStop(0.3, "#2d3547")    // Mid tone
                    gradient.addColorStop(0.7, "#1a1f2e")    // Dark edge
                    gradient.addColorStop(1, "#0a0e1a")      // Space black
                    
                    ctx.fillStyle = gradient
                    ctx.fillRect(0, 0, width, height)
                    
                    // Add nebula clouds with fixed seed for consistency
                    var seed = 12345  // Fixed seed for consistent positioning
                    for (var i = 0; i < 50; i++) {
                        // Simple pseudo-random with fixed seed
                        seed = (seed * 9301 + 49297) % 233280
                        var x = (seed / 233280) * width
                        seed = (seed * 9301 + 49297) % 233280
                        var y = (seed / 233280) * height
                        seed = (seed * 9301 + 49297) % 233280
                        var radius = ((seed / 233280) * 80) + 20
                        
                        var nebulaGradient = ctx.createRadialGradient(x, y, 0, x, y, radius)
                        var colors = ["#4a3d7a", "#7a4d6b", "#6b4d7a", "#3d4a7a"]
                        var colorIndex = Math.floor((seed / 233280) * colors.length)
                        var color = colors[colorIndex]
                        
                        nebulaGradient.addColorStop(0, color + "30")  // Semi-transparent center
                        nebulaGradient.addColorStop(1, color + "00")  // Fully transparent edge
                        
                        ctx.fillStyle = nebulaGradient
                        ctx.fillRect(x - radius, y - radius, radius * 2, radius * 2)
                    }
                    
                    // Add background stars with fixed positions
                    ctx.fillStyle = "#ffffff"
                    seed = 54321  // Different seed for stars
                    for (var j = 0; j < 1000; j++) {
                        seed = (seed * 9301 + 49297) % 233280
                        var starX = (seed / 233280) * width
                        seed = (seed * 9301 + 49297) % 233280
                        var starY = (seed / 233280) * height
                        seed = (seed * 9301 + 49297) % 233280
                        var starSize = ((seed / 233280) * 1.5) + 0.5
                        seed = (seed * 9301 + 49297) % 233280
                        var alpha = ((seed / 233280) * 0.8) + 0.2
                        
                        ctx.globalAlpha = alpha
                        ctx.beginPath()
                        ctx.arc(starX, starY, starSize, 0, Math.PI * 2)
                        ctx.fill()
                    }
                    ctx.globalAlpha = 1.0
                }
            }
            
            // Subtle grid overlay for navigation reference (moves with zoom/pan)
            Canvas {
                id: gridOverlay
                anchors.fill: parent
                opacity: 0.1  // Slightly more visible
                
                // Transform grid same as background and dots
                transform: [
                    Translate {
                        x: root.panOffset.x
                        y: root.panOffset.y
                    },
                    Scale {
                        xScale: root.zoomLevel
                        yScale: root.zoomLevel
                        origin.x: width / 2
                        origin.y: height / 2
                    }
                ]
                
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    
                    // Draw simple grid overlay (background will handle transform)
                    ctx.strokeStyle = "#ffffff"
                    ctx.lineWidth = 1
                    
                    var gridSize = 50  // Screen space grid
                    
                    // Vertical lines
                    for (var x = 0; x < width; x += gridSize) {
                        ctx.beginPath()
                        ctx.moveTo(x, 0)
                        ctx.lineTo(x, height)
                        ctx.stroke()
                    }
                    
                    // Horizontal lines
                    for (var y = 0; y < height; y += gridSize) {
                        ctx.beginPath()
                        ctx.moveTo(0, y)
                        ctx.lineTo(width, y)
                        ctx.stroke()
                    }
                }
                
                // Window resize handling
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
            }
        }
        
        // High-performance dot rendering
        Canvas {
            id: mapCanvas
            anchors.fill: parent
            
            // Transform dots same as background
            transform: [
                Translate {
                    x: root.panOffset.x
                    y: root.panOffset.y
                },
                Scale {
                    xScale: root.zoomLevel
                    yScale: root.zoomLevel
                    origin.x: width / 2
                    origin.y: height / 2
                }
            ]
            
            // Performance metrics
            property int fps: 0
            property int visibleDots: 0
            property int totalDots: 0
            property int activeChunks: 0
            
            // System data
            property var allSystems: []
            
            Timer {
                id: fpsTimer
                interval: 1000
                repeat: true
                running: true
                property int frameCount: 0
                
                onTriggered: {
                    mapCanvas.fps = frameCount
                    frameCount = 0
                }
            }
            
            Timer {
                id: redrawTimer
                interval: 200 // 5 FPS - QML transforms handle smooth navigation
                running: true  // Low rate refresh for animations
                repeat: true
                onTriggered: mapCanvas.requestPaint()
            }
            
            function requestRedraw() {
                // Timer is always running now, so just request paint directly
                requestPaint()
            }
            
            onPaint: {
                fpsTimer.frameCount++
                
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                
                if (root.isLoading) {
                    drawLoadingIndicator(ctx)
                    return
                }
                
                drawSystemsOptimized(ctx)
                drawCommander(ctx)
            }
            
            function drawSystemsOptimized(ctx) {
                // QML handles transforms, so draw in simple screen coordinates
                ctx.fillStyle = "#FFFFFF"
                ctx.strokeStyle = "#000000"
                ctx.lineWidth = 2
                
                var dotCount = 0
                var baseSize = 6
                
                // Use real systems from database if available
                var systems = []
                if (allSystems && allSystems.length > 0) {
                    // Convert real database systems to screen coordinates
                    var centerX = width / 2
                    var centerY = height / 2
                    var scale = 0.2  // Scale factor for galaxy coordinates
                    
                    for (var i = 0; i < allSystems.length; i++) {
                        var dbSystem = allSystems[i]
                        var screenX = centerX + (dbSystem.x || 0) * scale
                        var screenY = centerY + (dbSystem.z || 0) * scale  // Use Z for Y projection
                        
                        systems.push({
                            x: screenX,
                            y: screenY,
                            name: dbSystem.name || "Unknown",
                            category: dbSystem.category || "Unknown",
                            type: "database",
                            claimed: dbSystem.claimed || false,
                            claimedBy: dbSystem.claimedBy || "",
                            distance: dbSystem.distance || "N/A"
                        })
                    }
                    
                    console.log("Drawing", systems.length, "real systems from EDRH database")
                } else {
                    // Fallback: Show minimal test systems if no database data available
                    console.log("No database systems available, showing fallback systems")
                    systems = [
                        {x: width/2, y: height/2, type: "center", name: "Galaxy Center"},
                        {x: width/2 - 100, y: height/2 - 100, type: "test", name: "Test System 1"},
                        {x: width/2 + 100, y: height/2 + 100, type: "test", name: "Test System 2"},
                ]
                }
                
                // Draw all systems
                for (var i = 0; i < systems.length; i++) {
                    var system = systems[i]
                    
                    // Set color based on type/category
                    if (system.type === "center") {
                        ctx.fillStyle = "#00FF00"  // Bright green for center
                        ctx.strokeStyle = "#FFFFFF"
                        ctx.lineWidth = 4
                    } else if (system.type === "database") {
                        // Color by category or claim status
                        if (system.claimed) {
                            ctx.fillStyle = system.claimedBy === (edrhController.commanderName || "") ? "#7ED321" : "#F5A623"  // Green for your claims, orange for others
                        } else if (system.category && system.category.toLowerCase().includes("binary")) {
                            ctx.fillStyle = "#FFFF00"  // Yellow for binary systems
                        } else if (system.category && system.category.toLowerCase().includes("black")) {
                            ctx.fillStyle = "#800080"  // Purple for black holes
                        } else if (system.category && system.category.toLowerCase().includes("neutron")) {
                            ctx.fillStyle = "#00FFFF"  // Cyan for neutron stars
                        } else {
                            ctx.fillStyle = "#FFA500"  // Orange for general systems
                        }
                        ctx.strokeStyle = "#FFFFFF"
                        ctx.lineWidth = 1
                    } else {
                        ctx.fillStyle = "#FF0000"  // Red for test systems
                        ctx.strokeStyle = "#FFFFFF"
                        ctx.lineWidth = 1
                    }
                    
                    // Draw system dot
                    var dotSize = system.type === "center" ? baseSize * 2 : baseSize
                    if (system.claimed) dotSize *= 1.3  // Make claimed systems slightly larger
                    
                    ctx.beginPath()
                    ctx.arc(system.x, system.y, dotSize, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.stroke()
                    dotCount++
                }
                
                // Add header text
                ctx.fillStyle = "#FFFF00"  // Yellow text
                ctx.font = "16px Arial"
                ctx.textAlign = "center"
                if (systems.length > 10) {
                    ctx.fillText("🌌 EDRH DATABASE - " + systems.length + " SYSTEMS", width/2, 30)
                } else {
                    ctx.fillText("🌌 GALAXY CENTER (Waiting for data...)", width/2, 30)
                }
                
                mapCanvas.visibleDots = dotCount
                mapCanvas.activeChunks = 1
            }
            

            
            function worldToScreen(worldX, worldZ) {
                var px = root.origOffsetX + worldX / root.lyPerPixel
                var py = root.origOffsetY - worldZ / root.lyPerPixel
                
                // Apply zoom and pan
                return {
                    x: px * root.zoomLevel + root.panOffset.x,
                    y: py * root.zoomLevel + root.panOffset.y
                }
            }
            

            
            function drawCommander(ctx) {
                // Draw header text
                ctx.fillStyle = "#FFFFFF"
                ctx.font = "bold 20px Arial"
                ctx.textAlign = "center"
                ctx.fillText("🌌 ELITE DANGEROUS GALAXY MAP", width / 2, 40)
                
                ctx.font = "12px Arial"
                ctx.fillStyle = "#7ED321"
                ctx.fillText("Ready for navigation - zoom and pan to explore", width / 2, 65)
                
                // Show current zoom level
                ctx.font = "bold 16px Arial"
                ctx.fillStyle = "#FF7F50"
                ctx.fillText("Zoom: " + root.zoomLevel.toFixed(2) + "x", width / 2, 140)
                
                // Draw a simple pulsing dot as a reference point
                ctx.fillStyle = "#FFFF00"  // Yellow
                ctx.strokeStyle = "#000000"
                ctx.lineWidth = 2
                
                var pulseRadius = (8 + Math.sin(Date.now() / 300) * 3) * root.zoomLevel
                var pulseX = (width / 2 * root.zoomLevel) + root.panOffset.x
                var pulseY = ((height / 2 + 80) * root.zoomLevel) + root.panOffset.y
                
                ctx.beginPath()
                ctx.arc(pulseX, pulseY, Math.max(3, pulseRadius), 0, Math.PI * 2)
                ctx.fill()
                ctx.stroke()
            }
            
            function drawLoadingIndicator(ctx) {
                ctx.fillStyle = "#FF7F50"  // Orange
                ctx.font = "bold 32px Arial"
                ctx.textAlign = "center"
                ctx.fillText("GALAXY MAP LOADING...", width / 2, height / 2)
                
                ctx.fillStyle = "#ffffff"
                ctx.font = "18px Arial"
                ctx.fillText("Please wait while systems load", width / 2, height / 2 + 50)
                
                // Draw spinning loading indicator
                var angle = (Date.now() / 10) % 360
                ctx.strokeStyle = "#FF7F50"
                ctx.lineWidth = 4
                ctx.beginPath()
                ctx.arc(width / 2, height / 2 + 100, 30, 0, angle * Math.PI / 180)
                ctx.stroke()
            }
        }
        
        // Pan and zoom controls
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            
            property point lastPos
            property bool isPanning: false
            
            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    lastPos = Qt.point(mouse.x, mouse.y)
                    isPanning = true
                    cursorShape = Qt.ClosedHandCursor
                    console.log("Started panning from:", mouse.x, mouse.y)
                }
            }
            
            onPositionChanged: function(mouse) {
                if (isPanning && mouse.buttons & Qt.LeftButton) {
                    var delta = Qt.point(mouse.x - lastPos.x, mouse.y - lastPos.y)
                    root.panOffset = Qt.point(
                        root.panOffset.x + delta.x,
                        root.panOffset.y + delta.y
                    )
                    lastPos = Qt.point(mouse.x, mouse.y)
                    console.log("Panning to offset:", root.panOffset.x, root.panOffset.y)
                }
            }
            
            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    isPanning = false
                    cursorShape = Qt.ArrowCursor
                    console.log("Finished panning at offset:", root.panOffset.x, root.panOffset.y)
                }
            }
            
            onWheel: function(wheel) {
                var oldZoom = root.zoomLevel
                var zoomFactor = wheel.angleDelta.y > 0 ? 1.2 : 0.8
                root.zoomLevel = Math.max(0.2, Math.min(root.zoomLevel * zoomFactor, 5.0))
                
                console.log("Zoom changed from", oldZoom.toFixed(2), "to", root.zoomLevel.toFixed(2))
                
                // Simple zoom towards center
                var centerX = width / 2
                var centerY = height / 2
                var zoomChange = root.zoomLevel / oldZoom
                
                root.panOffset = Qt.point(
                    centerX + (root.panOffset.x - centerX) * zoomChange,
                    centerY + (root.panOffset.y - centerY) * zoomChange
                )
            }
            
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    // Reset view
                    root.zoomLevel = 1.0
                    root.panOffset = Qt.point(0, 0)
                    console.log("Reset view to center")
                } else {
                    console.log("Galaxy map clicked at:", mouse.x, mouse.y)
                }
            }
        }
    }
    
    // Keyboard shortcuts
    focus: true
    Keys.onPressed: function(event) {
        switch(event.key) {
            case Qt.Key_Plus:
            case Qt.Key_Equal:
                root.zoomLevel = Math.min(root.zoomLevel * 1.2, 5.0)
                console.log("Keyboard zoom in to", root.zoomLevel.toFixed(2))
                event.accepted = true
                break
            case Qt.Key_Minus:
                root.zoomLevel = Math.max(root.zoomLevel * 0.8, 0.2)
                console.log("Keyboard zoom out to", root.zoomLevel.toFixed(2))
                event.accepted = true
                break
            case Qt.Key_Home:
            case Qt.Key_Space:
                root.zoomLevel = 1.0
                root.panOffset = Qt.point(0, 0)
                console.log("Reset view to center")
                event.accepted = true
                break
            case Qt.Key_Up:
                root.panOffset = Qt.point(root.panOffset.x, root.panOffset.y + 50)
                event.accepted = true
                break
            case Qt.Key_Down:
                root.panOffset = Qt.point(root.panOffset.x, root.panOffset.y - 50)
                event.accepted = true
                break
            case Qt.Key_Left:
                root.panOffset = Qt.point(root.panOffset.x + 50, root.panOffset.y)
                event.accepted = true
                break
            case Qt.Key_Right:
                root.panOffset = Qt.point(root.panOffset.x - 50, root.panOffset.y)
                event.accepted = true
                break
        }
    }
    
    // Performance monitoring
    Timer {
        interval: 10000  // Check every 10 seconds instead of 5
        repeat: true
        running: true
        onTriggered: {
            if (mapCanvas.fps < 15) {  // Only warn if really bad
                console.log("Performance warning: FPS below 15")
                // Could automatically reduce system count
                root.maxVisibleSystems = Math.max(100, root.maxVisibleSystems - 100)
            }
        }
    }
} 