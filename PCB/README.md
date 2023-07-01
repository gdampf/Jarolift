# EasyEDA PCB
The idea is to use an ESP-07S for controlling a standard Jarolift Remote control via MQTT.
The PCB contains 2 Transistors (Q1 & Q2) for easy programming via PL2 as serial interface.
Further there are 4 Transistors connected to the LEDs of the Jarolift Remote Control to detect the current active channel and signal its state to IO13, IO16, IO02 and IO00 in that order for Channel 1-4.
TR1 is providing the necessary Voltage of 3,3V. I connected VCC to the Battery-connector (+) of the Remote control via a standard shotkey diode to reduce it to 3 V for the remote. Also Gnd is connected.
The 4 Keys (Up, Down, Stop and Select) are connected to the corresponding pins (IO05, IO14, IO04 and IO12), which pull them to ground as a keypress would do.
See Schematics for further details.
