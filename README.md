# ESP32 Modbus RTU/TCP Gateway

A generic firmware for an ESP32 to be used as a Modbus TCP/IP gateway for any modbus RTU device.
Default RX/TX pins for the TTL-RS485 communication are used from hardware-serial2 (check pinout of your esp32-board for UART2-TX and UART2-RX).
If you like to use different RX/TX pins, you can define them as build_flags in the platformio.ini.
E.g.:
    build_flags = -DRX_PIN=14 -DTX_PIN=5 

## State

It work's for me, but there's room for improvement. If you have an idea please open an issue - if you can improve anything just create a PR.


## Hardware

### ESP32 NodeMCU + XY-017 TTL-RS485 Board with automatic flow control
![Home](doc/img/esp32-xy017.png)

### ENC28J60 Ethernet Module Connection

To use ENC28J60 Ethernet module instead of WiFi:

```
ENC28J60    →    ESP32
------------------------
SI (MOSI)   →    GPIO23
SO (MISO)   →    GPIO19
SCK         →    GPIO18
CS          →    GPIO5
INT         →    GPIO4
RESET       →    GPIO2
Vcc         →    3.3V
GND         →    GND
```

**Important notes:** 
- Use `esp32-enc28j60` environment for building: `platformio run -e esp32-enc28j60 -t upload`
- Disconnect ENC28J60 power before flashing ESP32
- **Two modes of operation:**
  - **WORK MODE** (default): Modbus TCP enabled, Web UI disabled
  - **CONFIG MODE**: Web UI enabled, Modbus TCP disabled
- **To enter CONFIG MODE**: Short GPIO15 to GND (use jumper or button) during power-on/reset
- **To enter WORK MODE**: Remove jumper and reboot (GPIO15 floating/HIGH)
- Web UI available at DHCP IP address (check Serial Monitor for IP)
- Supports 1 Modbus TCP client in WORK mode (optimized for stability)


## Screenshots

### Home

![Home](doc/img/home.png)

### Status

![Status](doc/img/status.png)

### Config

![Config](doc/img/config.png)

### Debug

![Debug](doc/img/debug.png)
