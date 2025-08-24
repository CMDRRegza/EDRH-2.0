import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Modern floating panel with glassmorphism effect
Rectangle {
    id: root
    
    property string title: "Panel"
    property string icon: "📋"
    property alias content: contentLoader.sourceComponent
    property bool collapsed: false
    property bool draggable: true
    
    // Glassmorphism styling
    color: Qt.rgba(0.1, 0.1, 0.1, 0.8)
    radius: 15
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.2)
    
    // Smooth show/hide animation
    opacity: 1.0
    scale: 1.0
    
    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    
    // Simple shadow effect (Qt 6 compatible)
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 8
        anchors.leftMargin: 4
        color: Qt.rgba(0, 0, 0, 0.3)
        radius: parent.radius
        z: -1
    }
    
    // Header bar
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 45
        color: Qt.rgba(1, 1, 1, 0.1)
        radius: parent.radius
        
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.radius
            color: parent.color
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10
            
            Text {
                text: root.icon
                font.pixelSize: 16
                color: "#ffffff"
            }
            
            Text {
                text: root.title
                font.pixelSize: 14
                font.bold: true
                color: "#ffffff"
                Layout.fillWidth: true
            }
            
            // Collapse button
            Button {
                width: 24
                height: 24
                text: root.collapsed ? "+" : "−"
                
                background: Rectangle {
                    color: parent.pressed ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                    radius: 12
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.2)
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 12
                    font.bold: true
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    root.collapsed = !root.collapsed
                    if (root.collapsed) {
                        root.height = header.height
                    } else {
                        root.height = Math.max(200, contentLoader.implicitHeight + header.height + 20)
                    }
                }
            }
            
            // Close button
            Button {
                width: 24
                height: 24
                text: "×"
                
                background: Rectangle {
                    color: parent.pressed ? Qt.rgba(1, 0.2, 0.2, 0.8) : Qt.rgba(1, 1, 1, 0.1)
                    radius: 12
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.2)
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 14
                    font.bold: true
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    root.opacity = 0
                    root.scale = 0.8
                    hideTimer.start()
                }
                
                Timer {
                    id: hideTimer
                    interval: 200
                    onTriggered: root.visible = false
                }
            }
        }
        
        // Drag area for panel movement
        MouseArea {
            anchors.fill: parent
            enabled: root.draggable
            drag.target: root
            drag.threshold: 5
            cursorShape: Qt.SizeAllCursor
            
            onPressed: {
                root.z = 1000 // Bring to front when dragging
            }
        }
    }
    
    // Content area
    Loader {
        id: contentLoader
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        
        visible: !root.collapsed
        opacity: root.collapsed ? 0 : 1
        
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }
    
    // Resize handle (bottom-right corner)
    Rectangle {
        id: resizeHandle
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 20
        height: 20
        color: "transparent"
        
        // Visual indicator
        Canvas {
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.3)
                ctx.lineWidth = 1
                
                for (var i = 1; i <= 3; i++) {
                    ctx.beginPath()
                    ctx.moveTo(width - i * 4, height)
                    ctx.lineTo(width, height - i * 4)
                    ctx.stroke()
                }
            }
        }
        
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            
            property point startPos
            property size startSize
            
            onPressed: function(mouse) {
                startPos = Qt.point(mouse.x, mouse.y)
                startSize = Qt.size(root.width, root.height)
            }
            
            onPositionChanged: function(mouse) {
                if (pressed) {
                    var deltaX = mouse.x - startPos.x
                    var deltaY = mouse.y - startPos.y
                    
                    root.width = Math.max(200, startSize.width + deltaX)
                    root.height = Math.max(150, startSize.height + deltaY)
                }
            }
        }
    }
    
    // Auto-hide when not in use (optional)
    property bool autoHide: false
    property int autoHideDelay: 3000
    
    Timer {
        id: autoHideTimer
        interval: root.autoHideDelay
        running: root.autoHide && !hoverArea.containsMouse
        onTriggered: {
            if (!hoverArea.containsMouse) {
                root.opacity = 0.3
            }
        }
    }
    
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        
        onEntered: {
            if (root.autoHide) {
                root.opacity = 1.0
                autoHideTimer.stop()
            }
        }
        
        onExited: {
            if (root.autoHide) {
                autoHideTimer.restart()
            }
        }
    }
} 