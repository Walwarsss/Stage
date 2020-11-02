#include <Arduino.h>
#include <M5StickC.h>
#include <utility/MPU6886.h> // used for accessing MPU constants
#include <PubSubClient.h> //Librairie pour la gestion Mqtt
#include <WiFi.h>
#include <WiFiMulti.h>

// Propriétés du WIFI
WiFiClient espClient;
WiFiMulti WiFiMulti;

// Propriétés MQTT
#define PAYLOAD_BUFFER_SIZE  (60)
char formatted_payload[PAYLOAD_BUFFER_SIZE];
PubSubClient client(espClient);
int cpt_payload = 1;
const uint16_t port = 1883;
const char *host = "broker-dev.enovact.fr";
int battery = 0;

// Mesures des capteurs
float accx = 0;
float accy = 0;
float accz = 0;


void mpu6886_wake_on_motion_isr(void); // declaration of ISR
void mpu6886_wake_on_motion_setup(void); // declaration of setup

// lifted from https://github.com/m5stack/M5StickC/blob/master/src/utility/MPU6886.cpp
// if integrated with M5StickC library, use internal class function instead
void MPU6886_I2C_Read_NBytes(uint8_t start_Addr, uint8_t number_Bytes, uint8_t *read_Buffer){
  Wire1.beginTransmission(MPU6886_ADDRESS);
  Wire1.write(start_Addr);  
  Wire1.endTransmission(false);
  uint8_t i = 0;
  Wire1.requestFrom(MPU6886_ADDRESS, (int)number_Bytes);
 
  //! Put read results in the Rx buffer
  while (Wire1.available()) {
    read_Buffer[i++] = Wire1.read();
  }        
}

// lifted from https://github.com/m5stack/M5StickC/blob/master/src/utility/MPU6886.cpp
// if integrated with M5StickC library, use internal class function instead
void MPU6886_I2C_Write_NBytes(uint8_t start_Addr, uint8_t number_Bytes, uint8_t *write_Buffer){
  Wire1.beginTransmission(MPU6886_ADDRESS);
  Wire1.write(start_Addr);
  Wire1.write(*write_Buffer);
  Wire1.endTransmission();
}

// macros/defines manually created from MPU6886 datasheet
// #ifndef MPU6886_ACCEL_WOM_THR
// note - this #define is bogus and unnecessary; seems like a datasheet error
// #define MPU6886_ACCEL_WOM_THR 0x1F
// #endif

#ifndef MPU6886_INT_STATUS
// note - if integrated with M5StickC library, put this #define in MPU6886.h
#define MPU6886_INT_STATUS 0x3A
#endif

#define STEP1_PWR_MGMT_1_CYCLE_SLEEP_GYRO_STANDBY_000(r) (r & 0x8F) // zero bits 6,5,4 of 7:0
#define STEP1_PWR_MGMT_2_STBY_XYZ_A_110_G_111 0x37 // one bits 5,4, zero bit 3 and one bits 2,1,0 of 5:0

#define STEP2_ACCEL_CONFIG2_FCHOICE_B_0_DLPF_CFG_001(r) (0x21) // average 32 samples, use 218 Hz DLPF

#define STEP3_INT_ENABLE_WOM_INT_EN_001 0x20 // zero bits 7,6, on bit 5, zero bits 4,3,2,1,0 of 7:0

#define STEP5_ACCEL_INTEL_CTRL_INTEL_EN_1_MODE_1_WOM_TH_MODE_0 0xC2 // one bits 7,6,1 of 7:0

#define STEP8_PWR_MGMT_1_CYCLE_1(r) (r | 0x20)

