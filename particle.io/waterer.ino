#define SOIL_PIN A0
#define POWER_PIN D0
#define FLOW_PIN D1

// Min * Second * Millisec: every 15 min
#define kINTERVAL 30 * 60 * 1000.0
#define kMAXHUMIDITY 4095.0

// sets time since last publish.
unsigned int last_publish = 0;
// count the edges of the flow sensor
unsigned int nb_tops_fan = 0;

// Amount of volume to dispatch per dropping.
float water_volume = 0.1;

#define PUMP_PIN D7
void setup() {
  Particle.function("DropWater", DropWater);
  Particle.function("WaterOnFor", WaterOnFor);
  pinMode(POWER_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT);
  digitalWrite(POWER_PIN, LOW);
  Particle.publish("moisture", String(RecomputeAverageMoisture()), PRIVATE);
}

void loop() {
  unsigned long now = millis();
  if ((now - last_publish) < kINTERVAL) {
    // sleep 1 sec
    delay(1000);
    return;
  }

  // make sure the  moisture webhook is setup properly.
  Particle.publish("moisture", String(RecomputeAverageMoisture()), PRIVATE);

  // resets last_publish variable to now (resets timer)
  last_publish = now;
}

#define kNB_RUN_AVG 5

int RecomputeAverageMoisture() {
  int avg_moisture = 0;
  int readings = 0;
  digitalWrite(POWER_PIN, HIGH);
  delay(250);
  for (int i = 0; i < kNB_RUN_AVG; ++i) {
    // forward
    delay(50);
    int moistValue = ((float)analogRead(SOIL_PIN) * (100.0 / kMAXHUMIDITY));
    avg_moisture += moistValue;
    readings++;
  }
  digitalWrite(POWER_PIN, LOW);
  avg_moisture /= readings;
  return avg_moisture;
}

int WaterOnFor(String amound) {
  int seconds_on = amound.toInt();
  unsigned long start_time = millis();
  digitalWrite(PUMP_PIN, HIGH);
  delay(seconds_on * 1000);
  digitalWrite(PUMP_PIN, LOW);
  return seconds_on;
}

#define kOneSecond 1000
#define kFailSafeTimeOUT 1000 * 60
#define kDropLiter 0.1

float DropWater(String amound) {
  float water_amount = amound.toFloat();
  static unsigned long start_time = 0;
  static char no_water_time = 0;
  static float volume = 0;

  unsigned long fail_safe_time = millis();
  unsigned char waterflow_flag = 0;
  while (1) {
    // Increment flow if running.
    Particle.publish(
        "DEBUG", String::format("%f start measure flow", millis() / 1000.0));
    if (digitalRead(FLOW_PIN) == 1) {
      if (digitalRead(FLOW_PIN) == 1 && waterflow_flag == 0) {
        waterflow_flag = 1;
        nb_tops_fan++;
      }
    } else if (waterflow_flag == 1) {
      waterflow_flag = 0;
    }
    Particle.publish(
        "DEBUG", String::format("%f sec stop measure flow", millis() / 1000.0));

    if ((millis() - start_time) > kOneSecond) {
      unsigned int waterflow_rate = (nb_tops_fan * 60 / 73);  // L/Hour
      if (((float)(nb_tops_fan) / 73 / 60) < 0.005) {
        if (no_water_time++ >= 3) {  // 10sec?
          no_water_time = 0;
          // SystemWarning = NoWaterWarning;
          return -1;
        }
        volume += (float)((float)(nb_tops_fan) / 73 / 60);
      } else {
        no_water_time = 0;
        volume += (float)((float)(nb_tops_fan) / 73 / 60 + 0.005);
      }

      nb_tops_fan = 0;
      Particle.publish("DEBUG", String::format("%2d L/H", waterflow_rate));
      Particle.publish("DEBUG", String::format("%f L", (int)(volume)));
      Particle.publish(
          "DEBUG", String::format("%d % humidity", RecomputeAverageMoisture()));

      // We dropped enough, time to stop
      if (volume >= water_amount) {  // kDropLiter) {
        Particle.publish(
            "DEBUG", String::format("Finished Dropped %f L", (int)(volume)));
        return volume;
      }
      start_time = millis();
    }
    if ((millis() - fail_safe_time) > kFailSafeTimeOUT) {
      Particle.publish(
          "DEBUG", String::format("Fail timeout Dropped %f L", (int)(volume)));
      // fail safe tiemout.
      return -2;
    }
  }
}
