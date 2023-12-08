// Use this file to store all of the private credentials and connection details

// Telegram bot configuration (data obtained form @BotFather, see https://core.telegram.org/bots#6-botfather)
#define BOT_NAME "rferrante"                      // name displayed in contact list
#define BOT_USERNAME "rferrante_bot"              // short bot id
#define BOT_TOKEN "6079064774:AAFXMLCxoOw9n2Ff2gHDg1YWQ4DhXdk52b4"      // authorization token

// WiFi configuration
#define SECRET_SSID "IoTLabThingsU14"                  // SSID
#define SECRET_PASS "L@b%I0T*Ui4!P@sS**0%Lessons!"              // WiFi password

// openweathermap.org configuration
#define WEATHER_API_KEY "b0778d6d37e43eaf759cafad021946e5"           // api key form https://home.openweathermap.org/api_keys
#define WEATHER_CITY "Milan"                     // city
#define WEATHER_COUNTRY "it"                     // ISO3166 country code 

// MQTT access
#define MQTT_BROKERIP "149.132.178.180"           // IP address of the machine running the MQTT broker
#define MQTT_CLIENTID "ESP8266"                 // client identifier
#define MQTT_USERNAME "RiccardoFerrante"            // mqtt user's name
#define MQTT_PASSWORD "iot881061"            // mqtt user's password

// MySQL access
#define MYSQL_IP {149, 132, 178, 180}              // IP address of the machine running MySQL
#define MYSQL_USER "RiccardoFerrante"                  // db user
#define MYSQL_PASS "iot881061"              // db user's password

// ONLY if static configuration is needed
/*
#define IP {192, 168, 1, 100}                    // IP address
#define SUBNET {255, 255, 255, 0}                // Subnet mask
#define DNS {149, 132, 2, 3}                     // DNS
#define GATEWAY {149, 132, 182, 1}               // Gateway
*/
