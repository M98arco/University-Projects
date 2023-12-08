#include <SPI.h>
#include <WiFi101.h>
#include "secrets.h"
#include <Servo.h>

#include <ArduinoJson.h>
#include <MQTT.h>

#define LED_ONBOARD LED_BUILTIN
#define LED LED_BUILTIN
#define LED_GREEN 2  //connect LED to digital pin4
#define LED_RED 1    //connect LED to digital pin1
#define BUTTON 0     //Button
#define BUTTON_SERVO 3    //Button servo
#define BUTTON_DEBOUNCE_DELAY 20  // button debounce time in ms
#define BUZZER_GROVE 4


// MQTT data
#define MQTT_BUFFER_SIZE 128               // the maximum size for packets being published and received
MQTTClient mqttClient(MQTT_BUFFER_SIZE);   // handles the MQTT communication protocol
WiFiClient networkClient;                  // handles the network connection to the MQTT broker
#define MQTT_TOPIC_CONTROL "ferrante_pappagallo/control"   // topic to control led & buzzer
String MQTT_TOPIC_TEMP = "ferrante_pappagallo/temperature/status";    // topic to publish the temp status
String MQTT_TOPIC_LIGHT = "ferrante_pappagallo/light/status";     // topic to publish the light status
#define MQTT_TOPIC_SERVO "ferrante_pappagallo/servo/status"



// WiFi cfg
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif

WiFiClient client;

Servo myservo;

WiFiServer server(80);

int buttonState = 0;

const unsigned long ledInterval = 3000;
static unsigned long TempInterval = 3000;
static unsigned long LightInterval = 3000;


static byte led_state = LOW;  // current led state, start LOW=OFF     
static byte buzzer_state = LOW; // current buzzer state, start LOW=OFF        
static byte led_green_state = HIGH; // current led green state, start HIGH=ON
static byte moveservo = LOW; // current servo, start LOW=OFF

int pos = 0;

// Mac Address variables
byte mac[6];
String macAddress;


void setup() {
    
  pinMode(LED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUTTON, INPUT);
  pinMode(BUTTON_SERVO, INPUT);
  pinMode(BUZZER_GROVE, OUTPUT);


  digitalWrite(LED_ONBOARD, HIGH);

  digitalWrite(LED_BUILTIN, LOW);   

  digitalWrite(LED_RED, LOW);

  digitalWrite(LED_GREEN, HIGH);

  digitalWrite(BUZZER_GROVE, LOW);


  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);   // setup communication with MQTT broker
  mqttClient.onMessage(mqttMessageReceived);              // callback on message received from MQTT broker 

  Serial.begin(115200);
  Serial.println(F("\n\nSetup completed.\n\n"));
}


void loop() {
  bool static led_status = HIGH;

  static unsigned long lastTled = millis();
  static unsigned long lastTdb = millis();

  connectToWiFi();  // WiFi connect if not established and if connected get wifi signal strength

 
  connectToMQTTBroker();   // connect to MQTT broker (if not already connected)
  mqttClient.loop();       // MQTT client loop

  listenForEthernetClients();   


  if (isButtonPressed() == true) {  // button pressed
    led_state = !led_state;
    buzzer_state = !buzzer_state;
    led_green_state = !led_green_state;
    
    digitalWrite(LED_RED, led_state);
    if (digitalRead(LED_RED) == HIGH) {
      led_state = !led_state;
      digitalWrite(LED_RED, led_state);
      Serial.println(F("Button: PRESSED, Led OFF"));
    }
    
    digitalWrite(BUZZER_GROVE, buzzer_state);
    if (digitalRead(BUZZER_GROVE) == HIGH) {
      buzzer_state = !buzzer_state;
      digitalWrite(BUZZER_GROVE, buzzer_state);
      Serial.println(F("Button: PRESSED, Buzzer OFF"));
    }
    
    digitalWrite(LED_GREEN, led_green_state);
    if (digitalRead(LED_GREEN) == LOW) {
      led_green_state = !led_green_state;
      digitalWrite(LED_GREEN, led_green_state);
      Serial.println(F("Button: PRESSED, Green Led ON"));
    }
  }

  if (buttonServo() == true) {
    moveservo = !moveservo;
    if(moveservo == HIGH) {
      Serial.println(F("Button: PRESSED, Servo ON"));
      const int capacity = JSON_OBJECT_SIZE(2);
      StaticJsonDocument<capacity> doc;
      doc["servo"] = "rotate";
      doc["macAddress"] = (MQTT_TOPIC_SERVO,"F8:F0:05:ED:F1:F1");
      char buffer[256];
      size_t n = serializeJson(doc, buffer);
      Serial.print(F("JSON message: "));
      Serial.println(buffer);
      mqttClient.publish(MQTT_TOPIC_SERVO, "rotate F8:F0:05:ED:F1:F1");
    }   
  }
}


