#include <stdarg.h>

void debug_print(char const* fmt, ...) {
  char buf[128];  // resulting string limited to 128 chars
  sprintf(buf, "(%06lu)", millis());
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf + 8, 120, fmt, args);
  va_end(args);
  Serial.println(buf);
}
#define DEBUG 1
#if DEBUG
#define INTERVAL_TIME 10000
#define debug(format, ...) debug_print(format, ##__VA_ARGS__)
#else
#define INTERVAL_TIME 10 * 1000
#define debug(format, ...) \
  do {                     \
  } while (0)
#endif

#include <RotaryEncoder.h>

#define PIN_A 3
#define PIN_B 4
RotaryEncoder knob(PIN_A, PIN_B);
int lastPosition, position;

volatile unsigned int encoder0Pos = 0;

void setup() {
  lastPosition = knob.getPosition();
  Serial.begin (9600);
}

void loop() {
  knob.tick();
  position = knob.getPosition();
  if (lastPosition != position) {
    debug("Old pos:%i New pos:%i", lastPosition, position);
    lastPosition = position;
  }
}
