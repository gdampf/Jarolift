# Jarolift/Jarolator
Jarolift is offering shutter motors with embedded 433MHz remote receiver. The remote controller TDRC 04 has 4 LEDs to signal the current channel to control up to 4 independent shutter.
It has 4 Buttons, one to select the channel, one to open the shutter (up), one to close the shutter (down) and one to stop the current movements (stop).
A direct control of the 433MHz Signal is difficult because of the rolling code they use. I tried several solutions, but they are not that reliable as the original remote is.
As far as such a remote is costing just a few Euro and can be purchased used for even less, it isn't worth to try to make the remote by yourself. Just use a real remote and control it by an ESP8266.

The idea is to use an ESP-07S for controlling a standard Jarolift Remote control via MQTT.

You can build this on a bread board (see fritzing) or use my PCB (Jarollator).

If somebody has build a nice housing on 3d-printer for the cirquit and the remote, let me know.

To be used with VSCode Version: 1.79.2 (user setup)
and Platformio 6.1.7, but the versions shouldn't mean that much.
For Versions on Libraries, see the platformio.ini.template!