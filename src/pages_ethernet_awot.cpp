#include "pages_ethernet_awot.h"
#include <Update.h>

// Статические члены класса
EthernetServer EthernetWebUI::server(80);
Application EthernetWebUI::app;
ModbusClientRTU *EthernetWebUI::g_rtu = nullptr;
ModbusBridgeEthernet *EthernetWebUI::g_bridge = nullptr;
Config *EthernetWebUI::g_config = nullptr;

void EthernetWebUI::begin(ModbusClientRTU *rtu, ModbusBridgeEthernet *bridge, Config *config) {
    g_rtu = rtu;
    g_bridge = bridge;
    g_config = config;
    
    // Настройка маршрутов
    app.get("/", &handleRoot);
    app.get("/status", &handleStatus);
    app.get("/config", &handleConfig);
    app.post("/config", &handleConfigPost);
    app.get("/debug", &handleDebug);
    app.post("/debug", &handleDebugPost);
    app.get("/network", &handleNetwork);
    app.post("/network", &handleNetworkPost);
    app.get("/reboot", &handleReboot);
    app.post("/reboot", &handleRebootPost);
    app.get("/update", &handleUpdate);
    app.post("/update", &handleUpdatePost);
    app.get("/style.css", &handleStyleCss);
    app.get("/favicon.ico", &handleFavicon);
    
    server.begin();
    dbgln("[webserver] aWOT started on port 80");
}

void EthernetWebUI::loop() {
    EthernetClient client = server.available();
    if (client) {
        unsigned long timeout = millis();
        // Ждем пока клиент подключится или таймаут 100мс
        while (!client.connected() && millis() - timeout < 100) {
            delay(1);
        }
        
        if (client.connected()) {
            app.process(&client);
            
            // Убеждаемся что все данные отправлены
            client.flush();
            delay(10); // Даем больше времени на отправку
            
            // Явно закрываем соединение
            client.stop();
        }
    }
}

bool EthernetWebUI::checkAuth(Request &req, Response &res) {
    if (g_config->getWebPassword().equals("")) return true;
    
    // Простая проверка Basic Auth
    char *auth = req.get("Authorization");
    if (auth == nullptr) {
        res.set("WWW-Authenticate", "Basic realm=\"ESP32 Gateway\"");
        res.status(401);
        res.print("Unauthorized");
        return false;
    }
    return true;
}

