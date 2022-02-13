#include <stdio.h>
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <credentials.h>
#include <OTA.h>
#include <SSD1306Wire.h>
#include <InfluxDbClient.h>

SSD1306Wire display(0x3c, D2, D1); // D2=SDK  D1=SCK  As per labeling on NodeMCU

//#define DEBUG
#define LED_0 0
#define DHTPIN 2
#define URI "/humid"
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define DHTTYPE DHT11 // DHT22 (AM2302), AM2321

#define INFLUXDB_URL "http://192.168.63.28:8086"
#define INFLUXDB_TOKEN "tokedid"
#define INFLUXDB_ORG "org"
#define INFLUXDB_BUCKET "sensors"

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point sensor("room_sensor");
unsigned long previousMillisWiFi = 0;

DHT dht(DHTPIN, DHTTYPE);

unsigned long time_now = 0;
int period = 1 * 60 * 1000; // 120000;

int deviceCount = 3;

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
    deviceCount = 3;

    try
    {
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
        }
        else
        {
            th[0] = th[0] / numberRead;
            th[1] = th[1] / numberRead;
            // Compute heat index in Celcius
            float hic = dht.computeHeatIndex(th[0], th[1], false);

            sensor.clearFields();

            sensor.addField("temperature", th[0]);
            sensor.addField("humidity", th[1]);
            sensor.addField("heat-index", hic);
            Serial.print("Writing: ");
            Serial.println(client.pointToLineProtocol(sensor));

            if (!client.writePoint(sensor))
            {
                Serial.print("InfluxDB write failed: ");
                Serial.println(client.getLastErrorMessage());
            }
        }
    }
    catch (const std::exception &e)
    {
    }

    displayStatus("Spaarkamer");
    reading = false;
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

    Serial.println("HTTP REST Server Started");
    displayStatus("HTTP REST Server Started", true);

    delay(1000);

    if (client.validateConnection())
    {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
    }
    else
    {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(client.getLastErrorMessage());

        get_temps();
    }
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

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillisWiFi >= 15 * 1000) // send data every 15 sec
    {
        get_temps();
        previousMillisWiFi = currentMillis;
        Serial.print(F("Wifi is still connected with IP: "));
        Serial.println(WiFi.localIP()); // inform user about his IP address
    }
}