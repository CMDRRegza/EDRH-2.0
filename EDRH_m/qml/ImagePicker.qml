import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt.labs.platform 1.1

// **FIX for QWidget crash**: Use QML FileDialog instead of QFileDialog
Item {
    id: root
    
    property string systemName: ""
    property bool dialogVisible: fileDialog.visible
    
    signal imageSelected(string systemName, string fileName)
    signal cancelled()
    
    FileDialog {
        id: fileDialog
        title: systemName ? qsTr("Select Image for %1").arg(systemName) : qsTr("Select Image")
        nameFilters: [
            qsTr("Image files (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"),
            qsTr("All files (*)")
        ]
        defaultSuffix: "jpg"
        fileMode: FileDialog.OpenFile
        
        onAccepted: {
            console.log("ImagePicker: File selected:", file)
            root.imageSelected(root.systemName, file.toString())
        }
        
        onRejected: {
            console.log("ImagePicker: File selection cancelled")
            root.cancelled()
        }
    }
    
    function openForSystem(systemName) {
        console.log("ImagePicker: Opening file dialog for system:", systemName)
        root.systemName = systemName
        fileDialog.open()
    }
    
    function close() {
        fileDialog.close()
    }
}