void mpu6886_wake_on_motion_setup(uint8_t num_lsb_at_16G_FSR) {
    uint8_t regdata;
    /* 5.1 WAKE-ON-MOTION INTERRUPT
        The MPU-6886 provides motion detection capability. A qualifying motion sample is one where the high passed sample
        from any axis has an absolute value exceeding a user-programmable threshold. The following steps explain how to
        configure the Wake-on-Motion Interrupt.
    */

    /* Step 0: this isn't explicitly listed in the steps, but configuring the
       FSR or full-scale-range of the accelerometer is important to setting up
       the accel/motion threshold in Step 4
    */
    regdata = (M5.Mpu6886.Ascale::AFS_16G << 3);
    MPU6886_I2C_Write_NBytes(MPU6886_ACCEL_CONFIG, 1, &regdata);
    delay(10);

    /* Step 1: Ensure that Accelerometer is running
        • In PWR_MGMT_1 register (0x6B) set CYCLE = 0, SLEEP = 0, and GYRO_STANDBY = 0
        • In PWR_MGMT_2 register (0x6C) set STBY_XA = STBY_YA = STBY_ZA = 0, and STBY_XG = STBY_YG = STBY_ZG = 1
    */
    MPU6886_I2C_Read_NBytes(MPU6886_PWR_MGMT_1, 1, &regdata);
    ESP_LOGD("WOM", "1A: MPU6886_PWR_MGMT_1 = 0x%02X", regdata);
    regdata = STEP1_PWR_MGMT_1_CYCLE_SLEEP_GYRO_STANDBY_000(regdata);
    ESP_LOGD("WOM", "1B: MPU6886_PWR_MGMT_1 = 0x%02X", regdata);
    MPU6886_I2C_Write_NBytes(MPU6886_PWR_MGMT_1, 1, &regdata);
    
    // DEBUG READ
    MPU6886_I2C_Read_NBytes(MPU6886_PWR_MGMT_1, 1, &regdata);
    ESP_LOGD("WOM", "1C: MPU6886_PWR_MGMT_1 = 0x%02X", regdata);

    regdata = STEP1_PWR_MGMT_2_STBY_XYZ_A_110_G_111;
    ESP_LOGD("WOM", "1D: MPU6886_PWR_MGMT_2 = 0x%02X", regdata);
    MPU6886_I2C_Write_NBytes(MPU6886_PWR_MGMT_2, 1, &regdata);

    // DEBUG READ
    MPU6886_I2C_Read_NBytes(MPU6886_PWR_MGMT_2, 1, &regdata);
    ESP_LOGD("WOM", "1E: MPU6886_PWR_MGMT_2 = 0x%02X\n", regdata);


    /* Step 2: Set Accelerometer LPF bandwidth to 218.1 Hz
        • In ACCEL_CONFIG2 register (0x1D) set ACCEL_FCHOICE_B = 0 and A_DLPF_CFG[2:0] = 1 (b001)
    */
    MPU6886_I2C_Read_NBytes(MPU6886_ACCEL_CONFIG2, 1, &regdata);
    ESP_LOGD("WOM", "2A: MPU6886_ACCEL_CONFIG2 = 0x%02X", regdata);
    regdata = STEP2_ACCEL_CONFIG2_FCHOICE_B_0_DLPF_CFG_001(regdata);
    ESP_LOGD("WOM", "2B: MPU6886_ACCEL_CONFIG2 = 0x%02X", regdata);
    MPU6886_I2C_Write_NBytes(MPU6886_ACCEL_CONFIG2, 1, &regdata);

    // DEBUG READ
    MPU6886_I2C_Read_NBytes(MPU6886_ACCEL_CONFIG2, 1, &regdata);
    ESP_LOGD("WOM", "2C: MPU6886_ACCEL_CONFIG2 = 0x%02X\n", regdata);
    
    /* Step 3: Enable Motion Interrupt
        • In INT_ENABLE register (0x38) set WOM_INT_EN = 111 to enable motion interrupt
    */
    regdata = STEP3_INT_ENABLE_WOM_INT_EN_001;
    ESP_LOGD("WOM", "3A: MPU6886_INT_ENABLE = 0x%02X", regdata);
    MPU6886_I2C_Write_NBytes(MPU6886_INT_ENABLE, 1, &regdata);
    
    // DEBUG READ
    MPU6886_I2C_Read_NBytes(MPU6886_INT_ENABLE, 1, &regdata);
    ESP_LOGD("WOM", "3B: MPU6886_INT_ENABLE = 0x%02X\n", regdata);

    /* Step 4: Set Motion Threshold
        • Set the motion threshold in ACCEL_WOM_THR register (0x1F)
    */
    regdata = num_lsb_at_16G_FSR;
    ESP_LOGD("WOM", "4A: num_lsb_at_16G_FSR = 0x%02X", regdata);

    MPU6886_I2C_Write_NBytes(0x20, 1, &regdata);
    MPU6886_I2C_Read_NBytes(0x20, 1, &regdata);
    ESP_LOGD("WOM", "4B: 0x20(XTHR) = 0x%02X", regdata);

    MPU6886_I2C_Write_NBytes(0x21, 1, &regdata);
    MPU6886_I2C_Read_NBytes(0x21, 1, &regdata);
    ESP_LOGD("WOM", "4C: 0x21(YTHR) = 0x%02X", regdata);

    MPU6886_I2C_Write_NBytes(0x22, 1, &regdata);
    MPU6886_I2C_Read_NBytes(0x22, 1, &regdata);
    ESP_LOGD("WOM", "4D: 0x22(ZTHR) = 0x%02X\n", regdata);
    
    /* Step 5: Enable Accelerometer Hardware Intelligence
        • In ACCEL_INTEL_CTRL register (0x69) set ACCEL_INTEL_EN = ACCEL_INTEL_MODE = 1;
          Ensure that bit 0 is set to 0
    */
    regdata = STEP5_ACCEL_INTEL_CTRL_INTEL_EN_1_MODE_1_WOM_TH_MODE_0;
    ESP_LOGD("WOM", "5A: MPU6886_ACCEL_INTEL_CTRL = 0x%02X", regdata);
    MPU6886_I2C_Write_NBytes(MPU6886_ACCEL_INTEL_CTRL, 1, &regdata);
    MPU6886_I2C_Read_NBytes(MPU6886_ACCEL_INTEL_CTRL, 1, &regdata);
    ESP_LOGD("WOM", "5B: MPU6886_ACCEL_INTEL_CTRL = 0x%02X\n", regdata);

    /* Step 7: Set Frequency of Wake-Up
        • In SMPLRT_DIV register (0x19) set SMPLRT_DIV[7:0] = 3.9 Hz – 500 Hz
    */
    // sample_rate = 1e3 / (1 + regdata)
    //   4.0 Hz = 1e3 / (1 + 249)
    //  10.0 Hz = 1e3 / (1 +  99)
    //  20.0 Hz = 1e3 / (1 +  49)
    //  25.0 Hz = 1e3 / (1 +  39)
    //  50.0 Hz = 1e3 / (1 +  19)
    // 500.0 Hz = 1e3 / (1 +   1)
    regdata = 19; //
    ESP_LOGD("WOM", "7A: MPU6886_SMPLRT_DIV = 0x%02X", regdata);
    MPU6886_I2C_Write_NBytes(MPU6886_SMPLRT_DIV, 1, &regdata);
    MPU6886_I2C_Read_NBytes(MPU6886_SMPLRT_DIV, 1, &regdata);
    ESP_LOGD("WOM", "7B: MPU6886_SMPLRT_DIV = 0x%02X\n", regdata);
    ESP_LOGD("WOM", "7C: effective sample rate = 0x%d\n", 1000 / (1+regdata));
    
    /* Step 8: Enable Cycle Mode (Accelerometer Low-Power Mode)
        • In PWR_MGMT_1 register (0x6B) set CYCLE = 1
    */
    MPU6886_I2C_Read_NBytes(MPU6886_PWR_MGMT_1, 1, &regdata);
    ESP_LOGD("WOM", "8A: MPU6886_PWR_MGMT_1 = 0x%02X", regdata);
    regdata = STEP8_PWR_MGMT_1_CYCLE_1(regdata);
    ESP_LOGD("WOM", "8B: MPU6886_PWR_MGMT_1 = 0x%02X", regdata);
    MPU6886_I2C_Write_NBytes(MPU6886_PWR_MGMT_1, 1, &regdata);
    MPU6886_I2C_Read_NBytes(MPU6886_PWR_MGMT_1, 1, &regdata);
    ESP_LOGD("WOM", "8C: MPU6886_PWR_MGMT_1 = 0x%02X\n", regdata);
}

