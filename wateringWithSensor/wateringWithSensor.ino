#include <Adafruit_CC3000.h>
#include <Adafruit_CC3000_Server.h>
#include <ccspi.h>
#include <Client.h>
#include <SPI.h>
#include <string.h>
#include "Secrets.h"

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

const unsigned long kSECOND = 1000;
const unsigned long kMIN = 60 * kSECOND;
const unsigned long kHOUR = 60 * kMIN;
const unsigned long watertime = 15 * kSECOND; // how long to water in miliseconds
const unsigned long waittime =  2 * kSECOND; // how long to wait between watering
const unsigned long StreamInterval = 300 * 1000.0; // stream intervals in seconds
const unsigned int BasilThreshold = 500;

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
#define DEBUG 0
#if DEBUG
  #define INTERVAL_TIME 1000
  #define debug(format, ...) \
  debug_print(format, ## __VA_ARGS__)
#else
  #define INTERVAL_TIME 10*1000
  #define debug(format, ...) do {} while (0)
#endif

/**************************************************************************/
// @Main Routines
/**************************************************************************/
void setup() {
  // put your main code here, to run repeatedly:
  Serial.begin(9600);
  Serial.println("Start.");
  pinMode(motorPin, OUTPUT);
  while(!setupWifi()) {
    delay(300);
  }
  Serial.println("Setup complete.");
  debug("Watertime set to %d", watertime);
  sendData("", "0");
}

// State Machine
unsigned long long lastStart = 0;
unsigned long long previousMillis = 0;
int watering = 0;

void loop() {
  unsigned long long currentMillis = millis();
  int moistValue = analogRead(moistureSensorPin);

  if(moistValue < BasilThreshold && !watering) {
    debug("Watering for %i sec, moisture %i", watertime/kSECOND, moistValue);
    lastStart = currentMillis;
    startWater();
  } else if (currentMillis > lastStart + watertime && watering) {
    debug("Stop watering moisture %i", moistValue);
    stopWater();
  } else if(currentMillis - previousMillis > StreamInterval || previousMillis == 0) {
    previousMillis = currentMillis;
    debug("Reading moisture:%i", moistValue);
    debug("Start stream.");
    sendData(String(moistValue, DEC), "");
    debug("End stream.");
  }
  delay(INTERVAL_TIME);        // delay in between reads for stability
}

/**************************************************************************/
// @brief  Watering Routines
/**************************************************************************/
void startWater() {
  watering = 1;
  digitalWrite(motorPin, HIGH);
  sendData("", "1");
}

void stopWater() {
  watering = 0;
  digitalWrite(motorPin, LOW);
  sendData("", "0");
}

/**************************************************************************/
// @brief  Send moisture data to thingspeak
/**************************************************************************/
#define WEBSITE "api.thingspeak.com"
#define WEBPAGE "/update"
uint32_t ip = 0;
Adafruit_CC3000_Client www;
void sendData(String field1, String field2) {
  String fields_values = "";
  if (field1.length() > 0) {
    fields_values += "field1="+field1;
  }
  if (field2.length() > 0) {
    fields_values += fields_values.length() > 0 ? "&" : "";
    fields_values += "field2="+field2;
  }
  debug("Sending to thingspeak:'%s'", fields_values.c_str());
  // Try looking up the website's IP address
  while (ip == 0) {
    debug("Looking for website's ip");
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
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
  www.fastrprint(String(fields_values.length(), DEC).c_str());
  www.fastrprint("\n\n");
  www.fastrprint(fields_values.c_str());
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
}

/**************************************************************************/
// @brief  Network Routines
/**************************************************************************/
bool setupWifi(void) {
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
  return true;
}

bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;

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
}

uint16_t checkFirmwareVersion(void) {
  uint8_t  major, minor;
  uint16_t version = 0;

#ifndef CC3000_TINY_DRIVER
  if(!cc3000.getFirmwareVersion(&major, &minor)) {
    debug("Unable to retrieve the firmware version!\r\n");
  } else {
    debug("Firmware V. : %d.%d", major, minor);
    version = ((uint16_t)major << 8) | minor;
  }
#endif
  return version;
}

bool ping(uint32_t ip) {
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
#else
  return false;
#endif
}
