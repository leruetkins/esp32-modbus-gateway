#include "pages_ethernet_awot.h"
#include <Update.h>

// Статические члены класса
EthernetServer EthernetWebUI::server(80);
Application EthernetWebUI::app;
ModbusClientRTU *EthernetWebUI::g_rtu = nullptr;
ModbusBridgeEthernet *EthernetWebUI::g_bridge = nullptr;
Config *EthernetWebUI::g_config = nullptr;

// CSS стили (встроенные для избежания дополнительного запроса)
const char* CSS_STYLE = 
    "<style>"
    "body{"
        "font-family:sans-serif;"
        "text-align:center;"
        "background:#252525;"
        "color:#faffff;"
    "}"
    "#c{"
        "display:inline-block;"
        "min-width:340px;"
    "}"
    "button{"
        "width:100%;"
        "line-height:2.4rem;"
        "background:#1fa3ec;"
        "border:0;"
        "border-radius:0.3rem;"
        "font-size:1.2rem;"
        "-webkit-transition-duration:0.4s;"
        "transition-duration:0.4s;"
        "color:#faffff;"
        "cursor:pointer;"
    "}"
    "button:hover{"
        "background:#0e70a4;"
    "}"
    "button.r{"
        "background:#d43535;"
    "}"
    "button.r:hover{"
        "background:#931f1f;"
    "}"
    "table{"
        "text-align:left;"
        "width:100%;"
    "}"
    "input{"
        "width:100%;"
    "}"
    ".e{"
        "color:red;"
    "}"
    "pre{"
        "text-align:left;"
    "}"
    "</style>";

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
    app.get("/favicon.ico", &handleFavicon);
    
    server.begin();
    dbgln("[webserver] aWOT started on port 80");
}

void EthernetWebUI::loop() {
    EthernetClient client = server.available();
    if (client) {
        // Ждем данных с коротким таймаутом
        unsigned long timeout = millis() + 500;
        while (!client.available() && millis() < timeout) {
            yield(); // Даем время другим задачам
        }
        
        if (client.available()) {
            app.process(&client);
        }
        
        client.stop();
    }
}

bool EthernetWebUI::checkAuth(Request &req, Response &res) {
    if (g_config->getWebPassword().equals("")) return true;
    
    char *auth = req.get("Authorization");
    if (!auth) {
        res.set("WWW-Authenticate", "Basic realm=\"ModbusGW\"");
        res.status(401);
        res.print("Auth Required");
        return false;
    }
    return true;
}

// Вспомогательные функции для генерации HTML
String EthernetWebUI::htmlHeader(const char *title) {
    String h;
    h.reserve(1024); // Резервируем память сразу
    h += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    h += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    h += F("<meta http-equiv='x-dns-prefetch-control' content='off'>");
    h += F("<link rel='icon' href='data:,'><title>GW: ");
    h += title;
    h += F("</title>");
    h += CSS_STYLE;
    h += F("</head><body><h2>");
    h += title;
    h += F("</h2><div id='c'>");
    return h;
}

String EthernetWebUI::htmlFooter() {
    return F("</div></body></html>");
}

String EthernetWebUI::button(const char *title, const char *href, const char *cssClass) {
    String b = F("<form method='get' action='");
    b += href;
    b += F("'><button class='");
    b += cssClass;
    b += F("'>");
    b += title;
    b += F("</button></form><p></p>");
    return b;
}

void EthernetWebUI::handleRoot(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /");
    
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    
    res.print(htmlHeader("Main Menu"));
    res.print(button("Status", "/status", ""));
    res.print(button("Configuration", "/config", ""));
    res.print(button("Debug Tool", "/debug", ""));
    res.print(button("Network", "/network", ""));
    res.print(button("Firmware Update", "/update", ""));
    res.print(button("Reboot Device", "/reboot", "r"));
    res.print(htmlFooter());
}

