# esp8266-thermometer
1-wire esp8266-01 thermometer REST API with 15B20 chip.
Reads first found Device ID, must be on GPIO2.
Example:

http://192.168.xx.xx/temps

gives
{
  "id": "28ff7557801604ec",
  "gpio": 2,
  "UTC time": "18:09:04",
  "temperature": "24.50",
  "ipaddress": "192.168.xx.xx"
}
