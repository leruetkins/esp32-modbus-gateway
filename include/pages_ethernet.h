#ifndef PAGES_ETHERNET_H
    #define PAGES_ETHERNET_H

    #include <EthernetWebServer.h>
    #include <EthernetENC.h>
    #include <ModbusBridgeEthernet.h>
    #include <ModbusClientRTU.h>
    #include <Update.h>
    #include "config.h"
    #include "debug.h"

    void setupPagesEthernet(EthernetWebServer* server, ModbusClientRTU *rtu, ModbusBridgeEthernet *bridge, Config *config);
    String getResponseHeader(const char *title, bool inlineStyle = false);
    String getResponseTrailer();
    String getButton(const char *title, const char *action, const char *css = "");
    String getTableRow(const char *name, uint32_t value);
    String getTableRow(const char *name, String value);
    String getDebugForm(String slaveId, String reg, String function, String count);
    String getMinCss();
    const String ErrorName(Modbus::Error code);
#endif /* PAGES_ETHERNET_H */