void EthernetWebUI::handleStatus(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /status");
    
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    
    // Отправляем заголовок
    res.print(htmlHeader("System Status"));
    res.print(F("<table>"));
    
    // Отправляем каждую строку отдельно, используя char буфер
    char buf[150];
    
    sprintf(buf, "<tr><td>Uptime:</td><td>%lu s</td></tr>", (unsigned long)(esp_timer_get_time() / 1000000));
    res.print(buf);
    
    uint8_t mac[6];
    Ethernet.macAddress(mac);
    sprintf(buf, "<tr><td>MAC:</td><td>%02X:%02X:%02X:%02X:%02X:%02X</td></tr>", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    res.print(buf);
    
    sprintf(buf, "<tr><td>IP:</td><td>%s</td></tr>", Ethernet.localIP().toString().c_str());
    res.print(buf);
    
    sprintf(buf, "<tr><td>Gateway:</td><td>%s</td></tr>", Ethernet.gatewayIP().toString().c_str());
    res.print(buf);
    
    sprintf(buf, "<tr><td>Subnet:</td><td>%s</td></tr>", Ethernet.subnetMask().toString().c_str());
    res.print(buf);
    
    sprintf(buf, "<tr><td>RTU Messages:</td><td>%u</td></tr>", g_rtu->getMessageCount());
    res.print(buf);
    
    sprintf(buf, "<tr><td>RTU Pending:</td><td>%u</td></tr>", g_rtu->pendingRequests());
    res.print(buf);
    
    sprintf(buf, "<tr><td>RTU Errors:</td><td>%u</td></tr>", g_rtu->getErrorCount());
    res.print(buf);
    
    sprintf(buf, "<tr><td>TCP Messages:</td><td>%u</td></tr>", g_bridge->getMessageCount());
    res.print(buf);
    
    sprintf(buf, "<tr><td>TCP Active:</td><td>%u</td></tr>", g_bridge->activeClients());
    res.print(buf);
    
    sprintf(buf, "<tr><td>TCP Errors:</td><td>%u</td></tr>", g_bridge->getErrorCount());
    res.print(buf);
    
    sprintf(buf, "<tr><td>RAM Free:</td><td>%u bytes</td></tr>", ESP.getFreeHeap());
    res.print(buf);
    
    sprintf(buf, "<tr><td>Build:</td><td>%s %s</td></tr>", __DATE__, __TIME__);
    res.print(buf);
    
    res.print(F("</table><p></p>"));
    res.print(button("Back", "/", ""));
    res.print(htmlFooter());
}

void EthernetWebUI::handleConfig(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /config");
    
    String page = htmlHeader("Configuration");
    page += F("<form method='post'>");
    
    page += F("<h3>Modbus TCP</h3><table>");
    page += "<tr><td>TCP Port:</td><td><input type='number' name='tp' min='1' max='65535' value='" + String(g_config->getTcpPort()) + "'></td></tr>";
    page += "<tr><td>Timeout (ms):</td><td><input type='number' name='tt' min='1' value='" + String(g_config->getTcpTimeout()) + "'></td></tr>";
    page += F("</table>");
    
    page += F("<h3>Modbus RTU</h3><table>");
    page += "<tr><td>Baud Rate:</td><td><input type='number' name='mb' value='" + String(g_config->getModbusBaudRate()) + "'></td></tr>";
    page += "<tr><td>Data Bits:</td><td><input type='number' name='md' min='5' max='8' value='" + String(g_config->getModbusDataBits()) + "'></td></tr>";
    page += F("<tr><td>Parity:</td><td><select name='mp' id='mp'>");
    page += F("<option value='0'>None</option><option value='2'>Even</option><option value='3'>Odd</option>");
    page += F("</select></td></tr>");
    page += F("<tr><td>Stop Bits:</td><td><select name='ms' id='ms'>");
    page += F("<option value='1'>1</option><option value='2'>1.5</option><option value='3'>2</option>");
    page += F("</select></td></tr>");
    page += F("<tr><td>RTS Pin:</td><td><select name='mr' id='mr'>");
    page += F("<option value='-1'>Auto</option><option value='4'>GPIO4</option><option value='13'>GPIO13</option>");
    page += F("<option value='14'>GPIO14</option><option value='18'>GPIO18</option><option value='19'>GPIO19</option>");
    page += F("<option value='21'>GPIO21</option><option value='22'>GPIO22</option><option value='23'>GPIO23</option>");
    page += F("<option value='25'>GPIO25</option><option value='26'>GPIO26</option><option value='27'>GPIO27</option>");
    page += F("<option value='32'>GPIO32</option><option value='33'>GPIO33</option>");
    page += F("</select></td></tr>");
    page += F("</table>");
    
    page += F("<h3>Serial Debug</h3><table>");
    page += "<tr><td>Baud Rate:</td><td><input type='number' name='sb' value='" + String(g_config->getSerialBaudRate()) + "'></td></tr>";
    page += "<tr><td>Data Bits:</td><td><input type='number' name='sd' min='5' max='8' value='" + String(g_config->getSerialDataBits()) + "'></td></tr>";
    page += F("<tr><td>Parity:</td><td><select name='sp' id='sp'>");
    page += F("<option value='0'>None</option><option value='2'>Even</option><option value='3'>Odd</option>");
    page += F("</select></td></tr>");
    page += F("<tr><td>Stop Bits:</td><td><select name='ss' id='ss'>");
    page += F("<option value='1'>1</option><option value='2'>1.5</option><option value='3'>2</option>");
    page += F("</select></td></tr>");
    page += F("</table>");
    
    page += F("<h3>Web Interface</h3><table>");
    page += F("<tr><td>Password:</td><td><input type='password' name='wp' placeholder='Leave empty for no auth'></td></tr>");
    page += F("</table>");
    
    page += F("<button class='r'>Save Configuration</button></form><p></p>");
    page += button("Back", "/", "");
    
    // JavaScript для установки значений select
    page += "<script>";
    page += "document.getElementById('mp').value='" + String(g_config->getModbusParity()) + "';";
    page += "document.getElementById('ms').value='" + String(g_config->getModbusStopBits()) + "';";
    page += "document.getElementById('mr').value='" + String(g_config->getModbusRtsPin()) + "';";
    page += "document.getElementById('sp').value='" + String(g_config->getSerialParity()) + "';";
    page += "document.getElementById('ss').value='" + String(g_config->getSerialStopBits()) + "';";
    page += "</script>";
    
    page += htmlFooter();
    
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    res.print(page);
}

void EthernetWebUI::handleConfigPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /config");
    
    char buf[32];
    if (req.query("tp", buf, sizeof(buf))) g_config->setTcpPort(atoi(buf));
    if (req.query("tt", buf, sizeof(buf))) g_config->setTcpTimeout(atoi(buf));
    if (req.query("mb", buf, sizeof(buf))) g_config->setModbusBaudRate(atol(buf));
    if (req.query("md", buf, sizeof(buf))) g_config->setModbusDataBits(atoi(buf));
    if (req.query("mp", buf, sizeof(buf))) g_config->setModbusParity(atoi(buf));
    if (req.query("ms", buf, sizeof(buf))) g_config->setModbusStopBits(atoi(buf));
    if (req.query("mr", buf, sizeof(buf))) g_config->setModbusRtsPin(atoi(buf));
    if (req.query("sb", buf, sizeof(buf))) g_config->setSerialBaudRate(atol(buf));
    if (req.query("sd", buf, sizeof(buf))) g_config->setSerialDataBits(atoi(buf));
    if (req.query("sp", buf, sizeof(buf))) g_config->setSerialParity(atoi(buf));
    if (req.query("ss", buf, sizeof(buf))) g_config->setSerialStopBits(atoi(buf));
    if (req.query("wp", buf, sizeof(buf)) && strlen(buf) > 0) {
        g_config->setWebPassword(String(buf));
    }
    
    res.set("Location", "/");
    res.status(303);
    res.set("Connection", "close");
    res.print("Saved");
}

