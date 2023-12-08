#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include "secrets.h"
#include <MQTT.h>
#include <Servo.h>
// MySQL libraries
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <UniversalTelegramBot.h>
// JSON library
#include <ArduinoJson.h>

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

// weather api (refer to https://openweathermap.org/current)
const char weather_server[] = "api.openweathermap.org";
const char weather_query[] = "GET /data/2.5/weather?q=%s,%s&units=metric&APPID=%s";

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lastTime;  // last time messages' scan has been done


//MQTT
#define MQTT_BUFFER_SIZE 256                          // the maximum size for packets being published and received
MQTTClient mqttClient(MQTT_BUFFER_SIZE);              // handles the MQTT communication protocol
WiFiClient networkClient;                             // handles the network connection to the MQTT broker
#define MQTT_TOPIC_METEO "ferrante_pappagallo/meteo"  // topic to control


#define RSSI_THRESHOLD -60  // WiFi signal strength threshold
#define LED_RED D1          // LED pins (D0 and D4 are already used by board LEDs)
#define LED_GREEN D2
#define LED_BLUE D3
#define SERVO_PIN D5  // PWM pin for servo control (keep in mind that D4 is already used by board LED)

// calibrate the servo
// as per the datasheet ~1ms=-90°, ~2ms=90°, 1.5ms=0°
// in reality seems that the values are: ~0.55ms=-90°, ~2.5ms=90°
#define SERVO_PWM_MIN 550   // minimum PWM pulse in microsecond
#define SERVO_PWM_MAX 2500  // maximum PWM pulse in microsecond

#define DELAY 20  // delay between servo position changes, millis

Servo servo;  // create servo object to control a servo

unsigned long sendValuesTime = 0;
const unsigned long MeteoInterval = 3000;
unsigned long lastTime = millis();
unsigned long deepSleepTime = millis();

char mysql_user[] = MYSQL_USER;
char mysql_password[] = MYSQL_PASS;
IPAddress server_addr(MYSQL_IP);
MySQL_Connection conn((Client *)&client);

//mysql_table
char query[128];
char INSERT_METEO[] = "INSERT INTO `RiccardoFerrante`.`meteo_data` (`weather`, `temp`, `humidity` ) VALUES ('%s', %d, %d)";


void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/on") {
      moveServo();  // turn the Step-Motor on (HIGH is the voltage level)
      bot.sendMessage(chat_id, "Motor is ON", "");
      Serial.println(F("LED: Red"));
    }

    else if (text == "/start") {
      String welcome = "Welcome to Arduino Telegram Bot, " + from_name + ".\n";
      welcome += "This is Flash Led Bot example.\n\n";
      welcome += "/on : to switch the Motor ON\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    } else {
      String not_rec = "Command not recognized \n";
      not_rec += "please digit /start to get list of commands \n";
      bot.sendMessage(chat_id, not_rec, "Markdown");
    }
  }
}


void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\nSetup completed.\n\n"));

  // set LED pins as outputs
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  // turn led off
  ledOff();

  // set servo to start position
  servo.attach(SERVO_PIN, SERVO_PWM_MIN, SERVO_PWM_MAX);
  servo.write(0);

  //setup MQTT
  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);

  connectToWiFi();
  connectToMQTTBroker();
}


void loop() {

  if (millis() - bot_lastTime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lastTime = millis();
  }

  if (millis() - lastTime >= MeteoInterval) {
    printCurrentWeather();
    lastTime = millis();
  }
  mqttClient.loop();
}

void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {      // not connected
    configTime(0, 0, "pool.ntp.org");       // get UTC time via NTP
    secured_client.setTrustAnchors(&cert);  // Add root certificate for api.telegram.org
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(SECRET_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SECRET_SSID, SECRET_PASS);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network

    while (WiFi.status() != WL_CONNECTED) {
#ifdef IP
      WiFi.config(ip, dns, gateway, subnet);
#endif
      Serial.print(F("."));
      delay(2000);
    }
    Serial.println(F("\nConnected"));
    printWifiStatus();
  }
}