void EthernetWebUI::sendHtmlHeader(Response &res, const char *title) {
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    res.set("Cache-Control", "no-cache, no-store, must-revalidate");
    res.print("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    res.print("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    res.print("<link rel='icon' href='data:,'>");  // Предотвращаем запрос favicon
    res.print("<title>ESP32 Modbus Gateway - ");
    res.print(title);
    res.print("</title>");
    res.print("<style>");
    // Встраиваем CSS чтобы избежать второго запроса
    res.print("body{font-family:sans-serif;text-align:center;background:#252525;color:#faffff;}");
    res.print("#content{display:inline-block;min-width:340px;}");
    res.print("button{width:100%;line-height:2.4rem;background:#1fa3ec;border:0;");
    res.print("border-radius:0.3rem;font-size:1.2rem;color:#faffff;cursor:pointer;}");
    res.print("button:hover{background:#0e70a4;}");
    res.print("button.r{background:#d43535;}button.r:hover{background:#931f1f;}");
    res.print("table{text-align:left;width:100%;margin:10px 0;}");
    res.print("input,select{width:100%;padding:5px;box-sizing:border-box;}");
    res.print(".e{color:red;}");
    res.print("</style>");
    res.print("</head><body>");
    res.print("<h2>ESP32 Modbus Gateway</h2>");
    res.print("<h3>");
    res.print(title);
    res.print("</h3>");
    res.print("<div id='content'>");
}

void EthernetWebUI::sendHtmlFooter(Response &res) {
    res.print("</div></body></html>");
}

void EthernetWebUI::sendButton(Response &res, const char *title, const char *href, const char *cssClass) {
    res.print("<form method='get' action='");
    res.print(href);
    res.print("'><button class='");
    res.print(cssClass);
    res.print("'>");
    res.print(title);
    res.print("</button></form><p></p>");
}

void EthernetWebUI::handleRoot(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /");
    sendHtmlHeader(res, "Main");
    sendButton(res, "Status", "/status");
    sendButton(res, "Config", "/config");
    sendButton(res, "Debug", "/debug");
    sendButton(res, "Network", "/network");
    sendButton(res, "Firmware Update", "/update");
    sendButton(res, "Reboot", "/reboot", "r");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleStatus(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /status");
    sendHtmlHeader(res, "Status");
    
    res.print("<table>");
    res.print("<tr><td>ESP Uptime (sec):</td><td>");
    res.print(esp_timer_get_time() / 1000000);
    res.print("</td></tr>");
    
    res.print("<tr><td>Network:</td><td>Ethernet (ENC28J60)</td></tr>");
    
    uint8_t mac[6];
    Ethernet.macAddress(mac);
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    res.print("<tr><td>MAC:</td><td>");
    res.print(macStr);
    res.print("</td></tr>");
    
    res.print("<tr><td>IP:</td><td>");
    res.print(Ethernet.localIP().toString().c_str());
    res.print("</td></tr>");
    
    res.print("<tr><td>Gateway:</td><td>");
    res.print(Ethernet.gatewayIP().toString().c_str());
    res.print("</td></tr>");
    
    res.print("<tr><td>Subnet:</td><td>");
    res.print(Ethernet.subnetMask().toString().c_str());
    res.print("</td></tr>");
    
    res.print("<tr><td>RTU Messages:</td><td>");
    res.print(g_rtu->getMessageCount());
    res.print("</td></tr>");
    
    res.print("<tr><td>RTU Pending:</td><td>");
    res.print(g_rtu->pendingRequests());
    res.print("</td></tr>");
    
    res.print("<tr><td>RTU Errors:</td><td>");
    res.print(g_rtu->getErrorCount());
    res.print("</td></tr>");
    
    res.print("<tr><td>Bridge Messages:</td><td>");
    res.print(g_bridge->getMessageCount());
    res.print("</td></tr>");
    
    res.print("<tr><td>Bridge Clients:</td><td>");
    res.print(g_bridge->activeClients());
    res.print("</td></tr>");
    
    res.print("<tr><td>Bridge Errors:</td><td>");
    res.print(g_bridge->getErrorCount());
    res.print("</td></tr>");
    
    res.print("<tr><td>&nbsp;</td><td></td></tr>");
    res.print("<tr><td>Build time:</td><td>");
    res.print(__DATE__);
    res.print(" ");
    res.print(__TIME__);
    res.print("</td></tr>");
    
    res.print("</table><p></p>");
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleConfig(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /config");
    sendHtmlHeader(res, "Configuration");
    
    res.print("<form method='post'>");
    res.print("<h3>Modbus TCP</h3><table>");
    res.print("<tr><td><label for='tp'>TCP Port</label></td>");
    res.print("<td><input type='number' min='1' max='65535' id='tp' name='tp' value='");
    res.print(g_config->getTcpPort());
    res.print("'></td></tr>");
    
    res.print("<tr><td><label for='tt'>TCP Timeout (ms)</label></td>");
    res.print("<td><input type='number' min='1' id='tt' name='tt' value='");
    res.print(g_config->getTcpTimeout());
    res.print("'></td></tr>");
    
    res.print("</table><h3>Modbus RTU</h3><table>");
    res.print("<tr><td><label for='mb'>Baud rate</label></td>");
    res.print("<td><input type='number' id='mb' name='mb' value='");
    res.print(g_config->getModbusBaudRate());
    res.print("'></td></tr>");
    
    res.print("<tr><td><label for='md'>Data bits</label></td>");
    res.print("<td><input type='number' min='5' max='8' id='md' name='md' value='");
    res.print(g_config->getModbusDataBits());
    res.print("'></td></tr>");
    
    res.print("<tr><td><label for='mp'>Parity</label></td>");
    res.print("<td><select id='mp' name='mp' data-value='");
    res.print(g_config->getModbusParity());
    res.print("'>");
    res.print("<option value='0'>None</option>");
    res.print("<option value='2'>Even</option>");
    res.print("<option value='3'>Odd</option>");
    res.print("</select></td></tr>");
    
    res.print("<tr><td><label for='ms'>Stop bits</label></td>");
    res.print("<td><select id='ms' name='ms' data-value='");
    res.print(g_config->getModbusStopBits());
    res.print("'>");
    res.print("<option value='1'>1 bit</option>");
    res.print("<option value='2'>1.5 bits</option>");
    res.print("<option value='3'>2 bits</option>");
    res.print("</select></td></tr>");
    
    res.print("<tr><td><label for='mr'>RTS Pin</label></td>");
    res.print("<td><select id='mr' name='mr' data-value='");
    res.print(g_config->getModbusRtsPin());
    res.print("'>");
    res.print("<option value='-1'>Auto</option>");
    res.print("<option value='4'>D4</option>");
    res.print("<option value='13'>D13</option>");
    res.print("<option value='14'>D14</option>");
    res.print("<option value='18'>D18</option>");
    res.print("<option value='19'>D19</option>");
    res.print("<option value='21'>D21</option>");
    res.print("<option value='22'>D22</option>");
    res.print("<option value='23'>D23</option>");
    res.print("<option value='25'>D25</option>");
    res.print("<option value='26'>D26</option>");
    res.print("<option value='27'>D27</option>");
    res.print("<option value='32'>D32</option>");
    res.print("<option value='33'>D33</option>");
    res.print("</select></td></tr>");
    
    res.print("</table><h3>Serial (Debug)</h3><table>");
    res.print("<tr><td><label for='sb'>Baud rate</label></td>");
    res.print("<td><input type='number' id='sb' name='sb' value='");
    res.print(g_config->getSerialBaudRate());
    res.print("'></td></tr>");
    
    res.print("<tr><td><label for='sd'>Data bits</label></td>");
    res.print("<td><input type='number' min='5' max='8' id='sd' name='sd' value='");
    res.print(g_config->getSerialDataBits());
    res.print("'></td></tr>");
    
    res.print("<tr><td><label for='sp'>Parity</label></td>");
    res.print("<td><select id='sp' name='sp' data-value='");
    res.print(g_config->getSerialParity());
    res.print("'>");
    res.print("<option value='0'>None</option>");
    res.print("<option value='2'>Even</option>");
    res.print("<option value='3'>Odd</option>");
    res.print("</select></td></tr>");
    
    res.print("<tr><td><label for='ss'>Stop bits</label></td>");
    res.print("<td><select id='ss' name='ss' data-value='");
    res.print(g_config->getSerialStopBits());
    res.print("'>");
    res.print("<option value='1'>1 bit</option>");
    res.print("<option value='2'>1.5 bits</option>");
    res.print("<option value='3'>2 bits</option>");
    res.print("</select></td></tr>");
    
    res.print("</table><h3>Other</h3><table>");
    res.print("<tr><td><label for='wp'>Web password</label></td>");
    res.print("<td><input type='password' id='wp' name='wp' value='****'></td></tr>");
    
    res.print("</table>");
    res.print("<button class='r'>Save</button></form><p></p>");
    sendButton(res, "Back", "/");
    
    res.print("<script>");
    res.print("(function(){");
    res.print("var s=document.querySelectorAll('select[data-value]');");
    res.print("for(d of s){");
    res.print("d.querySelector(`option[value='${d.dataset.value}']`).selected=true");
    res.print("}})();");
    res.print("</script>");
    
    sendHtmlFooter(res);
}

void EthernetWebUI::handleConfigPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /config");
    char buffer[32];
    
    // Modbus TCP
    if (req.query("tp", buffer, sizeof(buffer))) {
        g_config->setTcpPort(atoi(buffer));
        dbgln("[webserver] saved TCP port");
    }
    if (req.query("tt", buffer, sizeof(buffer))) {
        g_config->setTcpTimeout(atoi(buffer));
        dbgln("[webserver] saved TCP timeout");
    }
    
    // Modbus RTU
    if (req.query("mb", buffer, sizeof(buffer))) {
        g_config->setModbusBaudRate(atol(buffer));
        dbgln("[webserver] saved Modbus baud rate");
    }
    if (req.query("md", buffer, sizeof(buffer))) {
        g_config->setModbusDataBits(atoi(buffer));
        dbgln("[webserver] saved Modbus data bits");
    }
    if (req.query("mp", buffer, sizeof(buffer))) {
        g_config->setModbusParity(atoi(buffer));
        dbgln("[webserver] saved Modbus parity");
    }
    if (req.query("ms", buffer, sizeof(buffer))) {
        g_config->setModbusStopBits(atoi(buffer));
        dbgln("[webserver] saved Modbus stop bits");
    }
    if (req.query("mr", buffer, sizeof(buffer))) {
        g_config->setModbusRtsPin(atoi(buffer));
        dbgln("[webserver] saved Modbus RTS pin");
    }
    
    // Serial (Debug)
    if (req.query("sb", buffer, sizeof(buffer))) {
        g_config->setSerialBaudRate(atol(buffer));
        dbgln("[webserver] saved Serial baud rate");
    }
    if (req.query("sd", buffer, sizeof(buffer))) {
        g_config->setSerialDataBits(atoi(buffer));
        dbgln("[webserver] saved Serial data bits");
    }
    if (req.query("sp", buffer, sizeof(buffer))) {
        g_config->setSerialParity(atoi(buffer));
        dbgln("[webserver] saved Serial parity");
    }
    if (req.query("ss", buffer, sizeof(buffer))) {
        g_config->setSerialStopBits(atoi(buffer));
        dbgln("[webserver] saved Serial stop bits");
    }
    
    // Web password
    if (req.query("wp", buffer, sizeof(buffer))) {
        if (strcmp(buffer, "****") != 0) {
            g_config->setWebPassword(String(buffer));
            dbgln("[webserver] saved web password");
        }
    }
    
    res.status(303);
    res.set("Location", "/");
    res.set("Connection", "close");
    res.set("Content-Type", "text/html");
    res.print("<html><head><meta http-equiv='refresh' content='0;url=/'></head>");
    res.print("<body>Saved! Redirecting...</body></html>");
}

void EthernetWebUI::handleDebug(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /debug");
    sendHtmlHeader(res, "Debug");
    
    res.print("<form method='post'><table>");
    res.print("<tr><td><label for='slave'>Slave ID</label></td>");
    res.print("<td><input type='number' min='0' max='247' id='slave' name='slave' value='1'></td></tr>");
    
    res.print("<tr><td><label for='func'>Function</label></td>");
    res.print("<td><select id='func' name='func'>");
    res.print("<option value='1'>01 Read Coils</option>");
    res.print("<option value='2'>02 Read Discrete Inputs</option>");
    res.print("<option value='3' selected>03 Read Holding Register</option>");
    res.print("<option value='4'>04 Read Input Register</option>");
    res.print("</select></td></tr>");
    
    res.print("<tr><td><label for='reg'>Register</label></td>");
    res.print("<td><input type='number' min='0' max='65535' id='reg' name='reg' value='1'></td></tr>");
    
    res.print("<tr><td><label for='count'>Count</label></td>");
    res.print("<td><input type='number' min='0' max='65535' id='count' name='count' value='1'></td></tr>");
    
    res.print("</table><button class='r'>Send</button></form><p></p>");
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleDebugPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /debug");
    
    char buffer[32];
    String slaveId = "1";
    String reg = "1";
    String func = "3";
    String count = "1";
    
    if (req.query("slave", buffer, sizeof(buffer))) {
        slaveId = String(buffer);
    }
    if (req.query("reg", buffer, sizeof(buffer))) {
        reg = String(buffer);
    }
    if (req.query("func", buffer, sizeof(buffer))) {
        func = String(buffer);
    }
    if (req.query("count", buffer, sizeof(buffer))) {
        count = String(buffer);
    }
    
    sendHtmlHeader(res, "Debug");
    
    res.print("<pre>");
    
    // Сохраняем текущий уровень логирования
    auto previousLevel = MBUlogLvl;
    MBUlogLvl = LOG_LEVEL_DEBUG;
    
    // Выполняем Modbus запрос
    ModbusMessage answer = g_rtu->syncRequest(0xdeadbeef, slaveId.toInt(), func.toInt(), reg.toInt(), count.toInt());
    
    // Восстанавливаем уровень логирования
    MBUlogLvl = previousLevel;
    
    res.print("</pre>");
    
    // Обработка ответа
    auto error = answer.getError();
    if (error == SUCCESS) {
        auto cnt = answer[2];
        res.print("<span>Answer: 0x");
        for (size_t i = 0; i < cnt; i++) {
            char hex[3];
            sprintf(hex, "%02x", answer[i + 3]);
            res.print(hex);
        }
        res.print("</span>");
    } else {
        res.print("<span class='e'>Error: 0x");
        char hex[3];
        sprintf(hex, "%02x", error);
        res.print(hex);
        res.print(" (");
        res.print(ErrorName(error).c_str());
        res.print(")</span>");
    }
    
    // Показываем форму снова с теми же параметрами
    res.print("<form method='post'><table>");
    res.print("<tr><td><label for='slave'>Slave ID</label></td>");
    res.print("<td><input type='number' min='0' max='247' id='slave' name='slave' value='");
    res.print(slaveId);
    res.print("'></td></tr>");
    
    res.print("<tr><td><label for='func'>Function</label></td>");
    res.print("<td><select id='func' name='func' data-value='");
    res.print(func);
    res.print("'>");
    res.print("<option value='1'>01 Read Coils</option>");
    res.print("<option value='2'>02 Read Discrete Inputs</option>");
    res.print("<option value='3'>03 Read Holding Register</option>");
    res.print("<option value='4'>04 Read Input Register</option>");
    res.print("</select></td></tr>");
    
    res.print("<tr><td><label for='reg'>Register</label></td>");
    res.print("<td><input type='number' min='0' max='65535' id='reg' name='reg' value='");
    res.print(reg);
    res.print("'></td></tr>");
    
    res.print("<tr><td><label for='count'>Count</label></td>");
    res.print("<td><input type='number' min='0' max='65535' id='count' name='count' value='");
    res.print(count);
    res.print("'></td></tr>");
    
    res.print("</table><button class='r'>Send</button></form><p></p>");
    
    res.print("<script>");
    res.print("(function(){");
    res.print("var s=document.querySelectorAll('select[data-value]');");
    res.print("for(d of s){");
    res.print("d.querySelector(`option[value='${d.dataset.value}']`).selected=true");
    res.print("}})();");
    res.print("</script>");
    
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

const String ErrorName(Modbus::Error code) {
    switch (code) {
        case Modbus::Error::SUCCESS: return "Success";
        case Modbus::Error::ILLEGAL_FUNCTION: return "Illegal function";
        case Modbus::Error::ILLEGAL_DATA_ADDRESS: return "Illegal data address";
        case Modbus::Error::ILLEGAL_DATA_VALUE: return "Illegal data value";
        case Modbus::Error::SERVER_DEVICE_FAILURE: return "Server device failure";
        case Modbus::Error::ACKNOWLEDGE: return "Acknowledge";
        case Modbus::Error::SERVER_DEVICE_BUSY: return "Server device busy";
        case Modbus::Error::NEGATIVE_ACKNOWLEDGE: return "Negative acknowledge";
        case Modbus::Error::MEMORY_PARITY_ERROR: return "Memory parity error";
        case Modbus::Error::GATEWAY_PATH_UNAVAIL: return "Gateway path unavailable";
        case Modbus::Error::GATEWAY_TARGET_NO_RESP: return "Gateway target no response";
        case Modbus::Error::TIMEOUT: return "Timeout";
        case Modbus::Error::INVALID_SERVER: return "Invalid server";
        case Modbus::Error::CRC_ERROR: return "CRC error";
        case Modbus::Error::FC_MISMATCH: return "Function code mismatch";
        case Modbus::Error::SERVER_ID_MISMATCH: return "Server id mismatch";
        case Modbus::Error::PACKET_LENGTH_ERROR: return "Packet length error";
        case Modbus::Error::PARAMETER_COUNT_ERROR: return "Parameter count error";
        case Modbus::Error::PARAMETER_LIMIT_ERROR: return "Parameter limit error";
        case Modbus::Error::REQUEST_QUEUE_FULL: return "Request queue full";
        case Modbus::Error::ILLEGAL_IP_OR_PORT: return "Illegal ip or port";
        case Modbus::Error::IP_CONNECTION_FAILED: return "IP connection failed";
        case Modbus::Error::TCP_HEAD_MISMATCH: return "TCP header mismatch";
        case Modbus::Error::EMPTY_MESSAGE: return "Empty message";
        case Modbus::Error::ASCII_FRAME_ERR: return "ASCII frame error";
        case Modbus::Error::ASCII_CRC_ERR: return "ASCII crc error";
        case Modbus::Error::ASCII_INVALID_CHAR: return "ASCII invalid character";
        default: return "undefined error";
    }
}

void EthernetWebUI::handleNetwork(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /network");
    sendHtmlHeader(res, "Network Config");
    
    res.print("<h3>Current Status</h3>");
    res.print("<table>");
    res.print("<tr><td>Current IP:</td><td>");
    res.print(Ethernet.localIP().toString().c_str());
    res.print("</td></tr>");
    
    res.print("<tr><td>Gateway:</td><td>");
    res.print(Ethernet.gatewayIP().toString().c_str());
    res.print("</td></tr>");
    
    res.print("<tr><td>Subnet:</td><td>");
    res.print(Ethernet.subnetMask().toString().c_str());
    res.print("</td></tr>");
    res.print("</table>");
    
    res.print("<h3>Network Configuration</h3>");
    res.print("<form method='post'>");
    res.print("<table>");
    
    res.print("<tr><td><label for='dhcp'>Use DHCP</label></td>");
    res.print("<td><input type='checkbox' id='dhcp' name='dhcp' value='1'");
    if (g_config->getUseDhcp()) res.print(" checked");
    res.print(" onchange='toggleStatic()'></td></tr>");
    
    res.print("</table>");
    
    res.print("<div id='static-config'");
    if (g_config->getUseDhcp()) res.print(" style='display:none'");
    res.print(">");
    res.print("<table>");
    
    // Если DHCP активен, показываем текущие настройки, иначе сохраненные
    IPAddress displayIp = g_config->getUseDhcp() ? Ethernet.localIP() : g_config->getStaticIp();
    IPAddress displayGw = g_config->getUseDhcp() ? Ethernet.gatewayIP() : g_config->getStaticGateway();
    IPAddress displaySn = g_config->getUseDhcp() ? Ethernet.subnetMask() : g_config->getStaticSubnet();
    IPAddress displayDns = g_config->getUseDhcp() ? Ethernet.gatewayIP() : g_config->getStaticDns();
    
    res.print("<tr><td><label for='ip'>Static IP</label></td>");
    res.print("<td><input type='text' id='ip' name='ip' value='");
    res.print(displayIp.toString().c_str());
    res.print("' placeholder='192.168.1.177'></td></tr>");
    
    res.print("<tr><td><label for='gw'>Gateway</label></td>");
    res.print("<td><input type='text' id='gw' name='gw' value='");
    res.print(displayGw.toString().c_str());
    res.print("' placeholder='192.168.1.1'></td></tr>");
    
    res.print("<tr><td><label for='sn'>Subnet</label></td>");
    res.print("<td><input type='text' id='sn' name='sn' value='");
    res.print(displaySn.toString().c_str());
    res.print("' placeholder='255.255.255.0'></td></tr>");
    
    res.print("<tr><td><label for='dns'>DNS</label></td>");
    res.print("<td><input type='text' id='dns' name='dns' value='");
    res.print(displayDns.toString().c_str());
    res.print("' placeholder='192.168.1.1'></td></tr>");
    
    res.print("</table></div>");
    
    res.print("<button class='r'>Save & Reboot</button></form><p></p>");
    res.print("<p class='e'>Note: Device will reboot after saving network settings</p>");
    
    res.print("<script>");
    res.print("function toggleStatic(){");
    res.print("var dhcp=document.getElementById('dhcp').checked;");
    res.print("var staticDiv=document.getElementById('static-config');");
    res.print("staticDiv.style.display=dhcp?'none':'block';");
    res.print("if(!dhcp){");
    // При переключении на статический IP, подставляем текущие значения
    res.print("document.getElementById('ip').value='");
    res.print(Ethernet.localIP().toString().c_str());
    res.print("';");
    res.print("document.getElementById('gw').value='");
    res.print(Ethernet.gatewayIP().toString().c_str());
    res.print("';");
    res.print("document.getElementById('sn').value='");
    res.print(Ethernet.subnetMask().toString().c_str());
    res.print("';");
    res.print("document.getElementById('dns').value='");
    res.print(Ethernet.gatewayIP().toString().c_str());
    res.print("';");
    res.print("}");
    res.print("}");
    res.print("</script>");
    
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleNetworkPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /network");
    char buffer[32];
    
    // Проверяем DHCP checkbox
    bool useDhcp = req.query("dhcp", buffer, sizeof(buffer));
    g_config->setUseDhcp(useDhcp);
    
    if (!useDhcp) {
        // Сохраняем статические настройки
        if (req.query("ip", buffer, sizeof(buffer))) {
            IPAddress ip;
            if (ip.fromString(buffer)) {
                g_config->setStaticIp(ip);
                dbgln("[webserver] saved static IP");
            }
        }
        
        if (req.query("gw", buffer, sizeof(buffer))) {
            IPAddress gw;
            if (gw.fromString(buffer)) {
                g_config->setStaticGateway(gw);
                dbgln("[webserver] saved gateway");
            }
        }
        
        if (req.query("sn", buffer, sizeof(buffer))) {
            IPAddress sn;
            if (sn.fromString(buffer)) {
                g_config->setStaticSubnet(sn);
                dbgln("[webserver] saved subnet");
            }
        }
        
        if (req.query("dns", buffer, sizeof(buffer))) {
            IPAddress dns;
            if (dns.fromString(buffer)) {
                g_config->setStaticDns(dns);
                dbgln("[webserver] saved DNS");
            }
        }
    }
    
    sendHtmlHeader(res, "Network Saved");
    res.print("<p>Network configuration saved!</p>");
    res.print("<p>Device will reboot in 3 seconds...</p>");
    sendHtmlFooter(res);
    
    delay(3000);
    ESP.restart();
}

void EthernetWebUI::handleReboot(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /reboot");
    sendHtmlHeader(res, "Reboot");
    sendButton(res, "Back", "/");
    res.print("<form method='post'><button class='r'>Yes, do it!</button></form>");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleRebootPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /reboot");
    res.print("Rebooting...");
    delay(1000);
    ESP.restart();
}

void EthernetWebUI::handleUpdate(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /update");
    sendHtmlHeader(res, "Firmware Update");
    
    res.print("<form method='post' enctype='multipart/form-data'>");
    res.print("<input type='file' name='file' required/><p></p>");
    res.print("<button class='r'>Upload</button></form><p></p>");
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleUpdatePost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /update - OTA");
    
    int contentLength = req.left();
    
    if (contentLength <= 0) {
        res.status(400);
        res.print("Error: No Content-Length");
        return;
    }
    
    dbg("[webserver] OTA size: ");
    dbgln(contentLength);
    
    if (!Update.begin(contentLength)) {
        res.status(500);
        res.print("Error: Not enough space");
        return;
    }
    
    // Читаем данные прошивки
    size_t written = 0;
    uint8_t buff[128];
    
    while (written < contentLength && req.available()) {
        int len = req.read(buff, sizeof(buff));
        if (len > 0) {
            Update.write(buff, len);
            written += len;
        }
    }
    
    if (Update.end()) {
        res.status(200);
        res.print("Update Success! Rebooting...");
        delay(1000);
        ESP.restart();
    } else {
        res.status(500);
        res.print("Update Failed");
    }
}

void EthernetWebUI::handleStyleCss(Request &req, Response &res) {
    res.set("Content-Type", "text/css");
    res.set("Connection", "close");
    res.print("body{font-family:sans-serif;text-align:center;background:#252525;color:#faffff;}");
    res.print("#content{display:inline-block;min-width:340px;}");
    res.print("button{width:100%;line-height:2.4rem;background:#1fa3ec;border:0;");
    res.print("border-radius:0.3rem;font-size:1.2rem;color:#faffff;cursor:pointer;}");
    res.print("button:hover{background:#0e70a4;}");
    res.print("button.r{background:#d43535;}button.r:hover{background:#931f1f;}");
    res.print("table{text-align:left;width:100%;margin:10px 0;}");
    res.print("input,select{width:100%;padding:5px;box-sizing:border-box;}");
    res.print(".e{color:red;}");
}

void EthernetWebUI::handleFavicon(Request &req, Response &res) {
    res.set("Connection", "close");
    res.status(204); // No Content
}
