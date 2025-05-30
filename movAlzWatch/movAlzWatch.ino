/*
 * @file  movAlzWatch.ino
 * @brief  Sensor de presencia
 * @copyright Copyright (c) 2025 Universidad Autónoma Metropolitana. México.
 * @license The MIT License (MIT)
 * @author UAM
 * @version V1.0
 * @date 2025-04-10
 */
#include <NTPClient.h>
#include <DFRobot_C4001.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Arduino_JSON.h>
#include <ESP8266WebServer.h>
#include <Uri.h>
#include <FS.h>       // File System for Web Server Files
#include <LittleFS.h> // This file system is used.
#include <WiFiUdp.h>
#include <AsyncTelegram2.h>

/* Locales mios con passwords*/
#include "wifiauth.h"
#include "tg_certificate.h"
/* Trabajo con telegram */
#ifdef ESP8266
#include <ESP8266WiFi.h>
BearSSL::WiFiClientSecure tg_client;
BearSSL::Session session;
BearSSL::X509List certificate(telegram_cert);

#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
WiFiClientSecure tg_client;
#endif

// AsyncTelegram2 myBot(tg_client);

typedef struct evento
{
  unsigned long tiempo;
  bool presente;
} evento;

evento eventos[256];
unsigned char iEvPtr, fEvPtr; // eventos se manejara como una cola circular
WiFiUDP ntpUDP;               /* Para obtener la hora de internet */
NTPClient timeClient(ntpUDP);

// local time zone definition (Berlin)
#define TIMEZONE "CST6" // Mexico

SoftwareSerial puertoSensor(D7, D6);           /*Conexìon de la comunicacion con el Sensor*/
DFRobot_C4001_UART radar(&puertoSensor, 9600); // Sensor de presencia
ESP8266WebServer server(80);                   /* Consulta de eventos */

unsigned lastEvent; /* Checar tiempo entre eventos */
bool lastState = 0; // Al principio no esta
const char *ssid = STASSID;
const char *password = STAPSK;
char hostname[400];

String respberryIP = "";

void setupESP(void)
{
  Serial.begin(9600);
  while (!Serial)
    ;
  delay(1000);
  Serial.println(".");
  Serial.println("Empezando");
  pinMode(D5, INPUT); /* Salida del sensor*/
  Serial.println("I/O configurado");
  snprintf(hostname, 400, "alzwatch_%06X", ESP.getChipId());
  Serial.print("hostname:");
  Serial.println(hostname);
  /* init events*/
  for (size_t i = 0; i < 128; i++)
  {
    eventos[i].tiempo = 0;
    eventos[i].presente = 0;
  }
  iEvPtr = 0;
  fEvPtr = 0;
  timeClient.begin();
  timeClient.setTimeOffset(-6 * 60 * 60);
  /*
    tg_client.setSession(&session);
    tg_client.setTrustAnchors(&certificate);
    tg_client.setBufferSizes(1024, 1024);
    */
}
/*
void setupTelegram()
{
  myBot.setUpdateTime(20000);
  myBot.setTelegramToken(TG_TOKEN);
  myBot.begin();
  myBot.setUpdateTime(10);
  char welcome_msg[128];
  snprintf(welcome_msg, 128, "BOT @%s en linea\nSensor:%s", myBot.getBotName(),hostname);

  // Send a message to specific user who has started your bot
  myBot.sendTo(TG_USER, welcome_msg);
}
*/
void setupSensor()
{
  while (!radar.begin())
  {
    Serial.println("Sensor no Conectado !");
    delay(1000);
  }
  Serial.println("Sensor Conectado!");

  Serial.println("Setting mode...");
  // exist Mode
  radar.setSensorMode(eExitMode);
  Serial.println("Mode set!");

  sSensorStatus_t data;
  data = radar.getStatus();
  //  0 stop  1 start
  Serial.print("work status  = ");
  Serial.println(data.workStatus == 0 ? "Stop" : "Start");

  //  0 is exist   1 speed
  Serial.print("work mode  = ");
  Serial.println(data.workMode == 0 ? "Presencia" : "Velocidad");

  //  0 no init    1 init success
  Serial.print("init status = ");
  Serial.println(data.initStatus == 0 ? "Sensor No iniciado" : "Sensor Ok");
  Serial.println();

  /*
   * min Detection range Minimum distance, unit cm, range 0.3~20m (30~2000), not exceeding max, otherwise the function is abnormal.
   * max Detection range Maximum distance, unit cm, range 2.4~20m (240~2000)
   * trig Detection range Maximum distance, unit cm, default trig = max
   */
  if (radar.setDetectionRange(/*min*/ 30, /*max*/ 240, /*trig*/ 10))
  {
    Serial.println("set detection range successfully!");
  }
  // set trigger sensitivity 0 - 9
  if (radar.setTrigSensitivity(1))
  {
    Serial.println("set trig sensitivity successfully!");
  }

  // set keep sensitivity 0 - 9
  if (radar.setKeepSensitivity(1))
  {
    Serial.println("set keep sensitivity successfully!");
  }
  /*
   * trig Trigger delay, unit 0.01s, range 0~2s (0~200)
   * keep Maintain the detection timeout, unit 0.5s, range 2~1500 seconds (4~3000)
   */
  if (radar.setDelay(/*trig*/ 1, /*keep*/ 2))
  {
    Serial.println("set delay successfully!");
  }

  /*
   * pwm1  When no target is detected, the duty cycle of the output signal of the OUT pin ranges from 0 to 100
   * pwm2  After the target is detected, the duty cycle of the output signal of the OUT pin ranges from 0 to 100
   * timer The value ranges from 0 to 255, corresponding to timer x 64ms
   *        For example, timer=20, it takes 20*64ms=1.28s for the duty cycle to change from pwm1 to pwm2.
   */
  if (radar.setPwm(/*pwm1*/ 50, /*pwm2*/ 50, /*timer*/ 1))
  {
    Serial.println("set pwm period successfully!");
  }

  /*
   * Serial module valid
   * Set pwm polarity
   * 0：Output low level when there is a target, output high level when there is no target
   * 1: Output high level when there is a target, output low level when there is no target (default)
   */
  if (radar.setIoPolaity(1))
  {
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
void setupWiFi()
{
  Serial.println("Configurando Wifi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Conexion fallida.  Reiniciando...");
    delay(5000);
    ESP.restart();
  }

  /* Preparando OTA */

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(hostname);

  ArduinoOTA.setPassword(STAOTAPWD);

  ArduinoOTA.onStart([]()
                     {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Empieza Actualizacion " + type); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nFin"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progreso: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("FAlla de Autorizacion");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Falla de inicializacion");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Falla de Conexion");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Falla de Recepcion");
    } else if (error == OTA_END_ERROR) {
      Serial.println("Falla de finalizacion");
    } });
  ArduinoOTA.begin();
   if (MDNS.begin(hostname))
  { 
    Serial.println("mDNS Responder started");
  }
  else
  {
    Serial.println("Failed to start mDNS responder");
  }
  MDNS.addService("http", "http", 80); // Announce an HTTP service on port 80
 
  Serial.println("Listo WIFI");
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());
}
/* WebHandlers */

