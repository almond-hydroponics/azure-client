//Includes---------------------------------------------------------------------
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "Wire.h"

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>
#include "ApplicationConfiguration.h"
#include "DeviceCredentials.h"
#include "Globals.h"
#include <cstdio>
#include "SerialReader.h"
#include "Message.h"

static char *ssid;
static char *pass;
static char *connectionString;

static WiFiClientSecure sslClient; // for ESP8266

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

static bool messagePending = false;
static bool messageSending = true;

const char *onSuccess = "\"Successfully invoke device method\"";
const char *notFound = "\"No method found\"";

//Implementation---------------------------------------------------------------
void blinkLED()
{
	digitalWrite(PIN_LED, HIGH);
	delay(500);
	digitalWrite(PIN_LED, LOW);
}

static void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
	if (IOTHUB_CLIENT_CONFIRMATION_OK == result) {
		Serial.println("Message sent to Azure IoT Hub.");
		blinkLED();
	}
	else {
		Serial.println("Failed to send message to Azure IoT Hub.");
	}
	messagePending = false;
}

static void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, char *buffer, bool temperatureAlert)
{
	IOTHUB_MESSAGE_HANDLE messageHandle =
		IoTHubMessage_CreateFromByteArray((const unsigned char *)buffer,
										  strlen(buffer));
	if (messageHandle == nullptr) {
		Serial.println("Unable to create a new IoTHubMessage.");
	}
	else {
		MAP_HANDLE properties = IoTHubMessage_Properties(messageHandle);
		Map_Add(properties,
				"temperatureAlert",
				temperatureAlert ? "true" : "false");
		Serial.printf("Sending message: %s.\r\n", buffer);
		if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback,nullptr) != IOTHUB_CLIENT_OK) {
			Serial.println("Failed to hand over the message to IoTHubClient.");
		}
		else {
			messagePending = true;
			Serial.println("IoTHubClient accepted the message for delivery.");
		}

		IoTHubMessage_Destroy(messageHandle);
	}
}

void start()
{
	Serial.println("Start sending temperature and humidity data.");
	messageSending = true;
}

void stop()
{
	Serial.println("Stop sending temperature and humidity data.");
	messageSending = false;
}

IOTHUBMESSAGE_DISPOSITION_RESULT receiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
	IOTHUBMESSAGE_DISPOSITION_RESULT result;
	const unsigned char *buffer;
	size_t size;
	if (IoTHubMessage_GetByteArray(message, &buffer, &size)
		!= IOTHUB_MESSAGE_OK) {
		Serial.println("Unable to IoTHubMessage_GetByteArray.");
		result = IOTHUBMESSAGE_REJECTED;
	}
	else {
		/*buffer is not zero terminated*/
		char *temp = (char *)malloc(size + 1);

		if (temp == nullptr) {
			return IOTHUBMESSAGE_ABANDONED;
		}

		strncpy(temp, (const char *)buffer, size);
		temp[size] = '\0';
		Serial.printf("Receive C2D message: %s.\r\n", temp);
		free(temp);
		blinkLED();
	}
	return IOTHUBMESSAGE_ACCEPTED;
}

int deviceMethodCallback(
	const char *methodName,
	const unsigned char *payload,
	size_t size,
	unsigned char **response,
	size_t *response_size,
	void *userContextCallback
)
{
	Serial.printf("Try to invoke method %s.\r\n", methodName);
	const char *responseMessage = onSuccess;
	int result = 200;

	if (strcmp(methodName, "start") == 0) {
		start();
	}
	else if (strcmp(methodName, "stop") == 0) {
		stop();
	}
	else {
		Serial.printf("No method %s found.\r\n", methodName);
		responseMessage = notFound;
		result = 404;
	}

	*response_size = strlen(responseMessage);
	*response = (unsigned char *)malloc(*response_size);
	strncpy((char *)(*response), responseMessage, *response_size);

	return result;
}

void twinCallback(
	DEVICE_TWIN_UPDATE_STATE updateState,
	const unsigned char *payLoad,
	size_t size,
	void *userContextCallback)
{
	char *temp = (char *)malloc(size + 1);
	for (int i = 0; i < size; i++) {
		temp[i] = (char)(payLoad[i]);
	}
	temp[size] = '\0';
	parseTwinMessage(temp);
	free(temp);
}

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

	if (ssidLength > 0 && passLength > 0 && connectionStringLength > 0 && !needEraseEEPROM()) return;

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
			Serial.println(
				"Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
			delay(2000);
		}
		else {
			Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
			break;
		}
	}
}

void setup()
{
	pinMode(PIN_LED, OUTPUT);
	Wire.begin(PIN_SDA, PIN_SCL);

	initSerial();
	delay(2000);
	readCredentials();

	initWifi();
	initTime();
	initSensor();

	iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
	if (iotHubClientHandle == nullptr)
	{
		Serial.println("Failed on IoTHubClient_CreateFromConnectionString.");
		while (true);
	}

	IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, nullptr);
	IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, nullptr);
	IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, twinCallback, nullptr);

}

static int messageCount = 1;
void loop()
{
    if (!messagePending && messageSending)
    {
        char messagePayload[MESSAGE_MAX_LEN];
        bool temperatureAlert = readMessage(messageCount, messagePayload);
        sendMessage(iotHubClientHandle, messagePayload, temperatureAlert);
        messageCount++;
        delay(interval);
    }
    IoTHubClient_LL_DoWork(iotHubClientHandle);
    delay(10);
}
