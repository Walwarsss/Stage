#include <Arduino.h>
#include <M5StickC.h>
#include <driver/rtc_io.h> // from ESP-IDF
#include <PubSubClient.h> //Librairie pour la gestion Mqtt
#include <WiFi.h>
#include <WiFiMulti.h>

// Option WiFi
#define Name_Wifi "devolo-5ac"
#define Mdp_Wifi "ACDCCTGKBZXULRUW"
// Propriétés du WIFI
WiFiClient espClient;
WiFiMulti WiFiMulti;
// Propriétés Timer
#define Time_to_sleep 86400000000   /* Time ESP32 will go to sleep (in microseconds); correspond a 24h*/
// Option MQTT
#define PAYLOAD_BUFFER_SIZE  (60)
#define name_device "GK001"
#define Id_name "Geoffrey"
#define Mdp_Lname "Lazzari"
// Propriétés MQTT
char formatted_payload[PAYLOAD_BUFFER_SIZE];
PubSubClient client(espClient);
RTC_DATA_ATTR int cpt_payload = 1;
RTC_DATA_ATTR const uint16_t port = 1883;
RTC_DATA_ATTR const char *host = "broker-dev.enovact.fr";
// Wake Up reason
int alive = 2;
int accelero = 1;

int battery = 0;
RTC_DATA_ATTR uint32_t g_wom_count = 0;

void format_payload(int alive_or_accelero) {
    delay(500);
    battery = (int)(M5.Axp.GetBatVoltage()  * 1000);
    if (alive_or_accelero == 2) {
        snprintf (formatted_payload, PAYLOAD_BUFFER_SIZE, "{\"d\":\"GK001\",\"cnt\":%d,\"bat\":%d}",cpt_payload,battery);    
    }
    if (alive_or_accelero == 1) {
        snprintf (formatted_payload, PAYLOAD_BUFFER_SIZE, "{\"d\":\"GK001\",\"cnt\":%d,\"bat\":%d,\"b1\":1}",cpt_payload,battery);
    }
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

void send_alarm(int alive_or_accelero) {
    format_payload(alive_or_accelero);
    connect_to_wifi();
    send_payload_to_broker();
    disconnect_all();
    return;
}

void setup() {
    M5.begin();
    M5.Axp.SetLDO2(false);
    // disable all wakeup sources
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // enable waking up on pin 35 (from IMU)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, LOW); //1 = High, 0 = Low
    esp_sleep_enable_timer_wakeup(Time_to_sleep);
    // initialization only on first awakening
    if (g_wom_count == 0) {
        M5.Mpu6886.Init(); // basic init
        M5.Mpu6886.enableWakeOnMotion(M5.Mpu6886.AFS_16G, 50);
    }
    // check if it is the alive or the accelerometer which causes the awakening
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0 : send_alarm(accelero); break;
        case ESP_SLEEP_WAKEUP_TIMER : send_alarm(alive); break;
    }
    // Increment boot number it every reboot
    g_wom_count++;
    // Go to sleep now
    M5.Axp.SetSleep(); // conveniently turn off screen, etc.
    delay(5000 - millis());
    esp_deep_sleep_start();
}
