//Includes---------------------------------------------------------------------
#include "Message.h"
#include <ArduinoJson.h>
#include "../lib/sht-sensor-lib/sht21.h"

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "Wire.h"

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>
#include "ApplicationConfiguration.h"

static int interval = INTERVAL;

#if SIMULATED_DATA
void initSensor()
{
	// use SIMULATED_DATA, no sensor need to be inited
}

float readTemperature()
{
	return random(20, 30);
}

float readHumidity()
{
	return random(30, 40);
}
#else

static SHT21 sht;

void initSensor()
{
	sht.begin();
}

float readTemperature()
{
	return sht.readTemperature();
}

float readHumidity()
{
	return sht.readHumidity();
}

#endif

bool readMessage(int messageId, char *payload)
{
	float temperature = readTemperature();
	float humidity = readHumidity();
	StaticJsonDocument<MESSAGE_MAX_LEN> jsonDocument;
//	jsonDocument["key"] = "value";
//	JsonObject &root = jsonDocument.createObject();
	jsonDocument["deviceId"] = DEVICE_ID;
	jsonDocument["messageId"] = messageId;
	bool temperatureAlert = false;

	// NAN is not the valid json, change it to NULL
	if (std::isnan(temperature)) {
		jsonDocument["temperature"] = NULL;
	} else {
		jsonDocument["temperature"] = temperature;
		if (temperature > TEMPERATURE_ALERT) temperatureAlert = true;
	}

	if (std::isnan(humidity)) {
		jsonDocument["humidity"] = NULL;
	} else {
		jsonDocument["humidity"] = humidity;
	}
//	jsonDocument.printTo(payload, MESSAGE_MAX_LEN);
	return temperatureAlert;
}

void parseTwinMessage(char *message)
{
	StaticJsonDocument<MESSAGE_MAX_LEN> jsonDocument;
//	JsonObject &root = jsonDocument.parseObject(message);

	DeserializationError error = deserializeJson(jsonDocument, message);
	if (error) {
		Serial.printf("Parse %s failed.\r\n", message);
		return;
	}

	if (jsonDocument["desired"]["interval"]) {
		interval = jsonDocument["desired"]["interval"];
	} else if (jsonDocument.containsKey("interval")) {
		interval = jsonDocument["interval"];
	}
}
