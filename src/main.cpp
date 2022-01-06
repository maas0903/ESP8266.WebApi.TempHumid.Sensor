#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <Wire.h>
#include <SPI.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <WiFiUdp.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <credentials.h>
#include <OTA.h>

#include <SSD1306Wire.h>

SSD1306Wire display(0x3c, D2, D1); // D2=SDK  D1=SCK  As per labeling on NodeMCU

//#define DEBUG
#define LED_0 0
#define DHTPIN 2
#define URI "/humid"

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50

#define DHTTYPE DHT11 // DHT22 (AM2302), AM2321

DHT dht(DHTPIN, DHTTYPE);

unsigned long time_now = 0;
int period = 1 * 60 * 1000; // 120000;

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

void displaySetup()
{
    display.init();

    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
}

float th[2];

void displayStatus(String status, boolean isStatus = false)
{
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, status);
    if (!isStatus)
    {
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_16);
        display.drawString(0, 14, "Lug T: " + String(th[0]) + " °C");
        display.drawString(0, 30, "Lugvog: " + String(th[1]) + " %");
        display.drawString(0, 46, "*******************");
    }
    display.display();
}

boolean reading = false;

void get_temps()
{
    reading = true;
    displayStatus("Spaarkamer - Lees ....", true);
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

        // float th[2];
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
            displayStatus("Spaarkamer - Lees .... " + String(numberRead), true);

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

    displayStatus("Spaarkamer");
    reading = false;
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []()
                        { http_rest_server.send(200, "text/html",
                                                "Welcome to the ESP8266 REST Web Server: " + hostName); });
    http_rest_server.on(URI, HTTP_GET, get_temps);
}

boolean init_wifi()
{
    int retries = 0;

    Serial.println("Connecting to WiFi");
    displayStatus("Spaarkamer", true);

    WiFi.config(staticIP, gateway, subnet, dnsGoogle);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostName);
    WiFi.begin(ssid, password);

    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
    {
        retries++;
        delay(WIFI_RETRY_DELAY);
        displayStatus("Spaarkamer " + String(retries), true);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
        BlinkNTimes(LED_0, 3, 500);
        return true;
    }
    else
    {
        displayStatus("Spaarkamer NO WIFI", true);
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
        return false;
    }
}

void resetInit()
{
    displaySetup();

    pinMode(DHTPIN, INPUT);
    delay(200);
    dht.begin();
    delay(200);

    if (!init_wifi())
    {
        displayStatus("No WIFI 2", true);
        ESP.restart;
    }

    delay(200);

    setupOTA("Spaarkamer", ssid, password);

    config_rest_server_routing();
    delay(200);
    http_rest_server.begin();

    Serial.println("HTTP REST Server Started");
    displayStatus("HTTP REST Server Started", true);

    delay(1000);

    get_temps();
}

void setup(void)
{
    Serial.begin(115200);
    resetInit();
}

void loop(void)
{
    // if (!reading)
    ArduinoOTA.handle();

    delay(200);

    if (millis() > time_now + period)
    {
        if (!reading)
        {
            get_temps();
        }
        time_now = millis();
    }

    http_rest_server.handleClient();
}