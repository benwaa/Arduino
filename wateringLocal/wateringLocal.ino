#include <stdarg.h>

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
#define BASIL_1_INPUT_PIN 8
#define BASIL_1_SENSOR_PIN A1
#define BASIL_1_THRESHOLD 45

#define BASIL_2_INPUT_PIN 7
#define BASIL_2_SENSOR_PIN A2
#define BASIL_2_THRESHOLD 40

const unsigned long kSECOND = 1000;
const unsigned long kMIN = 60 * kSECOND;
const unsigned long kHOUR = 60 * kMIN;
const unsigned long kProbeInterval = 8 * kMIN; // stream intervals in milliseconds
const unsigned int nbRunningAvg = 5; // number of point to compute the avg over
const int motorPin = A0;

#define DEBUG 1
#if DEBUG
#define INTERVAL_TIME 5*kSECOND
#define debug(format, ...) \
  debug_print(format, ## __VA_ARGS__)
#else
#define INTERVAL_TIME 2*kMIN
#define debug(format, ...) do {} while (0)
#endif


void setup() {
  Serial.begin(9600);
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
  stopWatering();
}

void basil1() {
  int avgMoistValue = RecomputeAverageMoisture(BASIL_1_INPUT_PIN, BASIL_1_SENSOR_PIN);
  debug("basil1 avg moisture:%i", avgMoistValue);
  if (shouldWater(1, avgMoistValue)) {
    startWatering(1);
  }
}

void basil2() {
  int avgMoistValue = RecomputeAverageMoisture(BASIL_1_INPUT_PIN, BASIL_1_SENSOR_PIN);
  debug("basil1 avg moisture:%i", avgMoistValue);
  if (shouldWater(1, avgMoistValue)) {
    startWatering(1);
  }
}

bool shouldWater(int plant, int moisture) {
  return false;
}

void startWatering(int plant) {
  digitalWrite(motorPin, HIGH);
  debug("start watering");
}
void stopWatering() {
  debug("stop watering");
  digitalWrite(motorPin, LOW);
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
