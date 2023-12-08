#include <Servo.h>
#include <WiFi101.h>
#include "secrets.h"
#include "rgb_lcd.h"

#include <MQTT.h>
#include <ArduinoJson.h>

//MYSQL
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>

// WiFi cfg
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
#ifdef IP
IPAddress ip(IP);
IPAddress subnet(SUBNET);
IPAddress dns(DNS);
IPAddress gateway(GATEWAY);
#endif
WiFiClient client;

// MySQL server cfg
char mysql_user[] = MYSQL_USER;
char mysql_password[] = MYSQL_PASS;
IPAddress server_addr(MYSQL_IP);
MySQL_Connection conn((Client *)&client);


char query[128];
char INSERT_DATA[] = "INSERT INTO `RiccardoFerrante`.`wifi_data_simple_1` (`ssid`, `rssi`,`led_status`) VALUES ('%s',%d,%d)";

char query1[129];
char INSERT_TEMPLIGHT[] = "INSERT INTO `RiccardoFerrante`.`TempLightSensor_1` (`temp`,`light`,`temp_status`,`light_status`) VALUES (%d,%d,%d,%d)";


#define LED_ONBOARD LED_BUILTIN
#define RSSI_THRESHOLD -60
#define PHOTORESISTOR A1
#define PHOTORESISTOR_THRESHOLD 500
#define LED LED_BUILTIN
#define TEMP A0
#define B 4275
#define R0 100000

//MQTT
#define MQTT_BUFFER_SIZE 128                              // the maximum size for packets being published and received
MQTTClient mqttClient(MQTT_BUFFER_SIZE);                  // handles the MQTT communication protocol
WiFiClient networkClient;                                 // handles the network connection to the MQTT broker
#define MQTT_TOPIC_CONTROL "ferrante_pappagallo/control"  // topic to control
#define MQTT_TOPIC_SERVO "ferrante_pappagallo/servo/status"   // topic to control
String MQTT_TOPIC_TEMP = "ferrante_pappagallo/temperature/status";
String MQTT_TOPIC_LIGHT = "ferrante_pappagallo/light/status";


byte mac[6];

int pos = 0;

static unsigned long TempInterval = 1000;
static unsigned long LightInterval = 1000;
static unsigned long ServoInterval = 100;
static unsigned long dbInterval = 2000;


static unsigned long lastTimeTemp = millis();
static unsigned long lastTimeLight = millis();
static unsigned long lastTimeServo = millis();
static unsigned long lastTime = millis();


Servo myservo;
rgb_lcd lcd;


void setup() {
  // pinmode sensori
  pinMode(TEMP, INPUT);
  pinMode(PHOTORESISTOR, INPUT);
  pinMode(LED_ONBOARD, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  myservo.attach(0);

  digitalWrite(LED_ONBOARD, HIGH);
  digitalWrite(LED_BUILTIN, LOW);

  lcd.begin(16, 2);  // 16 cols, 2 rows
  lcd.setRGB(240, 248, 20);
  lcd.print("Good morning!");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  //setup MQTT
  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);  // setup communication with MQTT broker PROVAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
  mqttClient.onMessage(mqttMessageReceived);             // callback on message received from MQTT broker

  Serial.begin(115200);
  Serial.println(F("\n\nSetup completed.\n\n"));
}

void loop() {

  TempLightWiFi();

  connectToMQTTBroker();
  mqttClient.loop();

}

//FUNZIONI

//Temperatura e Luce

