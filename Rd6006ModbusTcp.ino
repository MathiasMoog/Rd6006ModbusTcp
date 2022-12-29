/* Modbus (WiFi) TCP - RTU Bridge for Joy-Id or Riden power supply RD6006.
  
  See README.md file for documentation and details
  
  License: 
    Copyright, 2022, Mathias Moog, Deutschland, CC-BY-NC-SA
*/

//-------------------------------------------------------------------------------------
// Settings, adopt to your needs.

// Baud rate on the RTU side (Serial or Serial1)
#define RTU_BAUD 115200

#include "wifi_secrets.h"  // local header, not in git. Defines: SECRET_SSID and SECRET_PASS
//#define SECRET_SSID "xxx"
//#define SECRET_PASS "xxx"

// LED, ESP12-F Pin 2, change during developement according to your esp32 board.
#define LED_PIN 2

/* Logging per USB, use Serial for debug outputs and Serial1 for communication.
   Usefull during development on a NodeMuc or a similar ESP32 board with USB connection.
   See below how Serial1 is initialized.
   If switched off, Serial is used for the RTU interface.
   
   Do not use this debug flag on the RD6006 Wifi module
*/
//#define USE_LOG_USB

//-------------------------------------------------------------------------------------
// Logging

#ifdef USE_LOG_USB
// debug mode on esp32 board - only during development
#pragma message( "Use Serial (USB port) for logging and Serial1 for RTU" )
#define MODBUSIP_DEBUG
#define MODBUSRTU_DEBUG
#define LOG( a ) Serial.print( a )
#define LOG_LN( a ) Serial.println( a )
#define LOG_F( ... ) Serial.printf( __VA_ARGS__ )
#else
// code for the esp12-f based RD6006 wifi module
#undef MODBUSIP_DEBUG
#undef MODBUSRTU_DEBUG
#define LOG( a )
#define LOG_LN( a )
#define LOG_F( ... )
#endif

//-------------------------------------------------------------------------------------
// Modbus includes

//#include <vector>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <ModbusTCP.h>
#include <ModbusRTU.h>

//-------------------------------------------------------------------------------------
// Global variables

// RTU side, client (master), connect a RTU slave to this side
ModbusRTU rtu;
// TCP side, server (slave), connect a TCP master to this side
ModbusTCP tcp;

// Running modbus commands
uint16_t transRunning = 0;  // Currently executed ModbusTCP transaction
uint8_t slaveRunning = 0;   // Current request slave
uint32_t ipRunning = 0;

//-------------------------------------------------------------------------------------
// Functions

// callback, check erros, output only if defined USE_LOG_USB
bool cbTcpTrans(Modbus::ResultCode event, uint16_t transactionId, void* data) {  // Modbus Transaction callback
  if (event != Modbus::EX_SUCCESS) {                                               // If transaction got an error
    LOG_F("Modbus result: %02X, Mem: %d\n", event, ESP.getFreeHeap());   // Display Modbus error code (222527)
  }
  if (event == Modbus::EX_TIMEOUT) {                                             // If Transaction timeout took place
    //tcp.disconnect(tcp.eventSource());                                           // Close connection
    LOG_F("Modbus RTU Timeout");
  }
  return true;
}

// Callback receives raw data from ModbusTCP and sends it on behalf of slave (slaveRunning) to master
Modbus::ResultCode cbTcpRaw(uint8_t* data, uint8_t len, void* custom) {
  auto src = (Modbus::frame_arg_t*)custom;
  LOG("TCP IP: ");
  LOG_LN(IPAddress(src->ipaddr));
  LOG("Slave Id: ");
  LOG_LN(src->slaveId);
  LOG_F(" Fn: %02X, len: %d \n", data[0], len);
  // Switch LED on until response received
  digitalWrite(LED_PIN, LOW);
  // pass to RTU
  const boolean ok = rtu.rawRequest(src->slaveId, data, len, cbTcpTrans);
  if (!ok) {  // rawRequest returns 0 is unable to send data for some reason
    LOG_F("send failed\n");
    tcp.errorResponce(src->slaveId, (Modbus::FunctionCode)data[0], Modbus::EX_DEVICE_FAILED_TO_RESPOND);  // Send exceprional responce to master if request bridging failed
    return Modbus::EX_DEVICE_FAILED_TO_RESPOND;                                                           // Stop processing the frame
  }
  transRunning=src->transactionId;
  LOG_F("transaction: %d\n", transRunning);
  slaveRunning = src->slaveId;
  ipRunning = src->ipaddr;
  return Modbus::EX_SUCCESS;  // Stop other processing
}


