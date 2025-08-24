import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Window {
    id: galaxyMapWindow
    
    title: "EDRH - Galaxy Map - TESTING VERSION"
    width: 1200
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    
    // Center on screen
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    
    color: "#000000"  // Pure black
    
    // High-Performance Galaxy Map Content
    HighPerformanceGalaxyMapTab {
        anchors.fill: parent
        
        Component.onCompleted: {
            console.log("Galaxy Map Window - HIGH-PERFORMANCE GalaxyMapTab loaded successfully!")
            console.log("Window size:", galaxyMapWindow.width, "x", galaxyMapWindow.height)
            // Data is already loaded via the normal system signals, no need to reload
        }
    }
    
    // Close with Escape key
    Shortcut {
        sequence: "Escape"
        onActivated: galaxyMapWindow.close()
    }
    
    // Handle window close
    onClosing: {
        // Any cleanup if needed
        console.log("Galaxy map window closing")
    }
} 