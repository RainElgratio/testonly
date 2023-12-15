#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <DHT.h>

// State constants
#define DHTPIN 4
#define DHTTYPE DHT11
// #DEFINE DTHTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define STOP 1
#define WATERING 2
#define IDLE 3
#define LED_BUILTIN 2

#define GAUGE_VPIN_temp V4
#define GAUGE_VPIN_humid V2

// Timer period in milliseconds
#define TIMER_PERIOD 1000 // 1 second

// Global variables
double temperature = 0.00;
double humidity = 100.00;

int virtualpin = 0;
int state = STOP;
int prevState = STOP; // Menyimpan status sebelumnya
int timeStart = 0;
int maxDuration = 300; // 5 minutes in seconds
TimerHandle_t timer;


/* Fill-in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID     "TMPL6Jw-SP0dn"
#define BLYNK_TEMPLATE_NAME   "IoTprojectTemplate"
#define BLYNK_AUTH_TOKEN      "w-6AgqBbJ5mKQD0yfGlzuJcjQb0_ZbBW"

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial


#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "thisIsWiFi";
char pass[] = "";

// =======================================================================================================

void formatTime(int timeInSeconds, char *formattedTime) {
  int minutes = timeInSeconds / 60;
  int seconds = timeInSeconds % 60;
  sprintf(formattedTime, "%02d:%02d/%02d:%02d", minutes, seconds, maxDuration / 60, maxDuration % 60);
}

void onTimer(TimerHandle_t xTimer) {
  if (state == WATERING) {
    timeStart++;
    char formattedTime[12];
    formatTime(timeStart, formattedTime);
    Serial.print("Time: ");
    Serial.println(formattedTime);
    if (timeStart >= maxDuration) {
      state = STOP;
      timeStart = 0;
    }
  } else {
    if (state != prevState) {
      Serial.println(state == IDLE ? "Idle" : "Stop");
      prevState = state; // Memperbarui status sebelumnya
    }
  }
}

BLYNK_WRITE(V0) {
    if (param.asInt() == 1) {virtualpin = WATERING;}     
}

BLYNK_WRITE(V1) {
    if (param.asInt() == 1) {virtualpin = STOP;}
}

void TaskSerial(void *pvParameters) {
  Blynk.syncVirtual(V0);
  while (1) {
    if (Serial.available() > 0 || virtualpin != 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      if (input == "stop" || virtualpin == STOP) {
        state = STOP;
        timeStart = 0;
        Serial.println("Stop");
      } else if (input == "water" || (temperature > 23.00 || humidity < 45.00)) {
        state = WATERING;
        Serial.println("WATERING......................");
        Blynk.virtualWrite(V3, "Watering");
      } else if (input == "idle" || (temperature <= 23.00 && humidity >= 45.00)) {
        state = IDLE;
        Serial.println("Idle");
        Blynk.virtualWrite(V3, "Idle");
      }
      while (virtualpin != STOP) {
        Blynk.syncVirtual(V0);
        Blynk.syncVirtual(V1);
        vTaskDelay(30 * 1000);
      }
    }
    virtualpin = 0;
  }
}

void vBlynkTask(void *pvParam) {
  while (true) {
    Blynk.run(); // Run Blynk tasks
    vTaskDelay(10);
  }
}

void TaskDHT11sensor(void *pvParameters) {
  while(1) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    Blynk.virtualWrite(GAUGE_VPIN_humid, humidity);
    Blynk.virtualWrite(GAUGE_VPIN_temp, temperature);
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" °C, Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
    vTaskDelay(2000);
  }
}

void TaskLED(void *pvParameters) {
  pinMode(LED_BUILTIN, OUTPUT);
  while (1) {
    if (state == WATERING) {
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }
    vTaskDelay(10);
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // You can also specify server:
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, "blynk.cloud", 80);
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, IPAddress(192,168,1,100), 8080);

  xTaskCreatePinnedToCore(vBlynkTask, "BlynkTask", 2048, NULL, 255, NULL, 0);
  xTaskCreatePinnedToCore(TaskDHT11sensor, "DHT11read", 4096, NULL, 1, NULL, 0);
  xTaskCreate(TaskSerial, "SerialTask", 4096, NULL, 3, NULL);
  xTaskCreate(TaskLED, "LEDTask", 2048, NULL, 2, NULL);
  
  // Create and start the software timer
  timer = xTimerCreate("Timer", pdMS_TO_TICKS(TIMER_PERIOD), pdTRUE, 0, onTimer);
  if (timer != NULL) {
    xTimerStart(timer, 0);
  }
}

void loop() {
  // Nothing to do in the main loop
  vTaskDelete(NULL);
}