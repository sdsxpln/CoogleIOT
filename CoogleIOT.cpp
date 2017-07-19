/*
  +----------------------------------------------------------------------+
  | CoogleIOT for ESP8266                                                |
  +----------------------------------------------------------------------+
  | Copyright (c) 2017 John Coggeshall                                   |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");      |
  | you may not use this file except in compliance with the License. You |
  | may obtain a copy of the License at:                                 |
  |                                                                      |
  | http://www.apache.org/licenses/LICENSE-2.0                           |
  |                                                                      |
  | Unless required by applicable law or agreed to in writing, software  |
  | distributed under the License is distributed on an "AS IS" BASIS,    |
  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or      |
  | implied. See the License for the specific language governing         |
  | permissions and limitations under the License.                       |
  +----------------------------------------------------------------------+
  | Authors: John Coggeshall <john@coggeshall.org>                       |
  +----------------------------------------------------------------------+
*/

#include "CoogleIOT.h"

#define COOGLEIOT_DEBUG

#ifdef COOGLEIOT_DEBUG
#define COOGLEEEPROM_DEBUG
#endif

CoogleIOT::CoogleIOT(int statusPin)
{
    _statusPin = statusPin;
    _serial = false;

}

CoogleIOT::CoogleIOT()
{
	CoogleIOT(-1);
}

bool CoogleIOT::serialEnabled()
{
	return _serial;
}

void CoogleIOT::loop()
{
	if(mqttClientActive) {
		if(!mqttClient.connected()) {
			if(!connectToMQTT()) {
				flashSOS();
				return;
			}
		}

		mqttClient.loop();
	}
	
	webServer->loop();
}

CoogleIOT& CoogleIOT::flashSOS()
{
	for(int i = 0; i < 3; i++) {
		flashStatus(200, 3);
		delay(1000);
		flashStatus(500, 3);
		delay(1000);
		flashStatus(200, 3);
		delay(5000);
	}
	
	return *this;
}

CoogleIOT& CoogleIOT::flashStatus(int speed)
{
	flashStatus(speed, 5);
	return *this;
}

CoogleIOT& CoogleIOT::flashStatus(int speed, int repeat)
{
	if(_statusPin > -1) {
		for(int i = 0; i < repeat; i++) {
			digitalWrite(_statusPin, LOW);
			delay(speed);
			digitalWrite(_statusPin, HIGH);
			delay(speed);
		}
		
		digitalWrite(_statusPin, HIGH);
	}
	
	return *this;
}

bool CoogleIOT::initialize()
{
	
	if(_statusPin > -1) {
		pinMode(_statusPin, OUTPUT);
		flashStatus(COOGLEIOT_STATUS_INIT);
	}
	
	if(_serial) {
		Serial.println("Coogle IOT v" COOGLEIOT_VERSION " initializing..");
	}
	
	randomSeed(micros());
	
	eeprom.initialize(1024);

	if(!eeprom.isApp((const byte *)COOGLEIOT_MAGIC_BYTES)) {
		
		if(_serial) {
			Serial.println("EEPROM not initialized for platform, erasing..");
		}
		eeprom.reset();
		eeprom.setApp((const byte *)COOGLEIOT_MAGIC_BYTES);
	}
	
	if(!connectToSSID()) {
		if(_serial) {
			Serial.println("Failed to connect to AP");
		}
	} else {
	
		if(!initializeMQTT()) {
			if(_serial) {
				Serial.println("Failed to connect to MQTT");
			}
		}

	}

	enableConfigurationMode();

	return true;
}

void CoogleIOT::enableConfigurationMode()
{
	if(_serial) {
		Serial.println("Enabling Configuration Mode");
	}

	initializeLocalAP();

	if(_serial) {
		Serial.println("CIOT: Creating Webserver");
	}

	webServer = new CoogleIOTWebserver(*this);

	if(!webServer->initialize()) {
		if(_serial) {
			Serial.println("Failed to initialize Web Server");
		}

		flashSOS();
	}
}


