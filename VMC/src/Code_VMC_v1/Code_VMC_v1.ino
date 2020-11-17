#include <Arduino.h>
#include <M5StickC.h>
#include <driver/rtc_io.h> // from ESP-IDF
#include <PubSubClient.h> //Librairie pour la gestion Mqtt
#include <WiFi.h>
#include <WiFiMulti.h>

// Option WiFi
const char *Name_Wifi = "devolo-5ac";
const char *Mdp_Wifi = "ACDCCTGKBZXULRUW";
// Propriétés du WIFI
WiFiClient espClient;
WiFiMulti WiFiMulti;
// Propriétés Timer
#define Time_to_sleep 15000000/*900000000*/ /* Time ESP32 will go to sleep (in microseconds); correspond a 15min*/
// Option MQTT
#define PAYLOAD_BUFFER_SIZE  (60)
const char *name_device = "GK001";
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

float x = 0;
float y = 0;
float z = 0;
bool is_vmc_on = false;

void format_payload() {
    delay(500);
    battery = (int)(M5.Axp.GetBatVoltage()  * 1000);
    snprintf (formatted_payload, PAYLOAD_BUFFER_SIZE, "{\"d\":\"VMC01\",\"cnt\":%d,\"bat\":%d,\"b1\":%d}",cpt_payload,battery,(int)is_vmc_on);
    cpt_payload++;
    return;
}

void connect_to_wifi() {
    WiFiMulti.addAP(Name_Wifi, Mdp_Wifi);
    // check if the wifi connection works
    while(WiFiMulti.run() != WL_CONNECTED);
    return;
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

void send_state() {
    format_payload();
    connect_to_wifi();
    send_payload_to_broker();
    disconnect_all();
    return;
}

void setup() {
    M5.begin(false,true,false);
    M5.Axp.SetLDO2(false);
    M5.Axp.SetLDO3(false);
    // disable all wakeup sources
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // enable waking up on RTC timer
    esp_sleep_enable_timer_wakeup(Time_to_sleep);
    // initialization only on first awakening
    if (g_wom_count == 0) {
        M5.Mpu6886.Init(); // basic init
    }
    M5.MPU6886.getAccelData(&x,&y,&z);
    is_vmc_on = (y<0.03) ? false : true;
    send_state();
    // Increment boot number it every reboot
    g_wom_count++;
    // Go to sleep now
    M5.Axp.SetSleep(); // conveniently turn off screen, etc.
    esp_deep_sleep_start();
}
void loop() {
    //test
}
