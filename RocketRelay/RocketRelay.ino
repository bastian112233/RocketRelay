#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include "frames/rckt.h"

// ----------- CONFIGURACIÓN WiFi / MQTT ------------
const char* ssid = ""; //Tu SSID
const char* password = ""; //Tu password
const char* mqtt_server = "test.mosquitto.org";

// ----------- CONFIGURACIÓN RELÉS ------------------
const int relay1 = 26;
const int relay2 = 25;
const int relay3 = 33;
const int relay4 = 27;

// ----------- CONFIGURACIÓN NEOPIXELS --------------
#define PIN_NEOPIXEL 12
#define NUMPIXELS 3
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ----------- CONFIGURACIÓN BME680 -----------------
#define SDA_PIN 21
#define SCL_PIN 22
Adafruit_BME680 bme;

// ----------- CONFIGURACIÓN SCT-013 ----------------
const int pinSCT = 34;
const float VREF = 3.3;
const int RESOLUCION_ADC = 4095;
const float SENSIBILIDAD = 0.050; // 50mA/A
const float RESISTOR_CARGA = 22.0; // Ohms
const float UMBRAL_CORRIENTE = 2.0; // A

// ----------- OBJETOS ------------------------------
WiFiClient espClient;
PubSubClient client(espClient);
TFT_eSPI tft = TFT_eSPI();

// ----------- TEMPORIZADORES -----------------------
unsigned long prevSensorMillis = 0;
unsigned long prevPantallaMillis = 0;
unsigned long prevCorrienteMillis = 0;
const unsigned long intervaloSensor = 5000;
const unsigned long intervaloPantalla = 5000;
const unsigned long intervaloCorriente = 8000;

// ----------- ÚLTIMOS DATOS SENSOR -----------------
float ultimaTemp = 0, ultimaHum = 0, ultimaPres = 0, ultimaGas = 0;
float ultimaCorriente = 0;

// ----------- FUNCIONES ----------------------------
void setup_wifi() {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);

  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  pixels.setPixelColor(0, pixels.Color(0, 255, 0));
  pixels.show();

  mostrarPantallaPrincipal();
}

void mostrarPantallaPrincipal() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_GREEN);
  delay(1000);

  tft.setCursor(30, 90);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.println("RocketRelay");

  tft.setCursor(20, 110);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.print("IP: ");
  tft.println(WiFi.localIP());
}

void indicarActividadMQTT() {
  if (ultimaCorriente >= UMBRAL_CORRIENTE) {
    pixels.setPixelColor(2, pixels.Color(255, 0, 0)); 
  } else {
    pixels.setPixelColor(2, pixels.Color(0, 255, 255)); 
  }
  pixels.show();
  delay(650); 
  pixels.setPixelColor(2, 0); 
  pixels.show();
}

void mostrarDatosEnPantalla(float temp, float hum, float pres, float gas) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(80, 20);  tft.println("BME680:");

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(20, 60);  tft.printf("Temp: %.2f C", temp);
  tft.setCursor(20, 90);  tft.printf("Hum: %.2f %%", hum);
  tft.setCursor(20, 120); tft.printf("Pres: %.2f hPa", pres);
  tft.setCursor(20, 150); tft.printf("Gas: %.2f kOhms", gas);
}

void mostrarCorrienteEnPantalla(float corriente) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(60, 50);
  tft.println("SCT-013:");
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(20, 90);
  tft.printf("Corriente: %.2f A", corriente);
  tft.setCursor(20, 120);
  tft.printf("Potencia: %.1f W", corriente * 120.0);
  delay(1500);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  indicarActividadMQTT();

  if (String(topic) == "home/relay1") {
    digitalWrite(relay1, message == "ON" ? HIGH : LOW);
  } else if (String(topic) == "home/relay2") {
    digitalWrite(relay2, message == "ON" ? HIGH : LOW);
  } else if (String(topic) == "home/relay3") {
    digitalWrite(relay3, message == "ON" ? HIGH : LOW);
  } else if (String(topic) == "home/relay4") {
    digitalWrite(relay4, message == "ON" ? HIGH : LOW);
  } else if (String(topic) == "home/showCurrent") {
    mostrarCorrienteEnPantalla(ultimaCorriente);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("¡Conectado!");
      client.subscribe("home/relay1");
      client.subscribe("home/relay2");
      client.subscribe("home/relay3");
      client.subscribe("home/relay4");
      client.subscribe("home/showCurrent");
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      Serial.println(". Reintentando en 5 segundos...");
      pixels.setPixelColor(1, pixels.Color(255, 0, 0));
      pixels.show();
      delay(5000);
    }
  }
  pixels.setPixelColor(1, pixels.Color(0, 255, 0));
  pixels.show();
}

void animarDespegueCohete() {
  delay(1000);
  tft.pushImage(0, 0, 240, 240, rckt);
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 105);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(3);
  tft.println("RocketRelay");
  delay(2000);
}

float leerCorriente() {
  int lecturaADC = analogRead(pinSCT);
  float voltaje = (lecturaADC * VREF) / RESOLUCION_ADC;
  float corriente = (voltaje / RESISTOR_CARGA) / SENSIBILIDAD;
  return corriente;
}

// ------------------------ SETUP ------------------------
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(pinSCT, INPUT);

  pinMode(relay1, OUTPUT); digitalWrite(relay1, LOW);
  pinMode(relay2, OUTPUT); digitalWrite(relay2, LOW);
  pinMode(relay3, OUTPUT); digitalWrite(relay3, LOW);
  pinMode(relay4, OUTPUT); digitalWrite(relay4, LOW);

  pixels.begin();
  pixels.clear();
  pixels.setBrightness(70);
  pixels.show();

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_BLUE);
  delay(1000);
  animarDespegueCohete();

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  if (!bme.begin(0x76, &Wire)) {
    Serial.println("¡No se encontró el BME680!");
    while (1);
  }

  Serial.println("BME680 conectado correctamente.");

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);
}

// ------------------------ LOOP ------------------------
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  if (now - prevSensorMillis > intervaloSensor) {
    if (bme.performReading()) {
      ultimaTemp = bme.temperature;
      ultimaHum = bme.humidity;
      ultimaPres = bme.pressure / 100.0;
      ultimaGas = bme.gas_resistance / 1000.0;

      client.publish("home/temp", String(ultimaTemp, 2).c_str(), true);
      client.publish("home/humidity", String(ultimaHum, 2).c_str(), true);
      client.publish("home/pressure", String(ultimaPres, 2).c_str(), true);
      client.publish("home/gas", String(ultimaGas, 2).c_str(), true);

      indicarActividadMQTT();
    }
    prevSensorMillis = now;
  }

  if (now - prevCorrienteMillis > intervaloCorriente) {
    ultimaCorriente = leerCorriente();
    client.publish("home/current", String(ultimaCorriente, 2).c_str(), true);
    client.publish("home/power", String(ultimaCorriente * 120, 2).c_str(), true);

    indicarActividadMQTT();

    prevCorrienteMillis = now;
  }

  if (now - prevPantallaMillis > intervaloPantalla) {
    mostrarDatosEnPantalla(ultimaTemp, ultimaHum, ultimaPres, ultimaGas);
    prevPantallaMillis = now;
  }
}