void CoogleIOT::initializeLocalAP()
{
	String localAPName, localAPPassword;

	IPAddress apLocalIP(192,168,0,1);
	IPAddress apSubnetMask(255,255,255,0);
	IPAddress apGateway(192,168,0,1);
	
	localAPName = getAPName();
	localAPPassword = getAPPassword();
	
	if(localAPPassword.length() == 0) {
		if(_serial) {
			Serial.println("No AP Password found in memory");
			Serial.println("Setting to default password: " COOGLEIOT_AP_DEFAULT_PASSWORD);
		}

		localAPPassword = COOGLEIOT_AP_DEFAULT_PASSWORD;
		setAPPassword(localAPPassword);

	}

	if(localAPName.length() == 0) {
		if(_serial) {
			Serial.println("No AP Name found in memory. Auto-generating AP name");
		}

		localAPName = COOGLEIOT_AP;
		localAPName.concat((int)random(100000, 999999));

		if(_serial) {
			Serial.print("Setting AP Name To: ");
			Serial.println(localAPName);
		}

		setAPName(localAPName);

		localAPName = getAPName();

		Serial.print("AP Name is: ");
		Serial.println(localAPName);
	}
	
	if(_serial) {
		Serial.println("Initializing WiFi");
		Serial.print("Local AP Name: ");
		Serial.println(localAPName);
	}

	WiFi.mode(WIFI_AP_STA);
	WiFi.softAPConfig(apLocalIP, apGateway, apSubnetMask);
	WiFi.softAP(localAPName.c_str(), localAPPassword.c_str());
	
	if(_serial) {
		Serial.print("Local IP Address: ");
		Serial.println(WiFi.softAPIP());
	}
	
}

String CoogleIOT::getMQTTHostname()
{
	char mqttHost[COOGLEIOT_MQTT_HOST_MAXLEN];

	if(!eeprom.readString(COOGLEIOT_MQTT_HOST_ADDR, mqttHost, COOGLEIOT_MQTT_HOST_MAXLEN)) {
		if(_serial) {
			Serial.println("Failed to read MQTT Server Hostname");
		}
	}

	String retval(mqttHost);
	return retval;
}

String CoogleIOT::getMQTTClientId()
{
	char mqtt[COOGLEIOT_MQTT_CLIENT_ID_MAXLEN];

	if(!eeprom.readString(COOGLEIOT_MQTT_CLIENT_ID_ADDR, mqtt, COOGLEIOT_MQTT_CLIENT_ID_MAXLEN)) {
		if(_serial) {
			Serial.println("Failed to read MQTT Client ID");
		}
	}

	String retval(mqtt);
	return retval;
}

String CoogleIOT::getMQTTUsername()
{
	char mqtt[COOGLEIOT_MQTT_USER_MAXLEN];

	if(!eeprom.readString(COOGLEIOT_MQTT_USER_ADDR, mqtt, COOGLEIOT_MQTT_USER_MAXLEN)) {
		if(_serial) {
			Serial.println("Failed to read MQTT Username");
		}
	}

	String retval(mqtt);
	return retval;
}

String CoogleIOT::getMQTTPassword()
{
	char mqtt[COOGLEIOT_MQTT_USER_PASSWORD_MAXLEN];

	if(!eeprom.readString(COOGLEIOT_MQTT_USER_PASSWORD_ADDR, mqtt, COOGLEIOT_MQTT_USER_PASSWORD_MAXLEN)) {
		if(_serial) {
			Serial.println("Failed to read MQTT Password");
		}
	}

	String retval(mqtt);
	return retval;
}

int CoogleIOT::getMQTTPort()
{
	int mqtt;

	if(!eeprom.readInt(COOGLEIOT_MQTT_PORT_ADDR, &mqtt)) {
		if(_serial) {
			Serial.println("Failed to read MQTT Port");
		}
	}

	return mqtt;
}

CoogleIOT& CoogleIOT::setMQTTPort(int port)
{
	if(!eeprom.writeInt(COOGLEIOT_MQTT_PORT_ADDR, port)) {
		if(_serial) {
			Serial.println("Failed to write MQTT Port to memory!");
		}
	}

	return *this;
}

CoogleIOT& CoogleIOT::setMQTTClientId(String s)
{
	if(s.length() > COOGLEIOT_MQTT_CLIENT_ID_MAXLEN) {
		if(_serial) {
			Serial.println("Attempted to write beyond max length!");
		}
		return *this;
	}

	if(!eeprom.writeString(COOGLEIOT_MQTT_CLIENT_ID_ADDR, s)) {
		if(_serial) {
			Serial.println("Failed to write MQTT default Client ID");
		}
	}

	return *this;
}

CoogleIOT& CoogleIOT::setMQTTHostname(String s)
{
	if(s.length() > COOGLEIOT_MQTT_HOST_MAXLEN) {
		if(_serial) {
			Serial.println("Attempted to write beyond max length!");
		}
		return *this;
	}

	if(!eeprom.writeString(COOGLEIOT_MQTT_HOST_ADDR, s)) {
			if(_serial) {
				Serial.println("Failed to write MQTT default Client ID");
			}
		}

	return *this;
}

