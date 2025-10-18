#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <AltSoftSerial.h>
#include <TinyGPS++.h>
#include <Servo.h>
#include <Wire.h>
#include <math.h>




// Common pins (used on both boards)
#define BUZZER_PIN 12
#define BUTTON_PIN 11

// Sensor pins
#define TILT_SENSOR_PIN 2    // Digital tilt sensor (UNO primary)
#define X_PIN A1             // Accelerometer X-axis (NANO primary)
#define Y_PIN A2             // Accelerometer Y-axis (NANO primary)
#define Z_PIN A3             // Accelerometer Z-axis (NANO primary)

// UNO specific pins
#define SERVO_PIN 6          // Servo motor control
#define LASER_PIN 7          // Laser warning system
#define BT_RX 4              // Bluetooth RX
#define BT_TX 3              // Bluetooth TX

// NANO specific pins (GSM)
#define GSM_RX 2             // GSM Module RX
#define GSM_TX 3             // GSM Module TX
// GPS uses AltSoftSerial (pins 8,9 on NANO)


// UNO Hardware
#ifdef ARDUINO_AVR_UNO
  SoftwareSerial bluetooth(BT_RX, BT_TX);
  Servo wheelServo;
  #define BOARD_TYPE "UNO"
  #define USE_SERVO true
  #define USE_BLUETOOTH true
  #define USE_GPS_GSM false
  #define USE_LCD false
#endif

// NANO Hardware
#ifdef ARDUINO_AVR_NANO
  LiquidCrystal_I2C lcd(0x27, 16, 2);
  SoftwareSerial sim800(GSM_RX, GSM_TX);
  AltSoftSerial neogps;
  TinyGPSPlus gps;
  #define BOARD_TYPE "NANO"
  #define USE_SERVO false
  #define USE_BLUETOOTH false
  #define USE_GPS_GSM true
  #define USE_LCD true
#endif

// Default to UNO if board not detected
#ifndef BOARD_TYPE
  SoftwareSerial bluetooth(BT_RX, BT_TX);
  Servo wheelServo;
  #define BOARD_TYPE "UNO"
  #define USE_SERVO true
  #define USE_BLUETOOTH true
  #define USE_GPS_GSM false
  #define USE_LCD false
#endif


// Common system variables
bool systemReady = false;
bool alarmActive = false;
bool emergencyMode = false;
unsigned long alarmStartTime = 0;
const unsigned long ALARM_DURATION = 10000; // 10 seconds

// Emergency contact
const String EMERGENCY_PHONE = "+8801975414473";

// UNO specific variables (Servo control)
#if USE_SERVO
bool servoStopped = false;
int servoPosition = 0;
int servoDirection = 1;
unsigned long lastServoMove = 0;
const int SERVO_DELAY = 30;
const int SERVO_STEP = 3;
#endif

// NANO specific variables (GPS/GSM/Accelerometer)
#if USE_GPS_GSM
String latitude = "0.000000";
String longitude = "0.000000";
bool gsm_ready = false;
unsigned long impact_time = 0;
unsigned long alert_delay = 10000; // 10 seconds
bool impact_detected = false;
byte updateflag = 0;
#endif

// Accelerometer variables (used on both boards)
int xaxis = 0, yaxis = 0, zaxis = 0;
int deltx = 0, delty = 0, deltz = 0;
int magnitude = 0;
int vibration = 2, devibrate = 75;
int sensitivity = 20;
unsigned long time1;

//==============================================================================
// FORWARD DECLARATIONS
//==============================================================================
void Impact();
void triggerAccident(String cause);
void cancelAllAlarms();
void processCommands();

#if USE_GPS_GSM
void getGps();
void sendAlert();
void makeCall();
void sendSms(String text);
boolean SendAT(String at_command, String expected_answer, unsigned int timeout);
#endif

#if USE_SERVO
void moveServoWheel();
void processCommand(String command, String source);
#endif


void setup() {
  Serial.begin(9600);
  
  Serial.println("===============================================");
  Serial.println("    INTEGRATED ACCIDENT DETECTION SYSTEM");
  Serial.println("    Board: Arduino " + String(BOARD_TYPE));
  Serial.println("===============================================");
  
  // Common pin setup
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TILT_SENSOR_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Board-specific setup
  setupBoardSpecific();
  
  // Calibrate accelerometer (both boards have this)
  calibrateAccelerometer();
  
  // System ready
  systemReady = true;
  time1 = micros();
  
  // Display ready message
  displaySystemReady();
  
  // Startup tone
  playStartupTone();
  
  Serial.println("=== SYSTEM READY ===");
  printAvailableCommands();
}

