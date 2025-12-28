#include <LiquidCrystal.h>
#include <Wire.h>
#include <PinChangeInterrupt.h> // Install via Library Manager: "PinChangeInterrupt" by NicoHood

// ===== LCD Setup =====
// RS, E, D4, D5, D6, D7
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

bool hasStoppedFor20cm = false;

// ===== Timer & State =====
unsigned long startTime = 0;
unsigned long currentTime = 0;
unsigned long lastLCDUpdate = 0; 
bool isStopped = false;

// ===== Sensor Pins =====
const int LEFT_IR = A2;
const int RIGHT_IR = A3;

// ===== Encoder Pins =====
// A4/A5 require PinChangeInterrupt on Uno/Nano
const uint8_t encoderRightPin = A5;
const uint8_t encoderLeftPin = A4;


// ===== Dimensions & Accuracy =====
const float pulsesPerRev = 40.0; 
const float wheelDiameter = 6.0; // in cm
const float wheelCircumference = 3.1416 * wheelDiameter; 

// ===== Motor Pins =====
const int ENA = 3;   // Left Speed
const int IN1 = 1;   // Left Dir (WARNING: Unplug this pin during Upload!)
const int IN2 = 2;   // Left Dir

const int ENB = 11;  // Right Speed
const int IN3 = 12;  // Right Dir
const int IN4 = 13;  // Right Dir

// ===== Speed Settings =====
int baseSpeed = 85; 
// Turns usually need more power to overcome friction
int turnSpeed = 180; 

// ===== Encoder Variables =====
volatile long rightCount = 0;
volatile long leftCount = 0;

// ===== ISR Handlers (The Counting Logic) =====
void onRightPulse() {
  rightCount++;
}

void onLeftPulse() {
  leftCount++;
}

void setup() {
  // 1. Init LCD
  lcd.begin(16, 2);
  lcd.print("Ready...");
  
  // 2. Init Sensors
  pinMode(LEFT_IR, INPUT);
  pinMode(RIGHT_IR, INPUT);

  // 3. Init Encoders
  pinMode(encoderLeftPin, INPUT_PULLUP); 
  pinMode(encoderRightPin, INPUT_PULLUP);

  // 4. Attach Interrupts
  // We use CHANGE, so it counts twice per hole (Rise + Fall)
  attachPCINT(digitalPinToPCINT(encoderLeftPin), onLeftPulse, CHANGE);
  attachPCINT(digitalPinToPCINT(encoderRightPin), onRightPulse, CHANGE);

  // 5. Init Motors
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  delay(1000);
  lcd.clear();
  startTime = millis(); // Start the clock
}

// ===== Motor Functions =====
void moveForward(int speedLeft, int speedRight) {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENA, speedLeft);

  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENB, speedRight);
}

void turnRight() {
  // Pivot Right: Left Motor Forward, Right Motor Backward (or Stopped)
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  analogWrite(ENA, 195);

  // Setup for sharp turn
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
  analogWrite(ENB, 235); 
  delay (190);
}

void turnLeft() {
  // Pivot Left: Left Motor Backward, Right Motor Forward
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENA, 220);

  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  analogWrite(ENB, 180);

  delay(170);
}

void stopCar() {
  // Cut power immediately
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  
  isStopped = true; // Set the flag to freeze logic
}

void loop() {
  // ===== FIX 2: STOP GUARD =====
  // If the car has finished, do nothing forever.
  if (isStopped) {
    stopCar(); // Ensure motors stay off
    
    // Optional: Show "DONE" on LCD once
    if (millis() % 1000 == 0) { // Simple blink effect logic check
        lcd.setCursor(11, 0);
        lcd.print("DONE");
    }
    return; // CRITICAL: Exits loop() here, skipping all movement logic
  }

  // ===== 1. Calculate Distance =====
  float rightDistance = ((float)rightCount / pulsesPerRev) * wheelCircumference;
  float leftDistance  = ((float)leftCount  / pulsesPerRev) * wheelCircumference;
  float avgDistance   = (rightDistance + leftDistance) / 2.0;

  // ===== 2. Timer Logic =====
  currentTime = millis() - startTime;

  // ===== 3. Update LCD (Every 200ms) =====
  if (millis() - lastLCDUpdate > 200) {
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(currentTime / 1000); // Seconds
    lcd.print(".");
    lcd.print((currentTime % 1000) / 100); // Tenths of seconds

    lcd.setCursor(0, 1);
    lcd.print("D:");
    lcd.print(avgDistance, 1); // Show 1 decimal place (e.g., 105.4)
    lcd.print("cm");
    
    lastLCDUpdate = millis();
  }

  // ===== 4. Read Sensors =====
  int leftValue = analogRead(LEFT_IR);
  int rightValue = analogRead(RIGHT_IR);

  // THRESHOLDS: Change these if line detection is poor
  // < 400 usually means WHITE surface (Reflection)
  // > 400 usually means BLACK line (Absorption) 
  // NOTE: Logic below assumes LOW value = BLACK (common for digital IR, check your specific sensor!)
  
  int Threshold = 300; // Adjust this based on your specific sensor test

  // Assuming: LOW signal (< Threshold) means BLACK LINE
  bool leftOnBlack = (leftValue < Threshold);
  bool rightOnBlack = (rightValue < Threshold);

  // ===== 5. Movement Logic =====
  
  // CASE A: Stop Line (Both sensors see black)
  if (leftOnBlack && rightOnBlack) {
    stopCar(); 
  }
  // CASE B: Straight (Neither sensor on black - Line is in middle)
  else if (!leftOnBlack && !rightOnBlack) {
    moveForward(baseSpeed, baseSpeed);
  }
  // CASE C: Drifted Right (Left sensor hit line) -> Correction: Turn Left
  else if (leftOnBlack && !rightOnBlack) {
    turnLeft();
  }
  // CASE D: Drifted Left (Right sensor hit line) -> Correction: Turn Right
  else if (!leftOnBlack && rightOnBlack) {
    turnRight();
  }
}