#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// Define the data packet passed between tasks
typedef struct {
    float accelX;
    float accelY;
    float accelZ;
} TelemetryData;

// Thread-safe buffer handle
QueueHandle_t telemetryQueue;

// Task 1: Sample the sensor at a deterministic 10 Hz
void sensorTask(void *pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100 ms interval
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        TelemetryData sample;
        sample.accelX = a.acceleration.x;
        sample.accelY = a.acceleration.y;
        sample.accelZ = a.acceleration.z;

        // Push to queue; drop the sample if full to prevent blocking
        if (xQueueSend(telemetryQueue, &sample, (TickType_t)0) != pdPASS) {
            Serial.println("[SensorTask] Queue full. Sample dropped.");
        }

        // Wait precisely 100 ms relative to last wake time
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Task 2: Consume data from the queue
void networkTask(void *pvParameters) {
    TelemetryData receivedData;

    for (;;) {
        // Wait indefinitely until data is available in the queue
        if (xQueueReceive(telemetryQueue, &receivedData, portMAX_DELAY) == pdPASS) {
            Serial.print("Data Pulled -> X: ");
            Serial.print(receivedData.accelX);
            Serial.print(" | Y: ");
            Serial.println(receivedData.accelY);
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    // Initialize I2C (SDA=21, SCL=22)
    Wire.begin(21, 22);

    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip");
        while (1) { delay(10); }
    }
    Serial.println("MPU6050 initialized.");

    // Create a queue that holds up to 10 telemetry packets
    telemetryQueue = xQueueCreate(10, sizeof(TelemetryData));

    if (telemetryQueue != NULL) {
        // Pin the sensor task to Core 1
        xTaskCreatePinnedToCore(
            sensorTask,
            "Sensor_Task",
            2048,           // Stack size in words
            NULL,
            2,              // Higher priority than network handling
            NULL,
            1               // Core 1
        );
        Serial.println("Sensor task started.");

        // Pin the network task to Core 0 (separating it from the sensor on Core 1)
        xTaskCreatePinnedToCore(
            networkTask,
            "Network_Task",
            3072,           // Slightly larger stack for future networking
            NULL,
            1,              // Lower priority than the sensor task
            NULL,
            0               // Core 0
        );
        Serial.println("Network task started.");
    }
}

void loop() {
    // Left empty - FreeRTOS handles execution
}