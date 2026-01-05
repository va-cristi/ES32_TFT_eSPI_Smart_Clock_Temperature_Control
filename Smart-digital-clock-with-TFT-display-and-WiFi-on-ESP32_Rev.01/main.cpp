/*
🚀 Proiect
✅ Ceas HH:MM:SS cu animație rolling odometer
✅ DS3231 RTC cu backup
✅ Sincronizare NTP automată
✅ Ziua săptămânii și data
✅ Text scroll personalizat
✅ Culori custom pentru fiecare element
✅ Am utilizat core 0 pentru sincronizare -NonBlocking-
*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include "bigFont.h"
#include "middleFont.h"
#include "config_wifi.h"
#include "Free_Fonts.h" // Include the header file attached to this sketch

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// ========================================================================== Scroll text
TFT_eSprite scrollSprite = TFT_eSprite(&tft);
String scrollText = "   > > > Internet Clock with ESP32_TFT & DS3231 RTC < < <   ";
int scrollPos = 320;
int scrollSpeed = 1;

unsigned long previousClockTime = 0;
unsigned long previousScrollTime = 0;
unsigned long previousAnimTime = 0;

bool isPaused = false;           
unsigned long pauseStartTime = 0; 
const long pauseDuration = 1000;

// ========================================================================== DS3231 RTC
RTC_DS3231 rtc;
bool rtcFound = false;
// ========================================================================== Date & Day Display
TFT_eSprite dateSprite = TFT_eSprite(&tft);  // 🆕 Sprite pentru dată
const char* daysOfWeek[] = {"Duminica", "Luni", "Marti", "Miercuri", "Joi", "Vineri", "Sambata"}; // 🆕

// ========================================================================== Culori
unsigned short colorSprite = TFT_BLACK;
unsigned short back = TFT_WHITE;    // Culoare sprite cifre HH MM
unsigned short backSS = TFT_YELLOW; // Culoare sprite cifre SS
unsigned short darkG = 0x2945;      // Culoare cadran cifre HH MM
unsigned short lightG = 0xBDF7;     // Culoare linia care taie cifrele
unsigned short red = 0xa026;        // Culoare cadran cifre SS
unsigned short segCol[6] = {darkG, darkG, darkG, darkG, red, red};
// ========================================================================== WiFi & Time
int zone = 2;
const char* ntpServer = "pool.ntp.org";
// ========================================================================== ANIMAȚIE ROLLING
String digits[6] = {"0","0","0","0","0","0"};
String oldDigits[6] = {"0","0","0","0","0","0"};  // Cifrele anterioare
float animProgress[6] = {0,0,0,0,0,0};  // Progresul animației (0.0 - 1.0)
bool isAnimating[6] = {false,false,false,false,false,false};

const float ANIM_SPEED = 0.08;  // Viteza animației (mai mare = mai rapid)
int n = 0;

// Variabile globale pentru task
TaskHandle_t WiFiTaskHandle = NULL;
bool syncInProgress = false;
// ========================================================================== WiFi Status
bool wifiSyncSuccess = true;   // Flag pentru ultimul sync (true = OK, false = FAIL)
bool wifiSyncAttempted = false; // A încercat vreodată să se conecteze?

// ========================================================================= Task care rulează pe * * * Core 0 * * *
void wifiSyncTask(void * parameter) {
    Serial.println("WiFi task started on core: " + String(xPortGetCoreID()));
    
    wifiSyncAttempted = true;  // ✅ Marchează că a încercat
    
    // Conectare WiFi
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        
        // Sincronizare NTP
        configTime(3600 * zone, 0, ntpServer);
        struct tm timeinfo;
        
        if (getLocalTime(&timeinfo)) {
            rtc.adjust(DateTime(
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec
            ));
            Serial.println("DS3231 synchronized!");
            wifiSyncSuccess = true;  // ✅ Success!
        } else {
            Serial.println("NTP sync failed!");
            wifiSyncSuccess = false;  // ❌ Fail
        }
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    } else {
        Serial.println("\nWiFi connection failed!");
        wifiSyncSuccess = false;  // ❌ Fail
    }
    
    syncInProgress = false;
    vTaskDelete(NULL);
}
// ========================================================================= Task care rulează pe * * * Core 0 * * *
// ========================================================================= Blocking doar pentru setup/init (se folosește rar)
void connectWifiBlocking()
{ 
    Serial.print("Connecting to WiFi (blocking)");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
    } else {
        Serial.println("\nWiFi connection failed!");
    }
}
// ========================================================================= Blocking doar pentru setup/init (se folosește rar)
void syncRTCwithNTP()
{
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Syncing DS3231 with NTP...");
        
        configTime(3600 * zone, 0, ntpServer);
        struct tm timeinfo;
        
        if (getLocalTime(&timeinfo)) {
            rtc.adjust(DateTime(
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec
            ));
            
            Serial.println("DS3231 synchronized with NTP!");
            Serial.printf("Time set: %02d:%02d:%02d\n", 
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            Serial.println("Failed to get time from NTP");
        }
    } else {
        Serial.println("WiFi not connected, cannot sync with NTP");
    }
}

void initDS3231()
{
    Wire.begin(21, 22);
    
    if (rtc.begin()) {
        rtcFound = true;
        Serial.println("DS3231 found!");
        
        if (rtc.lostPower()) {
            Serial.println("DS3231 lost power, syncing with NTP...");
            connectWifiBlocking();  // ✅ Folosește versiunea blocking (doar la pornire)
            syncRTCwithNTP();
        } else {
            Serial.println("DS3231 is running with valid time");
            DateTime now = rtc.now();
            Serial.printf("Current time from DS3231: %02d:%02d:%02d\n", 
                now.hour(), now.minute(), now.second());
        }
    } else {
        rtcFound = false;
        Serial.println("DS3231 NOT found!");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ESP32 Clock with DS3231 & Rolling Animation ===");
    
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK); 
    sprite.createSprite(320, 100);
    scrollSprite.createSprite(320, 30);
    dateSprite.createSprite(320, 50);  // 🆕 Sprite pentru dată

    initDS3231();
    
   // ✅ Doar dacă DS3231 a pierdut curentul
    if (rtcFound && rtc.lostPower()) {
        connectWifiBlocking();  // Deja apelat în initDS3231(), dar poți șterge asta
        syncRTCwithNTP();
    }
}

// Funcție pentru easing (smooth animation)
float easeOutCubic(float t) {
    return 1 - pow(1 - t, 3);
}

void drawDigitWithAnimation(int index, int xPos, int startY, int digitWidth, int digitHeight)
{
    sprite.loadFont(bigFont);
    
    sprite.fillSmoothRoundRect(xPos, startY, digitWidth, digitHeight, 8, segCol[index], colorSprite);  // culoarea din spatele ecranelor de cifre
    //sprite.fillRect(xPos, startY + digitHeight/2, digitWidth, 2, lightG);                      
    
    if (isAnimating[index]) {
        float easedProgress = easeOutCubic(animProgress[index]);
        int offsetY = (int)(easedProgress * digitHeight);
        
        unsigned short textColor = (index >= 4) ? backSS : back;
        sprite.setTextColor(textColor, segCol[index]);
        
        sprite.setViewport(xPos, startY, digitWidth, digitHeight);
        
        // 🎯 FORMULA CORECTATĂ:
        int baseY = digitHeight / 2;  // Mijlocul relativ la viewport
        
        // Cifra veche urcă
        sprite.drawString(oldDigits[index], digitWidth/2, baseY - offsetY);
        
        // Cifra nouă intră
        sprite.drawString(digits[index], digitWidth/2, baseY + digitHeight - offsetY + 7); // Modifica asezarea pe centru
        
        sprite.resetViewport();
        //sprite.fillRect(xPos, startY + digitHeight/2, digitWidth, 1/*2*/, lightG); // Linia care taie cifrele
        
    } else {
        unsigned short textColor = (index >= 4) ? backSS : back;
        sprite.setTextColor(textColor, segCol[index]);
        sprite.drawString(digits[index], xPos + digitWidth/2, startY + digitHeight/2 + 7);  
        //sprite.fillRect(xPos, startY + digitHeight/2, digitWidth, 1 /*2*/, lightG); // Linia care taie cifrele
    }
    
    sprite.unloadFont();
}