//==============================================================================
// BOARD-SPECIFIC SETUP
//==============================================================================
void setupBoardSpecific() {
#if USE_SERVO && USE_BLUETOOTH
  // Arduino UNO Setup
  Serial.println("Initializing UNO features...");
  
  // Bluetooth setup
  bluetooth.begin(9600);
  bluetooth.println("Integrated Accident Detection System - UNO Mode");
  bluetooth.println("Features: Servo Wheel + Tilt Sensor + Bluetooth");
  
  // Servo setup
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);
  
  wheelServo.attach(SERVO_PIN);
  servoPosition = 90;
  wheelServo.write(servoPosition);
  
  Serial.println("UNO: Servo wheel initialized");
  Serial.println("UNO: Bluetooth connected");
  Serial.println("UNO: Laser system ready");
  
#elif USE_GPS_GSM && USE_LCD
  // Arduino NANO Setup
  Serial.println("Initializing NANO features...");
  
  // LCD setup
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Starting");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
  delay(1000);
  
  // GSM setup
  setupGSM();
  
  // GPS setup
  setupGPS();
  
  lcd.clear();
  lcd.print("NANO Ready!");
  Serial.println("NANO: All systems initialized");
#endif
}

#if USE_GPS_GSM
void setupGSM() {
  lcd.clear();
  lcd.print("Checking GSM...");
  sim800.begin(9600);
  
  if (SendAT("AT", "OK", 5000)) {
    Serial.println("GSM: Responding");
    
    // Network registration
    lcd.clear();
    lcd.print("Wait Network...");
    boolean registered = false;
    byte dots = 0;
    int attempts = 0;
    
    while (!registered && attempts < 30) {
      sim800.println("AT+CREG?");
      delay(1000);
      
      if (sim800.find("0,1") || sim800.find("0,5")) {
        registered = true;
        break;
      }
      
      // Show progress on LCD
      lcd.setCursor(0, 1);
      lcd.print("Waiting");
      for (byte i = 0; i < dots; i++) lcd.print(".");
      for (byte i = dots; i < 3; i++) lcd.print(" ");
      dots = (dots + 1) % 4;
      attempts++;
    }
    
    if (registered) {
      gsm_ready = true;
      lcd.clear();
      lcd.print("GSM Connected!");
      Serial.println("GSM: Network registered");
      
      // Initialize SMS
      SendAT("AT+CPIN?", "READY", 5000);
      SendAT("AT+CMGF=1", "OK", 5000);
      SendAT("AT+CNMI=1,1,0,0,0", "OK", 5000);
    } else {
      Serial.println("GSM: Registration failed");
      lcd.clear();
      lcd.print("GSM Failed");
    }
  } else {
    Serial.println("GSM: Not responding");
    lcd.clear();
    lcd.print("GSM Not Found");
  }
  delay(1500);
}

void setupGPS() {
  lcd.clear();
  lcd.print("GPS Starting...");
  neogps.begin(9600);
  delay(2000);
  
  lcd.clear();
  lcd.print("GPS Ready");
  Serial.println("GPS: Initialized");
  delay(1000);
}
#endif

void calibrateAccelerometer() {
  Serial.println("Calibrating accelerometer...");
  
#if USE_LCD
  lcd.clear();
  lcd.print("Calibrating...");
#endif

  // Take stable readings
  long xSum = 0, ySum = 0, zSum = 0;
  for (int i = 0; i < 10; i++) {
    xSum += analogRead(X_PIN);
    ySum += analogRead(Y_PIN);
    zSum += analogRead(Z_PIN);
    delay(100);
  }
  
  xaxis = xSum / 10;
  yaxis = ySum / 10;
  zaxis = zSum / 10;
  
  Serial.println("Accelerometer calibrated: X=" + String(xaxis) + 
                " Y=" + String(yaxis) + " Z=" + String(zaxis));
}

void displaySystemReady() {
#if USE_LCD
  lcd.clear();
  lcd.print("System Ready!");
  lcd.setCursor(0, 1);
  lcd.print(String(BOARD_TYPE) + " Mode");
#endif

#if USE_BLUETOOTH
  bluetooth.println("System Ready - " + String(BOARD_TYPE) + " Mode");
  if (USE_SERVO) {
    bluetooth.println("Servo wheel active");
  }
#endif
}

