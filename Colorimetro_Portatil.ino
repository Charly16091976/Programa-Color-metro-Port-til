#include <Wire.h>
#include "SH1106Wire.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

// Definiciones de pines para ESP8266
#define S0 D2  // S0 a pin D2
#define S1 D1  // S1 a pin D1
#define S2 D6  // S2 a pin D6
#define S3 D8  // S3 a pin D8
#define salidaTCS D7  // salidaTCS a pin D7
#define OLED_ADDRESS 0x3c
#define OLED_SDA D3
#define OLED_SCL D5
#define PULSADOR D4  // Define el pin para el pulsador

const char* ssid = "Colorimetro Portatil";
const char* password = "123456";

SH1106Wire display(OLED_ADDRESS, OLED_SDA, OLED_SCL);
ESP8266WebServer server(80);

bool primeraMedicionRealizada = false;
float valorPantalla = 0.0f;
float ultimoValorMedido = 0.0f;
float promedio = 0.0f; // Declaración global de promedio

float ultimosValores[10];
int indiceUltimosValores = 0;

void cargarValores() {
  if (LittleFS.begin()) {
    if (LittleFS.exists("/valores.txt")) {
      File file = LittleFS.open("/valores.txt", "r");
      if (file) {
        int i = 0;
        while (file.available() && i < 10) {
          ultimosValores[i] = file.parseFloat();
          i++;
        }
        file.close();
      }
    }
  }
}

void guardarValores() {
  if (LittleFS.begin()) {
    File file = LittleFS.open("/valores.txt", "w");
    if (file) {
      for (int i = 0; i < 10; i++) {
        file.println(ultimosValores[i]);
      }
      file.close();
    }
  }
}

void setup() {
  // Configuración de pines
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(salidaTCS, INPUT);
  pinMode(PULSADOR, INPUT_PULLUP);  // Configura el pulsador como entrada con resistencia de pull-up

  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  Serial.begin(9600);
  Serial.println("Iniciando...");

  // Inicializar display
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_24);

  // Inicializar LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Error al montar el sistema de archivos");
    return;
  }

  // Cargar valores previos
  cargarValores();

  // Configurar WiFi como punto de acceso
  WiFi.softAP(ssid, password);
  
  // Verificar si el punto de acceso fue creado exitosamente
  if (WiFi.softAP(ssid, password)) {
    Serial.print("Punto de acceso creado. Dirección IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Error al crear el punto de acceso.");
  }

  // Configurar servidor web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleUpdate);
  server.begin();
  Serial.println("Servidor web iniciado.");

  // Mostrar mensaje deslizante
  for (int i = 128; i >= -128; i--) {
    display.clear();
    display.drawString(i, 15, "Colorimetro Portatil");
    display.display();
    delay(40);
  }

  // Mostrar barra de carga
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

  // Mostrar mensaje de espera
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 25, "Esperando medicion");
  display.display();
}

void handleRoot() {
  String html = "<html><body style='background-color: yellow;'>";
  // Contenedor rectangular para el título y el valor en pantalla
  html += "<div style='border: 12px solid black; padding: 10px; margin-bottom: 20px; text-align: center;'>";
  html += "<h1 style='font-size: 80px;color: orange;font-weight: bold'>COLORIMETRO PORTATIL</h1>";
  html += "</div>";
  html += "<div style='border: 12px solid black; padding: 1px; text-align: center;'>";
  html += "<div style='font-size: 80px; color: red;font-style: italic;'>";
  html += "<p>Valor en pantalla:</p>";
  html += "<p>" + String(ultimoValorMedido, 2) + " Mg/l CL2</p>";
  html += "</div>";
  html += "</div>";

  //html += "<hr>"; // Agrega un separador horizontal

 // Estilos para el contenedor del botón de actualizar
  html += "<div style='position: absolute; bottom: 10px; right: 10px;'>";
  html += "<form action='/update' method='get'>";
  html += "<input type='submit' value='Actualizar' style='font-size: 45px; width: 250px; height: 150px;'>";
  html += "</form>";
  html += "</div>";

  // Agrega un espacio para mostrar los últimos 10 valores
  html += "<div style='font-size: 30px;text-align: center;color: green'>";
  html += "<p>Ultimos 10 valores:</p>";
  for (int i = 0; i < 10; i++) {
    html += "<p>" + String(ultimosValores[i], 2) + " Mg/l CL2</p>";
  }
  html += "</div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}


void handleUpdate() {
  handleRoot();  // Actualiza la página
}

float mapearValor(int valorSensor) {
  // Asegurarse de que el valor nunca sea negativo
  valorSensor = max(0, valorSensor);
  
  // Si el valor del sensor es menor que 420, mapear a 0.00
  if (valorSensor < 420) {
    return 0.00f;
  } else {
    // Mapear valores de 410 en adelante a incrementos de 0.10 por cada 1000 unidades
    return valorSensor / 1000.0f;
  }
}

void loop() {
  // Espera a que el pulsador sea presionado
  if (digitalRead(PULSADOR) == LOW) {
    // Configurar el sensor para detectar el color rojo
    digitalWrite(S2, LOW);
    digitalWrite(S3, LOW);
    
    int medicionTotal = 0;
    int medicionesValidas = 0;
    
    for (int i = 0; i < 3; i++) {
      int medicion = pulseIn(salidaTCS, LOW);
      delay(200);
      
      // Solo suma si la medición es mayor que cero
      if (medicion > 0) {
        medicionTotal += medicion;
        medicionesValidas++;
      }
    }

    // Mapea el valor promedio al rango de 0.10 a 1.00
    promedio = medicionesValidas > 0 ? (float)medicionTotal / medicionesValidas : 0;
    valorPantalla = mapearValor(promedio);

    // Asegura que el valor en pantalla no sea negativo
    valorPantalla = max(0.0f, valorPantalla);

    Serial.print("Valor en Pantalla: ");
    Serial.println(valorPantalla);
    Serial.print("Rojo: ");
    Serial.println(promedio);

    // Actualiza el último valor medido
    ultimoValorMedido = valorPantalla;

    // Desplaza los valores anteriores en el array
    for (int i = 9; i > 0; i--) {
      ultimosValores[i] = ultimosValores[i - 1];
    }
    ultimosValores[0] = valorPantalla;

    // Guarda los valores en el archivo
    guardarValores();

    display.clear();
    display.drawRect(0, 0, 128, 15);
    display.drawRect(0, 15, 128, 48);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(20, 0, "Colorimetro Portatil");
    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(10, 20, String(valorPantalla, 2)); // Muestra 2 decimales
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

  server.handleClient();  // Necesario para que el servidor maneje las solicitudes
}
