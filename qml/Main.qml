pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    width: 860
    height: 190
    minimumWidth: 800
    minimumHeight: 186
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    title: qsTr("Motion Bridge")
    font.family: "Segoe UI Variable"

    property bool connectionExpanded: false
    property bool tuningExpanded: false
    readonly property bool darkTheme: companion.theme !== "light"
    readonly property bool expanded: connectionExpanded || tuningExpanded
    readonly property color surface: darkTheme ? "#0C111A" : "#F3F6FA"
    readonly property color titleSurface: darkTheme ? "#0A0F17" : "#FFFFFF"
    readonly property color headerSurface: darkTheme ? "#0D131D" : "#F8FAFD"
    readonly property color footerSurface: darkTheme ? "#0A1018" : "#EEF3F8"
    readonly property color panel: darkTheme ? "#111824" : "#FFFFFF"
    readonly property color panelAlt: darkTheme ? "#121A26" : "#F7F9FC"
    readonly property color fieldSurface: darkTheme ? "#0C121B" : "#F2F5F9"
    readonly property color hoverSurface: darkTheme ? "#1B2635" : "#E8EEF5"
    readonly property color activeSurface: darkTheme ? "#18243A" : "#E5EBFF"
    readonly property color outline: darkTheme ? "#202B3B" : "#D5DEE9"
    readonly property color textPrimary: darkTheme ? "#EAF0F8" : "#182334"
    readonly property color textSecondary: darkTheme ? "#98A6B9" : "#536276"
    readonly property color textMuted: darkTheme ? "#637189" : "#7A889A"
    readonly property color primary: "#667DFF"
    readonly property color cyan: "#61DFFF"

    function keepTitleBarVisible(targetWindow, titleBarHeight) {
        if (!targetWindow.screen) return
        const area = targetWindow.screen.availableGeometry
        if (!area || area.width <= 0 || area.height <= 0) return
        // Keep enough horizontal title-bar area available for dragging while
        // allowing most of a wide window to remain on the user's chosen side.
        const horizontalGrip = Math.min(120, targetWindow.width)
        const minimumX = area.x - targetWindow.width + horizontalGrip
        const maximumX = area.x + area.width - horizontalGrip
        const maximumY = area.y + area.height - titleBarHeight
        targetWindow.x = Math.max(minimumX, Math.min(targetWindow.x, maximumX))
        targetWindow.y = Math.max(area.y, Math.min(targetWindow.y, maximumY))
    }

    function resizeForContent() {
        if (visibility === Window.Maximized) return
        const titleAnchorX = x
        const titleAnchorY = y
        const targetWidth = tuningExpanded ? 1160
                          : connectionExpanded ? 960
                          : 860
        // These workspaces have fixed layouts. Using their asynchronous
        // implicitHeight here can capture an intermediate value immediately
        // after visibility changes and leave the panel clipped. Keep explicit
        // complete heights in sync with the layouts below.
        const targetHeight = connectionExpanded && tuningExpanded ? 1020
                           : connectionExpanded ? 540
                           : tuningExpanded ? 700
                           : 190
        const targetMinimumWidth = tuningExpanded ? 980
                                 : connectionExpanded ? 860
                                 : 800
        const area = screen ? screen.availableGeometry : null
        const fittedWidth = area && area.width > 0 ? Math.min(targetWidth, Math.max(800, area.width - 12)) : targetWidth
        const fittedHeight = area && area.height > 0 ? Math.min(targetHeight, Math.max(186, area.height - 12)) : targetHeight
        // The expanded workspace is scrollable, so let the outer window fit
        // the usable desktop at high DPI instead of extending under the
        // taskbar. This is especially relevant at Windows 150% scaling.
        minimumWidth = Math.min(targetMinimumWidth, fittedWidth)
        minimumHeight = expanded ? fittedHeight : 186
        width = fittedWidth
        height = fittedHeight
        // Keep the custom title bar fixed and grow the workspaces to the right
        // and downward. Re-centering here can push the only drag surface above
        // the desktop when the compact window is close to the top edge.
        x = titleAnchorX
        y = titleAnchorY
        keepTitleBarVisible(window, 46)
        // Some Windows window-manager paths adjust a frameless window again
        // after its resize event. Restore the title-bar anchor once more after
        // that event has been delivered.
        Qt.callLater(function() {
            window.x = titleAnchorX
            window.y = titleAnchorY
            keepTitleBarVisible(window, 46)
        })
    }

    function toggleConnection() {
        if (!connectionExpanded) companion.refresh_usb_ports()
        connectionExpanded = !connectionExpanded
        Qt.callLater(resizeForContent)
    }

    function togglePreview() {
        if (previewWindow.visible) {
            previewWindow.hide()
        } else {
            placePreviewWindow()
            previewWindow.show()
            Qt.callLater(function() {
                keepPreviewWindowOnScreen()
                previewWindow.raise()
                previewWindow.requestActivate()
            })
        }
    }

    function previewAvailableArea() {
        // The preview has a fixed home: the primary display's usable area.
        // Its placement deliberately does not depend on the main window's
        // position or on the monitor where that window happens to be placed.
        const area = companion.primary_screen_available_geometry()
        if (area && area.width > 0 && area.height > 0) return area
        return previewWindow.screen ? previewWindow.screen.availableGeometry : null
    }

    function keepPreviewWindowOnScreen() {
        const area = previewAvailableArea()
        if (!area || area.width <= 0 || area.height <= 0) return
        fitPreviewToAvailableArea(area)
        const edgePadding = 16
        const minimumX = area.x + edgePadding
        const minimumY = area.y + edgePadding
        const maximumX = Math.max(minimumX, area.x + area.width - previewWindow.width - edgePadding)
        const maximumY = Math.max(minimumY, area.y + area.height - previewWindow.height - edgePadding)
        previewWindow.x = Math.max(minimumX, Math.min(previewWindow.x, maximumX))
        previewWindow.y = Math.max(minimumY, Math.min(previewWindow.y, maximumY))
    }

    function fitPreviewToAvailableArea(area) {
        const edgePadding = 16
        const maximumWidth = Math.max(previewWindow.minimumWidth, area.width - edgePadding * 2)
        const maximumHeight = Math.max(previewWindow.minimumHeight, area.height - edgePadding * 2)
        // On a scaled or short display, the original 500 px preview can be
        // taller than the usable desktop. Reduce it before centering so the
        // entire frameless window, including its drag bar, remains visible.
        if (previewWindow.width > maximumWidth)
            previewWindow.width = maximumWidth
        if (previewWindow.height > maximumHeight)
            previewWindow.height = maximumHeight
    }

    function placePreviewWindow() {
        const area = previewAvailableArea()
        if (!area) return
        // A predictable centered placement is easier to find than following
        // a main window that may be parked at any desktop edge or corner.
        fitPreviewToAvailableArea(area)
        previewWindow.x = area.x + Math.round((area.width - previewWindow.width) / 2)
        previewWindow.y = area.y + Math.round((area.height - previewWindow.height) / 2)
        keepPreviewWindowOnScreen()
    }

    function toggleTuning() {
        tuningExpanded = !tuningExpanded
        Qt.callLater(resizeForContent)
    }

    function motionStateLabel(state) {
        switch (state) {
        // A short source-frame gap enters the engine's safety hold state,
        // while the last valid motion remains in effect. Keep the UI's live
        // indication stable instead of flashing between ACTIVE and IDLE.
        case "active":
        case "holding": return qsTr("ACTIVE")
        case "returning": return qsTr("RELEASING")
        case "acquiring": return qsTr("ACQUIRING")
        case "releasing": return qsTr("RELEASING")
        case "unmapped": return qsTr("UNMAPPED")
        case "fault": return qsTr("FAULT")
        default: return qsTr("IDLE")
        }
    }

    function motionIsLive(state) {
        return state === "active" || state === "holding"
    }

    component WindowButton: Button {
        id: control
        required property string glyph
        property string tipText: ""
        property color hoverColor: window.hoverSurface
        property color glyphColor: window.textSecondary
        property real glyphSize: 13
        width: 42; height: 36
        text: glyph
        hoverEnabled: true
        background: Rectangle {
            radius: 8
            color: control.hovered ? control.hoverColor : "transparent"
            border.width: 0
        }
        contentItem: Label {
            text: control.text
            color: control.hovered && control.hoverColor === "#B84350" ? "white" : control.glyphColor
            font.pixelSize: control.glyphSize
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        AppToolTip {
            visible: control.hovered && control.tipText.length > 0
            text: control.tipText
            darkTheme: window.darkTheme
        }
    }

    component LanguageOption: MenuItem {
        id: languageOption
        required property string languageCode
        required property string optionText
        implicitWidth: 154
        implicitHeight: 38
        text: optionText
        checkable: true
        checked: languageController.language === languageCode
        hoverEnabled: true
        leftPadding: 9
        rightPadding: 9
        onTriggered: languageController.set_language(languageCode)

        indicator: Item { implicitWidth: 0; implicitHeight: 0 }
        arrow: Item { implicitWidth: 0; implicitHeight: 0 }

        background: Rectangle {
            radius: 8
            color: languageOption.highlighted || languageOption.hovered
                   ? window.hoverSurface : "transparent"
            border.width: languageOption.checked ? 1 : 0
            border.color: languageOption.checked
                          ? (window.darkTheme ? "#5268C7" : "#AEBBEE")
                          : "transparent"
        }

        contentItem: RowLayout {
            spacing: 9

            Rectangle {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: 6
                color: languageOption.checked ? window.primary : "transparent"
                border.width: languageOption.checked ? 0 : 1
                border.color: window.outline

                Label {
                    anchors.centerIn: parent
                    text: "✓"
                    visible: languageOption.checked
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Label {
                Layout.fillWidth: true
                text: languageOption.optionText
                color: languageOption.checked ? window.textPrimary : window.textSecondary
                font.pixelSize: 11
                font.weight: languageOption.checked ? Font.DemiBold : Font.Normal
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component StatusChip: Rectangle {
        required property string caption
        required property string value
        property color accent: window.cyan
        Layout.preferredWidth: 104
        Layout.minimumWidth: 92
        Layout.preferredHeight: 42
        radius: 12
        color: window.panelAlt
        border.color: window.outline
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 11; anchors.rightMargin: 10; spacing: 8
            Rectangle { width: 6; height: 6; radius: 3; color: parent.parent.accent }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 1
                Label { text: parent.parent.parent.caption; color: window.textMuted; font.pixelSize: 8; font.weight: Font.DemiBold; font.letterSpacing: 0.7 }
                Label { Layout.fillWidth: true; text: parent.parent.parent.value; color: window.textPrimary; elide: Text.ElideRight; font.pixelSize: 10; font.weight: Font.DemiBold }
            }
        }
    }

    component DisclosureButton: Button {
        required property string label
        required property string detail
        required property bool opened
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        leftPadding: 14
        rightPadding: 8
        background: Rectangle {
            radius: 12
            color: parent.opened ? window.activeSurface : parent.hovered ? window.hoverSurface : "transparent"
            border.width: parent.opened ? 1 : 0
            border.color: parent.opened ? (window.darkTheme ? "#536CCB" : "#8799E8") : "transparent"
        }
        contentItem: RowLayout {
            spacing: 8
            Label { text: parent.parent.label; color: window.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
            Label { text: parent.parent.detail; color: window.textMuted; font.pixelSize: 10 }
            Item { Layout.fillWidth: true }
            Rectangle {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: 8
                color: parent.parent.opened ? (window.darkTheme ? "#283865" : "#DDE4FF") : window.panelAlt
                border.color: window.outline
                Item {
                    anchors.centerIn: parent
                    width: 12
                    height: 8
                    property color strokeColor: parent.parent.parent.opened ? "#8FA3FF" : window.textSecondary
                    rotation: parent.parent.parent.opened ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    Rectangle {
                        width: 7; height: 1.5; radius: 0.75
                        x: -0.2; y: 2.5
                        color: parent.strokeColor
                        rotation: 42
                    }
                    Rectangle {
                        width: 7; height: 1.5; radius: 0.75
                        x: 5.2; y: 2.5
                        color: parent.strokeColor
                        rotation: -42
                    }
                }
            }
        }
    }

    component ViewerButton: Button {
        required property string label
        required property string detail
        required property bool opened
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        leftPadding: 14
        rightPadding: 8
        background: Rectangle {
            radius: 12
            color: parent.opened ? window.activeSurface : parent.hovered ? window.hoverSurface : "transparent"
            border.width: parent.opened ? 1 : 0
            border.color: parent.opened ? (window.darkTheme ? "#536CCB" : "#8799E8") : "transparent"
        }
        contentItem: RowLayout {
            spacing: 8
            Label { text: parent.parent.label; color: window.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
            Label { text: parent.parent.detail; color: window.textMuted; font.pixelSize: 10 }
            Item { Layout.fillWidth: true }
            Rectangle {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: 8
                color: parent.parent.opened ? (window.darkTheme ? "#283865" : "#DDE4FF") : window.panelAlt
                border.color: window.outline
                Label {
                    anchors.centerIn: parent
                    text: parent.parent.parent.opened ? "—" : "↗"
                    color: parent.parent.parent.opened ? window.primary : window.textSecondary
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }
        }
    }

    component ScaleOption: MenuItem {
        id: scaleOption
        required property int scalePercent
        required property string optionText
        implicitWidth: 154
        implicitHeight: 34
        text: optionText
        checkable: true
        checked: companion.displayScalePercent === scalePercent
        hoverEnabled: true
        leftPadding: 9
        rightPadding: 9
        onTriggered: companion.set_display_scale_percent(scalePercent)

        indicator: Item { implicitWidth: 0; implicitHeight: 0 }
        arrow: Item { implicitWidth: 0; implicitHeight: 0 }

        background: Rectangle {
            radius: 8
            color: scaleOption.highlighted || scaleOption.hovered
                   ? window.hoverSurface : "transparent"
            border.width: scaleOption.checked ? 1 : 0
            border.color: scaleOption.checked
                          ? (window.darkTheme ? "#5268C7" : "#AEBBEE")
                          : "transparent"
        }

        contentItem: RowLayout {
            spacing: 9
            Rectangle {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                radius: 5
                color: scaleOption.checked ? window.primary : "transparent"
                border.width: scaleOption.checked ? 0 : 1
                border.color: window.outline
                Label {
                    anchors.centerIn: parent
                    text: "✓"
                    visible: scaleOption.checked
                    color: "white"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
            Label {
                Layout.fillWidth: true
                text: scaleOption.optionText
                color: scaleOption.checked ? window.textPrimary : window.textSecondary
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component MiniToggle: AbstractButton {
        id: toggle
        checkable: true
        implicitWidth: 30
        implicitHeight: 16
        contentItem: Item {}
        background: Rectangle {
            radius: height / 2
            color: toggle.checked ? "#5C73F2" : (window.darkTheme ? "#2A3545" : "#CAD3DE")
            border.width: 1
            border.color: toggle.checked ? "#8092FF" : (window.darkTheme ? "#3A475A" : "#B8C3D0")
            Rectangle {
                width: 12; height: 12; radius: 6
                y: 2
                x: toggle.checked ? parent.width - width - 2 : 2
                color: "#FFFFFF"
                Behavior on x { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            }
        }
    }

    component ThinSlider: Slider {
        id: thinSlider
        Layout.preferredHeight: 18
        background: Rectangle {
            x: thinSlider.leftPadding
            y: thinSlider.topPadding + thinSlider.availableHeight / 2 - height / 2
            width: thinSlider.availableWidth
            height: 4
            radius: 2
            color: window.darkTheme ? "#273241" : "#D5DDE7"
            Rectangle {
                width: thinSlider.visualPosition * parent.width
                height: parent.height; radius: 2
                color: window.primary
                opacity: thinSlider.enabled ? 0.9 : 0.35
            }
        }
        handle: Rectangle {
            x: thinSlider.leftPadding + thinSlider.visualPosition * (thinSlider.availableWidth - width)
            y: thinSlider.topPadding + thinSlider.availableHeight / 2 - height / 2
            width: 14; height: 14; radius: 7
            color: thinSlider.pressed ? window.primary : window.textPrimary
            border.width: 2
            border.color: window.primary
            opacity: thinSlider.enabled ? 1.0 : 0.45
        }
    }

    component ModeButton: Button {
        required property string modeName
        required property string label
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        onClicked: companion.set_output_mode(modeName)
        background: Rectangle {
            radius: 9
            color: companion.outputMode === parent.modeName ? "#5068E8" : parent.hovered ? window.hoverSurface : window.panelAlt
            border.color: companion.outputMode === parent.modeName ? "#7589FF" : window.outline
        }
        contentItem: Label { text: parent.label; color: companion.outputMode === parent.modeName ? "white" : window.textSecondary; font.pixelSize: 10; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }

    component DarkField: TextField {
        Layout.fillWidth: true
        Layout.preferredHeight: 38
        color: window.textPrimary
        placeholderTextColor: window.textMuted
        selectionColor: window.primary
        font.pixelSize: 11
        leftPadding: 12; rightPadding: 12
        background: Rectangle { radius: 10; color: window.fieldSurface; border.color: parent.activeFocus ? "#596FE3" : window.outline }
    }

    component ComboChevron: Item {
        implicitWidth: 16
        implicitHeight: 16
        property color strokeColor: window.textSecondary

        onStrokeColorChanged: chevronCanvas.requestPaint()

        Canvas {
            id: chevronCanvas
            anchors.fill: parent
            onPaint: {
                const context = getContext("2d")
                context.clearRect(0, 0, width, height)
                context.strokeStyle = parent.strokeColor
                context.lineWidth = 1.5
                context.lineCap = "round"
                context.lineJoin = "round"
                context.beginPath()
                context.moveTo(4.5, 6.25)
                context.lineTo(8, 9.75)
                context.lineTo(11.5, 6.25)
                context.stroke()
            }
        }
    }

    component SerialPortCombo: ComboBox {
        id: portControl
        Layout.fillWidth: true
        Layout.preferredHeight: 38
        model: companion.usbPorts
        currentIndex: companion.usbPorts.indexOf(companion.usbPort)
        displayText: currentIndex >= 0 ? currentText : companion.usbPort.length ? companion.usbPort + qsTr("  ·  unavailable") : count > 0 ? qsTr("Select COM port") : qsTr("No COM ports found")
        onActivated: companion.set_usb_port(currentText)
        leftPadding: 12; rightPadding: 34
        contentItem: Label {
            text: portControl.displayText
            color: portControl.currentIndex >= 0 ? window.textPrimary : window.textMuted
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: ComboChevron {
            x: portControl.width - width - 10
            y: Math.round((portControl.height - height) / 2)
        }
        background: Rectangle {
            radius: 10; color: window.fieldSurface
            border.color: portControl.activeFocus ? "#596FE3" : window.outline
        }
        delegate: ItemDelegate {
            required property int index
            required property var modelData
            width: portControl.width - 12
            height: 34
            text: modelData
            highlighted: portControl.highlightedIndex === index
            background: Rectangle { radius: 8; color: parent.highlighted ? window.activeSurface : parent.hovered ? window.hoverSurface : "transparent" }
            contentItem: Label { text: parent.text; color: window.textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; leftPadding: 7 }
        }
        popup: Popup {
            y: portControl.height + 4
            width: portControl.width
            implicitHeight: Math.min(contentItem.implicitHeight + 12, 220)
            padding: 6
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: portControl.popup.visible ? portControl.delegateModel : null
                currentIndex: portControl.highlightedIndex
                ScrollBar.vertical: AppScrollBar {
                    darkTheme: window.darkTheme
                    policy: ScrollBar.AsNeeded
                    interactive: false
                }
            }
            background: Rectangle { radius: 11; color: window.panel; border.color: window.outline }
        }
    }

    component ParticipantCombo: ComboBox {
        id: participantControl
        required property var choices
        required property string selectedKey
        property string unavailableText: qsTr("Automatic until a live frame arrives")
        signal participantChosen(string key)
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        model: choices
        textRole: "label"
        valueRole: "key"
        currentIndex: {
            for (let i = 0; i < choices.length; ++i) {
                if (choices[i].key === selectedKey) return i
            }
            return selectedKey.length ? -1 : 0
        }
        displayText: currentIndex >= 0 && count > 0 ? currentText : selectedKey.length ? selectedKey : unavailableText
        onActivated: participantChosen(currentValue)
        leftPadding: 12; rightPadding: 34
        contentItem: Label {
            text: participantControl.displayText
            color: participantControl.currentIndex >= 0 && participantControl.count > 0 ? window.textPrimary : window.textMuted
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: ComboChevron {
            x: participantControl.width - width - 10
            y: Math.round((participantControl.height - height) / 2)
        }
        background: Rectangle {
            radius: 10; color: window.fieldSurface
            border.color: participantControl.activeFocus ? "#596FE3" : window.outline
        }
        delegate: ItemDelegate {
            id: participantOption
            required property int index
            width: participantControl.width - 12
            height: 34
            text: participantControl.textAt(index)
            highlighted: participantControl.highlightedIndex === index
            background: Rectangle { radius: 8; color: participantOption.highlighted ? window.activeSurface : participantOption.hovered ? window.hoverSurface : "transparent" }
            contentItem: Label { text: participantOption.text; color: window.textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; leftPadding: 7 }
        }
        popup: Popup {
            y: participantControl.height + 4
            width: participantControl.width
            implicitHeight: Math.min(contentItem.implicitHeight + 12, 220)
            padding: 6
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: participantControl.popup.visible ? participantControl.delegateModel : null
                currentIndex: participantControl.highlightedIndex
                ScrollBar.vertical: AppScrollBar {
                    darkTheme: window.darkTheme
                    policy: ScrollBar.AsNeeded
                    interactive: false
                }
            }
            background: Rectangle { radius: 11; color: window.panel; border.color: window.outline }
        }
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: window.visibility === Window.Maximized ? 0 : 15
        color: window.surface
        border.color: window.darkTheme ? "#2A3647" : "#C9D4E1"
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                id: titleBar
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                Layout.maximumHeight: 42
                color: window.titleSurface
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 8; spacing: 9
                    Image {
                        Layout.minimumWidth: 27
                        Layout.preferredWidth: 27
                        Layout.maximumWidth: 27
                        Layout.minimumHeight: 27
                        Layout.preferredHeight: 27
                        Layout.maximumHeight: 27
                        source: "qrc:/qt/qml/MotionBridge/App/assets/icons/motion-bridge.svg"
                        sourceSize.width: 72
                        sourceSize.height: 72
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                    Label { text: "Motion Bridge"; color: window.textPrimary; font.pixelSize: 12; font.weight: Font.DemiBold }
                    Label { text: qsTr("GAME MOTION · MULTI-AXIS"); color: window.textMuted; font.pixelSize: 8; font.letterSpacing: 1.1 }
                    Item { Layout.fillWidth: true }
                    WindowButton {
                        id: scaleButton
                        Layout.preferredWidth: 48
                        glyph: companion.displayScalePercent === 0 ? "DPI" : companion.displayScalePercent + "%"
                        glyphSize: 9
                        glyphColor: companion.displayScaleRestartRequired ? "#F1B865" : window.textSecondary
                        tipText: companion.displayScaleRestartRequired
                                 ? qsTr("Display scale · restart required")
                                 : qsTr("Display scale")
                        onClicked: scaleMenu.popup(scaleButton,
                                                   scaleButton.width - scaleMenu.width,
                                                   scaleButton.height + 4)
                        Menu {
                            id: scaleMenu
                            width: 168
                            padding: 7
                            margins: 6
                            background: Rectangle {
                                radius: 12
                                color: window.panel
                                border.color: window.outline
                            }
                            ScaleOption { scalePercent: 0; optionText: qsTr("Follow system") }
                            ScaleOption { scalePercent: 75; optionText: "75%" }
                            ScaleOption { scalePercent: 90; optionText: "90%" }
                            ScaleOption { scalePercent: 100; optionText: "100%" }
                            ScaleOption { scalePercent: 110; optionText: "110%" }
                            ScaleOption { scalePercent: 125; optionText: "125%" }
                            MenuItem {
                                enabled: false
                                implicitHeight: 28
                                background: Item {}
                                contentItem: Label {
                                    text: qsTr("Applies after restart")
                                    color: window.textMuted
                                    font.pixelSize: 8
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                    WindowButton {
                        id: languageButton
                        glyph: languageController.effectiveLanguage === "zh_CN" ? "中" : "EN"
                        tipText: qsTr("Language")
                        glyphSize: languageController.effectiveLanguage === "zh_CN" ? 12 : 10
                        onClicked: languageMenu.popup(languageButton,
                                                       languageButton.width - languageMenu.width,
                                                       languageButton.height + 4)
                        Menu {
                            id: languageMenu
                            width: 168
                            padding: 7
                            margins: 6
                            background: Rectangle {
                                radius: 12
                                color: window.panel
                                border.color: window.outline
                            }
                            LanguageOption {
                                languageCode: "auto"
                                optionText: qsTr("Follow system")
                            }
                            LanguageOption {
                                languageCode: "zh_CN"
                                optionText: "中文"
                            }
                            LanguageOption {
                                languageCode: "en"
                                optionText: "English"
                            }
                        }
                    }
                    WindowButton {
                        glyph: window.darkTheme ? "☀" : "☾"
                        tipText: window.darkTheme ? qsTr("Switch to light theme") : qsTr("Switch to dark theme")
                        onClicked: companion.set_theme(window.darkTheme ? "light" : "dark")
                    }
                    WindowButton { glyph: "—"; onClicked: window.showMinimized() }
                    WindowButton { glyph: "✕"; hoverColor: "#B84350"; onClicked: window.close() }
                }
                MouseArea {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
                    // Keep the drag layer away from all title-bar controls;
                    // overlap here made the left-most hover target intermittent.
                    anchors.rightMargin: 266
                    acceptedButtons: Qt.LeftButton
                    onPressed: window.startSystemMove()
                    onDoubleClicked: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 82
                Layout.maximumHeight: 82
                color: window.headerSurface
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 20; spacing: 16
                    ColumnLayout {
                        Layout.preferredWidth: 208; spacing: 2
                        Label { text: qsTr("Live control"); color: window.textPrimary; font.pixelSize: 18; font.bold: true }
                        Label {
                            text: companion.actionName.length
                                ? companion.actionName + (companion.referencePlane.length ? "  ·  " + companion.referencePlane : "")
                                : "Operation Lovecraft: Fallen Doll"
                            color: window.textMuted; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 10
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 9
                        Item { Layout.fillWidth: true }
                        StatusChip { caption: qsTr("STREAM"); value: companion.streamConnected ? qsTr("ONLINE") : qsTr("WAITING"); accent: companion.streamConnected ? "#58D9FA" : "#F1B865" }
                        StatusChip { caption: qsTr("MOTION"); value: window.motionStateLabel(companion.motionState); accent: window.motionIsLive(companion.motionState) ? "#56E3B1" : "#7E8CA2" }
                        StatusChip {
                            caption: qsTr("DEVICE")
                            value: companion.armed ? qsTr("ARMED")
                                  : companion.outputConnecting ? qsTr("CONNECTING")
                                  : companion.outputMode === "none" ? qsTr("OFF") : companion.outputMode.toUpperCase()
                            accent: companion.armed ? "#56E3B1" : "#F1B865"
                        }
                    }
                    Button {
                        Layout.preferredWidth: 128; Layout.preferredHeight: 42
                        text: companion.armed || companion.outputConnecting ? qsTr("STOP OUTPUT") : qsTr("ARM OUTPUT")
                        onClicked: companion.set_armed(!(companion.armed || companion.outputConnecting))
                        background: Rectangle { radius: 12; color: companion.armed || companion.outputConnecting ? "#DA5361" : window.primary; opacity: parent.down ? 0.78 : 1.0 }
                        contentItem: Label { text: parent.text; color: "white"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 62
                Layout.maximumHeight: 62
                color: window.footerSurface
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 18; spacing: 6
                    DisclosureButton { label: qsTr("Device connection"); detail: qsTr("USB · Wi-Fi · Intiface"); opened: window.connectionExpanded; onClicked: window.toggleConnection() }
                    DisclosureButton { label: qsTr("Motion tuning"); detail: "L0 · L1 · L2 · R0 · R1 · R2"; opened: window.tuningExpanded; onClicked: window.toggleTuning() }
                    ViewerButton { label: qsTr("3D preview"); detail: qsTr("Separate window"); opened: previewWindow.visible; onClicked: window.togglePreview() }
                    Label { text: qsTr("OUTPUT SAFE"); color: window.textMuted; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.8; Layout.leftMargin: 10 }
                }
            }

            Rectangle {
                id: expandedArea
                visible: window.expanded
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                color: window.darkTheme ? "#090E16" : "#EDF2F7"

                ScrollView {
                    id: expandedScroll
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: expandedContent.implicitHeight + 24
                    ScrollBar.horizontal: AppScrollBar {
                        darkTheme: window.darkTheme
                        policy: ScrollBar.AlwaysOff
                    }
                    ScrollBar.vertical: AppScrollBar {
                        darkTheme: window.darkTheme
                        policy: ScrollBar.AsNeeded
                    }

                    ColumnLayout {
                        id: expandedContent
                        x: 16; y: 12
                        width: expandedScroll.availableWidth - 32
                        spacing: 10

                    RowLayout {
                        visible: window.connectionExpanded
                        Layout.fillWidth: true
                        Layout.preferredHeight: 360
                        spacing: 14
                        Rectangle {
                            visible: window.connectionExpanded
                            Layout.fillWidth: true
                            Layout.minimumWidth: 354
                            Layout.maximumWidth: 920
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillHeight: true
                            radius: 16; color: window.panel; border.color: window.outline
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 18; spacing: 11
                                RowLayout { Layout.fillWidth: true
                                    ColumnLayout { spacing: 2
                                        Label { text: qsTr("Device connection"); color: window.textPrimary; font.pixelSize: 16; font.bold: true }
                                        Label { text: qsTr("One transport at a time"); color: window.textMuted; font.pixelSize: 10 }
                                    }
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        id: autoReconnectButton
                                        visible: companion.outputMode !== "none"
                                        Layout.preferredWidth: 132
                                        Layout.preferredHeight: 26
                                        onClicked: companion.set_auto_reconnect(!companion.autoReconnect)
                                        background: Rectangle {
                                            radius: 8
                                            color: companion.autoReconnect
                                                   ? (window.darkTheme ? "#173728" : "#E8F8EF")
                                                   : window.panelAlt
                                            border.width: 1
                                            border.color: companion.autoReconnect ? "#45C98F" : window.outline
                                        }
                                        contentItem: RowLayout {
                                            spacing: 6
                                            Item {
                                                Layout.preferredWidth: 16
                                                Layout.preferredHeight: 16
                                                Canvas {
                                                    anchors.fill: parent
                                                    onPaint: {
                                                        const ctx = getContext("2d")
                                                        ctx.clearRect(0, 0, width, height)
                                                        ctx.strokeStyle = companion.autoReconnect ? "#45C98F" : window.textMuted
                                                        ctx.lineWidth = 1.5
                                                        ctx.lineCap = "round"
                                                        ctx.lineJoin = "round"
                                                        ctx.beginPath()
                                                        ctx.arc(8, 8, 5, Math.PI * 0.20, Math.PI * 1.35)
                                                        ctx.moveTo(2.7, 8.6); ctx.lineTo(2.3, 12.1); ctx.lineTo(5.8, 11.5)
                                                        ctx.arc(8, 8, 5, Math.PI * 1.20, Math.PI * 0.35, true)
                                                        ctx.moveTo(13.3, 7.4); ctx.lineTo(13.7, 3.9); ctx.lineTo(10.2, 4.5)
                                                        ctx.stroke()
                                                    }
                                                    Connections {
                                                        target: companion
                                                        function onSettingsChanged() { parent.requestPaint() }
                                                    }
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: qsTr("AUTO RECONNECT")
                                                color: companion.autoReconnect ? "#45C98F" : window.textMuted
                                                font.pixelSize: 8
                                                font.bold: true
                                                font.letterSpacing: 0.45
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                            Label {
                                                text: companion.autoReconnect ? qsTr("ON") : qsTr("OFF")
                                                color: companion.autoReconnect ? "#45C98F" : window.textMuted
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                        }
                                        AppToolTip {
                                            visible: autoReconnectButton.hovered
                                            text: companion.outputMode === "wifi"
                                                ? qsTr("Recreates Wi-Fi output after a local network, address, or UDP socket error. UDP cannot confirm whether the remote device itself is connected. Manual STOP OUTPUT always cancels reconnection.")
                                                : qsTr("After an unexpected USB, Intiface, or device disconnect, reconnect automatically and resume the output that you already enabled. Manual STOP OUTPUT always cancels reconnection.")
                                            darkTheme: window.darkTheme
                                        }
                                    }
                                    Rectangle { Layout.preferredWidth: companion.outputConnecting ? 92 : 70; Layout.preferredHeight: 24; radius: 12; color: companion.armed ? (window.darkTheme ? "#173B34" : "#DDF7ED") : window.panelAlt; border.color: window.outline
                                        Label { anchors.centerIn: parent; text: companion.armed ? qsTr("ARMED") : companion.outputConnecting ? qsTr("CONNECTING") : qsTr("SAFE"); color: companion.armed ? (window.darkTheme ? "#5BE4B5" : "#167A5B") : window.textMuted; font.pixelSize: 9; font.bold: true }
                                    }
                                }
                                RowLayout { Layout.fillWidth: true; spacing: 6
                                    ModeButton { modeName: "none"; label: qsTr("OFF") }
                                    ModeButton { modeName: "usb"; label: "USB" }
                                    ModeButton { modeName: "wifi"; label: "WI-FI" }
                                    ModeButton { modeName: "intiface"; label: "INTIFACE" }
                                }
                                Label {
                                    visible: companion.outputMode === "intiface"
                                    Layout.fillWidth: true
                                    text: qsTr("Connect devices in Intiface Central first, then ARM OUTPUT. MotionBridge maps L0 0–100% to each device's advertised range. Timed-position linear devices, mainly Handy, use a dedicated 20 Hz clock. Target arrival time is automatic by default, with an optional manual override in Output processing.")
                                    wrapMode: Text.WordWrap
                                    color: window.textMuted
                                    font.pixelSize: 9
                                    lineHeight: 1.12
                                }
                                Label {
                                    visible: companion.outputMode !== "none"
                                    Layout.fillWidth: true
                                    text: qsTr("OUTPUT STATUS: %1").arg(companion.outputStatus)
                                    color: companion.armed ? "#56E3B1" : "#F1B865"
                                    font.pixelSize: 9
                                    font.bold: true
                                    wrapMode: Text.WordWrap
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    columns: 2
                                    columnSpacing: 10
                                    rowSpacing: 8

                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 4
                                        Label { text: qsTr("USB PORT"); color: window.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.7 }
                                        RowLayout {
                                            Layout.fillWidth: true; spacing: 7
                                            SerialPortCombo { }
                                            Button {
                                                id: refreshPortsButton
                                                Layout.preferredWidth: 38; Layout.preferredHeight: 38
                                                text: "↻"; onClicked: companion.refresh_usb_ports()
                                                background: Rectangle { radius: 10; color: parent.hovered ? window.hoverSurface : window.panelAlt; border.color: window.outline }
                                                contentItem: Label { text: parent.text; color: window.textSecondary; font.pixelSize: 15; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                                AppToolTip {
                                                    visible: refreshPortsButton.hovered
                                                    text: qsTr("Refresh serial ports")
                                                    darkTheme: window.darkTheme
                                                }
                                            }
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 4
                                        Label { text: qsTr("WI-FI HOST"); color: window.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.7 }
                                        DarkField { text: companion.wifiHost; placeholderText: qsTr("Wi-Fi host"); onEditingFinished: companion.set_wifi_endpoint(text, companion.wifiPort) }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 4
                                        Label { text: qsTr("WI-FI PORT"); color: window.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.7 }
                                        DarkField {
                                            text: companion.wifiPort
                                            placeholderText: qsTr("Wi-Fi port")
                                            inputMethodHints: Qt.ImhDigitsOnly
                                            onEditingFinished: {
                                                const value = text.trim()
                                                const port = Number(value)
                                                if (/^\d+$/.test(value) && port >= 1 && port <= 65535) {
                                                    companion.set_wifi_endpoint(companion.wifiHost, Math.floor(port))
                                                } else {
                                                    text = companion.wifiPort.toString()
                                                }
                                            }
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 4
                                        Label { text: qsTr("INTIFACE URL"); color: window.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.7 }
                                        DarkField { text: companion.intifaceUrl; placeholderText: qsTr("Intiface Desktop URL"); onEditingFinished: companion.set_intiface_url(text) }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 4
                                        Label { text: qsTr("SAFETY"); color: window.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.7 }
                                        Button {
                                            Layout.fillWidth: true; Layout.preferredHeight: 38; text: qsTr("CENTER & DISARM")
                                            onClicked: companion.emergency_stop()
                                            background: Rectangle { radius: 10; color: parent.hovered ? window.hoverSurface : window.panelAlt; border.color: window.outline }
                                            contentItem: Label { text: parent.text; color: window.textSecondary; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        visible: window.tuningExpanded
                        Layout.fillWidth: true
                        Label { text: qsTr("Motion tuning"); color: window.textPrimary; font.pixelSize: 16; font.bold: true }
                        Label { text: qsTr("Device-side response · raw game motion stays unchanged"); color: window.textMuted; font.pixelSize: 10; Layout.leftMargin: 7 }
                        Item { Layout.fillWidth: true }
                    }
                    Rectangle {
                        visible: window.tuningExpanded
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        radius: 12
                        color: window.panel
                        border.color: window.outline
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10
                            ColumnLayout {
                                Layout.preferredWidth: 190; spacing: 1
                                Label { text: qsTr("Participant routing"); color: window.textPrimary; font.pixelSize: 11; font.bold: true }
                                Label { text: qsTr("Choose the motion source; target comes from the game stream"); color: window.textMuted; font.pixelSize: 8; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            }
                            ColumnLayout {
                                Layout.minimumWidth: 260
                                Layout.preferredWidth: 330
                                Layout.maximumWidth: 330
                                spacing: 2
                                Label { text: qsTr("REFERENCE PARTICIPANT"); color: window.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.7 }
                                ParticipantCombo {
                                    choices: companion.referenceParticipants
                                    selectedKey: companion.referenceParticipant
                                    onParticipantChosen: (key) => companion.set_reference_participant(key)
                                }
                            }
                            ColumnLayout {
                                Layout.minimumWidth: 220
                                Layout.preferredWidth: 260
                                Layout.maximumWidth: 280
                                spacing: 3
                                RowLayout {
                                    Layout.preferredHeight: 16
                                    Label { text: qsTr("SAFETY DISTANCE"); color: window.textSecondary; font.pixelSize: 9; font.bold: true }
                                    MiniToggle { checked: companion.safetyDistanceEnabled; onToggled: companion.set_safety_distance_enabled(checked) }
                                    Item { Layout.fillWidth: true }
                                    Label { text: companion.safetyDistanceCm.toFixed(0) + " cm"; color: window.textMuted; font.pixelSize: 9; font.family: "Cascadia Mono" }
                                }
                                ThinSlider {
                                    id: safetyDistanceSlider
                                    Layout.fillWidth: true
                                    enabled: companion.safetyDistanceEnabled
                                    from: 2; to: 50; stepSize: 1
                                    value: companion.safetyDistanceCm
                                    onPressedChanged: if (!pressed) companion.set_safety_distance_cm(value)
                                    AppToolTip {
                                        visible: safetyDistanceSlider.hovered
                                        text: qsTr("Signals start only after Reference and Target are within this distance")
                                        darkTheme: window.darkTheme
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                    Rectangle {
                        visible: window.tuningExpanded
                        Layout.fillWidth: true
                        Layout.preferredHeight: 62
                        radius: 12
                        color: window.panel
                        border.color: window.outline
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 18
                            ColumnLayout {
                                Layout.preferredWidth: 170; spacing: 1
                                Label { text: qsTr("Output processing"); color: window.textPrimary; font.pixelSize: 11; font.bold: true }
                                Label { text: qsTr("Fixed cadence · device-side protection"); color: window.textMuted; font.pixelSize: 8 }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.preferredWidth: 300; spacing: 3
                                RowLayout {
                                    Layout.preferredHeight: 16
                                    Label { text: qsTr("OUTPUT RATE"); color: window.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.7 }
                                    Item { Layout.fillWidth: true }
                                    Label { text: companion.outputRateHz + " Hz"; color: window.textSecondary; font.pixelSize: 9; font.family: "Cascadia Mono" }
                                }
                                ThinSlider {
                                    id: outputRateSlider
                                    Layout.fillWidth: true
                                    from: 20; to: 100; stepSize: 5
                                    value: companion.outputRateHz
                                    onPressedChanged: if (!pressed) companion.set_output_rate_hz(Math.round(value))
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.preferredWidth: 280; spacing: 3
                                RowLayout {
                                    Layout.preferredHeight: 16
                                    Label { text: qsTr("TARGET ARRIVAL TIME"); color: window.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 0.7 }
                                    MiniToggle {
                                        checked: companion.intifaceTargetTimeAutomatic
                                        onToggled: companion.set_intiface_target_time_automatic(checked)
                                    }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: companion.intifaceTargetTimeAutomatic
                                            ? qsTr("AUTO")
                                            : companion.intifaceTargetTimeMs + " ms"
                                        color: window.textSecondary
                                        font.pixelSize: 9
                                        font.family: "Cascadia Mono"
                                    }
                                }
                                ThinSlider {
                                    id: intifaceTargetTimeSlider
                                    Layout.fillWidth: true
                                    from: 50; to: 100; stepSize: 5
                                    value: companion.intifaceTargetTimeMs
                                    enabled: !companion.intifaceTargetTimeAutomatic
                                    opacity: enabled ? 1.0 : 0.42
                                    onPressedChanged: if (!pressed) companion.set_intiface_target_time_ms(Math.round(value))
                                    AppToolTip {
                                        visible: intifaceTargetTimeSlider.hovered
                                        text: qsTr("Only used by timed-position linear devices through Intiface, mainly Handy. Automatic follows the real 20 Hz output interval, normally 50 ms. Manual values from 50–100 ms can soften movement, but higher values add response delay.")
                                        darkTheme: window.darkTheme
                                    }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.preferredWidth: 330; spacing: 3
                                RowLayout {
                                    Layout.preferredHeight: 16
                                    Label { text: qsTr("SOFT START"); color: window.textSecondary; font.pixelSize: 9; font.bold: true }
                                    MiniToggle { checked: companion.softStartEnabled; onToggled: companion.set_soft_start_enabled(checked) }
                                    Item { Layout.fillWidth: true }
                                    Label { text: companion.softStartDurationMs + " ms"; color: window.textMuted; font.pixelSize: 9; font.family: "Cascadia Mono" }
                                }
                                ThinSlider {
                                    id: softStartSlider
                                    Layout.fillWidth: true
                                    enabled: companion.softStartEnabled
                                    from: 0; to: 3000; stepSize: 100
                                    value: companion.softStartDurationMs
                                    onPressedChanged: if (!pressed) companion.set_soft_start_duration_ms(Math.round(value))
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                    GridLayout {
                        visible: window.tuningExpanded
                        Layout.fillWidth: true
                        columns: width > 940 ? 3 : 2
                        columnSpacing: 9; rowSpacing: 9
                        AxisCard { controller: companion; darkTheme: window.darkTheme; axisIndex: 0; axisName: qsTr("L0  STROKE"); axisValue: companion.deviceAxes[0]; axisValues: companion.smartLimitInputAxes; gain: companion.axisGains[0]; outputMinimum: companion.axisMinimums[0]; outputMaximum: companion.axisMaximums[0]; outputSettings: companion.axisOutputSettings[0] || ({}); travelPreference: companion.axisTravelPreferences[0] || ({}); travelStatus: companion.axisTravelStatuses[0] || ({}) }
                        AxisCard { controller: companion; darkTheme: window.darkTheme; axisIndex: 1; axisName: qsTr("L1  SURGE"); axisValue: companion.deviceAxes[1]; axisValues: companion.smartLimitInputAxes; gain: companion.axisGains[1]; outputMinimum: companion.axisMinimums[1]; outputMaximum: companion.axisMaximums[1]; outputSettings: companion.axisOutputSettings[1] || ({}); travelPreference: companion.axisTravelPreferences[1] || ({}); travelStatus: companion.axisTravelStatuses[1] || ({}) }
                        AxisCard { controller: companion; darkTheme: window.darkTheme; axisIndex: 2; axisName: qsTr("L2  SWAY"); axisValue: companion.deviceAxes[2]; axisValues: companion.smartLimitInputAxes; gain: companion.axisGains[2]; outputMinimum: companion.axisMinimums[2]; outputMaximum: companion.axisMaximums[2]; outputSettings: companion.axisOutputSettings[2] || ({}); travelPreference: companion.axisTravelPreferences[2] || ({}); travelStatus: companion.axisTravelStatuses[2] || ({}) }
                        AxisCard { controller: companion; darkTheme: window.darkTheme; axisIndex: 3; axisName: qsTr("R0  TWIST"); axisValue: companion.deviceAxes[3]; axisValues: companion.smartLimitInputAxes; gain: companion.axisGains[3]; outputMinimum: companion.axisMinimums[3]; outputMaximum: companion.axisMaximums[3]; outputSettings: companion.axisOutputSettings[3] || ({}); travelPreference: companion.axisTravelPreferences[3] || ({}); travelStatus: companion.axisTravelStatuses[3] || ({}) }
                        AxisCard { controller: companion; darkTheme: window.darkTheme; axisIndex: 4; axisName: qsTr("R1  ROLL"); axisValue: companion.deviceAxes[4]; axisValues: companion.smartLimitInputAxes; gain: companion.axisGains[4]; outputMinimum: companion.axisMinimums[4]; outputMaximum: companion.axisMaximums[4]; outputSettings: companion.axisOutputSettings[4] || ({}); travelPreference: companion.axisTravelPreferences[4] || ({}); travelStatus: companion.axisTravelStatuses[4] || ({}) }
                        AxisCard { controller: companion; darkTheme: window.darkTheme; axisIndex: 5; axisName: qsTr("R2  PITCH"); axisValue: companion.deviceAxes[5]; axisValues: companion.smartLimitInputAxes; gain: companion.axisGains[5]; outputMinimum: companion.axisMinimums[5]; outputMaximum: companion.axisMaximums[5]; outputSettings: companion.axisOutputSettings[5] || ({}); travelPreference: companion.axisTravelPreferences[5] || ({}); travelStatus: companion.axisTravelStatuses[5] || ({}) }
                    }
                    Item { Layout.preferredHeight: 4 }
                    }
                }
            }
        }
    }

    Window {
        id: previewWindow
        width: 520
        height: 500
        minimumWidth: 340
        minimumHeight: 240
        visible: false
        title: qsTr("3D preview")
        color: "transparent"
        property bool alwaysOnTop: false
        // The device path stays at 50 Hz, but rendering a static desktop
        // preview above a VR game does not need to consume the GPU at that
        // rate. Keep it continuously live at 30 FPS instead.
        property real previewL0: 0.5
        property real previewL1: 0.5
        property real previewL2: 0.5
        property real previewR0: 0.5
        property real previewR1: 0.5
        property real previewR2: 0.5
        flags: alwaysOnTop
               ? (Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
               : (Qt.Window | Qt.FramelessWindowHint)

        function refreshPreviewAxes() {
            previewL0 = companion.deviceAxes[0]
            previewL1 = companion.deviceAxes[1]
            previewL2 = companion.deviceAxes[2]
            previewR0 = companion.deviceAxes[3]
            previewR1 = companion.deviceAxes[4]
            previewR2 = companion.deviceAxes[5]
        }

        onVisibleChanged: {
            if (!visible) return
            refreshPreviewAxes()
            // Windows can apply its final placement after show() returns.
            // Re-center once that placement has settled so the custom title
            // bar is never left outside the usable desktop.
            initialPlacementTimer.restart()
        }
        onWidthChanged: if (visible) keepPreviewWindowOnScreen()
        onHeightChanged: if (visible) keepPreviewWindowOnScreen()
        onScreenChanged: if (visible) keepPreviewWindowOnScreen()

        Timer {
            id: initialPlacementTimer
            interval: 180
            repeat: false
            onTriggered: {
                if (!previewWindow.visible) return
                placePreviewWindow()
                previewWindow.raise()
                previewWindow.requestActivate()
            }
        }

        Timer {
            interval: 33
            repeat: true
            running: previewWindow.visible
            onTriggered: previewWindow.refreshPreviewAxes()
        }

        Rectangle {
            anchors.fill: parent
            radius: 14
            color: window.surface
            border.width: 1
            border.color: window.darkTheme ? "#2A3647" : "#C9D4E1"
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    id: previewTitleBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    color: window.titleSurface

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 8
                        spacing: 8

                        Rectangle { width: 7; height: 7; radius: 4; color: window.cyan }
                        Label {
                            text: qsTr("SR6 / OSR 3D preview")
                            color: window.textPrimary
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            id: alwaysOnTopButton
                            Layout.preferredWidth: 74
                            Layout.preferredHeight: 28
                            text: qsTr("TOP")
                            checkable: true
                            checked: previewWindow.alwaysOnTop
                            onClicked: {
                                previewWindow.alwaysOnTop = checked
                                Qt.callLater(function() {
                                    keepPreviewWindowOnScreen()
                                    previewWindow.show()
                                    previewWindow.raise()
                                    previewWindow.requestActivate()
                                })
                            }
                            background: Rectangle {
                                radius: 8
                                color: parent.checked ? window.activeSurface
                                                      : parent.hovered ? window.hoverSurface : "transparent"
                                border.width: parent.checked ? 1 : 0
                                border.color: window.darkTheme ? "#536CCB" : "#8799E8"
                            }
                            contentItem: Label {
                                text: parent.text
                                color: parent.checked ? window.primary : window.textSecondary
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            AppToolTip {
                                visible: alwaysOnTopButton.hovered
                                text: qsTr("Always on top")
                                darkTheme: window.darkTheme
                            }
                        }
                        WindowButton { glyph: "✕"; hoverColor: "#B84350"; onClicked: previewWindow.hide() }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.rightMargin: 132
                        acceptedButtons: Qt.LeftButton
                        onPressed: previewWindow.startSystemMove()
                    }
                }

                OsrPreview {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 10
                    darkTheme: window.darkTheme
                    l0: previewWindow.previewL0; l1: previewWindow.previewL1; l2: previewWindow.previewL2
                    r0: previewWindow.previewR0; r1: previewWindow.previewR1; r2: previewWindow.previewR2
                }
            }
        }

        MouseArea { z: 20; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 6; cursorShape: Qt.SizeHorCursor; onPressed: previewWindow.startSystemResize(Qt.LeftEdge) }
        MouseArea { z: 20; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 6; cursorShape: Qt.SizeHorCursor; onPressed: previewWindow.startSystemResize(Qt.RightEdge) }
        MouseArea { z: 20; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 5; cursorShape: Qt.SizeVerCursor; onPressed: previewWindow.startSystemResize(Qt.TopEdge) }
        MouseArea { z: 20; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 6; cursorShape: Qt.SizeVerCursor; onPressed: previewWindow.startSystemResize(Qt.BottomEdge) }
    }

    // Native resizing keeps the frameless window behaving like a normal Windows app.
    MouseArea { z: 20; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 6; cursorShape: Qt.SizeHorCursor; onPressed: window.startSystemResize(Qt.LeftEdge) }
    MouseArea { z: 20; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 6; cursorShape: Qt.SizeHorCursor; onPressed: window.startSystemResize(Qt.RightEdge) }
    MouseArea { z: 20; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 5; cursorShape: Qt.SizeVerCursor; onPressed: window.startSystemResize(Qt.TopEdge) }
    MouseArea { z: 20; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 6; cursorShape: Qt.SizeVerCursor; onPressed: window.startSystemResize(Qt.BottomEdge) }
}
