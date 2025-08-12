#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// Inisialisasi objek ADS1115
Adafruit_ADS1115 ads;

// Konstanta (menggunakan PROGMEM untuk menghemat RAM)
const float MIN_VOLTAGE = 400.0;
const float MAX_VOLTAGE = 600.0;
const float MAX_ADC_V = 4.9;
const float ADC_REF = 6.144;
const int16_t ADC_MAX = 32767;

// Variabel kalibrasi (simplified)
float cal_offset = 0.0;
float cal_scale = 1.0;

// Filtering - reduced size
const byte SAMPLES = 8;  // Reduced from 20 to 8
float readings[SAMPLES];
byte readIndex = 0;
float readSum = 0;
bool ready = false;

// Timing
unsigned long lastRead = 0;
const unsigned int INTERVAL = 100;

// Mode
byte mode = 0; // 0=normal, 1=calibrate

void setup() {
  Serial.begin(115200);
  
  Serial.println(F("=== Nano HV Monitor ==="));
  
  if (!ads.begin()) {
    Serial.println(F("ADS1115 Failed!"));
    while(1);
  }
  
  ads.setGain(GAIN_TWOTHIRDS);
  
  // Initialize readings array
  for (byte i = 0; i < SAMPLES; i++) {
    readings[i] = 0;
  }
  
  Serial.println(F("Ready! Commands:"));
  Serial.println(F("N-Normal, C-Cal, I-Info"));
  printHeader();
}

void loop() {
  // Check commands
  if (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }
  
  // Main operation
  if (millis() - lastRead >= INTERVAL) {
    lastRead = millis();
    
    if (mode == 0) {
      normalMode();
    } else {
      calibrateMode();
    }
  }
}

void handleCommand(char cmd) {
  switch (cmd) {
    case 'n':
    case 'N':
      mode = 0;
      Serial.println(F("\n--- NORMAL ---"));
      printHeader();
      break;
      
    case 'c':
    case 'C':
      mode = 1;
      Serial.println(F("\n--- CALIBRATE ---"));
      Serial.println(F("Apply 400.0V, press 1"));
      Serial.println(F("Apply 800V, press 2"));
      Serial.println(F("Reset cal, press R"));
      break;
      
    case 'i':
    case 'I':
      printInfo();
      break;
      
    case '1':
      if (mode == 1) calibratePoint(400.0);
      break;
      
    case '2':
      if (mode == 1) calibratePoint(600.0);
      break;
      
    case 'r':
    case 'R':
      cal_offset = 0.0;
      cal_scale = 1.0;
      Serial.println(F("Cal reset"));
      break;
  }
}

void normalMode() {
  float voltage = readVoltage();
  
  Serial.print(millis());
  Serial.print(F("\t"));
  Serial.print(voltage, 1);
  
  if (ready) {
    Serial.print(F("\t"));
    Serial.print(getFiltered(), 1);
  } else {
    Serial.print(F("\tStabilizing"));
  }
  
  Serial.println();
}

void calibrateMode() {
  float voltage = readVoltage();
  Serial.print(F("Current: "));
  Serial.print(voltage, 2);
  Serial.println(F("V"));
}

float readVoltage() {
  // Read ADC
  int16_t raw = ads.readADC_SingleEnded(0);
  float adcV = (raw * ADC_REF) / ADC_MAX;
  
  if (adcV < 0) adcV = 0;
  if (adcV > MAX_ADC_V) adcV = MAX_ADC_V;
  
  // Map to voltage
  float voltage = map_f(adcV, 0.0, MAX_ADC_V, MIN_VOLTAGE, MAX_VOLTAGE);
  
  // Apply calibration
  voltage = (voltage * cal_scale) + cal_offset;
  
  // Add to filter
  readSum -= readings[readIndex];
  readings[readIndex] = voltage;
  readSum += readings[readIndex];
  readIndex = (readIndex + 1) % SAMPLES;
  
  if (readIndex == 0 && !ready) {
    ready = true;
  }
  
  return voltage;
}

float getFiltered() {
  return readSum / SAMPLES;
}

void calibratePoint(float known) {
  Serial.print(F("Measuring for "));
  Serial.print(known, 0);
  Serial.println(F("V..."));
  
  // Take average of multiple readings
  float sum = 0;
  for (byte i = 0; i < 20; i++) {
    sum += readVoltage();
    delay(50);
  }
  float measured = sum / 20.0;
  
  Serial.print(F("Measured: "));
  Serial.print(measured, 2);
  Serial.println(F("V"));
  
  // Simple 2-point calibration
  static float known1 = 0, measured1 = 0;
  static float known2 = 0, measured2 = 0;
  static bool point1Set = false;
  
  if (known == 400.0) {
    known1 = known;
    measured1 = measured;
    point1Set = true;
    Serial.println(F("Point 1 set"));
  } else if (known == 600.0 && point1Set) {
    known2 = known;
    measured2 = measured;
    
    // Calculate calibration coefficients
    cal_scale = (known2 - known1) / (measured2 - measured1);
    cal_offset = known1 - (cal_scale * measured1);
    
    Serial.println(F("Calibration done!"));
    Serial.print(F("Scale: "));
    Serial.println(cal_scale, 4);
    Serial.print(F("Offset: "));
    Serial.println(cal_offset, 2);
    
    point1Set = false; // Reset for next calibration
  }
}

void printInfo() {
  Serial.println(F("\n=== INFO ==="));
  Serial.println(F("Board: Arduino Nano"));
  Serial.println(F("ADC: ADS1115 16-bit"));
  Serial.println(F("Range: 400.0-800V"));
  Serial.print(F("Scale: "));
  Serial.println(cal_scale, 4);
  Serial.print(F("Offset: "));
  Serial.println(cal_offset, 2);
  Serial.println(F("============\n"));
}

void printHeader() {
  Serial.println(F("Time(ms)\tVoltage(V)\tFiltered(V)"));
  Serial.println(F("--------\t----------\t-----------"));
}

// Optimized map function
float map_f(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}