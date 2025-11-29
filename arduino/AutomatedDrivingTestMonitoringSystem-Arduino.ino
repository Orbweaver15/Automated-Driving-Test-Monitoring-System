#include <Wire.h>
#include <AS5600.h>

// Pins and sensor setup
const int RPin = 8;
const int LPin = 9;
const int Vcc = 10;
AS5600 as5600;

// State variables
bool menuShown = false;
bool gameMode = false;
float centerAngle = 0.0;
unsigned long lastSend = 0;

// Test structures
struct TestResult {
  const char* name;
  bool passed;
  float achieved;
  float expectedMin;
  float expectedMax;
};

struct Scenario {
  const char* name;
  bool clockwise;
  float minRotation;
  float maxRotation;
  unsigned long duration;
};

const Scenario scenarios[4] PROGMEM = {
  {"Scenario 1: LANE CHANGE to the LEFT ", false, 20, 60, 5000},
  {"Scenario 2: LANE CHANGE to the RIGHT", true, 20, 60, 5000},
  {"Scenario 3: 90° LEFT TURN           ", false, 270, 400, 10000},
  {"Scenario 4: 90° RIGHT TURN          ", true, 270, 400, 10000}
};

TestResult testResults[5];
int testCount = 0;

void setup() {
  Serial.begin(9600);
  pinMode(LPin, INPUT);
  pinMode(RPin, INPUT);
  pinMode(Vcc, OUTPUT);
  digitalWrite(Vcc, HIGH);
  Wire.begin();
  initializeSensors();
}

void loop() {
  if (gameMode) {
    gameLoop();
    return;
  }

  if (!menuShown) {
    showMenu();
    menuShown = true;
  }

  if (Serial.available()) {
    handleSerialCommand();
  }
}


// Helper functions
void gameLoop() {
  if (millis() - lastSend > 50) {
    lastSend = millis();
    bool leftDetected = digitalRead(LPin) == LOW;
    bool rightDetected = digitalRead(RPin) == LOW;
    float currentAngle = as5600.readAngle() * 0.0879;
    float steering = normalizeAngle(currentAngle - centerAngle);
    
    Serial.print(leftDetected ? "1" : "0");
    Serial.print(rightDetected ? "1" : "0");
    Serial.print(",");
    if(steering >= 0) Serial.print(" ");
    Serial.println(steering, 2);
  }
}

float normalizeAngle(float angle) {
  if (angle > 180) angle -= 360;
  else if (angle < -180) angle += 360;
  return constrain(angle, -90, 90) / 90.0;
}

void handleSerialCommand() {
  char command = Serial.read();
  if (command == 'G') {
    gameMode = true;
    centerAngle = as5600.readAngle() * 0.0879;
    Serial.println("GAME_START");
  } 
  else if (command == 'X') {
    gameMode = false;
    Serial.println("GAME_STOP");
    processCommand(command);
  }
  else {
    processCommand(command);
  }
}

void initializeSensors() {
  bool as5600Issue = true;
  bool irIssue = true;
  
  for (int i = 0; i < 5; i++) {  // Check sensors 5 times
    if (as5600Issue && as5600.begin()) {
      as5600Issue = false;
    }
    if (irIssue && (digitalRead(LPin) == HIGH && digitalRead(RPin) == HIGH)) {
      irIssue = false;
    }
    delay(500);
  }

  if (as5600Issue) {
    Serial.println("\n\nAS5600 not found");
  } else {
    as5600.setDirection(AS5600_CLOCK_WISE);
  }

  if (irIssue) {
    Serial.println("\n\nIR sensors issue");
  }

  if (!as5600Issue && !irIssue) {
    Serial.println("\n\nSensors OK");
  }
}



void showMenu() {
  Serial.println(F("\nAll Systems Fully Functional and Online, please use buttons to navigate the test"));
  Serial.println();  // Extra newline for spacing
}

