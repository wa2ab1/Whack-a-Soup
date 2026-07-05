#include <TM1637Display.h>

#define CLK_PIN  11  // Shared Clock pin
#define DIO_PIN1 12 // Data pin for Display 1
#define DIO_PIN2 13  // Data pin for Display 2

const int NUM_MOTORS = 5;
const int PIN_UP[NUM_MOTORS]   = {1, 3, 5, 7, 9}; // Motor 1 (index 0), Motor 2 (index 1)...
const int PIN_DOWN[NUM_MOTORS] = {2, 4, 6, 8, 10};

// ── Shock Sensor Pins (assign to your available pins) ──
// If replacing the whack buttons with shock sensors,
// use the same pins as PIN_BUTTON:
const int PIN_SHOCK[NUM_MOTORS] = {2, 15, 16, 17, 18};

// HW-483 outputs LOW when vibrated (matches your original FALLING interrupt)
#define SHOCK_TRIGGER_STATE LOW

// ── Per-sensor state ──
byte          sensorPulses[NUM_MOTORS]    = {0, 0, 0, 0, 0};
bool          sensorPrevState[NUM_MOTORS]  = {false, false, false, false, false};
unsigned long sensorLastHit[NUM_MOTORS]    = {0, 0, 0, 0, 0};

// Shared window (same as your original 30ms)
unsigned long windowStart = 0;
const unsigned long SHOCK_WINDOW_MS   = 30;
const unsigned long SHOCK_LOCKOUT_MS   = 200;
const byte          SHOCK_MIN_PULSES  = 2;

// 1. FIXED: Added COMPLETED to the enum list
enum MotorState { IDLE, MOVING_UP, AT_TOP, MOVING_DOWN, COMPLETED };
MotorState motorStates[NUM_MOTORS] = {IDLE, IDLE, IDLE, IDLE, IDLE}; 
unsigned long motorTimers[NUM_MOTORS] = {0, 0, 0, 0, 0};

//------------------Start Button Logic
bool startGame = true;
const int STARTBTN = 14;
bool lastButtonState = HIGH;

//-------------------Score Logic
int playerScore = 0; // FIXED: Declared missing global variable

//-------------------Timer
long startTime = 0;
const long timerDuration = 30000; // 30 seconds
int lastDisplayedTime = -1; 
int lastDisplayedScore = 0; 

//-------------------Mole Logic
long randPopUpInterval = 1500; 
long moleDelay = 1000; 
long elapsedMoleInterval = 0;

unsigned long lastSpawnTime = 0;
unsigned long nextSpawnDelay = 1000;   
const unsigned long baseDelay = 800;   
const unsigned long randomMax = 1500;  
const unsigned long upTravelTime = 500;  
const unsigned long downTravelTime = 600;  
const unsigned long holdTime = 1000;   

// Create separate instances for each display
TM1637Display timerDisplay(CLK_PIN, DIO_PIN1);
TM1637Display scoreDisplay(CLK_PIN, DIO_PIN2);
\
void setup()   
{
  Serial.begin(9600);

    for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(PIN_SHOCK[i], INPUT_PULLUP);
    sensorPrevState[i] = (digitalRead(PIN_SHOCK[i]) == SHOCK_TRIGGER_STATE);
  }

  // motors & buttons
  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(PIN_UP[i], OUTPUT);
    pinMode(PIN_DOWN[i], OUTPUT);
    pinMode(PIN_BUTTON[i], INPUT_PULLUP); // FIXED: Added missing pinMode initialization for whack buttons
  }

  // start button
  pinMode(STARTBTN, INPUT_PULLUP);

  // segmented displays
  timerDisplay.setBrightness(0x0f); // Max brightness
  scoreDisplay.setBrightness(0x0f);
  
  timerDisplay.showNumberDec(0); 
  timerDisplay.showNumberDecEx(3000, 0x40, true); 
  scoreDisplay.showNumberDec(0); 
}

void loop() {
  bool currentButtonState = digitalRead(STARTBTN);

  // FIXED: Added state tracking logic so you can restart a new game after one ends
  if (!startGame && currentButtonState == LOW && lastButtonState == HIGH)
  {
        startGame = true;
        playerScore = 0; // Reset score for a new game
        startTime = millis();
  }
  lastButtonState = currentButtonState;

  if (startGame)
  {
    handleTimer();
    motorRandom();
    //checkHitButtons();
    checkConsoleInput();
    updateScoreDisplay();
    checkHit();
  }
}

void handleTimer() {
  unsigned long currentMillis = millis();
  unsigned long elapsedTime = currentMillis - startTime;

  if (elapsedTime <= timerDuration) {
    unsigned long remainingMillis = timerDuration - elapsedTime;

    int seconds = remainingMillis / 1000;
    int hundredths = (remainingMillis % 1000) / 10;

    int timeToDisplay = (seconds * 100) + hundredths;

    if (timeToDisplay != lastDisplayedTime) {
      timerDisplay.showNumberDecEx(timeToDisplay, 0x40, true);
      lastDisplayedTime = timeToDisplay;
    }
  } 
  else {
    // FIXED: Stop the game loop and clean up motor states when time runs out
    startGame = false;
    timerDisplay.showNumberDec(0, true); // Force display to show 0000
    for (int i = 0; i < NUM_MOTORS; i++) {
      motorStates[i] = IDLE;
    }
  }
}