void draw()
{
    sprite.fillSprite(colorSprite); 
    sprite.setTextDatum(4);
    
    int digitWidth = 45;
    int digitHeight = 80;
    int startX = 15;
    int startY = 10;
    int spacing = 48;
    
    for(int i = 0; i < 6; i++) {
        int xPos = startX + (i * spacing);
        drawDigitWithAnimation(i, xPos, startY, digitWidth, digitHeight);
    }

    sprite.pushSprite(0, 30);
}

// 🆕 ========================================================================= Desenează ziua și data
void drawDate(DateTime now)
{
    dateSprite.fillSprite(TFT_BLACK);
    dateSprite.setTextDatum(4);
    
    // Ziua săptămânii
    dateSprite.setTextColor(TFT_CYAN, TFT_BLACK);
    dateSprite.setTextSize(2);
    dateSprite.drawString(daysOfWeek[now.dayOfTheWeek()], 160, 12);
    
    // Data
    char dateStr[12];
    sprintf(dateStr, "%02d/%02d/%04d", now.day(), now.month(), now.year());
    dateSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    dateSprite.setTextSize(2);
    dateSprite.drawString(dateStr, 160, 35);
    
    // ✅ INDICATOR WiFi în colțul dreapta-jos
    // Icon mai mare, mai evident:
if (wifiSyncAttempted) {
    if (wifiSyncSuccess) {
        dateSprite.fillCircle(295, 42, 10, TFT_GREEN);
        dateSprite.setTextColor(TFT_BLACK);
        dateSprite.setTextSize(2);
        dateSprite.drawString("✓", 295, 42);
    } else {
        dateSprite.fillCircle(295, 42, 10, TFT_RED);
        dateSprite.setTextColor(TFT_WHITE);
        dateSprite.setTextSize(2);
        dateSprite.drawString("!", 295, 42);
    }
}
    
    dateSprite.pushSprite(0, 130);
}

