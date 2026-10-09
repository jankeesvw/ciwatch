import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1440
    height: 900
    minimumWidth: 900
    minimumHeight: 520
    visible: true
    title: "CI Watch · " + backend.repo
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent

    readonly property color panel: "#151518"
    readonly property color line: "#26262c"
    readonly property color fg: "#e6e6e6"
    readonly property color dim: "#8a8a94"
    readonly property string mono: "monospace"

    property bool helpVisible: false
    property real now: Date.now()

    Timer { interval: 1000; running: true; repeat: true; onTriggered: win.now = Date.now() }

    function seconds(from, to) {
        if (!from) return 0
        const end = to ? new Date(to).getTime() : win.now
        return Math.max(0, Math.round((end - new Date(from).getTime()) / 1000))
    }
    function duration(s) {
        if (s < 60) return s + "s"
        if (s < 3600) return Math.floor(s / 60) + "m " + (s % 60) + "s"
        return Math.floor(s / 3600) + "h " + Math.floor(s % 3600 / 60) + "m"
    }
    function ago(iso) {
        const s = seconds(iso)
        if (s < 60) return "now"
        if (s < 3600) return Math.floor(s / 60) + "m ago"
        if (s < 86400) return Math.floor(s / 3600) + "h ago"
        return Math.floor(s / 86400) + "d ago"
    }
    function runDuration(run) {
        const active = run.state === "running" || run.state === "queued"
        return duration(seconds(run.startedAt, active ? null : run.updatedAt))
    }

    function scrollLogBy(pages) {
        logView.scripted = true
        logView.contentY = Math.max(logView.originY, Math.min(logView.contentY + pages * logView.height * 0.9,
                                    logView.originY + logView.contentHeight - logView.height))
        logView.scripted = false
        logView.follow = logView.nearEnd
    }
    function jumpToError(index) {
        if (index < 0) return
        logView.follow = false
        logView.highlighted = index
        logView.scripted = true
        logView.positionViewAtIndex(index, ListView.Center)
        logView.scripted = false
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        onActivated: win.helpVisible ? win.helpVisible = false : backend.clearCommit()
    }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequences: ["Space", "R", "Ctrl+R"]; context: Qt.ApplicationShortcut; onActivated: backend.refresh() }
    Shortcut { sequences: ["J", "Down"]; context: Qt.ApplicationShortcut; onActivated: backend.moveRun(1) }
    Shortcut { sequences: ["K", "Up"]; context: Qt.ApplicationShortcut; onActivated: backend.moveRun(-1) }
    Shortcut { sequences: ["L", "Right", "Tab"]; context: Qt.ApplicationShortcut; onActivated: backend.moveJob(1) }
    Shortcut { sequences: ["H", "Left", "Shift+Tab"]; context: Qt.ApplicationShortcut; onActivated: backend.moveJob(-1) }
    Shortcut { sequence: "F"; context: Qt.ApplicationShortcut; onActivated: backend.cycleFilter() }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; onActivated: backend.toggleSkipped() }
    Shortcut { sequence: "E"; context: Qt.ApplicationShortcut; onActivated: win.jumpToError(backend.log.nextError(logView.highlighted)) }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; onActivated: { backend.copyLog(); toast.show("Log copied") } }
    Shortcut { sequence: "PgDown"; context: Qt.ApplicationShortcut; onActivated: win.scrollLogBy(1) }
    Shortcut { sequence: "PgUp"; context: Qt.ApplicationShortcut; onActivated: win.scrollLogBy(-1) }
    Shortcut { sequence: "Home"; context: Qt.ApplicationShortcut; onActivated: { logView.follow = false; logView.positionViewAtBeginning() } }
    Shortcut { sequence: "End"; context: Qt.ApplicationShortcut; onActivated: { logView.follow = true; logView.toEnd(true) } }
    Shortcut {
        sequences: ["O", "Return"]
        context: Qt.ApplicationShortcut
        onActivated: Qt.openUrlExternally(backend.selectedJob.url || backend.selectedRun.url)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header: which repository, how much is moving, how fresh the data is.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: win.panel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 14

                Text { text: backend.repo; textFormat: Text.PlainText; color: win.fg; font.pixelSize: 16; font.bold: true }
                Rectangle {
                    radius: 10
                    color: "transparent"
                    border.color: backend.filter || backend.commit ? theme.accent : win.line
                    implicitWidth: filterLabel.implicitWidth + 20
                    implicitHeight: 24
                    Text {
                        id: filterLabel
                        anchors.centerIn: parent
                        text: (backend.commit ? "Commit " + backend.commit.substring(0, 7) + "  ·  Esc for all runs"
                                              : backend.filter || "All workflows")
                              + (backend.showSkipped ? " · with skipped" : "")
                        color: backend.filter || backend.commit ? theme.accent : win.dim
                        font.pixelSize: 12
                    }
                }
                Row {
                    spacing: 6
                    visible: backend.activeCount > 0
                    StatusIcon { state: "running"; size: 10; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: backend.activeCount + " running"; color: theme.accent; font.pixelSize: 13 }
                }
                Item { Layout.fillWidth: true }
                BusyIndicator { running: backend.busy; implicitWidth: 22; implicitHeight: 22; opacity: running ? 1 : 0 }
                Text {
                    color: win.dim
                    font.pixelSize: 12
                    text: {
                        win.now
                        if (isNaN(backend.lastUpdated.getTime())) return "Loading…"
                        const s = Math.round((win.now - backend.lastUpdated.getTime()) / 1000)
                        return "Updated " + (s < 2 ? "just now" : s + "s ago")
                    }
                }
                Text { text: "? shortcuts"; color: win.dim; font.pixelSize: 12 }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: win.line }
        }

        // What went wrong, said plainly, instead of an empty window.
        Rectangle {
            Layout.fillWidth: true
            visible: backend.error !== ""
            implicitHeight: errorText.implicitHeight + 20
            color: "#3a1418"
            Text {
                id: errorText
                anchors.fill: parent
                anchors.margins: 10
                anchors.leftMargin: 18
                text: backend.error
                textFormat: Text.PlainText
                color: "#fecaca"
                wrapMode: Text.Wrap
                font.pixelSize: 13
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Runs, newest first.
            ListView {
                id: runList
                Layout.preferredWidth: Math.max(260, Math.min(320, win.width * 0.24))
                Layout.fillHeight: true
                clip: true
                model: backend.runs
                currentIndex: backend.selectedRunIndex
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    readonly property bool current: modelData.id === backend.selectedRunId
                    width: ListView.view.width
                    height: 64
                    color: current ? "#1f1f25" : (mouse.containsMouse ? "#18181c" : "transparent")

                    Rectangle { width: 3; height: parent.height; color: theme.accent; visible: parent.current }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 14
                        spacing: 12

                        StatusIcon { state: modelData.state; size: 16 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                Layout.fillWidth: true
                                text: modelData.title
                                textFormat: Text.PlainText
                                color: win.fg
                                font.pixelSize: 14
                                font.bold: current
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.workflow + " · " + modelData.branch
                                textFormat: Text.PlainText
                                color: win.dim
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                            }
                        }
                        ColumnLayout {
                            spacing: 3
                            Text { Layout.alignment: Qt.AlignRight; text: win.ago(modelData.startedAt); color: win.dim; font.pixelSize: 12 }
                            Text {
                                Layout.alignment: Qt.AlignRight
                                text: win.runDuration(modelData)
                                color: modelData.state === "running" ? theme.accent : win.dim
                                font.pixelSize: 12
                                font.family: win.mono
                            }
                        }
                    }
                    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: backend.selectRun(modelData.id) }
                }

                Text {
                    anchors.centerIn: parent
                    visible: runList.count === 0 && !isNaN(backend.lastUpdated.getTime())
                    text: "No runs match this filter."
                    color: win.dim
                }
            }

            Rectangle { Layout.fillHeight: true; width: 1; color: win.line }

            // The selected run: its jobs and steps beside the log of one job.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                visible: backend.selectedRunId !== 0

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: 18
                    Layout.bottomMargin: 14
                    spacing: 6
                    RowLayout {
                        spacing: 10
                        Layout.fillWidth: true
                        StatusIcon { state: backend.selectedRun.state || "queued"; size: 20 }
                        Text {
                            Layout.fillWidth: true
                            text: backend.selectedRun.title || ""
                            textFormat: Text.PlainText
                            color: win.fg
                            font.pixelSize: 19
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        color: win.dim
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        text: {
                            const r = backend.selectedRun
                            if (!r.id) return ""
                            return [r.workflow + " #" + r.number + (r.attempt > 1 ? " (attempt " + r.attempt + ")" : ""),
                                    r.branch, r.sha, r.event, r.actor,
                                    win.runDuration(r)].join("  ·  ")
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: win.line }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    Flickable {
                        Layout.preferredWidth: 220
                        Layout.fillHeight: true
                        contentHeight: jobColumn.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}

                        Column {
                            id: jobColumn
                            width: parent.width
                            topPadding: 8
                            bottomPadding: 8

                            Repeater {
                                model: backend.jobs
                                delegate: Column {
                                    required property var modelData
                                    readonly property bool current: modelData.id === backend.selectedJobId
                                    width: jobColumn.width

                                    Rectangle {
                                        width: parent.width
                                        height: 38
                                        color: current ? "#1f1f25" : (jobMouse.containsMouse ? "#18181c" : "transparent")
                                        Rectangle { width: 3; height: parent.height; color: theme.accent; visible: current }
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 16
                                            anchors.rightMargin: 12
                                            spacing: 10
                                            StatusIcon { state: modelData.state; size: 14 }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                textFormat: Text.PlainText
                                                color: modelData.state === "skipped" ? win.dim : win.fg
                                                font.pixelSize: 14
                                                font.bold: current
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                visible: modelData.state !== "skipped" && modelData.state !== "queued"
                                                text: win.duration(win.seconds(modelData.startedAt,
                                                        modelData.state === "running" ? null : modelData.completedAt))
                                                color: modelData.state === "running" ? theme.accent : win.dim
                                                font.pixelSize: 12
                                                font.family: win.mono
                                            }
                                        }
                                        MouseArea { id: jobMouse; anchors.fill: parent; hoverEnabled: true; onClicked: backend.selectJob(modelData.id) }
                                    }

                                    Repeater {
                                        model: current ? modelData.steps : []
                                        delegate: RowLayout {
                                            required property var modelData
                                            x: 42
                                            width: jobColumn.width - 54
                                            height: 26
                                            spacing: 9
                                            StatusIcon { state: modelData.state; size: 10 }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                textFormat: Text.PlainText
                                                color: modelData.state === "running" ? theme.accent
                                                     : modelData.state === "failure" ? "#f87171"
                                                     : modelData.state === "skipped" ? "#5a5a62" : "#b4b4bc"
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillHeight: true; width: 1; color: win.line }

                    // The log, which follows the end while the job runs.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0a0a0c"

                        ListView {
                            id: logView
                            // Following means staying at the bottom as lines arrive.
                            // Only a scroll the user makes decides it, by wheel, drag
                            // or scrollbar: scrolled to the end follows, scrolled up
                            // stops. The view also moves by itself while it lays out
                            // new lines, and that must not count as scrolling away.
                            property bool follow: true
                            property bool scripted: false
                            property int highlighted: -1

                            // Glides to the end rather than jumping, so new output
                            // scrolls into view the way it would in a terminal. A
                            // long way (a reload, a different job) goes at once.
                            function toEnd(animated) {
                                scripted = true
                                const from = contentY
                                positionViewAtEnd()
                                const to = contentY
                                scripted = false
                                if (!animated || to <= from || to - from > height * 2) {
                                    followAnimation.stop()
                                    return
                                }
                                scripted = true
                                contentY = from
                                followAnimation.from = from
                                followAnimation.to = to
                                followAnimation.restart()
                            }

                            NumberAnimation {
                                id: followAnimation
                                target: logView
                                property: "contentY"
                                duration: 450
                                easing.type: Easing.OutCubic
                                onRunningChanged: logView.scripted = running
                            }
                            onMovementStarted: followAnimation.stop()

                            // New lines slide in from the right and fade up, and
                            // the lines above them make room smoothly. A reload
                            // or a different job appears at once.
                            add: Transition {
                                ParallelAnimation {
                                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 380; easing.type: Easing.OutQuad }
                                    NumberAnimation { property: "x"; from: 28; to: 0; duration: 420; easing.type: Easing.OutCubic }
                                }
                            }
                            addDisplaced: Transition {
                                NumberAnimation { property: "y"; duration: 300; easing.type: Easing.OutCubic }
                            }
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true
                            model: backend.log
                            boundsBehavior: Flickable.StopAtBounds
                            reuseItems: true
                            ScrollBar.vertical: ScrollBar { id: logBar }
                            ScrollBar.horizontal: ScrollBar {}
                            flickableDirection: Flickable.AutoFlickIfNeeded
                            contentWidth: Math.max(width, 2400)

                            readonly property bool nearEnd: contentY + height >= originY + contentHeight - 24
                            onContentYChanged: if (!scripted && (moving || logBar.pressed)) follow = nearEnd
                            onCountChanged: if (follow) Qt.callLater(toEnd, true)

                            delegate: Rectangle {
                                required property int index
                                required property string time
                                required property string text
                                required property int kind
                                width: logView.contentWidth
                                height: 19
                                color: index === logView.highlighted ? "#3a1418"
                                     : kind === 2 ? "#24111a" : "transparent"
                                Text {
                                    x: 10
                                    width: 64
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: time
                                    color: "#4a4a52"
                                    font.family: win.mono
                                    font.pixelSize: 12
                                }
                                Text {
                                    x: 82
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: (kind === 1 ? "▸ " : kind === 2 ? "Error: " : kind === 3 ? "Warning: " : "") + parent.text
                                    textFormat: Text.PlainText
                                    color: [win.fg, theme.accent, "#f87171", "#fbbf24", "#7dd3fc", "#a5b4fc"][kind]
                                    font.family: win.mono
                                    font.pixelSize: 12
                                    font.bold: kind === 1 || kind === 2
                                }
                            }
                        }

                        Connections {
                            target: backend.log
                            function onModelAboutToBeReset() { logView.scripted = true }
                            function onModelReset() {
                                logView.scripted = false
                                if (logView.follow) Qt.callLater(logView.toEnd)
                            }
                        }

                        Connections {
                            target: backend
                            function onLogReset() {
                                logView.highlighted = -1
                                const job = backend.selectedJob
                                if (job.state === "failure" && backend.log.firstError >= 0) {
                                    win.jumpToError(backend.log.firstError)
                                } else {
                                    logView.follow = true
                                    Qt.callLater(logView.toEnd)
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            width: parent.width * 0.7
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            visible: backend.logStatus !== "" && logView.count === 0
                            text: backend.logStatus
                            textFormat: Text.PlainText
                            color: win.dim
                            font.pixelSize: 14
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 16
                            readonly property bool shown: !logView.follow && backend.selectedJob.state === "running"
                            opacity: shown ? 1 : 0
                            visible: opacity > 0
                            anchors.bottomMargin: shown ? 16 : -8
                            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }
                            Behavior on anchors.bottomMargin { NumberAnimation { duration: 320; easing.type: Easing.OutBack } }
                            radius: 6
                            color: theme.accent
                            implicitWidth: followText.implicitWidth + 20
                            implicitHeight: 28
                            Text { id: followText; anchors.centerIn: parent; text: "End  follow the log"; color: theme.accentForeground; font.pixelSize: 12 }
                        }
                    }
                }
            }
        }
    }

    // A short confirmation for actions with no visible result, like copying.
    Rectangle {
        id: toast
        function show(message) { toastText.text = message; opacity = 1; toastTimer.restart() }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 30
        radius: 8
        color: theme.accent
        opacity: 0
        implicitWidth: toastText.implicitWidth + 28
        implicitHeight: 34
        Behavior on opacity { NumberAnimation { duration: 150 } }
        Text { id: toastText; anchors.centerIn: parent; color: theme.accentForeground; font.pixelSize: 13; font.bold: true }
        Timer { id: toastTimer; interval: 1400; onTriggered: toast.opacity = 0 }
    }

    // Every shortcut, on ?.
    Rectangle {
        anchors.fill: parent
        color: "#cc000000"
        visible: win.helpVisible
        MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

        Rectangle {
            anchors.centerIn: parent
            width: 460
            height: helpGrid.implicitHeight + 70
            radius: 12
            color: win.panel
            border.color: win.line

            Text { x: 28; y: 22; text: "Shortcuts"; color: win.fg; font.pixelSize: 17; font.bold: true }
            Column {
                id: helpGrid
                x: 28
                y: 56
                spacing: 7
                Repeater {
                    model: [
                        ["J / ↓", "Next run"], ["K / ↑", "Previous run"],
                        ["L / → / Tab", "Next job"], ["H / ← / Shift+Tab", "Previous job"],
                        ["E", "Next error in the log"], ["PgUp / PgDn", "Scroll the log"],
                        ["Home / End", "Top of the log / follow the end"], ["C", "Copy the log"],
                        ["O / Enter", "Open the job on GitHub"], ["F", "Cycle the workflow filter"],
                        ["S", "Show or hide skipped runs"], ["Esc", "Back to all runs from one commit"],
                        ["Space / R / Ctrl+R", "Refresh now"],
                        ["?", "Show or hide this help"], ["Q", "Quit"]
                    ]
                    delegate: Row {
                        required property var modelData
                        Text { width: 170; text: modelData[0]; color: theme.accent; font.family: win.mono; font.pixelSize: 13 }
                        Text { text: modelData[1]; color: win.fg; font.pixelSize: 13 }
                    }
                }
            }
        }
    }
}