// Callback receive Response from connected RTU server (slave) and pass to TCP client (master)
Modbus::ResultCode cbRtuRaw(uint8_t* data, uint8_t len, void* custom) {
  auto src = (Modbus::frame_arg_t*)custom;
  LOG_F("RTU Slave: %d %d, Fn: %02X, len: %d, ", src->slaveId, slaveRunning, data[0], len);
  LOG_F(" to server %d, transRunning %d, trans Id %d\n", src->to_server, transRunning, src->transactionId);
  // send to TCP client (master)
  if (!src->to_server) {  // Hm, no transaction in rawResponse possible.
    LOG_LN("Send response to tcp client");
    tcp.setTransactionId(transRunning);
    tcp.rawResponce(ipRunning, data, len);
  } else {
    return Modbus::EX_PASSTHROUGH;  // Allow frame to be processed by generic ModbusTCP routines
  }
  transRunning = 0;
  slaveRunning = 0;
  ipRunning = 0;
  // switch LED off, received response
  digitalWrite(LED_PIN, HIGH);
  return Modbus::EX_SUCCESS;  // Stop other processing
}

//-------------------------------------------------------------------------------------
// setup and loop

void setup() {
#ifdef USE_LOG_USB
  // Open serial port for debugging
  Serial.begin(115000);
  while (!Serial)
    ;
  LOG_LN("Modbus TCP RTU Bridge, debug mode. Wait for serial port");
#endif  
  // activate LED PIN
  pinMode(LED_PIN, OUTPUT);

  // connect to wifi
  LOG_LN("Try WiFi connection ...");  
#ifdef ESP8266
  /* See ESP8266WiFi documentation an samples:
    "Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network." */
  WiFi.mode(WIFI_STA);
#endif  
  WiFi.begin(SECRET_SSID, SECRET_PASS); // Connect to WPA/WPA2 Wi-Fi network.
  while (WiFi.status() != WL_CONNECTED)
  {
    LOG(".");
    // LED blinking while waiting for connection.
    digitalWrite(LED_PIN, LOW); // Switch LED on
    delay(500);
    digitalWrite(LED_PIN, HIGH); // Switch LED off
    delay(500); 
  }
  LOG_LN(""); // line break
  LOG_LN("IP address: ");
  LOG_LN(WiFi.localIP()); // nice printout

  
  // start modbus tcp as server
  LOG_LN("Starting TCP server (slave)");
  tcp.server();         // Initialize ModbusTCP to pracess as server
  tcp.onRaw(cbTcpRaw);  // Assign raw data processing callback

  // start modbus rtu as client, take care behaves different in debug mode
#ifdef USE_LOG_USB
  // debug mode, use Serial1 since Serial is used for debug output
  LOG_LN("Starting Serial1 for RTU interface 18-rx, 19-tx");
  Serial1.begin(RTU_BAUD, SERIAL_8N1, 18, 19);  // default pins 9 and 10 ar not working 18-rx, 19-tx
  LOG_LN("Starting RTU");
  rtu.begin(&Serial1);
#else  
  // ESP12-F, use Serial (UART0 for Modbus, no debug output on Serial ...)
  Serial.begin(RTU_BAUD);
  //Serial1.begin(RTU_BAUD, SERIAL_8N1, 18, 19);  // default pins 9 and 10 ar not working 18-rx, 19-tx
  rtu.begin(&Serial);
#endif  
  rtu.client();         // Initialize ModbusRTU as client (master)
  rtu.onRaw(cbRtuRaw);  // Assign raw data processing callback
}

void loop() {
  rtu.task();
  tcp.task();
  yield();
}