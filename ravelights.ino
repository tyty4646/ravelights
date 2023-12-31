#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "ESP_Color.h"
#include <Adafruit_NeoPixel.h>


#define LED_COUNT 111
#define LED_PIN D0
#define LED_PIN_TWO D1

#define RIGHT_TOUCH_PIN T7
#define MIDDLE_TOUCH_PIN T8
#define LEFT_TOUCH_PIN T9

#define WIFI_SSID    "Verizon_6NSP4Q"
#define WIFI_PASS    "splash9-fax-con"

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel stripTwo(LED_COUNT, LED_PIN_TWO, NEO_RGB + NEO_KHZ800);
ESP_Color::Color c;
String hexColor = "";
unsigned int ledMode = 1;
unsigned int prevLedMode = 1;
bool lightsOn = 1;
String hueString = "";

int brightness = 80;
int brightnessLast = 80;
int avgZValue = 0;

unsigned int touchThreshold = 46000;
unsigned int debounce = 0;
int rightTouchVal = 0;
int middleTouchVal = 0;
int leftTouchVal = 0;
bool rightPrevPressed = false;
bool middlePrevPressed = false;
bool leftPrevPressed = false;
int rightTouchAvg = 0;
int middleTouchAvg = 0;
int leftTouchAvg = 0;
bool brightnessGoingUp = true;


unsigned int connectedClients = 0;
unsigned int lastConnectedClients = 0;

bool hitRegistered = false;

// ====================================================================================================================================

int brightnessCalc(int maxBrightness) {
  return maxBrightness * (brightness/255.0f);
}

void setAllWhite() {
  strip.fill(strip.Color(255, 130, 75));
  stripTwo.fill(strip.Color(255, 130, 75));
  strip.setBrightness(brightness);
  stripTwo.setBrightness(brightness);
}

void hueChange() {
  strip.fill(strip.Color(c.R_Byte(), c.G_Byte(), c.B_Byte()));
  stripTwo.fill(strip.Color(c.R_Byte(), c.G_Byte(), c.B_Byte()));
  strip.setBrightness(brightness);
  stripTwo.setBrightness(brightness);
}

unsigned int prevRainbow = 0;
long firstPixelHue = 0;

void rainbow() {
  strip.setBrightness(brightness);
  stripTwo.setBrightness(brightness);
  if (millis() - prevRainbow >= 5) {
    if (firstPixelHue < 5*65536) {
      strip.rainbow(firstPixelHue);
      stripTwo.rainbow(firstPixelHue);
      firstPixelHue += 64;
    } else {
      firstPixelHue = 0;
    }
    prevRainbow = millis();
  }
}

uint8_t reactiveBrightness = 25;
unsigned int prevReactiveTimer = 5;
bool firstRun = true;

void reactive() {
  if (hitRegistered) {
    reactiveBrightness = 255;
    hitRegistered = false;
  }
  
  if (millis() - prevReactiveTimer >= 5) {
    if (reactiveBrightness > 25) {
      reactiveBrightness--;
    }
    prevReactiveTimer = millis();
  }
  
  strip.fill(strip.Color(c.R_Byte(), c.G_Byte(), c.B_Byte()));
  stripTwo.fill(strip.Color(c.R_Byte(), c.G_Byte(), c.B_Byte()));
  strip.setBrightness(reactiveBrightness);
  stripTwo.setBrightness(reactiveBrightness);
}

void off() {
  strip.clear();
  stripTwo.clear();
}

void setupWiFi() {
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  String hostname = "ravelights";
  WiFi.setHostname(hostname.c_str()); //define hostname
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Connected! Starting OTA.");
  Serial.println("Finished booting. :D");
}

void initButtons() {
  for (int i = 0; i < 20; i++) {
    rightTouchAvg += touchRead(RIGHT_TOUCH_PIN);
    middleTouchAvg += touchRead(MIDDLE_TOUCH_PIN);
    leftTouchAvg += touchRead(LEFT_TOUCH_PIN);
    delay(3);
  }
  rightTouchAvg /= 20;
  middleTouchAvg /= 20;
  leftTouchAvg /= 20;
}

