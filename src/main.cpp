#include "Version.h"
#include <Arduino.h>
#include <settings.h>
#include <string.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <NTPtimeESP.h>

#if !defined(ESP8266)
  #error This code is designed to run on ESP8266 only
#endif

#ifdef USECERT
  X509List cert(CA_CERT);
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

#ifdef USETLS
  WiFiClientSecure espClient;
#else
  WiFiClient espClient;
#endif
PubSubClient client(espClient);

void delay_OTA(unsigned long msec = 0)
{
  if (msec == 0)
  {
    client.loop();
    ArduinoOTA.handle();
    yield();    
    return;
  }

  unsigned long stopm = millis() + msec;
  while (stopm > millis())
  {
    client.loop();
    ArduinoOTA.handle();
    yield();    
  }
}

enum State { lazy, selectr, selnohit, selhit, cmdsend, endwait };
enum Cmd { none = 0, up = 1, stop = 2, down = 3 };
Cmd cmdqueue[5] = { none };
int cur_RolNo = -1;
unsigned long last_event = millis();
Cmd cur_Cmd = none;

char Datum[30];

void update_time() {
  static NTPtime NTPch(ntp_server);
  static strDateTime dateTime;

  int i = 0;
  do
  {
    dateTime = NTPch.getNTPtime(1.0,1);
    if (!dateTime.valid) delay(100);  
    #ifdef DEBUG
      Serial.print(".");
    #endif
  } while (!dateTime.valid and i++ < 50);  

  if(!dateTime.valid){
    #ifdef DEBUG
      Serial.println("Failed to obtain time");
    #endif
    return;
  }
  sprintf(Datum,"%02d.%02d.%04d %02d:%02d",dateTime.day, dateTime.month, dateTime.year, dateTime.hour, dateTime.minute);

  #ifdef DEBUG
    Serial.printf("NTP: %d %s:%02d\n", dateTime.dayofWeek-1, Datum, dateTime.second);
  #endif
}

void tspublish(const char *topic, const char *payload) {
  String pl;
  static unsigned long seq = 0;
  pl = Datum + String("-") + (seq++) + String(":") + payload;
  client.publish(topic,pl.c_str(),true);
}

void callback(char* topic, byte* payload, unsigned int length) {
  char spayload[3];
  int RolNo;

  int b = strlen(mqtt_stopic);
  if (strncmp(topic,mqtt_stopic,b) == 0) return;
  b = strlen(mqtt_etopic);
  if (strncmp(topic,mqtt_etopic,b) == 0) return;
  b = strlen(mqtt_topic)-2;
  if (strncmp(topic,mqtt_topic,b)) {
    #ifdef DEBUG
      Serial.println("Ungültiges Topic!");
    #endif
    tspublish(mqtt_etopic,"Ungültiges Topic!");
    return;
  }
  int o = strlen(topic)-b;
  if (o == 0) RolNo = 0;
  else if (o == 2 && topic[b+o-2] == '/') {
    RolNo = topic[b+o-1] - '0';
    if (RolNo < 0 || RolNo > 4) RolNo = -1;
  }
  else RolNo = -1;

  if (RolNo < 0) {
    #ifdef DEBUG
      Serial.println("Ungültiger Rollladen!");
    #endif
    tspublish(mqtt_etopic,"Ungültiger Rollladen!");
    return;
  }
  if (length == 0) return;
  if (length != 2) {
    #ifdef DEBUG
      Serial.println("Ungültige Befehlslänge!");
    #endif
    tspublish(mqtt_etopic,"Ungültige Befehlslänge!");
    return;
  }

  int j = 0;
  for (unsigned int i = 0; i < length; i++) {
    if (payload[i] > 32) spayload[j++] = toUpperCase(payload[i]);
  }
  spayload[j] = '\0';
  #ifdef DEBUG
    Serial.printf("Nachricht für %d empfangen: %s\n",RolNo,spayload);
  #endif
  if (strcmp(spayload,"UP") == 0) cmdqueue[RolNo] = up;
  else if (strcmp(spayload,"DN") == 0) cmdqueue[RolNo] = down;
  else if (strcmp(spayload,"ST") == 0) cmdqueue[RolNo] = stop;
  else {
    #ifdef DEBUG
      Serial.println("Kenne Befehl nicht!");
    #endif
    tspublish(mqtt_etopic,"Kenne Befehl nicht!");
    return;
  }
  #ifdef DEBUG
    Serial.printf("Befehl erkannt: %d\n",cmdqueue[RolNo]);
  #endif
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect(HOSTNAME, mqtt_user, mqtt_pass)) {
      #ifdef DEBUG
        Serial.println("connected.");
      #endif
  
      if (client.subscribe(mqtt_topic)) {
        #ifdef DEBUG
          Serial.print("subscribed to ");
          Serial.println(mqtt_topic);
        #endif
      }
      else {
        #ifdef DEBUG
          Serial.println("subscribe failed!");
        #endif
      }
    }
    else {
      #ifdef DEBUG
        Serial.print("failed, status code = ");
        Serial.println(client.state());
        #ifdef USETLS
          char errt[50];
          Serial.print("LastSSLError = ");
          espClient.getLastSSLError(errt, 50);
          Serial.println(errt);
        #endif
      #endif
      delay_OTA(2000);
    }
  }
  update_time();
  tspublish(mqtt_stopic,"New Connected by " VersionString);
}

