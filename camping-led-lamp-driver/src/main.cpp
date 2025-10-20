#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Encoder.h>
#include <Bounce2.h>
#include <IRremote.h>
// NEW: Include libraries for the temperature sensor
#include <OneWire.h>
#include <DallasTemperature.h>

// --- Pin Definitions ---
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define ENCODER_SWITCH_PIN 4
#define PWM_OUTPUT_PIN 9
#define IR_RECEIVE_PIN 7
#define VOLTAGE_SENSE_PIN A1
// NEW: Pin for the DS18B20 temperature sensor
#define ONEWIRE_BUS_PIN 8 


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

// NEW: Setup for the temperature sensor
OneWire oneWire(ONEWIRE_BUS_PIN);
DallasTemperature sensors(&oneWire);


// --- UI State Machine Definition ---
enum LampMode {
  MODE_SMOOTH_DIM,
  MODE_PRESET_SELECT,
  MODE_STATS
};
LampMode currentMode = MODE_SMOOTH_DIM;

// NEW: Master Operational State Machine Definition
enum OperationalState {
  STATE_OPERATING,
  STATE_CHARGING,
  STATE_LOW_BATTERY,
  STATE_OVERHEAT
};
OperationalState currentState = STATE_OPERATING;


// --- Global State & Settings ---
int brightness = 50;
int lastBrightness = brightness;
bool isLampOn = true;
const int rotaryScaleFactor = 2;
bool longPressActionTaken = false;

int highlightedPreset = 0;
const int presets[] = {10, 25, 50, 100};

// Variables for battery monitoring
float batteryVoltage = 0.0;
int batteryPercent = 0;

// NEW: Variable for temperature monitoring
float ledTemperature = 0.0;


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
long readVcc();
// NEW: Forward declaration for temperature function
void updateTemperature();

void setup() {
    IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
    // MODIFIED: Serial communication removed
    u8g2.begin();

    analogReference(DEFAULT);

    debouncer.attach(ENCODER_SWITCH_PIN, INPUT_PULLUP);
    debouncer.interval(25);

    // NEW: Initialize the temperature sensor
    sensors.begin();
    // NEW: Set the sensor to non-blocking mode
    sensors.setWaitForConversion(false);

    pinMode(PWM_OUTPUT_PIN, OUTPUT);

    TCCR1A = _BV(COM1A1) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
    ICR1 = 511;
    OCR1A = 0;

    myEncoder.write(brightness * rotaryScaleFactor);

    // MODIFIED: Serial communication removed
}

void loop() {
    // MODIFIED: Main loop is now controlled by the master state machine
    switch (currentState) {
        case STATE_OPERATING:
            // --- Fast Loop (runs thousands of times per second) ---
            // These functions are always called to ensure maximum responsiveness.
            handleInputs();
            updateOutputs();
            drawDisplay();

            // --- A proper, non-interfering scheduler for slow tasks ---
            static unsigned long lastBatteryCheckTime = 0;
            static unsigned long lastTempCheckTime = 1000; // Offset by 1 second to prevent collision

            const unsigned long SENSOR_UPDATE_INTERVAL = 2000; // 2 seconds

            unsigned long currentTime = millis();

            // Check and run the battery update task
            if (currentTime - lastBatteryCheckTime > SENSOR_UPDATE_INTERVAL) {
                lastBatteryCheckTime = currentTime;
                updateBatteryStats();
            }

            // Check and run the temperature update task on a different, offset schedule
            if (currentTime - lastTempCheckTime > SENSOR_UPDATE_INTERVAL) {
                lastTempCheckTime = currentTime;
                updateTemperature(); 
            }
            break;

        case STATE_CHARGING:
            // Placeholder for charging logic
            break;

        case STATE_LOW_BATTERY:
            // Placeholder for low battery logic
            break;

        case STATE_OVERHEAT:
            // Placeholder for overheat logic
            break;
    }
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

void updateBatteryStats() {
    // Step 1: Measure the true VCC voltage in millivolts
    long vcc_mV = readVcc();

    // Step 2: Read the averaged value from our voltage divider
    long rawValueSum = 0;
    const int numReadings = 10;
    for (int i = 0; i < numReadings; i++) {
        rawValueSum += analogRead(VOLTAGE_SENSE_PIN);
    }
    float averageRawValue = (float)rawValueSum / numReadings;

    // Step 3: Calculate the true voltage at the pin
    // V_pin = (ADC reading / ADC max) * Vcc
    float pinVoltage = (averageRawValue / 1023.0) * (vcc_mV / 1000.0);

    // Step 4: Convert back to the real battery voltage
    // Divider ratio for 68k/10k is (68+10)/10 = 7.8
    batteryVoltage = pinVoltage * 7.8;
    
    // Step 5: Calculate percentage
    batteryPercent = map(batteryVoltage * 100, 640, 830, 0, 100);
    batteryPercent = constrain(batteryPercent, 0, 100);
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
    // NEW: Track temperature for redraws
    static float lastTemperature = -999; 

    // NEW: The 'if' condition now includes the temperature
    if (brightness != lastDisplayedBrightness ||
         isLampOn != lastDisplayedState ||
          currentMode != lastUpdatedMode ||
           highlightedPreset != lastHighlightedPreset ||
            batteryPercent != lastBatteryPercent ||
             ledTemperature != lastTemperature) {
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
        // NEW: Update the last known temperature
        lastTemperature = ledTemperature; 
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

    char voltageString[6];
    dtostrf(batteryVoltage, 4, 2, voltageString);
    char buffer[20];
    sprintf(buffer, "Batt: %d%% (%sV)", batteryPercent, voltageString);
    u8g2.drawStr(0, 35, buffer);

    // NEW: Display the live temperature
    char tempString[6];
    dtostrf(ledTemperature, 4, 1, tempString);
    sprintf(buffer, "Temp: %s C", tempString);
    u8g2.drawStr(0, 55, buffer);
}

long readVcc() {
  // Selects the 1.1V internal reference as the ADC input.
  // The ADC reference is still the default VCC.
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // Wait for conversion to complete
  long result = ADC;
  // Calculate Vcc in millivolts
  // 1.1V * 1023 ADC steps = 1125.3. We use 1125300L for integer math.
  result = (1125300L / result);
  return result; // Vcc in millivolts
}

//MODIFIED: This function is now fully non-blocking.
void updateTemperature() {
    static unsigned long lastTempRequestTime = 0;
    static bool conversionStarted = false;

    // This runs in a two-step, non-blocking way.
    // On the first pass, it requests the temperature.
    // On the second pass (after the conversion time), it reads it.
    if (!conversionStarted) {
        sensors.requestTemperatures(); // Send the (non-blocking) command
        conversionStarted = true;
        lastTempRequestTime = millis();
    }
    
    // The sensor needs ~750ms to convert. We check after that time has passed.
    if (conversionStarted && millis() - lastTempRequestTime > 800) {
        float tempC = sensors.getTempCByIndex(0);

        if (tempC != DEVICE_DISCONNECTED_C) {
            ledTemperature = tempC;
        }
        conversionStarted = false;
    }
}