void processCommand(char command) {
  menuShown = false;
  
  // Clear any leftover serial data
  while(Serial.available() > 0) Serial.read();
  
  switch(command) {
    case '1':
      Serial.println(F("\nStarting full test..."));
      FullTest();
      break;
    case '2':
      testIR();
      break;
    case '3':
      testAS();
      break;
    case '4':
      break;
    default:
      Serial.println(F("\nInvalid command"));
  }
  // Add small delay after command processing
  delay(100);
}


void FullTest() {
  Serial.println(F("\nSTARTING FULL TEST"));
  testCount = 0; // Reset test counter
  
  // Add hand placement test to results
  testResults[testCount].name = "HAND PLACEMENT                      ";
  testResults[testCount].passed = handPlacementTest();
  testResults[testCount].achieved = 0;
  testResults[testCount].expectedMin = 0;
  testResults[testCount].expectedMax = 0;
  testCount++;

  // Run all scenarios
  for (int i = 0; i < 4; i++) {
    runScenario(i);
  }

  printTestSummary();
  Serial.println(F("\nTEST COMPLETE - Returning to menu"));
  delay(5000);
}

bool handPlacementTest() {
  Serial.println(F("\nHAND PLACEMENT TEST"));
  Serial.println(F("Place hands on wheel"));
  delay(6000);

  bool leftDetected = digitalRead(LPin) == LOW;
  bool rightDetected = digitalRead(RPin) == LOW;

  Serial.print(F("Left: "));
  Serial.println(leftDetected ? "DETECTED" : "MISSING");
  Serial.print(F("Right: "));
  Serial.println(rightDetected ? "DETECTED" : "MISSING");

  bool passed = (leftDetected && rightDetected);
  Serial.println(passed ? "PASSED" : "FAILED");
  
  return passed;
}

void runScenario(int index) {
  // Read from PROGMEM
  Scenario s;
  memcpy_P(&s, &scenarios[index], sizeof(Scenario));
  
  Serial.print(F("\n"));
  Serial.println(s.name);

  float startAngle = as5600.readAngle() * 0.0879;
  delay(3000);
  
  Serial.println(F("Start moving now!"));

  float maxRotation = 0.0;
  float totalRotation = 0.0;
  float lastAngle = startAngle;
  unsigned long startTime = millis();

  while (millis() - startTime < s.duration) {
    float currentAngle = as5600.readAngle() * 0.0879;
    float change = currentAngle - lastAngle;

    // Handle wrap-around
    if (change > 180) change -= 360;
    else if (change < -180) change += 360;

    if (abs(change) > 0.5) {
      totalRotation += change;

      if (s.clockwise) {
        if (totalRotation < maxRotation) {
          maxRotation = totalRotation;
        }
      } else {
        if (totalRotation > maxRotation) {
          maxRotation = totalRotation;
        }
      }
      lastAngle = currentAngle;
    }
    delay(50);
  }

  float rotationAchieved = s.clockwise ? -maxRotation : maxRotation;
  bool passed = rotationAchieved >= s.minRotation && rotationAchieved <= s.maxRotation;

  Serial.print(F("Rotation: "));
  Serial.print(rotationAchieved, 0);
  Serial.println(F("°"));
  Serial.print(F("Result: "));
  Serial.println(passed ? "PASSED" : "FAILED");
  
  // Store results
  testResults[testCount].name = s.name;
  testResults[testCount].passed = passed;
  testResults[testCount].achieved = rotationAchieved;
  testResults[testCount].expectedMin = s.minRotation;
  testResults[testCount].expectedMax = s.maxRotation;
  testCount++;
}

