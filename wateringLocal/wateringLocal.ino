#include <stdarg.h>
#include <Servo.h>

void debug_print(char const*fmt, ... ) {
  char buf[128]; // resulting string limited to 128 chars
  sprintf(buf, "(%06lu)", millis());
  va_list args;
  va_start (args, fmt);
  vsnprintf(buf + 8, 120, fmt, args);
  va_end (args);
  Serial.println(buf);
}

#define kMAXHUMIDITY 1023.0

#define SERVO_PIN 2

#define BASIL_1_INPUT_PIN 8
#define BASIL_1_SENSOR_PIN A1
#define BASIL_1_WATER_ANGLE 0
#define BASIL_1_THRESHOLD 45

#define BASIL_2_INPUT_PIN 8 //7
#define BASIL_2_SENSOR_PIN A1 //A2
#define BASIL_2_WATER_ANGLE 90
#define BASIL_2_THRESHOLD 40

#define kSECOND 1000
#define kMIN 60 * kSECOND
#define kHOUR 60 * kMIN
#define kWaterTime 30 * kSECOND
const unsigned int nbRunningAvg = 5; // number of point to compute the avg over
const int motorPin = A0;

#define DEBUG 0
#if DEBUG
#define INTERVAL_TIME 5*kSECOND
#define debug(format, ...) \
  debug_print(format, ## __VA_ARGS__)
#else
#define INTERVAL_TIME 2*kMIN
#define debug(format, ...) do {} while (0)
#endif

Servo myservo;
void setup() {
  Serial.begin(9600);
  myservo.attach(SERVO_PIN);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);
  pinMode(BASIL_1_INPUT_PIN, OUTPUT);
  digitalWrite(BASIL_1_INPUT_PIN, LOW);
  debug("Start");
}

void loop() {
  basil1();
  basil2();
  delay(INTERVAL_TIME);
}

void basil1() {
  int avgMoistValue = RecomputeAverageMoisture(BASIL_1_INPUT_PIN, BASIL_1_SENSOR_PIN);
  debug("basil1 avg moisture:%i", avgMoistValue);
  if (shouldWater(1, avgMoistValue)) {
    watering(1);
  }
}

void basil2() {
  int avgMoistValue = RecomputeAverageMoisture(BASIL_2_INPUT_PIN, BASIL_2_SENSOR_PIN);
  debug("basil2 avg moisture:%i", avgMoistValue);
  if (shouldWater(2, avgMoistValue)) {
    watering(2);
  }
}

void driveWater(int plant) {
  switch (plant) {
  case 1:
    myservo.write(BASIL_1_WATER_ANGLE);
    break;
  case 2:
    myservo.write(BASIL_2_WATER_ANGLE);
    break;
  }
}

bool shouldWater(int plant, int moisture) {
  switch (plant) {
  case 1:
    if (moisture <= BASIL_1_THRESHOLD) return true;
    break;
  case 2:
    if (moisture <= BASIL_2_THRESHOLD) return true;
    break;
  }
  return false;
}

void watering(int plant) {
  driveWater(plant);
  delay(kSECOND);
  debug("start watering %d", plant);
  digitalWrite(motorPin, HIGH);
  delay(kWaterTime);
  digitalWrite(motorPin, LOW);
  debug("stop watering");
}

int RecomputeAverageMoisture(int inputPIN, int sensorPIN) {
  int avgMoistValue = 0;
  int readings = 0;
  for (int i = 0; i < nbRunningAvg; ++i) {
    // forward
    digitalWrite(inputPIN, HIGH);
    delay(50);
    int moistValue = analogRead(sensorPIN) * (100.0 / kMAXHUMIDITY);
    digitalWrite(inputPIN, LOW);
    avgMoistValue += moistValue;
    readings++;
    delay(50);
  }
  avgMoistValue /= readings;
  return avgMoistValue;
}
