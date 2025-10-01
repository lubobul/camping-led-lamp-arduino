#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

// --- Pin Definitions ---
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define ENCODER_SWITCH_PIN 4
#define PWM_OUTPUT_PIN 9 // Using pin D9 for safe, fast PWM

// --- OLED Display Setup ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Encoder Setup ---
Encoder myEncoder(ENCODER_PIN_A, ENCODER_PIN_B);

// --- Global Variables ---
int brightness = 50; // Start at 50% brightness
int lastDisplayedBrightness = -1; // To track when the display needs updating
bool isLampOn = true; // Start with the lamp ON
bool lastDisplayedState = false; // To track when the display needs updating

// Variables for button debouncing
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
bool buttonState = HIGH;      // The debounced, stable state of the switch
bool lastButtonState = HIGH;  // The last raw reading of the switch

void setup() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); 
  }

  pinMode(ENCODER_SWITCH_PIN, INPUT_PULLUP);
  pinMode(PWM_OUTPUT_PIN, OUTPUT);

  // Configure silent, ~31kHz PWM on Pin 9
  TCCR1A = _BV(COM1A1) | _BV(WGM11);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
  ICR1 = 511;
  OCR1A = 0;

  // Set the initial encoder position to match our starting brightness
  myEncoder.write(brightness * 4);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Dimmer Ready!");
  display.display();
  delay(1000); 
}

void loop() {
  // --- 1. Check for Button Press (On/Off) with Debouncing ---
  bool reading = digitalRead(ENCODER_SWITCH_PIN);

  // If the switch state has changed, reset the debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // After the debounce delay has passed, check the state again
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the reading has been stable and is different from the last known stable state
    if (reading != buttonState) {
      buttonState = reading; // Update the stable state

      // If the new stable state is a press (transition to LOW)
      if (buttonState == LOW) {
        isLampOn = !isLampOn; // Toggle the on/off state
      }
    }
  }
  lastButtonState = reading; // Save the current reading for the next loop

  // --- 2. Read Encoder (only if lamp is on) ---
  if (isLampOn) {
    long newEncoderValue = myEncoder.read() / 4;
    brightness = constrain(newEncoderValue, 0, 100);
    if (brightness != newEncoderValue) {
      myEncoder.write(brightness * 4);
    }
  } else {
    // If lamp is off, ensure brightness is 0 and encoder is reset
    brightness = 0;
    myEncoder.write(0);
  }

  // --- 3. Update PWM and Display ---
  // This section only runs if the state has changed, to prevent flickering
  if (brightness != lastDisplayedBrightness || isLampOn != lastDisplayedState) {
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);

    if (isLampOn) {
      // --- Lamp is ON ---
      int pwmValue;
      // Specific check to fix the 0% glow issue
      if (brightness == 0) {
        pwmValue = 0; // Force output to be completely off
      } else {
        pwmValue = map(brightness, 0, 100, 0, 511);
      }
      OCR1A = pwmValue; 
      
      // Changed label to "Pwr:" to ensure it fits on one line
      display.setCursor(0, 20);
      display.print("Pwr: ");
      display.print(brightness);
      display.println("%");

    } else {
      // --- Lamp is OFF ---
      OCR1A = 0; // Ensure PWM is off
      
      display.setCursor(35, 20); // Center the text
      display.println("OFF");
    }

    display.display();
    lastDisplayedBrightness = brightness;
    lastDisplayedState = isLampOn;
  }
}

