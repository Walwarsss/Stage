#STAGE

I) Projet M5StickC
=================

a) Gate Keeper 
-----------------

Le but est d'utiliser **l'accéléromètre** du M5StickC pour surveiller l'ouverture de porte.
Pour ensuite envoier un message afin de prévenir l'ouverture de la porte.

Mais la contrainte c'est que le projet est un Objet connecter et qu'il doit être autonome sur 3 mois
sauf que la batterie de M5StickC ne depasse pas les 95ma/h (riduclement petit).
      
b) Code
-----------------    
Pour ce faire on utilise la fonction 'enableWakeOnMotion' pouyr parrametre l'accéléromètre pour émettre une intérruption

    void MPU6886::enableWakeOnMotion(Ascale ascale, uint8_t thresh_num_lsb) {
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
        regdata = (ascale << 3);
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_ACCEL_CONFIG, 1, &regdata);
        delay(10);

        /* Step 1: Ensure that Accelerometer is running
        • In PWR_MGMT_1 register (0x6B) set CYCLE = 0, SLEEP = 0, and GYRO_STANDBY = 0
        • In PWR_MGMT_2 register (0x6C) set STBY_XA = STBY_YA = STBY_ZA = 0, and STBY_XG = STBY_YG = STBY_ZG = 1
        */
        I2C_Read_NBytes(MPU6886_ADDRESS, MPU6886_PWR_MGMT_1, 1, &regdata);
        regdata = regdata & 0b10001111; // set cyle, sleep, and gyro to standby, i.e. 0
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_PWR_MGMT_1, 1, &regdata);

        regdata = 0b00010111; // set accel x, y, and z to standby 
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_PWR_MGMT_2, 1, &regdata);

        /* Step 2: Set Accelerometer LPF bandwidth to 218.1 Hz
        • In ACCEL_CONFIG2 register (0x1D) set ACCEL_FCHOICE_B = 0 and A_DLPF_CFG[2:0] = 1 (b001)
        */
        I2C_Read_NBytes(MPU6886_ADDRESS, MPU6886_ACCEL_CONFIG2, 1, &regdata);
        regdata = 0b00100001; // average 32 samples, use 218 Hz DLPF
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_ACCEL_CONFIG2, 1, &regdata);

        /* Step 2.5 - active low? */
        I2C_Read_NBytes(MPU6886_ADDRESS, MPU6886_INT_PIN_CFG, 1, &regdata);
        regdata =  ((regdata | 0b10000000) & 0b11011111); // configure pin active-low, no latch
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_INT_PIN_CFG, 1, &regdata);

        /* Step 3: Enable Motion Interrupt
            • In INT_ENABLE register (0x38) set WOM_INT_EN = 111 to enable motion interrupt
        */
        regdata = 0b00100000; // enable wake-on-motion interrupt for X, Y, and Z axes
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_INT_ENABLE, 1, &regdata);

        /* Step 4: Set Motion Threshold
            • Set the motion threshold in ACCEL_WOM_THR register (0x1F)
            NOTE: the data sheet mentions 0x1F, but is probably referring to
            registers 0x20, 0x21, and 0x22 based on empirical tests
        */
        regdata = thresh_num_lsb; // set accel motion threshold for X, Y, and Z axes
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_ACCEL_WOM_X_THR, 1, &regdata);
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_ACCEL_WOM_Y_THR, 1, &regdata);
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_ACCEL_WOM_Z_THR, 1, &regdata);

        /* Step 5: Enable Accelerometer Hardware Intelligence
            • In ACCEL_INTEL_CTRL register (0x69) set ACCEL_INTEL_EN = ACCEL_INTEL_MODE = 1;
              Ensure that bit 0 is set to 0
        */
        regdata = 0b11000010; // enable wake-on-motion if any of X, Y, or Z axes is above threshold
        // WOM_STEP5_ACCEL_INTEL_CTRL_INTEL_EN_1_MODE_1_WOM_TH_MODE_0;
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_ACCEL_INTEL_CTRL, 1, &regdata);

        /* Step 7: Set Frequency of Wake-Up
            • In SMPLRT_DIV register (0x19) set SMPLRT_DIV[7:0] = 3.9 Hz – 500 Hz
        */
        // sample_rate = 1e3 / (1 + 99)
        //   4.0 Hz = 1e3 / (1 + 249)
        //  10.0 Hz = 1e3 / (1 +  99)
        //  20.0 Hz = 1e3 / (1 +  49)
        //  25.0 Hz = 1e3 / (1 +  39)
        //  50.0 Hz = 1e3 / (1 +  19) <----
        // 500.0 Hz = 1e3 / (1 +   1)
        regdata = 99;
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_SMPLRT_DIV, 1, &regdata);

        /* Step 8: Enable Cycle Mode (Accelerometer Low-Power Mode)
            • In PWR_MGMT_1 register (0x6B) set CYCLE = 1
        */
        I2C_Read_NBytes(MPU6886_ADDRESS, MPU6886_PWR_MGMT_1, 1, &regdata);
        regdata = regdata | 0b00100000; // enable accelerometer low-power mode
        I2C_Write_NBytes(MPU6886_ADDRESS, MPU6886_PWR_MGMT_1, 1, &regdata);
    }

Ce qui va nous permettre de détecter l'ouverture de la porte.
Pour le problème de la batterie on utillise le deep sleep, ce qui va plonger le M5StickC dans un mode veille.
(faire attention quand on passe en deep sleep)
![M5_en_deep](https://github.com/Walwarsss/Stage/blob/main/image/deep%20sleep%20exampel.PNG)
