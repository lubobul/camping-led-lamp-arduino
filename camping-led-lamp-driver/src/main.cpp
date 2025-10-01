#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h> // The library for reading the rotary encoder

// --- Pin Definitions ---
// Define the pins for your rotary encoder's rotation function
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3

// Define the pin for your rotary encoder's push-button switch
#define ENCODER_SWITCH_PIN 4

// --- OLED Display Setup ---
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Encoder Setup ---
// Create an Encoder object that will read pins A and B
Encoder myEncoder(ENCODER_PIN_A, ENCODER_PIN_B);

// --- Global Variables ---
long oldEncoderValue = -999; // A variable to store the last known encoder value
bool buttonState = false;    // A variable to track the button state

void setup() {
  // --- Initialize OLED ---
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // If the display fails to initialize, do nothing.
    for(;;); 
  }

  // --- Initialize Encoder Pins ---
  // We use INPUT_PULLUP because we are connecting the common pin to GND.
  // This activates the Arduino's internal pull-up resistors, so we don't need external ones.
  pinMode(ENCODER_SWITCH_PIN, INPUT_PULLUP);

  // --- Initial Display Message ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Rotary Encoder Test");
  display.display();
  delay(2000); // Show message for 2 seconds
}

void loop() {
  // Read the current value from the encoder
  long newEncoderValue = myEncoder.read();

  // Read the button state. It will be LOW when pressed because of the pull-up resistor.
  bool newButtonState = (digitalRead(ENCODER_SWITCH_PIN) == LOW);

  // This 'if' block only runs if the encoder value has changed OR the button state has changed.
  // This is efficient and prevents the screen from flickering.
  if (newEncoderValue != oldEncoderValue || newButtonState != buttonState) {

    // Update the stored values
    oldEncoderValue = newEncoderValue;
    buttonState = newButtonState;

    // Clear the screen and set up the text style
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    // Display the current encoder value
    display.print("Val: ");
    display.println(newEncoderValue);

    // If the button is currently being pressed, display a message
    if (buttonState == true) {
      display.setTextSize(1);
      display.setCursor(0, 30);
      display.println("Button Pressed!");
    }
    
    // Send the updated information to the OLED display
    display.display();
  }
}