uint32_t g_wom_count = 0;
void mpu6886_wake_on_motion_isr(void) {
    g_wom_count++;
    // ESP_LOGD("ISR", "UNSAFE DEBUG!");
}

void setup() {
    // put your setup code here, to run once:
    M5.begin();
    
    // used for debugging attachInterrupt pinMode(GPIO_NUM_26, OUTPUT);
    // used for debugging attachInterrupt pinMode(GPIO_NUM_36, INPUT);
    pinMode(GPIO_NUM_35, INPUT);

    // set up ISR to trigger on GPIO35
    attachInterrupt(GPIO_NUM_35, mpu6886_wake_on_motion_isr, RISING);

    // éteint l'écran (moin de consommation)
    M5.Axp.SetLDO2(false);
    // Initialisation du MPU (pour lecture accéléro)
    M5.MPU6886.Init();
    // Paramétrage WIFI
    WiFiMulti.addAP("Bbox-FB965A8A", "4CA16F1F347F1354E27E42F7CCCD75"); // Déplacé par Emmanuel dans le setup. Vérifier que cela n’induit pas d’erreur.
    // Paramétrage MQTT
    client.setServer(host, port);
    
    // set up mpu6886 for low-power operation
    M5.IMU.Init(); // basic init
    mpu6886_wake_on_motion_setup(20);
}

