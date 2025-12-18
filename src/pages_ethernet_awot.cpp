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
    app.get("/favicon.ico", &handleFavicon);
    
    server.begin();
    dbgln("[webserver] aWOT started on port 80");
}

void EthernetWebUI::loop() {
    EthernetClient client = server.available();
    if (client && client.connected()) {
        app.process(&client);
        // Даем время на отправку данных
        delay(5);
        client.stop();
    }
}

bool EthernetWebUI::checkAuth(Request &req, Response &res) {
    if (g_config->getWebPassword().equals("")) return true;
    
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
    res.set("Cache-Control", "no-cache");
    
    // Собираем весь заголовок в один вызов print
    String header = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    header += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    header += "<link rel='icon' href='data:,'><title>ESP32 Modbus Gateway - ";
    header += title;
    header += "</title><style>";
    header += "body{font-family:sans-serif;text-align:center;background:#252525;color:#faffff;}";
    header += "#content{display:inline-block;min-width:340px;}";
    header += "button{width:100%;line-height:2.4rem;background:#1fa3ec;border:0;";
    header += "border-radius:0.3rem;font-size:1.2rem;color:#faffff;cursor:pointer;}";
    header += "button:hover{background:#0e70a4;}";
    header += "button.r{background:#d43535;}button.r:hover{background:#931f1f;}";
    header += "table{text-align:left;width:100%;margin:10px 0;}";
    header += "input,select{width:100%;padding:5px;box-sizing:border-box;}";
    header += ".e{color:red;}pre{text-align:left;}";
    header += "</style></head><body><h2>ESP32 Modbus Gateway</h2><h3>";
    header += title;
    header += "</h3><div id='content'>";
    
    res.print(header);
}

void EthernetWebUI::sendHtmlFooter(Response &res) {
    res.print("</div></body></html>");
}

