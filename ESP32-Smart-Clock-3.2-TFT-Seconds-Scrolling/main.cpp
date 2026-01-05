#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ESP32Time.h>
#include <WiFi.h>
#include "bigFont.h"
#include "middleFont.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
// ========================================================================== Scroll text
TFT_eSprite scrollSprite = TFT_eSprite(&tft);  // sprite pentru textul scroll
String scrollText = "   > > > Internet Clock with ESP32 & Display SPI-TFT_eSPI < < <   "; // Spațiile la capete ajută la buclă
int scrollPos = 320; // Poziția de start a textului (în afara ecranului, în dreapta)
int scrollSpeed = 1; // Cât de repede se mișcă (pixeli per frame)

unsigned long previousClockTime = 0;
unsigned long previousScrollTime = 0;

// Variabile pentru controlul pauzei (versiunea corectă)
bool isPaused = false;           
unsigned long pauseStartTime = 0; 
const long pauseDuration = 1000;  // Durata pauzei (2 secunde)
// ========================================================================== Scroll text
ESP32Time rtc(0); 

//colours
// ======================================================== Culoare de Fundal
 unsigned short back=TFT_WHITE;
unsigned short backSS = TFT_YELLOW;  // culoare secunde
// ========================================================
unsigned short darkG=0x2945;
unsigned short lightG=0xBDF7; // Gri 
unsigned short red=0xa026; // 9820;
unsigned short segCol[6]={darkG,darkG,darkG,darkG,red,red}; // 6 cifre acum! SI CULORILE FIECAREI CIFRE

//time zone and wifi
int zone=2;
const char* ntpServer = "pool.ntp.org";          
String ssid = "xxxxx";
String password = "xxxxxxx";

// 6 digits in clock (HH:MM:SS)
String digits[6]={"0","0","0","0","0","0"};
int n=0;  // counter for seconds

void connectWifi()
{ 
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Can't connect to wifi!");
  }
}

void setTime()
{
  configTime(3600*zone, 0, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)){
    rtc.setTimeStruct(timeinfo); 
  }
}


void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_RED); 
  sprite.createSprite(320,100); // (320,170); ORIGINAL Dimensiune sprite
  scrollSprite.createSprite(320, 30); // ORIGINAL 25); // sprite-ul pentru scroll
 
    connectWifi();
    setTime();

    /* 
//brightness for ESP32 S3 & amoLED
     ledcSetup(0, 10000, 8);
     ledcAttachPin(38, 0);
     ledcWrite(0, 120);
*/
}

void draw()
{
    sprite.fillSprite(back); 
    sprite.loadFont(bigFont);
    sprite.setTextDatum(4);
    
    // Dimensiuni mai mici pentru 6 cifre
    int digitWidth = 45;   // lățime cifră (era 68)
    int digitHeight = 80;  // înălțime cifră (era 100)
    int startX = 15;       // start X (era 21)
    int startY = 10;       // start Y (era 37) asezarea cifrelor la linia n dupa sprite 
    int spacing = 48;      // spațiu între cifre (era 70)
    
    for(int i=0; i<6; i++){
        int xPos = startX + (i * spacing);
        
        // Dreptunghi rotunjit pentru fiecare cifră
        sprite.fillSmoothRoundRect(xPos, startY, digitWidth, digitHeight, 8, segCol[i], back);
        
       
        
        // Afișare cifră
        // sprite.setTextColor(back, segCol[i]);
        if (i >= 4) { // Dacă este a 5-a sau a 6-a cifră (secundele)
                 sprite.setTextColor(backSS, segCol[i]); // Text galben, fundal rămâne cel din array
                     } else { // Pentru ore și minute
                sprite.setTextColor(back, segCol[i]); // Text alb, fundal din array
           }
       sprite.drawString(digits[i], xPos + digitWidth/2 , startY + digitHeight/2 + 7); // startY + digitHeight/2  ORIGINAL asezarea cifrelor

        // ========================================================================================= Linie Orizontală Decorativă la mijlocul cifrelor
         sprite.fillRect(xPos, startY + digitHeight/2, digitWidth, 2, lightG);
    } // final for

    sprite.unloadFont();
    sprite.pushSprite(0, 30); // ORIGINAL 25); // 0, 0 coloana si linia de unde incepe sprite-ul
}

void loop() {
  unsigned long currentTime = millis();

  // --- 1. LOGICA PENTRU TEXTUL SCROLL (versiunea reparată) ---
  if (currentTime - previousScrollTime >= 30) {
    previousScrollTime = currentTime;

    int textWidth = scrollText.length() * 6; 

    if (!isPaused) {
      // === STARE DE SCROLL ===
      // Mută poziția textului spre stânga
      scrollPos -= scrollSpeed;

      // Verifică dacă textul a dispărut complet
      if (scrollPos < -(textWidth / 2)) {
        isPaused = true;           // Intră în starea de pauză
        pauseStartTime = currentTime; // Salvează momentul exact când a început pauza
      }
    } else {
      // === STARE DE PAUZĂ ===
      // Verifică dacă pauza de 2 secunde s-a terminat folosind timer-ul dedicat
      if (currentTime - pauseStartTime >= pauseDuration) {
        // Pauza s-a terminat, resetăm poziția instant și reluăm scroll-ul
        scrollPos = 320 + (textWidth / 2); // Apare instant în dreapta
        isPaused = false; // Ieșim din starea de pauză
      }
    }

    // Desenarea sprite-ului se face mereu
    scrollSprite.fillSprite(TFT_BLACK); 
    scrollSprite.setTextColor(TFT_YELLOW);
    scrollSprite.setTextDatum(4); 
    
    // Desenăm textul DOAR dacă nu este în pauză
    if (!isPaused) {
        scrollSprite.drawString(scrollText, scrollPos, 12, 2); 
    }
    
    scrollSprite.pushSprite(0, 0);
  }

  // --- 2. LOGICA PENTRU CEAS (rămâne neschimbată) ---
  if (currentTime - previousClockTime >= 1000) {
    previousClockTime = currentTime;
    
    String timeStr = rtc.getTime("%H:%M:%S");
    digits[0] = timeStr.substring(0, 1);
    digits[1] = timeStr.substring(1, 2);
    digits[2] = timeStr.substring(3, 4);
    digits[3] = timeStr.substring(4, 5);
    digits[4] = timeStr.substring(6, 7);
    digits[5] = timeStr.substring(7, 8);
  
    draw();
    sprite.pushSprite(0, 30);

    n++;
    if(n==6800) {
      n=0;
      setTime();
    }
  }
}