void handleTouchButtons() {
  if (millis() - debounce >= 50) {
    rightTouchVal = 0;
    middleTouchVal = 0;
    leftTouchVal = 0;
    for (int i = 0; i < 4; i++) {
      rightTouchVal += touchRead(RIGHT_TOUCH_PIN);
      middleTouchVal += touchRead(MIDDLE_TOUCH_PIN);
      leftTouchVal += touchRead(LEFT_TOUCH_PIN);
    }
    rightTouchVal /= 4;
    middleTouchVal /= 4;
    leftTouchVal /= 4;
    
    debounce = millis();
  }

  // ----- TOUCH PRESS HANDLING -----
  
  // Button on right side (on/off)
  if (abs(rightTouchVal - rightTouchAvg) > touchThreshold) {
    if (!rightPrevPressed) { // button was just pressed
      toggleOnOff();
      rightPrevPressed = true;
    }
  } else {
    rightPrevPressed = false;
  }

  // Button in middle (mode cycling)
  if (abs(middleTouchVal - middleTouchAvg) > touchThreshold) {
    if (!middlePrevPressed) { // button was just pressed
      if (ledMode < 5 && lightsOn) {
        ledMode++;
      } else if (lightsOn) {
        ledMode = 1;
      }
      middlePrevPressed = true;
    }
  } else {
    middlePrevPressed = false;
  }

  // Button in middle (mode cycling)
  if (abs(leftTouchVal - leftTouchAvg) > touchThreshold) {
    if (!leftPrevPressed) { // button was just pressed
      if (brightness < 255) brightnessGoingUp = true;
      else brightnessGoingUp = false;
      leftPrevPressed = true;
    } else { // button is being held
      if (brightnessGoingUp && brightness < 255) {
        brightness++;
      } else if (brightnessGoingUp && brightness == 255) {
        brightnessGoingUp = false;
        brightness--;
      } else if (!brightnessGoingUp && brightness > 1) {
        brightness--;
      } else if (!brightnessGoingUp && brightness == 1) {
        brightnessGoingUp = true;
        brightness++;
      }
    }
  } else {
    leftPrevPressed = false;
  }
}

// ====================================================================================================================================

void setup() {
  //Serial.begin(9600);
  //while(!Serial);
  Serial.println("Serial Initialized");
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN_TWO, OUTPUT);
  //pinMode(TOGGLE_BTN_PIN, INPUT);
  delay(1000);             // sanity check delay - allows reprogramming if accidently blowing power w/leds
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  stripTwo.begin();
  strip.show();            // Turn OFF all pixels ASAP  
  stripTwo.show();
  strip.setBrightness(brightness);
  stripTwo.setBrightness(brightness);

  c = ESP_Color::Color::FromHex("#30D5C8");
  
  setupWiFi();
  setupOTA();
  startWebServer();
  initWebClient();

  initButtons();
  
  xTaskCreatePinnedToCore(
    handleWS,
    "websocket",
    5000,
    NULL,
    0,
    NULL,
    1);  

  xTaskCreatePinnedToCore(
    ota,
    "OTA handling",
    5000,
    NULL,
    0,
    NULL,
    1);

  xTaskCreatePinnedToCore(
    handleWSClient,
    "ws client handle hits",
    5000,
    NULL,
    0,
    NULL,
    0);
}

void loop() {  
  handleTouchButtons();
  switch (ledMode) {
    case 0:
      off();
      break;
    case 1:
      setAllWhite();
      break;
    case 2:
      hueChange();
      break;
    case 3:
      rainbow();
      break;
    case 4:
      reactive();
      break;
    default:
      ledMode = 1;
      break;
    }
  strip.show();
  stripTwo.show();
}
