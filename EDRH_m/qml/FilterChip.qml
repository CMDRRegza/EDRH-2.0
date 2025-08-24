import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Modern filter chip with toggle functionality
Rectangle {
    id: root
    
    property string text: "Filter"
    property string filterColor: "#FF7F50"
    property bool enabled: true
    property int count: 0
    
    signal toggled(bool enabled)
    
    height: 36
    radius: 18
    border.width: 2
    border.color: root.enabled ? root.filterColor : "#666666"
    color: root.enabled ? Qt.rgba(root.filterColor.r, root.filterColor.g, root.filterColor.b, 0.2) : "transparent"
    
    // Smooth animations
    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }
    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6
        
        // Color indicator dot
        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: root.filterColor
            opacity: root.enabled ? 1.0 : 0.5
            
            // Subtle glow effect when enabled
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 2
                height: parent.height * 2
                radius: width / 2
                color: parent.color
                opacity: root.enabled ? 0.3 : 0
                
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }
        }
        
        // Filter text
        Text {
            text: root.text
            font.pixelSize: 12
            font.bold: root.enabled
            color: root.enabled ? "#ffffff" : "#888888"
            Layout.fillWidth: true
            elide: Text.ElideRight
            
            Behavior on color { ColorAnimation { duration: 150 } }
        }
        
        // Count badge
        Rectangle {
            visible: root.count > 0
            width: countText.width + 8
            height: 16
            radius: 8
            color: root.enabled ? root.filterColor : "#666666"
            
            Text {
                id: countText
                anchors.centerIn: parent
                text: root.count > 999 ? "999+" : root.count.toString()
                font.pixelSize: 9
                font.bold: true
                color: "#ffffff"
            }
        }
    }
    
    // Click area
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        
        onEntered: {
            root.scale = 1.05
        }
        
        onExited: {
            root.scale = 1.0
        }
        
        onPressed: {
            root.scale = 0.95
        }
        
        onReleased: {
            root.scale = containsMouse ? 1.05 : 1.0
        }
        
        onClicked: {
            root.enabled = !root.enabled
            root.toggled(root.enabled)
            
            // Haptic feedback effect
            clickFeedback.start()
        }
    }
    
    // Visual click feedback
    ParallelAnimation {
        id: clickFeedback
        
        SequentialAnimation {
            NumberAnimation {
                target: root
                property: "opacity"
                to: 0.7
                duration: 50
            }
            NumberAnimation {
                target: root
                property: "opacity"
                to: 1.0
                duration: 100
            }
        }
        
        SequentialAnimation {
            NumberAnimation {
                target: root
                property: "scale"
                to: 1.1
                duration: 50
            }
            NumberAnimation {
                target: root
                property: "scale"
                to: 1.0
                duration: 150
                easing.type: Easing.OutBounce
            }
        }
    }
    
    // Accessibility
    Accessible.role: Accessible.CheckBox
    Accessible.name: root.text
    Accessible.checked: root.enabled
    Accessible.onPressAction: mouseArea.clicked()
} 