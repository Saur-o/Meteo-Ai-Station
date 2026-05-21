#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <sensori.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <PubSubClient.h>
// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

//Alloca 256KB di buffer massimo per le foto
#define MQTT_MAX_PACKET_SIZE 262144

//Istanze I2C
#define I2C_SDA 2
#define I2C_SCL 1

// pin attivazione transistor
#define PIN_ATTIVAZIONE_GAS_SENSORS 21

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "STAZIONE_METEO1_5H";
const char *password = "m3Teo$t4tion";

const char* mqtt_server = "mqtt-broker.lan"; // <--- METTI L'IP DEL RASPBERRY
const int mqtt_port = 1883;
const char* mqtt_topic_dati = "stazione1/meteo/dati";
const char* mqtt_topic_img = "stazione1/meteo/immagini";

WiFiClient espClient;
PubSubClient client(espClient);

//const char* serverDataEndpoint = "http://192.168.1.23:5000/data";

//void startCameraServer();   Disattiva web server della cam per risparmiare memoria PSRAM
void setupLedFlash();

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.setDebugOutput(true);
  Serial.println();


  // Configurazione MQTT
  client.setServer(mqtt_server, mqtt_port);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 1;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
  }
   // IMPOSTAZIONI QUALITÀ (Fuori dall'if: valide per tutti i sensori)
      s->set_quality(s, 10);                // 10 è molto alta (0-63, più basso è meglio)
    s->set_brightness(s, 0);             // Luminosità (-2 a 2)
    s->set_contrast(s, 0);               // Contrasto (-2 a 2)
    s->set_saturation(s, 0);             // Saturazione (-2 a 2)
    s->set_special_effect(s, 0);         // 0 = Nessun effetto
    s->set_wb_mode(s, 0);                // 0 = Auto White Balance
    s->set_ae_level(s, 0);               // Esposizione automatica

  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_UXGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif  
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

 // startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  pinMode(PIN_ATTIVAZIONE_GAS_SENSORS, OUTPUT);
  
  // ACCENSIONE IMMEDIATA
  // L'UPS da 3A gestirà il carico iniziale senza problemi
  digitalWrite(PIN_ATTIVAZIONE_GAS_SENSORS, HIGH); 
  Serial.println("[POWER] Riscaldatori MQ Attivati");
  delay(100);

  inizializzaSensori();
  delay(500);
}

unsigned long previousMillis = 0;
const long interval = 20000; // Intervallo desiderato (20 secondi)

// Funzione per riconnettersi a MQTT se cade la linea
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Tentativo connessione MQTT...");
    // Crea un ID client unico basato sul MAC o fisso
    if (client.connect("ESP32_StazioneMeteo")) {
      Serial.println("connesso!");
    } else {
      Serial.print("fallito, rc=");
      Serial.print(client.state());
      Serial.println(" riprovo tra 5 secondi");
      delay(5000);
    }
  }
}

void loop() {
  
  // Mantieni viva la connessione MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  unsigned long currentMillis = millis();

  // Controlla se è ora di scattare
  if (currentMillis - previousMillis >= interval) {
    
    // 1. SINCRONIZZAZIONE (Fondamentale)
    // Usiamo += invece di = currentMillis per recuperare i millisecondi 
    // persi durante l'esecuzione del codice (scatto + invio).
    previousMillis += interval; 

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("--- Scatto Programmato ---");

     // inviaFotoPC();
      delay(2000);



      inviaFotoPC(); // <--- NUOVA FUNZIONE MQTT


      
     // inviaDatiSensori();
      debug_sensori();
    } else {
      Serial.println("WiFi disconnesso! Tentativo di riconnessione...");
      // Proviamo a ricollegarci, ma non mettiamo un delay lungo qui 
      // altrimenti blocchiamo il tempismo del prossimo scatto.
      WiFi.begin(ssid, password);
    }
  }
}


void inviaDatiMQTT(const char *timestamp) {
  // Con ArduinoJson 7 si usa solo JsonDocument
  JsonDocument doc; 

  // Popoliamo il JSON
  doc["temp"]  = leggiTemperatura();
  doc["hum"]   = leggiUmidita();
  doc["pres"]  = leggiPressione();
  doc["light"] = leggiLuce();
  doc["wind"]  = leggiVento();
  doc["co"]    = leggiCO();
  doc["air"]   = leggiQualitaAria();
  doc["timestamp"] = timestamp;

  // Serializziamo in una stringa
  String jsonOutput;
  serializeJson(doc, jsonOutput);

  // Invio al broker MQTT sul Raspberry
  if (client.publish(mqtt_topic_dati, (uint8_t*)jsonOutput.c_str(), jsonOutput.length(), false)) {
    Serial.print("Dati pubblicati su MQTT: ");
    Serial.println(jsonOutput);
  } else {
    Serial.println("Errore pubblicazione MQTT: Controlla connessione broker!");
  }
}


/*
// codice invio dati sensori
void inviaDatiSensori() {
  StaticJsonDocument<512> doc; // Creiamo il contenitore JSON

  // Popoliamo il JSON usando le tue funzioni .cpp che restituiscono double
  doc["temp"]  = leggiTemperatura();
  doc["hum"]   = leggiUmidita();
  doc["pres"]  = leggiPressione();
  doc["light"] = leggiLuce();
  doc["wind"]  = leggiVento();
  doc["co"]    = leggiCO();
  doc["air"]   = leggiQualitaAria();

  String jsonOutput;
  serializeJson(doc, jsonOutput); // Trasformiamo il JSON in una stringa

  Serial.print("Invio dati: ");
  Serial.println(jsonOutput);

  HTTPClient http;
  http.setTimeout(3000);
 // http.begin(serverDataEndpoint);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonOutput);

  if (httpResponseCode > 0) {
    Serial.printf("Dati inviati! Risposta server: %d\n", httpResponseCode);
  } else {
    Serial.printf("Errore invio dati: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

*/
// CODICE PER LO SCATTO SINGOLO + INVIO AL SERVER INTERNO DEL PC
void inviaFotoPC() {
  Serial.println("\n--- Inizio procedura di scatto ---");

  // 1. Scatta la foto
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("ERRORE: Acquisizione camera fallita");
    return;
  }
  Serial.printf("Foto scattata! Dimensione: %u bytes\n", fb->len);
  
  size_t packet_size = 19 + fb->len;
  
  time_t t_sec = fb->timestamp.tv_sec;
  struct tm *timeinfo = localtime(&t_sec);
  
  char timestamp[20];
  strftime(timestamp, 20, "%Y-%m-%d_%H-%M-%S", timeinfo);
  
  uint8_t *mqtt_img   = (uint8_t *)malloc(packet_size);
  
  if (mqtt_img != NULL)
  {
    memcpy(mqtt_img, timestamp, 19);
    memcpy(mqtt_img + 19, fb->buf, fb->len);
  
    unsigned long start_time = millis();
    if(client.publish(mqtt_topic_img, mqtt_img, packet_size, false))
    {
      unsigned long end_time = millis();
      Serial.printf("SUCCESSO: Immagine inviata! time taken: %d\n", (end_time - start_time));
    }
    else
    {
      Serial.println("ERRORE: Memoria insufficiente per allocare il pacchetto MQTT");
    }
    free(mqtt_img);
  }

  inviaDatiMQTT(timestamp);
  
  esp_camera_fb_return(fb);
  
  Serial.println("Memoria liberata.");
}
