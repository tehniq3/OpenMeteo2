/*
 * v.1.0 - used to Wemos D1 mini (ESP8266) 
 * v.2.0 - moved to ESP32 C3-mini 
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// --- Configuratii Wi-Fi ---
const char* ssid = "bbk2";
const char* password = "internet2";

// --- Configurare LCD ---
LiquidCrystal_I2C lcd(0x3F, 16, 2); 

// --- URL-uri Open-Meteo (Craiova) ---
const char* weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=44.33&longitude=23.79&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,wind_direction_10m,pressure_msl,uv_index&timezone=Europe/Bucharest";
const char* airQualityUrl = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=44.33&longitude=23.79&current=european_aqi";

// --- Variabile globale pentru Vreme ---
float temperatura = 0.0;
int umiditate = 0;
int codVreme = 0;
float vitezaVant = 0.0;
int directieVant = 0;
float presiune = 0.0; 
float uvIndex = 0.0;
int aqi = -1; 

// --- Variabile pentru meniu / afisaj ---
int ecranCurent = 0; 
unsigned long ultimulSchimb = 0;
const unsigned long intervalSchimb = 3000; // 3 secunde

unsigned long ultimaActualizareVreme = 0;
const unsigned long intervalActualizare = 600000; // 10 minute

// --- Variabile pentru verificare Wi-Fi ---
bool esteConectat = true;
unsigned long ultimaVerificareWifi = 0;
const unsigned long intervalVerificareWifi = 5000; // Verifica la fiecare 5 secunde

WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  
  // Pe ESP32-C3 Mini, I2C default este pe GPIO8 (SDA) si GPIO9 (SCL)
  Wire.begin(8, 9); 
  
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Conectare Wi-Fi");
  lcd.setCursor(0, 1);
  lcd.print("...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectat!");
  lcd.clear();
  lcd.print("Wi-Fi Conectat!");

  // --- Setare ora Romaniei corect pentru ESP32 ---
  // Setam fusul orar POSIX (tine cont automat de ora de vara/iarna)
  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
  tzset();
  // Pornim NTP cu offset 0 (deoarece TZ se ocupa de offset)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  client.setInsecure();

  delay(2000);
  actualizeazaDate();
}

void loop() {
  // ==========================================
  // VERIFICARE PERIODICĂ WI-FI
  // ==========================================
  if (millis() - ultimaVerificareWifi >= intervalVerificareWifi) {
    ultimaVerificareWifi = millis();
    
    if (WiFi.status() == WL_CONNECTED) {
      if (!esteConectat) {
        esteConectat = true;
        lcd.clear();
        Serial.println("Wi-Fi Reconectat cu succes!");
        
        // Resetam fusul orar si NTP si la reconectare
        setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
        tzset();
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        
        actualizeazaDate(); 
      }
    } else {
      if (esteConectat) {
        esteConectat = false;
        lcd.clear();
      }
      lcd.setCursor(0, 0);
      lcd.print("Wi-Fi Deconectat");
      lcd.setCursor(0, 1);
      lcd.print("Reconectare...  ");
      
      WiFi.disconnect();
      WiFi.begin(ssid, password); 
      return; 
    }
  }

  if (!esteConectat) return;

  // ==========================================
  // AFISAJ NORMAL (ESP32 foloseste getLocalTime)
  // ==========================================
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    lcd.setCursor(0, 0);
    lcd.print("Astept NTP...   ");
    delay(1000);
    return;
  }

  // --- RANDUL 1: Data scurta si Ora (STATIC) ---
  char bufferRand1[17];
  sprintf(bufferRand1, "%02d.%02d  %02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  lcd.setCursor(0, 0);
  lcd.print(bufferRand1);

  // --- LOGICA DE ALTERNARE LA 3 SECUNDE ---
  if (millis() - ultimulSchimb >= intervalSchimb) {
    ecranCurent++;
    
    if (ecranCurent == 4 && (timeinfo.tm_hour >= 20 || timeinfo.tm_hour < 6)) {
      ecranCurent = 5; 
    }
    
    if (ecranCurent > 5) {
      ecranCurent = 0; 
    }
    ultimulSchimb = millis();
  }

  // --- RANDUL 2: Afisarea informatiei curente ---
  char bufferRand2[17];
  lcd.setCursor(0, 1);

  switch (ecranCurent) {
    case 0: 
      sprintf(bufferRand2, " %4.1f%cC   %2d%%  ", temperatura, 223, umiditate);
      lcd.print(bufferRand2);
      break;
      
    case 1: 
      obtineTextVremeLung(codVreme, bufferRand2);
      lcd.print(bufferRand2);
      break;
      
    case 2: 
      char dirVant[4];
      obtineDirectieVant(directieVant, dirVant);
      sprintf(bufferRand2, "Vant %4.1fkm/h %-2s", vitezaVant, dirVant);
      lcd.print(bufferRand2);
      break;
      
    case 3: 
      sprintf(bufferRand2, "P: %5.1f mmHg   ", presiune * 0.750062);
      lcd.print(bufferRand2);
      break;
      
    case 4: 
      char nivelUV[10];
      obtineNivelUV(uvIndex, nivelUV);
      sprintf(bufferRand2, "UV: %3.1f %-7s", uvIndex, nivelUV);
      lcd.print(bufferRand2);
      break;
      
    case 5: 
      char calitateAer[10];
      obtineCalitateAer(aqi, calitateAer);
      if (aqi >= 0) {
        sprintf(bufferRand2, "AQI: %2d   %-6s", aqi, calitateAer);
      } else {
        sprintf(bufferRand2, "AQI: Fara date  ");
      }
      lcd.print(bufferRand2);
      break;
  }

  if (millis() - ultimaActualizareVreme > intervalActualizare) {
    actualizeazaDate();
  }

  delay(50);
}

// ==========================================
// FUNCTII DE DESCARCARE SI PARSARE DATE
// ==========================================

void actualizeazaDate() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("Descarcare date meteo si aer...");
  lcd.setCursor(0, 1);
  lcd.print("Se incarca date ");

  HTTPClient httpWeather;
  httpWeather.begin(client, weatherUrl);
  int httpCode1 = httpWeather.GET();

  if (httpCode1 > 0) {
    String payload = httpWeather.getString();
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      temperatura = doc["current"]["temperature_2m"].as<float>();
      umiditate = doc["current"]["relative_humidity_2m"].as<int>();
      codVreme = doc["current"]["weather_code"].as<int>();
      vitezaVant = doc["current"]["wind_speed_10m"].as<float>();
      directieVant = doc["current"]["wind_direction_10m"].as<int>();
      presiune = doc["current"]["pressure_msl"].as<float>(); 
      uvIndex = doc["current"]["uv_index"].as<float>();
      Serial.println("Vreme actualizata cu succes.");
    }
  }
  httpWeather.end();

  HTTPClient httpAir;
  httpAir.begin(client, airQualityUrl);
  int httpCode2 = httpAir.GET();

  if (httpCode2 > 0) {
    String payloadAir = httpAir.getString();
    StaticJsonDocument<128> docAir;
    DeserializationError errorAir = deserializeJson(docAir, payloadAir);
    if (!errorAir) {
      aqi = docAir["current"]["european_aqi"].as<int>();
      Serial.printf("AQI actualizat: %d\n", aqi);
    }
  } else {
    aqi = -1; 
  }
  httpAir.end();

  ultimaActualizareVreme = millis();
}

// ==========================================
// FUNCTII AJUTATOARE PENTRU TEXTE
// ==========================================

void obtineTextVremeLung(int cod, char* outText) {
  switch(cod) {
    case 0:   strcpy(outText, "       Clar    "); break;
    case 1:   strcpy(outText, "Mai mult senin "); break; 
    case 2:   strcpy(outText, " Partial noros "); break; 
    case 3:   strcpy(outText, "      Noros    "); break; 
    case 45:  
    case 48:  strcpy(outText, "      Cetza    "); break; 
    case 51:  
    case 53:  
    case 55:  strcpy(outText, "     Burnita   "); break;
    case 56:  
    case 57:  strcpy(outText, " Burnita cu gh "); break; 
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
    default:  strcpy(outText, "      N/A      "); break;
  }
}

void obtineDirectieVant(int grade, char* out) {
  if (grade >= 337 || grade < 23) strcpy(out, "N");
  else if (grade < 68) strcpy(out, "NE");
  else if (grade < 113) strcpy(out, "E");
  else if (grade < 158) strcpy(out, "SE");
  else if (grade < 203) strcpy(out, "S");
  else if (grade < 248) strcpy(out, "SV");
  else if (grade < 293) strcpy(out, "V");
  else strcpy(out, "NV");
}

void obtineNivelUV(float uv, char* out) {
  if (uv <= 2.9) strcpy(out, "Scazut");     
  else if (uv <= 5.9) strcpy(out, "Mediu");
  else if (uv <= 7.9) strcpy(out, "Ridicat"); 
  else if (uv <= 10.9) strcpy(out, "Foarte");
  else strcpy(out, "Extrem");
}

void obtineCalitateAer(int val, char* out) {
  if (val < 0) strcpy(out, "N/A");
  else if (val <= 20) strcpy(out, "Bun");
  else if (val <= 40) strcpy(out, "OK");
  else if (val <= 60) strcpy(out, "Mediu");
  else if (val <= 80) strcpy(out, "Slab");
  else if (val <= 100) strcpy(out, "Rau");
  else strcpy(out, "F. bRau");
}
