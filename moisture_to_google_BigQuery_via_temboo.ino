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
#include <Temboo.h>
#include "TembooAccount.h" // Contains Temboo account information
#include "utility/debug.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed

Adafruit_CC3000_Client client;

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define IDLE_TIMEOUT_MS  3000      // Amount of time to wait (in milliseconds) with no data
                                   // received before closing the connection.  If you know the server
                                   // you're accessing is quick to respond, you can reduce this value.


/**************************************************************************/
/*!
    @brief  Sets up the HW and the CC3000 module (called automatically
            on startup)
*/
/**************************************************************************/

uint32_t ip;

void setup(void)
{
  Serial.begin(9600);
  while(!Serial);
  Serial.println(F("Hello, CC3000!\n"));

  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);

  /* Initialise the module */
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }

  // Optional SSID scan
  // listSSIDResults();

  status_t wifiStatus = STATUS_DISCONNECTED;
  while (wifiStatus != STATUS_CONNECTED) {
    Serial.print("WiFi:");
    if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
      Serial.println(F("Failed!"));
    } else {
      wifiStatus = STATUS_CONNECTED;
    }
  }

  Serial.println(F("Connected!"));

  Serial.print(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(1000); // ToDo: Insert a DHCP timeout!
    Serial.print(".");
  }
  Serial.println(".");

  /* Display the IP address DNS, Gateway, etc. */
  while (! displayConnectionDetails()) {
    delay(1000);
    Serial.println("Acquiring connection details...\n");
  }

  Serial.println("Setup complete.");
}

int sensorPin = A0;
int sensorValue = 0;

/**************************************************************************/
/*!
    @brief  Debug print function
*/
/**************************************************************************/
#include <stdarg.h>
void p(char *fmt, ... ){
        char buf[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(buf, 128, fmt, args);
        va_end (args);
        Serial.println(buf);
}

int calls = 0;
int maxCalls = 10;
int streamInterval = 90000;
uint32_t lastStreamRunTime = millis() - streamInterval;
void loop() {
  p("Start loop.");
  sensorValue = analogRead(sensorPin);
  p("Value is:%3d\n", sensorValue);
  uint32_t now = millis(); //check current time
  if(now - lastStreamRunTime >= streamInterval) {
    lastStreamRunTime = now;
  p("Start stream.");
    streamMoisture();
  p("End stream.");
  }
  delay(10000);
}

void streamMoisture() {
  TembooChoreo stream(client);

  // Invoke the Temboo client
  stream.begin();

  // Set Temboo account credentials
  stream.setAccountName(TEMBOO_ACCOUNT);
  stream.setAppKeyName(TEMBOO_APP_KEY_NAME);
  stream.setAppKey(TEMBOO_APP_KEY);

  // Identify the Choreo to run
  stream.setChoreo("/Library/Util/StreamSensorData");

  // Set the Streaming profile to use
  stream.setProfile("wateringStreamingSetting");

  // Generate sensor data to stream
  String pinData = "{";
  pinData += "\"Moisture\":" + String(analogRead(A0));
  pinData += "}";

  // Add sensor data as an input to the streaming Choreo
  p("pindata %s",pinData.c_str());
  stream.addInput("SensorData", pinData);
  // NOTE: for debugging set "Async" to false (indicating that a response should be returned)
  stream.addInput("Async", "false");

  // Stream the data; when results are available, print them to serial
  p("Start run.");
  p("run out val %d",stream.run());
  while(stream.available()) {
    char c = stream.read();
    Serial.print(c);
  }
  p("Close");
  stream.close();
}

/**************************************************************************/
/*!
    @brief  Begins an SSID scan and prints out all the visible networks
*/
/**************************************************************************/

void listSSIDResults(void)
{
  uint32_t index;
  uint8_t valid, rssi, sec;
  char ssidname[33];

  if (!cc3000.startSSIDscan(&index)) {
    Serial.println(F("SSID scan failed!"));
    return;
  }

  Serial.print(F("Networks found: ")); Serial.println(index);
  Serial.println(F("================================================"));

  while (index) {
    index--;

    valid = cc3000.getNextSSID(&rssi, &sec, ssidname);

    Serial.print(F("SSID Name    : ")); Serial.print(ssidname);
    Serial.println();
    Serial.print(F("RSSI         : "));
    Serial.println(rssi);
    Serial.print(F("Security Mode: "));
    Serial.println(sec);
    Serial.println();
  }
  Serial.println(F("================================================"));

  cc3000.stopSSIDscan();
}

/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
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
