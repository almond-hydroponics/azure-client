#pragma once

void initSensor();

bool readMessage(int messageId, char *payload);

void parseTwinMessage(char *message);
