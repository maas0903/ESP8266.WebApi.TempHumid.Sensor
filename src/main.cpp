#include <stdio.h>
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <credentials.h>
#include <InfluxDbClient.h>

//#define DEBUG
#define LED_0 0
#define DHTPIN 2
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define DHTTYPE DHT22 // DHT11

#define INFLUXDB_URL "http://192.168.63.28:8086"
#define INFLUXDB_TOKEN "tokedid"
#define INFLUXDB_ORG "org"
#define INFLUXDB_BUCKET "sensors"

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point sensor("humid-test2");
unsigned long previousMillisWiFi = 0;

DHT dht(DHTPIN, DHTTYPE);

unsigned long time_now = 0;
int period = 1 * 60 * 1000; // 120000;

int deviceCount = 3;

int resets = 0;

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

float th[2];

boolean init_wifi()
{
    int retries = 0;

    Serial.println("Connecting to WiFi");

    WiFi.config(staticIP, gateway, subnet, dnsGoogle);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostName);
    WiFi.begin(ssid, password);

    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
    {
        retries++;
        delay(WIFI_RETRY_DELAY);
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
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
        return false;
    }
}

void resetInit()
{
    resets++;
    Serial.begin(115200);
    pinMode(DHTPIN, INPUT);
    delay(200);
    dht.begin();
    delay(200);

    if (!init_wifi())
    {
        ESP.restart();
    }

    delay(200);

    if (client.validateConnection())
    {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(client.getServerUrl());
    }
    else
    {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(client.getLastErrorMessage());
    }
}

void get_temps()
{
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
            delay(1000);
            humidity = dht.readHumidity();
            delay(1000);
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

            if (numberNotRead > 6)
            {
                Serial.println("Beginning DHT again");
                dht.begin();
                delay(1000);
                numberNotRead = 0;
            }

        } while (numberOfTries < 20);

#endif

        if (numberRead == 0)
        {
            Serial.println("Failed to read from DHT sensor!");
            deviceCount = 0;
        }

        if (deviceCount == 0)
        {
            Serial.print("No Content");
            resetInit();
        }
        else
        {
            th[0] = th[0] / numberRead;
            th[1] = th[1] / numberRead;

            float hic = dht.computeHeatIndex(th[0], th[1], false);

            sensor.clearFields();

            sensor.addField("temperature", th[0]);
            sensor.addField("humidity", th[1]);
            sensor.addField("heat-index", hic);
            sensor.addField("resets", resets);
            sensor.addField("ipaddress", WiFi.localIP().toString());
            sensor.addField("mac-address", WiFi.macAddress());
            Serial.print("writing: ");
            Serial.println(client.pointToLineProtocol(sensor));

            while (!client.canSendRequest())
            {
                Serial.println("client not ready");
            }

            if (!client.writePoint(sensor))
            {
                Serial.print("InfluxDB write failed: ");
                Serial.println(client.getLastErrorMessage());
                resetInit();
            }
            Serial.println("done");
        }
    }
    catch (const std::exception &e)
    {
    }
}

void setup(void)
{
    resetInit();
    get_temps();
}

void loop(void)
{
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillisWiFi >= 120 * 1000)
    {
        get_temps();
        previousMillisWiFi = currentMillis;
        Serial.print(F("Wifi is still connected with IP: "));
        Serial.println(WiFi.localIP());
    }
}