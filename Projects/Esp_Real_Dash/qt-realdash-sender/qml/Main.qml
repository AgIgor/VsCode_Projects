import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 860
    height: 620
    visible: true
    title: "RealDash ESP32 Sender"

    property string lastError: ""

    Connections {
        target: telemetrySender
        function onSerialError(message) {
            root.lastError = message
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        GroupBox {
            title: "Serial ESP32"
            Layout.fillWidth: true

            RowLayout {
                anchors.fill: parent
                spacing: 8

                ComboBox {
                    id: portCombo
                    Layout.preferredWidth: 200
                    model: telemetrySender.availablePorts
                    onActivated: telemetrySender.portName = currentText
                }

                Button {
                    text: "Atualizar portas"
                    onClicked: telemetrySender.refreshPorts()
                }

                Button {
                    text: telemetrySender.connected ? "Desconectar" : "Conectar"
                    onClicked: {
                        if (telemetrySender.connected)
                            telemetrySender.disconnectSerial()
                        else {
                            telemetrySender.portName = portCombo.currentText
                            telemetrySender.connectSerial()
                        }
                    }
                }

                Label {
                    text: telemetrySender.connected ? "Conectado" : "Desconectado"
                    color: telemetrySender.connected ? "#2e7d32" : "#c62828"
                }

                Item { Layout.fillWidth: true }

                Label { text: "Intervalo (ms)" }
                SpinBox {
                    from: 20
                    to: 1000
                    value: telemetrySender.intervalMs
                    onValueModified: telemetrySender.intervalMs = value
                }
            }
        }

        GroupBox {
            title: "Telemetria"
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridLayout {
                columns: 3
                anchors.fill: parent
                columnSpacing: 16
                rowSpacing: 10

                Label { text: "RPM" }
                Slider {
                    from: 0
                    to: 12000
                    value: telemetrySender.rpm
                    onMoved: telemetrySender.rpm = value
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0
                    to: 12000
                    value: telemetrySender.rpm
                    onValueModified: telemetrySender.rpm = value
                }

                Label { text: "Velocidade (km/h)" }
                Slider {
                    from: 0
                    to: 320
                    value: telemetrySender.speed
                    onMoved: telemetrySender.speed = value
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0
                    to: 320
                    value: telemetrySender.speed
                    onValueModified: telemetrySender.speed = value
                }

                Label { text: "Coolant (°C)" }
                Slider {
                    from: -40
                    to: 215
                    value: telemetrySender.coolant
                    onMoved: telemetrySender.coolant = value
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: -40
                    to: 215
                    value: telemetrySender.coolant
                    onValueModified: telemetrySender.coolant = value
                }

                Label { text: "Throttle (%)" }
                Slider {
                    from: 0
                    to: 100
                    value: telemetrySender.throttle
                    onMoved: telemetrySender.throttle = value
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0
                    to: 100
                    value: telemetrySender.throttle
                    onValueModified: telemetrySender.throttle = value
                }

                Label { text: "MAP (kPa)" }
                Slider {
                    from: 0
                    to: 255
                    value: telemetrySender.map
                    onMoved: telemetrySender.map = value
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0
                    to: 255
                    value: telemetrySender.map
                    onValueModified: telemetrySender.map = value
                }

                Label { text: "Bateria (V x100)" }
                Slider {
                    from: 0
                    to: 6500
                    value: telemetrySender.voltage * 100
                    onMoved: telemetrySender.voltage = value / 100
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0
                    to: 6500
                    value: telemetrySender.voltage * 100
                    onValueModified: telemetrySender.voltage = value / 100
                }

                Label { text: "Combustível (%)" }
                Slider {
                    from: 0
                    to: 100
                    value: telemetrySender.fuel
                    onMoved: telemetrySender.fuel = value
                    Layout.fillWidth: true
                }
                SpinBox {
                    from: 0
                    to: 100
                    value: telemetrySender.fuel
                    onValueModified: telemetrySender.fuel = value
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: "Enviar agora"
                onClicked: telemetrySender.sendNow()
            }

            Label {
                text: root.lastError.length > 0 ? ("Erro: " + root.lastError) : ""
                color: "#c62828"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
    }
}