void TempLightWiFi() {
  bool static led_status = HIGH;
  bool static temp_status = HIGH;
  bool static light_status = HIGH;

  int a = analogRead(TEMP);
  float R = 1023.0 / ((float)a) - 1.0;
  R = R0 * R;
  float temperature = 1.0 / (log(R / 100000.0) / B + 1 / 298.15);
  temperature = temperature - 273.15;

  static unsigned int lightSensorValue;
  lightSensorValue = analogRead(PHOTORESISTOR);

  if (millis() - lastTime >= dbInterval) {
    Serial.print(F("Temperature (°C): "));
    Serial.println(temperature);

    Serial.print(F("Light: "));
    Serial.println(lightSensorValue);

    lcd.clear();
    lcd.setRGB(240, 248, 20);  // clear text
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print(" ");
    lcd.print((char)223);
    lcd.print("C");

    delay(1000);


    if ((millis () - lastTimeTemp >= TempInterval) && (temperature >= 27) && (temp_status)) {
      temp_status = HIGH;
      Serial.print(F("HIGH TEMPERATURE"));
      lcd.setRGB(255, 8, 0);
      lcd.setCursor(0, 1);
      lcd.print("HIGH TEMP!");

      delay(1000);

      Serial.print(F("Red Led: LED ON"));
      Serial.println(F("Buzzer: BUZZER ON"));  
      // publish new MQTT (as a JSON object)
      const int capacity = JSON_OBJECT_SIZE(2);
      StaticJsonDocument<capacity> doc;
      doc["temperature"] = (MQTT_TOPIC_TEMP, temperature);
      doc["macAddress"] = (MQTT_TOPIC_TEMP, "F8:F0:05:EA:64:2F");
      char buffer[128];
      size_t n = serializeJson(doc, buffer);
      Serial.print(F("JSON message: "));
      Serial.println(buffer);
      mqttClient.publish(MQTT_TOPIC_CONTROL, buffer, n);
    } else if ((temperature <= 27) && (!temp_status)) {  
      temp_status = LOW;                                 

      Serial.print(F("Red Led: LED OFF"));
      Serial.println(F("Buzzer: BUZZER OFF"));
      // publish new MQTT (as a JSON object)
      const int capacity = JSON_OBJECT_SIZE(2);
      StaticJsonDocument<capacity> doc;
      doc["temperauture"] = (MQTT_TOPIC_TEMP, temperature);
      doc["macAddress"] = (MQTT_TOPIC_TEMP, "F8:F0:05:EA:64:2F");
      char buffer[128];
      size_t n = serializeJson(doc, buffer);
      Serial.print(F("JSON message: "));
      Serial.println(buffer);
      mqttClient.publish(MQTT_TOPIC_CONTROL, buffer, n);
    }

      lastTimeTemp = millis ();

    lcd.clear();
    lcd.setRGB(240, 248, 20);  // clear text
    lcd.print("Light: ");
    lcd.print(lightSensorValue);
    lcd.print(" lx");

    if ((millis () - lastTimeLight >= LightInterval) && (lightSensorValue <= 500) && (light_status)) {
      light_status = HIGH;
      lcd.setRGB(255, 8, 0);
      lcd.setCursor(0, 1);
      lcd.print("LOW LIGHT!");

      delay(1000);

      Serial.print(F("Red Led: LED ON"));
      Serial.println(F("Buzzer: BUZZER OFF"));
      // publish new MQTT (as a JSON object)
      const int capacity = JSON_OBJECT_SIZE(2);
      StaticJsonDocument<capacity> doc;
      doc["light"] = (MQTT_TOPIC_LIGHT, lightSensorValue);
      doc["macAddress"] = (MQTT_TOPIC_LIGHT, "F8:F0:05:EA:64:2F");
      char buffer[128];
      size_t n = serializeJson(doc, buffer);
      Serial.print(F("JSON message: "));
      Serial.println(buffer);
      mqttClient.publish(MQTT_TOPIC_CONTROL, buffer, n);
    } else if ((lightSensorValue >= 500) && (!light_status)) {
      light_status = LOW;
      // publish new MQTT (as a JSON object)
      const int capacity = JSON_OBJECT_SIZE(2);
      StaticJsonDocument<capacity> doc;
      doc["light"] = (MQTT_TOPIC_LIGHT, lightSensorValue);
      doc["macAddress"] = (MQTT_TOPIC_LIGHT, "F8:F0:05:EA:64:2F");
      char buffer[128];
      size_t n = serializeJson(doc, buffer);
      Serial.print(F("JSON message: "));
      Serial.println(buffer);
      mqttClient.publish(MQTT_TOPIC_CONTROL, buffer, n);
    }

      lastTimeLight = millis ();

  lastTime = millis();
  }

  long rssi = connectToWiFi();  // WiFi connect if not established and if connected get wifi signal strength

  if ((rssi > RSSI_THRESHOLD) && (led_status)) {  // if wifi signal strength is high then keep led on
    led_status = LOW;
    digitalWrite(LED_ONBOARD, led_status);
  } else if ((rssi <= RSSI_THRESHOLD) && (!led_status)) {  // if wifi signal strength is high then keep led off
    led_status = HIGH;
    digitalWrite(LED_ONBOARD, led_status);
  }

  WriteMultiToDB(ssid, (int)rssi, led_status, (int)temperature, (int)lightSensorValue, temp_status, light_status);
}


//Stampa del wifi
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

// Stampa lcd
//Connessione al wifi
long connectToWiFi() {
  long rssi_strength;
  // connect to WiFi (if not already connected)
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("Connecting to SSID: "));
    Serial.println(ssid);

#ifdef IP
    WiFi.config(ip, dns, gateway, subnet);  // by default network is configured using DHCP
#endif

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      delay(250);
    }

    Serial.println(F("\nConnected!"));
    rssi_strength = WiFi.RSSI();  // get wifi signal strength
    printWifiStatus();
  } else {
    rssi_strength = WiFi.RSSI();  // get wifi signal strength
  }

  return rssi_strength;
}

void moveServo() {
  for (pos = 0; pos <= 180; pos += 3) {
    myservo.write(pos);
    delay(15);
  }
  for (pos = 180; pos >= 0; pos -= 2) {
    myservo.write(pos);
    delay(15);
  }
}


int WriteMultiToDB(char field1[], int field2, int field3, int field4, int field5, int field6, int field7) {

  // connect to MySQL
  if (!conn.connected()) {
    conn.close();
    Serial.println(F("Connecting to MySQL..."));
    if (conn.connect(server_addr, 3306, mysql_user, mysql_password)) {
      Serial.println(F("MySQL connection established."));
    } else {
      Serial.println(F("MySQL connection failed."));
      return -1;
    }
  }

  sprintf(query, INSERT_DATA, field1, field2, field3);
  queryExecution(query);

  Serial.println(F("Wi-Fi, data recorded on MySQL"));

  sprintf(query1, INSERT_TEMPLIGHT, field4, field5, field6, field7);
  queryExecution(query1);
  Serial.println(F("TempLight, data recorded on MySQL"));
}

void queryExecution(char query[]) {
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  Serial.println(query);
  cur_mem->execute(query);
  delete cur_mem;
}

void connectToMQTTBroker() {
  if (!mqttClient.connected()) {  // not connected
    Serial.print(F("\nConnecting to MQTT broker..."));
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


  if (topic == MQTT_TOPIC_SERVO) {
    Serial.println("Attivazione servo da F8:F0:05:ED:F1:F1 ...");

    if (payload == "rotate", "F8:F0:05:ED:F1:F1") {  // received payload is: "rotate"
      Serial.println("Servo in movimento");
      moveServo();
      
    } else {
      Serial.println(F("Device not recognized"));
    }
  } else {
    Serial.println(F("MQTT Topic not recognized, message skipped"));
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

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print(F("0"));
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(F(":"));
    }
  }
  Serial.println();
}