void playStartupTone() {
  tone(BUZZER_PIN, 1000, 200);
  delay(300);
  tone(BUZZER_PIN, 1200, 200);
  delay(300);
  tone(BUZZER_PIN, 1500, 200);
  delay(500);
}

void printAvailableCommands() {
#if USE_BLUETOOTH
  Serial.println("UNO Commands: STOP, STATUS, TEST, TRIGGER, RESET, HELP");
#endif
#if USE_GPS_GSM
  Serial.println("NANO Features: GPS tracking, Emergency calls, SMS alerts");
#endif
  Serial.println("Both: Button press cancels alarms");
}

//==============================================================================
// MAIN LOOP
//==============================================================================
void loop() {
  // Process commands (UNO Bluetooth commands)
  processCommands();
  
  // Update servo wheel (UNO only)
#if USE_SERVO
  updateServoWheel();
#endif
  
  // Monitor all sensors for accidents
  monitorSensors();
  
  // Handle emergency situations (NANO GPS/GSM)
  handleEmergency();
  
  // Check for button press (both boards)
  if (digitalRead(BUTTON_PIN) == LOW && (alarmActive || emergencyMode)) {
    cancelAllAlarms();
    delay(500); // Debounce
  }
  
  // Check GSM responses (NANO only)
#if USE_GPS_GSM
  if (sim800.available()) {
    Serial.write(sim800.read());
  }
#endif
  
  delay(10);
}

//==============================================================================
// COMMAND PROCESSING (UNO Bluetooth)
//==============================================================================
void processCommands() {
  // Serial commands (both boards)
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();
    
#if USE_SERVO && USE_BLUETOOTH
    processCommand(command, "SERIAL");
#else
    Serial.println("Command received: " + command);
    if (command == "STATUS") {
      Serial.println("Board: " + String(BOARD_TYPE));
      Serial.println("Alarm: " + String(alarmActive ? "ACTIVE" : "OFF"));
      Serial.println("Tilt: " + String(digitalRead(TILT_SENSOR_PIN) == LOW ? "TRIGGERED" : "NORMAL"));
    }
#endif
  }
  
#if USE_BLUETOOTH
  // Bluetooth commands (UNO only)
  if (bluetooth.available()) {
    String command = bluetooth.readStringUntil('\n');
    command.trim();
    command.toUpperCase();
    processCommand(command, "BLUETOOTH");
  }
#endif
}

#if USE_SERVO && USE_BLUETOOTH
void processCommand(String command, String source) {
  Serial.println(source + " CMD: " + command);
  
  if (command == "STOP") {
    stopAlarmOnly();
    sendResponse("Alarm stopped", source);
  }
  else if (command == "STATUS") {
    String status = "Board: UNO | Alarm: " + String(alarmActive ? "ON" : "OFF");
    status += " | Tilt: " + String(digitalRead(TILT_SENSOR_PIN) == LOW ? "TILTED" : "NORMAL");
    status += " | Servo: " + String(servoStopped ? "STOPPED" : "MOVING");
    status += " | Laser: " + String(digitalRead(LASER_PIN) ? "ON" : "OFF");
    
    Serial.println(status);
    if (source == "BLUETOOTH") bluetooth.println(status);
  }
  else if (command == "TEST") {
    testAllSystems();
    sendResponse("Test complete", source);
  }
  else if (command == "TRIGGER") {
    if (!alarmActive) {
      triggerAccident("Manual trigger from " + source);
    } else {
      sendResponse("Already triggered", source);
    }
  }
  else if (command == "RESET") {
    resetSystem();
    sendResponse("System reset complete", source);
  }
  else if (command == "HELP") {
    String help = "Commands: STOP, STATUS, TEST, TRIGGER, RESET, HELP";
    Serial.println(help);
    if (source == "BLUETOOTH") bluetooth.println(help);
  }
  else if (command.length() > 0) {
    sendResponse("Unknown command: " + command, source);
  }
}

void sendResponse(String message, String source) {
  Serial.println(message);
  if (source == "BLUETOOTH") bluetooth.println(message);
}

void testAllSystems() {
  Serial.println("Testing all systems...");
  
  // Test buzzer
  tone(BUZZER_PIN, 1500, 1000);
  
  // Test laser
  digitalWrite(LASER_PIN, HIGH);
  delay(1000);
  digitalWrite(LASER_PIN, LOW);
  
  // Test servo (if not stopped)
  if (!servoStopped) {
    int originalPos = servoPosition;
    wheelServo.write(45);
    delay(500);
    wheelServo.write(135);
    delay(500);
    wheelServo.write(originalPos);
  }
  
  bluetooth.println("Test complete");
}

