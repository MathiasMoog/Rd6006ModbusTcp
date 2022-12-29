# Modbus (WiFi) TCP - RTU Bridge for Joy-Id or Riden power supply RD6006.

The RD6006 offers a Modbus RTU interface via USB, see https://github.com/msillano/RD6006-Super-power-supply and
https://github.com/Baldanos/rd6006, but the wifi module does not provide Modbus over TCP. It uses some secret 
protocoll for communication with the Riden Software, but I want to use the Modbus Interface without USB cables ...

This bridge is quite simple, it passes all modbus commands from TCP (wifi) to RTU. Use only one TCP connection! 
The WIFI SSID and Password must be provided in the source code.

##  !!! A T T E N T I O N !!!    

- Flashing the wifi module will delete the original code. You might try to download the binary code
- The original Riden Software does not support this Modbus TCP solution! 
- No warranty. The code is as it is!
- This solution worked for me, but I'm not responsible for any damage!
- Take care of high voltages inside the RD6006, switch it off completely before opening the case!
- You find a reversed engineered schematic for the wifi module at: https://cuttlefishblacknet.wordpress.com/category/uncategorized/   
  
## Wifi module socket

- On main pcb:

        NC  TXD  RXD 5V
        NC  3V3  EN  GND
    
- On the wific pcb, TXD and RXD interchanged!       
- Please check!   
  
## Programming the wifi module

- Check the settings in the code (Baud rate, debug off, wifi_secrets.h - or local defines)
- Switch the RD6006 off and disconnect it completely from the mains.
- Remove the wifi module
- Arduino IDE setup for EPS12-F as in https://circuitjournal.com/esp8266-with-arduino-ide
- Additional Board Manager:  http://arduino.esp8266.com/stable/package_esp8266com_index.json
- Install esp8266 boards
- Board NodeMCU 1.0 (ESP-12E Module) - should work with the 12F also
- Connect the wifi module with an USB-TTL converter:

            ESP12 Reset -> USBTTL RST
            wifi module 12 V  -> USBTTL RST
            wifi module TXD   -> USBTTL RXD
            wifi module RXD   -> USBTTL TXD
            wifi module GND   -> USBTTL GND
            ESP12 GPIO0       -> USBTTL DTR
        
  I solderd two pins to the ESP12 reset and GPIO0 
    
- Save the original firmware

          esptool.py --connect-attempts 0 -p /dev/ttyUSB0 -b230400 read_flash 0x0 0x400000 original.bin
          
  See https://cuttlefishblacknet.wordpress.com/category/uncategorized/     
- Try to upload the blink sketch, built-in LED on pin 2        
- Remove the enable Pin from the wifi module, see https://community.home-assistant.io/t/riden-rd6006-dc-power-supply-ha-support-wifi/163849
- Upload this sketch
- Mount the wifi module, close the case and switch the power supply on.
- Set communication to TTL ( shift 0 for menu ). The Wifi mode uses some special communication, but TTL ist just ModbusRTU
- Switch the Load off and on again, now TTL and Modbus TCP should work.
    
## LED Signals

- Blinking with period 1s during Wifi connection, see function reconnect()
- Short flash during modbus operation (on with tcp request, off after rtu answer)

## ToDo

- OTA Updates, https://circuitjournal.com/programming-esp8266-over-wifi
- Web Page for configuration, open access point if not connected to wifi

## Works with the following hardware
- ESP8266, the original RD6006 wifi module
       An ESP12-F is soldered on the RD6006 wifi module
- ESP32, nice for developement
       NodeMcu or similar

## Required Arduino Libraries

- ESP8266WiFi provieded by the esp8266 board and hardware definitions 
- Alexander Emelianov, Arduino modbus-esp8266 library, https://github.com/emelianov/modbus-esp8266, 2021

## Based on

True RTU-TCP bridge example from Arduino modbus-esp8266 library


(c)2021 Alexander Emelianov (a.m.emelianov@gmail.com), https://github.com/emelianov/modbus-esp8266. That code is licensed under the BSD New License. 

## License
   
My code ist published under the  CC-BY-NC-SA license   
Copyright, 2022, Mathias Moog, Hochschule Ansbach, Deutschland, CC-BY-NC-SA