void EthernetWebUI::handleDebug(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /debug");
    
    String page = htmlHeader("Debug Tool");
    page += F("<form method='post'><table>");
    page += F("<tr><td>Slave ID:</td><td><input type='number' name='id' min='1' max='247' value='1'></td></tr>");
    page += F("<tr><td>Function:</td><td><select name='fc'>");
    page += F("<option value='1'>01 - Read Coils</option>");
    page += F("<option value='2'>02 - Read Discrete Inputs</option>");
    page += F("<option value='3' selected>03 - Read Holding Registers</option>");
    page += F("<option value='4'>04 - Read Input Registers</option>");
    page += F("</select></td></tr>");
    page += F("<tr><td>Address:</td><td><input type='number' name='ad' min='0' max='65535' value='0'></td></tr>");
    page += F("<tr><td>Count:</td><td><input type='number' name='cn' min='1' max='125' value='1'></td></tr>");
    page += F("</table><button class='r'>Send Request</button></form><p></p>");
    
    page += button("Back", "/", "");
    page += htmlFooter();
    
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    res.print(page);
}

void EthernetWebUI::handleDebugPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /debug");
    
    char buf[16];
    int id = 1, fc = 3, ad = 0, cn = 1;
    if (req.query("id", buf, sizeof(buf))) id = atoi(buf);
    if (req.query("fc", buf, sizeof(buf))) fc = atoi(buf);
    if (req.query("ad", buf, sizeof(buf))) ad = atoi(buf);
    if (req.query("cn", buf, sizeof(buf))) cn = atoi(buf);
    
    String page = htmlHeader("Debug Result");
    
    // Выполнение Modbus запроса
    auto previousLevel = MBUlogLvl;
    MBUlogLvl = LOG_LEVEL_DEBUG;
    ModbusMessage response = g_rtu->syncRequest(0xDEADBEEF, id, fc, ad, cn);
    MBUlogLvl = previousLevel;
    
    Modbus::Error err = response.getError();
    
    if (err == Modbus::Error::SUCCESS) {
        page += F("<h3 style='color:#5f5'>✓ SUCCESS</h3>");
        page += "<table><tr><td>Slave ID:</td><td>" + String(id) + "</td></tr>";
        page += "<tr><td>Function:</td><td>" + String(fc) + "</td></tr>";
        page += "<tr><td>Address:</td><td>" + String(ad) + "</td></tr>";
        page += "<tr><td>Count:</td><td>" + String(cn) + "</td></tr></table>";
        
        page += F("<h4>Response Data (HEX):</h4><pre>");
        for (size_t i = 3; i < response.size(); ++i) {
            if (response[i] < 0x10) page += "0";
            page += String(response[i], HEX);
            page += " ";
            if ((i - 2) % 16 == 0) page += "\n";
        }
        page += F("</pre>");
    } else {
        page += F("<h3 class='e'>✗ ERROR</h3>");
        page += "<table><tr><td>Error Code:</td><td>" + String((int)err) + "</td></tr>";
        page += "<tr><td>Description:</td><td>" + ErrorName(err) + "</td></tr></table>";
    }
    
    page += button("Try Again", "/debug", "");
    page += button("Back", "/", "");
    page += htmlFooter();
    
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    res.print(page);
}

