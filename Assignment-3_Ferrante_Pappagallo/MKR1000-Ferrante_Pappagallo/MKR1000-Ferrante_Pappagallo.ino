#include <SPI.h>
#include <WiFi101.h>
#include "secrets.h"
#include "rgb_lcd.h"


#include <ArduinoJson.h>
#include <MQTT.h>


#define LED_GREEN 0  //connect LED to digital pin0
#define LED_RED 3    //connect LED to digital pin3
#define BUZZER_GROVE 4
#define BUTTON 1
#define BUTTON_DEBOUNCE_DELAY 20  // button debounce time in ms
#define LED_ONBOARD LED_BUILTIN
#define LED LED_BUILTIN

// MQTT data
#define MQTT_BUFFER_SIZE 128                          // the maximum size for packets being published and received
MQTTClient mqttClient(MQTT_BUFFER_SIZE);              // handles the MQTT communication protocol
WiFiClient networkClient;                             // handles the network connection to the MQTT broker
#define MQTT_TOPIC_METEO "ferrante_pappagallo/meteo"  // topic to control led & buzzer


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
WiFiServer server(80);


//byte MessaggeReceived
StaticJsonDocument<384> pDoc;
int show = 0;

String country;
String location;
String weather;
String temperature;
String humidity;
bool display_bool = false;

static byte led_state = LOW;         // current led state, start LOW=OFF
static byte buzzer_state = LOW;      // current buzzer state, start LOW=OFF
static byte led_green_state = HIGH;  // current led green state, start HIGH=ON

static unsigned long lastDisplay = millis();

// Mac Address variables
byte mac[6];
String macAddress;

rgb_lcd lcd;

void setup() {

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_GROVE, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_ONBOARD, OUTPUT);

  digitalWrite(LED_ONBOARD, HIGH);

  digitalWrite(LED_BUILTIN, LOW);

  digitalWrite(LED_RED, LOW);

  digitalWrite(LED_GREEN, HIGH);

  digitalWrite(BUZZER_GROVE, LOW);

  lcd.begin(16, 2);  // 16 cols, 2 rows
  lcd.setRGB(240, 248, 20);
  lcd.print("Good morning!");
  lcd.setCursor(0, 1);
  lcd.print("Please wait");

  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);  // setup communication with MQTT broker
  mqttClient.onMessage(mqttMessageReceived);             // callback on message received from MQTT broker

  Serial.begin(115200);
  Serial.println(F("\n\nSetup completed.\n\n"));
}


void loop() {

  bool static led_status = HIGH;

  connectToWiFi();  // WiFi connect if not established and if connected get wifi signal strength

  connectToMQTTBroker();  // connect to MQTT broker (if not already connected)
  mqttClient.loop();      // MQTT client loop
  listenForEthernetClients();

  Display();

  alarmCondition();
}

void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {  // not connected
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(SECRET_SSID);

    while (WiFi.status() != WL_CONNECTED) {
#ifdef IP
      WiFi.config(ip, dns, gateway, subnet);
#endif
      WiFi.begin(SECRET_SSID, SECRET_PASS);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(F("."));

      delay(5000);
    }
    Serial.println(F("\nConnected"));

    printWifiStatus();
    server.begin();
  }
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

