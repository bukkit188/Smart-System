/*
 * main_rebase.cpp
 *
 *  Created on: Jan 1, 2019
 *      Author: nhan
 */
// Include libraries to use
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
// Define value
#define RFID_SCAN 0
#define PEOPLE_PRESENT 1
#define PEOPLE_NOT_PRESENT 2
// Function declaration
void RFIDCheckTask(void *pvParameters);
void sendHTTPRequest(String UID, int mode);
void blockRFIDReader(int blockTime);
void blockRFIDTask(void *pvParameters);
void setup();
void loop();
void HTTPResponseHandler(String HTTPResponse);

String readHTTPResponse(int delayTime);
String HTTPRequestBuilder(String UID, int mode);

// Constant Variable Declaration
const char *serverIP = "112.197.235.17";
const String className = "11T";
const String UUID = "0e47f9cf-ac45-46a8-baeb-c5ec048ceec3";
const int serverPORT = 2705;
const int MFRC522RstPin = 13;
const int MFRC522SsPin = 12;

// Normal Variable Declaration
byte rfUID[4];
int FLAG_ISBLOCKED = 0;

WiFiMulti wifiMulti;
MFRC522 mfrc522(MFRC522SsPin, MFRC522RstPin);
WiFiClient wifiClient;

void setup() {
	// setup communication
	Serial.begin(115200);
	SPI.begin();

	// prepare MRFC522
	mfrc522.PCD_Init();
	mfrc522.PCD_DumpVersionToSerial();

	// connect to WiFi using WiFiMulti function
	wifiMulti.addAP("DD_WRT", "NguyenThiHongTho");
	wifiMulti.addAP("Hello_World", "NguyenThiHongTho");
	while (wifiMulti.run() != WL_CONNECTED) {
		Serial.println(".");
		delay(100);
	}
	Serial.print("Connected to");
	Serial.println(WiFi.SSID());

	// freeRTOS time!!
	xTaskCreate(RFIDCheckTask, /* Task main function */"RFIDCheckTask", /* Name of Task */
	30000, /* Task size in bytes */NULL, /* Parameter passed as input of the task */
	3, /* Priority of the task */NULL); /* Task handle */
}
void loop() {
	vTaskDelay(10);
} // end loop
void RFIDCheckTask(void *pvParameters) {
	String card_UID = "";
	while (1) {
		vTaskDelay(10);
		if (FLAG_ISBLOCKED == 0 && mfrc522.PICC_IsNewCardPresent()
				&& mfrc522.PICC_ReadCardSerial()) {
			for (int i = 0; i < 4; i++) {
				rfUID[i] = mfrc522.uid.uidByte[i];
				card_UID = card_UID + String(rfUID[i]);
			}
			Serial.println(card_UID);
			sendHTTPRequest(card_UID, RFID_SCAN);
			card_UID = "";
			mfrc522.PICC_HaltA();
			mfrc522.PCD_StopCrypto1();
		}
	} // end infinite loop;
} // end RFIDCheckTask
void sendHTTPRequest(String UID, int mode) {
	if (!wifiClient.connect(serverIP, serverPORT)) {
		Serial.println("Connect to host failed");
		return;
	} else {
		wifiClient.print(HTTPRequestBuilder(UID, mode));
	}
	Serial.println("");
	HTTPResponseHandler(readHTTPResponse(5000));

} // end sendHTTPRequest

String HTTPRequestBuilder(String UID, int mode) {
	String httpRequest = "";
	switch (mode) {
	case RFID_SCAN:
		httpRequest = String("GET ") + "/:" + className + "/" + UID
				+ " HTTP/1.1\r\n" + "Host: " + serverIP + "\r\n"
				+ "Connection: close \r\n\r\n";
		break;
	case PEOPLE_PRESENT:
		httpRequest = String("GET ") + "/:" + className + "/" + "ON"
				+ " HTTP/1.1\r\n" + "Host: " + serverIP + "\r\n"
				+ "Connection: close\r\n\r\n";
		break;
	case PEOPLE_NOT_PRESENT:
		httpRequest = String("GET ") + "/:" + className + "/" + "OFF"
				+ " HTTP/1.1\r\n" + "Host: " + serverIP + "\r\n"
				+ "Connection: close\r\n\r\n";
		break;
	}
	Serial.println("HTTP REQUEST IS:");
	Serial.println(httpRequest);
	return httpRequest;
}

String readHTTPResponse(int delayTime) {
	String httpResponse = "";
	while (wifiClient.available() == 0) {
		unsigned long timeOut = millis();
		if (millis() - timeOut > 5000) {
			Serial.println("NO HTTP RESPONSE RECIEVED ON TIME");
		}
	}
	Serial.println("Recieved:");
	while (wifiClient.available()) {
		httpResponse = wifiClient.readString();
		Serial.println(httpResponse);
	}
	return httpResponse;
}

void HTTPResponseHandler(String httpResponse) {
	String header;
	String body;
	String content;
	header = httpResponse.substring(0, httpResponse.indexOf("\n"));
	Serial.println(header);
	if (header.indexOf("200 OK")) {
		body = httpResponse.substring(httpResponse.indexOf("\n"));
		content = body.substring(body.indexOf("GMT") + 7);
		if (content.indexOf("OK") != -1) {
			Serial.println("Content: OK");

		} else if (content.indexOf("NOTFOUND") != -1) {
			Serial.println("Content: NOTFOUND");
			Serial.println("Block RFID for 10s");
			blockRFIDReader(10000);

		} else if (content.indexOf("ERROR") != -1) {
			Serial.println("Content: ERROR");
			Serial.println("Block RFID for 30s");
			blockRFIDReader(30000);
		}
	}
}
void blockRFIDReader(int blockTime) {
	xTaskCreate(blockRFIDTask, "blockRFIDTask", 1000, (void *) blockTime, 1,
	NULL);
}
void blockRFIDTask(void *pvParameters) {
	int delayTime = (int) pvParameters;
	FLAG_ISBLOCKED = 1;
	vTaskDelay(delayTime);
	FLAG_ISBLOCKED = 0;
	vTaskDelete(NULL);
}
