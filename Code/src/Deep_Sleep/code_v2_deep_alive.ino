#include <Arduino.h>
#include <M5StickC.h>
#include <driver/rtc_io.h> // from ESP-IDF
#include <PubSubClient.h> //Librairie pour la gestion Mqtt
#include <WiFi.h>
#include <WiFiMulti.h>

// Propriétés du WIFI
WiFiClient espClient;
WiFiMulti WiFiMulti;

// Propriétés Timer
#define Time_to_sleep 86400000000   /* Time ESP32 will go to sleep (in microseconds); correspond a 24h*/

// Propriétés MQTT
#define PAYLOAD_BUFFER_SIZE  (60)
char formatted_payload[PAYLOAD_BUFFER_SIZE];
PubSubClient client(espClient);
RTC_DATA_ATTR int cpt_payload = 1;
RTC_DATA_ATTR const uint16_t port = 1883;
RTC_DATA_ATTR const char *host = "broker-dev.enovact.fr";

int battery = 0;
RTC_DATA_ATTR uint32_t g_wom_count = 0;

void format_payload() {
  delay(500);
  battery = (int)(M5.Axp.GetBatVoltage()  * 1000);
  //mise en place de la payload a envoyer
  snprintf (formatted_payload, PAYLOAD_BUFFER_SIZE, "{\"d\":\"GK001\",\"cnt\":%d,\"bat\":%d,\"b1\":1}",cpt_payload,battery);
  cpt_payload++;
  return;
}
void format_alive_payload() {
  delay(500);
  battery = (int)(M5.Axp.GetBatVoltage()  * 1000);
  //mise en place de la payload a envoyer
  snprintf (formatted_payload, PAYLOAD_BUFFER_SIZE, "{\"d\":\"GK001\",\"cnt\":%d,\"bat\":%d}",cpt_payload,battery);
  cpt_payload++;
  return;
}

void send_payload_to_broker() {
 //connexion au broker
 client.setServer(host, port);
 client.connect("GK001", "Geoffrey", "Lazzari");
 //envoie de la payload
 client.publish("data/eqp", formatted_payload);
 delay(500);
 return;
}

void connect_to_wifi() {
  WiFiMulti.addAP("devolo-5ac", "ACDCCTGKBZXULRUW"); // Déplacé dans le setup. Vérifier que cela n’induit pas d’erreur.
  //vérifie si la connexion au wifi à marcher
  while(WiFiMulti.run() != WL_CONNECTED);
  return;
}

void disconnect_all() {
  //Déconnexion du broker
  client.disconnect();
  //Déconnexion du Wifi
  WiFi.disconnect();
  return;
}

void alive_alarm() {
  format_alive_payload();
  connect_to_wifi();  
  send_payload_to_broker();
  disconnect_all();
  return;
}

void send_alarm() {
  format_payload();
  connect_to_wifi();
  send_payload_to_broker();
  disconnect_all();
  return;
}

void setup() {
    // put your setup code here, to run once:
    M5.begin();
    M5.Axp.SetLDO2(false);

    // disable all wakeup sources
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    
    // enable waking up on pin 35 (from IMU)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, LOW); //1 = High, 0 = Low
    esp_sleep_enable_timer_wakeup(Time_to_sleep);

    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
   
    if (g_wom_count == 0) {
        M5.Mpu6886.Init(); // basic init
        M5.Mpu6886.enableWakeOnMotion(M5.Mpu6886.AFS_16G, 50);
    }
    switch(wakeup_reason) {
       case ESP_SLEEP_WAKEUP_EXT0 : send_alarm(); break;
       case ESP_SLEEP_WAKEUP_TIMER : alive_alarm(); break;
    }
   
    //Increment boot number it every reboot
    g_wom_count++;

    delay(5000 - millis());
    //Go to sleep now
    M5.Axp.SetSleep(); // conveniently turn off screen, etc.
    delay(100);
    esp_deep_sleep_start();
}

void loop() {
}
