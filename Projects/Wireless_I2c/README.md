# Wireless I2C bridge for ESP32 (ESP-NOW)

This workspace contains two example subprojects showing how to tunnel I2C operations over ESP-NOW between two ESP32 boards.

- `esp32_i2c_bridge_master`: sends I2C read/write requests to a remote slave via ESP-NOW.
- `esp32_i2c_bridge_slave`: receives requests, performs the local I2C operation and responds.

How it works
- The master sends a small packed request (cmd, i2c_addr, reg, len, data...) to the slave MAC via ESP-NOW.
- The slave performs the requested I2C transaction on its local `Wire` bus and replies with status/data.

Quick start (PowerShell)
1. Build & flash the SLAVE to the ESP32 that has the physical I2C device(s):

```powershell
cd esp32_i2c_bridge_slave
platformio run -t upload -e esp32_slave
```

2. Open Serial Monitor (115200) for the slave and note the printed MAC address.

3. Build & flash the MASTER to the other ESP32:

```powershell
cd ..\esp32_i2c_bridge_master
platformio run -t upload -e esp32_master
```

4. Open Serial Monitor (115200) for the master. Set the peer with the slave MAC (example):

```
peer AA:BB:CC:DD:EE:FF
```

5. Do a read from an I2C device at address `0x76`, register `0xD0`, length 1:

```
r 0x76 0xD0 1
```

6. Do a write to device `0x40`, register `0x00`, bytes `0x12 0x34`:

```
w 0x40 0x00 0x12 0x34
```

Notes & customization
- Default I2C pins used are SDA=21, SCL=22. Change in `main.cpp` if needed.
- ESP-NOW requires adding peers; the master adds the slave peer after you type the `peer` command.
- Payloads are limited by ESP-NOW maximum size; keep I2C transfers small (sensor registers, short bursts).
- This example is a starting point — add checksums, timeouts, retries, encryption or pairing for production.

If you want, I can:
- Add automatic peer discovery (broadcast + reply) so you don't have to copy MACs.
- Add a small test harness that polls a specific sensor and prints results.
