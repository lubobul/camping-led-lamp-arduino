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
#define VOLTAGE_SENSE_PIN A1

// --- IR Code Definitions ---
#define IR_CODE_POWER 0xE31CFF00
#define IR_CODE_UP 0xE718FF00
#define IR_CODE_DOWN 0xAD52FF00
#define IR_CODE_PRESET_1 0xBA45FF00
#define IR_CODE_PRESET_2 0xB946FF00
#define IR_CODE_PRESET_3 0xB847FF00
#define IR_CODE_PRESET_4 0xBB44FF00

// --- MODIFIED: ADC Calibration Constants ---
// These are the real-world values you measured with the calibration sketch.
const int ADC_LOW = 684;  // Raw ADC value corresponding to 6.4V
const int ADC_HIGH = 895; // Raw ADC value corresponding to 8.4V (adjusted for headroom)

// --- OLED Display Setup ---
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- Input Component Setup ---
Encoder myEncoder(ENCODER_PIN_A, ENCODER_PIN_B);
Bounce debouncer = Bounce();

// --- State Machine Definition ---
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
bool longPressActionTaken = false;

// Variables for preset menu
int highlightedPreset = 0;
// Presets adjusted for better perceived brightness steps
const int presets[] = {10, 25, 50, 100};

// Variables for battery monitoring
float batteryVoltage = 0.0;
int batteryPercent = 0;

// --- Forward Declarations ---
void handleInputs();
void handleIrInputs();
void handleRotaryEncoderInputs();
void updateOutputs();
void drawDisplay();
void drawSmoothDimScreen();
void drawPresetScreen();
void drawStatsScreen();
void updateBatteryStats();

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

    analogReference(INTERNAL);

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

void loop() {
    handleInputs();
    updateBatteryStats();
    updateOutputs();
    drawDisplay();
}

void handleInputs() {
    handleIrInputs();
    handleRotaryEncoderInputs();
}

void handleIrInputs() {
    if (IrReceiver.decode()) {
        switch (IrReceiver.decodedIRData.decodedRawData) {
            case IR_CODE_POWER:
                isLampOn = !isLampOn;
                if (isLampOn) {
                    brightness = lastBrightness;
                    if (brightness < 10) brightness = 10;
                    currentMode = MODE_SMOOTH_DIM;
                } else {
                    lastBrightness = brightness;
                }
                break;
            case IR_CODE_UP: if (isLampOn) brightness += 5; break;
            case IR_CODE_DOWN: if (isLampOn) brightness -= 5; break;
            case IR_CODE_PRESET_1: if (isLampOn) brightness = 10; break;
            case IR_CODE_PRESET_2: if (isLampOn) brightness = 25; break;
            case IR_CODE_PRESET_3: if (isLampOn) brightness = 50; break;
            case IR_CODE_PRESET_4: if (isLampOn) brightness = 100; break;
        }
        brightness = constrain(brightness, 0, 100);
        myEncoder.write(brightness * rotaryScaleFactor);
        IrReceiver.resume();
    }
}

void handleRotaryEncoderInputs() {
    debouncer.update();

    if (debouncer.read() == LOW) {
        if (debouncer.duration() > 1000 && !longPressActionTaken) {
            if (currentMode == MODE_PRESET_SELECT) {
                brightness = presets[highlightedPreset];
                currentMode = MODE_SMOOTH_DIM;
                myEncoder.write(brightness * rotaryScaleFactor);
            } else {
                isLampOn = !isLampOn;
                if (isLampOn) {
                    brightness = lastBrightness;
                    if (brightness < 10) brightness = 10;
                    currentMode = MODE_SMOOTH_DIM;
                    myEncoder.write(brightness * rotaryScaleFactor);
                } else {
                    lastBrightness = brightness;
                }
            }
            longPressActionTaken = true;
        }
    }

    if (debouncer.rose()) {
        if (!longPressActionTaken) {
            currentMode = (LampMode)((currentMode + 1) % 3);
            switch(currentMode) {
                case MODE_SMOOTH_DIM: myEncoder.write(brightness * rotaryScaleFactor); break;
                case MODE_PRESET_SELECT: myEncoder.write(highlightedPreset * 4); break;
                case MODE_STATS: break;
            }
        }
        longPressActionTaken = false;
    }

    if (isLampOn) {
        long newEncoderValue;
        switch(currentMode) {
            case MODE_SMOOTH_DIM:
                newEncoderValue = myEncoder.read() / rotaryScaleFactor;
                brightness = constrain(newEncoderValue, 0, 100);
                if (brightness != newEncoderValue) myEncoder.write(brightness * rotaryScaleFactor);
                break;
            case MODE_PRESET_SELECT:
                newEncoderValue = myEncoder.read() / 4;
                highlightedPreset = (newEncoderValue % 4 + 4) % 4;
                break;
            case MODE_STATS: break;
        }
    } else {
        brightness = 0;
    }
}

