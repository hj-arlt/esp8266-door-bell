/*
 *   Sonoff SV (ESP8266)
     GPIO_KEY1,        // GPIO00 Button
     GPIO_USER,        // GPIO01 Serial RXD and Optional sensor
     0,                // GPIO02
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
#include <PubSubClient.h>

#define KLINGEL_GONG

/* change it with your ssid-password */
const char* ssid = "WLAN XYZ";
const char* password = "12345678";
/* mqtt server on a raspberry NAS */
const char* mqtt_server = "192.168.178.28";
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
  #define PIN      12
#else
/* Klingel Taster on ESP32-01 (esp8266) */
  #define TASTER   0
  /* LED */
  #define PIN      2
#endif

/* topics */
#ifdef KLINGEL_GONG
#define GONG_TOPIC  "smarthome/klingel/taste"     /* gong 1=on, 0=off */
#define LED_TOPIC   "smarthome/klingel/led"       /* send ack 0=on, 1=off */
#else
#define LED_TOPIC   "smarthome/klingel/led"       /* rec ack 0=on, 1=off */
#define TAST_TOPIC  "smarthome/klingel/taste"     /* 1=pressed, 0=released */
#endif

long lastMsg = 0;
long lastGong = 0;
char msg[20];

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
    digitalWrite(PIN, HIGH);
    lastGong = 1;
  } else {
    /* we got '1' -> gong off */
    Serial.println("MQTT relais off");
    digitalWrite(PIN, LOW); 
    lastGong = 0;
  }
#else
  if ((char)payload[0] == '0') {
  /* we got '0' -> on */
    Serial.println("MQTT led on");
    digitalWrite(PIN, LOW);
  } else {
    /* we got '1' -> off */
    Serial.println("MQTT led off");
    digitalWrite(PIN, HIGH); 
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
  pinMode(PIN, OUTPUT);
#ifdef KLINGEL_GONG
  /* set relais off */
  digitalWrite(PIN, LOW); 
  lastGong = 0;
#else
  /* set led off */
  digitalWrite(PIN, HIGH); 
  pinMode(TASTER, INPUT); // Port as input
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
  int taste = digitalRead(TASTER);
  if (digitalRead(TASTER)== LOW) 
  {
    Serial.println();
    Serial.printf("Taste gedrückt");
    client.publish(TAST_TOPIC, "1");
    delay(3000);   
    client.publish(TAST_TOPIC, "0");
  }
#endif

  /* we send alive every 3 secs
  we count until 3 secs reached to avoid blocking program if using delay()*/
  long now = millis();
  if (now - lastMsg > 3000) {
    lastMsg = now;
      //snprintf (msg, 20, "alive..");
      /* publish the message */
      //client.publish(TAST_TOPIC, msg);
  }
}
