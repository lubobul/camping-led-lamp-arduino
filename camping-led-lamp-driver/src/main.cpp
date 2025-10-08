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

// --- MODIFIED: State Machine Definition ---
enum LampMode {
  MODE_SMOOTH_DIM,
  MODE_PRESET_SELECT,
  MODE_STATS
};
LampMode currentMode = MODE_SMOOTH_DIM;

// --- Global State & Settings ---
int brightness = 50;
int lastBrightness = brightness;
bool isLampOn = true;
const int rotaryScaleFactor = 2;

// Add a flag to track if the long press has been triggered
bool longPressActionTaken = false;


// --- Forward Declarations for our new functions ---
void handleInputs();
void handleIrInputs();
void handleRotaryEncoderInputs();
void updateOutputs();
void drawDisplay();
void drawSmoothDimScreen(); 
// NEW: Forward declarations for new screens
void drawPresetScreen();
void drawStatsScreen();


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
    
    Serial.print(F("Free SRAM after all setup: "));
    Serial.println(getFreeSram());
    
}

// The main loop is now beautifully simple
void loop() {
    handleInputs();
    updateOutputs();
    drawDisplay();
}

// This is now a master function that calls the specific input handlers
void handleInputs() {
    handleIrInputs();
    handleRotaryEncoderInputs();
}

// All IR remote logic is in its own function
void handleIrInputs() {
    if (IrReceiver.decode()) {
        // Use a switch statement for clean code
        switch (IrReceiver.decodedIRData.decodedRawData) {
            case IR_CODE_POWER:
                isLampOn = !isLampOn;
                if (isLampOn) {
                    brightness = lastBrightness;
                    if (brightness < 10) brightness = 10;
                    currentMode = MODE_SMOOTH_DIM; // MODIFIED: Reset to main screen
                } else {
                    lastBrightness = brightness;
                }
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
}

// All rotary encoder logic is in its own function
void handleRotaryEncoderInputs() {
    debouncer.update();
    
    // --- MODIFIED: Cleaner logic for long and short press ---

    // Long Press logic (while holding)
    if (debouncer.read() == LOW) { 
        if (debouncer.duration() > 1000 && !longPressActionTaken) {
            isLampOn = !isLampOn; 
            if (isLampOn) {
                brightness = lastBrightness;
                if (brightness < 10) brightness = 10;
                currentMode = MODE_SMOOTH_DIM; // MODIFIED: Reset to main screen
                myEncoder.write(brightness * rotaryScaleFactor);
            } else {
                lastBrightness = brightness;
            }
            longPressActionTaken = true;
        }
    }

    // Short Press logic (on release)
    if (debouncer.rose()) { // Button was just released
        if (!longPressActionTaken) { // Only if it was NOT a long press
            currentMode = (LampMode)((currentMode + 1) % 3);
        }
        longPressActionTaken = false; // Reset for the next press cycle
    }


    // Read Encoder turn (only if lamp is on)
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

// All output logic is in its own function
void updateOutputs() {
    // We need to track the mode to force screen redraws
    static int lastDisplayedBrightness = -1;
    static bool lastDisplayedState = !isLampOn;
    static LampMode lastUpdatedMode = (LampMode)-1;

    // This section only runs if a state has changed
    if (brightness != lastDisplayedBrightness || isLampOn != lastDisplayedState || currentMode != lastUpdatedMode) {
        
        // Update PWM
        int pwmValue = (brightness == 0) ? 0 : map(brightness, 0, 100, 0, 511);
        OCR1A = pwmValue;

        // Update the 'last state' variables
        lastDisplayedBrightness = brightness;
        lastDisplayedState = isLampOn;
        lastUpdatedMode = currentMode;
    }
}

// All display drawing logic is in its own function
void drawDisplay() {
    u8g2.firstPage();
    do {
        // This now calls the correct drawing function based on the mode.
        // It also handles the global "OFF" state.
        if (!isLampOn) {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2_uint_t textWidth = u8g2.getStrWidth("OFF");
            u8g2.drawStr((128 - textWidth) / 2, 38, "OFF");
        } else {
            switch(currentMode) {
                case MODE_SMOOTH_DIM:
                    drawSmoothDimScreen();
                    break;
                case MODE_PRESET_SELECT:
                    drawPresetScreen();
                    break;
                case MODE_STATS:
                    drawStatsScreen();
                    break;
            }
        }
    } while (u8g2.nextPage());
}

// --- Function to draw the main screen ---
void drawSmoothDimScreen() {
    // Draw the title with a smaller font
    u8g2.setFont(u8g2_font_7x13B_tr);
    // MODIFIED: Center the title text
    u8g2_uint_t textWidth = u8g2.getStrWidth("Smooth Dimming");
    u8g2.drawStr((128 - textWidth) / 2, 12, "Smooth Dimming");
    u8g2.drawHLine(0, 15, 128);

    // Draw the main brightness value with a large font
    u8g2.setFont(u8g2_font_ncenB14_tr);
    char buffer[10];
    sprintf(buffer, "%d%%", brightness);
    textWidth = u8g2.getStrWidth(buffer);
    u8g2.drawStr((128 - textWidth) / 2, 45, buffer);
}

// --- NEW: Function to draw the preset screen ---
void drawPresetScreen() {
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2_uint_t textWidth = u8g2.getStrWidth("Select Preset");
    u8g2.drawStr((128 - textWidth) / 2, 12, "Select Preset");
    u8g2.drawHLine(0, 15, 128);
    // Content for this screen will be added in the next step
}

// --- NEW: Function to draw the stats screen ---
void drawStatsScreen() {
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2_uint_t textWidth = u8g2.getStrWidth("System Stats");
    u8g2.drawStr((128 - textWidth) / 2, 12, "System Stats");
    u8g2.drawHLine(0, 15, 128);
    // Content for this screen will be added in the next step
}

