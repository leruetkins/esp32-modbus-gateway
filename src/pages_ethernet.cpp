#include "pages_ethernet.h"
#define ETAG "\"" __DATE__ "" __TIME__ "\""
#define WEB_PASS_PLACEHOLDER "****"

// Глобальные переменные для доступа в обработчиках
static EthernetWebServer *g_server = nullptr;
static ModbusClientRTU *g_rtu = nullptr;
static ModbusBridgeEthernet *g_bridge = nullptr;
static Config *g_config = nullptr;

bool checkAuth() {
  if (g_config->getWebPassword().equals("")) return true;
  if (!g_server->authenticate("admin", g_config->getWebPassword().c_str())) {
    g_server->requestAuthentication();
    return false;
  }
  return true;
}

void handleRoot() {
  if (!checkAuth()) return;
  dbgln("[webserver] GET /");
  String html = getResponseHeader("Main");
  html += getButton("Status", "status");
  html += getButton("Config", "config");
  html += getButton("Debug", "debug");
  html += getButton("Firmware update", "update");
  html += getButton("Network config", "network");
  html += getButton("Reboot", "reboot", "r");
  html += getResponseTrailer();
  g_server->send(200, "text/html", html);
}

void handleStatus() {
  if (!checkAuth()) return;
  dbgln("[webserver] GET /status");
  String html = getResponseHeader("Status");
  html += "<table>";
  html += getTableRow("ESP Uptime (sec)", esp_timer_get_time() / 1000000);
  html += getTableRow("Network", "Ethernet (ENC28J60)");
  
  uint8_t mac[6];
  Ethernet.macAddress(mac);
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  html += getTableRow("MAC", String(macStr));
  
  html += getTableRow("IP", Ethernet.localIP().toString());
  html += getTableRow("Gateway", Ethernet.gatewayIP().toString());
  html += getTableRow("Subnet", Ethernet.subnetMask().toString());
  html += getTableRow("RTU Messages", g_rtu->getMessageCount());
  html += getTableRow("RTU Pending Messages", g_rtu->pendingRequests());
  html += getTableRow("RTU Errors", g_rtu->getErrorCount());
  html += getTableRow("Bridge Message", g_bridge->getMessageCount());
  html += getTableRow("Bridge Clients", g_bridge->activeClients());
  html += getTableRow("Bridge Errors", g_bridge->getErrorCount());
  html += "<tr><td>&nbsp;</td><td></td></tr>";
  html += getTableRow("Build time", String(__DATE__) + " " + String(__TIME__));
  html += "</table><p></p>";
  html += getButton("Back", "/");
  html += getResponseTrailer();
  g_server->send(200, "text/html", html);
}

void handleRebootGet() {
  if (!checkAuth()) return;
  dbgln("[webserver] GET /reboot");
  String html = getResponseHeader("Really?");
  html += getButton("Back", "/");
  html += "<form method=\"post\"><button class=\"r\">Yes, do it!</button></form>";
  html += getResponseTrailer();
  g_server->send(200, "text/html", html);
}

void handleRebootPost() {
  if (!checkAuth()) return;
  dbgln("[webserver] POST /reboot");
  g_server->sendHeader("Location", "/");
  g_server->send(303);
  delay(1000);
  dbgln("[webserver] rebooting...");
  ESP.restart();
}

