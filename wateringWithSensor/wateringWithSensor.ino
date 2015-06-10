int motorPin = A0;
int moistureSensorPin = A1;

const unsigned long kSECOND = 1000;
const unsigned long kMIN = 60 * kSECOND;
const unsigned long kHOUR = 60 * kMIN;
const unsigned long watertime = 5 * kSECOND; // how long to water in miliseconds
const unsigned long waittime =  2 * kSECOND; // how long to wait between watering
const unsigned int BasilThreshold = 500;

// State Machine
unsigned long long lastStart = 0;
int watering = 0;

char buf[64];

#define DEBUG 1
#ifdef DEBUG
#define debug(format, ...) \
  sprintf(buf, format, ## __VA_ARGS__); \
  Serial.println(buf);
#else
#  define debug(format, ...) do {} while (0)
#endif

void setup() {
  // put your main code here, to run repeatedly:
  Serial.begin(57600);
  pinMode(motorPin, OUTPUT);
  debug("Start watertime %d\n", watertime);
}

void loop() {
  unsigned long currentMillis = millis();
  int moistValue = analogRead(moistureSensorPin);

  if(moistValue < BasilThreshold) {
    debug("(%lu) Watering for %lu moist %i\n", currentMillis, watertime, moistValue);
    lastStart = currentMillis;
    startWater();
  }

  if (currentMillis > lastStart + watertime && watering) {
    debug("(%lu) Stop watering moist %i\n", currentMillis, moistValue);
    stopWater();
  }
  delay(1000);        // delay in between reads for stability
}

void startWater() {
  watering = 1;
  digitalWrite(motorPin, HIGH);
}

void stopWater() {
  watering = 0;
  digitalWrite(motorPin, LOW);
}
