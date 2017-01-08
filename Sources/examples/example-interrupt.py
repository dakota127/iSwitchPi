#!/usr/bin/python
# coding: utf-8
#--------------------------------------------------------------------------
#   Pi  Example Interrupt Script
#   Version 1
#
#   This script is just an example on how to use the square wave ouput of the iSwitchPi
#   to trigger an interrupt regularily.
#
#   iSwitchPi's square wave can be routed (by Dip switch on the iSwitchPi board) to one of 4 GPIO pins:
#   17,22,23 or 27. The selected pin will be declared as an INPUT pin here.
#
#   This script sets up an interrupt handler on the defined GPIO pin.
#   The script counts the interrupts and blinks a led when the counter reaches 10, counter is then reset to zero.
#   The interrupt-frequency is determined by a dip-switch on the iSwitchPi board.
#   We use GPIO pin 18 for the led - this pin is declared as output.

#   Testing: run script with these commandline options
#             -p nn  nn selects the GPIO IR pin (one of the above),
#                   NOTE: if -p is missing, GPIO 17 ist used as the default value
#
#   by Peter Boxler, December 2016
#
# ------------------------------------------------------------------------
#
# Import the modules to send commands to the system and access GPIO pins
import RPi.GPIO as GPIO

from time import sleep
import argparse
import sys,os
import signal
#
#
ret=0;
debug=0                 # set this to 1 or 2 mit Commandline ar -d
appname=""              # script name
pfad=""                 # path where script runs
counter_ir1=0           # Interrupt Counter 1
counter_ir2=0           # Interrupt Counter 2
do_blink=0              # blink in main loop if 1
GPIO_INTERRUPT_PIN=0    # Input pin interrupt, value will be set later
GPIO_LED_Pin=18         # Output pin led
IR_COUNT=10             # do something after that many interrupts
BLINK_TIME=0.1          # blink time in ms


# ------ Function Definitions ------------------------------------

# -----------------------------------------------------------------
# get and parse commandline args
# -----------------------------------------------------------------
def argu():
    parser = argparse.ArgumentParser()

    parser.add_argument("-d", help="debug", default=1,
                    type=int)
    parser.add_argument("-p", help="gpio", default=17,
                    type=int)

    args = parser.parse_args()
#   print (args.d, args.p)          # initial debug
    return(args)

# -----------------------------------------------------------------
# Signal Handler
# -----------------------------------------------------------------
def sigterm_handler(_signo, _stack_frame):
    # Raises SystemExit(0):
    sys.exit(0)

# -----------------------------------------------------------------
# check selected GPIO IR Pin
# -----------------------------------------------------------------
def gpio_pincheck(pin):

    if ((pin == 17) or (pin == 22) or (pin==23) or (pin==27)) :
      if debug==1:   print "\n%s: Selected IR-Pin: %d\n" % (appname,pin)
      return();
    print "%s: ERROR Selected GPIO %d not valid (use 17,22,23 or 27)" % (appname, pin)
    print "\n%s: Terminating" % appname

    return(99);

# -----------------------------------------------------------------
# Interrupt Handler
# is called if Input on GPIO pin goes high
# NOTE
# IR handlers should always be as short as possible - no long running task
# Therefore, the blinking with its 100ms delay is done in the mainloop
# we only set the variable do_blink which signals: ok, please do the blinking...
#
# Let us say we want to blink the led (on GPIO 18) once every 10 seconds - or read
# a temperature every 10 seconds or whatever...
# so we set the square wave output of iSwitchPi to 1 HZ with dip-switches set to 1010
# In this script we count the IR's and blink the led when the counter reaches 10
# Note: this is not very precise, since it depends on other things that are done in the mainloop
# -----------------------------------------------------------------
def my_callback(channel):
    global counter_ir1, counter_ir2, do_blink

    counter_ir1 += 1        # simple count for the print

    if debug:   print "%s: Interrupt %3d, GPIO-Pin Rising: %d" %  (appname,counter_ir1, channel)

    counter_ir2 += 1        # counting up to a certain value, then do the action
    if (counter_ir2 > IR_COUNT):
      counter_ir2=0;
      do_blink=1            # signal that blinking is in order

    return();

# -----------------------------------------------------------------
# Function blink led
# -----------------------------------------------------------------
def blink_led():  # blink led
    global do_blink
    GPIO.output(GPIO_LED_Pin, True)
    sleep(BLINK_TIME)
    GPIO.output(GPIO_LED_Pin, False)
    do_blink=0
    if debug: print "\n%s: Led blink..." %  appname


# -----------------------------------------------------------------
# Function initialize
# -----------------------------------------------------------------
def initialize():
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    sleep(0.2)
    GPIO.setup(GPIO_INTERRUPT_PIN, GPIO.IN)       # Set GPIO Pin to input
    GPIO.setup(GPIO_LED_Pin, GPIO.OUT)            # Set GPIO Pin to output for led

# setup Signal Handler
    signal.signal(signal.SIGINT, sigterm_handler)
    signal.signal(signal.SIGTERM, sigterm_handler)

    GPIO.add_event_detect(GPIO_INTERRUPT_PIN, GPIO.RISING, callback=my_callback)
    if debug: print "\n%s: initialize done, connect a led on GPIO %d" %  (appname,GPIO_LED_Pin)
    return();

# -----------------------------------------------------------------
# Main starts here
# -----------------------------------------------------------------
if __name__ == '__main__':
    appname=os.path.basename(__file__)      # name des scripts holen
    pfad=os.path.dirname(os.path.realpath(__file__))    # pfad wo dieses script läuft

    options=argu()                          # get commandline args
    debug=options.d                         #
    if (debug != 1): debug=1                # accept only 0,1 or 2
    GPIO_INTERRUPT_PIN=options.p                  # GPIO Pin


    if debug: print ("\n%s: Started: Path: %s") % (appname,pfad)
    ret=gpio_pincheck(GPIO_INTERRUPT_PIN)          # check pin
    if (ret==99):
      sys.exit(0)

    initialize()

    try:
#   Loop forever and do nothing- blinking is done in the Interrupt handler.
        while True:
            if (do_blink): blink_led()
            sleep(0.1)                # rest for a while....


    finally:

# End  do while --------------------------------------------
#
#       cleanup GPIO
        GPIO.remove_event_detect(GPIO_INTERRUPT_PIN);
        GPIO.cleanup(GPIO_INTERRUPT_PIN)
        GPIO.cleanup(GPIO_LED_Pin)

        if debug: print "\n%s: Terminating" %  appname
#-------------------------------------------------------------
#   end of program
#-------------------------------------------------------------
