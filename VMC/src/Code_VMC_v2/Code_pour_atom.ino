#include <Arduino.h> 
#include <M5Atom.h>
#include <driver/rtc_io.h> // from ESP-IDF
#include <PubSubClient.h> //Librairie pour la gestion Mqtt
#include <WiFi.h>
#include <WiFiMulti.h>

// Option WiFi
const char *Name_Wifi = "devolo-5ac";
const char *Mdp_Wifi = "ACDCCTGKBZXULRUW";

// Parametres du code
#define SERIAL_ENABLE false   // Pour activer l'écran mettre true 
#define LCD_ENABLE false      // Pour activer le seriam mettre true
#define SEND_MQTT_ALARM true  // Pour désactier l'envoie de payload mettre false (jamais mettre false sinon le code ne fonctionne pas)
#define ENVOI_SUR_EVENT false // Pour envoyer la payload seulement si changement d'état
#define TIMEOUT_WIFI true    // Pour verifier la connexion au wifi

// Propriétés du WIFI
WiFiClient espClient;
WiFiMulti WiFiMulti;

// Propriétés Timer
#define Time_to_reco 300000000  // Temps pour retenter une connexion a internet si echouer 
#define Time_to_sleep 1800000000 // Temps avant le prochain reveille

// Option MQTT
#define PAYLOAD_BUFFER_SIZE  (60)
const char *name_device = "VMC03";
const char *Id_name = "Geoffrey";
const char *Mdp_Lname = "Lazzari";

// Propriétés MQTT
char formatted_payload[PAYLOAD_BUFFER_SIZE];
PubSubClient client(espClient);
RTC_DATA_ATTR int cpt_payload = 1;
RTC_DATA_ATTR const uint16_t port = 1883;
RTC_DATA_ATTR const char *host = "broker-dev.enovact.fr";

int battery = 0;
RTC_DATA_ATTR uint32_t g_wom_count = 0;

// Propriétés Valeur Accéléromètre
float X = 0;
float Y = 0;
float Z = 0;

int is_vmc_on = 0;
RTC_DATA_ATTR int save_vmc_value = 1;

void format_payload() {
    delay(500);
    battery = 4000;
    snprintf (formatted_payload, PAYLOAD_BUFFER_SIZE, "{\"d\":\"%s\",\"cnt\":%d,\"bat\":%d,\"b1\":%d}",name_device, cpt_payload, battery, is_vmc_on);
    cpt_payload++;
    return;
}

int connect_to_wifi() {
    int i = 0;
    WiFiMulti.addAP(Name_Wifi, Mdp_Wifi);
    // check if the wifi connection works
    while(WiFiMulti.run() != WL_CONNECTED) {
      i++;
      delay(100);
      if(i > 5) return (false);
    }
    return (0);
}

void send_payload_to_broker() {
    // connection to the broker
    client.setServer(host, port);
    client.connect(name_device, Id_name, Mdp_Lname);
    // send the payload
    client.publish("data/eqp", formatted_payload);
    delay(500);
    return;
}

void disconnect_all() {
    // Disconnect from the broker
    client.disconnect();
    // Disconnect from Wifi
    WiFi.disconnect();
    return;
}

int send_state() {
    format_payload();
    bool i = connect_to_wifi();
    if (i == false) return (false);
    send_payload_to_broker();
    disconnect_all();
    return (0);
}

void setup() {
    M5.begin(LCD_ENABLE,SEND_MQTT_ALARM,SERIAL_ENABLE);

    // disable all wakeup sources
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // enable waking up on RTC timer
    esp_sleep_enable_timer_wakeup(Time_to_sleep);
    
    M5.IMU.Init(); // basic init
    bool state_wifi = 0;
    float res = 0;
    int i = 0;
    while (i != 30) {
        delay(100); // att 0.1sec pour l'échantillonnage des valuers
        M5.IMU.getAccelData(&X,&Y,&Z);
        Y = (Y < 0) ?-Y : Y;
        res += Y;
        i++;
    }
    res /= i;
    // prend 3 second pour faire la moyenne
    
    is_vmc_on = (res > 0.030) ? 1 : 0;
    if (ENVOI_SUR_EVENT == true) {
        if (is_vmc_on != save_vmc_value) {
            state_wifi = send_state();
        }
        save_vmc_value = is_vmc_on;
    } else
        state_wifi = send_state();
    if (TIMEOUT_WIFI != state_wifi){
      esp_sleep_enable_timer_wakeup(Time_to_reco);
    }
    // Increment boot number it every reboot
    g_wom_count++;

    // Go to sleep now
    esp_deep_sleep_start();
}
void loop() {
    //test
}
