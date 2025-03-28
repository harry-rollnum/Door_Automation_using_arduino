#define ENCODER_A 3
#define ENCODER_B 4
#define PWM_PIN 9
#define DIR_PIN 8
#define OPEN_BUTTON 5
#define CLOSE_BUTTON 6
#define HOME 11

volatile int encoderCount = 0;
const int maxPosition = 1000;
const int baseCloseDelay = 5000;

bool reached = false;
bool isOpening = false;
bool isClosing = false;
unsigned long closeStartTime = 0;
unsigned long closeDelay = baseCloseDelay;

int currentSpeed = 0;

// Debug state tracking to prevent repeated Serial messages
bool prevOpening = false;
bool prevClosing = false;
bool prevHomed = false;
bool prevStopped = true;

void encoderISR() {
  if (digitalRead(ENCODER_A) > digitalRead(ENCODER_B)) {
    encoderCount++;
  } else {
    encoderCount--;
  }
}

int calculateSpeed(int position) {
  int minSpeed = 80;
  int maxSpeed = 180;
  int slowZone = maxPosition * 0.2;
  int mediumZoneEnd = maxPosition * 0.8;
  
  int targetSpeed = (position < slowZone || position > mediumZoneEnd) ? minSpeed : maxSpeed;

  int speedStep = 5;
  if (currentSpeed < targetSpeed) {
    currentSpeed = min(currentSpeed + speedStep, targetSpeed);
  } else if (currentSpeed > targetSpeed) {
    currentSpeed = max(currentSpeed - speedStep, targetSpeed);
  }
  
  return currentSpeed;
}

void moveMotor(int direction) {
  digitalWrite(DIR_PIN, direction);
  analogWrite(PWM_PIN, currentSpeed);
}

void stopMotor() {
  analogWrite(PWM_PIN, 0);
  currentSpeed = 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(ENCODER_A, INPUT);
  pinMode(ENCODER_B, INPUT);
  pinMode(PWM_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(HOME, INPUT_PULLUP);
  pinMode(OPEN_BUTTON, INPUT_PULLUP);
  pinMode(CLOSE_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, RISING);
  delay(5000);
}

void loop() {
  if (!reached) {
    if (digitalRead(HOME) == HIGH) {
      digitalWrite(DIR_PIN, HIGH);
      analogWrite(PWM_PIN, 80);
      if (!prevHomed) {
        Serial.println("🔄 Homing: Moving to Opened Position...");
        prevHomed = true;
      }
    } else {
      stopMotor();
      reached = true;
      encoderCount = maxPosition;
      Serial.println("✅ Homing Complete! Door is Fully Opened.");
      closeStartTime = millis();
    }
  }

  if (digitalRead(OPEN_BUTTON) == LOW) {
    if (isClosing) {
      stopMotor();
      isClosing = false;
      isOpening = false;
      Serial.println("🛑 Emergency Stop Activated!");
    } else {
      if (!isOpening) {
        isOpening = true;
        Serial.println("🔼 Opening Door...");
      } else {
        closeDelay += 5000;
        Serial.print("⏳ Auto-Close Delay Extended: ");
        Serial.print(closeDelay / 1000);
        Serial.println(" sec.");
      }
    }
  }

  if (digitalRead(CLOSE_BUTTON) == LOW) {
    isClosing = true;
    Serial.println("🔒 Manual Close Triggered.");
  }

  if (isOpening) {
    if (!prevOpening) {
      Serial.println("[STATUS] Door Opening...");
      prevOpening = true;
      prevClosing = false;
      prevStopped = false;
    }
    int speed = calculateSpeed(abs(encoderCount));
    moveMotor(HIGH);
    if (abs(encoderCount) >= maxPosition) {
      stopMotor();
      isOpening = false;
      closeStartTime = millis();
      Serial.println("✅ Door Fully Opened. Auto-close Timer Started.");
    }
  }

  if (!isOpening && !isClosing && (millis() - closeStartTime > closeDelay)) {
    isClosing = true;
    Serial.println("⏳ Auto-Closing Initiated...");
  }

  if (isClosing) {
    if (!prevClosing) {
      Serial.println("[STATUS] Door Closing...");
      prevClosing = true;
      prevOpening = false;
      prevStopped = false;
    }
    int speed = calculateSpeed(abs(encoderCount));
    moveMotor(LOW);
    if (encoderCount <= 0) {
      stopMotor();
      isClosing = false;
      closeDelay = baseCloseDelay;
      Serial.println("🔒 Door Fully Closed. System Ready.");
    }
  }
  
  if (!isOpening && !isClosing && !prevStopped) {
    Serial.println("[IDLE] Waiting for User Input...");
    prevStopped = true;
    prevOpening = false;
    prevClosing = false;
  }
}
