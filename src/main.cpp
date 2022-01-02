#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentials.h>

#include <Wire.h>
#include <SPI.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <WiFiUdp.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

//#define DEBUG
IPAddress staticIP(192, 168, 63, 65);
#define LED_0 0
#define DHTPIN 2
#define URI "/humid"
String hostName = "kelder1";
IPAddress gateway(192, 168, 63, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 63, 21);
IPAddress dnsGoogle(8, 8, 8, 8);

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50

#define DHTTYPE DHT22 //DHT11 (AM2302), AM2321

DHT dht(DHTPIN, DHTTYPE);

// Define NTP Client to get time
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 0;

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

void get_temps()
{
    BlinkNTimes(LED_0, 2, 500);
    StaticJsonDocument<1024> jsonObj;
    deviceCount = 3;

    try
    {
#ifdef DEBUG
        jsonObj["DEBUG"] = "******* true *******";
#else
        jsonObj["DEBUG"] = "false";
#endif
        jsonObj["Hostname"] = hostName;
        jsonObj["IpAddress"] = WiFi.localIP().toString();
        jsonObj["MacAddress"] = WiFi.macAddress();
        jsonObj["Gpio"] = 2;
        jsonObj["DeviceType"] = "Humidity";
        jsonObj["DeviceCount"] = deviceCount;

        float th[2];
#ifdef DEBUG
        h = 55;
        t = 22;
#else
        int numberOfTries = 0;
        int numberRead = 0;
        int numberNotRead = 0;
        float temperature = 0;
        float humidity = 0;
        do
        {
            temperature = dht.readTemperature();
            delay(250);
            humidity = dht.readHumidity();
            delay(250);
            numberOfTries++;

            if (isnan(temperature) || isnan(humidity))
            {
                numberNotRead++;
            }
            else
            {
                numberRead++;
                th[0] = th[0] + temperature;
                th[1] = th[1] + humidity;
            }

            Serial.print("Number of tries = ");
            Serial.println(numberOfTries);
            if (numberNotRead > 1)
            {
                dht.begin();
                delay(500);
                numberNotRead = 0;
            }

        } while (numberOfTries < 20);

#endif

        // Check if any reads failed and exit early (to try again).
        if (numberRead == 0)
        {
            Serial.println("Failed to read from DHT sensor!");
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
            th[0] = th[0] / numberRead;
            th[1] = th[1] / numberRead;
            jsonObj["numberOfTries"] = numberOfTries;
            jsonObj["numberOfRead"] = numberRead;
            // Compute heat index in Celcius
            float hic = dht.computeHeatIndex(th[0], th[1], false);

            JsonArray sensors = jsonObj.createNestedArray("Sensors");
            JsonObject sensor1 = sensors.createNestedObject();
            sensor1["Id"] = "0";
            sensor1["ValueType"] = "Humidity";
            sensor1["Value"] = (String)th[1];
            JsonObject sensor2 = sensors.createNestedObject();
            sensor2["Id"] = "0";
            sensor2["ValueType"] = "Temperature";
            sensor2["Value"] = (String)th[0];
            JsonObject sensor3 = sensors.createNestedObject();
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
        // std::cerr << e.what() << '\n';
    }

    String jSONmessageBuffer;
    serializeJsonPretty(jsonObj, jSONmessageBuffer);

    http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
    http_rest_server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");

    http_rest_server.send(200, "application/json", jSONmessageBuffer);

    Serial.println(jSONmessageBuffer);
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []()
                        { http_rest_server.send(200, "text/html",
                                                "Welcome to the ESP8266 REST Web Server: " + hostName); });
    http_rest_server.on(URI, HTTP_GET, get_temps);
}

void init_wifi()
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

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
        BlinkNTimes(LED_0, 3, 500);
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }
}

void resetInit()
{
    pinMode(DHTPIN, INPUT);
    delay(200);
    dht.begin();
    delay(200);

    init_wifi();
    delay(200);

    config_rest_server_routing();
    delay(200);
    http_rest_server.begin();

    Serial.println("HTTP REST Server Started");
}

void setup(void)
{
    Serial.begin(115200);
    resetInit();
}

void loop(void)
{
    http_rest_server.handleClient();
}