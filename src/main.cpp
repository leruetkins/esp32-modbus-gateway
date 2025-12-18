#include <WiFi.h>

#ifdef USE_ENC28J60
  #include <EthernetENC.h>
  #include <ModbusBridgeEthernet.h>
  #include "pages_ethernet_awot.h"
  #define NETWORK_TYPE "Ethernet"
#else
  #include <AsyncTCP.h>
  #include <WiFiManager.h>
  #include <ESPAsyncWebServer.h>
  #include "pages.h"
  #define NETWORK_TYPE "WiFi"
#endif

#include <Preferences.h>
#include <Logging.h>
#include <ModbusClientRTU.h>
#include "config.h"

#ifdef USE_ENC28J60
  EthernetWebUI webUI;
#else
  AsyncWebServer webServer(80);
  WiFiManager wm;
#endif

Config config;
Preferences prefs;
ModbusClientRTU *MBclient;

#ifdef USE_ENC28J60
  ModbusBridgeEthernet MBbridge;
#else
  ModbusBridgeWiFi MBbridge;
#endif

void setup() {
  debugSerial.begin(115200);
  dbgln();
  dbgln("[config] load")
  prefs.begin("modbusRtuGw");
  config.begin(&prefs);
  debugSerial.end();
  debugSerial.begin(config.getSerialBaudRate(), config.getSerialConfig());
  
#ifdef USE_ENC28J60
  dbgln("[ethernet] start");
  
  // ВАЖНО: Отключаем WiFi чтобы избежать конфликта с lwIP
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  // Настройка пинов для ENC28J60
  #ifndef ENC_CS_PIN
    #define ENC_CS_PIN 5  // По умолчанию GPIO5
  #endif
  #ifndef ENC_INT_PIN
    #define ENC_INT_PIN 4  // По умолчанию GPIO4
  #endif
  #ifndef ENC_RESET_PIN
    #define ENC_RESET_PIN 2  // По умолчанию GPIO2
  #endif
  
  // Настройка RESET пина
  pinMode(ENC_RESET_PIN, OUTPUT);
  digitalWrite(ENC_RESET_PIN, LOW);
  delay(10);
  digitalWrite(ENC_RESET_PIN, HIGH);
  delay(100);
  
  dbg("ENC28J60 pins - CS: ");
  dbg(ENC_CS_PIN);
  dbg(", INT: ");
  dbg(ENC_INT_PIN);
  dbg(", RESET: ");
  dbgln(ENC_RESET_PIN);
  
  // Настройка INT пина (прерывание)
  pinMode(ENC_INT_PIN, INPUT_PULLUP);
  
  // Инициализация Ethernet с MAC адресом
  uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
  
  dbgln("Initializing ENC28J60...");
  
  if (config.getUseDhcp()) {
    dbgln("Using DHCP...");
    if (Ethernet.begin(mac) == 0) {
      dbgln("DHCP failed! Using static IP as fallback");
      Ethernet.begin(mac, config.getStaticIp(), config.getStaticDns(), 
                     config.getStaticGateway(), config.getStaticSubnet());
    }
  } else {
    dbgln("Using static IP...");
    Ethernet.begin(mac, config.getStaticIp(), config.getStaticDns(), 
                   config.getStaticGateway(), config.getStaticSubnet());
  }
  
  delay(1000);
  dbg("IP address: ");
  dbgln(Ethernet.localIP());
  dbg("Gateway: ");
  dbgln(Ethernet.gatewayIP());
  dbg("Subnet: ");
  dbgln(Ethernet.subnetMask());
  dbgln("[ethernet] finished");
#else
  dbgln("[wifi] start");
  WiFi.mode(WIFI_STA);
  wm.setClass("invert");
  auto reboot = false;
  wm.setAPCallback([&reboot](WiFiManager *wifiManager){reboot = true;});
  wm.autoConnect();
  if (reboot){
    ESP.restart();
  }
  dbgln("[wifi] finished");
#endif
  dbgln("[modbus] start");

  MBUlogLvl = LOG_LEVEL_WARNING;
  RTUutils::prepareHardwareSerial(modbusSerial);
#if defined(RX_PIN) && defined(TX_PIN)
  // use rx and tx-pins if defined in platformio.ini
  modbusSerial.begin(config.getModbusBaudRate(), config.getModbusConfig(), RX_PIN, TX_PIN );
  dbgln("Use user defined RX/TX pins");
#else
  // otherwise use default pins for hardware-serial2
  modbusSerial.begin(config.getModbusBaudRate(), config.getModbusConfig());
#endif

  MBclient = new ModbusClientRTU(config.getModbusRtsPin());
  MBclient->setTimeout(1000);
  MBclient->begin(modbusSerial, 1);
  for (uint8_t i = 1; i < 248; i++)
  {
    MBbridge.attachServer(i, i, ANY_FUNCTION_CODE, MBclient);
  }  
  MBbridge.start(config.getTcpPort(), 10, config.getTcpTimeout());
  dbgln("[modbus] finished");
  
  dbgln("[webserver] start");
#ifdef USE_ENC28J60
  webUI.begin(MBclient, &MBbridge, &config);
#else
  setupPages(&webServer, MBclient, &MBbridge, &config, &wm);
  webServer.begin();
#endif
  dbgln("[webserver] finished");
  
  dbgln("[setup] finished");
}

void loop() {
#ifdef USE_ENC28J60
  // Поддержка Ethernet соединения
  Ethernet.maintain();
  // Обработка веб-запросов
  webUI.loop();
#endif
}