void motorRandom() {
  for (int m = 0; m < NUM_MOTORS; m++) {
    motorUpdate(m); 
  }

  unsigned long currentMillis = millis();
  
  if (currentMillis - lastSpawnTime >= nextSpawnDelay) {
    lastSpawnTime = currentMillis;
    
    int spawnCount = random(1, 4);
    nextSpawnDelay = baseDelay + random(randomMax);

    for (int i = 0; i < spawnCount; i++) {
      int randomTarget = random(NUM_MOTORS);
      
      if (motorStates[randomTarget] == IDLE || motorStates[randomTarget] == COMPLETED) {
            motorStates[randomTarget] = MOVING_UP;
            motorTimers[randomTarget] = millis();
      }
    }
  }
} // FIXED: Removed stray character 'a' that was breaking compilation here

void motorUpdate(int motor)
{   
    int pinUp = PIN_UP[motor];
    int pinDown = PIN_DOWN[motor];

    unsigned long currentMillis = millis();
    unsigned long timeInState = currentMillis - motorTimers[motor];

  switch (motorStates[motor]) {
    case MOVING_UP:
      digitalWrite(pinUp, HIGH);
      digitalWrite(pinDown, LOW);
      if (timeInState >= upTravelTime) {
        motorStates[motor] = AT_TOP;
        motorTimers[motor] = currentMillis; 
        Serial.print(motor + " at top");
      }
      break;

    case AT_TOP:
      digitalWrite(pinUp, LOW);
      digitalWrite(pinDown, LOW);
      if (timeInState >= holdTime) {
        motorStates[motor] = MOVING_DOWN;
        motorTimers[motor] = currentMillis;
        Serial.print(motor + " moving down");
      }
      break;

    case MOVING_DOWN:
      digitalWrite(pinUp, LOW);
      digitalWrite(pinDown, HIGH);
      if (timeInState >= downTravelTime) {
        motorStates[motor] = IDLE;
        motorTimers[motor] = currentMillis;
        Serial.print(motor + " finished");
      }
      break;

    case IDLE:
    default:
      digitalWrite(pinUp, LOW);
      digitalWrite(pinDown, LOW);
      break;
  }
}

void isr() {
  pulses++;
}

void checkHitButtons() {
  for (int i = 0; i < NUM_MOTORS; i++) {
    bool currentBtnState = digitalRead(PIN_BUTTON[i]);

    if (currentBtnState == LOW) {
      if (startGame && (motorStates[i] == MOVING_UP || motorStates[i] == AT_TOP)) {
        playerScore++; 
        motorStates[i] = MOVING_DOWN;
        motorTimers[i] = millis(); 
      }
    }
    lastHitButtonStates[i] = currentBtnState; 
  }
}

void checkHit()
{
 unsigned long now = millis();

  // ── Phase 1: Read all sensors & count edge transitions ──
  // Runs every loop iteration (fast — just digitalRead)
  for (int i = 0; i < NUM_MOTORS; i++) {
    bool triggered = (digitalRead(PIN_SHOCK[i]) == SHOCK_TRIGGER_STATE);

    // Count a pulse only on transition: not-triggered → triggered
    // This mimics the original FALLING interrupt behavior
    if (triggered && !sensorPrevState[i]) {
      sensorPulses[i]++;
    }
    sensorPrevState[i] = triggered;
  }

  // ── Phase 2: Process window every 30ms ──
  if (now - windowStart >= SHOCK_WINDOW_MS) {
    windowStart = now;

    for (int i = 0; i < NUM_MOTORS; i++) {
      // Confirm hit: enough pulses AND lockout expired
      if (sensorPulses[i] >= SHOCK_MIN_PULSES &&
          (now - sensorLastHit[i] > SHOCK_LOCKOUT_MS)) {

        sensorLastHit[i] = now;

        // ── Game logic (same as checkHitButtons) ──
        if (startGame &&
            (motorStates[i] == MOVING_UP || motorStates[i] == AT_TOP)) {
          playerScore++;
          motorStates[i] = MOVING_DOWN;
          motorTimers[i] = millis();
          Serial.print("HIT on motor ");
          Serial.println(i);
        }
      }
      // Reset pulse count at end of each window
      sensorPulses[i] = 0;
    }
  }
}
void updateScoreDisplay() {
  if (playerScore != lastDisplayedScore) {
    scoreDisplay.showNumberDec(playerScore); 
    lastDisplayedScore = playerScore;
  }
}

void checkConsoleInput() {
  if (Serial.available() > 0) {
    char inputChar = Serial.read();
    
    if (inputChar >= '0' && inputChar <= '4') {
      int motorIndex = inputChar - '0';
      motorStates[motorIndex] = MOVING_DOWN;
      motorTimers[motorIndex] = millis(); 
      Serial.print("Console forced spawn on Motor: ");
      Serial.println(motorIndex);
    }
    if (inputChar == 'h')
    {
      Serial.println("h");
      playerScore++;
      updateScoreDisplay();
    }
  }
}