# Edge IoT Telemetry Node

A multi-tasking embedded firmware system built for the ESP32 using C++ and FreeRTOS. It samples 6-axis motion data from an MPU6050 IMU over I2C, performs on-device anomaly detection (impact/freefall), and streams structured JSON payloads over MQTT via a decoupled, thread-safe queue.

---

## System Architecture

The firmware separates high-frequency sensor acquisition from variable-latency network operations across the ESP32's dual-core Xtensa processor:

* **Core 1 — `SensorTask` (Priority 2):** Deterministically samples the MPU6050 at 10 Hz using `vTaskDelayUntil`. Calculates real-time 3D vector magnitudes to flag freefall or high-G impact events, pushing the data to a FreeRTOS queue.
* **Core 0 — `NetworkTask` (Priority 1):** Manages Wi-Fi lifecycle and MQTT broker connectivity. Pulls telemetry frames from the queue, serializes them using `ArduinoJson`, and publishes updates to `workspace/telemetry/mpu6050`.
* **Thread-Safe Buffer:** A bounded FreeRTOS queue acts as a shock absorber during network reconnections, preventing I2C polling stalls and memory leaks.

---

## Hardware & Pinout

| Peripheral | ESP32 Pin | Protocol / Description |
| :--- | :--- | :--- |
| **MPU6050 VCC** | `3V3` | 3.3V Power |
| **MPU6050 GND** | `GND` | Ground |
| **MPU6050 SDA** | `GPIO 21` | I2C Data Line |
| **MPU6050 SCL** | `GPIO 22` | I2C Clock Line |

---

## Project Structure

    ├── .gitignore
    ├── diagram.json          # Wokwi circuit layout and wire map
    ├── platformio.ini        # PlatformIO build flags and dependencies
    ├── wokwi.toml            # Simulator binary paths
    └── src/
        └── main.cpp          # Application entry, FreeRTOS tasks, and queues

---

## Getting Started

### Prerequisites
* [VS Code](https://code.visualstudio.com/)
* [PlatformIO IDE Extension](https://platformio.org/)
* [Wokwi for VS Code Extension](https://wokwi.com/vscode) (optional for simulation)

### Build & Run
1. Clone this repository:
    git clone https://github.com/ananth-2022/edge-iot-telemetry.git
    cd edge-telemetry-node
   
2. Build the firmware using PlatformIO:
    pio run
   
3. **Simulation:** Press `F1`, select `Wokwi: Start Simulator`, and adjust the MPU6050 sliders to test dynamic payloads and anomaly triggers.

4. **Physical Deployment:** Connect your ESP32 via USB, update `ssid` and `password` in `src/main.cpp`, and flash:
    pio run --target upload --target monitor

---

## Sample Payload

    {
      "node_id": "edge_device_01",
      "accel_x": 0.12,
      "accel_y": -0.04,
      "accel_z": 9.81,
      "impact_alert": false
    }