CoogleIOT& CoogleIOT::setMQTTUsername(String s)
{
	if(s.length() > COOGLEIOT_MQTT_USER_MAXLEN) {
		if(_serial) {
			Serial.println("Attempted to write beyond max length!");
		}
		return *this;
	}

	if(!eeprom.writeString(COOGLEIOT_MQTT_USER_ADDR, s)) {
			if(_serial) {
				Serial.println("Failed to write MQTT default Client ID");
			}
		}

	return *this;
}

CoogleIOT& CoogleIOT::setMQTTPassword(String s)
{
	if(s.length() > COOGLEIOT_MQTT_USER_PASSWORD_MAXLEN) {
		if(_serial) {
			Serial.println("Attempted to write beyond max length!");
		}
		return *this;
	}

	if(!eeprom.writeString(COOGLEIOT_MQTT_USER_PASSWORD_ADDR, s)) {
			if(_serial) {
				Serial.println("Failed to write MQTT default Client ID");
			}
		}

	return *this;
}

CoogleIOT& CoogleIOT::setRemoteAPName(String s)
{
	if(s.length() > COOGLEIOT_REMOTE_AP_NAME_MAXLEN) {
		if(_serial) {
			Serial.println("Attempted to write beyond max length!");
		}
		return *this;
	}

	if(!eeprom.writeString(COOGLEIOT_REMOTE_AP_NAME_ADDR, s)) {
			if(_serial) {
				Serial.println("Failed to write MQTT default Client ID");
			}
		}

	return *this;
}

CoogleIOT& CoogleIOT::setRemoteAPPassword(String s)
{
	if(s.length() > COOGLEIOT_REMOTE_AP_PASSWORD_MAXLEN) {
		if(_serial) {
			Serial.println("Attempted to write beyond max length!");
		}
		return *this;
	}

	if(!eeprom.writeString(COOGLEIOT_REMOTE_AP_PASSWORD_ADDR, s)) {
			if(_serial) {
				Serial.println("Failed to write MQTT default Client ID");
			}
		}

	return *this;
}

CoogleIOT& CoogleIOT::setAPName(String s)
{
	if(s.length() > COOGLEIOT_AP_NAME_MAXLEN) {
		if(_serial) {
			Serial.println("Attempted to write beyond max length!");
		}
		return *this;
	}

	if(!eeprom.writeString(COOGLEIOT_AP_NAME_ADDR, s)) {
			if(_serial) {
				Serial.println("Failed to write MQTT default Client ID");
			}
		}

	return *this;
}

CoogleIOT& CoogleIOT::setAPPassword(String s)
{
	if(s.length() > COOGLEIOT_AP_PASSWORD_MAXLEN) {
		if(_serial) {
			Serial.println("Attempted to write beyond max length!");
		}
		return *this;
	}

	if(!eeprom.writeString(COOGLEIOT_AP_PASSWORD_ADDR, s)) {
			if(_serial) {
				Serial.println("Failed to write AP Password");
			}
		}

	return *this;
}

bool CoogleIOT::initializeMQTT()
{
	String mqttHostname, mqttClientId, mqttUsername, mqttPassword;
	int mqttPort;

	flashStatus(COOGLEIOT_STATUS_MQTT_INIT);

	mqttHostname = getMQTTHostname();

	if(mqttHostname.length() == 0) {
		
		if(_serial) {
			Serial.println("No MQTT Hostname specified. Cannot continue");
			mqttClientActive = false;
			return false;
		}
	}
	
	mqttClientId = getMQTTClientId();

	if(mqttClientId.length() == 0) {
		if(_serial) {
			Serial.println("Failed to read MQTT Client ID. Setting to Default");
		}
		
		mqttClientId = COOGLEIOT_DEFAULT_MQTT_CLIENT_ID;
		
		setMQTTClientId(mqttClientId);
	}
	
	mqttPort = getMQTTPort();
	
	if(mqttPort == 0) {
		if(_serial) {
			Serial.println("Failed to read MQTT Port from memory. Setting to Default");
		}
		
		setMQTTPort(COOGLEIOT_DEFAULT_MQTT_PORT);
	}
	
	mqttClient.setServer(mqttHostname.c_str(), mqttPort);
	
	/**
	 * @todo Callback here
	 */
	
	return connectToMQTT();
}

PubSubClient CoogleIOT::getMQTTClient()
{
	return mqttClient;
}

