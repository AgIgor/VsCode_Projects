from __future__ import annotations

from PySide6.QtCore import QObject, Property, QTimer, Signal, Slot
import serial
from serial.tools import list_ports

from driving_simulator import DrivingSimulator, DrivingScenario


class TelemetrySender(QObject):
    availablePortsChanged = Signal()
    portNameChanged = Signal()
    baudRateChanged = Signal()
    intervalMsChanged = Signal()
    connectedChanged = Signal()
    telemetryChanged = Signal()
    serialError = Signal(str)
    simulationStateChanged = Signal(str)

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._serial: serial.Serial | None = None
        self._available_ports: list[str] = []
        self._port_name = ""
        self._baud_rate = 115200
        self._interval_ms = 100

        self._rpm = 900.0
        self._speed = 0.0
        self._engine_load = 12.0
        self._coolant = 80.0
        self._intake = 28.0
        self._throttle = 0.0
        self._map = 35.0
        self._maf = 3.5
        self._advance = 8.0
        self._oil_temp = 85.0
        self._voltage = 13.8
        self._fuel = 75.0
        self._runtime = 120.0
        self._left_signal = False
        self._right_signal = False
        self._headlights = False
        self._check_engine = False
        self._door_open = False
        self._brake = False

        self._timer = QTimer(self)
        self._timer.setInterval(self._interval_ms)
        self._timer.timeout.connect(self.sendNow)
        self._timer.start()

        self._simulator_timer = QTimer(self)
        self._simulator_timer.setInterval(30)
        self._simulator_timer.timeout.connect(self._updateSimulator)

        self._driving_simulator = DrivingSimulator(self._onSimulatorUpdate)
        self._sim_running = False

        self.refreshPorts()

    def _clamp(self, value: float, min_value: float, max_value: float) -> float:
        return max(min_value, min(max_value, value))

    def _set_if_changed(self, attr: str, value: float, minimum: float, maximum: float) -> None:
        value = self._clamp(value, minimum, maximum)
        if abs(getattr(self, attr) - value) < 1e-6:
            return
        setattr(self, attr, value)
        self.telemetryChanged.emit()

    @Property("QStringList", notify=availablePortsChanged)
    def availablePorts(self):
        return self._available_ports

    @Property(str, notify=portNameChanged)
    def portName(self) -> str:
        return self._port_name

    @portName.setter
    def portName(self, value: str) -> None:
        if self._port_name == value:
            return
        self._port_name = value
        self.portNameChanged.emit()

    @Property(int, notify=baudRateChanged)
    def baudRate(self) -> int:
        return self._baud_rate

    @baudRate.setter
    def baudRate(self, value: int) -> None:
        if self._baud_rate == value:
            return
        self._baud_rate = value
        self.baudRateChanged.emit()

    @Property(int, notify=intervalMsChanged)
    def intervalMs(self) -> int:
        return self._interval_ms

    @intervalMs.setter
    def intervalMs(self, value: int) -> None:
        value = int(self._clamp(float(value), 20, 2000))
        if self._interval_ms == value:
            return
        self._interval_ms = value
        self._timer.setInterval(self._interval_ms)
        self.intervalMsChanged.emit()

    @Property(bool, notify=connectedChanged)
    def connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    @Property(float, notify=telemetryChanged)
    def rpm(self) -> float:
        return self._rpm

    @rpm.setter
    def rpm(self, value: float) -> None:
        self._set_if_changed("_rpm", float(value), 0.0, 12000.0)

    @Property(float, notify=telemetryChanged)
    def speed(self) -> float:
        return self._speed

    @speed.setter
    def speed(self, value: float) -> None:
        self._set_if_changed("_speed", float(value), 0.0, 320.0)

    @Property(float, notify=telemetryChanged)
    def engineLoad(self) -> float:
        return self._engine_load

    @engineLoad.setter
    def engineLoad(self, value: float) -> None:
        self._set_if_changed("_engine_load", float(value), 0.0, 100.0)

    @Property(float, notify=telemetryChanged)
    def coolant(self) -> float:
        return self._coolant

    @coolant.setter
    def coolant(self, value: float) -> None:
        self._set_if_changed("_coolant", float(value), -40.0, 215.0)

    @Property(float, notify=telemetryChanged)
    def intake(self) -> float:
        return self._intake

    @intake.setter
    def intake(self, value: float) -> None:
        self._set_if_changed("_intake", float(value), -40.0, 215.0)

    @Property(float, notify=telemetryChanged)
    def throttle(self) -> float:
        return self._throttle

    @throttle.setter
    def throttle(self, value: float) -> None:
        self._set_if_changed("_throttle", float(value), 0.0, 100.0)

    @Property(float, notify=telemetryChanged)
    def map(self) -> float:
        return self._map

    @map.setter
    def map(self, value: float) -> None:
        self._set_if_changed("_map", float(value), 0.0, 255.0)

    @Property(float, notify=telemetryChanged)
    def maf(self) -> float:
        return self._maf

    @maf.setter
    def maf(self, value: float) -> None:
        self._set_if_changed("_maf", float(value), 0.0, 655.35)

    @Property(float, notify=telemetryChanged)
    def advance(self) -> float:
        return self._advance

    @advance.setter
    def advance(self, value: float) -> None:
        self._set_if_changed("_advance", float(value), -64.0, 63.5)

    @Property(float, notify=telemetryChanged)
    def oilTemp(self) -> float:
        return self._oil_temp

    @oilTemp.setter
    def oilTemp(self, value: float) -> None:
        self._set_if_changed("_oil_temp", float(value), -40.0, 215.0)

    @Property(float, notify=telemetryChanged)
    def voltage(self) -> float:
        return self._voltage

    @voltage.setter
    def voltage(self, value: float) -> None:
        self._set_if_changed("_voltage", float(value), 0.0, 65.0)

    @Property(float, notify=telemetryChanged)
    def fuel(self) -> float:
        return self._fuel

    @fuel.setter
    def fuel(self, value: float) -> None:
        self._set_if_changed("_fuel", float(value), 0.0, 100.0)

    @Property(float, notify=telemetryChanged)
    def runtime(self) -> float:
        return self._runtime

    @runtime.setter
    def runtime(self, value: float) -> None:
        self._set_if_changed("_runtime", float(value), 0.0, 65535.0)

    @Property(bool, notify=telemetryChanged)
    def leftSignal(self) -> bool:
        return self._left_signal

    @leftSignal.setter
    def leftSignal(self, value: bool) -> None:
        value = bool(value)
        if self._left_signal == value:
            return
        self._left_signal = value
        self.telemetryChanged.emit()

    @Property(bool, notify=telemetryChanged)
    def rightSignal(self) -> bool:
        return self._right_signal

    @rightSignal.setter
    def rightSignal(self, value: bool) -> None:
        value = bool(value)
        if self._right_signal == value:
            return
        self._right_signal = value
        self.telemetryChanged.emit()

    @Property(bool, notify=telemetryChanged)
    def headlights(self) -> bool:
        return self._headlights

    @headlights.setter
    def headlights(self, value: bool) -> None:
        value = bool(value)
        if self._headlights == value:
            return
        self._headlights = value
        self.telemetryChanged.emit()

    @Property(bool, notify=telemetryChanged)
    def checkEngine(self) -> bool:
        return self._check_engine

    @checkEngine.setter
    def checkEngine(self, value: bool) -> None:
        value = bool(value)
        if self._check_engine == value:
            return
        self._check_engine = value
        self.telemetryChanged.emit()

    @Property(bool, notify=telemetryChanged)
    def doorOpen(self) -> bool:
        return self._door_open

    @doorOpen.setter
    def doorOpen(self, value: bool) -> None:
        value = bool(value)
        if self._door_open == value:
            return
        self._door_open = value
        self.telemetryChanged.emit()

    @Property(bool, notify=telemetryChanged)
    def brake(self) -> bool:
        return self._brake

    @brake.setter
    def brake(self, value: bool) -> None:
        value = bool(value)
        if self._brake == value:
            return
        self._brake = value
        self.telemetryChanged.emit()

    @Slot()
    def refreshPorts(self) -> None:
        ports = [p.device for p in list_ports.comports()]
        if ports != self._available_ports:
            self._available_ports = ports
            self.availablePortsChanged.emit()

        if not self._port_name and self._available_ports:
            self._port_name = self._available_ports[0]
            self.portNameChanged.emit()

    @Slot(result=bool)
    def connectSerial(self) -> bool:
        if self.connected:
            return True

        if not self._port_name:
            self.serialError.emit("Selecione uma porta COM")
            return False

        try:
            self._serial = serial.Serial(
                port=self._port_name,
                baudrate=self._baud_rate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0,
                write_timeout=0,
            )
            self.connectedChanged.emit()
            return True
        except Exception as exc:
            self._serial = None
            self.serialError.emit(str(exc))
            return False

    @Slot()
    def disconnectSerial(self) -> None:
        if self._serial is None:
            return
        try:
            self._serial.close()
        finally:
            self._serial = None
            self.connectedChanged.emit()

    def _build_payload(self) -> str:
        return (
            f"RPM={self._rpm:.1f},"
            f"SPEED={self._speed:.1f},"
            f"LOAD={self._engine_load:.1f},"
            f"ECT={self._coolant:.1f},"
            f"IAT={self._intake:.1f},"
            f"TPS={self._throttle:.1f},"
            f"MAP={self._map:.1f},"
            f"MAF={self._maf:.2f},"
            f"ADV={self._advance:.1f},"
            f"VBAT={self._voltage:.2f},"
            f"FUEL={self._fuel:.1f},"
            f"RUNTIME={self._runtime:.0f},"
            f"OILTEMP={self._oil_temp:.1f},"
            f"LEFT={1 if self._left_signal else 0},"
            f"RIGHT={1 if self._right_signal else 0},"
            f"HEADLIGHT={1 if self._headlights else 0},"
            f"CEL={1 if self._check_engine else 0},"
            f"DOOR={1 if self._door_open else 0},"
            f"BRAKE={1 if self._brake else 0}\n"
        )

    @Slot()
    def sendNow(self) -> None:
        if not self.connected or self._serial is None:
            return

        payload = self._build_payload().encode("utf-8")
        try:
            self._serial.write(payload)
        except Exception as exc:
            self.serialError.emit(str(exc))
            self.disconnectSerial()

    @Property(bool, notify=simulationStateChanged)
    def simulationRunning(self) -> bool:
        return self._sim_running

    @Slot(str)
    def startSimulation(self, scenario_name: str) -> None:
        """Start a driving scenario."""
        try:
            scenario = DrivingScenario(scenario_name.lower())
            self._driving_simulator.start_scenario(scenario)
            self._sim_running = True
            self._simulator_timer.start()
            self.simulationStateChanged.emit(scenario_name)
        except ValueError:
            self.serialError.emit(f"Cenário desconhecido: {scenario_name}")

    @Slot()
    def stopSimulation(self) -> None:
        """Stop the current driving scenario."""
        self._driving_simulator.stop()
        self._sim_running = False
        self._simulator_timer.stop()
        self.simulationStateChanged.emit("stopped")

    def _updateSimulator(self) -> None:
        """Update simulator state internally."""
        delta_time = 0.03
        self._driving_simulator.update(delta_time)

    def _onSimulatorUpdate(self, state: dict) -> None:
        """Callback when simulator updates telemetry."""
        self._rpm = state.get("rpm", self._rpm)
        self._speed = state.get("speed", self._speed)
        self._engine_load = state.get("engine_load", self._engine_load)
        self._coolant = state.get("coolant", self._coolant)
        self._intake = state.get("intake", self._intake)
        self._throttle = state.get("throttle", self._throttle)
        self._map = state.get("map", self._map)
        self._maf = state.get("maf", self._maf)
        self._advance = state.get("advance", self._advance)
        self._oil_temp = state.get("oil_temp", self._oil_temp)
        self._voltage = state.get("voltage", self._voltage)
        self._fuel = state.get("fuel", self._fuel)
        self._runtime = state.get("runtime", self._runtime)
        self._left_signal = bool(state.get("left_signal", self._left_signal))
        self._right_signal = bool(state.get("right_signal", self._right_signal))
        self._headlights = bool(state.get("headlights", self._headlights))
        self._check_engine = bool(state.get("check_engine", self._check_engine))
        self._door_open = bool(state.get("door_open", self._door_open))
        self._brake = bool(state.get("brake", self._brake))
        self.telemetryChanged.emit()
