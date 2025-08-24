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
        anchors.margins: 25
        spacing: 15
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Text {
                text: "⚡"
                font.pixelSize: 10
                color: Theme.successColor
            }
            
            Text {
                text: "QUICK ACTIONS"
                font.pixelSize: 11
                font.bold: true
                color: Theme.textMuted
            }
        }
        
        // Action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            
            Button {
                id: viewSystemBtn
                text: "View System"
                Layout.preferredWidth: 145
                Layout.preferredHeight: 40
                enabled: false
                
                background: Rectangle {
                    color: parent.pressed ? Theme.tertiaryBgColor : Theme.secondaryBgColor
                    border.color: Theme.borderColor
                    border.width: 1
                    radius: 10
                }
                
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 13
                    font.bold: true
                    color: Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                // Temporarily disabled
            }
            
            // Temporarily remove POI action
        }
    }
} 