void resetSystem() {
  Serial.println("*** SYSTEM RESET ***");
  
  // Reset all states
  alarmActive = false;
  emergencyMode = false;
  servoStopped = false;
  servoPosition = 90;
  servoDirection = 1;
  
  // Reset hardware
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LASER_PIN, LOW);
  wheelServo.write(90);
  
  Serial.println("Reset complete - Servo wheel moving again");
  bluetooth.println("System reset - Wheel active");
}

void stopAlarmOnly() {
  if (!alarmActive) return;
  
  alarmActive = false;
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LASER_PIN, LOW);
  
  Serial.println("Buzzer and laser stopped");
  bluetooth.println("Alarm stopped");
  
  if (servoStopped) {
    Serial.println("Servo remains stopped (use RESET to restart)");
    bluetooth.println("Servo still stopped");
  }
}
#endif

//==============================================================================
// SERVO CONTROL (UNO)
//==============================================================================
#if USE_SERVO
void updateServoWheel() {
  if (!servoStopped && (millis() - lastServoMove >= SERVO_DELAY)) {
    moveServoWheel();
    lastServoMove = millis();
  }
}

void moveServoWheel() {
  // Continuous back-and-forth movement
  servoPosition += (servoDirection * SERVO_STEP);
  
  // Reverse at limits
  if (servoPosition >= 180) {
    servoPosition = 180;
    servoDirection = -1;
  } else if (servoPosition <= 0) {
    servoPosition = 0;
    servoDirection = 1;
  }
  
  wheelServo.write(servoPosition);
}
#endif

//==============================================================================
// SENSOR MONITORING
//==============================================================================
void monitorSensors() {
  // Check tilt sensor (primary on UNO, backup on NANO)
  if (digitalRead(TILT_SENSOR_PIN) == LOW && !alarmActive) {
    triggerAccident("Tilt sensor activated");
  }
  
  // Check accelerometer every ~2ms (primary on NANO, backup on UNO)
  if (micros() - time1 > 1999) {
    Impact();
    time1 = micros();
  }
}

void Impact() {
  int oldx = xaxis, oldy = yaxis, oldz = zaxis;
  
  // Read current accelerometer values
  xaxis = analogRead(X_PIN);
  yaxis = analogRead(Y_PIN);
  zaxis = analogRead(Z_PIN);
  
  // Anti-vibration filter
  vibration--;
  if (vibration < 0) vibration = 0;
  if (vibration > 0) return;
  
  // Calculate change in acceleration
  deltx = xaxis - oldx;
  delty = yaxis - oldy;
  deltz = zaxis - oldz;
  
  // Calculate magnitude of impact
  magnitude = sqrt(sq(deltx) + sq(delty) + sq(deltz));
  
  // Check for significant impact
  if (magnitude >= sensitivity && !impact_detected && !alarmActive) {
    updateflag = 1;
    vibration = devibrate;
    
#if USE_GPS_GSM
    impact_detected = true;
    impact_time = millis();
#endif
    
    triggerAccident("Accelerometer impact (Magnitude: " + String(magnitude) + ")");
  } else {
    magnitude = 0;
  }
}

//==============================================================================
// ACCIDENT HANDLING
//==============================================================================
void triggerAccident(String cause) {
  alarmActive = true;
  emergencyMode = true;
  alarmStartTime = millis();
  
  Serial.println("*** ACCIDENT DETECTED ***");
  Serial.println("Cause: " + cause);
  
#if USE_SERVO
  // Stop servo wheel immediately
  servoStopped = true;
  Serial.println("Servo wheel STOPPED!");
  bluetooth.println("*** ACCIDENT - SERVO STOPPED! ***");
  bluetooth.println("Cause: " + cause);
#endif

#if USE_GPS_GSM
  // Get GPS location
  getGps();
  
  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CRASH DETECTED!");
  lcd.setCursor(0, 1);
  lcd.print("Mag:" + String(magnitude));
#endif
  
  // Start alarm
  tone(BUZZER_PIN, 1000);
  
#if USE_SERVO
  digitalWrite(LASER_PIN, HIGH);
#endif
  
  Serial.println("Alarm activated - Press button or send 'STOP' to cancel");
  
#if USE_GPS_GSM
  if (gsm_ready) {
    Serial.println("Emergency call will be made in " + String(alert_delay/1000) + " seconds");
  }
#endif
}

