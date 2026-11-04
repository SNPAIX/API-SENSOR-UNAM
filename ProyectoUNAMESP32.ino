#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <HTTPClient.h> // NUEVO: Librería para peticiones web
#include "time.h"

// Handles de las tareas
TaskHandle_t Task_Sensores = NULL;
TaskHandle_t Task_Antena = NULL;
TaskHandle_t Task_Hora = NULL;
TaskHandle_t Task_Empaquetado = NULL;

// Handles para las colas
QueueHandle_t QEmpaquetador;

// Variables
Adafruit_BME280 bme; 
TinyGPSPlus gps; 

typedef struct {
    float Temp;     
    float Pres;     
    float Hume;     
    float Lati;     
    float Long;     
    float Alti;     
    float Velo;     
} PaqueteSensores_t;

typedef struct  {
    struct tm timeinfo;  
} PaqueteTiempo_t;

typedef struct {
    uint8_t OrigenId;    
    union {
        PaqueteSensores_t dataSensores;   
        PaqueteTiempo_t   dataTiempo;     
    } payload;
} QueueMessage_t;

// Configuración de red
const char* ssid     = "TU_WIFI";       // CAMBIAR POR EL WIFI QUE USARÁ LA SONDA/DRON
const char* password = "TU_PASSWORD";   // CAMBIAR POR LA CONTRASEÑA
const char* ntpServer = "pool.ntp.org";

const long  gmtOffset_sec = -21600; // UTC-6
const int   daylightOffset_sec = 0;

void Sensores(void *parameter) {
  for(;;) {
    QueueMessage_t msg;
    msg.OrigenId = 1;
  
    while (Serial1.available() > 0) { 
      gps.encode(Serial1.read()); 
    }

    if (gps.location.isValid()) {
      msg.payload.dataSensores.Lati = gps.location.lat();
      msg.payload.dataSensores.Long = gps.location.lng();
      msg.payload.dataSensores.Alti = gps.altitude.meters(); // Altura sobre nivel medio del mar
      msg.payload.dataSensores.Velo = gps.speed.kmph();
    } else {
      msg.payload.dataSensores.Lati = 0.0;
      msg.payload.dataSensores.Long = 0.0;
      msg.payload.dataSensores.Alti = 0.0;
      msg.payload.dataSensores.Velo = 0.0;
    }

    // LECTURA DE SENSORES REALES (Descomentados para la prueba real)
    msg.payload.dataSensores.Temp = bme.readTemperature();
    msg.payload.dataSensores.Pres = (bme.readPressure() / 100.0F);
    msg.payload.dataSensores.Hume = bme.readHumidity();

    xQueueSend(QEmpaquetador, &msg, portMAX_DELAY); 
    
    // CAMBIO: Frecuencia de 10 segundos
    vTaskDelay(pdMS_TO_TICKS(10000)); 
  }
}

void Antena(void *parameter) {
  for(;;) {
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void TomarHora(void *parameter) {
  for(;;) {
    QueueMessage_t msg;
    msg.OrigenId = 2;

    if(getLocalTime(&msg.payload.dataTiempo.timeinfo)){
      xQueueSend(QEmpaquetador, &msg, portMAX_DELAY);
    }
    
    // CAMBIO: Frecuencia de 10 segundos
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void Empaquetado(void *parameter){
  QueueMessage_t receivedMsg;
  PaqueteSensores_t latestDataSensores;
  PaqueteTiempo_t   latestDataTiempo;
  bool BanderaSensores = 0;
  bool BanderaTiempo = 0;

  for(;;) {
    if (xQueueReceive(QEmpaquetador, &receivedMsg, portMAX_DELAY) == pdPASS) {
            
      if (receivedMsg.OrigenId == 1) {
          latestDataSensores = receivedMsg.payload.dataSensores; 
          BanderaSensores = 1;
      } 
      else if (receivedMsg.OrigenId == 2) {
          latestDataTiempo = receivedMsg.payload.dataTiempo;
          BanderaTiempo = 1;
      }

      if ((BanderaTiempo & BanderaSensores) == 1) {                
        
        // CAMBIO: Enviar JSON a la IP de la UNAM por HTTP
        if(WiFi.status() == WL_CONNECTED){
          HTTPClient http;
          // Servidor UNAM IP Fija, Puerto 3000
          http.begin("http://132.248.26.212:3000/api/sensores"); 
          http.addHeader("Content-Type", "application/json");

          String jsonPayload = "{";
          jsonPayload += "\"t\":" + String(latestDataSensores.Temp, 2) + ",";
          jsonPayload += "\"p\":" + String(latestDataSensores.Pres, 2) + ",";
          jsonPayload += "\"h\":" + String(latestDataSensores.Hume, 2) + ",";
          jsonPayload += "\"gps\":{";
          jsonPayload += "\"fix\":" + String(latestDataSensores.Lati != 0.0 ? "true" : "false") + ",";
          jsonPayload += "\"lat\":" + String(latestDataSensores.Lati, 6) + ",";
          jsonPayload += "\"lon\":" + String(latestDataSensores.Long, 6) + ",";
          jsonPayload += "\"alt\":" + String(latestDataSensores.Alti, 2) + ",";
          jsonPayload += "\"sats\":" + String(gps.satellites.value()) + ",";
          
          char bufferHora[10];
          sprintf(bufferHora, "%02d:%02d:%02d", latestDataTiempo.timeinfo.tm_hour, latestDataTiempo.timeinfo.tm_min, latestDataTiempo.timeinfo.tm_sec);
          jsonPayload += "\"hora\":\"" + String(bufferHora) + "\"";
          jsonPayload += "}}";

          int httpResponseCode = http.POST(jsonPayload);
          
          if(httpResponseCode > 0){
            Serial.println("Datos enviados a UNAM. HTTP: " + String(httpResponseCode));
          } else {
            Serial.println("Error HTTP: " + http.errorToString(httpResponseCode));
          }
          http.end();
        }

        BanderaSensores = 0;
        BanderaTiempo = 0;
      }
    }
  }
}

void setup() {
  Serial.begin(115200); 
  Serial1.begin(9600); 

  while (!Serial) { delay(10); }

  Serial.printf("Conectando a %s...", ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡Conectado a WiFi!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)){
    delay(500);
  }
  
  // ELIMINADO: WiFi.disconnect(true) y WiFi.mode(WIFI_OFF) para mantener la conexión.

  QEmpaquetador = xQueueCreate(10, sizeof(QueueMessage_t)); 

  xTaskCreatePinnedToCore(Sensores, "Lectura de Sensores", 4096, NULL, 3, &Task_Sensores, 1);
  xTaskCreatePinnedToCore(Antena, "Envio de datos", 4096, NULL, 1, &Task_Antena, 0);
  xTaskCreatePinnedToCore(TomarHora, "Lectura de RTC", 4096, NULL, 3, &Task_Hora, 1);
  xTaskCreatePinnedToCore(Empaquetado, "Empaquetado de datos", 4096, NULL, 2, &Task_Empaquetado, 0);

  Wire.begin(4, 5); 
  if (!bme.begin(0x76, &Wire)) {  
    Serial.println("¡Error! BME280 no inicializado.");  
    while (1);
  }
}

void loop() {}