void EthernetWebUI::sendButton(Response &res, const char *title, const char *href, const char *cssClass) {
    String btn = "<form method='get' action='";
    btn += href;
    btn += "'><button class='";
    btn += cssClass;
    btn += "'>";
    btn += title;
    btn += "</button></form><p></p>";
    res.print(btn);
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
    
    // Собираем всю таблицу в один String
    String table = "<table>";
    table += "<tr><td>ESP Uptime (sec):</td><td>" + String(esp_timer_get_time() / 1000000) + "</td></tr>";
    table += "<tr><td>Network:</td><td>Ethernet (ENC28J60)</td></tr>";
    
    uint8_t mac[6];
    Ethernet.macAddress(mac);
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    table += "<tr><td>MAC:</td><td>" + String(macStr) + "</td></tr>";
    table += "<tr><td>IP:</td><td>" + Ethernet.localIP().toString() + "</td></tr>";
    table += "<tr><td>Gateway:</td><td>" + Ethernet.gatewayIP().toString() + "</td></tr>";
    table += "<tr><td>Subnet:</td><td>" + Ethernet.subnetMask().toString() + "</td></tr>";
    table += "<tr><td>RTU Messages:</td><td>" + String(g_rtu->getMessageCount()) + "</td></tr>";
    table += "<tr><td>RTU Pending:</td><td>" + String(g_rtu->pendingRequests()) + "</td></tr>";
    table += "<tr><td>RTU Errors:</td><td>" + String(g_rtu->getErrorCount()) + "</td></tr>";
    table += "<tr><td>Bridge Messages:</td><td>" + String(g_bridge->getMessageCount()) + "</td></tr>";
    table += "<tr><td>Bridge Clients:</td><td>" + String(g_bridge->activeClients()) + "</td></tr>";
    table += "<tr><td>Bridge Errors:</td><td>" + String(g_bridge->getErrorCount()) + "</td></tr>";
    table += "<tr><td>&nbsp;</td><td></td></tr>";
    table += "<tr><td>Build time:</td><td>" __DATE__ " " __TIME__ "</td></tr>";
    table += "</table><p></p>";
    
    res.print(table);
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleConfig(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /config");
    sendHtmlHeader(res, "Configuration");
    
    // Собираем всю форму в один String для минимизации вызовов print
    String form = "<form method='post'><h3>Modbus TCP</h3><table>";
    form += "<tr><td><label for='tp'>TCP Port</label></td>";
    form += "<td><input type='number' min='1' max='65535' id='tp' name='tp' value='" + String(g_config->getTcpPort()) + "'></td></tr>";
    form += "<tr><td><label for='tt'>TCP Timeout (ms)</label></td>";
    form += "<td><input type='number' min='1' id='tt' name='tt' value='" + String(g_config->getTcpTimeout()) + "'></td></tr>";
    form += "</table><h3>Modbus RTU</h3><table>";
    form += "<tr><td><label for='mb'>Baud rate</label></td>";
    form += "<td><input type='number' id='mb' name='mb' value='" + String(g_config->getModbusBaudRate()) + "'></td></tr>";
    form += "<tr><td><label for='md'>Data bits</label></td>";
    form += "<td><input type='number' min='5' max='8' id='md' name='md' value='" + String(g_config->getModbusDataBits()) + "'></td></tr>";
    form += "<tr><td><label for='mp'>Parity</label></td>";
    form += "<td><select id='mp' name='mp' data-value='" + String(g_config->getModbusParity()) + "'>";
    form += "<option value='0'>None</option><option value='2'>Even</option><option value='3'>Odd</option>";
    form += "</select></td></tr>";
    form += "<tr><td><label for='ms'>Stop bits</label></td>";
    form += "<td><select id='ms' name='ms' data-value='" + String(g_config->getModbusStopBits()) + "'>";
    form += "<option value='1'>1 bit</option><option value='2'>1.5 bits</option><option value='3'>2 bits</option>";
    form += "</select></td></tr>";
    
    res.print(form);
    
    String form2 = "<tr><td><label for='mr'>RTS Pin</label></td>";
    form2 += "<td><select id='mr' name='mr' data-value='" + String(g_config->getModbusRtsPin()) + "'>";
    form2 += "<option value='-1'>Auto</option><option value='4'>D4</option><option value='13'>D13</option>";
    form2 += "<option value='14'>D14</option><option value='18'>D18</option><option value='19'>D19</option>";
    form2 += "<option value='21'>D21</option><option value='22'>D22</option><option value='23'>D23</option>";
    form2 += "<option value='25'>D25</option><option value='26'>D26</option><option value='27'>D27</option>";
    form2 += "<option value='32'>D32</option><option value='33'>D33</option>";
    form2 += "</select></td></tr>";
    form2 += "</table><h3>Serial (Debug)</h3><table>";
    form2 += "<tr><td><label for='sb'>Baud rate</label></td>";
    form2 += "<td><input type='number' id='sb' name='sb' value='" + String(g_config->getSerialBaudRate()) + "'></td></tr>";
    form2 += "<tr><td><label for='sd'>Data bits</label></td>";
    form2 += "<td><input type='number' min='5' max='8' id='sd' name='sd' value='" + String(g_config->getSerialDataBits()) + "'></td></tr>";
    form2 += "<tr><td><label for='sp'>Parity</label></td>";
    form2 += "<td><select id='sp' name='sp' data-value='" + String(g_config->getSerialParity()) + "'>";
    form2 += "<option value='0'>None</option><option value='2'>Even</option><option value='3'>Odd</option>";
    form2 += "</select></td></tr>";
    form2 += "<tr><td><label for='ss'>Stop bits</label></td>";
    form2 += "<td><select id='ss' name='ss' data-value='" + String(g_config->getSerialStopBits()) + "'>";
    form2 += "<option value='1'>1 bit</option><option value='2'>1.5 bits</option><option value='3'>2 bits</option>";
    form2 += "</select></td></tr>";
    form2 += "</table><h3>Other</h3><table>";
    form2 += "<tr><td><label for='wp'>Web password</label></td>";
    form2 += "<td><input type='password' id='wp' name='wp' value='****'></td></tr>";
    form2 += "</table><button class='r'>Save</button></form><p></p>";
    
    res.print(form2);
    sendButton(res, "Back", "/");
    
    res.print("<script>(function(){var s=document.querySelectorAll('select[data-value]');");
    res.print("for(d of s){d.querySelector(`option[value='${d.dataset.value}']`).selected=true}})();</script>");
    
    sendHtmlFooter(res);
}

void EthernetWebUI::handleConfigPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /config");
    char buffer[32];
    
    if (req.query("tp", buffer, sizeof(buffer))) g_config->setTcpPort(atoi(buffer));
    if (req.query("tt", buffer, sizeof(buffer))) g_config->setTcpTimeout(atoi(buffer));
    if (req.query("mb", buffer, sizeof(buffer))) g_config->setModbusBaudRate(atol(buffer));
    if (req.query("md", buffer, sizeof(buffer))) g_config->setModbusDataBits(atoi(buffer));
    if (req.query("mp", buffer, sizeof(buffer))) g_config->setModbusParity(atoi(buffer));
    if (req.query("ms", buffer, sizeof(buffer))) g_config->setModbusStopBits(atoi(buffer));
    if (req.query("mr", buffer, sizeof(buffer))) g_config->setModbusRtsPin(atoi(buffer));
    if (req.query("sb", buffer, sizeof(buffer))) g_config->setSerialBaudRate(atol(buffer));
    if (req.query("sd", buffer, sizeof(buffer))) g_config->setSerialDataBits(atoi(buffer));
    if (req.query("sp", buffer, sizeof(buffer))) g_config->setSerialParity(atoi(buffer));
    if (req.query("ss", buffer, sizeof(buffer))) g_config->setSerialStopBits(atoi(buffer));
    if (req.query("wp", buffer, sizeof(buffer)) && strcmp(buffer, "****") != 0) {
        g_config->setWebPassword(String(buffer));
    }
    
    res.status(303);
    res.set("Location", "/");
    res.set("Connection", "close");
    res.print("<html><head><meta http-equiv='refresh' content='0;url=/'></head><body>Saved!</body></html>");
}