void EthernetWebUI::handleNetwork(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /network");
    
    String page = htmlHeader("Network Config");
    
    page += F("<h3>Current Status</h3><table>");
    page += "<tr><td>IP Address:</td><td>" + Ethernet.localIP().toString() + "</td></tr>";
    page += "<tr><td>Gateway:</td><td>" + Ethernet.gatewayIP().toString() + "</td></tr>";
    page += "<tr><td>Subnet:</td><td>" + Ethernet.subnetMask().toString() + "</td></tr>";
    page += F("</table>");
    
    page += F("<h3>Configuration</h3>");
    page += F("<form method='post'><table>");
    page += F("<tr><td>Use DHCP:</td><td><input type='checkbox' name='dhcp' id='dhcp' value='1'");
    if (g_config->getUseDhcp()) page += F(" checked");
    page += F(" onchange='toggleStatic()'></td></tr>");
    page += F("</table>");
    
    page += F("<div id='static' style='display:");
    page += g_config->getUseDhcp() ? F("none") : F("block");
    page += F("'><table>");
    
    IPAddress displayIp = g_config->getUseDhcp() ? Ethernet.localIP() : g_config->getStaticIp();
    IPAddress displayGw = g_config->getUseDhcp() ? Ethernet.gatewayIP() : g_config->getStaticGateway();
    IPAddress displaySn = g_config->getUseDhcp() ? Ethernet.subnetMask() : g_config->getStaticSubnet();
    IPAddress displayDns = g_config->getUseDhcp() ? Ethernet.gatewayIP() : g_config->getStaticDns();
    
    page += "<tr><td>Static IP:</td><td><input type='text' name='ip' id='ip' value='" + displayIp.toString() + "'></td></tr>";
    page += "<tr><td>Gateway:</td><td><input type='text' name='gw' id='gw' value='" + displayGw.toString() + "'></td></tr>";
    page += "<tr><td>Subnet:</td><td><input type='text' name='sn' id='sn' value='" + displaySn.toString() + "'></td></tr>";
    page += "<tr><td>DNS:</td><td><input type='text' name='dns' id='dns' value='" + displayDns.toString() + "'></td></tr>";
    page += F("</table></div>");
    
    page += F("<button class='r'>Save & Reboot</button></form><p></p>");
    page += F("<p class='e'>⚠ Device will reboot after saving</p>");
    
    page += button("Back", "/", "");
    
    // JavaScript для переключения статических настроек
    page += F("<script>");
    page += F("function toggleStatic(){");
    page += F("var dhcp=document.getElementById('dhcp').checked;");
    page += F("document.getElementById('static').style.display=dhcp?'none':'block';");
    page += F("if(!dhcp){");
    page += "document.getElementById('ip').value='" + Ethernet.localIP().toString() + "';";
    page += "document.getElementById('gw').value='" + Ethernet.gatewayIP().toString() + "';";
    page += "document.getElementById('sn').value='" + Ethernet.subnetMask().toString() + "';";
    page += "document.getElementById('dns').value='" + Ethernet.gatewayIP().toString() + "';";
    page += F("}}");
    page += F("</script>");
    
    page += htmlFooter();
    
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    res.print(page);
}