void format_payload() {
  battery = (int)(M5.Axp.GetBatVoltage()  * 1000);
 
//mise en place de la payload a envoyer
    snprintf (formatted_payload, PAYLOAD_BUFFER_SIZE, "{\"d\":\"GK001\",\"cnt\":%d,\"bat\":%d,\"b1\":1}",cpt_payload,battery);
  cpt_payload++;
 
return;
}

void send_payload_to_broker() {
 //connexion au broker
 // client.setServer(host, port); // => Déplacé dans le setup (vérifier que n’induit pas de bug)
 client.connect("GK001", "Geoffrey", "Lazzari");
 //envoie de la payload
 client.publish("data/eqp", formatted_payload);
 delay(500);
 return;
}

void disconnect_all() {
  //Déconnexion du broker
  client.disconnect();
  //Déconnexion du Wifi
  WiFi.disconnect();
  return;
}

void connect_to_wifi() {
// WiFiMulti.addAP("Bbox-FB965A8A", "4CA16F1F347F1354E27E42F7CCCD75"); // Déplacé dans le setup. Vérifier que cela n’induit pas d’erreur.
 
  //vérifie si la connexion au wifi à marcher
  while(WiFiMulti.run() != WL_CONNECTED);
  return;
}

void send_alarm() {
  connect_to_wifi();
  format_payload();
  send_payload_to_broker();
  disconnect_all();
  return;
}

void loop() {
    uint8_t regdata;
    static uint32_t wom_count = 0;
    static uint32_t last_dwrite_millis = 0;
    static uint32_t last_wom_millis = 0;
    uint32_t now_millis = millis();

    if(now_millis - last_dwrite_millis > 2000) {
        last_dwrite_millis = now_millis;
        digitalWrite(GPIO_NUM_26, 1);
        delay(100);
        digitalWrite(GPIO_NUM_26, 0);
    }

    if(wom_count != g_wom_count ) {
        // clear interrupt status register by reading it
        MPU6886_I2C_Read_NBytes(MPU6886_INT_STATUS, 1, &regdata);

        // update wom_count and last_wom_millis
        wom_count = g_wom_count;
        last_wom_millis = now_millis;

        // récupère les mesures des accéléromètres
        M5.MPU6886.getAccelData(&accx, &accy, &accz);
        // M5.MPU6886.getAccelData(~, ~, &accz);

        // Gestion du mouvement de la porte
        if (accz > 0.3) {
            send_alarm(); // Génère l’envoi de la payload MQTT
            delay(200);
        }
    }
    delay(1000);  
}
