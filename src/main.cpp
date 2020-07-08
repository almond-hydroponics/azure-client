//Includes---------------------------------------------------------------------
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>
#include "ApplicationConfiguration.h"
#include "DeviceCredentials.h"
#include "Globals.h"
#include <cstdio>
#include "SerialReader.h"

static char *ssid;
static char *pass;
static char *connectionString;

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

void blinkLED()
{
	digitalWrite(PIN_LED, HIGH);
	delay(500);
	digitalWrite(PIN_LED, LOW);
}

//Implementation---------------------------------------------------------------
void readCredentials()
{
	int ssidAddress = 0;
	int passAddress = ssidAddress + SSID_LEN;
	int connectionStringAddress = passAddress + SSID_LEN;

	// malloc for parameters
	ssid = (char *)malloc(SSID_LEN);
	pass = (char *)malloc(PASS_LEN);
	connectionString = (char *)malloc(CONNECTION_STRING_LEN);

	int ssidLength = readEEPROM(ssidAddress, ssid);
	int passLength = readEEPROM(passAddress, pass);
	int connectionStringLength =
		readEEPROM(connectionStringAddress, connectionString);

	if (ssidLength > 0 && passLength > 0 && connectionStringLength > 0
		&& !needEraseEEPROM())
		return;

	// read from Serial and save to EEPROM
	readFromSerial("Input your Wi-Fi SSID: ", ssid, SSID_LEN, 0);
	writeEEPROM(ssidAddress, ssid, strlen(ssid));

	readFromSerial("Input your Wi-Fi password: ", pass, PASS_LEN, 0);
	writeEEPROM(passAddress, pass, strlen(pass));

	readFromSerial("Input your Azure IoT hub device connection string: ", connectionString, CONNECTION_STRING_LEN, 0);
	writeEEPROM(connectionStringAddress, connectionString, strlen(connectionString));
}

void initWifi()
{
	// Attempt to connect to Wifi network:
	Serial.printf("Attempting to connect to SSID: %s.\r\n", ssid);

	// Connect to WPA/WPA2 network. Change this line if using open or WEP network:
	WiFi.begin(ssid, pass);
	while (WiFi.status() != WL_CONNECTED) {
		// Get Mac Address and show it.
		// WiFi.macAddress(mac) save the mac address into a six length array, but the endian may be different. The huzzah board should
		// start from mac[0] to mac[5], but some other kinds of board run in the oppsite direction.
		uint8_t mac[6];
		WiFi.macAddress(mac);
		Serial.printf(
			"You device with MAC address %02x:%02x:%02x:%02x:%02x:%02x connects to %s failed! Waiting 10 seconds to retry.\r\n",
			mac[0],
			mac[1],
			mac[2],
			mac[3],
			mac[4],
			mac[5],
			ssid);
		WiFi.begin(ssid, pass);
		delay(10000);
	}
	Serial.printf("Connected to wifi %s.\r\n", ssid);
}

void initTime()
{
	time_t epochTime;
	configTime(0, 0, "pool.ntp.org", "time.nist.gov");

	while (true) {
		epochTime = time(nullptr);

		if (epochTime == 0) {
			Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
			delay(2000);
		} else {
			Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
			break;
		}
	}
}