void listenForEthernetClients() {
  WiFiClient client = server.available();  // listen for incoming clients

  if (client) {                       // if you get a client,
    Serial.println(F("new client"));  // print a message out the serial port
    String currentLine = "";          // make a String to hold incoming data from the client
    while (client.connected()) {      // loop while the client's connected
      if (client.available()) {       // if there's bytes to read from the client,
        char c = client.read();       // read a byte, then
        Serial.write(c);              // print it out the serial monitor
        if (c == '\n') {              // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.println("<HEAD>");

            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
                           "body{margin-top: 50px;} h1 {color: #444444; margin: 50px auto 30px;} h3 {color: #444444; margin-bottom: 50px;}"
                           "button {display: inline-block; background-color: #FF4433; border: none; color: white; padding: 15px 32px; text-decoration: none; font-size: 25px; margin: 25px auto; cursor: pointer; border-radius: 4px;}"
                           "a {color: white; text-decoration: none;} </style>");

            client.println("<TITLE>Flowerbed Alarm</TITLE>");



            client.println("</HEAD>");

            // the content of the HTTP response follows the header:
            client.print("<h1>Alarm System</h1>");
            client.print("Click here to <button><a href=\"/H\">Red Led OFF</a></button><br>");
            client.print("Click here to <button><a href=\"/L\">Buzzer OFF</a></button><br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {  // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was "GET /H" or "GET /L":
        if (currentLine.endsWith("GET /H")) {
          digitalWrite(LED_RED, LOW);
          digitalWrite(LED_GREEN, HIGH);
        }
        if (currentLine.endsWith("GET /L")) {
          digitalWrite(BUZZER_GROVE, LOW);  // GET /L turns the Button off
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println(F("client disonnected"));
  }
}

void connectToMQTTBroker() {
  if (!mqttClient.connected()) {  // not connected
    Serial.println("Connected to MQTT broker!");
    Serial.print(F("\nConnecting to MQTT broker..."));
    while (!mqttClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nConnected!"));
    // connected to broker, subscribe topics
    mqttClient.subscribe(MQTT_TOPIC_METEO);
    Serial.println(F("\nSubscribed to control topic!"));
  }
}

void mqttMessageReceived(String &topic, String &payload) {
  // this function handles a message from the MQTT broker
  Serial.println("Incoming MQTT message: " + topic + " - " + payload);

  if (topic == MQTT_TOPIC_METEO) {
    // deserialize the JSON object
    StaticJsonDocument<384> pDoc;
    deserializeJson(pDoc, payload);

    const char *country_char = pDoc["country"];
    country = String(country_char);

    const char *location_char = pDoc["location"];
    location = String(location_char);

    const char *weather_char = pDoc["weather"];
    weather = String(weather_char);

    const char *temperature_char = pDoc["temperature"];
    temperature = String(temperature_char);

    const char *humidity_char = pDoc["humidity"];
    humidity = String(humidity_char);

    display_bool = true;

    Serial.print(F("JSON message: "));

    Serial.print("Country: ");
    Serial.print(country);

    Serial.print("Location: ");
    Serial.print(location);

    Serial.print("Weather: ");
    Serial.print(weather);

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("°C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print("%");
  }
}

void Display() {

  static unsigned long current_Display = millis();

  if (display_bool) {
    if (show == 5) {
      show = 0;
    }

    if ((show == 0) && ((millis() - current_Display) > 2000)) {
      current_Display = millis();
      lcd.clear();
      lcd.setRGB(240, 248, 20);  // clear text
      lcd.print("Country: ");
      lcd.print(country);
      show += 1;
    }

    if ((show == 1) && ((millis() - current_Display) > 2000)) {
      current_Display = millis();
      lcd.clear();
      lcd.setRGB(240, 248, 20);  // clear text
      lcd.print("Location: ");
      lcd.print(location);
      show += 1;
    }

    if ((show == 2) && ((millis() - current_Display) > 2000)) {
      current_Display = millis();
      lcd.clear();
      lcd.setRGB(240, 248, 20);  // clear text
      lcd.print("Weather: ");
      lcd.print(weather);
      show += 1;
    }

    if ((show == 3) && ((millis() - current_Display) > 2000)) {
      current_Display = millis();
      if (temperature >= "30") {
        Serial.println("HIGH TEMPERATURE!");
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, HIGH);
        digitalWrite(BUZZER_GROVE, HIGH);
        lcd.clear();
        lcd.setRGB(255, 0, 0);
        lcd.print("HIGH TEMP!");
        lcd.setCursor(0, 1);
        lcd.print(temperature);
        lcd.print((char)223);
        lcd.print("C");
      } else {
        lcd.clear();
        lcd.setRGB(240, 248, 20);  // clear text
        lcd.print("Temp: ");
        lcd.print(temperature);
        lcd.print((char)223);
        lcd.print("C");
      }
      show += 1;
    }

    if ((show == 4) && ((millis() - current_Display) > 2000)) {
      current_Display = millis();
      if (humidity < "40") {
        Serial.println("DRY ENVIRONMENT!");
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, HIGH);
        digitalWrite(BUZZER_GROVE, HIGH);
        lcd.clear();
        lcd.setRGB(255, 0, 0);
        lcd.print("DRY ENVIRONMENT!");
        lcd.setCursor(0, 1);
        lcd.println(humidity);
        lcd.print("%");
      } else {
        lcd.clear();
        lcd.setRGB(240, 248, 20);  // clear text
        lcd.print("Humidity: ");
        lcd.print(humidity);
        lcd.print("%");
      }
      show += 1;
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

void alarmCondition() {

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
