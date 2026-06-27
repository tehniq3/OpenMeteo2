#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// --- Configuratii Wi-Fi ---
const char* ssid = "wifi_name";
const char* password = "wifi_password";

// --- Configurare LCD ---
LiquidCrystal_I2C lcd(0x3F, 16, 2); 

// --- Configurare Open-Meteo (Craiova) ---
const char* openMeteoUrl = "https://api.open-meteo.com/v1/forecast?latitude=44.33&longitude=23.79&current=temperature_2m,relative_humidity_2m,weather_code&timezone=Europe/Bucharest";

// --- Variabile globale pentru vreme ---
float temperatura = 0.0;
int umiditate = 0;
int codVreme = 0;
unsigned long ultimaActualizareVreme = 0;
const unsigned long intervalActualizare = 600000; // Actualizeaza vremea la 10 minute

// --- Variabile pentru alternarea afisajului ---
bool arataTempUmid = true; // Daca e true, arata temp/umid. Daca e false, arata text vreme
unsigned long ultimulSchimb = 0;
const unsigned long intervalSchimb = 2000; // Schimba la fiecare 2 secunde

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Conectare Wi-Fi");
  lcd.setCursor(0, 1);
  lcd.print("...");.

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectat!");
  lcd.clear();
  lcd.print("Wi-Fi Conectat!");

  // Setare ora Romaniei
  configTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org", "time.nist.gov");
  client.setInsecure();

  delay(2000);
  actualizeazaVremea();
}

void loop() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  if (timeinfo == nullptr) {
    lcd.setCursor(0, 0);
    lcd.print("Astept NTP...   ");
    delay(1000);
    return;
  }

  // --- RANDUL 1: Data scurta si Ora ---
  char bufferRand1[17];
  sprintf(bufferRand1, "%02d.%02d  %02d:%02d:%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  lcd.setCursor(0, 0);
  lcd.print(bufferRand1);

  // --- LOGICA DE ALTERNARE PENTRU RANDUL 2 ---
  if (millis() - ultimulSchimb >= intervalSchimb) {
    arataTempUmid = !arataTempUmid; // Inverseaza comutatorul
    ultimulSchimb = millis();
  }

  char bufferRand2[17];
  lcd.setCursor(0, 1);

  if (arataTempUmid) {
    // Afiseaza Temperatura si Umiditatea
    // Simbolul de grade este caracterul cu codul 223 in tabelul LCD-ului
    sprintf(bufferRand2, " %4.1f%cC   %2d%%  ", temperatura, 223, umiditate);
    lcd.print(bufferRand2);
  } else {
    // Afiseaza starea vremii (text complet, centrat)
    char textVremeLung[17];
    obtineTextVremeLung(codVreme, textVremeLung);
    lcd.print(textVremeLung);
  }

  // Actualizeaza datele meteo de la Open-Meteo
  if (millis() - ultimaActualizareVreme > intervalActualizare) {
    actualizeazaVremea();
  }

  delay(100); // Mic delay pentru a nu supraîncărca procesorul, dar destul de rapid pentru alternanta
}

void actualizeazaVremea() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("Descarcare date meteo...");

  HTTPClient http;
  http.begin(client, openMeteoUrl);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      temperatura = doc["current"]["temperature_2m"].as<float>();
      umiditate = doc["current"]["relative_humidity_2m"].as<int>();
      codVreme = doc["current"]["weather_code"].as<int>();
      
      ultimaActualizareVreme = millis();
      Serial.printf("Vreme actualizata: %.1fC, Umiditate: %d%%, Cod: %d\n", temperatura, umiditate, codVreme);
    } else {
      Serial.print("Eroare parse JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("Eroare HTTP: %d\n", httpCode);
  }
  http.end();
}

// Functie care returneaza text LUNG (fara diacritice) pentru vreme, adaptat la 16 caractere
void obtineTextVremeLung(int cod, char* outText) {
  switch(cod) {
    case 0:   strcpy(outText, "       Senin    "); break;
    case 1:   
    case 2:   strcpy(outText, " Partial noros "); break;
    case 3:   strcpy(outText, "      Noros    "); break;
    case 45:  
    case 48:  strcpy(outText, "      Ceata    "); break; // Ceață
    case 51:  
    case 53:  
    case 55:  strcpy(outText, "     Burnita   "); break;
    case 56:  
    case 57:  strcpy(outText, " Burnita cu gh "); break; // Burniță cu gheață
    case 61:  strcpy(outText, " Ploaie usoara "); break;
    case 63:  strcpy(outText, "Ploaie moderata "); break;
    case 65:  strcpy(outText, "Ploaie puternic"); break;
    case 66:  
    case 67:  strcpy(outText, "Ploaie cu gh.  "); break;
    case 71:  strcpy(outText, "    Ninsoare   "); break;
    case 73:  strcpy(outText, " Ninsoare moder "); break;
    case 75:  strcpy(outText, " Ninsoare grea  "); break;
    case 77:  strcpy(outText, "    Grindina   "); break;
    case 80:  strcpy(outText, "     Averse    "); break;
    case 81:  strcpy(outText, " Averse moderate"); break;
    case 82:  strcpy(outText, "Averse puternic"); break;
    case 85:  strcpy(outText, "Ninsoare averse"); break;
    case 86:  strcpy(outText, "Nins. averse gr"); break;
    case 95:  strcpy(outText, "     Furtuna   "); break;
    case 96:  
    case 99:  strcpy(outText, " Furtuna+grind  "); break;
    default:  strcpy(outText, "     N/A        "); break;
  }
}
