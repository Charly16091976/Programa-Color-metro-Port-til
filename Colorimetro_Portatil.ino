#include <Wire.h>
#include "SH1106Wire.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

// Definiciones de pines para ESP8266
#define S0 D2
#define S1 D1
#define S2 D6
#define S3 D8
#define salidaTCS D7
#define OLED_ADDRESS 0x3c
#define OLED_SDA D3
#define OLED_SCL D5
#define PULSADOR D4

const char* ssid = "Colorimetro Portatil";
const char* password = "123456";

SH1106Wire display(OLED_ADDRESS, OLED_SDA, OLED_SCL);
ESP8266WebServer server(80);

bool primeraMedicionRealizada = false;
float valorPantalla = 0.0f;
float ultimoValorMedido = 0.0f;
float promedio = 0.0f;

float ultimosValores[10];
int indiceUltimosValores = 0;

void cargarValores() {
  if (LittleFS.exists("/valores.txt")) {
    File file = LittleFS.open("/valores.txt", "r");
    if (file) {
      int i = 0;
      while (file.available() && i < 10) {
        ultimosValores[i] = file.parseFloat();
        i++;
      }
      file.close();
    } else {
      Serial.println("Error al abrir el archivo para lectura.");
    }
  } else {
    Serial.println("No se encontraron valores previos.");
  }
}

void guardarValores() {
  File file = LittleFS.open("/valores.txt", "w");
  if (file) {
    for (int i = 0; i < 10; i++) {
      file.println(ultimosValores[i]);
    }
    file.close();
  } else {
    Serial.println("Error al abrir el archivo para escritura.");
  }
}

void setup() {
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(salidaTCS, INPUT);
  pinMode(PULSADOR, INPUT_PULLUP);

  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  Serial.begin(9600);
  Serial.println("Iniciando...");

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_24);

  if (!LittleFS.begin()) {
    Serial.println("Error al montar el sistema de archivos");
    return;
  }

  cargarValores();

  WiFi.softAP(ssid, password);

  if (WiFi.softAP(ssid, password)) {
    Serial.print("Punto de acceso creado. Dirección IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Error al crear el punto de acceso.");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleUpdate);
  server.begin();
  Serial.println("Servidor web iniciado.");

  for (int i = 128; i >= -128; i--) {
    display.clear();
    display.drawString(i, 15, "Colorimetro Portatil");
    display.display();
    delay(40);
  }

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Cargando...");
  for (int i = 0; i <= 100; i++) {
    display.drawProgressBar(0, 25, 120, 10, i);
    display.setColor(BLACK);
    display.fillRect(52, 46, 76, 14);
    display.setColor(WHITE);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 46, String(i) + "%");
    display.display();
    delay(50);
  }

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 25, "Esperando medicion");
  display.display();
}

void handleRoot() {
  String html = "<html><body style='background-color: yellow;'>";
  html += "<div style='border: 12px solid black; padding: 10px; margin-bottom: 20px; text-align: center;'>";
  html += "<h1 style='font-size: 80px; color: orange; font-weight: bold'>COLORIMETRO PORTATIL</h1>";
  html += "</div>";
  html += "<div style='border: 12px solid black; padding: 1px; text-align: center;'>";
  html += "<div style='font-size: 80px; color: red; font-style: italic;'>";
  html += "<p>Valor en pantalla:</p>";
  html += "<p>" + String(ultimoValorMedido, 2) + " Mg/l CL2</p>";
  html += "</div>";
  html += "</div>";
  html += "<div style='position: absolute; bottom: 10px; right: 10px;'>";
  html += "<form action='/update' method='get'>";
  html += "<input type='submit' value='Actualizar' style='font-size: 45px; width: 250px; height: 150px;'>";
  html += "</form>";
  html += "</div>";
  html += "<div style='font-size: 30px; text-align: center; color: green'>";
  html += "<p>Ultimos 10 valores:</p>";
  for (int i = 0; i < 10; i++) {
    html += "<p>" + String(ultimosValores[i], 2) + " Mg/l CL2</p>";
  }
  html += "</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleUpdate() {
  handleRoot();
}

float mapearValor(int valorSensor) {
  valorSensor = constrain(valorSensor, 75, 150); // Nuevo rango basado en la nueva información
  float valorMapeado = (float)(valorSensor - 75) / (150 - 75); // Mapear 60-150 a 0-1
  return max(0.0f, valorMapeado);
}

void loop() {
  if (digitalRead(PULSADOR) == LOW) {
    digitalWrite(S2, LOW);
    digitalWrite(S3, LOW);
    
    int medicionTotal = 0;
    int medicionesValidas = 0;
    
    for (int i = 0; i < 3; i++) {
      int medicion = pulseIn(salidaTCS, LOW);
      delay(200);
      
      if (medicion > 0) {
        medicionTotal += medicion;
        medicionesValidas++;
      }
    }

    promedio = medicionesValidas > 0 ? (float)medicionTotal / medicionesValidas : 0;
    valorPantalla = mapearValor(promedio);
    valorPantalla = max(0.0f, valorPantalla);

    Serial.print("Valor en Pantalla: ");
    Serial.println(valorPantalla);
    Serial.print("Rojo: ");
    Serial.println(promedio);

    ultimoValorMedido = valorPantalla;

    for (int i = 9; i > 0; i--) {
      ultimosValores[i] = ultimosValores[i - 1];
    }
    ultimosValores[0] = valorPantalla;

    guardarValores();

    display.clear();
    display.drawRect(0, 0, 128, 15);
    display.drawRect(0, 15, 128, 48);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(20, 0, "Colorimetro Portatil");
    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(10, 20, String(valorPantalla, 2));
    display.setFont(ArialMT_Plain_16);
    display.drawString(60, 20, "Mg/l CL2");
    display.display();
    delay(1000);

    primeraMedicionRealizada = true;
  } else if (!primeraMedicionRealizada) {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 25, "Esperando medicion");
    display.display();
  }

  server.handleClient();
}


  server.handleClient();  // Necesario para que el servidor maneje las solicitudes
}
