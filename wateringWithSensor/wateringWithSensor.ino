#include <Adafruit_CC3000.h>
#include <Adafruit_CC3000_Server.h>
#include <ccspi.h>
#include <Client.h>
#include <SPI.h>
#include <string.h>
#include "Secrets.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define WIFI_ON 1
#define DEBUG 0

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS,
                                         ADAFRUIT_CC3000_IRQ,
                                         ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER);

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

/**************************************************************************/
// @brief  System Constants
/**************************************************************************/
int motorPin = A0;
int moistureSensorPin = A1;
int moistureInputPIN = 8;
int moistureOutputPIN = 9;

#define kMAXHUMIDITY 1023.0

const unsigned long kSECOND = 1000;
const unsigned long kMIN = 60 * kSECOND;
const unsigned long kHOUR = 60 * kMIN;
const unsigned long kWaterTime = 25 * kSECOND; // how long to water in miliseconds
const unsigned long kRestartWaterTime =  5 * kMIN; // how long to wait between watering
const unsigned long kStreamInterval = 8 * kMIN; // stream intervals in milliseconds
const unsigned int BasilThreshold = 50; // % of humidity
const unsigned int nbRunningAvg = 5; // number of point to compute the avg over

/**************************************************************************/
// @brief  Debug print function
/**************************************************************************/
#include <stdarg.h>
void debug_print(char const*fmt, ... ) {
  char buf[128]; // resulting string limited to 128 chars
  sprintf(buf, "(%06lu)", millis());
  va_list args;
  va_start (args, fmt);
  vsnprintf(buf+8, 120, fmt, args);
  va_end (args);
  Serial.println(buf);
}
#if DEBUG
  #define INTERVAL_TIME 1000
  #define debug(format, ...) \
  debug_print(format, ## __VA_ARGS__)
#else
  #define INTERVAL_TIME 10*kMIN // every 10min
  #define debug(format, ...) do {} while (0)
#endif

/**************************************************************************/
// @Main Routines
/**************************************************************************/

// State Machine
unsigned long long waterStartTime = 0;
unsigned long long lastDataSendTime = 0;
int watering = 0;

void setup() {
  // put your main code here, to run repeatedly:
  Serial.begin(9600);
  Serial.println("Start.");
  pinMode(motorPin, OUTPUT);
  pinMode(moistureInputPIN, OUTPUT);
  pinMode(moistureOutputPIN, OUTPUT);
  digitalWrite(moistureInputPIN, LOW);
  digitalWrite(moistureOutputPIN, LOW);
  while(!setupWifi()) {
    delay(300);
  }
  Serial.println("Setup complete.");
}

void loop() {
  long startMillis = millis();
  int avgMoistValue = RecomputeAverageMoisture();
  debug("Loop Begin current avg moisture:%i", avgMoistValue);

  if(avgMoistValue < BasilThreshold && !watering) {
    debug("Watering for up to %i sec", kWaterTime/kSECOND);
    waterStartTime = startMillis;
    startWater();
  } else if (watering && avgMoistValue > BasilThreshold) {
    debug("Stop watering");
    stopWater();
  } else if (watering && (startMillis > waterStartTime + kWaterTime)) {
    debug("Forced Stop watering");
    stopWater();
    delay(kRestartWaterTime); // just wait it's been watering for long.
  } else { // Wait for next measure since nothing happened.
    debug("Waiting for next mesure in:%d msec", INTERVAL_TIME);
    delay(INTERVAL_TIME); // delay in between reads for stability
  }

  if((startMillis - lastDataSendTime) > kStreamInterval ||
        lastDataSendTime == 0) {
    sendData(dataForMoisture(String(avgMoistValue, DEC)));
  }
}

/**************************************************************************/
// @brief  Watering Routines
/**************************************************************************/

int RecomputeAverageMoisture() {
  int avgMoistValue = 0;
  int readings = 0;
  for (int i = 0; i < nbRunningAvg; ++i) {
    // forward
    digitalWrite(moistureInputPIN, HIGH);
    delay(50);
    int moistValue = analogRead(moistureSensorPin) * (100.0 / kMAXHUMIDITY);
    digitalWrite(moistureInputPIN, LOW);
    avgMoistValue += moistValue;
    readings++;
    delay(50);
    // backward
    // digitalWrite(moistureOutputPIN, HIGH);
    // delay(150);
    // int rawmoisture = kMAXHUMIDITY - analogRead(moistureSensorPin);
    // moistValue = rawmoisture * (100.0 / kMAXHUMIDITY);
    // digitalWrite(moistureOutputPIN, LOW);
    // avgMoistValue += moistValue;
    // readings++;
  }
  avgMoistValue /= readings;
  return avgMoistValue;
}

void startWater() {
  watering = 1;
  sendData(dataForPumping(false));
  digitalWrite(motorPin, HIGH);
  sendData(dataForPumping(true));
}

void stopWater() {
  watering = 0;
  // stop before sending data cuz it takes too long.
  digitalWrite(motorPin, LOW);
  sendData(dataForPumping(true));
  sendData(dataForPumping(false));
}

String dataForMoisture(String moistValue) {
  return (moistValue.length() > 0) ? "field1="+moistValue : "";
}

String dataForPumping(boolean isPumping) {
  return isPumping ? "field2=1" : "field2=0";
}

String dataFromValues(String moistValue, boolean isPumping) {
  String fields_values = dataForMoisture(moistValue);
  if (fields_values.length() > 0) {
    fields_values += "&";
  }
  return fields_values + dataForPumping(isPumping);
}

/**************************************************************************/
// @brief  Send moisture data to thingspeak
/**************************************************************************/
#define WEBSITE "api.thingspeak.com"
#define WEBPAGE "/update"
uint32_t ip = 0;
Adafruit_CC3000_Client www;
void sendData(String data) {
#if WIFI_ON
  debug("Sending to thingspeak:'%s'", data.c_str());
  // Try looking up the website's IP address
  while (ip == 0) {
    debug("Looking for website's ip");
    if (!cc3000.getHostByName(WEBSITE, &ip)) {
      debug("Couldn't resolve!");
    }
    if (ip == 0) delay(500);
    #if DEBUG
    if (ip != 0) cc3000.printIPdotsRev(ip);
    #endif
    Serial.println("");
  }
  // Make sure the ping is working
  /* Try connecting to the website.
     Note: HTTP/1.1 protocol is used to keep the server from closing the connection before all data is read.
  */
  do {
    www = cc3000.connectTCP(ip, 80);
  } while(!www.connected());
  while(!ping(ip));
  debug("Sending Data...");
  www.fastrprint(F("POST "));
  www.fastrprint(WEBPAGE);
  www.fastrprint(F(" HTTP/1.1\r\n"));
  www.fastrprint(F("Host: ")); www.fastrprint(WEBSITE); www.fastrprint(F("\r\n"));
  www.fastrprint("Connection: close\n");
  www.fastrprint("X-THINGSPEAKAPIKEY: "); www.fastrprint(THINKSPEAK_API_KEY); www.fastrprint("\n");
  www.fastrprint("Content-Type: application/x-www-form-urlencoded\n");
  www.fastrprint("Content-Length: ");
  www.fastrprint(String(data.length(), DEC).c_str());
  www.fastrprint("\n\n");
  www.fastrprint(data.c_str());
  www.fastrprint(F("\r\n"));
  www.println();
  debug("Done.");
  debug("-------------------------------------");
  /* Read data until either the connection is closed, or the idle timeout is reached. */ 
  unsigned long lastRead = millis();
  while (www.connected()) {
    while (www.available()) {
      char c = www.read();
#if DEBUG
      Serial.print(c);
#endif
      lastRead = millis();
    }
  }
  www.close();
  debug("-------------------------------------");
#endif

  lastDataSendTime = millis();
}

/**************************************************************************/
// @brief  Network Routines
/**************************************************************************/
bool setupWifi(void) {
#if WIFI_ON
  debug("Initialising the CC3000 ...");
  if (!cc3000.begin()) {
    debug("Unable to initialise the CC3000! Check your wiring?");
    return false;
  }
  debug("Deleting old connection profiles");
  if (!cc3000.deleteProfiles()) {
    debug("Failed!");
    return false;
  }
  debug("Attempting to connect to %s", WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    debug("Failed!");
    return false;
  }
  debug("Connected!");

  /* Wait for DHCP to complete */
  debug("Requesting DHCP");
  while (!cc3000.checkDHCP()) {
    delay(100); // ToDo: Insert a DHCP timeout!
  }

  while (!displayConnectionDetails()) {
    delay(1000);
  }
#endif
  return true;
}

bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
#if WIFI_ON
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    debug("Unable to retrieve the IP Address!");
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
#else
  return true;
#endif
}

uint16_t checkFirmwareVersion(void) {
  uint8_t  major, minor;
  uint16_t version = 0;
#if WIFI_ON
#ifndef CC3000_TINY_DRIVER
  if(!cc3000.getFirmwareVersion(&major, &minor)) {
    debug("Unable to retrieve the firmware version!\r\n");
  } else {
    debug("Firmware V. : %d.%d", major, minor);
    version = ((uint16_t)major << 8) | minor;
  }
#endif
#endif
  return version;
}

bool ping(uint32_t ip) {
#if WIFI_ON
#ifndef CC3000_TINY_DRIVER
  #if DEBUG
  Serial.print(F("Pinging ")); cc3000.printIPdotsRev(ip); Serial.print("...\n");
  #endif
  uint8_t replies = cc3000.ping(ip, 5);
  debug("%d replies", replies);
  if (replies > 3)
    return true;
  else
    return false;
#endif
#endif
  return false;
}
