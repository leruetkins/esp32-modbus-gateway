#ifndef PAGES_ETHERNET_AWOT_H
#define PAGES_ETHERNET_AWOT_H

#include <EthernetENC.h>
#include <aWOT.h>
#include <ModbusBridgeEthernet.h>
#include <ModbusClientRTU.h>
#include "config.h"

class EthernetWebUI {
public:
    void begin(ModbusClientRTU *rtu, ModbusBridgeEthernet *bridge, Config *config);
    void loop();

private:
    static EthernetServer server;
    static Application app;
    static ModbusClientRTU *g_rtu;
    static ModbusBridgeEthernet *g_bridge;
    static Config *g_config;
    
    // Обработчики маршрутов
    static void handleRoot(Request &req, Response &res);
    static void handleStatus(Request &req, Response &res);
    static void handleConfig(Request &req, Response &res);
    static void handleConfigPost(Request &req, Response &res);
    static void handleDebug(Request &req, Response &res);
    static void handleDebugPost(Request &req, Response &res);
    static void handleNetwork(Request &req, Response &res);
    static void handleNetworkPost(Request &req, Response &res);
    static void handleReboot(Request &req, Response &res);
    static void handleRebootPost(Request &req, Response &res);
    static void handleUpdate(Request &req, Response &res);
    static void handleUpdatePost(Request &req, Response &res);
    static void handleStyleCss(Request &req, Response &res);
    static void handleFavicon(Request &req, Response &res);
    
    // Вспомогательные функции
    static void sendHtmlHeader(Response &res, const char *title);
    static void sendHtmlFooter(Response &res);
    static void sendButton(Response &res, const char *title, const char *href, const char *cssClass = "");
    static bool checkAuth(Request &req, Response &res);
};

// Вспомогательная функция для декодирования ошибок Modbus
const String ErrorName(Modbus::Error code);

#endif