boolean isButtonPressed() {
  static byte lastState = digitalRead(BUTTON);  // the previous reading from the input pin

  for (byte count = 0; count < BUTTON_DEBOUNCE_DELAY; count++) {
    if (digitalRead(BUTTON) == lastState) return false;
    delay(1);
  }

  lastState = !lastState;
  return lastState == LOW ? false : true;
}


boolean buttonServo() {
  static byte lastState = digitalRead(BUTTON_SERVO);  // the previous reading from the input pin

  for (byte count = 0; count < BUTTON_DEBOUNCE_DELAY; count++) {
    if (digitalRead(BUTTON_SERVO) == lastState) return false;
    delay(1);
  }

  lastState = !lastState;
  return lastState == LOW ? false : true;
}



void printWifiStatus() {
  Serial.println(F("\n=== WiFi connection status ==="));


  // SSID
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // signal strength
  Serial.print(F("Signal strength (RSSI): "));
  Serial.print(WiFi.RSSI());
  Serial.println(F(" dBm"));

  // current IP
  Serial.print(F("IP Address: "));
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);
  Serial.println(macToString(WiFi.macAddress(mac)));

  Serial.println(F("==============================\n"));
}


void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {   // not connected
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(SECRET_SSID);
    
    while (WiFi.status() != WL_CONNECTED) {
#ifdef IP
      WiFi.config(ip, dns, gateway, subnet);
#endif
      WiFi.begin(SECRET_SSID, SECRET_PASS);   // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(F("."));
      
      delay(5000);
    }
    Serial.println(F("\nConnected"));

    printWifiStatus();

    server.begin();
  }
}


void connectToMQTTBroker() {
  if (!mqttClient.connected()) {   // not connected
    Serial.println("Connected to MQTT broker!"); 
    Serial.print(F("\nConnecting to MQTT broker..."));
    //mqttClient.setWill(MQTT_TOPIC_LWT, "Unexpected disconnetion");
    while (!mqttClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nConnected!"));
    // connected to broker, subscribe topics
    mqttClient.subscribe(MQTT_TOPIC_CONTROL);
    mqttClient.subscribe(MQTT_TOPIC_SERVO);
    Serial.println(F("\nSubscribed to control topic!"));
  }
}


void mqttMessageReceived(String &topic, String &payload) {
  // this function handles a message from the MQTT broker
  Serial.println("Incoming MQTT message: " + topic + " - " + payload);

  if (topic == MQTT_TOPIC_CONTROL) {
    // deserialize the JSON object
    StaticJsonDocument<128> doc;
    deserializeJson(doc, payload);
    const char *hightemp = doc["temperature"];
    const char *lowlight = doc["light"];  
      
    digitalWrite(LED_RED, HIGH);
    Serial.println(F("LED ON"));
    digitalWrite(BUZZER_GROVE, HIGH);
    Serial.println(F("BUZZER ON"));
    digitalWrite(LED_GREEN, LOW);
    Serial.println(F("GREEN LED OFF"));

  }  else {
    Serial.println(F("MQTT Topic not recognized, message skipped"));
  }
}



void listenForEthernetClients() {
  WiFiClient client = server.available();   // listen for incoming clients

 if (client) {                             // if you get a client,
    Serial.println(F("new client"));        // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.println("<HEAD>");
          
            client.println("<style> button { background-color:#cb3234; border: none; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; }"
              "a {color: white;} </style>");

            client.println("<TITLE>Home Automation</TITLE>");

            client.println("</HEAD>");

            // the content of the HTTP response follows the header:
            client.print("Click here to <button><a href=\"/H\">Red Led OFF</a></button><br>");
            client.print("Click here to <button><a href=\"/L\">Buzzer OFF</a></button><br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          }
          else {      // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.endsWith("GET /H")) {
          digitalWrite(LED_RED, LOW);
          digitalWrite(LED_GREEN, HIGH);
        }

        if (currentLine.endsWith("GET /L")) {
          digitalWrite(BUZZER_GROVE, LOW);                // GET /L turns the Button off
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println(F("client disonnected"));
  }
}

String macToString(byte macAdd[]) {
  String macAddressString = "";
  for (int i = 5; i >= 0; i--) {
    char hexCar[2];

    sprintf(hexCar, "%02X", macAdd[i]);

    macAddressString += String(hexCar);

    if (i > 0) {
      macAddressString += ":";
    }
  }
  return macAddressString;
}