// MODIFIED: This function now uses the new calibration constants.
void updateBatteryStats() {
    static unsigned long lastBatteryCheckTime = 0;
    if (millis() - lastBatteryCheckTime > 2000) {
        lastBatteryCheckTime = millis();

        long rawValueSum = 0;
        const int numReadings = 10;
        for (int i = 0; i < numReadings; i++) {
            rawValueSum += analogRead(VOLTAGE_SENSE_PIN);
        }
        float averageRawValue = (float)rawValueSum / numReadings;
        
        // --- This is the new, calibrated logic ---
        // Map the measured raw ADC range to the known voltage range (6.4V to 8.4V)
        // We use 640 and 840 to do integer math before dividing to a float.
        batteryVoltage = map(averageRawValue, ADC_LOW, ADC_HIGH, 640, 840) / 100.0;
        
        // Map the same raw ADC range to a 0-100% scale
        batteryPercent = map(averageRawValue, ADC_LOW, ADC_HIGH, 0, 100);
        
        // Constrain the final values to prevent out-of-bounds results
        batteryVoltage = constrain(batteryVoltage, 6.4, 8.4);
        batteryPercent = constrain(batteryPercent, 0, 100);
    }
}

void updateOutputs() {
    int pwmValue = (isLampOn) ? map(brightness, 0, 100, 0, 511) : 0;
    if(brightness == 0 && isLampOn) pwmValue = 0;
    OCR1A = pwmValue;
}

void drawDisplay() {
    static int lastDisplayedBrightness = -1;
    static bool lastDisplayedState = !isLampOn;
    static LampMode lastUpdatedMode = (LampMode)-1;
    static int lastHighlightedPreset = -1;
    static int lastBatteryPercent = -1;

    if (brightness != lastDisplayedBrightness || isLampOn != lastDisplayedState || currentMode != lastUpdatedMode || highlightedPreset != lastHighlightedPreset || batteryPercent != lastBatteryPercent) {
        u8g2.firstPage();
        do {
            if (!isLampOn) {
                u8g2.setFont(u8g2_font_ncenB14_tr);
                u8g2_uint_t textWidth = u8g2.getStrWidth("OFF");
                u8g2.drawStr((128 - textWidth) / 2, 38, "OFF");
            } else {
                switch(currentMode) {
                    case MODE_SMOOTH_DIM: drawSmoothDimScreen(); break;
                    case MODE_PRESET_SELECT: drawPresetScreen(); break;
                    case MODE_STATS: drawStatsScreen(); break;
                }
            }
        } while (u8g2.nextPage());

        lastDisplayedBrightness = brightness;
        lastDisplayedState = isLampOn;
        lastUpdatedMode = currentMode;
        lastHighlightedPreset = highlightedPreset;
        lastBatteryPercent = batteryPercent;
    }
}

void drawSmoothDimScreen() {
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2_uint_t textWidth = u8g2.getStrWidth("Smooth Dimming");
    u8g2.drawStr((128 - textWidth) / 2, 12, "Smooth Dimming");
    u8g2.drawHLine(0, 15, 128);

    u8g2.setFont(u8g2_font_ncenB14_tr);
    char buffer[10];
    sprintf(buffer, "%d%%", brightness);
    textWidth = u8g2.getStrWidth(buffer);
    u8g2.drawStr((128 - textWidth) / 2, 45, buffer);
}

void drawPresetScreen() {
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2_uint_t textWidth = u8g2.getStrWidth("Select Preset");
    u8g2.drawStr((128 - textWidth) / 2, 12, "Select Preset");
    u8g2.drawHLine(0, 15, 128);

    for (int i = 0; i < 4; i++) {
        char buffer[5];
        sprintf(buffer, "%d%%", presets[i]);
        u8g2_uint_t textWidth = u8g2.getStrWidth(buffer);
        int xPos = 30 * i + 5;
        if (i == highlightedPreset) {
            u8g2.drawBox(xPos, 25, textWidth + 6, 15);
            u8g2.setDrawColor(0);
            u8g2.drawStr(xPos + 3, 38, buffer);
            u8g2.setDrawColor(1);
        } else {
            u8g2.drawStr(xPos + 3, 38, buffer);
        }
    }

    u8g2.setFont(u8g2_font_6x12_tr);
    const char* helpText = "Long-Press to Select";
    textWidth = u8g2.getStrWidth(helpText);
    u8g2.drawStr((128 - textWidth) / 2, 58, helpText);
}

void drawStatsScreen() {
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2_uint_t textWidth = u8g2.getStrWidth("System Stats");
    u8g2.drawStr((128 - textWidth) / 2, 12, "System Stats");
    u8g2.drawHLine(0, 15, 128);

    // This now displays the live, calibrated battery data
    char voltageString[6];
    dtostrf(batteryVoltage, 4, 2, voltageString);
    char buffer[20];
    sprintf(buffer, "Batt: %d%% (%sV)", batteryPercent, voltageString);
    u8g2.drawStr(0, 35, buffer);

    u8g2.drawStr(0, 55, "Temp: -- C");
}