void setup() {
  #ifdef DEBUG
    Serial.begin(115200);
    Serial.println("Booting");
    Serial.printf("Project version v%s, built %s\n",VERSION,BUILD_TIMESTAMP);
  #endif

  pinMode(PIN_OUT1, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_OUT1, HIGH);
  pinMode(PIN_OUT2, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_OUT2, HIGH);
  pinMode(PIN_OUT3, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_OUT3, HIGH);
  pinMode(PIN_OUT4, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_OUT4, HIGH);
  // All input need Pullup
  pinMode(PIN_IN1, INPUT_PULLUP);
  pinMode(PIN_IN2, INPUT); // no internal Pullup at GPIO16 - external 10k needed
  pinMode(PIN_IN3, INPUT); // GPIO03 has allways a Pullup
  pinMode(PIN_IN4, INPUT); // GPIO04 has allways a Pullup

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    #ifdef DEBUG
      Serial.println("Connection Failed! Rebooting...");
    #endif
    delay(5000);
    ESP.restart();
  }

  #ifdef USETLS
    #ifdef USECERT
      espClient.setTrustAnchors(&cert);
    #else
      espClient.setInsecure();
    #endif
  #endif
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(UPTIMESEC>DNTIMESEC?UPTIMESEC:DNTIMESEC);

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(HOSTNAME);

  // No authentication by default
  ArduinoOTA.setPassword(OTA_PWD);

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    if (ArduinoOTA.getCommand() == U_FLASH) {
      #ifdef DEBUG
        Serial.println("Start updating sketch");
      #endif
    } else { // U_FS
      #ifdef DEBUG
        Serial.println("Start updating filesystem");
      #endif
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
  });
  ArduinoOTA.onEnd([]() {
    #ifdef DEBUG
      Serial.println("\nEnd");
    #endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    #ifdef DEBUG
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    #endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef DEBUG
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
    }
    #endif
  });
  ArduinoOTA.begin();

  #ifdef DEBUG
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  #endif
}

