pragma Singleton
import QtQuick 2.15

QtObject {
    // Main theme colors matching the Python version
    readonly property color mainBgColor: "#0a0a0a"
    readonly property color cardBgColor: "#141414"
    readonly property color secondaryBgColor: "#1f1f1f"
    readonly property color tertiaryBgColor: "#2a2a2a"
    readonly property color borderColor: "#333333"
    
    // Accent colors
    readonly property color accentColor: "#FF7F50"
    readonly property color accentHover: "#FF9068"
    readonly property color successColor: "#4ECDC4"
    readonly property color dangerColor: "#E74C3C"
    readonly property color warningColor: "#F39C12"
    readonly property color infoColor: "#3498DB"
    
    // Text colors
    readonly property color textColor: "#FFFFFF"
    readonly property color textSecondary: "#B0B0B0" 
    readonly property color textMuted: "#808080"
    readonly property color textDark: "#333333"
    
    // Interactive states
    readonly property color hoverColor: "#2a2a2a"
    readonly property color pressedColor: "#3a3a3a"
    readonly property color disabledColor: "#555555"
} 