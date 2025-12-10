/*
  Portal Gun LED Controller - Clean & Maintainable Version
  =======================================================
  
  Refactored for easy maintenance and customization
  - Centralized text strings for easy modification
  - Unified display functions with parameters
  - Cleaner code organization
  - Configurable constants at the top
  
  Controls:
  - Pin 8: Toggle between Blue/Red portal modes
  - Pin 7: Shoot current portal type
  
  Version: 3.1 (Refactored)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// =============================================================================
// CONFIGURATION SECTION - Easy to modify
// =============================================================================

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

// Pin assignments
const int TOGGLE_BUTTON_PIN = 8;
const int SHOOT_BUTTON_PIN = 7;
const int BLUE_LED_PIN = 5;
const int RED_LED_PIN = 6;
const int ONBOARD_LED_PIN = 13;

// Audio configuration
const int AUDIO_VOLUME = 25;        // 0-30 scale
const int DFPLAYER_RX = 12;
const int DFPLAYER_TX = 4;

// LED settings
const int DIM_BRIGHTNESS = 30;
const int MAX_BRIGHTNESS = 255;
const int FADE_STEP = 2;
const int FADE_DELAY = 20;

// Timing settings
const int DEBOUNCE_DELAY = 50;
const int BLINK_INTERVAL = 500;
const int OLED_UPDATE_INTERVAL = 200;
const int PULSE_DELAY = 15;
const int PULSE_COUNT = 8;
const int BRIGHTNESS_STEP = 20;

// Audio file indices
enum AudioTrack {
  SOUND_POWER_UP = 1,
  SOUND_BLUE_PORTAL = 2,
  SOUND_RED_PORTAL = 3,
  SOUND_MODE_SWITCH = 4,
  SOUND_POWER_DOWN = 5
};

// =============================================================================
// TEXT STRINGS - Centralized for easy modification
// =============================================================================

// Main display strings
const char* const DISPLAY_STRINGS[] = {
  "PORTAL GUN",           // 0
  "BLUE",                 // 1
  "RED",                  // 2
  "Status: READY",        // 3
  "Fired: ",              // 4
  "PWR:",                 // 5
  "♪",                    // 6
  "O",                    // 7 - Blue portal symbol
  "@"                     // 8 - Red portal symbol
};

// Mode switching strings
const char* const MODE_STRINGS[] = {
  "MODE SWITCHING",       // 0
  ">> BLUE PORTAL <<",    // 1
  ">> RED PORTAL <<",     // 2
  "AUTO-SWITCH!",         // 3
  "Next: BLUE PORTAL",    // 4
  "Next: RED PORTAL"      // 5
};

// Startup sequence strings
const char* const STARTUP_STRINGS[] = {
  "PORTAL GUN v3.1",      // 0
  "AUDIO: READY",         // 1
  "AUDIO: OFFLINE",       // 2
  "INITIALIZING...",      // 3
  "SYSTEMS ONLINE",       // 4
  "READY TO FIRE"         // 5
};

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SoftwareSerial audioSerial(DFPLAYER_RX, DFPLAYER_TX);
DFRobotDFPlayerMini audioPlayer;

enum PortalMode { BLUE_MODE, RED_MODE };

PortalMode currentMode = BLUE_MODE;
bool isShooting = false;
bool audioEnabled = true;
int totalPortalsFired = 0;

// Timing variables
unsigned long lastButtonPress = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastOLEDUpdate = 0;

// Button state tracking
bool lastToggleState = HIGH;
bool lastShootState = HIGH;
bool onboardLEDState = false;

// =============================================================================
// DISPLAY HELPER FUNCTIONS
// =============================================================================

void showCenteredText(const char* text, int y, int textSize = 1) {
  display.setTextSize(textSize);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.print(text);
}

void showTextAt(const char* text, int x, int y, int textSize = 1) {
  display.setTextSize(textSize);
  display.setCursor(x, y);
  display.print(text);
}

void displayMessage(const char* line1, const char* line2 = nullptr, int delayMs = 1000) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  if (line2 == nullptr) {
    showCenteredText(line1, 12);
  } else {
    showCenteredText(line1, 4);
    showCenteredText(line2, 20);
  }
  
  display.display();
  if (delayMs > 0) delay(delayMs);
}

void drawPowerBars(int x, int y, int count) {
  for (int i = 0; i < count; i++) {
    display.setCursor(x + i * 3, y);
    display.print("|");
  }
}

// =============================================================================
// CORE FUNCTIONS
// =============================================================================

void setup() {
  Serial.begin(9600);
  Serial.println(F("Portal Gun v3.1 - Right On Edition"));
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED initialization failed!"));
    for(;;);
  }
  
  // Initialize audio
  audioSerial.begin(9600);
  if (!audioPlayer.begin(audioSerial)) {
    Serial.println(F("DFPlayer Mini not detected"));
    audioEnabled = false;
  } else {
    audioPlayer.volume(AUDIO_VOLUME);
    delay(1000);
    playAudio(SOUND_POWER_UP);
  }
  
  // Setup pins
  pinMode(TOGGLE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SHOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  
  // Optimize PWM for LED noodles
  TCCR0B = TCCR0B & B11111000 | B00000010;
  
  // Run startup sequence
  runStartupSequence();
  setIdleMode();
  updateMainDisplay();
  
  Serial.println(F("Portal Gun Ready!"));
}

void loop() {
  handleButtons();
  updateOnboardLED();
  updateDisplayPeriodically();
  delay(10);
}

// =============================================================================
// BUTTON HANDLING
// =============================================================================

void handleButtons() {
  bool togglePressed = (digitalRead(TOGGLE_BUTTON_PIN) == LOW);
  bool shootPressed = (digitalRead(SHOOT_BUTTON_PIN) == LOW);
  
  if (millis() - lastButtonPress > DEBOUNCE_DELAY) {
    if (togglePressed && lastToggleState == HIGH) {
      togglePortalMode();
      lastButtonPress = millis();
    }
    
    if (shootPressed && lastShootState == HIGH && !isShooting) {
      shootPortal();
      lastButtonPress = millis();
    }
  }
  
  lastToggleState = togglePressed ? LOW : HIGH;
  lastShootState = shootPressed ? LOW : HIGH;
}

void togglePortalMode() {
  currentMode = (currentMode == BLUE_MODE) ? RED_MODE : BLUE_MODE;
  
  playAudio(SOUND_MODE_SWITCH);
  
  if (currentMode == BLUE_MODE) {
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    displayMessage(MODE_STRINGS[0], MODE_STRINGS[1]);
    fadeTransition(RED_LED_PIN, BLUE_LED_PIN);
  } else {
    lastBlinkTime = millis();
    onboardLEDState = true;
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    displayMessage(MODE_STRINGS[0], MODE_STRINGS[2]);
    fadeTransition(BLUE_LED_PIN, RED_LED_PIN);
  }
  
  updateMainDisplay();
}

// =============================================================================
// PORTAL SHOOTING
// =============================================================================

void shootPortal() {
  isShooting = true;
  totalPortalsFired++;
  
  int activePin = (currentMode == BLUE_MODE) ? BLUE_LED_PIN : RED_LED_PIN;
  AudioTrack soundTrack = (currentMode == BLUE_MODE) ? SOUND_BLUE_PORTAL : SOUND_RED_PORTAL;
  
  playAudio(soundTrack);
  performShootingEffect(activePin);
  autoSwitchMode();
  
  isShooting = false;
  updateMainDisplay();
}

void performShootingEffect(int ledPin) {
  for (int pulse = 0; pulse < PULSE_COUNT; pulse++) {
    // Pulse up
    for (int brightness = DIM_BRIGHTNESS; brightness <= MAX_BRIGHTNESS; brightness += BRIGHTNESS_STEP) {
      analogWrite(ledPin, brightness);
      delay(PULSE_DELAY);
    }
    // Pulse down
    for (int brightness = MAX_BRIGHTNESS; brightness >= DIM_BRIGHTNESS; brightness -= BRIGHTNESS_STEP) {
      analogWrite(ledPin, brightness);
      delay(PULSE_DELAY);
    }
  }
  
  analogWrite(ledPin, MAX_BRIGHTNESS);
  delay(100);
}

void autoSwitchMode() {
  currentMode = (currentMode == BLUE_MODE) ? RED_MODE : BLUE_MODE;
  
  if (currentMode == BLUE_MODE) {
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    displayMessage(MODE_STRINGS[3], MODE_STRINGS[4], 1500);
    fadeTransition(RED_LED_PIN, BLUE_LED_PIN);
  } else {
    lastBlinkTime = millis();
    onboardLEDState = true;
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    displayMessage(MODE_STRINGS[3], MODE_STRINGS[5], 1500);
    fadeTransition(BLUE_LED_PIN, RED_LED_PIN);
  }
}

// =============================================================================
// LED CONTROL
// =============================================================================

void fadeTransition(int fromPin, int toPin) {
  for (int step = 0; step <= DIM_BRIGHTNESS; step += FADE_STEP) {
    analogWrite(fromPin, DIM_BRIGHTNESS - step);
    analogWrite(toPin, step);
    delay(FADE_DELAY);
  }
  
  analogWrite(fromPin, 0);
  analogWrite(toPin, DIM_BRIGHTNESS);
}

void setIdleMode() {
  if (currentMode == BLUE_MODE) {
    analogWrite(BLUE_LED_PIN, DIM_BRIGHTNESS);
    analogWrite(RED_LED_PIN, 0);
    digitalWrite(ONBOARD_LED_PIN, HIGH);
  } else {
    analogWrite(RED_LED_PIN, DIM_BRIGHTNESS);
    analogWrite(BLUE_LED_PIN, 0);
  }
}

void updateOnboardLED() {
  if (currentMode == BLUE_MODE) {
    digitalWrite(ONBOARD_LED_PIN, HIGH);
  } else {
    if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
      onboardLEDState = !onboardLEDState;
      digitalWrite(ONBOARD_LED_PIN, onboardLEDState);
      lastBlinkTime = millis();
    }
  }
}

// =============================================================================
// DISPLAY FUNCTIONS
// =============================================================================

void runStartupSequence() {
  // Title screen
  displayMessage(STARTUP_STRINGS[0], 
                audioEnabled ? STARTUP_STRINGS[1] : STARTUP_STRINGS[2], 
                2000);
  
  // Initializing
  displayMessage(STARTUP_STRINGS[3], nullptr, 800);
  
  // Ready screen
  displayMessage(STARTUP_STRINGS[4], STARTUP_STRINGS[5], 1500);
  
  // Portal symbols demo
  if (audioEnabled) {
    delay(2000);
    display.clearDisplay();
    showTextAt("O", 35, 8);  // Blue portal
    showTextAt("[]", 55, 8); // Portal gun
    showTextAt("@", 75, 8);  // Red portal
    display.display();
    delay(1200);
  }
}

void updateMainDisplay() {
  if (isShooting) return;
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Title and current mode
  showTextAt(DISPLAY_STRINGS[0], 0, 0);
  showTextAt(currentMode == BLUE_MODE ? DISPLAY_STRINGS[1] : DISPLAY_STRINGS[2], 85, 0);
  
  // Status line
  showTextAt(DISPLAY_STRINGS[3], 0, 10);
  if (audioEnabled) {
    showTextAt(DISPLAY_STRINGS[6], 85, 10);
  }
  
  // Portal counter and power indicator
  showTextAt(DISPLAY_STRINGS[4], 0, 20);
  display.print(totalPortalsFired);
  showTextAt(DISPLAY_STRINGS[5], 70, 20);
  drawPowerBars(100, 20, 4);
  
  // Portal mode symbol
  showTextAt(currentMode == BLUE_MODE ? DISPLAY_STRINGS[7] : DISPLAY_STRINGS[8], 110, 0, 2);
  
  display.display();
}

void updateDisplayPeriodically() {
  if (millis() - lastOLEDUpdate >= OLED_UPDATE_INTERVAL) {
    updateMainDisplay();
    lastOLEDUpdate = millis();
  }
}

// =============================================================================
// AUDIO FUNCTIONS
// =============================================================================

void playAudio(AudioTrack track) {
  if (audioEnabled && track >= 1 && track <= 5) {
    audioPlayer.play(track);
  }
}