void printWifiStatus() {
  Serial.println(F("\n=== WiFi connection status ==="));

  // SSID
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // signal strength
  long rssi = WiFi.RSSI();
  Serial.print(F("Signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
  Serial.println(WiFi.macAddress());

  // current IP
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // subnet mask
  Serial.print(F("Subnet mask: "));
  Serial.println(WiFi.subnetMask());

  // gateway
  Serial.print(F("Gateway IP: "));
  Serial.println(WiFi.gatewayIP());

  // DNS
  Serial.print(F("DNS IP: "));
  Serial.println(WiFi.dnsIP());

  Serial.println(F("==============================\n"));
}

void connectToMQTTBroker() {
  if (!mqttClient.connected()) {  // not connected
    Serial.print(F("\nConnecting to MQTT broker..."));
    while (!mqttClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
    }
    Serial.println(F("\nConnected!"));

    // connected to broker, subscribe topics
    mqttClient.subscribe(MQTT_TOPIC_METEO);
    Serial.println(F("\nSubscribed to control topic!"));
  }
}

void WriteMultiToDB(String field1[], int field2, int field3) {

  // connect to MySQL
  if (!conn.connected()) {
    conn.close();
    Serial.println(F("Connecting to MySQL..."));
    if (conn.connect(server_addr, 3306, mysql_user, mysql_password)) {
      Serial.println(F("MySQL connection established."));
    } else {
      Serial.println(F("MySQL connection failed."));
    }
  }
  sprintf(query, INSERT_METEO, field1, field2, field3);

  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  Serial.println(query);
  cur_mem->execute(query);
  delete cur_mem;

  Serial.println(F("Meteo, data recorded on MySQL"));
}

void printCurrentWeather() {
  if (millis() - lastTime >= 2000) {
    // Current weather api documentation at: https://openweathermap.org/current
    Serial.println(F("\n=== Current weather ==="));

    // call API for current weather
    if (client.connect(weather_server, 80)) {
      char request[100];
      sprintf(request, weather_query, WEATHER_CITY, WEATHER_COUNTRY, WEATHER_API_KEY);
      client.println(request);
      client.println(F("Host: api.openweathermap.org"));
      client.println(F("User-Agent: ArduinoWiFi/1.1"));
      client.println(F("Connection: close"));
      client.println();
    } else {
      Serial.println(F("Connection to api.openweathermap.org failed!\n"));
    }

    while (client.connected() && !client.available()) delay(1);  // wait for data
    String result;
    while (client.connected() || client.available()) {  // read data
      char c = client.read();
      result = result + c;
    }

    client.stop();           // end communication
    Serial.println(result);  // print JSON

    char jsonArray[result.length() + 1];
    result.toCharArray(jsonArray, sizeof(jsonArray));
    jsonArray[result.length() + 1] = '\0';
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, jsonArray);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }

    Serial.print(F("Location: "));
    Serial.println(doc["name"].as<String>());
    Serial.print(F("Country: "));
    Serial.println(doc["sys"]["country"].as<String>());
    Serial.print(F("Temperature (°C): "));
    Serial.println((float)doc["main"]["temp"]);
    Serial.print(F("Humidity (%): "));
    Serial.println((float)doc["main"]["humidity"]);
    Serial.print(F("Weather: "));
    Serial.println(doc["weather"][0]["main"].as<String>());
    Serial.print(F("Weather description: "));
    Serial.println(doc["weather"][0]["description"].as<String>());
    Serial.print(F("Pressure (hPa): "));
    Serial.println((float)doc["main"]["pressure"]);
    Serial.print(F("Sunrise (UNIX timestamp): "));
    Serial.println((float)doc["sys"]["sunrise"]);
    Serial.print(F("Sunset (UNIX timestamp): "));
    Serial.println((float)doc["sys"]["sunset"]);
    Serial.print(F("Temperature min. (°C): "));
    Serial.println((float)doc["main"]["temp_min"]);
    Serial.print(F("Temperature max. (°C): "));
    Serial.println((float)doc["main"]["temp_max"]);
    Serial.print(F("Wind speed (m/s): "));
    Serial.println((float)doc["wind"]["speed"]);
    Serial.print(F("Wind angle: "));
    Serial.println((float)doc["visibility"]);
    Serial.print(F("Visibility (m): "));
    Serial.println((float)doc["wind"]["deg"]);

    Serial.println(F("==============================\n"));


    if (millis() - sendValuesTime >= 20000) {
      Serial.println("Invio dei messaggi in corso...");
      const int capacity = JSON_OBJECT_SIZE(10);
      StaticJsonDocument<capacity> pDoc;
      String location = doc["name"].as<String>();
      String temperature = doc["main"]["temp"].as<String>();
      String country = doc["sys"]["country"].as<String>();
      String weather = doc["weather"][0]["main"].as<String>();
      String humidity = doc["main"]["humidity"].as<String>();

      pDoc["country"] = (country);
      pDoc["location"] = (location);
      pDoc["weather"] = (weather);
      pDoc["temperature"] = (temperature);
      pDoc["humidity"] = (humidity);

      char buffer[384];
      size_t n = serializeJson(pDoc, buffer);

      Serial.print(F("JSON message: "));

      Serial.println("Country: ");
      Serial.println(country);
      Serial.println("Location: ");
      Serial.println(location);
      Serial.println("Weather: ");
      Serial.println(weather);
      Serial.println("Temperature: ");
      Serial.println(temperature);
      Serial.println("Humidity (%): ");
      Serial.println(humidity);

      mqttClient.publish(MQTT_TOPIC_METEO, buffer, n);

      WriteMultiToDB(&weather, doc["main"]["temp"].as<int>(), doc["main"]["humidity"].as<int>());

      ledOff();
      Serial.println(F("LED: Green"));
      digitalWrite(LED_GREEN, HIGH);
      sendValuesTime = millis();
    }
    lastTime = millis();
  }
  if (millis() - deepSleepTime >= 30000) {
    ledOff();
    digitalWrite(LED_BLUE, HIGH);
    Serial.println("deep sleep for 15 seconds");
    ESP.deepSleep(15e6);
  }
}

void ledOff() {
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
}

void moveServo() {
  Serial.println(F("0°"));
  delay(DELAY * 10);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW);
  for (int pos = 0; pos <= 180; pos++) {  // from 0 degrees to 180 degrees
    servo.write(pos);                     // set position
    delay(DELAY);                         // waits for the servo to reach the position
  }

  Serial.println(F("180°"));
  delay(DELAY * 10);

  for (int pos = 180; pos >= 0; pos--) {  // from 180 degrees to 0 degrees
    servo.write(pos);
    delay(DELAY);
  }
  digitalWrite(LED_RED, LOW);
}