bool CoogleIOT::connectToMQTT()
{
	bool connectResult;
	String mqttHostname, mqttUsername, mqttPassword, mqttClientId;
	int mqttPort;

	if(mqttClient.connected()) {
		return true;
	}

	mqttHostname = getMQTTHostname();
	mqttUsername = getMQTTUsername();
	mqttPassword = getMQTTPassword();
	mqttPort = getMQTTPort();
	mqttClientId = getMQTTClientId();

	if(mqttHostname.length() == 0) {
		return false;
	}

	if(_serial) {
		Serial.println("Attempting to connect to MQTT Server");
		Serial.print("Server: ");
		Serial.print(mqttHostname);
		Serial.print(":");
		Serial.println(mqttPort);
	}
	
	for(int i = 0; (i < 5) && (!mqttClient.connected()); i++) {
		
		if(mqttUsername.length() == 0) {
			connectResult = mqttClient.connect(mqttClientId.c_str());
		} else {
			connectResult = mqttClient.connect(mqttClientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str());
		}
		
		if(!connectResult) {
			if(_serial) {
				Serial.println("Could not connect to MQTT Server.. Retrying in 5 seconds..");
				Serial.print("State: ");
				Serial.println(mqttClient.state());
				delay(5000);
			}
		}
	}
	
	if(!mqttClient.connected()) {

		if(_serial) {
			Serial.println("Failed to connect to MQTT Server! Aborting.");
		}
		
		flashSOS();
		mqttClientActive = false;
		return false;
	}
	
	mqttClientActive = true;
	return true;
}

String CoogleIOT::getAPName()
{
	char APName[COOGLEIOT_AP_NAME_MAXLEN];

	if(!eeprom.readString(COOGLEIOT_AP_NAME_ADDR, APName, COOGLEIOT_AP_NAME_MAXLEN)) {
		if(_serial) {
			Serial.println("Failed to read AP Name from EEPROM");
		}
	}

	String retval(APName);
	return retval;
}

String CoogleIOT::getAPPassword()
{
	char password[COOGLEIOT_AP_PASSWORD_MAXLEN];

	if(!eeprom.readString(COOGLEIOT_AP_PASSWORD_ADDR, password, COOGLEIOT_AP_PASSWORD_MAXLEN)) {
		if(_serial) {
			Serial.println("Failed to read AP Password from EEPROM");
		}
	}

	String retval(password);
	return retval;
}

String CoogleIOT::getRemoteAPName()
{
	char remoteAPName[COOGLEIOT_AP_NAME_MAXLEN];

	if(!eeprom.readString(COOGLEIOT_REMOTE_AP_NAME_ADDR, remoteAPName, COOGLEIOT_REMOTE_AP_NAME_MAXLEN)) {
		if(_serial) {
			Serial.println("Failed to read Remote AP Name from EEPROM");
		}
	}

	String retval(remoteAPName);
	return retval;
}

String CoogleIOT::getRemoteAPPassword()
{
	char remoteAPPassword[COOGLEIOT_REMOTE_AP_PASSWORD_MAXLEN];

	if(!eeprom.readString(COOGLEIOT_REMOTE_AP_PASSWORD_ADDR, remoteAPPassword, COOGLEIOT_REMOTE_AP_PASSWORD_MAXLEN)) {
		if(_serial) {
			Serial.println("Failed to read remote AP password from EEPROM");
		}
	}

	String retval(remoteAPPassword);
	return retval;
}

bool CoogleIOT::connectToSSID()
{
	String remoteAPName;
	String remoteAPPassword;

	flashStatus(COOGLEIOT_STATUS_WIFI_INIT);
	
	remoteAPName = getRemoteAPName();
	
	if(remoteAPName.length() == 0) {
	
		if(_serial) {
			Serial.println("No Remote AP Found in memory");
		}
		
		return true;
	} 
	
	if(_serial) {
		Serial.print("Connecting to AP: ");
		Serial.println(remoteAPName);
	}
	
	if(remoteAPPassword.length() == 0) {
		if(_serial) {
			Serial.println("WARNING No Remote AP Password Set");
		}
		
		WiFi.begin(remoteAPName.c_str());
		
	} else {
		
		WiFi.begin(remoteAPName.c_str(), remoteAPPassword.c_str());
		
	}
	
	for(int i = 0; (i < 20) && (WiFi.status() != WL_CONNECTED); i++) {
		delay(500);
		
		if(_serial) {
			Serial.print('.');
		}
		
	}
	
	if(WiFi.status() != WL_CONNECTED) {
		if(_serial) {
			Serial.println("ERROR: Could not connect to AP!");
			
		}
		
		flashSOS();
		
		return false;
	}
	
	if(_serial) {
		Serial.println("");
		Serial.println("Connected to Remote AP");
		Serial.print("Remote IP Address: ");
		Serial.println(WiFi.localIP());
	}
	
	return true;
}

CoogleIOT& CoogleIOT::enableSerial()
{
	return enableSerial(15200);
}

CoogleIOT& CoogleIOT::enableSerial(int baud)
{
    if(!Serial) {

      Serial.begin(baud);
  
      while(!Serial) {
          /* .... tic toc .... */
      }

    }

    _serial = true;
    return *this;
}
