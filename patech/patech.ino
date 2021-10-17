#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include <DHT.h>
#include <FirebaseJson.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#if defined(DHT22)
#define DHTTYPE DHT22
#elif defined(DHT11)
#define DHTTYPE DHT11
#endif

//Enable WiFiClientSecure library after include the library and before include the FirebaseJson.
#define FBJS_ENABLE_WIFI_CLIENT_SECURE
#define WIFI_SSID "Redmi@99"
#define WIFI_PASSWORD "xxxxxxxx@123"
#define WIFI_SSID_1 "Patech_WiFi"
#define WIFI_PASSWORD_1 "patech2345"
#define FIREBASE_SERVER_MAIN "patech-xl-imdp-default-rtdb.asia-southeast1.firebasedatabase.app"
#define DATABASE_SERCRET "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define API_KEY "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define USER_EMAIL "abdan.idza2345@gmail.com"
#define USER_PASSWORD "xxxxxxxxxxxxxxxxxxxx"
#define BAUDRATE 115200
#define DHTPIN 27
#define RDSPIN 34
#define SUCCESS_LED 2

#define uS_TO_S_FACTOR 1000000UL /* Conversion factor for micro seconds to seconds */ 
#define TIME_TO_SLEEP 3700/* Time ESP32 will go to sleep (in seconds) */ 
#define BOT_TOKEN "xxxxxxxxxx:xxxxxxxxxxxxxxxxxxxxxxxx"

WiFiClientSecure secured_client, sslClient;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WiFiUDP ntpUDP;
DHT dht22(DHTPIN, DHTTYPE);
NTPClient timeClient(ntpUDP, "pool.ntp.org");
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
unsigned long ms = 0;
RTC_DATA_ATTR byte last_rain = 0; 
RTC_DATA_ATTR float averageItem = 0;
RTC_DATA_ATTR float temp = 0;
RTC_DATA_ATTR byte duration = 0;
byte tries = 0;
bool send_status;

void setup()
{
    Serial.begin(BAUDRATE);
    DHTSetup();
    RDSSetup();
    setupWiFi();
    retrieveTime();
    setupCurrentTime();
    WiFi.disconnect();
    startDeepSleep();
}

void loop(){}

void RDSSetup(){
    pinMode(RDSPIN, INPUT);
}

void sendToFirebase(String currentTime, String date_today, String hour){
    float hum = dht22.readHumidity();
    float temp = dht22.readTemperature();
    float temp_f = dht22.readTemperature(true);
    int rdSensor = analogRead(RDSPIN);
    float rain_duration = processRainDuration(rdSensor);
    int leaf_wetness = processLeafWetness(temp,duration);
    String last_update = hour.substring(0,5);
    int hour_now = last_update.substring(0,2).toInt();
    if(hour_now == 7){
       currentTime = showCurrentTime(); 
       date_today = dateToday();
       hour = hourToday();
       String last_update = hour.substring(0,5);
       int hour_now = last_update.substring(0,2).toInt();
    }

    if (millis() - ms > 15000 || ms == 0)
    {
        ms = millis();

        FirebaseJson json;
        FirebaseJsonData result;
    
        sslClient.setInsecure();
        secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
        
        json.set("/current_time", currentTime);
        json.set("/humidity", hum);
        json.set("/rain_dur", rain_duration);
        json.set("/temp_c", temp);
        json.set("/temp_f", temp_f);
        json.set("/date", date_today);
        json.set("/hour", hour_now);
        json.set("/last_update", last_update);
        json.set("/average_temp", leaf_wetness);
        
        Serial.print(F("Connecting to server..."));
        tries = 0;
        while(tries != 3){
            if (sslClient.connect("patech-xl-imdp-default-rtdb.asia-southeast1.firebasedatabase.app", 443)){
                Serial.println(F(" ok"));
                Serial.println(F("Send POST request..."));
                sslClient.print("POST /data/station_utara.json HTTP/1.1\n");
                sslClient.print("Host: patech-xl-imdp-default-rtdb.asia-southeast1.firebasedatabase.app\n");
                sslClient.print("Content-Type: application/json\n");
                sslClient.print("Content-Length: ");
                sslClient.print(json.serializedBufferLength());
                sslClient.print("\n\n");
                json.toString(sslClient);

                Serial.print("Read response...");

                //Automatically parsing for response (w or w/o header) with chunk encoding supported.
                if (json.readFrom(sslClient))
                {
                    Serial.println();
                    json.toString(Serial, true);
                    Serial.println("\n\nComplete");
                    tries = 3;
                }
                else{
                    serverIndicatorLED();
                    ++tries;
                    Serial.print("failed connect to server, retrying...,");
                    Serial.print(" attempt ");
                    Serial.print(tries);
                    Serial.println(" of 3");
                    bot_send_error_status();
                    sslClient.stop();
                    // espRestart();
                }
            }
            else{
                serverIndicatorLED();
                ++tries;
                Serial.print("failed connect to server, retrying...,");
                Serial.print(" attempt ");
                Serial.print(tries);
                Serial.println(" of 3");
                bot_send_error_status();
                sslClient.stop();
                // espRestart();
            }
        sslClient.stop();
        }     
    }
}

