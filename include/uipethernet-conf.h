#ifndef UIPETHERNET_CONF_H
#define UIPETHERNET_CONF_H

// Увеличенные лимиты для Modbus TCP Gateway
// https://github.com/jandrassy/EthernetENC/wiki/Settings

/* for TCP */
#ifndef UIP_SOCKET_NUMPACKETS
#define UIP_SOCKET_NUMPACKETS    5  // Увеличено с 3 до 5
#endif
#ifndef UIP_CONF_MAX_CONNECTIONS
#define UIP_CONF_MAX_CONNECTIONS 12  // Увеличено с 4 до 12 (10 Modbus + 2 HTTP)
#endif

/* for UDP */
#ifndef UIP_CONF_UDP
#define UIP_CONF_UDP             1
#endif
#ifndef UIP_CONF_UDP_CONNS
#define UIP_CONF_UDP_CONNS       2
#endif

#ifndef UIP_UDP_BACKLOG
#define UIP_UDP_BACKLOG       2
#endif

/* timeout in ms */
#ifndef UIP_WRITE_TIMEOUT
#define UIP_WRITE_TIMEOUT    3000  // Увеличено с 2000 до 3000
#endif

#ifndef UIP_CONNECT_TIMEOUT
#define UIP_CONNECT_TIMEOUT      10  // Увеличено с 5 до 10
#endif

/* periodic timer for uip (in ms) */
#ifndef UIP_PERIODIC_TIMER
#define UIP_PERIODIC_TIMER       50  // Уменьшено со 100 до 50 для более быстрой обработки
#endif

#endif