void handleEmergency() {
  // Auto-stop buzzer after alarm duration
  if (alarmActive && (millis() - alarmStartTime >= ALARM_DURATION)) {
    Serial.println("Auto-stopping alarm sounds");
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
    
#if USE_SERVO
    digitalWrite(LASER_PIN, LOW);
    bluetooth.println("Alarm auto-stopped");
#endif

#if USE_LCD
    lcd.setCursor(0, 1);
    lcd.print("Alarm stopped");
#endif
  }
  
#if USE_GPS_GSM
  // Handle emergency call and SMS (NANO only)
  if (impact_detected && emergencyMode && 
      (millis() - impact_time >= alert_delay) && gsm_ready) {
    
    Serial.println("=== SENDING EMERGENCY ALERT ===");
    
    // Make emergency call
    makeCall();
    delay(2000);
    
    // Send SMS with location
    sendAlert();
    
    lcd.clear();
    lcd.print("Alert Sent!");
    lcd.setCursor(0, 1);
    lcd.print("Help is coming");
    
    Serial.println("Emergency alert sent successfully");
    
    // Reset emergency flags
    impact_detected = false;
    emergencyMode = false;
  }
#endif
}

void cancelAllAlarms() {
  Serial.println("*** USER CANCELLATION - ALL ALARMS OFF ***");
  
  alarmActive = false;
  emergencyMode = false;
  
#if USE_GPS_GSM
  impact_detected = false;
  impact_time = 0;
#endif
  
  // Stop all alarms
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  
#if USE_SERVO
  digitalWrite(LASER_PIN, LOW);
  bluetooth.println("All alarms cancelled by user");
#endif

#if USE_LCD
  lcd.clear();
  lcd.print("Alert Cancelled");
  lcd.setCursor(0, 1);
  lcd.print("by User");
  delay(2000);
  lcd.clear();
  lcd.print("System Ready");
#endif
  
  Serial.println("All emergency procedures cancelled");
}

//==============================================================================
// GPS AND GSM FUNCTIONS (NANO)
//==============================================================================
#if USE_GPS_GSM
void getGps() {
  Serial.println("Reading GPS coordinates...");
  boolean newData = false;
  
  for (unsigned long start = millis(); millis() - start < 2000;) {
    while (neogps.available()) {
      if (gps.encode(neogps.read())) {
        newData = true;
        break;
      }
    }
  }
  
  if (newData && gps.location.isValid()) {
    latitude = String(gps.location.lat(), 6);
    longitude = String(gps.location.lng(), 6);
    Serial.println("GPS: " + latitude + ", " + longitude);
  } else {
    latitude = "0.000000";
    longitude = "0.000000";
    Serial.println("GPS: No fix available");
  }
}

void sendAlert() {
  String sms_data = "EMERGENCY - Accident detected!\r\n";
  sms_data += "Location: http://maps.google.com/maps?q=loc:";
  sms_data += latitude + "," + longitude + "\r\n";
  sms_data += "Impact magnitude: " + String(magnitude) + "\r\n";
  sms_data += "Please send help immediately!";
  
  sendSms(sms_data);
}

void makeCall() {
  Serial.println("Making emergency call to: " + EMERGENCY_PHONE);
  sim800.println("ATD" + EMERGENCY_PHONE + ";");
  delay(20000); // Ring for 20 seconds
  sim800.println("ATH"); // Hang up
  delay(1000);
  Serial.println("Emergency call completed");
}

void sendSms(String text) {
  Serial.println("Sending emergency SMS...");
  
  sim800.print("AT+CMGF=1\r");
  delay(500);
  sim800.print("AT+CMGS=\"" + EMERGENCY_PHONE + "\"\r");
  delay(500);
  sim800.print(text);
  delay(200);
  sim800.write(0x1A); // Ctrl+Z to send
  delay(5000);
  
  Serial.println("Emergency SMS sent");
}

boolean SendAT(String at_command, String expected_answer, unsigned int timeout) {
  boolean answer = false;
  String response = "";
  unsigned long previous = millis();
  
  // Clear buffer
  while (sim800.available() > 0) sim800.read();
  
  // Send command
  sim800.println(at_command);
  
  // Wait for response
  do {
    if (sim800.available() != 0) {
      char c = sim800.read();
      response += c;
      if (response.indexOf(expected_answer) > 0) {
        answer = true;
      }
    }
  } while ((answer == false) && ((millis() - previous) < timeout));
  
  return answer;
}
#endif
