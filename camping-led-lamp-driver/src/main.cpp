#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <Bounce2.h>

// --- Pin Definitions ---
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define ENCODER_SWITCH_PIN 4
#define PWM_OUTPUT_PIN 9 // Using pin D9 for safe, fast PWM

// --- OLED Display Setup ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Input Component Setup ---
Encoder myEncoder(ENCODER_PIN_A, ENCODER_PIN_B);
Bounce debouncer = Bounce();

// --- Global State & Settings ---
int brightness = 50;
int lastBrightness = brightness;
bool isLampOn = true;
const int rotaryScaleFactor = 2; //Set to 2 for faster response. 1 click = 2 steps.

// Variables to prevent unnecessary screen updates
int lastDisplayedBrightness = -1;
bool lastDisplayedState = !isLampOn;

void setup() {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        for (;;);
    }

    debouncer.attach(ENCODER_SWITCH_PIN, INPUT_PULLUP);
    debouncer.interval(25);

    pinMode(PWM_OUTPUT_PIN, OUTPUT);

    // Configure silent, ~31kHz PWM on Timer1 (Pin 9)
    TCCR1A = _BV(COM1A1) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
    ICR1 = 511;
    OCR1A = 0;

    myEncoder.write(brightness * rotaryScaleFactor);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 30);
    display.println("Lamp Ready!");
    display.display();
    delay(2000);
}

void loop() {
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

    if (isLampOn) {

        long newEncoderValue = myEncoder.read() / rotaryScaleFactor;
        brightness = constrain(newEncoderValue, 0, 100);

        if (brightness != newEncoderValue) {
            myEncoder.write(brightness * rotaryScaleFactor);
        }
    } else {
        brightness = 0;
    }

    if (brightness != lastDisplayedBrightness || isLampOn != lastDisplayedState) {
        int pwmValue;
        if (brightness == 0) {
            pwmValue = 0;
        } else {
            pwmValue = map(brightness, 0, 100, 0, 511);
        }
        OCR1A = pwmValue;

        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 20);

        if (isLampOn) {
            display.print("Pwr: ");
            display.print(brightness);
            display.println("%");
        } else {
            display.setCursor(35, 20);
            display.println("OFF");
        }
        display.display();

        lastDisplayedBrightness = brightness;
        lastDisplayedState = isLampOn;
    }
}

