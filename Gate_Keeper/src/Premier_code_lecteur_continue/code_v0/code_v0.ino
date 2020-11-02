#include <M5StickC.h>
#include <PubSubClient.h> //Librairie pour la gestion Mqtt
#include <WiFi.h>
#include <WiFiMulti.h>

#define MSG_BUFFER_SIZE  (60)

 int value = 1;

WiFiMulti WiFiMulti;

void message_to_broker(char* Payload) {
    int i = 13;
    int b = 0;
    const uint16_t port = 1883;
    const char *host = "broker-dev.enovact.fr";
 
   Serial.println(Payload);
   WiFiClient espClient;
   PubSubClient client(espClient);
   client.setServer(host, port);
   client.connect("GK001", "Geoffrey", "Lazzari");
   client.publish("data/eqp", Payload);
}

void payload() {
 
  int battery = (int)(M5.Axp.GetBatVoltage()  * 1000);
  char msg[MSG_BUFFER_SIZE];
  char *device_id = "GK001";
  int door_state = 1;

  snprintf (msg, MSG_BUFFER_SIZE, "{\"d\":\"%s\",\"cnt\":%d,\"bat\":%d,\"b1\":%d}",device_id,value,battery,door_state);
  value++;
  message_to_broker(msg);
}

char *wifi() {
    WiFiMulti.addAP("devolo-5ac", "ACDCCTGKBZXULRUW");
   /* Serial.print("Waiting for Wifi ...");*/

    WiFi.begin("devolo-5ac", "ACDCCTGKBZXULRUW");
    
    while(WiFiMulti.run() != WL_CONNECTED) {
          Serial.print(".");
          delay(500);
     }
     
     Serial.println("");
     Serial.println("WiFi connected");
     Serial.println("IP address: ");
     Serial.println(WiFi.localIP());
 
     delay(500);
     payload();
}

void setup() {
  // put your setup code here, to run once:
    M5.begin();
    Serial.begin(115200);
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(40, 0);
    M5.Lcd.println("MPU6886 TEST");
    M5.Lcd.setCursor(0, 15);
    M5.Lcd.println("  X       Y       Z");
    M5.MPU6886.Init();
   
}

void loop() {
  // put your main code here, to run repeatedly:
    float x = 0;
    float y = 0;
    float z = 0;
    
    M5.MPU6886.getAccelData(&x, &y, &z);
    M5.Lcd.setCursor(0, 45);
    M5.Lcd.printf("%.2f   %.2f   %.2f      ",x,y,z );
    delay(500);

    if (z > 0.5)
      wifi();
}