void handleConfigGet() {
  if (!checkAuth()) return;
  dbgln("[webserver] GET /config");
  String html = getResponseHeader("Modbus TCP");
  html += "<form method=\"post\"><table>";
  html += "<tr><td><label for=\"tp\">TCP Port</label></td>";
  html += "<td><input type=\"number\" min=\"1\" max=\"65535\" id=\"tp\" name=\"tp\" value=\"" + String(g_config->getTcpPort()) + "\"></td></tr>";
  html += "<tr><td><label for=\"tt\">TCP Timeout (ms)</label></td>";
  html += "<td><input type=\"number\" min=\"1\" id=\"tt\" name=\"tt\" value=\"" + String(g_config->getTcpTimeout()) + "\"></td></tr>";
  html += "</table><h3>Modbus RTU</h3><table>";
  html += "<tr><td><label for=\"mb\">Baud rate</label></td>";
  html += "<td><input type=\"number\" min=\"0\" id=\"mb\" name=\"mb\" value=\"" + String(g_config->getModbusBaudRate()) + "\"></td></tr>";
  html += "<tr><td><label for=\"md\">Data bits</label></td>";
  html += "<td><input type=\"number\" min=\"5\" max=\"8\" id=\"md\" name=\"md\" value=\"" + String(g_config->getModbusDataBits()) + "\"></td></tr>";
  html += "<tr><td><label for=\"mp\">Parity</label></td>";
  html += "<td><select id=\"mp\" name=\"mp\" data-value=\"" + String(g_config->getModbusParity()) + "\">";
  html += "<option value=\"0\">None</option><option value=\"2\">Even</option><option value=\"3\">Odd</option></select></td></tr>";
  html += "<tr><td><label for=\"ms\">Stop bits</label></td>";
  html += "<td><select id=\"ms\" name=\"ms\" data-value=\"" + String(g_config->getModbusStopBits()) + "\">";
  html += "<option value=\"1\">1 bit</option><option value=\"2\">1.5 bits</option><option value=\"3\">2 bits</option></select></td></tr>";
  html += "<tr><td><label for=\"mr\">RTS Pin</label></td>";
  html += "<td><select id=\"mr\" name=\"mr\" data-value=\"" + String(g_config->getModbusRtsPin()) + "\">";
  html += "<option value=\"-1\">Auto</option><option value=\"4\">D4</option><option value=\"13\">D13</option>";
  html += "<option value=\"14\">D14</option><option value=\"18\">D18</option><option value=\"19\">D19</option>";
  html += "<option value=\"21\">D21</option><option value=\"22\">D22</option><option value=\"23\">D23</option>";
  html += "<option value=\"25\">D25</option><option value=\"26\">D26</option><option value=\"27\">D27</option>";
  html += "<option value=\"32\">D32</option><option value=\"33\">D33</option></select></td></tr>";
  html += "</table><h3>Serial (Debug)</h3><table>";
  html += "<tr><td><label for=\"sb\">Baud rate</label></td>";
  html += "<td><input type=\"number\" min=\"0\" id=\"sb\" name=\"sb\" value=\"" + String(g_config->getSerialBaudRate()) + "\"></td></tr>";
  html += "<tr><td><label for=\"sd\">Data bits</label></td>";
  html += "<td><input type=\"number\" min=\"5\" max=\"8\" id=\"sd\" name=\"sd\" value=\"" + String(g_config->getSerialDataBits()) + "\"></td></tr>";
  html += "<tr><td><label for=\"sp\">Parity</label></td>";
  html += "<td><select id=\"sp\" name=\"sp\" data-value=\"" + String(g_config->getSerialParity()) + "\">";
  html += "<option value=\"0\">None</option><option value=\"2\">Even</option><option value=\"3\">Odd</option></select></td></tr>";
  html += "<tr><td><label for=\"ss\">Stop bits</label></td>";
  html += "<td><select id=\"ss\" name=\"ss\" data-value=\"" + String(g_config->getSerialStopBits()) + "\">";
  html += "<option value=\"1\">1 bit</option><option value=\"2\">1.5 bits</option><option value=\"3\">2 bits</option></select></td></tr>";
  html += "</table><h3>Other</h3><table>";
  html += "<tr><td><label for=\"wp\">Web password</label></td>";
  html += "<td><input type=\"password\" id=\"wp\" name=\"wp\" value=\"" + String(WEB_PASS_PLACEHOLDER) + "\"></td></tr>";
  html += "</table><button class=\"r\">Save</button></form><p></p>";
  html += getButton("Back", "/");
  html += "<script>(function(){var s=document.querySelectorAll('select[data-value]');";
  html += "for(d of s){d.querySelector(`option[value='${d.dataset.value}']`).selected=true}})();</script>";
  html += getResponseTrailer();
  g_server->send(200, "text/html", html);
}

void handleConfigPost() {
  if (!checkAuth()) return;
  dbgln("[webserver] POST /config");
  if (g_server->hasArg("tp")) g_config->setTcpPort(g_server->arg("tp").toInt());
  if (g_server->hasArg("tt")) g_config->setTcpTimeout(g_server->arg("tt").toInt());
  if (g_server->hasArg("mb")) g_config->setModbusBaudRate(g_server->arg("mb").toInt());
  if (g_server->hasArg("md")) g_config->setModbusDataBits(g_server->arg("md").toInt());
  if (g_server->hasArg("mp")) g_config->setModbusParity(g_server->arg("mp").toInt());
  if (g_server->hasArg("ms")) g_config->setModbusStopBits(g_server->arg("ms").toInt());
  if (g_server->hasArg("mr")) g_config->setModbusRtsPin(g_server->arg("mr").toInt());
  if (g_server->hasArg("sb")) g_config->setSerialBaudRate(g_server->arg("sb").toInt());
  if (g_server->hasArg("sd")) g_config->setSerialDataBits(g_server->arg("sd").toInt());
  if (g_server->hasArg("sp")) g_config->setSerialParity(g_server->arg("sp").toInt());
  if (g_server->hasArg("ss")) g_config->setSerialStopBits(g_server->arg("ss").toInt());
  if (g_server->hasArg("wp")) {
    String wp = g_server->arg("wp");
    if (!wp.equals(WEB_PASS_PLACEHOLDER)) g_config->setWebPassword(wp);
  }
  g_server->sendHeader("Location", "/");
  g_server->send(303);
}

