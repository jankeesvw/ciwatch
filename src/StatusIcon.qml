import QtQuick

// A status dot for a run, job, or step. Running ones pulse in the theme
// accent, so a glance at the window says whether anything is still moving.
Item {
    id: icon
    property string state: "queued"
    property int size: 16

    implicitWidth: size
    implicitHeight: size

    readonly property color color: ({
        "success": "#4ade80",
        "failure": "#f87171",
        "running": theme.accent,
        "queued": "#8a8a94",
        "cancelled": "#8a8a94",
        "skipped": "#4a4a52",
        "neutral": "#8a8a94"
    })[state] || "#8a8a94"

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: icon.state === "queued" || icon.state === "skipped" ? "transparent" : icon.color
        border.width: icon.state === "queued" || icon.state === "skipped" ? Math.max(1.5, icon.size / 9) : 0
        border.color: icon.color

        SequentialAnimation on opacity {
            running: icon.state === "running"
            loops: Animation.Infinite
            onRunningChanged: if (!running) parent.opacity = 1
            NumberAnimation { to: 0.35; duration: 700; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutSine }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: text !== ""
        text: ({ "success": "✓", "failure": "✕", "cancelled": "–", "neutral": "–" })[icon.state] || ""
        color: "#0e0e10"
        font.pixelSize: icon.size * 0.7
        font.bold: true
    }
}