void printTestSummary() {
  Serial.println(F("\n==== TEST SUMMARY ===="));
  Serial.println(F("               Test Name             | Status   | Details"));
  Serial.println(F("---------------------------------------"));
  
  bool allPassed = true;
  
  for (int i = 0; i < testCount; i++) {
    // Format test name with padding for alignment
    String nameStr = String(testResults[i].name);
    while (nameStr.length() < 18) nameStr += " ";
    
    Serial.print(nameStr);
    Serial.print(F(" | "));
    Serial.print(testResults[i].passed ? F("PASSED") : F("FAILED"));
    
    // Add spacing for alignment
    if (testResults[i].passed) {
      Serial.print(F("   | "));
    } else {
      Serial.print(F("   | "));
    }
    
    if (testResults[i].passed) {
      Serial.println(F("OK"));
    } else {
      allPassed = false;
      if (i == 0) { // Hand test
        Serial.println(F("Hand(s) not detected"));
      } else {
        Serial.print(F("Achieved: "));
        Serial.print(testResults[i].achieved, 0);
        Serial.print(F("° (Expected: "));
        Serial.print(testResults[i].expectedMin, 0);
        Serial.print(F("°-"));
        Serial.print(testResults[i].expectedMax, 0);
        Serial.println(F("°)"));
      }
    }
  }
  
  Serial.println(F("\nFINAL VERDICT:"));
  Serial.println(allPassed ? F("*** ALL TESTS PASSED ***") : F("!!! SOME TESTS FAILED !!!"));
  Serial.println(F("========================"));
}

void testIR() {
  Serial.println(F("\nTESTING STEERING SENSORS"));
  Serial.println(F("Left | Right"));
  Serial.println(F("-----------"));

  unsigned long lastPrint = millis();
  unsigned long lastSerialCheck = millis();
  
  while (true) {
    // Check for exit command every 100ms
    if (millis() - lastSerialCheck > 100) {
      lastSerialCheck = millis();
      
      if (Serial.available()) {
        char c = Serial.peek(); // Don't remove from buffer yet
        if (c == '1' || c == '2' || c == '3' || c == 'G' || c == 'X') {
          Serial.println(F("\n"));
          while(Serial.available()) Serial.read(); // Clear buffer
          return;
        }
        else {
          // Discard any unexpected characters
          Serial.read();
        }
      }
    }

    // Sensor reading and printing logic
    if (millis() - lastPrint > 500) {
      lastPrint = millis();
      int left = digitalRead(LPin);
      int right = digitalRead(RPin);
      
      Serial.print(left == LOW ? "DETECTED " : "MISSING  ");
      Serial.print("| ");
      Serial.println(right == LOW ? "DETECTED" : "MISSING");
    }
    
    delay(50);
  }
}

void testAS() {
  Serial.println(F("\nTESTING ROTATION SENSOR"));
  Serial.println(F("Angle | Revolutions"));

  float startAngle = as5600.readAngle() * 0.0879;
  float lastAngle = startAngle;
  float rotationSinceRev = 0.0;
  int revolutions = 0;
  unsigned long lastPrint = millis();
  unsigned long lastSerialCheck = millis();

  while (true) {
    // Check for exit command every 100ms
    if (millis() - lastSerialCheck > 100) {
      lastSerialCheck = millis();
      
      if (Serial.available()) {
        char c = Serial.peek();
        if (c == '1' || c == '2' || c == '3' || c == 'G' || c == 'X') {
          Serial.println(F("\n"));
          while(Serial.available()) Serial.read();
          return;
        }
        else {
          Serial.read();
        }
      }
    }

    // Rotation sensor logic
    float currentAngle = as5600.readAngle() * 0.0879;
    float change = currentAngle - lastAngle;

    if (change > 180) change -= 360;
    else if (change < -180) change += 360;

    if (abs(change) > 0.5 || (millis() - lastPrint > 500)) {
      rotationSinceRev += change;
      
      if (abs(rotationSinceRev) >= 360) {
        revolutions += (rotationSinceRev > 0) ? 1 : -1;
        rotationSinceRev = 0;
      }

      if (millis() - lastPrint > 500) {
        lastPrint = millis();
        Serial.print(currentAngle, 0);
        Serial.print(F("° | "));
        Serial.println(revolutions);
      }
      
      lastAngle = currentAngle;
    }
    
    delay(50);
  }
}

