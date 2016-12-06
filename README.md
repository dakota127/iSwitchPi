# iswitchpi
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

Documentation, Code and EagleCAD files on GitHub. 
Documentation is in German, with English Abstract.  

Free to use, modify, and distribute with proper attribution.
Frei für jedermann, vollständige Quellangabe vorausgesetzt.

