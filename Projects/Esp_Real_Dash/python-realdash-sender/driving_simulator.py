from enum import Enum
from typing import Callable

class DrivingScenario(Enum):
    IDLE = "idle"
    CITY_DRIVE = "city"
    HIGHWAY = "highway"
    SPIRITED = "spirited"


class DrivingSimulator:
    """Simulates realistic driving scenarios with gear changes, curves, braking, etc."""

    def __init__(self, update_callback: Callable[[dict], None]) -> None:
        self.update_callback = update_callback
        self.time_elapsed = 0.0
        self.scenario = DrivingScenario.IDLE
        self.running = False
        self.scenario_time = 0.0

    def start_scenario(self, scenario: DrivingScenario) -> None:
        """Start a new driving scenario."""
        self.scenario = scenario
        self.running = True
        self.scenario_time = 0.0
        self.time_elapsed = 0.0

    def stop(self) -> None:
        """Stop the simulator."""
        self.running = False
        self.scenario_time = 0.0

    def update(self, delta_time: float) -> None:
        """Update simulator state. delta_time in seconds."""
        if not self.running:
            return

        self.scenario_time += delta_time

        if self.scenario == DrivingScenario.IDLE:
            self._update_idle()
        elif self.scenario == DrivingScenario.CITY_DRIVE:
            self._update_city()
        elif self.scenario == DrivingScenario.HIGHWAY:
            self._update_highway()
        elif self.scenario == DrivingScenario.SPIRITED:
            self._update_spirited()

    def _get_idle_state(self) -> dict:
        """Idle state."""
        return {
            "rpm": 700.0,
            "speed": 0.0,
            "engine_load": 5.0,
            "throttle": 0.0,
            "map": 20.0,
            "maf": 1.2,
            "advance": 25.0,
            "coolant": 92.0,
            "intake": 35.0,
            "oil_temp": 90.0,
            "voltage": 13.8,
            "fuel": 75.0,
            "runtime": 0,
            "left_signal": False,
            "right_signal": False,
            "headlights": False,
            "check_engine": False,
            "door_open": True,
            "brake": True,
        }

    def _update_idle(self) -> None:
        """Idle at parking."""
        state = self._get_idle_state()
        state["runtime"] = int(self.scenario_time)
        self.update_callback(state)

    def _update_city(self) -> None:
        """City driving: starts, traffic lights, turns, stops."""
        total_duration = 60.0

        if self.scenario_time > total_duration:
            self.stop()
            return

        t = self.scenario_time

        # 0-3s: cold start and acceleration
        if t < 3.0:
            progress = t / 3.0
            rpm = 700.0 + progress * 2500.0
            speed = progress * 15.0
            load = 8.0 + progress * 45.0
            throttle = 40.0 + progress * 20.0
            map_val = 30.0 + progress * 60.0
            maf = 2.0 + progress * 18.0
            advance = 20.0
            coolant = 92.0

        # 3-5s: cruise at ~35 km/h
        elif t < 5.0:
            rpm = 2200.0
            speed = 35.0 + (t - 3.0) * 5.0
            load = 32.0
            throttle = 25.0
            map_val = 52.0
            maf = 15.0
            advance = 22.0
            coolant = 92.0

        # 5-7s: light braking for stop
        elif t < 7.0:
            progress = (t - 5.0) / 2.0
            rpm = 2200.0 - progress * 1500.0
            speed = 40.0 - progress * 35.0
            load = 32.0 - progress * 20.0
            throttle = 25.0 - progress * 20.0
            map_val = 52.0 - progress * 30.0
            maf = 15.0 - progress * 10.0
            advance = 22.0
            coolant = 92.5

        # 7-10s: stopped at traffic light
        elif t < 10.0:
            rpm = 700.0
            speed = 0.0
            load = 5.0
            throttle = 0.0
            map_val = 20.0
            maf = 1.2
            advance = 25.0
            coolant = 93.0

        # 10-14s: strong acceleration (downshift, WOT)
        elif t < 14.0:
            progress = (t - 10.0) / 4.0
            rpm = 700.0 + progress * 5300.0
            speed = progress * 65.0
            load = 8.0 + progress * 88.0
            throttle = 100.0
            map_val = 25.0 + progress * 180.0
            maf = 2.0 + progress * 140.0
            advance = 12.0 + progress * 12.0
            coolant = 93.0 + progress * 3.0

        # 14-20s: steady cruise at ~80 km/h
        elif t < 20.0:
            rpm = 3200.0
            speed = 65.0 + (t - 14.0) * 2.5
            load = 42.0
            throttle = 28.0
            map_val = 65.0
            maf = 35.0
            advance = 20.0
            coolant = 96.0

        # 20-24s: turn (reduce speed + light braking)
        elif t < 24.0:
            progress = (t - 20.0) / 4.0
            rpm = 3200.0 - progress * 1700.0
            speed = 80.0 - progress * 50.0
            load = 42.0 - progress * 25.0
            throttle = 28.0 - progress * 18.0
            map_val = 65.0 - progress * 30.0
            maf = 35.0 - progress * 20.0
            advance = 20.0
            coolant = 96.0

        # 24-30s: cruise again at ~50 km/h
        elif t < 30.0:
            rpm = 2400.0
            speed = 30.0 + (t - 24.0) * 3.33
            load = 25.0
            throttle = 15.0
            map_val = 40.0
            maf = 12.0
            advance = 21.0
            coolant = 96.5

        # 30-35s: another stop
        elif t < 35.0:
            progress = (t - 30.0) / 5.0
            rpm = 2400.0 - progress * 1700.0
            speed = 50.0 - progress * 50.0
            load = 25.0 - progress * 15.0
            throttle = 15.0 - progress * 10.0
            map_val = 40.0 - progress * 20.0
            maf = 12.0 - progress * 8.0
            advance = 21.0
            coolant = 96.5

        # 35-38s: stopped
        elif t < 38.0:
            rpm = 700.0
            speed = 0.0
            load = 5.0
            throttle = 0.0
            map_val = 20.0
            maf = 1.2
            advance = 25.0
            coolant = 97.0

        # 38-42s: another acceleration
        elif t < 42.0:
            progress = (t - 38.0) / 4.0
            rpm = 700.0 + progress * 4500.0
            speed = progress * 70.0
            load = 8.0 + progress * 75.0
            throttle = 80.0 + progress * 20.0
            map_val = 25.0 + progress * 140.0
            maf = 2.0 + progress * 110.0
            advance = 15.0 + progress * 10.0
            coolant = 97.0

        # 42-60s: highway cruise
        else:
            rpm = 2800.0 + 200.0 * (t - 42.0) / 18.0
            speed = 70.0 + (t - 42.0) * 0.78
            load = 38.0
            throttle = 22.0
            map_val = 55.0
            maf = 28.0
            advance = 22.0
            coolant = 97.5

        intake = 25.0 + (rpm / 6000.0) * 35.0
        oil_temp = 85.0 + (coolant - 80.0) * 0.8
        blink = (int(t * 2) % 2) == 0

        left_signal = 20.0 <= t < 24.0 and blink
        right_signal = 30.0 <= t < 35.0 and blink
        brake = (5.0 <= t < 7.0) or (20.0 <= t < 24.0) or (30.0 <= t < 35.0) or (7.0 <= t < 10.0)
        door_open = (8.0 <= t < 9.0) or (36.0 <= t < 37.0)
        headlights = t >= 45.0
        check_engine = coolant > 108.0

        state = {
            "rpm": max(0.0, min(7000.0, rpm)),
            "speed": max(0.0, min(320.0, speed)),
            "engine_load": max(0.0, min(100.0, load)),
            "throttle": max(0.0, min(100.0, throttle)),
            "map": max(0.0, min(255.0, map_val)),
            "maf": max(0.0, min(655.35, maf)),
            "advance": max(-64.0, min(63.5, advance)),
            "coolant": max(-40.0, min(215.0, coolant)),
            "intake": max(-40.0, min(215.0, intake)),
            "oil_temp": max(-40.0, min(215.0, oil_temp)),
            "voltage": 13.2 + 0.6 * (load / 100.0),
            "fuel": 75.0 - (t / total_duration) * 5.0,
            "runtime": int(t),
            "left_signal": left_signal,
            "right_signal": right_signal,
            "headlights": headlights,
            "check_engine": check_engine,
            "door_open": door_open,
            "brake": brake,
        }
        self.update_callback(state)

    def _update_highway(self) -> None:
        """Highway driving: smooth cruising, gentle lane changes."""
        total_duration = 60.0

        if self.scenario_time > total_duration:
            self.stop()
            return

        t = self.scenario_time

        # 0-5s: acceleration to highway speed
        if t < 5.0:
            progress = t / 5.0
            rpm = 800.0 + progress * 3200.0
            speed = progress * 100.0
            load = 10.0 + progress * 40.0
            throttle = 30.0 + progress * 30.0

        # 5-30s: cruising at ~110 km/h
        elif t < 30.0:
            rpm = 3000.0 + 100.0 * ((t - 5.0) / 25.0)
            speed = 100.0 + (t - 5.0) * 0.4
            load = 35.0
            throttle = 28.0

        # 30-35s: lane change (gentle speed variation)
        elif t < 35.0:
            progress = (t - 30.0) / 5.0
            rpm = 3300.0 + 200.0 * abs(0.5 - progress)
            speed = 110.0 - 5.0 * abs(0.5 - progress)
            load = 35.0
            throttle = 28.0

        # 35-55s: steady cruise
        else:
            rpm = 3200.0
            speed = 105.0 + (t - 35.0) * 0.25
            load = 33.0
            throttle = 26.0

        map_val = 50.0 + load * 1.5
        maf = 20.0 + load * 2.0
        advance = 20.0 + (rpm / 6000.0) * 15.0
        coolant = 95.0 + (load / 100.0) * 8.0
        intake = 22.0 + (rpm / 6000.0) * 40.0
        oil_temp = 88.0 + (coolant - 80.0) * 0.7
        blink = (int(t * 2) % 2) == 0

        left_signal = 30.0 <= t < 32.5 and blink
        right_signal = 32.5 <= t < 35.0 and blink
        brake = False
        door_open = False
        headlights = t >= 40.0
        check_engine = False

        state = {
            "rpm": max(0.0, min(7000.0, rpm)),
            "speed": max(0.0, min(320.0, speed)),
            "engine_load": max(0.0, min(100.0, load)),
            "throttle": max(0.0, min(100.0, throttle)),
            "map": max(0.0, min(255.0, map_val)),
            "maf": max(0.0, min(655.35, maf)),
            "advance": max(-64.0, min(63.5, advance)),
            "coolant": max(-40.0, min(215.0, coolant)),
            "intake": max(-40.0, min(215.0, intake)),
            "oil_temp": max(-40.0, min(215.0, oil_temp)),
            "voltage": 13.5 + 0.3 * (load / 100.0),
            "fuel": 75.0 - (t / total_duration) * 4.0,
            "runtime": int(t),
            "left_signal": left_signal,
            "right_signal": right_signal,
            "headlights": headlights,
            "check_engine": check_engine,
            "door_open": door_open,
            "brake": brake,
        }
        self.update_callback(state)

    def _update_spirited(self) -> None:
        """Spirited driving: aggressive acceleration, hard cornering, high RPMs."""
        total_duration = 50.0

        if self.scenario_time > total_duration:
            self.stop()
            return

        t = self.scenario_time

        # 0-2s: hard launch
        if t < 2.0:
            progress = t / 2.0
            rpm = 1000.0 + progress * 5500.0
            speed = progress * 40.0
            load = 15.0 + progress * 80.0
            throttle = 100.0

        # 2-5s: first gear push
        elif t < 5.0:
            progress = (t - 2.0) / 3.0
            rpm = 6500.0 - progress * 2500.0
            speed = 40.0 + progress * 50.0
            load = 95.0
            throttle = 100.0

        # 5-8s: strong acceleration high load
        elif t < 8.0:
            progress = (t - 5.0) / 3.0
            rpm = 4000.0 + progress * 2000.0
            speed = 90.0 + progress * 40.0
            load = 92.0
            throttle = 100.0

        # 8-12s: sustained high RPM cruise
        elif t < 12.0:
            rpm = 6000.0
            speed = 130.0 + (t - 8.0) * 2.5
            load = 88.0
            throttle = 95.0

        # 12-15s: hard braking for corner
        elif t < 15.0:
            progress = (t - 12.0) / 3.0
            rpm = 6000.0 - progress * 3500.0
            speed = 140.0 - progress * 70.0
            load = 88.0 - progress * 60.0
            throttle = 95.0 - progress * 80.0

        # 15-18s: exiting corner with hard acceleration
        elif t < 18.0:
            progress = (t - 15.0) / 3.0
            rpm = 2500.0 + progress * 3500.0
            speed = 70.0 + progress * 50.0
            load = 28.0 + progress * 65.0
            throttle = 100.0

        # 18-30s: steady high-speed cruise
        elif t < 30.0:
            rpm = 5200.0 + 400.0 * ((t - 18.0) / 12.0)
            speed = 120.0 + (t - 18.0) * 1.67
            load = 75.0
            throttle = 85.0

        # 30-35s: another aggressive acceleration
        elif t < 35.0:
            progress = (t - 30.0) / 5.0
            rpm = 5600.0 + progress * 1400.0
            speed = 140.0 + progress * 30.0
            load = 80.0 + progress * 15.0
            throttle = 100.0

        # 35-50s: high speed steady
        else:
            rpm = 5800.0 + 200.0 * ((t - 35.0) / 15.0)
            speed = 170.0 + (t - 35.0) * 0.27
            load = 85.0
            throttle = 90.0

        map_val = 80.0 + load * 1.2
        maf = 50.0 + load * 2.5
        advance = 12.0 + (rpm / 6000.0) * 15.0
        coolant = 92.0 + (load / 100.0) * 15.0
        intake = 28.0 + (rpm / 6000.0) * 50.0
        oil_temp = 88.0 + (coolant - 80.0) * 0.9

        left_signal = False
        right_signal = False
        brake = 12.0 <= t < 15.0
        door_open = False
        headlights = t >= 35.0
        check_engine = coolant > 104.0

        state = {
            "rpm": max(0.0, min(7000.0, rpm)),
            "speed": max(0.0, min(320.0, speed)),
            "engine_load": max(0.0, min(100.0, load)),
            "throttle": max(0.0, min(100.0, throttle)),
            "map": max(0.0, min(255.0, map_val)),
            "maf": max(0.0, min(655.35, maf)),
            "advance": max(-64.0, min(63.5, advance)),
            "coolant": max(-40.0, min(215.0, coolant)),
            "intake": max(-40.0, min(215.0, intake)),
            "oil_temp": max(-40.0, min(215.0, oil_temp)),
            "voltage": 13.0 + 0.8 * (load / 100.0),
            "fuel": 75.0 - (t / total_duration) * 15.0,
            "runtime": int(t),
            "left_signal": left_signal,
            "right_signal": right_signal,
            "headlights": headlights,
            "check_engine": check_engine,
            "door_open": door_open,
            "brake": brake,
        }
        self.update_callback(state)
