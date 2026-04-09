import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1000
    height: 750
    visible: true
    title: "RealDash ESP32 - Simulador Realista de Condução"

    property string lastError: ""
    property color primaryColor: "#1976d2"
    property color accentColor: "#ff6f00"
    property color backgroundColor: "#fafafa"
    property color cardColor: "#ffffff"
    property color textPrimary: "#212121"
    property color textSecondary: "#757575"
    property color successColor: "#4caf50"
    property color errorColor: "#f44336"

    Connections {
        target: telemetrySender
        function onSerialError(message) { root.lastError = message }
        function onSimulationStateChanged(state) { }
    }

    palette {
        buttonText: root.textPrimary
        base: root.backgroundColor
    }

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // ===== TOP BAR: Connection =====
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 65
            color: root.primaryColor
            radius: 8
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                Text {
                    text: "🚗 ESP32 ELM327 Simulador"
                    color: "white"
                    font.pointSize: 13
                    font.bold: true
                }

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: telemetrySender.connected ? root.successColor : root.errorColor
                }

                Text {
                    text: telemetrySender.connected ? "CONECTADO" : "DESCONECTADO"
                    color: "white"
                    font.pointSize: 10
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                ComboBox {
                    id: portCombo
                    Layout.preferredWidth: 140
                    model: telemetrySender.availablePorts
                    onActivated: telemetrySender.portName = portCombo.currentText
                }

                Button {
                    text: telemetrySender.connected ? "Desconectar" : "Conectar"
                    onClicked: {
                        if (telemetrySender.connected) {
                            telemetrySender.disconnectSerial()
                        } else {
                            telemetrySender.portName = portCombo.currentText
                            telemetrySender.connectSerial()
                        }
                    }
                }

                Button {
                    text: "🔄"
                    onClicked: telemetrySender.refreshPorts()
                }
            }
        }

        // ===== SIMULATION CONTROL =====
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 90
            color: root.cardColor
            radius: 8
            border.color: root.accentColor
            border.width: 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                Text {
                    text: telemetrySender.simulationRunning ? "▶️ Simulação em Andamento - Cenários Realistas" : "📊 Simulação Automática - Escolha um Cenário"
                    font.pointSize: 10
                    font.bold: true
                    color: telemetrySender.simulationRunning ? root.accentColor : root.textPrimary
                }

                RowLayout {
                    spacing: 6
                    Button {
                        text: "🏁 Marcha Lenta"
                        onClicked: telemetrySender.startSimulation("idle")
                    }
                    Button {
                        text: "🚗 Cidade (Saídas, Trânsito)"
                        onClicked: telemetrySender.startSimulation("city")
                    }
                    Button {
                        text: "🛣️  Rodovia (Cruise Suave)"
                        onClicked: telemetrySender.startSimulation("highway")
                    }
                    Button {
                        text: "🏎️  Esportivo (Performance)"
                        onClicked: telemetrySender.startSimulation("spirited")
                    }
                    Button {
                        text: "⏹️  Parar"
                        enabled: telemetrySender.simulationRunning
                        onClicked: telemetrySender.stopSimulation()
                    }
                    Label { text: "ms:" }
                    SpinBox {
                        from: 20
                        to: 1000
                        value: telemetrySender.intervalMs
                        onValueModified: telemetrySender.intervalMs = value
                    }
                }
            }
        }

        // ===== SCROLLABLE CONTENT =====
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: Math.max(root.width - 30, 500)
                spacing: 8

                // Motor
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 280
                    color: root.cardColor
                    radius: 8
                    border.color: root.primaryColor
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            text: "🔧 Motor"
                            font.pointSize: 11
                            font.bold: true
                            color: root.primaryColor
                        }

                        ColumnLayout {
                            spacing: 6

                            RowLayout {
                                spacing: 8
                                Text { text: "RPM"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 12000; value: telemetrySender.rpm; onMoved: telemetrySender.rpm = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.rpm.toFixed(0); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 12000; value: Math.round(telemetrySender.rpm); onValueModified: telemetrySender.rpm = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "Velocidade (km/h)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 320; value: telemetrySender.speed; onMoved: telemetrySender.speed = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.speed.toFixed(0); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 320; value: Math.round(telemetrySender.speed); onValueModified: telemetrySender.speed = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "Carga Motor (%)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 100; value: telemetrySender.engineLoad; onMoved: telemetrySender.engineLoad = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.engineLoad.toFixed(1); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 100; value: Math.round(telemetrySender.engineLoad); onValueModified: telemetrySender.engineLoad = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "Coolant ECT (°C)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: -40; to: 215; value: telemetrySender.coolant; onMoved: telemetrySender.coolant = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.coolant.toFixed(1); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: -40; to: 215; value: Math.round(telemetrySender.coolant); onValueModified: telemetrySender.coolant = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "Intake IAT (°C)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: -40; to: 215; value: telemetrySender.intake; onMoved: telemetrySender.intake = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.intake.toFixed(1); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: -40; to: 215; value: Math.round(telemetrySender.intake); onValueModified: telemetrySender.intake = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "Avanço Ignição (°)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: -64; to: 63; value: telemetrySender.advance; onMoved: telemetrySender.advance = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.advance.toFixed(1); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: -64; to: 63; value: Math.round(telemetrySender.advance); onValueModified: telemetrySender.advance = value; Layout.preferredWidth: 70 }
                            }
                        }
                    }
                }

                // Ar/Combustível
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                    color: root.cardColor
                    radius: 8
                    border.color: root.primaryColor
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            text: "💨 Ar e Combustível"
                            font.pointSize: 11
                            font.bold: true
                            color: root.primaryColor
                        }

                        ColumnLayout {
                            spacing: 6

                            RowLayout {
                                spacing: 8
                                Text { text: "Throttle TPS (%)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 100; value: telemetrySender.throttle; onMoved: telemetrySender.throttle = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.throttle.toFixed(1); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 100; value: Math.round(telemetrySender.throttle); onValueModified: telemetrySender.throttle = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "MAP (kPa)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 255; value: telemetrySender.map; onMoved: telemetrySender.map = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.map.toFixed(1); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 255; value: Math.round(telemetrySender.map); onValueModified: telemetrySender.map = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "MAF (g/s)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 655; value: telemetrySender.maf; onMoved: telemetrySender.maf = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.maf.toFixed(2); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 655; value: Math.round(telemetrySender.maf); onValueModified: telemetrySender.maf = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "Combustível (%)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 100; value: telemetrySender.fuel; onMoved: telemetrySender.fuel = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.fuel.toFixed(1); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 100; value: Math.round(telemetrySender.fuel); onValueModified: telemetrySender.fuel = value; Layout.preferredWidth: 70 }
                            }
                        }
                    }
                }

                // Elétrico
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    color: root.cardColor
                    radius: 8
                    border.color: root.primaryColor
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            text: "⚡ Sistema Elétrico"
                            font.pointSize: 11
                            font.bold: true
                            color: root.primaryColor
                        }

                        ColumnLayout {
                            spacing: 6

                            RowLayout {
                                spacing: 8
                                Text { text: "Bateria (V)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 65; value: telemetrySender.voltage; onMoved: telemetrySender.voltage = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.voltage.toFixed(2); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 65; value: Math.round(telemetrySender.voltage); onValueModified: telemetrySender.voltage = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "Temp. Óleo (°C)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: -40; to: 215; value: telemetrySender.oilTemp; onMoved: telemetrySender.oilTemp = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.oilTemp.toFixed(1); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: -40; to: 215; value: Math.round(telemetrySender.oilTemp); onValueModified: telemetrySender.oilTemp = value; Layout.preferredWidth: 70 }
                            }

                            RowLayout {
                                spacing: 8
                                Text { text: "Runtime (s)"; font.pointSize: 9; color: root.textSecondary; Layout.preferredWidth: 150 }
                                Slider { from: 0; to: 65535; value: telemetrySender.runtime; onMoved: telemetrySender.runtime = value; Layout.fillWidth: true }
                                Text { text: telemetrySender.runtime.toFixed(0); font.pointSize: 9; color: root.textPrimary; font.bold: true; Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight }
                                SpinBox { from: 0; to: 65535; value: Math.round(telemetrySender.runtime); onValueModified: telemetrySender.runtime = value; Layout.preferredWidth: 70 }
                            }
                        }
                    }
                }

                // Sinalizações
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 165
                    color: root.cardColor
                    radius: 8
                    border.color: root.primaryColor
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        Text {
                            text: "💡 Sinalizações"
                            font.pointSize: 11
                            font.bold: true
                            color: root.primaryColor
                        }

                        RowLayout {
                            spacing: 14
                            Switch {
                                text: "Seta Esquerda"
                                checked: telemetrySender.leftSignal
                                onToggled: telemetrySender.leftSignal = checked
                            }
                            Switch {
                                text: "Seta Direita"
                                checked: telemetrySender.rightSignal
                                onToggled: telemetrySender.rightSignal = checked
                            }
                            Switch {
                                text: "Faróis"
                                checked: telemetrySender.headlights
                                onToggled: telemetrySender.headlights = checked
                            }
                        }

                        RowLayout {
                            spacing: 14
                            Switch {
                                text: "Luz Injeção (MIL)"
                                checked: telemetrySender.checkEngine
                                onToggled: telemetrySender.checkEngine = checked
                            }
                            Switch {
                                text: "Porta Aberta"
                                checked: telemetrySender.doorOpen
                                onToggled: telemetrySender.doorOpen = checked
                            }
                            Switch {
                                text: "Freio"
                                checked: telemetrySender.brake
                                onToggled: telemetrySender.brake = checked
                            }
                        }

                        RowLayout {
                            spacing: 16
                            Text { text: telemetrySender.leftSignal ? "⬅️" : "⬅"; font.pointSize: 16 }
                            Text { text: telemetrySender.rightSignal ? "➡️" : "➡"; font.pointSize: 16 }
                            Text { text: telemetrySender.headlights ? "💡" : "🔅"; font.pointSize: 16 }
                            Text { text: telemetrySender.checkEngine ? "🟠 CEL" : "⚪ CEL"; font.pointSize: 12; font.bold: true }
                            Text { text: telemetrySender.doorOpen ? "🚪 Aberta" : "🚪 Fechada"; font.pointSize: 12; font.bold: true }
                            Text { text: telemetrySender.brake ? "🛑 Freando" : "🟢 Livre"; font.pointSize: 12; font.bold: true }
                        }
                    }
                }

                // Status
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: root.lastError.length > 0 ? "#ffebee" : "#e8f5e9"
                    radius: 6
                    Text {
                        anchors.fill: parent
                        anchors.margins: 8
                        text: root.lastError.length > 0 ? ("❌ " + root.lastError) : "✅ Sistema OK"
                        color: root.lastError.length > 0 ? root.errorColor : root.successColor
                        font.pointSize: 10
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
