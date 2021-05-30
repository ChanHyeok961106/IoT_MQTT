#include <WiFi.h>
#include <WiFiClient.h> // wifi클라
#include <WiFiServer.h> // wifi서버
#include <PubSubClient.h> // mqtt pub, sub사용위해
#ifndef ESP32
#include <SoftwareSerial.h>
#endif
#include <PMserial.h> // PM센서(미세먼지)
#include <DHT11.h>  //아두이노 온습도센서 DHT11

#define WFSSID      "iptime_chan" // wifi를 이용해서 와파 이름
#define PASSWORD    "chanwoo@3485" // 비밀번호

#define mqtt_server "192.168.0.15" // 브로커 ip 라즈베리파이
#define mqtt_topic  "test" // 테스트용 topic

#define TRIG 13 //TRIG 핀 설정 (초음파 보내는 핀)
#define ECHO 12 //ECHO 핀 설정 (초음파 받는 핀)
constexpr auto PMS_RX = 14; // 미세먼지 센서 RX 핀 설정
constexpr auto PMS_TX = 27; // 미세먼지 센서 TX 핀 설정
SerialPM pms(PMS5003, PMS_RX, PMS_TX); // PMSx003, RX, TX
DHT11 dht11(26); // 온습도센서 핀 설정

const char* mqtt_message = "test";
const char* topic_distance = "dis"; // 보안거리  topic
const char* topic_temperature = "temp"; // 온도 topic
const char* topic_humidity = "hum"; // 습도 topic
const char* topic_PM25 = "pmm"; // 초미세먼지 topic
const char* topic_PM10 = "pm"; // 미세먼지 topic
const char* topic_safe = "warn"; // 미세먼지 topic

const char* j = 0;
long duration, distance; // 초음파 거리
// distance = cm로 변환할 거리
// 테스트용 측정 duration 시간
float temp, humi; // 온도 습도

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int val = 0;

void setup() {
    pinMode(TRIG, OUTPUT); // 초음파 TRIG
    pinMode(ECHO, INPUT);  // 초음파 ECHO
    
    Serial.begin(115200);
    Serial.println();

    for(uint8_t t = 3; t > 0; t--) {
        Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }

    WiFi.begin(WFSSID, PASSWORD);
    Serial.print("Connecting to WiFi");
    while(WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    client.setServer(mqtt_server, 1883); // 브로커 포트 :1883
    client.setCallback(callback);

    // PM센서 파트 + 시리얼포트 확인용
    Serial.println(F("PMS sensor on SWSerial"));
    Serial.print(F("  RX:"));
    Serial.println(PMS_RX);
    Serial.print(F("  TX:"));
    Serial.println(PMS_TX);
    pms.init(); // pm센서
}

void loop() {
    char buffer[10]; // mqtt msg 보내기 위한 임시 buffer
    client.loop();
    if(!client.connected()) {
        reconnect();
    }
    if(WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WFSSID, PASSWORD);
        Serial.print("Reconnecting WiFi");
        while(WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
        }
        Serial.println();
        Serial.println("WiFi Reconnected");
    }

    //초음파 센서로 보안거리 topic:dis
    digitalWrite(TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG, LOW);
    duration = pulseIn (ECHO, HIGH); //물체에 반사되어돌아온 초음파의 시간을 변수에 저장합니다.
    distance = duration * 17 / 1000;
    itoa(distance,buffer,10);
    Serial.print("distacne : ");
    Serial.println(buffer);
    strcat(buffer, "cm");
    client.publish(topic_distance, buffer); // dis topic 거리값(cm) pub
    Serial.println("MQTT : sent distance (dis)");
    if(distance >= 100) { // warn topic 거리1m이상 안전 msg pub
      client.publish(topic_safe, "안전");
    } else { // warn topic 거리1m이하 위험 msg pub
      client.publish(topic_safe, "근접위험");
    }


    //pm센서
    pms.read();
    if (pms) {
      Serial.print(F("PM2.5 : "));
      Serial.print(pms.pm25);
      Serial.println("ug/m3");
      itoa(pms.pm25,buffer,10);
      strcat(buffer, "ug/m3");
      client.publish(topic_PM25, buffer);// pmm topic으로 초미세먼지값 pub

      Serial.print(F("PM10 : "));
      Serial.print(pms.pm10);
      Serial.println("ug/m3");
      itoa(pms.pm10,buffer,10);
      strcat(buffer, "ug/m3");
      client.publish(topic_PM10, buffer); // pm topic으로 미세먼지값 pub
      
      Serial.println("MQTT : sent pm value (pmm, pm)");
    }

    int result = dht11.read(humi, temp); // 센서값이 정상이면 0리턴
    if (result == 0) {
      Serial.print("temperature:"); 
      Serial.print((int)temp); //온도값
      itoa((int)temp,buffer,10);
      strcat(buffer, "°C");
      client.publish(topic_temperature, buffer); // temp topic으로 온도값 pub
      
      Serial.print(" humidity:");
      Serial.println((int)humi); //습도값
      itoa((int)humi,buffer,10);
      strcat(buffer, "%");
      client.publish(topic_humidity, buffer); // hum topic으로 습도값 pub
    }
    delay(3000);
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");


    String msgText = "=> ";
    
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      msgText += (char)payload[i];
    }
    Serial.println();
}


void reconnect() {
    while(!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if(client.connect("ESP32Client")) {
            Serial.println("connected");
            Serial.println("DONE connecting!");
        } else {
            //Serial.print("failed, rc = ");
            //Serial.print(client.state());
            //Serial.println(" automatically trying again in 1 second");
            delay(1000);
        }
    }
}
