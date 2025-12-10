/*
  Portal Gun LED Controller with OLED Display and Audio
  ====================================================
  
  Complete Portal Gun with LED noodles, OLED status display, and authentic sound effects
  Based on original Portal Gun code with OLED integration
  - Blue/Red portal mode switching with visual and audio feedback
  - Portal shooting with pulsation effects and firing sounds
  - Auto-switch after shooting (game mechanics)
  - Smooth LED fade effects
  - 128x32 OLED display with Portal-themed graphics
  - Audio system with power-up, mode switching, and firing sounds
  
  Wiring (SparkFun RedBoard):
  - Blue Portal Button: Pin 8 (with 10kΩ pull-up to GND)
  - Red Portal Button: Pin 9 (with 10kΩ pull-up to GND)  
  - Shoot Button: Pin 7 (with 10kΩ pull-up to GND)
  - Blue LED Noodle: Pin 5 (PWM, with 330Ω resistor)
  - Red LED Noodle: Pin 6 (PWM, with 330Ω resistor)
  - Power: 10,000mAh power bank through Arduino barrel jack
  - OLED Display: SDA→A4, SCL→A5, VCC→5V, GND→GND
  
  Required Libraries: 
  - Adafruit SSD1306
  - Adafruit GFX Library
  Install via: Tools → Manage Libraries
  
  SD Card Files (FAT32, 2GB max):
  - 0001.mp3 - Power up sound
  - 0002.mp3 - Blue portal firing sound
  - 0003.mp3 - Red portal firing sound
  - 0004.mp3 - Mode switch beep
  - 0005.mp3 - Power down sound (optional)
  
  Author: Portal Gun Project
  Version: 3.0 (Complete Edition)
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// OLED Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DFPlayer Mini setup
SoftwareSerial audioSerial(12, 4); // RX, TX (Arduino pins 12, 4)
DFRobotDFPlayerMini audioPlayer;

// Pin definitions - UPDATED for current LED placement
const int BLUE_BUTTON_PIN = 8;
const int RED_BUTTON_PIN = 9;
const int SHOOT_BUTTON_PIN = 7;
const int BLUE_LED_PIN = 5;        // Blue LED moved to Pin 5 (PWM capable)
const int RED_LED_PIN = 6;         // Red LED moved to Pin 6 (PWM capable)
const int ONBOARD_LED_PIN = 13;    // Built-in LED for mode indication

// Audio file definitions (on microSD card)
const int SOUND_POWER_UP = 1;       // 0001.mp3
const int SOUND_BLUE_PORTAL = 2;    // 0002.mp3  
const int SOUND_RED_PORTAL = 3;     // 0003.mp3
const int SOUND_MODE_SWITCH = 4;    // 0004.mp3
const int SOUND_POWER_DOWN = 5;     // 0005.mp3

// Game state variables
enum PortalMode {
  BLUE_MODE,
  RED_MODE
};

PortalMode currentMode = BLUE_MODE;  // Start with blue portal
bool isShooting = false;             // Prevent multiple simultaneous shoots
unsigned long lastButtonPress = 0;  // For button debouncing
int totalPortalsFired = 0;           // Portal counter
bool audioEnabled = true;            // Audio system status

// LED brightness values - FULL POWER with 10,000mAh power bank
const int DIM_BRIGHTNESS = 30;       // Restored to original dim brightness
const int MAX_BRIGHTNESS = 255;      // Full brightness - no power limitations!
const int FADE_SPEED = 5;            // Smooth transitions restored

// Onboard LED blinking for red mode
unsigned long lastBlinkTime = 0;     // For red mode blinking
bool onboardLEDState = false;        // Current state of onboard LED
const int BLINK_INTERVAL = 500;      // Blink every 500ms for red mode

// OLED update timing
unsigned long lastOLEDUpdate = 0;
const int OLED_UPDATE_INTERVAL = 200;  // Update OLED every 200ms

// Animation variables
int animationFrame = 0;
unsigned long lastAnimationTime = 0;

// Button state tracking
bool lastBlueState = HIGH;
bool lastRedState = HIGH;
bool lastShootState = HIGH;

void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  Serial.println("Portal Gun Complete Edition Starting...");
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED allocation failed!");
    for(;;); // Don't proceed, loop forever
  }
  
  Serial.println("OLED initialized successfully");
  
  // Initialize DFPlayer Mini
  audioSerial.begin(9600);
  Serial.println("Initializing DFPlayer Mini...");
  
  if (!audioPlayer.begin(audioSerial)) {
    Serial.println("DFPlayer Mini not detected! Check wiring and SD card.");
    audioEnabled = false;  // Continue without audio
  } else {
    Serial.println("DFPlayer Mini initialized successfully");
    audioPlayer.volume(25);  // Volume 0-30 (adjust as needed)
    delay(1000);  // Wait for DFPlayer to stabilize
    
    // Play power-up sound
    playSound(SOUND_POWER_UP);
    audioEnabled = true;
  }
  
  // Display startup sequence
  displayStartupSequence();
  
  // Configure button pins as inputs with pull-ups
  pinMode(BLUE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SHOOT_BUTTON_PIN, INPUT_PULLUP);
  
  // Configure LED pins as outputs
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  
  // Optimize PWM for LED noodles (Pins 5 & 6 use Timer 0)
  TCCR0B = TCCR0B & B11111000 | B00000010; // Set Timer 0 to 7.8kHz for pins 5 & 6
  Serial.println("PWM optimized for LED noodles on pins 5 & 6");
  
  // Initialize LEDs - start in blue mode
  setIdleMode();
  
  // Display initial status
  updateOLEDDisplay();
  
  Serial.println("Portal Gun Ready!");
  Serial.println("Current Mode: BLUE");
  
  // Wait for power-up sound to finish
  if (audioEnabled) {
    delay(2000);
  
  // Portal symbols
  display.clearDisplay();
  display.setCursor(35, 8);
  display.print("O");  // Blue portal symbol
  display.setCursor(55, 8);
  display.print("[]"); // Portal gun
  display.setCursor(75, 8);
  display.print("@");  // Red portal symbol
  display.display();
  delay(1200);  // Time for user to see the message
  }
}

void loop() {
  // Read button states - Pin 8 now used as toggle, Pin 7 as shoot
  bool togglePressed = (digitalRead(BLUE_BUTTON_PIN) == LOW);  // Pin 8 as toggle
  bool shootPressed = (digitalRead(SHOOT_BUTTON_PIN) == LOW);   // Pin 7 as shoot
  
  // Handle button presses with debouncing
  if (millis() - lastButtonPress > 50) {  // 50ms debounce
    
    // Toggle button pressed - switch between modes
    if (togglePressed && lastBlueState == HIGH) {
      if (currentMode == BLUE_MODE) {
        switchToRedMode();
      } else {
        switchToBlueMode();
      }
      lastButtonPress = millis();
    }
    
    // Shoot button pressed
    if (shootPressed && lastShootState == HIGH && !isShooting) {
      shootPortal();
      lastButtonPress = millis();
    }
  }
  
  // Store button states for next loop - reuse existing variables
  lastBlueState = togglePressed ? LOW : HIGH;   // Track toggle button state
  lastShootState = shootPressed ? LOW : HIGH;   // Track shoot button state
  
  // Handle onboard LED based on current mode
  updateOnboardLED();
  
  // Update OLED display periodically
  if (millis() - lastOLEDUpdate >= OLED_UPDATE_INTERVAL) {
    updateOLEDDisplay();
    lastOLEDUpdate = millis();
  }
  
  // Small delay to prevent excessive CPU usage
  delay(10);
}

void switchToBlueMode() {
  if (currentMode != BLUE_MODE) {
    Serial.println("Switching to BLUE mode");
    currentMode = BLUE_MODE;
    
    // Play mode switch sound
    playSound(SOUND_MODE_SWITCH);
    
    // Set onboard LED solid ON for blue mode
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    
    // Display mode switch on OLED
    displayModeSwitch("BLUE PORTAL");
    
    // Smooth transition: fade out red, fade in blue
    fadeTransition(RED_LED_PIN, BLUE_LED_PIN);
    
    // Update OLED after transition
    updateOLEDDisplay();
  }
}

void switchToRedMode() {
  if (currentMode != RED_MODE) {
    Serial.println("Switching to RED mode");
    currentMode = RED_MODE;
    
    // Play mode switch sound
    playSound(SOUND_MODE_SWITCH);
    
    // Initialize blinking for red mode
    lastBlinkTime = millis();
    onboardLEDState = true;
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    
    // Display mode switch on OLED
    displayModeSwitch("RED PORTAL");
    
    // Smooth transition: fade out blue, fade in red
    fadeTransition(BLUE_LED_PIN, RED_LED_PIN);
    
    // Update OLED after transition
    updateOLEDDisplay();
  }
}

void shootPortal() {
  if (isShooting) return;  // Prevent multiple simultaneous shoots
  
  isShooting = true;
  totalPortalsFired++;  // Increment portal counter
  
  Serial.print("Shooting ");
  Serial.print(currentMode == BLUE_MODE ? "BLUE" : "RED");
  Serial.println(" portal!");
  
  // Get current active LED pin
  int activePin = (currentMode == BLUE_MODE) ? BLUE_LED_PIN : RED_LED_PIN;

  // Start audio immediately
  if (currentMode == BLUE_MODE) {
    playSound(SOUND_BLUE_PORTAL);
  } else {
    playSound(SOUND_RED_PORTAL);
  }
    
  // Portal shooting effect: SIMULTANEOUS with audio - shorter delays for sync
  for (int pulse = 0; pulse < 8; pulse++) {
    // Rapid pulse up - faster for better audio sync
    for (int brightness = DIM_BRIGHTNESS; brightness <= MAX_BRIGHTNESS; brightness += 20) {
      analogWrite(activePin, brightness);
      delay(15);  // Reduced from 20ms for better sync
    }
    
    // Rapid pulse down - faster for better audio sync
    for (int brightness = MAX_BRIGHTNESS; brightness >= DIM_BRIGHTNESS; brightness -= 20) {
      analogWrite(activePin, brightness);
      delay(15);  // Reduced from 20ms for better sync
    }
  }
  
  // Final dramatic flash - shorter to stay in sync with audio
  analogWrite(activePin, MAX_BRIGHTNESS);
  delay(100);  // Keep this brief
  
  // Auto-switch to opposite color (game mechanics)
  if (currentMode == BLUE_MODE) {
    currentMode = RED_MODE;
    Serial.println("Auto-switched to RED mode");
    
    // Initialize blinking for red mode
    lastBlinkTime = millis();
    onboardLEDState = true;
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    
    // Display auto-switch message
    displayAutoSwitch("RED");
    
    fadeTransition(BLUE_LED_PIN, RED_LED_PIN);
  } else {
    currentMode = BLUE_MODE;
    Serial.println("Auto-switched to BLUE mode");
    
    // Set onboard LED solid ON for blue mode
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    
    // Display auto-switch message
    displayAutoSwitch("BLUE");
    
    fadeTransition(RED_LED_PIN, BLUE_LED_PIN);
  }
  
  isShooting = false;
  
  // Update OLED to show new status
  updateOLEDDisplay();
}

void fadeTransition(int fromPin, int toPin) {
  // Smooth simultaneous fade - full power bank can handle both LEDs
  for (int step = 0; step <= DIM_BRIGHTNESS; step += 2) {
    // Fade out the old LED
    analogWrite(fromPin, DIM_BRIGHTNESS - step);
    
    // Fade in the new LED  
    analogWrite(toPin, step);
    
    delay(20);  // Smooth transition speed
  }
  
  // Ensure final states are correct
  analogWrite(fromPin, 0);
  analogWrite(toPin, DIM_BRIGHTNESS);
}

void setIdleMode() {
  // Set initial idle state with improved Red LED handling
  if (currentMode == BLUE_MODE) {
    analogWrite(BLUE_LED_PIN, DIM_BRIGHTNESS);  // Pin 5 - smooth dimming
    analogWrite(RED_LED_PIN, 0);                // Pin 6 - off
    digitalWrite(ONBOARD_LED_PIN, HIGH);        // Solid ON for blue mode
  } else {
    analogWrite(RED_LED_PIN, DIM_BRIGHTNESS);   // Pin 6 - smooth dimming
    analogWrite(BLUE_LED_PIN, 0);               // Pin 5 - off
    // Blinking will be handled by updateOnboardLED() for red mode
  }
}

void updateOnboardLED() {
  if (currentMode == BLUE_MODE) {
    // Blue mode: onboard LED stays solid ON
    digitalWrite(ONBOARD_LED_PIN, HIGH);
  } else {
    // Red mode: onboard LED blinks every 500ms
    if (millis() - lastBlinkTime >= BLINK_INTERVAL) {
      onboardLEDState = !onboardLEDState;
      digitalWrite(ONBOARD_LED_PIN, onboardLEDState);
      lastBlinkTime = millis();
    }
  }
}

// Audio Functions
void playSound(int trackNumber) {
  if (audioEnabled && trackNumber >= 1 && trackNumber <= 5) {
    Serial.print("Playing sound track: ");
    Serial.println(trackNumber);
    audioPlayer.play(trackNumber);
  }
}

void setVolume(int volume) {
  if (audioEnabled && volume >= 0 && volume <= 30) {
    audioPlayer.volume(volume);
    Serial.print("Volume set to: ");
    Serial.println(volume);
  }
}

// OLED Display Functions
void displayStartupSequence() {
  // Portal Gun logo
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setCursor(10, 0);
  display.println("PORTAL GUN v3.0");
  
  // Audio indicator
  display.setCursor(25, 16);
  if (audioEnabled) {
    display.println("AUDIO: READY");
  } else {
    display.println("AUDIO: OFFLINE");
  }
  
  display.display();
  delay(2000);
  
  // Initializing message
  display.clearDisplay();
  display.setCursor(20, 8);
  display.println("INITIALIZING...");
  display.display();
  delay(800);  // Shorter delay since we have audio feedback
  
  // Systems online
  display.clearDisplay();
  display.setCursor(15, 0);
  display.println("SYSTEMS ONLINE");
  display.setCursor(25, 16);
  display.println("READY TO FIRE");
  display.display();
  delay(1500);
}

void updateOLEDDisplay() {
  if (isShooting) return; // Don't update during shooting animations
  
  display.clearDisplay();
  
  // Top line: Portal Gun title with mode indicator
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("PORTAL GUN");
  
  // Mode indicator on top right
  display.setCursor(85, 0);
  if (currentMode == BLUE_MODE) {
    display.print("BLUE");
  } else {
    display.print("RED");
  }
  
  // Status line with audio indicator
  display.setCursor(0, 10);
  display.print("Status: READY");
  if (audioEnabled) {
    display.setCursor(85, 10);
    display.print("[♪]");  // Audio indicator
  }
  
  // Portal counter
  display.setCursor(0, 20);
  display.print("Fired: ");
  display.print(totalPortalsFired);
  
  // Battery indicator (simple)
  display.setCursor(70, 20);
  display.print("PWR:");
  
  // Animated power indicator
  int powerBars = 4; // Simulate full power
  for (int i = 0; i < powerBars; i++) {
    display.setCursor(100 + i * 3, 20);
    display.print("|");
  }
  
  // Portal mode symbol
  display.setTextSize(2);
  display.setCursor(110, 0);
  if (currentMode == BLUE_MODE) {
    display.print("O"); // Blue portal symbol
  } else {
    display.print("@"); // Red portal symbol (filled)
  }
  
  display.display();
}

void displayModeSwitch(String newMode) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(15, 0);
  display.println("MODE SWITCHING");
  
  display.setCursor(0, 16);
  display.print(">> ");
  display.print(newMode);
  display.print(" <<");
  
  display.display();
  delay(1000);
}

void displayFiringAnimation() {
  String currentPortal = (currentMode == BLUE_MODE) ? "BLUE" : "RED";
  
  // Simplified firing message - no long animation that delays LED effects
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(25, 0);
  display.print("FIRING ");
  display.print(currentPortal);
  
  // Quick progress indication without blocking delays
  display.drawRect(4, 16, 120, 8, SSD1306_WHITE);
  display.fillRect(6, 18, 116, 4, SSD1306_WHITE);  // Full bar immediately
  
  display.display();
  // No delay here - return immediately so LED pulsation can start with audio
}

void displayAutoSwitch(String newMode) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(20, 0);
  display.println("AUTO-SWITCH!");
  
  display.setCursor(15, 16);
  display.print("Next: ");
  display.print(newMode);
  display.print(" PORTAL");
  
  display.display();
  delay(1500);
}

// Power management with audio
void enterSleepMode() {
  Serial.println("Entering sleep mode...");
  
  // Play power down sound
  playSound(SOUND_POWER_DOWN);
  delay(2000);  // Wait for sound to finish
  
  // Turn off all LEDs
  analogWrite(BLUE_LED_PIN, 0);
  analogWrite(RED_LED_PIN, 0);
  digitalWrite(ONBOARD_LED_PIN, LOW);
  
  // Display sleep message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 8);
  display.println("SLEEP MODE");
  display.setCursor(15, 20);
  display.println("Press any key...");
  display.display();
  
  delay(1000);
  
  // Wake up sequence
  Serial.println("Waking up...");
  playSound(SOUND_POWER_UP);
  delay(1000);
  setIdleMode();
  updateOLEDDisplay();
}

// Optional: Startup light show
void startupSequence() {
  Serial.println("Portal Gun startup sequence...");
  
  // Alternate flashing
  for (int i = 0; i < 3; i++) {
    analogWrite(BLUE_LED_PIN, MAX_BRIGHTNESS);
    delay(200);
    analogWrite(BLUE_LED_PIN, 0);
    
    analogWrite(RED_LED_PIN, MAX_BRIGHTNESS);
    delay(200);
    analogWrite(RED_LED_PIN, 0);
  }
  
  // Final fade to idle mode
  setIdleMode();
}