void loop() {
  if (!client.loop()) {
    reconnect();
  } 
  else {
    #ifdef DEBUG
      static unsigned long pause = millis();
      if (millis() - pause > 10000) {
        Serial.println("still connected...");
        pause = millis();
      }
    #endif
  }

  #ifndef dryrun
    static State s = lazy;
    static String ss;

    static bool in[4] = {false,false,false,false};
    bool changed __attribute__ ((unused)) = false;

    if (in[0] == digitalRead(PIN_IN1)) { in[0] = !in[0]; changed = true; } 
    if (in[1] == digitalRead(PIN_IN2)) { in[1] = !in[1]; changed = true; } 
    if (in[2] == digitalRead(PIN_IN3)) { in[2] = !in[2]; changed = true; } 
    if (in[3] == digitalRead(PIN_IN4)) { in[3] = !in[3]; changed = true; } 
    delay_OTA();

    #ifdef DEBUG
      if (changed) {
        Serial.print("LED-Status: ");
        for (int i = 0; i < 4; i++) { Serial.print(in[i]); if (i<3) Serial.print(","); else Serial.println(); }
      }
    #endif
  
    //RolNo: 0 = ALL, 1-4 = Rollo 1-4
    switch(s) {
      case lazy:
        for (int i = 0; i < 5; i++) {
          if (cmdqueue[i] != none) {
            // activate select
            digitalWrite(PIN_OUT4, LOW);
            // next_event = now + 100 ms
            last_event = millis();
            #ifdef DEBUG
              Serial.printf("Signal 4 low at %lu\n",last_event);
            #endif
            cur_Cmd = cmdqueue[i];
            cur_RolNo = i;
            cmdqueue[i] = none;
            s = selectr;
            break;
          } 
        }
        if (s == lazy && UPDATEPERIOD < millis() - last_event) {
          last_event = millis();
          if (ss.length() > 0) {
            ss = String(VersionString) + String(" - Last Cmds done") + ss;
            tspublish(mqtt_stopic,ss.c_str());
            ss = "";
          }
          update_time();
        }  
        break;
      case selectr:
        if (millis() - last_event > PRESSDURATIONSEL) {
          // deactivate select
          digitalWrite(PIN_OUT4, HIGH);
          last_event = millis();
          #ifdef DEBUG
            Serial.printf("Signal 4 high at %lu\n",last_event);
          #endif
          int curSel = 0;
          int cnt = 0;
          for (int i=0; i<4; i++) if (in[i]) {curSel = i+1; cnt++;}
          if ((cnt == 4 && cur_RolNo == 0) || (cnt == 1 && curSel == cur_RolNo)) s = selhit;
          else s = selnohit;
        }
        break;
      case selnohit:
        if (millis() - last_event > RELEASEDURATION) {
          // activate select
          digitalWrite(PIN_OUT4, LOW);
          // next_event = now + 100 ms
          last_event = millis();
          #ifdef DEBUG
            Serial.printf("Signal 4 low at %lu\n",last_event);
          #endif
          s = selectr;
        }
        break;
      case selhit:
        if (millis() - last_event > RELEASEDURATION) {
          last_event = millis();
          switch(cur_Cmd) {
            case up:
              digitalWrite(PIN_OUT1, LOW);           
              #ifdef DEBUG
                Serial.printf("Signal 1 low at %lu\n",last_event);
              #endif
              s = cmdsend;
              break;
            case stop:
              digitalWrite(PIN_OUT2, LOW);           
              #ifdef DEBUG
                Serial.printf("Signal 2 low at %lu\n",last_event);
              #endif
              s = cmdsend;
              break;
            case down:
              digitalWrite(PIN_OUT3, LOW);           
              #ifdef DEBUG
                Serial.printf("Signal 3 low at %lu\n",last_event);
              #endif
              s = cmdsend;
              break;
            default:
              s = lazy;
          }
        }
        break;
      case cmdsend:
        if (millis() - last_event > PRESSDURATIONACT) {
          last_event = millis();
          // deactivate CMD
          switch(cur_Cmd) {
            case up:
              digitalWrite(PIN_OUT1, HIGH);           
              #ifdef DEBUG
                Serial.printf("Signal 1 high at %lu\n",last_event);
              #endif
              s = endwait;
              break;
            case stop:
              digitalWrite(PIN_OUT2, HIGH);           
              #ifdef DEBUG
                Serial.printf("Signal 2 high at %lu\n",last_event);
              #endif
              s = endwait;
              break;
            case down:
              digitalWrite(PIN_OUT3, HIGH);          
              #ifdef DEBUG
                Serial.printf("Signal 3 high at %lu\n",last_event);
              #endif
              s = endwait;
              break;
            default:
              s = lazy;
          }
          ss += String(" : ") + String(cur_RolNo) + String("/") + String(cur_Cmd == up ? "UP" : cur_Cmd == stop ? "ST" :  cur_Cmd == down ? "DN" : "__");
          cur_Cmd = none;
        }
        break;
      case endwait:
        if (millis() - last_event > RELEASEDURATION) s = lazy;
    }
  #endif

  delay_OTA(1);
}