void handleRoot()
{
  String result;

  result += "<img src=\"https://www.uam.mx/_imgstc/logouam-variacion6.png\"/>";
  result += "<br>Sensor de Movimiento Alzwatch<br>" + String(hostname) + "<br>";
  result += "";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/html; charset=utf-8", result);
}

void handleRaspberry()
{
  String result;

  result += "Raspberry on:";
  raspberryIP = server.client().remoteIP().toString();
  result += raspberryIP;
  Serial.println(result);

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/html; charset=utf-8", result);
}

void handleSysInfo()
{
  String result;

  FSInfo fs_info;
  LittleFS.info(fs_info);

  result += "{\n";
  result += "  \"host\": " + String(hostname) + ",\n";
  result += "  \"flashSize\": " + String(ESP.getFlashChipSize()) + ",\n";
  result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
  result += "  \"fsTotalBytes\": " + String(fs_info.totalBytes) + ",\n";
  result += "  \"fsUsedBytes\": " + String(fs_info.usedBytes) + ",\n";
  result += "}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/javascript; charset=utf-8", result);
} // handleSysInfo()

void handleEventos()
{
  String result;
  const unsigned long offset = 6 * 60 * 60;

  result += "{\"" + String(hostname) + "\" :[\n";
  for (size_t i = iEvPtr; i < fEvPtr; i++)
  {
    result += "{\"T\":" + String(eventos[i].tiempo + offset) + ",";
    result += "\"P\":" + String(eventos[i].presente) + "},\n";
  }
  result += "{\"T\":" + String(timeClient.getEpochTime() + offset) + ",";
  result += "\"P\":" + String(digitalRead(D5)) + "}\n";

  result += "]}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/javascript; charset=utf-8", result);
} // handleEventos()

void setupWebServer()
{
  configTime(TIMEZONE, "pool.ntp.org");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/$sysinfo", HTTP_GET, handleSysInfo);
  server.on("/eventos", HTTP_GET, handleEventos);
  server.on("/raspberry", HTTP_GET, handleRaspberry);
  server.begin();
}

void enviaCambioEstado(){
  WiFiClient client;
  HTTPClient http;  
  http.begin(client, "uam.local/cambio");
 int httpResponseCode = http.GET();
  
  String payload = "--"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

}

/*!
 * Setup  punto de entrada
 */
void setup()
{
  setupESP();
  setupWiFi();
  setupWebServer();
  setupSensor();
  /*setupTelegram();*/
  Serial.println("Set up Terminado");
} // setup
/*
void checkTelegram()
{
  TBMessage msg;
  if (myBot.getNewMessage(msg))
  {
    Serial.print("User ");
    Serial.print(msg.sender.username);
    Serial.print(" send this message: ");
    Serial.println(msg.text);

    // echo the received message
    myBot.sendMessage(msg, msg.text);
  }
}
*/
/* Ejecucion continua */
void loop()
{
  static uint32_t ledTime = millis();
  if (millis() - ledTime > 200)
  {
    ledTime = millis();
    // digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  // Hay movimiento?
  bool state = digitalRead(D5);
  timeClient.update();
  // ver si hubo cambios
  if (state != lastState)
  {

    eventos[fEvPtr].tiempo = timeClient.getEpochTime();
    eventos[fEvPtr].presente = state;
    fEvPtr++;
    char tg_msg[256];

    // Send a message to specific user who has started your bot

    if (state) /* Presencia detectada */
    {
      Serial.print("Presente ");
      Serial.println(timeClient.getFormattedTime());
      snprintf(tg_msg, 256, "Sen:%s Presente", hostname);

    }
    else
    {
      Serial.print("Ausente  ");
      Serial.println(timeClient.getFormattedTime());
      snprintf(tg_msg, 256, "Sen:%s Ausente", hostname);
    }
    // myBot.sendTo(TG_USER, tg_msg);
    lastState = state;
    lastEvent = timeClient.getEpochTime();
  }
  MDNS.update();
  ArduinoOTA.handle();   /* Checar si hay Actualizacion OTA */
  server.handleClient(); /* Atender Web */
  /*checkTelegram();*/
  delay(10);
}