void handleDebugGet() {
  if (!checkAuth()) return;
  dbgln("[webserver] GET /debug");
  String html = getResponseHeader("Debug");
  html += getDebugForm("1", "1", "3", "1");
  html += getButton("Back", "/");
  html += getResponseTrailer();
  g_server->send(200, "text/html", html);
}

void handleDebugPost() {
  if (!checkAuth()) return;
  dbgln("[webserver] POST /debug");
  String slaveId = g_server->hasArg("slave") ? g_server->arg("slave") : "1";
  String reg = g_server->hasArg("reg") ? g_server->arg("reg") : "1";
  String func = g_server->hasArg("func") ? g_server->arg("func") : "3";
  String count = g_server->hasArg("count") ? g_server->arg("count") : "1";
  
  String html = getResponseHeader("Debug");
  html += "<pre>";
  auto previousLevel = MBUlogLvl;
  MBUlogLvl = LOG_LEVEL_DEBUG;
  ModbusMessage answer = g_rtu->syncRequest(0xdeadbeef, slaveId.toInt(), func.toInt(), reg.toInt(), count.toInt());
  MBUlogLvl = previousLevel;
  html += "</pre>";
  
  auto error = answer.getError();
  if (error == SUCCESS){
    auto cnt = answer[2];
    html += "<span>Answer: 0x";
    for (size_t i = 0; i < cnt; i++) {
      char buf[3];
      sprintf(buf, "%02x", answer[i + 3]);
      html += buf;
    }
    html += "</span>";
  } else {
    char buf[50];
    sprintf(buf, "<span class=\"e\">Error: %#02x (%s)</span>", error, ErrorName(error).c_str());
    html += buf;
  }
  html += getDebugForm(slaveId, reg, func, count);
  html += getButton("Back", "/");
  html += getResponseTrailer();
  g_server->send(200, "text/html", html);
}

void handleNetworkGet() {
  if (!checkAuth()) return;
  dbgln("[webserver] GET /network");
  String html = getResponseHeader("Network Config");
  html += "<p>Ethernet configuration (ENC28J60)</p><table>";
  html += getTableRow("Current IP", Ethernet.localIP().toString());
  html += getTableRow("Gateway", Ethernet.gatewayIP().toString());
  html += getTableRow("Subnet", Ethernet.subnetMask().toString());
  html += "</table><p></p>";
  html += "<p class=\"e\">Note: Static IP configuration requires code modification</p>";
  html += getButton("Back", "/");
  html += getResponseTrailer();
  g_server->send(200, "text/html", html);
}

void handleUpdateGet() {
  if (!checkAuth()) return;
  dbgln("[webserver] GET /update");
  String html = getResponseHeader("Firmware Update");
  html += "<form method=\"post\" enctype=\"multipart/form-data\">";
  html += "<input type=\"file\" name=\"file\" id=\"file\" required/><p></p>";
  html += "<button class=\"r\">Upload</button></form><p></p>";
  html += getButton("Back", "/");
  html += getResponseTrailer();
  g_server->send(200, "text/html", html);
}

void handleUpdatePost() {
  if (!checkAuth()) return;
  g_server->sendHeader("Connection", "close");
  if (Update.hasError()) {
    g_server->send(500, "text/plain", "OTA failed");
  } else {
    String html = getResponseHeader("Firmware Update", true);
    html += "<p>Update successful. Rebooting...</p>";
    html += getButton("Back", "/");
    html += getResponseTrailer();
    g_server->send(200, "text/html", html);
    delay(1000);
    ESP.restart();
  }
}

void handleUpdateUpload() {
  HTTPUpload& upload = g_server->upload();
  if (upload.status == UPLOAD_FILE_START) {
    dbgln("[webserver] OTA start");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      dbgln("[webserver] OTA finished");
    } else {
      Update.printError(Serial);
    }
  }
}

void handleFavicon() {
  dbgln("[webserver] GET /favicon.ico");
  g_server->send(204);
}

void handleStyleCss() {
  if (g_server->hasHeader("If-None-Match")) {
    if (g_server->header("If-None-Match") == String(ETAG)) {
      g_server->send(304);
      return;
    }
  }
  dbgln("[webserver] GET /style.css");
  String css = getMinCss();
  css += "button.r{background:#d43535;}button.r:hover{background:#931f1f;}";
  css += "table{text-align:left;width:100%;}input{width:100%;}";
  css += ".e{color:red;}pre{text-align:left;}";
  g_server->sendHeader("ETag", ETAG);
  g_server->send(200, "text/css", css);
}

