#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h> // The efficient display library
#include <Encoder.h>
#include <Bounce2.h>
#include <IRremote.h>

// --- Pin Definitions ---
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define ENCODER_SWITCH_PIN 4
#define PWM_OUTPUT_PIN 9
#define IR_RECEIVE_PIN 7

// --- IR Code Definitions ---
#define IR_CODE_POWER 0xE31CFF00 // The code from your remote

// --- OLED Display Setup using U8g2 (Page Buffer Mode) ---
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- Input Component Setup ---
Encoder myEncoder(ENCODER_PIN_A, ENCODER_PIN_B);
Bounce debouncer = Bounce();

// --- Global State & Settings ---
int brightness = 50;
int lastBrightness = brightness;
bool isLampOn = true;
const int rotaryScaleFactor = 2;

// Variables to prevent unnecessary screen updates
int lastDisplayedBrightness = -1;
bool lastDisplayedState = !isLampOn;

// Helper function to measure free SRAM
int getFreeSram() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void setup() {
    // Initialize components in the correct order to prevent conflicts
    IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
    Serial.begin(9600);
    u8g2.begin();

    debouncer.attach(ENCODER_SWITCH_PIN, INPUT_PULLUP);
    debouncer.interval(25);

    pinMode(PWM_OUTPUT_PIN, OUTPUT);

    // Configure silent, ~31kHz PWM on Timer1 (Pin 9)
    TCCR1A = _BV(COM1A1) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
    ICR1 = 511;
    OCR1A = 0;

    myEncoder.write(brightness * rotaryScaleFactor);
    
    // Print the final memory usage
    Serial.print(F("Free SRAM after all setup: "));
    Serial.println(getFreeSram());
}

void loop() {
    // --- 1. Handle All Inputs ---
    
    // Check for IR remote signal
    if (IrReceiver.decode()) {
        if (IrReceiver.decodedIRData.decodedRawData == IR_CODE_POWER) {
            isLampOn = !isLampOn;
            if (isLampOn) {
                brightness = lastBrightness;
                myEncoder.write(brightness * rotaryScaleFactor);
            } else {
                lastBrightness = brightness;
            }
        }
        IrReceiver.resume();
    }

    // Check for physical button press
    debouncer.update();
    if (debouncer.fell()) {
        isLampOn = !isLampOn;
        if (isLampOn) {
            brightness = lastBrightness;
            myEncoder.write(brightness * rotaryScaleFactor);
        } else {
            lastBrightness = brightness;
        }
    }

    // Read Encoder (only if lamp is on)
    if (isLampOn) {
        long newEncoderValue = myEncoder.read() / rotaryScaleFactor;
        brightness = constrain(newEncoderValue, 0, 100);

        if (brightness != newEncoderValue) {
            myEncoder.write(brightness * rotaryScaleFactor);
        }
    } else {
        brightness = 0;
    }

    // --- 2. Update Outputs (PWM and Display) ---
    // This section only runs if a state has changed, which is very efficient
    if (brightness != lastDisplayedBrightness || isLampOn != lastDisplayedState) {
        
        // --- Update PWM ---
        int pwmValue;
        if (brightness == 0) {
            pwmValue = 0; // Force output completely off to prevent glow
        } else {
            pwmValue = map(brightness, 0, 100, 0, 511);
        }
        OCR1A = pwmValue; // Set the PWM duty cycle

        // --- Update Display using U8g2 ---
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_ncenB14_tr); // A nice 14-pixel bold font

            if (isLampOn) {
                char buffer[10];
                sprintf(buffer, "Pwr: %d%%", brightness);
                u8g2_uint_t textWidth = u8g2.getStrWidth(buffer);
                u8g2.drawStr((128 - textWidth) / 2, 38, buffer); // Center the text
            } else {
                u8g2_uint_t textWidth = u8g2.getStrWidth("OFF");
                u8g2.drawStr((128 - textWidth) / 2, 38, "OFF");
            }
        } while (u8g2.nextPage());

        // Update the 'last state' variables
        lastDisplayedBrightness = brightness;
        lastDisplayedState = isLampOn;
    }
}

