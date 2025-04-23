/*
  * @file  movAlzWatch.ino
  * @brief  Sensor de presencia
  * @copyright Copyright (c) 2025 Universidad Autónoma Metropolitana. México.
  * @license The MIT License (MIT)
  * @author UAM
  * @version V1.0
  * @date 2025-04-10
  */

#include "DFRobot_C4001.h"

SoftwareSerial mySerial(D7, D6);  /*Conexìon de la comunicacion con el Sensor*/        
DFRobot_C4001_UART radar(&mySerial,9600);

long lastEvent;
bool lastState = false;


/*!
 * Setup 
 */
void setup()
{
  Serial.begin(9600);
  while(!Serial);
  delay(1000);
  Serial.println(".");
  Serial.println("Empezando");
  delay(1000);
  
  pinMode(D5,INPUT); /* Salida del sensor*/
  
  while(!radar.begin()){
    Serial.println("NO Deivces !");
    delay(1000);
  }
  Serial.println("Device connected!");

  Serial.println("Settng mode...");
  // exist Mode
  radar.setSensorMode(eExitMode);
  Serial.println("Mode set!");

  sSensorStatus_t data;
  data = radar.getStatus();
  //  0 stop  1 start
  Serial.print("work status  = ");
  Serial.println(data.workStatus);

  //  0 is exist   1 speed
  Serial.print("work mode  = ");
  Serial.println(data.workMode);

  //  0 no init    1 init success
  Serial.print("init status = ");
  Serial.println(data.initStatus);
  Serial.println();

  /*
   * min Detection range Minimum distance, unit cm, range 0.3~20m (30~2000), not exceeding max, otherwise the function is abnormal.
   * max Detection range Maximum distance, unit cm, range 2.4~20m (240~2000)
   * trig Detection range Maximum distance, unit cm, default trig = max
   */
  if(radar.setDetectionRange(/*min*/30, /*max*/240, /*trig*/100)){
    Serial.println("set detection range successfully!");
  }
  // set trigger sensitivity 0 - 9
  if(radar.setTrigSensitivity(1)){
    Serial.println("set trig sensitivity successfully!");
  }

  // set keep sensitivity 0 - 9
  if(radar.setKeepSensitivity(1)){
    Serial.println("set keep sensitivity successfully!");
  }
  /*
   * trig Trigger delay, unit 0.01s, range 0~2s (0~200)
   * keep Maintain the detection timeout, unit 0.5s, range 2~1500 seconds (4~3000)
   */
  if(radar.setDelay(/*trig*/5, /*keep*/2)){
    Serial.println("set delay successfully!");
  }
  
  /*
   * pwm1  When no target is detected, the duty cycle of the output signal of the OUT pin ranges from 0 to 100
   * pwm2  After the target is detected, the duty cycle of the output signal of the OUT pin ranges from 0 to 100
   * timer The value ranges from 0 to 255, corresponding to timer x 64ms
   *        For example, timer=20, it takes 20*64ms=1.28s for the duty cycle to change from pwm1 to pwm2.
   */
  if(radar.setPwm(/*pwm1*/50, /*pwm2*/50, /*timer*/1)){
    Serial.println("set pwm period successfully!");
  }

  /*
   * Serial module valid
   * Set pwm polarity
   * 0：Output low level when there is a target, output high level when there is no target
   * 1: Output high level when there is a target, output low level when there is no target (default)
   */
  if(radar.setIoPolaity(1)){
    Serial.println("set Io Polaity successfully!");
  }

  // get confige params
  Serial.print("trig sensitivity = ");
  Serial.println(radar.getTrigSensitivity());
  Serial.print("keep sensitivity = ");
  Serial.println(radar.getKeepSensitivity());

  Serial.print("min range = ");
  Serial.println(radar.getMinRange());
  Serial.print("max range = ");
  Serial.println(radar.getMaxRange());
  Serial.print("trig range = ");
  Serial.println(radar.getTrigRange());

  Serial.print("keep time = ");
  Serial.println(radar.getKeepTimerout());

  Serial.print("trig delay = ");
  Serial.println(radar.getTrigDelay());

  Serial.print("polaity = ");
  Serial.println(radar.getIoPolaity());

  sPwmData_t pwmData;
  pwmData = radar.getPwm();
  Serial.print("pwm1 = ");
  Serial.println(pwmData.pwm1);
  Serial.print("pwm2 = ");
  Serial.println(pwmData.pwm2);
  Serial.print("pwm timer = ");
  Serial.println(pwmData.timer);
}

void loop()
{
  // Determine whether the object is moving
  bool state = digitalRead(D5);
  if(state != lastState){
    if(state){
      Serial.println("Presente");
    }else{
      Serial.println("Ausente");
    }
    lastState=state;
  }
  delay(500);
}
