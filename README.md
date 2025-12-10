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
