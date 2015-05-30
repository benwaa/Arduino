/*************************************************** 
  This is an example for the Adafruit CC3000 Wifi Breakout & Shield

  Designed specifically to work with the Adafruit WiFi products:
  ----> https://www.adafruit.com/products/1469

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/
 
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
/*!
    @brief  Debug print function
*/
/**************************************************************************/
#include <stdarg.h>
void p(char *fmt, ... ) {
        char buf[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(buf, 128, fmt, args);
        va_end (args);
        Serial.println(buf);
}

/**************************************************************************/
/*!
    @brief  Network Routines
*/
/**************************************************************************/
bool setupWifi(void) {
  Serial.println(F("Initialising the CC3000 ..."));
  if (!cc3000.begin()) {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    return false;
  }
  Serial.println(F("\nDeleting old connection profiles"));
  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Failed!"));
    return false;
  }
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    return false;
  }
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Requesting DHCP"));
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
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
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
    Serial.println(F("Unable to retrieve the firmware version!\r\n"));
  } else {
    Serial.print(F("Firmware V. : "));
    Serial.print(major); Serial.print(F(".")); Serial.println(minor);
    version = ((uint16_t)major << 8) | minor;
  }
#endif
  return version;
}

bool ping(uint32_t ip) {
#ifndef CC3000_TINY_DRIVER
  Serial.print(F("Pinging ")); cc3000.printIPdotsRev(ip); Serial.print("...");
  uint8_t replies = cc3000.ping(ip, 5);
  p("%d replies", replies);
  if (replies > 3)
    return true;
  else
    return false;
#else
  return false;
#endif
}

/**************************************************************************/
/*!
    @brief  Send moisture data to thingspeak
*/
/**************************************************************************/
#define WEBSITE "api.thingspeak.com"
#define WEBPAGE "/update"
uint32_t ip = 0;
Adafruit_CC3000_Client www;
void sendData(int moisture) {
  String moistureStr = "field1="+String(moisture, DEC);
  // Try looking up the website's IP address
  while (ip == 0) {
    Serial.print(WEBPAGE); Serial.print(F(" -> "));
    if (! cc3000.getHostByName(WEBSITE, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
    if (ip != 0) cc3000.printIPdotsRev(ip);
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
  p("Sending Data...");
  www.fastrprint(F("POST "));
  www.fastrprint(WEBPAGE);
  www.fastrprint(F(" HTTP/1.1\r\n"));
  www.fastrprint(F("Host: ")); www.fastrprint(WEBSITE); www.fastrprint(F("\r\n"));
  www.fastrprint("Connection: close\n");
  www.fastrprint("X-THINGSPEAKAPIKEY: "); www.fastrprint(THINKSPEAK_API_KEY); www.fastrprint("\n");
  www.fastrprint("Content-Type: application/x-www-form-urlencoded\n");
  www.fastrprint("Content-Length: ");
  www.fastrprint(String(moistureStr.length(), DEC).c_str());
  www.fastrprint("\n\n");
  www.fastrprint(moistureStr.c_str());
  www.fastrprint(F("\r\n"));
  www.println();
  p("Done.");
  p("-------------------------------------");
  /* Read data until either the connection is closed, or the idle timeout is reached. */ 
  unsigned long lastRead = millis();
  while (www.connected()) {
    while (www.available()) {
      char c = www.read();
      Serial.print(c);
      lastRead = millis();
    }
  }
  www.close();
  p("-------------------------------------");
}

/**************************************************************************/
/*!
    @Main Routines
*/
/**************************************************************************/
void setup(void) {
  Serial.begin(9600);
  while(!setupWifi()) {
    delay(300);
  }
  p("Setup complete.");
}

int sensorPin = A0;
int sensorValue = 0;
long streamInterval = 60 * 1000.0; // stream intervals in seconds
long previousMillis = -streamInterval;
void loop() {
  sensorValue = analogRead(sensorPin);
  p("Moisture is:%3d\n", sensorValue);
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > streamInterval) {
    previousMillis = currentMillis;
    p("Start stream.");
    sendData(sensorValue);
    p("End stream.");
  }
  delay(1000);
}
