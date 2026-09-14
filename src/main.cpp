#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

Adafruit_MPU6050 mpu;

// replace with your wifi's ssid, password, and your ip address
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";

WiFiClient espClient;
PubSubClient client(espClient);

typedef struct {
    float accelX;
    float accelY;
    float accelZ;
    bool impactAlert;
} TelemetryData;

QueueHandle_t telemetryQueue;

void sensorTask(void *pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(100); 
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        TelemetryData sample;
        sample.accelX = a.acceleration.x;
        sample.accelY = a.acceleration.y;
        sample.accelZ = a.acceleration.z;

        float magnitude = sqrt(pow(sample.accelX, 2) + pow(sample.accelY, 2) + pow(sample.accelZ, 2));
        sample.impactAlert = (magnitude > 15.0 || magnitude < 3.0);

        if (xQueueSend(telemetryQueue, &sample, (TickType_t)0) != pdPASS) {
            Serial.println("[SensorTask] Queue full. Sample dropped.");
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void networkTask(void *pvParameters) {
    WiFi.begin(ssid, password, 6);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");

    client.setServer(mqtt_server, 1883);
    TelemetryData receivedData;

    for (;;) {
        if (!client.connected()) {
            Serial.println("Connecting to MQTT...");
            String clientId = "ESP32Client-";
            clientId += String(random(0, 0xffff), HEX);
            
            if (client.connect(clientId.c_str())) {
                Serial.println("MQTT Connected!");
            } else {
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue; 
            }
        }
        client.loop(); 

        if (xQueueReceive(telemetryQueue, &receivedData, pdMS_TO_TICKS(50)) == pdPASS) {
            
            // Serialize to JSON
            JsonDocument doc;
            doc["node_id"] = "edge_device_01";
            doc["accel_x"] = receivedData.accelX;
            doc["accel_y"] = receivedData.accelY;
            doc["accel_z"] = receivedData.accelZ;
            doc["impact_alert"] = receivedData.impactAlert;

            char jsonBuffer[256];
            serializeJson(doc, jsonBuffer);

            client.publish("workspace/telemetry/mpu6050", jsonBuffer);
            
            Serial.print("Published: ");
            Serial.println(jsonBuffer);
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Wire.begin(21, 22);
    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip");
        while (1) { delay(10); }
    }
    
    telemetryQueue = xQueueCreate(10, sizeof(TelemetryData));

    if (telemetryQueue != NULL) {
        xTaskCreatePinnedToCore(sensorTask, "Sensor_Task", 2048, NULL, 2, NULL, 1);
        xTaskCreatePinnedToCore(networkTask, "Network_Task", 4096, NULL, 1, NULL, 0);
    }
}

void loop() {}