void espRestart(){
    esp_sleep_enable_timer_wakeup(1 * 1000000);
    delay(500);
    Serial.flush();
    esp_deep_sleep_start();
}

byte processRainDuration(int analog_value){
    if(analog_value <= 4095 && analog_value >= 4000){
        duration = 0;
    }
    else{
        duration++;
    }
    return duration;
}

float processLeafWetness(float temp_c, byte duration){
    if(duration != 0){
      temp += temp_c;
      last_rain = duration;
      ++averageItem;
    }
    else{
      if(last_rain != 0){
        last_rain = 0;
        return temp / averageItem;  
      }else{
        return 0;
      }
    }
}

void setupWiFi(){
    int i = 0; 
    WiFi.begin(WIFI_SSID_1, WIFI_PASSWORD_1);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(1000);
        if(i >= 19){
            espRestart();
        }
        i++;
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
}

String showCurrentTime(){
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  String formattedTime = timeClient.getFormattedTime();
  String weekDay = weekDays[timeClient.getDay()];
  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  String currentMonthName = months[currentMonth-1];
  int currentYear = ptm->tm_year+1900;
  String date = (weekDay + ", " + monthDay + " " + currentMonthName + " " + currentYear + ", " + formattedTime);//21:21:19, Tuesday, 28 September 2021
  return date;
}

String dateToday(){
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  String formattedTime = timeClient.getFormattedTime();
  String weekDay = weekDays[timeClient.getDay()];
  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentYear = ptm->tm_year+1900;
  String date = String(monthDay);
  return date;
}

String hourToday(){
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  String formattedTime = timeClient.getFormattedTime();
  String weekDay = weekDays[timeClient.getDay()];
  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int currentYear = ptm->tm_year+1900;
  return formattedTime;
}

void setupCurrentTime(){
  timeClient.begin();
  timeClient.setTimeOffset(25200);
}

void retrieveTime(){
  Serial.print(F("Retrieving time: "));
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600){
     Serial.print(".");
     delay(1000);
     now = time(nullptr);
  }
  Serial.println(F("Finish Retrieve Time"));
}

void DHTSetup(){
    dht22.begin();
}

void startDeepSleep(){
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    delay(500);
    Serial.flush();
    esp_deep_sleep_start();
}

void setupLED(){
    pinMode(SUCCESS_LED, OUTPUT);   
    digitalWrite(SUCCESS_LED, LOW);
}

void serverIndicatorLED(){
   for (byte i = 0; i < 10; i++)
    {
       digitalWrite(SUCCESS_LED, HIGH);
       delay(500);
       digitalWrite(SUCCESS_LED, LOW);
       delay(500);
    }    
}

void bot_send_error_status(){
    String myChatId = "1139051246";
    String message = "Patech cannot connect to server!\nRetrying from device...";
    bot.sendMessage(myChatId, message);
}
