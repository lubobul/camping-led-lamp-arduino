#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
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
#define IR_CODE_POWER 0xE31CFF00
#define IR_CODE_UP 0xE718FF00
#define IR_CODE_DOWN 0xAD52FF00
#define IR_CODE_PRESET_1 0xBA45FF00
#define IR_CODE_PRESET_2 0xB946FF00
#define IR_CODE_PRESET_3 0xB847FF00
#define IR_CODE_PRESET_4 0xBB44FF00


// --- OLED Display Setup ---
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- Input Component Setup ---
Encoder myEncoder(ENCODER_PIN_A, ENCODER_PIN_B);
Bounce debouncer = Bounce();

// --- Global State & Settings ---
int brightness = 50;
int lastBrightness = brightness;
bool isLampOn = true;
const int rotaryScaleFactor = 2;

// --- Forward Declarations for our new functions ---
void handleInputs();
void updateOutputs();
void drawDisplay();

// Helper function to measure free SRAM
int getFreeSram() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void setup() {
    IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
    Serial.begin(9600);
    u8g2.begin();

    debouncer.attach(ENCODER_SWITCH_PIN, INPUT_PULLUP);
    debouncer.interval(25);

    pinMode(PWM_OUTPUT_PIN, OUTPUT);

    TCCR1A = _BV(COM1A1) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
    ICR1 = 511;
    OCR1A = 0;

    myEncoder.write(brightness * rotaryScaleFactor);
    u8g2.setFont(u8g2_font_ncenB14_tr);

    Serial.print(F("Free SRAM after all setup: "));
    Serial.println(getFreeSram());
    
}

// The main loop is now beautifully simple
void loop() {
    handleInputs();
    updateOutputs();
    drawDisplay();
}

// --- NEW: All input logic is now in its own function ---
void handleInputs() {
    // Check for IR remote signal
    if (IrReceiver.decode()) {
        // Use a switch statement for clean code
        switch (IrReceiver.decodedIRData.decodedRawData) {
            case IR_CODE_POWER:
                isLampOn = !isLampOn;
                if (isLampOn) brightness = lastBrightness; else lastBrightness = brightness;
                break;
            
            case IR_CODE_UP:
                if (isLampOn) brightness += 5; // Increase brightness by 5
                break;

            case IR_CODE_DOWN:
                if (isLampOn) brightness -= 5; // Decrease brightness by 5
                break;

            case IR_CODE_PRESET_1:
                if (isLampOn) brightness = 10;
                break;
            
            case IR_CODE_PRESET_2:
                if (isLampOn) brightness = 30;
                break;

            case IR_CODE_PRESET_3:
                if (isLampOn) brightness = 70;
                break;

            case IR_CODE_PRESET_4:
                if (isLampOn) brightness = 100;
                break;
        }
        
        // After any IR command that changes brightness, constrain and update encoder
        brightness = constrain(brightness, 0, 100);
        myEncoder.write(brightness * rotaryScaleFactor);

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
}

// --- NEW: All output logic is now in its own function ---
void updateOutputs() {
    static int lastDisplayedBrightness = -1;
    static bool lastDisplayedState = !isLampOn;

    // This section only runs if a state has changed
    if (brightness != lastDisplayedBrightness || isLampOn != lastDisplayedState) {
        
        // --- Update PWM ---
        int pwmValue = (brightness == 0) ? 0 : map(brightness, 0, 100, 0, 511);
        OCR1A = pwmValue;

        // Update the 'last state' variables
        lastDisplayedBrightness = brightness;
        lastDisplayedState = isLampOn;
    }
}

// --- NEW: All display drawing logic is now in its own function ---
void drawDisplay() {
    u8g2.firstPage();
    do {
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
}