void EthernetWebUI::handleDebug(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /debug");
    sendHtmlHeader(res, "Debug");
    
    String form = "<form method='post'><table>";
    form += "<tr><td><label for='slave'>Slave ID</label></td>";
    form += "<td><input type='number' min='0' max='247' id='slave' name='slave' value='1'></td></tr>";
    form += "<tr><td><label for='func'>Function</label></td>";
    form += "<td><select id='func' name='func'>";
    form += "<option value='1'>01 Read Coils</option>";
    form += "<option value='2'>02 Read Discrete Inputs</option>";
    form += "<option value='3' selected>03 Read Holding Register</option>";
    form += "<option value='4'>04 Read Input Register</option>";
    form += "</select></td></tr>";
    form += "<tr><td><label for='reg'>Register</label></td>";
    form += "<td><input type='number' min='0' max='65535' id='reg' name='reg' value='1'></td></tr>";
    form += "<tr><td><label for='count'>Count</label></td>";
    form += "<td><input type='number' min='0' max='65535' id='count' name='count' value='1'></td></tr>";
    form += "</table><button class='r'>Send</button></form><p></p>";
    
    res.print(form);
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleDebugPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /debug");
    
    char buffer[32];
    String slaveId = "1", reg = "1", func = "3", count = "1";
    
    if (req.query("slave", buffer, sizeof(buffer))) slaveId = String(buffer);
    if (req.query("reg", buffer, sizeof(buffer))) reg = String(buffer);
    if (req.query("func", buffer, sizeof(buffer))) func = String(buffer);
    if (req.query("count", buffer, sizeof(buffer))) count = String(buffer);
    
    sendHtmlHeader(res, "Debug");
    res.print("<pre>");
    
    auto previousLevel = MBUlogLvl;
    MBUlogLvl = LOG_LEVEL_DEBUG;
    ModbusMessage answer = g_rtu->syncRequest(0xdeadbeef, slaveId.toInt(), func.toInt(), reg.toInt(), count.toInt());
    MBUlogLvl = previousLevel;
    
    res.print("</pre>");
    
    auto error = answer.getError();
    if (error == SUCCESS) {
        auto cnt = answer[2];
        String result = "<span>Answer: 0x";
        for (size_t i = 0; i < cnt; i++) {
            char hex[3];
            sprintf(hex, "%02x", answer[i + 3]);
            result += hex;
        }
        result += "</span>";
        res.print(result);
    } else {
        char hex[3];
        sprintf(hex, "%02x", error);
        String err = "<span class='e'>Error: 0x" + String(hex) + " (" + ErrorName(error) + ")</span>";
        res.print(err);
    }
    
    String form = "<form method='post'><table>";
    form += "<tr><td><label for='slave'>Slave ID</label></td>";
    form += "<td><input type='number' min='0' max='247' id='slave' name='slave' value='" + slaveId + "'></td></tr>";
    form += "<tr><td><label for='func'>Function</label></td>";
    form += "<td><select id='func' name='func' data-value='" + func + "'>";
    form += "<option value='1'>01 Read Coils</option>";
    form += "<option value='2'>02 Read Discrete Inputs</option>";
    form += "<option value='3'>03 Read Holding Register</option>";
    form += "<option value='4'>04 Read Input Register</option>";
    form += "</select></td></tr>";
    form += "<tr><td><label for='reg'>Register</label></td>";
    form += "<td><input type='number' min='0' max='65535' id='reg' name='reg' value='" + reg + "'></td></tr>";
    form += "<tr><td><label for='count'>Count</label></td>";
    form += "<td><input type='number' min='0' max='65535' id='count' name='count' value='" + count + "'></td></tr>";
    form += "</table><button class='r'>Send</button></form><p></p>";
    form += "<script>(function(){var s=document.querySelectorAll('select[data-value]');";
    form += "for(d of s){d.querySelector(`option[value='${d.dataset.value}']`).selected=true}})();</script>";
    
    res.print(form);
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleNetwork(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] GET /network");
    sendHtmlHeader(res, "Network Config");
    
    String page = "<h3>Current Status</h3><table>";
    page += "<tr><td>Current IP:</td><td>" + Ethernet.localIP().toString() + "</td></tr>";
    page += "<tr><td>Gateway:</td><td>" + Ethernet.gatewayIP().toString() + "</td></tr>";
    page += "<tr><td>Subnet:</td><td>" + Ethernet.subnetMask().toString() + "</td></tr>";
    page += "</table><h3>Network Configuration</h3>";
    page += "<form method='post'><table>";
    page += "<tr><td><label for='dhcp'>Use DHCP</label></td>";
    page += "<td><input type='checkbox' id='dhcp' name='dhcp' value='1'";
    if (g_config->getUseDhcp()) page += " checked";
    page += " onchange='toggleStatic()'></td></tr>";
    page += "</table><div id='static-config'";
    if (g_config->getUseDhcp()) page += " style='display:none'";
    page += "><table>";
    
    IPAddress displayIp = g_config->getUseDhcp() ? Ethernet.localIP() : g_config->getStaticIp();
    IPAddress displayGw = g_config->getUseDhcp() ? Ethernet.gatewayIP() : g_config->getStaticGateway();
    IPAddress displaySn = g_config->getUseDhcp() ? Ethernet.subnetMask() : g_config->getStaticSubnet();
    IPAddress displayDns = g_config->getUseDhcp() ? Ethernet.gatewayIP() : g_config->getStaticDns();
    
    page += "<tr><td><label for='ip'>Static IP</label></td>";
    page += "<td><input type='text' id='ip' name='ip' value='" + displayIp.toString() + "'></td></tr>";
    page += "<tr><td><label for='gw'>Gateway</label></td>";
    page += "<td><input type='text' id='gw' name='gw' value='" + displayGw.toString() + "'></td></tr>";
    page += "<tr><td><label for='sn'>Subnet</label></td>";
    page += "<td><input type='text' id='sn' name='sn' value='" + displaySn.toString() + "'></td></tr>";
    page += "<tr><td><label for='dns'>DNS</label></td>";
    page += "<td><input type='text' id='dns' name='dns' value='" + displayDns.toString() + "'></td></tr>";
    page += "</table></div>";
    page += "<button class='r'>Save & Reboot</button></form><p></p>";
    page += "<p class='e'>Note: Device will reboot after saving network settings</p>";
    
    res.print(page);
    
    String script = "<script>function toggleStatic(){var dhcp=document.getElementById('dhcp').checked;";
    script += "var staticDiv=document.getElementById('static-config');staticDiv.style.display=dhcp?'none':'block';";
    script += "if(!dhcp){document.getElementById('ip').value='" + Ethernet.localIP().toString() + "';";
    script += "document.getElementById('gw').value='" + Ethernet.gatewayIP().toString() + "';";
    script += "document.getElementById('sn').value='" + Ethernet.subnetMask().toString() + "';";
    script += "document.getElementById('dns').value='" + Ethernet.gatewayIP().toString() + "';}}</script>";
    
    res.print(script);
    sendButton(res, "Back", "/");
    sendHtmlFooter(res);
}

