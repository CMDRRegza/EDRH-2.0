import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    
    property string title: ""
    property string value: ""
    property string icon: ""
    property color accentColor: "#FF7F50"
    
    color: Theme.secondaryBgColor
    radius: 12
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18  // Increased from 15 to 18
        spacing: 8  // Increased from 5 to 8
        
        // Title row with icon
        RowLayout {
            Layout.fillWidth: true
            spacing: 8  // Increased from 5 to 8
            
            Text {
                text: root.icon
                font.pixelSize: 18  // Increased from 16 to 18
                color: root.accentColor
                visible: root.icon !== ""
            }
            
            Text {
                text: root.title
                font.pixelSize: 13  // Increased from 12 to 13
                font.bold: true  // Made bold for better visibility
                color: Theme.textMuted
                Layout.fillWidth: true
                wrapMode: Text.WordWrap  // Added word wrap for longer titles
            }
        }
        
        // Value
        Text {
            text: root.value
            font.pixelSize: 22  // Increased from 20 to 22
            font.bold: true
            color: Theme.textColor
            Layout.fillWidth: true
            Layout.fillHeight: true
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter  // Center the value text
            elide: Text.ElideRight
            wrapMode: Text.WordWrap  // Added word wrap for longer values
        }
    }
} 