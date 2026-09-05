pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property int axisIndex
    required property string axisName
    required property real axisValue
    required property var axisValues
    required property var controller
    property bool darkTheme: true
    property real gain: 1.0
    property real outputMinimum: 0.0
    property real outputMaximum: 1.0
    property var outputSettings: ({})
    property var travelPreference: ({})
    property var travelStatus: ({})
    Layout.fillWidth: true
    Layout.preferredHeight: 128
    radius: 15
    color: darkTheme ? "#111722" : "#FFFFFF"
    border.color: darkTheme ? "#202B3B" : "#D5DEE9"

    readonly property color accent: axisIndex === 0 ? "#59D7FF" : axisIndex < 3 ? "#7C91FF" : "#B27DFF"
    readonly property color primaryText: darkTheme ? "#F1F5FB" : "#182334"
    readonly property color secondaryText: darkTheme ? "#AAB7C9" : "#536276"
    readonly property color mutedText: darkTheme ? "#637189" : "#7A889A"
    readonly property color trackSurface: darkTheme ? "#080D15" : "#E8EDF3"
    readonly property color valueSurface: darkTheme ? "#192231" : "#EEF2F7"
    readonly property color enabledGreen: "#38D47A"
    readonly property color disabledRed: "#F06A5D"
    readonly property bool axisOutputEnabled: !outputSettings || outputSettings.axisEnabled === undefined || outputSettings.axisEnabled === true
    readonly property real axisReturnPosition: outputSettings && outputSettings.returnPosition !== undefined ? outputSettings.returnPosition : 0.5
    readonly property bool speedEnabled: outputSettings && outputSettings.speedEnabled === true
    readonly property bool axisInverted: outputSettings && outputSettings.inverted === true
    readonly property real speedLimit: outputSettings && outputSettings.maxSpeed !== undefined ? outputSettings.maxSpeed : 4.0
    readonly property var axisLabels: ["L0", "L1", "L2", "R0", "R1", "R2"]
    readonly property bool preferredTravelEnabled: travelPreference && travelPreference.enabled === true
    readonly property real preferredMinimum: travelPreference && travelPreference.preferredMinimum !== undefined ? travelPreference.preferredMinimum : (axisIndex === 0 ? 0 : 20)
    readonly property real preferredMaximum: travelPreference && travelPreference.preferredMaximum !== undefined ? travelPreference.preferredMaximum : (axisIndex === 0 ? 60 : 80)
    readonly property real preferredTravelMaximumGain: travelPreference && travelPreference.maximumGain !== undefined ? travelPreference.maximumGain : (axisIndex < 3 ? 4.0 : 2.0)
    readonly property string preferredTravelState: travelStatus && travelStatus.state !== undefined ? travelStatus.state : "disabled"
    readonly property real observedTravel: travelStatus && travelStatus.observedTravel !== undefined ? travelStatus.observedTravel : 0
    readonly property real automaticGain: travelStatus && travelStatus.automaticGain !== undefined ? travelStatus.automaticGain : 1.0
    readonly property int stableHalfStrokes: travelStatus && travelStatus.stableHalfStrokes !== undefined ? travelStatus.stableHalfStrokes : 0
    readonly property bool smartLimitEnabled: outputSettings && outputSettings.smartLimitEnabled === true
    readonly property int smartLimitInputAxis: outputSettings && outputSettings.smartLimitInputAxis !== undefined ? outputSettings.smartLimitInputAxis : 0
    readonly property string smartLimitMode: outputSettings && outputSettings.smartLimitMode !== undefined ? outputSettings.smartLimitMode : "value"
    readonly property real smartLimitTarget: outputSettings && outputSettings.smartLimitTargetValue !== undefined ? outputSettings.smartLimitTargetValue : 0.5
    readonly property real smartLimitLowerInput: outputSettings && outputSettings.smartLimitLowerInput !== undefined ? outputSettings.smartLimitLowerInput : 0.25
    readonly property real smartLimitLowerFactor: outputSettings && outputSettings.smartLimitLowerFactor !== undefined ? outputSettings.smartLimitLowerFactor : 1.0
    readonly property real smartLimitUpperInput: outputSettings && outputSettings.smartLimitUpperInput !== undefined ? outputSettings.smartLimitUpperInput : 0.9
    readonly property real smartLimitUpperFactor: outputSettings && outputSettings.smartLimitUpperFactor !== undefined ? outputSettings.smartLimitUpperFactor : 0.0
    readonly property real smartLimitInputValue: Math.max(0, Math.min(1,
        axisValues && axisValues.length > smartLimitInputAxis ? axisValues[smartLimitInputAxis] : axisValue))

    function preferredTravelStatusText() {
        if (!root.preferredTravelEnabled || root.preferredTravelState === "disabled") return qsTr("Off")
        if (root.preferredTravelState === "learning") return qsTr("Learning · %1/6").arg(Math.min(6, root.stableHalfStrokes))
        if (root.preferredTravelState === "limited") return qsTr("%1× limit · %2% travel").arg(root.preferredTravelMaximumGain.toFixed(0)).arg(root.observedTravel.toFixed(1))
        return qsTr("Locked · %1×").arg(root.automaticGain.toFixed(2))
    }

    component CompactToggle: AbstractButton {
        id: toggle
        checkable: true
        implicitWidth: 32
        implicitHeight: 17
        contentItem: Item {}
        background: Rectangle {
            radius: height / 2
            color: toggle.checked ? root.enabledGreen : (root.darkTheme ? "#2A3545" : "#CAD3DE")
            border.width: 1
            border.color: toggle.checked ? Qt.lighter(root.enabledGreen, 1.08) : (root.darkTheme ? "#3A475A" : "#B8C3D0")
            Rectangle {
                width: 13; height: 13; radius: 7
                y: 2
                x: toggle.checked ? parent.width - width - 2 : 2
                color: toggle.checked ? "#FFFFFF" : (root.darkTheme ? "#AAB5C5" : "#FFFFFF")
                Behavior on x { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            }
        }
    }

    component StableStepper: Item {
        id: stepper
        property real value: 4.0
        property real from: 0.25
        property real to: 10.0
        property real stepSize: 0.25
        property int decimals: 2
        property string suffixText: "units/s"
        property int numberWidth: 39
        property int suffixWidth: 43
        signal valueModified(real nextValue)
        implicitWidth: 148
        implicitHeight: 27

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: root.darkTheme ? "#101722" : "#F0F3F7"
            border.color: root.darkTheme ? "#334155" : "#C8D2DE"
        }
        ToolButton {
            id: speedDown
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 27
            onClicked: stepper.valueModified(Math.max(stepper.from, stepper.value - stepper.stepSize))
            background: Rectangle {
                radius: 6
                color: speedDown.pressed ? root.valueSurface : speedDown.hovered ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.10) : "transparent"
            }
            contentItem: Label {
                text: "−"
                color: root.secondaryText
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height
            spacing: 3
            Label {
                width: stepper.numberWidth
                height: parent.height
                text: stepper.value.toFixed(stepper.decimals)
                color: root.primaryText
                font.pixelSize: 10
                font.family: "Cascadia Mono"
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
            Label {
                width: stepper.suffixWidth
                height: parent.height
                text: stepper.suffixText
                color: root.mutedText
                font.pixelSize: 9
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }
        }
        ToolButton {
            id: speedUp
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 27
            onClicked: stepper.valueModified(Math.min(stepper.to, stepper.value + stepper.stepSize))
            background: Rectangle {
                radius: 6
                color: speedUp.pressed ? root.valueSurface : speedUp.hovered ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.10) : "transparent"
            }
            contentItem: Label {
                text: "+"
                color: root.secondaryText
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component AxisFunctionButton: ToolButton {
        id: functionButton
        property string iconKind
        property string toolTipText
        property bool emphasized: false
        property color emphasisColor: root.accent
        property color emphasisSurface: root.darkTheme ? "#173444" : "#E6F7FC"
        signal activated()

        Layout.preferredWidth: 24
        Layout.preferredHeight: 24
        onClicked: activated()
        background: Rectangle {
            radius: 6
            color: functionButton.emphasized
                   ? functionButton.emphasisSurface
                   : functionButton.hovered ? root.valueSurface : "transparent"
            border.width: functionButton.emphasized ? 1 : 0
            border.color: functionButton.emphasisColor
        }
        contentItem: Canvas {
            id: functionIcon
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = functionButton.emphasized
                                  ? functionButton.emphasisColor : root.secondaryText
                ctx.fillStyle = ctx.strokeStyle
                ctx.lineWidth = 1.45
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                if (functionButton.iconKind === "invert") {
                    ctx.moveTo(5, 8); ctx.lineTo(19, 8)
                    ctx.moveTo(15.5, 4.8); ctx.lineTo(19, 8); ctx.lineTo(15.5, 11.2)
                    ctx.moveTo(19, 16); ctx.lineTo(5, 16)
                    ctx.moveTo(8.5, 12.8); ctx.lineTo(5, 16); ctx.lineTo(8.5, 19.2)
                } else if (functionButton.iconKind === "range") {
                    ctx.moveTo(5, 12); ctx.lineTo(19, 12)
                    ctx.moveTo(5, 8); ctx.lineTo(5, 16)
                    ctx.moveTo(19, 8); ctx.lineTo(19, 16)
                    ctx.moveTo(8, 9); ctx.lineTo(5, 12); ctx.lineTo(8, 15)
                    ctx.moveTo(16, 9); ctx.lineTo(19, 12); ctx.lineTo(16, 15)
                } else if (functionButton.iconKind === "return") {
                    ctx.moveTo(4, 12); ctx.lineTo(11, 12)
                    ctx.moveTo(8, 9); ctx.lineTo(11, 12); ctx.lineTo(8, 15)
                    ctx.arc(16, 12, 4, 0, Math.PI * 2)
                    ctx.moveTo(16, 8); ctx.lineTo(16, 16)
                    ctx.moveTo(12, 12); ctx.lineTo(20, 12)
                } else if (functionButton.iconKind === "smart") {
                    ctx.moveTo(5, 5); ctx.lineTo(5, 19); ctx.lineTo(20, 19)
                    ctx.moveTo(7, 7)
                    ctx.quadraticCurveTo(12, 7, 14, 12)
                    ctx.quadraticCurveTo(16, 16, 19, 16)
                } else {
                    ctx.arc(12, 13, 6.5, Math.PI * 0.85, Math.PI * 2.15)
                    ctx.moveTo(12, 13); ctx.lineTo(15.8, 9.2)
                }
                ctx.stroke()
                if (functionButton.iconKind === "speed") {
                    ctx.beginPath(); ctx.arc(12, 13, 1.2, 0, Math.PI * 2); ctx.fill()
                } else if (functionButton.iconKind === "smart") {
                    ctx.beginPath(); ctx.arc(9.5, 7.7, 1.15, 0, Math.PI * 2); ctx.fill()
                    ctx.beginPath(); ctx.arc(16.2, 15.2, 1.15, 0, Math.PI * 2); ctx.fill()
                }
            }
            Connections {
                target: functionButton
                function onEmphasizedChanged() { functionIcon.requestPaint() }
                function onEmphasisColorChanged() { functionIcon.requestPaint() }
                function onIconKindChanged() { functionIcon.requestPaint() }
            }
        }
        AppToolTip {
            visible: functionButton.hovered
            text: functionButton.toolTipText
            darkTheme: root.darkTheme
        }
    }

    function openPopupNear(button, popup) {
        const overlay = Overlay.overlay
        if (!overlay) {
            popup.open()
            return
        }
        const point = button.mapToItem(overlay, 0, button.height + 7)
        popup.x = Math.max(8, Math.min(point.x + button.width - popup.width, overlay.width - popup.width - 8))
        const above = button.mapToItem(overlay, 0, -popup.height - 7).y
        popup.y = point.y + popup.height + 8 <= overlay.height ? point.y : Math.max(8, above)
        popup.open()
    }

    function closeAxisPopups() {
        preferredTravelPopup.close()
        returnPositionPopup.close()
        smartLimitPopup.close()
        speedPopup.close()
    }

    function commitGain() {
        root.controller.set_axis_gain(root.axisIndex, gainSlider.value)
    }

    function commitRange() {
        root.controller.set_axis_range(root.axisIndex,
                                 rangeSlider.first.value,
                                 rangeSlider.second.value)
    }

    Timer {
        interval: 40
        repeat: true
        running: gainSlider.pressed
        onTriggered: root.commitGain()
    }

    Timer {
        interval: 40
        repeat: true
        running: rangeSlider.first.pressed || rangeSlider.second.pressed
        onTriggered: root.commitRange()
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 12; spacing: 6
        RowLayout {
            Layout.fillWidth: true
            Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4; color: root.accent }
            Label { text: root.axisName; color: root.secondaryText; font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 0.7 }
            Item { Layout.fillWidth: true }
            AxisFunctionButton {
                id: invertButton
                iconKind: "invert"
                toolTipText: qsTr("Reverse %1 direction").arg(root.axisLabels[root.axisIndex])
                emphasized: root.axisInverted
                emphasisColor: "#B27DFF"
                emphasisSurface: root.darkTheme ? "#352748" : "#F0E8FC"
                onActivated: root.controller.set_axis_inverted(root.axisIndex, !root.axisInverted)
            }
            AxisFunctionButton {
                id: preferredTravelButton
                iconKind: "range"
                toolTipText: qsTr("Preferred %1 range").arg(root.axisLabels[root.axisIndex])
                emphasized: root.preferredTravelEnabled
                onActivated: {
                    root.closeAxisPopups()
                    root.openPopupNear(preferredTravelButton, preferredTravelPopup)
                }
            }
            AxisFunctionButton {
                id: returnPositionButton
                iconKind: "return"
                toolTipText: qsTr("Return %1 position").arg(root.axisLabels[root.axisIndex])
                emphasized: Math.abs(root.axisReturnPosition - 0.5) >= 0.0005
                emphasisColor: "#F1B865"
                emphasisSurface: root.darkTheme ? "#3A2D1D" : "#FFF3DD"
                onActivated: {
                    root.closeAxisPopups()
                    root.openPopupNear(returnPositionButton, returnPositionPopup)
                }
            }
            AxisFunctionButton {
                id: smartLimitButton
                iconKind: "smart"
                toolTipText: qsTr("Smart limit · axis-linked curve")
                emphasized: root.smartLimitEnabled
                emphasisColor: root.enabledGreen
                emphasisSurface: root.darkTheme ? "#173728" : "#E8F8EF"
                onActivated: {
                    root.closeAxisPopups()
                    root.openPopupNear(smartLimitButton, smartLimitPopup)
                }
            }
            AxisFunctionButton {
                id: speedButton
                iconKind: "speed"
                toolTipText: root.axisOutputEnabled ? qsTr("Output and speed limit") : qsTr("Axis output off")
                emphasized: !root.axisOutputEnabled || root.speedEnabled
                emphasisColor: root.axisOutputEnabled ? root.enabledGreen : root.disabledRed
                emphasisSurface: !root.axisOutputEnabled
                                 ? (root.darkTheme ? "#3A2223" : "#FCECEA")
                                 : (root.darkTheme ? "#173728" : "#E8F8EF")
                onActivated: {
                    root.closeAxisPopups()
                    root.openPopupNear(speedButton, speedPopup)
                }
            }
            Label { text: (root.axisValue * 100).toFixed(1) + "%"; color: root.primaryText; font.family: "Cascadia Mono"; font.pixelSize: 17; font.weight: Font.DemiBold }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 8; radius: 4; color: root.trackSurface
            Rectangle {
                width: Math.max(8, parent.width * Math.max(0, Math.min(1, root.axisValue))); height: parent.height; radius: 4
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Qt.darker(root.accent, 1.35) }
                    GradientStop { position: 1; color: root.accent }
                }
                Behavior on width { NumberAnimation { duration: 42; easing.type: Easing.OutQuad } }
            }
            Rectangle { width: 1; height: 14; y: -3; x: parent.width / 2; color: root.darkTheme ? "#718097" : "#6D7D91"; opacity: 0.55 }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("GAIN"); color: root.mutedText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.8 }
            Slider {
                id: gainSlider
                Layout.fillWidth: true; Layout.preferredHeight: 20
                from: 0.25; to: 4.0; stepSize: 0.05; value: root.gain
                onPressedChanged: if (!pressed) root.commitGain()
                background: Rectangle {
                    x: gainSlider.leftPadding
                    y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                    width: gainSlider.availableWidth; height: 4; radius: 2
                    color: root.trackSurface
                    Rectangle { width: gainSlider.visualPosition * parent.width; height: parent.height; radius: 2; color: root.accent; opacity: 0.78 }
                }
                handle: Rectangle {
                    x: gainSlider.leftPadding + gainSlider.visualPosition * (gainSlider.availableWidth - width)
                    y: gainSlider.topPadding + gainSlider.availableHeight / 2 - height / 2
                    width: 14; height: 14; radius: 7
                    color: gainSlider.pressed ? root.accent : root.primaryText
                    border.width: 2; border.color: root.accent
                }
            }
            Rectangle { Layout.preferredWidth: 48; Layout.preferredHeight: 22; radius: 7; color: root.valueSurface
                Label { anchors.centerIn: parent; text: gainSlider.value.toFixed(2) + "×"; color: root.secondaryText; font.pixelSize: 10; font.family: "Cascadia Mono" }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("RANGE"); color: root.mutedText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.8 }
            RangeSlider {
                id: rangeSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 20
                from: 0.0
                to: 1.0
                stepSize: 0.001
                first.value: root.outputMinimum
                second.value: root.outputMaximum
                first.onPressedChanged: if (!first.pressed) root.commitRange()
                second.onPressedChanged: if (!second.pressed) root.commitRange()

                background: Rectangle {
                    x: rangeSlider.leftPadding
                    y: rangeSlider.topPadding + rangeSlider.availableHeight / 2 - height / 2
                    width: rangeSlider.availableWidth
                    height: 4
                    radius: 2
                    color: root.trackSurface
                    Rectangle {
                        x: rangeSlider.first.visualPosition * parent.width
                        width: (rangeSlider.second.visualPosition - rangeSlider.first.visualPosition) * parent.width
                        height: parent.height
                        radius: 2
                        color: root.accent
                        opacity: 0.82
                    }
                }
                first.handle: Rectangle {
                    x: rangeSlider.leftPadding + rangeSlider.first.visualPosition * (rangeSlider.availableWidth - width)
                    y: rangeSlider.topPadding + rangeSlider.availableHeight / 2 - height / 2
                    width: 14; height: 14; radius: 7
                    color: rangeSlider.first.pressed ? root.accent : root.primaryText
                    border.width: 2; border.color: root.accent
                }
                second.handle: Rectangle {
                    x: rangeSlider.leftPadding + rangeSlider.second.visualPosition * (rangeSlider.availableWidth - width)
                    y: rangeSlider.topPadding + rangeSlider.availableHeight / 2 - height / 2
                    width: 14; height: 14; radius: 7
                    color: rangeSlider.second.pressed ? root.accent : root.primaryText
                    border.width: 2; border.color: root.accent
                }
            }
            Rectangle { Layout.preferredWidth: 106; Layout.preferredHeight: 22; radius: 7; color: root.valueSurface
                Label {
                    anchors.centerIn: parent
                    text: (rangeSlider.first.value * 100).toFixed(1) + "%"
                          + "–"
                          + (rangeSlider.second.value * 100).toFixed(1) + "%"
                    color: root.secondaryText
                    font.pixelSize: 9
                    font.family: "Cascadia Mono"
                }
            }
        }
    }

    Popup {
        id: preferredTravelPopup
        parent: Overlay.overlay
        width: 252
        padding: 13
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 10
            color: root.darkTheme ? "#151C27" : "#FFFFFF"
            border.color: root.darkTheme ? "#303C4E" : "#CDD6E1"
        }
        contentItem: ColumnLayout {
            spacing: 9
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("PREFERRED %1 RANGE").arg(root.axisLabels[root.axisIndex]); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                CompactToggle {
                    checked: root.preferredTravelEnabled
                    onToggled: root.controller.set_axis_preferred_travel_enabled(root.axisIndex, checked)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: root.preferredTravelEnabled
                Label { text: qsTr("MINIMUM"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                StableStepper {
                    Layout.preferredWidth: 112
                    from: 0; to: Math.max(0, root.preferredMaximum - 10); stepSize: 0.5
                    value: root.preferredMinimum
                    decimals: 1
                    suffixText: "%"
                    numberWidth: 42
                    suffixWidth: 10
                    onValueModified: (nextValue) => root.controller.set_axis_preferred_travel_range(root.axisIndex, nextValue, root.preferredMaximum)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: root.preferredTravelEnabled
                Label { text: qsTr("MAXIMUM"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                StableStepper {
                    Layout.preferredWidth: 112
                    from: Math.min(100, root.preferredMinimum + 10); to: 100; stepSize: 0.5
                    value: root.preferredMaximum
                    decimals: 1
                    suffixText: "%"
                    numberWidth: 42
                    suffixWidth: 10
                    onValueModified: (nextValue) => root.controller.set_axis_preferred_travel_range(root.axisIndex, root.preferredMinimum, nextValue)
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 1
                color: root.darkTheme ? "#293547" : "#E0E6ED"
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("STATUS"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.preferredTravelStatusText()
                    color: root.preferredTravelState === "limited" ? "#F4B860"
                         : root.preferredTravelState === "locked" ? root.enabledGreen : root.mutedText
                    font.pixelSize: 10; font.family: "Cascadia Mono"
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Maps the learned stable endpoints to this preferred interval. Extra motion keeps the remaining headroom.")
                wrapMode: Text.WordWrap
                color: root.mutedText
                font.pixelSize: 9
                lineHeight: 1.15
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Automatic gain is applied before this axis Gain control.")
                    color: root.mutedText; font.pixelSize: 8
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Button {
                    id: relearnButton
                    Layout.preferredWidth: 72; Layout.preferredHeight: 26
                    text: qsTr("Relearn")
                    enabled: root.preferredTravelEnabled
                    onClicked: root.controller.reset_axis_travel_learning(root.axisIndex)
                    background: Rectangle {
                        radius: 6
                        color: relearnButton.pressed ? root.valueSurface
                             : relearnButton.hovered ? (root.darkTheme ? "#223044" : "#E8EEF6") : "transparent"
                        border.width: 1
                        border.color: root.darkTheme ? "#3A475A" : "#C3CDD9"
                    }
                    contentItem: Label {
                        text: relearnButton.text
                        color: root.secondaryText; font.pixelSize: 9; font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    Popup {
        id: returnPositionPopup
        parent: Overlay.overlay
        width: 252
        padding: 13
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 10
            color: root.darkTheme ? "#151C27" : "#FFFFFF"
            border.color: root.darkTheme ? "#303C4E" : "#CDD6E1"
        }
        contentItem: ColumnLayout {
            spacing: 9
            Label {
                text: qsTr("RETURN %1 POSITION").arg(root.axisLabels[root.axisIndex])
                color: root.secondaryText
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.5
            }
            StableStepper {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 132
                from: 0; to: 100; stepSize: 0.5
                value: root.axisReturnPosition * 100
                decimals: 1
                suffixText: "%"
                numberWidth: 42
                suffixWidth: 10
                onValueModified: (nextValue) => root.controller.set_axis_return_position(root.axisIndex, nextValue / 100)
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Used when motion returns, before live motion, and while this axis is disabled.")
                wrapMode: Text.WordWrap
                color: root.mutedText
                font.pixelSize: 9
                lineHeight: 1.15
            }
        }
    }

    Popup {
        id: speedPopup
        parent: Overlay.overlay
        width: 240
        padding: 13
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 10
            color: root.darkTheme ? "#151C27" : "#FFFFFF"
            border.color: root.darkTheme ? "#303C4E" : "#CDD6E1"
        }
        contentItem: ColumnLayout {
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("AXIS OUTPUT"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                CompactToggle {
                    checked: root.axisOutputEnabled
                    onToggled: root.controller.set_axis_output_enabled(root.axisIndex, checked)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("SPEED LIMIT"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                CompactToggle {
                    checked: root.speedEnabled
                    onToggled: root.controller.set_axis_speed_limit_enabled(root.axisIndex, checked)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: root.speedEnabled
                Label { text: qsTr("LIMIT"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                StableStepper {
                    Layout.preferredWidth: 148
                    value: root.speedLimit
                    onValueModified: (nextValue) => root.controller.set_axis_speed_limit(root.axisIndex, nextValue)
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: (1.0 / Math.max(0.25, root.speedLimit)).toFixed(3) + " " + qsTr("s/unit")
                color: root.mutedText; font.pixelSize: 10; font.family: "Cascadia Mono"
            }
        }
    }

    Popup {
        id: smartLimitPopup
        parent: Overlay.overlay
        width: 278
        padding: 13
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property real editLowerInput: root.smartLimitLowerInput
        property real editLowerFactor: root.smartLimitLowerFactor
        property real editUpperInput: root.smartLimitUpperInput
        property real editUpperFactor: root.smartLimitUpperFactor
        function curveFactor(input) {
            if (input <= editLowerInput) return editLowerFactor
            if (input >= editUpperInput) return editUpperFactor
            const position = (input - editLowerInput) / (editUpperInput - editLowerInput)
            return editLowerFactor + (editUpperFactor - editLowerFactor) * position
        }
        function saveCurve() {
            root.controller.set_axis_smart_limit_curve(root.axisIndex,
                                                        editLowerInput, editLowerFactor,
                                                        editUpperInput, editUpperFactor)
        }
        onOpened: {
            editLowerInput = root.smartLimitLowerInput
            editLowerFactor = root.smartLimitLowerFactor
            editUpperInput = root.smartLimitUpperInput
            editUpperFactor = root.smartLimitUpperFactor
            curveCanvas.requestPaint()
        }
        onEditLowerInputChanged: curveCanvas.requestPaint()
        onEditLowerFactorChanged: curveCanvas.requestPaint()
        onEditUpperInputChanged: curveCanvas.requestPaint()
        onEditUpperFactorChanged: curveCanvas.requestPaint()
        background: Rectangle {
            radius: 10
            color: root.darkTheme ? "#151C27" : "#FFFFFF"
            border.color: root.darkTheme ? "#303C4E" : "#CDD6E1"
        }
        contentItem: ColumnLayout {
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("SMART LIMIT"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                CompactToggle {
                    checked: root.smartLimitEnabled
                    onToggled: root.controller.set_axis_smart_limit_enabled(root.axisIndex, checked)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: root.smartLimitEnabled
                Label { text: qsTr("DRIVER AXIS"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                ComboBox {
                    id: driverAxisChoice
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 28
                    model: root.axisLabels
                    currentIndex: root.smartLimitInputAxis
                    font.pixelSize: 9
                    onActivated: (index) => root.controller.set_axis_smart_limit_input(root.axisIndex, index)
                    opacity: enabled ? 1.0 : 0.5
                    contentItem: Label {
                        leftPadding: 10
                        rightPadding: 26
                        text: driverAxisChoice.displayText
                        color: root.primaryText
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                        verticalAlignment: Text.AlignVCenter
                    }
                    indicator: Canvas {
                        x: driverAxisChoice.width - width - 9
                        y: (driverAxisChoice.height - height) / 2
                        width: 10; height: 6
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = root.secondaryText
                            ctx.lineWidth = 1.4
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.moveTo(1, 1); ctx.lineTo(width / 2, height - 1); ctx.lineTo(width - 1, 1)
                            ctx.stroke()
                        }
                    }
                    background: Rectangle {
                        radius: 7
                        color: root.valueSurface
                        border.width: 1
                        border.color: driverAxisChoice.activeFocus
                                      ? root.accent
                                      : (root.darkTheme ? "#344154" : "#C8D2DE")
                    }
                    delegate: ItemDelegate {
                        id: driverAxisItem
                        required property var modelData
                        required property int index
                        width: driverAxisChoice.width - 8
                        height: 26
                        highlighted: driverAxisChoice.highlightedIndex === index
                        background: Rectangle {
                            radius: 5
                            color: driverAxisItem.highlighted
                                   ? (root.darkTheme ? "#26354A" : "#E2EAF4")
                                   : driverAxisItem.hovered ? root.valueSurface : "transparent"
                        }
                        contentItem: Label {
                            leftPadding: 7
                            text: driverAxisItem.modelData
                            color: driverAxisItem.index === driverAxisChoice.currentIndex ? root.accent : root.primaryText
                            font.pixelSize: 9
                            font.weight: driverAxisItem.index === driverAxisChoice.currentIndex ? Font.DemiBold : Font.Normal
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    popup: Popup {
                        y: driverAxisChoice.height + 4
                        width: driverAxisChoice.width
                        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
                        padding: 4
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: driverAxisChoice.popup.visible ? driverAxisChoice.delegateModel : null
                            currentIndex: driverAxisChoice.highlightedIndex
                        }
                        background: Rectangle {
                            radius: 8
                            color: root.darkTheme ? "#151C27" : "#FFFFFF"
                            border.width: 1
                            border.color: root.darkTheme ? "#344154" : "#C8D2DE"
                        }
                    }
                }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("X: %1 position  ·  Y: allowed motion").arg(root.axisLabels[root.smartLimitInputAxis])
                color: root.mutedText
                font.pixelSize: 9
            }
            Item {
                id: curveGraph
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 210
                Layout.preferredHeight: 146
                opacity: root.smartLimitEnabled ? 1.0 : 0.45
                readonly property real plotLeft: 20
                readonly property real plotRight: width - 20
                readonly property real plotTop: 14
                readonly property real plotBottom: height - 24
                readonly property real plotHeight: plotBottom - plotTop
                readonly property real plotWidth: plotRight - plotLeft

                Canvas {
                    id: curveCanvas
                    anchors.fill: parent
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        const graph = parent
                        ctx.strokeStyle = root.darkTheme ? "#2A3545" : "#E1E7EE"
                        ctx.lineWidth = 1
                        for (let step = 0; step <= 4; ++step) {
                            const x = graph.plotLeft + graph.plotWidth * step / 4
                            const y = graph.plotTop + graph.plotHeight * step / 4
                            ctx.beginPath(); ctx.moveTo(x, graph.plotTop); ctx.lineTo(x, graph.plotBottom); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(graph.plotLeft, y); ctx.lineTo(graph.plotRight, y); ctx.stroke()
                        }
                        ctx.strokeStyle = root.darkTheme ? "#64748B" : "#8290A3"
                        ctx.strokeRect(graph.plotLeft, graph.plotTop, graph.plotWidth, graph.plotHeight)
                        ctx.strokeStyle = root.smartLimitEnabled ? root.enabledGreen : root.accent
                        ctx.lineWidth = 2.2
                        ctx.lineCap = "round"
                        const lowerX = graph.plotLeft + smartLimitPopup.editLowerInput * graph.plotWidth
                        const lowerY = graph.plotBottom - smartLimitPopup.editLowerFactor * graph.plotHeight
                        const upperX = graph.plotLeft + smartLimitPopup.editUpperInput * graph.plotWidth
                        const upperY = graph.plotBottom - smartLimitPopup.editUpperFactor * graph.plotHeight
                        ctx.beginPath()
                        ctx.moveTo(graph.plotLeft, lowerY)
                        ctx.lineTo(lowerX, lowerY)
                        ctx.lineTo(upperX, upperY)
                        ctx.lineTo(graph.plotRight, upperY)
                        ctx.stroke()
                    }
                    Connections {
                        target: root
                        function onOutputSettingsChanged() { curveCanvas.requestPaint() }
                    }
                }
                Rectangle {
                    id: lowerHandle
                    width: 15; height: 15; radius: 8
                    x: parent.plotLeft + smartLimitPopup.editLowerInput * parent.plotWidth - width / 2
                    y: parent.plotBottom - smartLimitPopup.editLowerFactor * parent.plotHeight - height / 2
                    color: root.smartLimitEnabled ? root.enabledGreen : root.secondaryText
                    border.width: 2; border.color: root.darkTheme ? "#151C27" : "#FFFFFF"
                    scale: lowerArea.pressed ? 1.18 : lowerArea.containsMouse ? 1.08 : 1.0
                    Behavior on scale { NumberAnimation { duration: 90 } }
                    MouseArea {
                        id: lowerArea
                        anchors.centerIn: parent
                        width: 34; height: 34
                        enabled: root.smartLimitEnabled
                        hoverEnabled: true
                        cursorShape: Qt.SizeAllCursor
                        onPositionChanged: function(mouse) {
                            if (!pressed) return
                            const point = mapToItem(curveGraph, mouse.x, mouse.y)
                            smartLimitPopup.editLowerInput = Math.max(0, Math.min(smartLimitPopup.editUpperInput - 0.01,
                                (point.x - curveGraph.plotLeft) / curveGraph.plotWidth))
                            smartLimitPopup.editLowerFactor = Math.max(0, Math.min(1,
                                1 - (point.y - curveGraph.plotTop) / curveGraph.plotHeight))
                        }
                        onReleased: smartLimitPopup.saveCurve()
                    }
                }
                Rectangle {
                    id: upperHandle
                    width: 15; height: 15; radius: 8
                    x: parent.plotLeft + smartLimitPopup.editUpperInput * parent.plotWidth - width / 2
                    y: parent.plotBottom - smartLimitPopup.editUpperFactor * parent.plotHeight - height / 2
                    color: root.smartLimitEnabled ? root.enabledGreen : root.secondaryText
                    border.width: 2; border.color: root.darkTheme ? "#151C27" : "#FFFFFF"
                    scale: upperArea.pressed ? 1.18 : upperArea.containsMouse ? 1.08 : 1.0
                    Behavior on scale { NumberAnimation { duration: 90 } }
                    MouseArea {
                        id: upperArea
                        anchors.centerIn: parent
                        width: 34; height: 34
                        enabled: root.smartLimitEnabled
                        hoverEnabled: true
                        cursorShape: Qt.SizeAllCursor
                        onPositionChanged: function(mouse) {
                            if (!pressed) return
                            const point = mapToItem(curveGraph, mouse.x, mouse.y)
                            smartLimitPopup.editUpperInput = Math.min(1, Math.max(smartLimitPopup.editLowerInput + 0.01,
                                (point.x - curveGraph.plotLeft) / curveGraph.plotWidth))
                            smartLimitPopup.editUpperFactor = Math.max(0, Math.min(1,
                                1 - (point.y - curveGraph.plotTop) / curveGraph.plotHeight))
                        }
                        onReleased: smartLimitPopup.saveCurve()
                    }
                }
                Rectangle {
                    width: 9; height: 9; radius: 5
                    x: parent.plotLeft + root.smartLimitInputValue * parent.plotWidth - width / 2
                    y: parent.plotBottom - smartLimitPopup.curveFactor(root.smartLimitInputValue) * parent.plotHeight - height / 2
                    color: root.darkTheme ? "#151C27" : "#FFFFFF"
                    border.width: 2
                    border.color: root.smartLimitEnabled ? root.enabledGreen : root.secondaryText
                }
                Label {
                    x: Math.max(0, Math.min(parent.width - width, lowerHandle.x - width / 2 + lowerHandle.width / 2))
                    y: lowerHandle.y < 18 ? lowerHandle.y + 18 : lowerHandle.y - 14
                    text: "(" + Math.round(smartLimitPopup.editLowerInput * 100) + "%, " + Math.round(smartLimitPopup.editLowerFactor * 100) + "%)"
                    color: root.secondaryText; font.pixelSize: 8
                }
                Label {
                    x: Math.max(0, Math.min(parent.width - width, upperHandle.x - width / 2 + upperHandle.width / 2))
                    y: upperHandle.y < 18 ? upperHandle.y + 18 : upperHandle.y - 14
                    text: "(" + Math.round(smartLimitPopup.editUpperInput * 100) + "%, " + Math.round(smartLimitPopup.editUpperFactor * 100) + "%)"
                    color: root.secondaryText; font.pixelSize: 8
                }
                Label { x: 0; y: parent.plotTop - height / 2; text: "100%"; color: root.mutedText; font.pixelSize: 7 }
                Label { x: 4; y: parent.plotBottom - height / 2; text: "0%"; color: root.mutedText; font.pixelSize: 7 }
                Label { x: parent.plotLeft - width / 2; anchors.bottom: parent.bottom; text: "0%"; color: root.mutedText; font.pixelSize: 8 }
                Label { x: parent.plotRight - width / 2; anchors.bottom: parent.bottom; text: "100%"; color: root.mutedText; font.pixelSize: 8 }
            }
            RowLayout {
                Layout.fillWidth: true; enabled: root.smartLimitEnabled
                Label { text: qsTr("MODE"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                RowLayout {
                    Layout.preferredWidth: 112; spacing: 2
                    Repeater {
                        model: [{ label: qsTr("Value"), value: "value" }, { label: qsTr("Speed"), value: "speed" }]
                        delegate: Button {
                            id: modeChoice
                            required property var modelData
                            readonly property bool selected: root.smartLimitMode === modelData.value
                            Layout.fillWidth: true; Layout.preferredHeight: 27
                            text: modelData.label
                            onClicked: root.controller.set_axis_smart_limit_mode(root.axisIndex, modelData.value)
                            background: Rectangle {
                                radius: 6
                                color: modeChoice.selected
                                       ? (root.darkTheme ? "#253651" : "#DDE7F5")
                                       : (modeChoice.hovered ? root.valueSurface : "transparent")
                                border.width: modeChoice.selected ? 1 : 0
                                border.color: root.accent
                            }
                            contentItem: Label {
                                text: modeChoice.text
                                color: modeChoice.selected ? root.primaryText : root.mutedText
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: root.smartLimitMode === "value"
                enabled: root.smartLimitEnabled
                Label { text: qsTr("TARGET VALUE"); color: root.secondaryText; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.5 }
                Item { Layout.fillWidth: true }
                StableStepper {
                    Layout.preferredWidth: 112
                    from: 0; to: 100; stepSize: 1
                    value: Math.round(root.smartLimitTarget * 100)
                    decimals: 0
                    suffixText: "%"
                    numberWidth: 30
                    suffixWidth: 10
                    onValueModified: (nextValue) => root.controller.set_axis_smart_limit_target(root.axisIndex, nextValue / 100)
                }
            }
        }
    }
}
