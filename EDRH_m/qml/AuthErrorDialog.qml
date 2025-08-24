import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: authDialog
    
    property string errorMessage: ""
    
    signal closed()
    
    title: "Authentication Failed"
    modal: true
    width: 400
    height: 200
    
    background: Rectangle {
        color: "#141414"
        border.color: "#E74C3C"
        border.width: 2
        radius: 8
    }
    
    header: Rectangle {
        color: "#E74C3C"
        height: 50
        radius: 8
        
        Text {
            anchors.centerIn: parent
            text: "❌ Authentication Failed"
            color: "white"
            font.pixelSize: 16
            font.bold: true
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20
        
        Text {
            Layout.fillWidth: true
            text: authDialog.errorMessage
            color: "#FFFFFF"
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
        
        Item { Layout.fillHeight: true }
        
        Button {
            Layout.alignment: Qt.AlignHCenter
            text: "Exit Application"
            
            background: Rectangle {
                color: "#E74C3C"
                radius: 4
                border.color: "#C0392B"
                border.width: 1
            }
            
            contentItem: Text {
                text: parent.text
                color: "white"
                font.pixelSize: 14
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            
            onClicked: {
                authDialog.close()
                authDialog.closed()
            }
        }
    }
} 