void handleNotFound() {
  dbg("[webserver] request to ");dbg(g_server->uri());dbgln(" not found");
  g_server->send(404, "text/plain", "404");
}

void setupPagesEthernet(EthernetWebServer *server, ModbusClientRTU *rtu, ModbusBridgeEthernet *bridge, Config *config){
  g_server = server;
  g_rtu = rtu;
  g_bridge = bridge;
  g_config = config;

  server->on("/", handleRoot);
  server->on("/status", handleStatus);
  server->on("/reboot", HTTP_GET, handleRebootGet);
  server->on("/reboot", HTTP_POST, handleRebootPost);
  server->on("/config", HTTP_GET, handleConfigGet);
  server->on("/config", HTTP_POST, handleConfigPost);
  server->on("/debug", HTTP_GET, handleDebugGet);
  server->on("/debug", HTTP_POST, handleDebugPost);
  server->on("/network", HTTP_GET, handleNetworkGet);
  server->on("/update", HTTP_GET, handleUpdateGet);
  server->on("/update", HTTP_POST, handleUpdatePost, handleUpdateUpload);
  server->on("/favicon.ico", handleFavicon);
  server->on("/style.css", handleStyleCss);
  server->onNotFound(handleNotFound);
}

String getMinCss(){
  return "body{font-family:sans-serif;text-align:center;background:#252525;color:#faffff;}"
    "#content{display:inline-block;min-width:340px;}"
    "button{width:100%;line-height:2.4rem;background:#1fa3ec;border:0;border-radius:0.3rem;"
    "font-size:1.2rem;-webkit-transition-duration:0.4s;transition-duration:0.4s;color:#faffff;}"
    "button:hover{background:#0e70a4;}";
}

String getResponseHeader(const char *title, bool inlineStyle){
  String html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset='utf-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,user-scalable=no\"/>";
  html += "<title>ESP32 Modbus Gateway - ";
  html += title;
  html += "</title>";
  if (inlineStyle){
    html += "<style>";
    html += getMinCss();
    html += "</style>";
  } else {
    html += "<link rel=\"stylesheet\" href=\"style.css\">";
  }
  html += "</head><body><h2>ESP32 Modbus Gateway</h2><h3>";
  html += title;
  html += "</h3><div id=\"content\">";
  return html;
}

String getResponseTrailer(){
  return "</div></body></html>";
}

String getButton(const char *title, const char *action, const char *css){
  String html = "<form method=\"get\" action=\"";
  html += action;
  html += "\"><button class=\"";
  html += css;
  html += "\">";
  html += title;
  html += "</button></form><p></p>";
  return html;
}

String getTableRow(const char *name, String value){
  String html = "<tr><td>";
  html += name;
  html += ":</td><td>";
  html += value;
  html += "</td></tr>";
  return html;
}

String getTableRow(const char *name, uint32_t value){
  return getTableRow(name, String(value));
}

String getDebugForm(String slaveId, String reg, String function, String count){
  String html = "<form method=\"post\"><table>";
  html += "<tr><td><label for=\"slave\">Slave ID</label></td>";
  html += "<td><input type=\"number\" min=\"0\" max=\"247\" id=\"slave\" name=\"slave\" value=\"" + slaveId + "\"></td></tr>";
  html += "<tr><td><label for=\"func\">Function</label></td>";
  html += "<td><select id=\"func\" name=\"func\" data-value=\"" + function + "\">";
  html += "<option value=\"1\">01 Read Coils</option>";
  html += "<option value=\"2\">02 Read Discrete Inputs</option>";
  html += "<option value=\"3\">03 Read Holding Register</option>";
  html += "<option value=\"4\">04 Read Input Register</option>";
  html += "</select></td></tr>";
  html += "<tr><td><label for=\"reg\">Register</label></td>";
  html += "<td><input type=\"number\" min=\"0\" max=\"65535\" id=\"reg\" name=\"reg\" value=\"" + reg + "\"></td></tr>";
  html += "<tr><td><label for=\"count\">Count</label></td>";
  html += "<td><input type=\"number\" min=\"0\" max=\"65535\" id=\"count\" name=\"count\" value=\"" + count + "\"></td></tr>";
  html += "</table><button class=\"r\">Send</button></form><p></p>";
  html += "<script>(function(){var s=document.querySelectorAll('select[data-value]');";
  html += "for(d of s){d.querySelector(`option[value='${d.dataset.value}']`).selected=true}})();</script>";
  return html;
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
