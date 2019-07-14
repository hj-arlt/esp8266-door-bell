/*
 *   PIN description of ESP8266 and Sonoff SV (ESP8266)
 *
     GPIO_KEY1,        // GPIO00 Button
     GPIO_USER,        // GPIO01 Serial RXD and Optional sensor
     0,                // GPIO02 gong, sensor
     GPIO_USER,        // GPIO03 Serial TXD and Optional sensor
     GPIO_USER,        // GPIO04 Optional sensor
     GPIO_USER,        // GPIO05 Optional sensor
     0, 0, 0, 0, 0, 0, // Flash connection
     GPIO_REL1,        // GPIO12 Red Led and Relay (0 = Off, 1 = On)
     GPIO_LED1_INV,    // GPIO13 Green Led (0 = On, 1 = Off)
     GPIO_USER,        // GPIO14 Optional sensor
     0, 0,
     GPIO_ADC0         // ADC0 Analog input
 */
#include <ESP8266WiFi.h>
/* pubsubclient ver.2.6.0 only */
#include <PubSubClient.h>
#include <OneWire.h> 

//#define KLINGEL_GONG

/* change it with your ssid-password */
const char* ssid = "WLANmy";
const char* password = "1234567890";
const char* mqtt_server = "192.168.178.10";
#ifdef KLINGEL_GONG
const char* clientId = "ESP8266-gong";
#else
const char* clientId = "ESP8266-klingel";
#endif

/* create an instance of PubSubClient client */
WiFiClient espClient;
PubSubClient client(espClient);


#ifdef KLINGEL_GONG
/* Klingel Gong on sonoff SV (esp8266) */
  /* Relais, LED rot */
  #define LED_PIN      12
#else
/* Klingel TASTER_PIN on ESP32-01 (esp8266) */
  #define TASTER_PIN   0
  /* LED rot */
  #define LED_PIN      12
  /* Sensor */
  #define DS18_PIN     2
#endif

/* topics */
#ifdef KLINGEL_GONG
  #define GONG_TOPIC  "smarthome/klingel/taste"     /* gong 1=on, 0=off */
  #define LED_TOPIC   "smarthome/klingel/led"       /* send ack 0=on, 1=off */
#else
  #define LED_TOPIC   "smarthome/klingel/led"       /* rec ack 0=on, 1=off */
  #define TAST_TOPIC  "smarthome/klingel/taste"     /* 1=pressed, 0=released */
  #define TEMP_TOPIC  "smarthome/klingel/temp"
  OneWire ds(DS18_PIN);
#endif

byte i;
long lastMsg = 0;
long lastGong = 0;
char msg[20];
byte addr[8];
byte data[12];
byte type_s = 0;
byte present = 0;
float celsius, fahrenheit;

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received: ");
  Serial.println(topic);

  Serial.print("payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
#ifdef KLINGEL_GONG
  /* 1 gedrückt */
  if ((char)payload[0] == '1') {
  /* we got '0' -> relais on */
    Serial.println("MQTT gong on");
    digitalWrite(LED_PIN, HIGH);
    lastGong = 1;
  } else {
    /* we got '1' -> gong off */
    Serial.println("MQTT relais off");
    digitalWrite(LED_PIN, LOW); 
    lastGong = 0;
  }
#else
  if ((char)payload[0] == '0') {
  /* we got '0' -> on */
    Serial.println("MQTT led on");
    digitalWrite(LED_PIN, LOW);
  } else {
    /* we got '1' -> off */
    Serial.println("MQTT led off");
    digitalWrite(LED_PIN, HIGH); 
  }
#endif
}

void mqttconnect() {
  /* Loop until reconnected */
  while (!client.connected()) {
    Serial.println("MQTT connecting");
    Serial.println(mqtt_server);
    /* connect now */
    if (client.connect(clientId)) {
      Serial.println("connected");
      /* subscribe topic with default QoS 0*/
#ifdef KLINGEL_GONG
      client.subscribe(GONG_TOPIC);
#else
      client.subscribe(LED_TOPIC);
#endif
    } else {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      /* Wait 5 seconds before retrying */
      delay(5000);
    }
  }
}

#ifndef KLINGEL_GONG

void sensor_init() {
  Serial.println();
  if (!ds.search(addr) ) {
    Serial.println("No Sensor found");
    type_s = 0;
  }
  else {
    Serial.print("ROM =");
    for (i=0; i<8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);      
    }
    Serial.println();
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("Onewire CRC is not valid!");
    }
    // the first ROM byte indicates which chip
    switch (addr[0]) {
      case 0x10:
        Serial.println("Chip = DS18S20");  // or old DS1820
        type_s = 2;
        break;
      case 0x28:
        Serial.println("Chip = DS18B20");
        type_s = 1;
        break;
      case 0x22:
        Serial.println("Chip = DS1822");
        type_s = 1;
        break;
      default:
        Serial.println("Device is not a DS18x20 family device.");
    }
  } 
}

void sensor_temp() {
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);   // start conversion, with parasite power on at the end
    
  delay(1000);         // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  //Serial.print("  Data = ");
  //Serial.print(present, HEX);
  //Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    //Serial.print(data[i], HEX);
    //Serial.print(" ");
  }
  //Serial.print(" CRC=");
  //Serial.print(OneWire::crc8(data, 8), HEX);
  //Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s == 2) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit"); 
}
#endif

void setup() {
  Serial.begin(115200);
  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  /* set led as output to control led on-off */
  pinMode(LED_PIN, OUTPUT);
#ifdef KLINGEL_GONG
  /* set relais off */
  digitalWrite(LED_PIN, LOW); 
  lastGong = 0;
#else
  /* set led off */
  digitalWrite(LED_PIN, HIGH); 
  pinMode(TASTER_PIN, INPUT); // Port as input
  
  sensor_init();  
#endif

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  /* configure the MQTT server with IPaddress and port */
  client.setServer(mqtt_server, 1883);
  /* this receivedCallback function will be invoked 
  when client received subscribed topic */
  client.setCallback(receivedCallback);
}

void loop() {
  /* if client was disconnected then try to reconnect again */
  if (!client.connected()) {
    mqttconnect();
  }
  /* this function will listen for incomming 
  subscribed topic-process-invoke receivedCallback */
  client.loop();

#ifdef KLINGEL_GONG
  if (lastGong) {
    /* ack led on */
    client.publish(LED_TOPIC, "0");
    delay(3000);   
    /* ack led off */
    client.publish(LED_TOPIC, "1");
    lastGong = 0;    
  }
#else
  int taste = digitalRead(TASTER_PIN);
  if (digitalRead(TASTER_PIN)== LOW) 
  {
    Serial.println("Taste gedrückt 3 sec..");
    client.publish(TAST_TOPIC, "1");
    delay(3000);   
    Serial.println("Taste losgelassen");
    client.publish(TAST_TOPIC, "0");
  }
#endif

  /* we send alive every 3 secs
  we count until 3 secs reached to avoid blocking program if using delay()*/
  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    
#ifdef KLINGEL_GONG

#else
    if (type_s) {
      sensor_temp();
      if (!isnan(celsius)) {
        snprintf (msg, 20, "%.1f", celsius);
        /* publish the message */
        client.publish(TEMP_TOPIC, msg);
      }
    }
#endif
    //snprintf (msg, 20, "alive..");
    /* publish the message */
    //client.publish(TAST_TOPIC, msg);
  }
}
