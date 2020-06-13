#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentials.h>

#include <Wire.h>
#include <DHT.h>
#include <SPI.h>

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

//#define DEBUG
IPAddress staticIP(192, 168, 63, 133);
#define URI "/humid"
IPAddress gateway(192, 168, 63, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 63, 21);
IPAddress dnsGoogle(8, 8, 8, 8);
String hostName = "esp01";

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50

#define LED_0 0
#define DHTPIN 2
#define DHTTYPE DHT22 // DHT 22 (AM2302), AM2321

DHT dht(DHTPIN, DHTTYPE);

// Define NTP Client to get time
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 0;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

int deviceCount = 3;

ESP8266WebServer http_rest_server(HTTP_REST_PORT);

void BlinkNTimes(int pin, int blinks, unsigned long millies)
{
    digitalWrite(pin, LOW);
    for (int i = 0; i < blinks; i++)
    {
        digitalWrite(pin, HIGH);
        delay(millies);
        digitalWrite(pin, LOW);
        delay(millies);
    }
}

int init_wifi()
{
    int retries = 0;

    Serial.println("Connecting to WiFi");

    WiFi.config(staticIP, gateway, subnet, dns, dnsGoogle);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostName);
    WiFi.begin(ssid, password);

    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
    {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    Serial.println();
    BlinkNTimes(LED_0, 3, 500);
    return WiFi.status();
}

String GetCurrentTime()
{
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    char buff[32];

    sprintf(buff, "%02d-%02d-%02d %02d:%02d:%02d",
            year(epochTime),
            month(epochTime),
            day(epochTime),
            hour(epochTime),
            minute(epochTime),
            second(epochTime));
    String currentTime = buff;
    return currentTime;
}

void get_temps()
{
    BlinkNTimes(LED_0, 2, 500);
    StaticJsonBuffer<600> jsonBuffer;
    JsonObject &jsonObj = jsonBuffer.createObject();
    char JSONmessageBuffer[600];
    deviceCount = 3;

    try
    {
#ifdef DEBUG
        jsonObj["DEBUG"] = "******* true *******";
#else
        jsonObj["DEBUG"] = "false";
#endif
        jsonObj["UtcTime"] = GetCurrentTime();
        jsonObj["Hostname"] = hostName;
        jsonObj["IpAddress"] = WiFi.localIP().toString();
        jsonObj["MacAddress"] = WiFi.macAddress();
        jsonObj["Gpio"] = 2;
        jsonObj["DeviceType"] = "Humidity";
        jsonObj["DeviceCount"] = deviceCount;

        float h;
        float t;
#ifdef DEBUG
        h = 55;
        t = 22;
#else
        h = dht.readHumidity();
        t = dht.readTemperature();
#endif

        String mess;
        // Check if any reads failed and exit early (to try again).
        if (isnan(h) || isnan(t))
        {
            mess = "Failed to read from DHT sensor!";
            Serial.println(mess);
            deviceCount = 0;
        }

        if (deviceCount == 0)
        {
            Serial.print("No Content");
            String sHostName(WiFi.hostname());
            jsonObj["Error"] = "No devices found on " + sHostName + " (" + WiFi.macAddress() + ")";
        }
        else
        {
            // Compute heat index in Celcius
            float hic = dht.computeHeatIndex(t, h, false);
 
            JsonArray &sensors = jsonObj.createNestedArray("Sensors");
            JsonObject &sensor1 = sensors.createNestedObject();
            sensor1["Id"] = "0";
            sensor1["ValueType"] = "Humidity";
            sensor1["Value"] = (String)h;
            JsonObject &sensor2 = sensors.createNestedObject();
            sensor2["Id"] = "0";
            sensor2["ValueType"] = "Temperature";
            sensor2["Value"] = (String)t;
            JsonObject &sensor3 = sensors.createNestedObject();
            sensor3["Id"] = "0";
            sensor3["ValueType"] = "HeatIndex";
            sensor3["Value"] = (String)hic;
        }
    }
    catch (const std::exception &e)
    {
        // String exception = e.what();
        // jsonObj["Exception"] = exception.substring(0, 99);
        jsonObj["Exception"] = " ";
        //std::cerr << e.what() << '\n';
    }

    jsonObj.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));

    http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
    http_rest_server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");

    http_rest_server.send(200, "application/json", JSONmessageBuffer);

    Serial.println(JSONmessageBuffer);
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []() {
        http_rest_server.send(200, "text/html",
                              "Welcome to the ESP8266 REST Web Server: " + GetCurrentTime());
    });
    http_rest_server.on(URI, HTTP_GET, get_temps);
}

void setup(void)
{
    Serial.begin(115200);

    pinMode(DHTPIN, INPUT);
    dht.begin();

    if (init_wifi() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }

    timeClient.begin();

    config_rest_server_routing();

    http_rest_server.begin();
    Serial.println("HTTP REST Server Started");
}

void loop(void)
{
    http_rest_server.handleClient();
}