void EthernetWebUI::handleNetworkPost(Request &req, Response &res) {
    if (!checkAuth(req, res)) return;
    
    dbgln("[webserver] POST /network");
    char buffer[32];
    
    bool useDhcp = req.query("dhcp", buffer, sizeof(buffer));
    g_config->setUseDhcp(useDhcp);
    
    if (!useDhcp) {
        if (req.query("ip", buffer, sizeof(buffer))) {
            IPAddress ip;
            if (ip.fromString(buffer)) g_config->setStaticIp(ip);
        }
        if (req.query("gw", buffer, sizeof(buffer))) {
            IPAddress gw;
            if (gw.fromString(buffer)) g_config->setStaticGateway(gw);
        }
        if (req.query("sn", buffer, sizeof(buffer))) {
            IPAddress sn;
            if (sn.fromString(buffer)) g_config->setStaticSubnet(sn);
        }
        if (req.query("dns", buffer, sizeof(buffer))) {
            IPAddress dns;
            if (dns.fromString(buffer)) g_config->setStaticDns(dns);
        }
    }
    
    sendHtmlHeader(res, "Network Saved");
    res.print("<p>Network configuration saved!</p><p>Device will reboot in 3 seconds...</p>");
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

void EthernetWebUI::handleFavicon(Request &req, Response &res) {
    res.set("Connection", "close");
    res.status(204);
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