void EthernetWebUI::handleNetworkPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /network");
    
    char buf[32];
    bool useDhcp = req.query("dhcp", buf, sizeof(buf));
    g_config->setUseDhcp(useDhcp);
    
    if (!useDhcp) {
        if (req.query("ip", buf, sizeof(buf))) {
            IPAddress ip;
            if (ip.fromString(buf)) g_config->setStaticIp(ip);
        }
        if (req.query("gw", buf, sizeof(buf))) {
            IPAddress gw;
            if (gw.fromString(buf)) g_config->setStaticGateway(gw);
        }
        if (req.query("sn", buf, sizeof(buf))) {
            IPAddress sn;
            if (sn.fromString(buf)) g_config->setStaticSubnet(sn);
        }
        if (req.query("dns", buf, sizeof(buf))) {
            IPAddress dns;
            if (dns.fromString(buf)) g_config->setStaticDns(dns);
        }
    }
    
    res.set("Connection", "close");
    res.print("Saved. Rebooting...");
    delay(1000);
    ESP.restart();
}

void EthernetWebUI::handleReboot(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /reboot");
    
    String page = htmlHeader("Reboot Device");
    page += F("<p>Are you sure you want to reboot the device?</p>");
    page += F("<form method='post'><button class='r'>⚠ CONFIRM REBOOT</button></form><p></p>");
    page += button("Cancel", "/", "");
    page += htmlFooter();
    
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    res.print(page);
}

void EthernetWebUI::handleRebootPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /reboot");
    
    res.set("Connection", "close");
    res.print("Rebooting...");
    delay(1000);
    ESP.restart();
}

void EthernetWebUI::handleUpdate(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /update");
    
    String page = htmlHeader("Firmware Update");
    page += F("<p class='e'>⚠ Warning: Do not disconnect power during update!</p>");
    page += F("<form method='post' enctype='multipart/form-data'>");
    page += F("<input type='file' name='firmware' accept='.bin'><br><br>");
    page += F("<button class='r'>Upload Firmware</button></form><p></p>");
    page += button("Back", "/", "");
    page += htmlFooter();
    
    res.set("Content-Type", "text/html; charset=utf-8");
    res.set("Connection", "close");
    res.print(page);
}

void EthernetWebUI::handleUpdatePost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /update - OTA");
    
    // Здесь НЕ используем буферизацию String - файл слишком большой
    // Используем потоковое чтение
    int contentLength = req.left();
    
    if (contentLength <= 0) {
        res.status(400);
        res.set("Connection", "close");
        res.print("Error: No content");
        return;
    }
    
    dbg("[webserver] OTA size: ");
    dbgln(contentLength);
    
    if (!Update.begin(contentLength)) {
        res.status(500);
        res.set("Connection", "close");
        res.print("Error: Not enough space");
        Update.printError(Serial);
        return;
    }
    
    // Потоковое чтение и запись прошивки
    uint8_t buff[128];
    size_t written = 0;
    
    while (written < contentLength && req.available()) {
        int len = req.read(buff, sizeof(buff));
        if (len > 0) {
            if (Update.write(buff, len) != len) {
                res.status(500);
                res.set("Connection", "close");
                res.print("Error: Write failed");
                Update.printError(Serial);
                return;
            }
            written += len;
        }
    }
    
    if (Update.end(true)) {
        dbgln("[webserver] OTA success");
        res.set("Connection", "close");
        res.print("Update successful! Rebooting...");
        delay(1000);
        ESP.restart();
    } else {
        res.status(500);
        res.set("Connection", "close");
        res.print("Error: Update failed");
        Update.printError(Serial);
    }
}

void EthernetWebUI::handleFavicon(Request &req, Response &res) {
    res.status(204); // No content
    res.set("Connection", "close");
}

// Функция декодирования ошибок Modbus
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
        default: return "Unknown error";
    }
}
