#include <M5Atom.h> 
#include <Arduino.h> 

// Propriétés Valeur Accéléromètre
float X = 0;
float Y = 0;
float Z = 0;

int is_vmc_on = 0;

void setup() {
  // put your setup code here, to run once:
  M5.begin(true, true, true);
  M5.IMU.Init();
}

void loop() {
  // put your main code here, to run repeatedly:
  float res = 0;
  int i = 0;
  while (i != 30) {
      delay(100); // att 0.1sec pour l'échantillonnage des valuers
      M5.IMU.getAccelData(&X,&Y,&Z);
      Y = (Y < 0) ?-Y : Y;
      Serial.printf("Y : %f\n", Y);
      res += Y;
      i++;
  }
  res /= i;
  Serial.printf("res : %f\n", res);
  delay(5000);
  is_vmc_on = (res > 0.030) ? 1 : 0;
  if (is_vmc_on == 1) {
    M5.dis.drawpix(0, 0xf00000);
    delay(1000);
    M5.update();
    }
}