void updateAnimations()
{
    bool anyAnimating = false;
    
    for(int i = 0; i < 6; i++) {
        if (isAnimating[i]) {
            animProgress[i] += ANIM_SPEED;
            
            if (animProgress[i] >= 1.0) {
                // Animația s-a terminat
                animProgress[i] = 0.0;
                isAnimating[i] = false;
                oldDigits[i] = digits[i];
            } else {
                anyAnimating = true;
            }
        }
    }
    
    if (anyAnimating) {
        draw();
    }
}

void updateTime(String newDigits[6])
{
    // Verifică care cifre s-au schimbat și pornește animația
    for(int i = 0; i < 6; i++) {
        if (newDigits[i] != digits[i]) {
            oldDigits[i] = digits[i];
            digits[i] = newDigits[i];
            isAnimating[i] = true;
            animProgress[i] = 0.0;
        }
    }
}

void loop() {
    unsigned long currentTime = millis();

    // --- SCROLL TEXT ---
    if (currentTime - previousScrollTime >= 30) {
        previousScrollTime = currentTime;
        int textWidth = scrollText.length() * 6; 

        if (!isPaused) {
            scrollPos -= scrollSpeed;
            if (scrollPos < -(textWidth / 2)) {
                isPaused = true;
                pauseStartTime = currentTime;
            }
        } else {
            if (currentTime - pauseStartTime >= pauseDuration) {
                scrollPos = 320 + (textWidth / 2);
                isPaused = false;
            }
        }

        scrollSprite.fillSprite(0x702e); 
        scrollSprite.setTextColor(TFT_YELLOW);
        scrollSprite.setTextDatum(4); 
        
        if (!isPaused) {
            scrollSprite.drawString(scrollText, scrollPos, 12, 2);  // , 2); 
        }
        
        scrollSprite.pushSprite(0, 0);
    }

    // --- ANIMAȚIE UPDATE (rulează des pentru smooth animation) ---
    if (currentTime - previousAnimTime >= 16) {  // ~60 FPS
        previousAnimTime = currentTime;
        updateAnimations();
    }

    // --- CLOCK UPDATE ---
    if (currentTime - previousClockTime >= 1000) {
        previousClockTime = currentTime;
        
        if (rtcFound) {
            DateTime now = rtc.now();
            
            char timeStr[9];
            sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
            
            String newDigits[6];
            newDigits[0] = String(timeStr[0]);
            newDigits[1] = String(timeStr[1]);
            newDigits[2] = String(timeStr[3]);
            newDigits[3] = String(timeStr[4]);
            newDigits[4] = String(timeStr[6]);
            newDigits[5] = String(timeStr[7]);
            
            updateTime(newDigits);

            // 🆕 Desenează data și ziua
            drawDate(now);
  
          // sincronizarea: ceas
if (now.hour() == 3 && now.minute() == 0 && now.second() == 0) {
    if (!syncInProgress) {
        Serial.println("Starting NTP sync task on Core 0...");
        syncInProgress = true;
        
        // Creează task pe Core 0 (Core 1 e ocupat cu display)
        xTaskCreatePinnedToCore(
            wifiSyncTask,      // Funcția
            "WiFiSync",        // Nume
            10000,             // Stack size
            NULL,              // Parametri
            1,                 // Prioritate
            &WiFiTaskHandle,   // Handle
            0                  // Core 0
        );
    }
}
        
        draw();
    }

}
}



