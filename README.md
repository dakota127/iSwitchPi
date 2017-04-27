# iSwitchPi
Intelligent Power Switch for Raspberry Pi 

Native Raspberry Pi does not have an On/Off switch and there is no easy way
to shutdown the Pi while keeping the filesystem intact.
This Intelligent Power Switch allows just that: 
Power-On the Pi by pressing a pushbutton and also properly Power-Off
the Pi with another press on the same button.
The intelligence is provided by a program running in an AVR MCU ATtiny44.
This C-program implements a Finite State Machine in the MCU.
A small Python script is running in the Pi itself. 
Just one GPIO-Pin is used for two-way communication.
In addition, a variable frequency square wave is available for externally interrupting the Pi.

Formfactor: Raspi hat with 2x20 Pins for other hats on top of iSwitchPi.

Full Documentation, Code and EagleCAD files on GitHub. 
Documentation is in English and German.
Quick Start guide explains installation.  
See state diagram to understand operation of iSwitchPi

Also check out my project webpage at 
http://projects.descan.com/projekt5.html

Update April 2017:
In April of 2017 an update was made to the firmware in the ATtiny44. 
A user requested an auto-power-on functionality: the Pi should restart 
unattended (meaning without the need to press the ON-pushbutton) when 5 Volt comes back after a power failure. 
State 0 was added to the state diagram  and position 4 of dip-switch 3 is used to select/deselect this feature.

Free to use, modify, and distribute with proper attribution.
Frei für jedermann, vollständige Quellangabe vorausgesetzt.

