#!/usr/bin/python
# coding: utf-8
#--------------------------------------------------------------------------
#   Pi  Shutdown Script
#   Version 2
#   Works together with the Intelligent Power Switch ISWITCHPI
#   Only one GPIO pin is used to communicate with the ISWITCHPI
#
#   Testing: run script with commadline -d 1   or -d 2
#
#   WARNING: Do not change time/sleep values, they correspond to what iswitchpi does
#
#   Note:   killfiles are use for testing only: we can check if Signals from OS are treated ok
#           by looking for killfiles in the sripts directory
#
#   by Peter Boxler, Februar 2016
# ------------------------------------------------------------------------
#
# Import the modules to send commands to the system and access GPIO pins
from subprocess import call
import RPi.GPIO as GPIO

from time import sleep
import time, datetime
import optparse
import sys, os, re
import signal
#
#-------------------------------------------------------------------------------------
# Define the Pin numbers for signaling to/from ISWITCHPi 
# Change here if you use other pins
FROMPI = 20    #   Keep me alive Pin, Pi signals running by setting this hi  (17 in testenv)
# -----------------------------------------------------------------------------------
#
debug=0                 # set this to 1 or 2 mit Commandline ar -d
appname=""              # script name
pfad=""                 # path where script runs
start_time=0            # time start
intervall=0             # time intervall for signalling pulses from ISWITCHPI
INTERVALLMAX=1          # max waittime for second pulse
PULSE=0.05              # Pulse lenght in sec for I am alive pulses to ISWITCHPI    
anzir=0                 # Interrupt Counter
sleeptime=0.1           # DO NOT CHANGE THIS - DEPENDENT ON PULSES SENT BY ISWITCHPI.C (ATtiny44)
sleepbeforedown=2
kill_shellscript="/home/pi/myservices/killjobs.sh"
killfile="iswitch-kill"

# killcomm="kill $(ps aux | grep tr_main | awk '/python/ {print $2}')"

# ------ Function Definitions -----------------------------
#----------------------------------------------------------
# get and parse commandline args
def argu():
    parser = optparse.OptionParser()
    parser.add_option('-d', '--dbg', dest='debug', default=0,
                    action='store', type='int',
                    help='debug')

    options, args = parser.parse_args()
    return(options)
    
#----------------------------------------------
#-- Class for Graceful termination
class GracefulKiller:
  kill_now = False
  def __init__(self):
    signal.signal(signal.SIGINT, self.exit_gracefully)
    signal.signal(signal.SIGTERM, self.exit_gracefully)

  def exit_gracefully(self,signum, frame):
      if debug==2: print "Signal received " , signum 
      tmp=killfile + "-" + '{:2d}'.format(signum) + ".txt"
      killit=open(tmp,"w")  # write killfile, so we know this was done
      killit.close()
      if debug==2: print "Return in Signalhandler.. " , signum
      self.kill_now = True
      
#------------------------------------------------
# delete ev. vorhandene killfiles
def purgekf(dir, pattern):
    for f in os.listdir(dir):
    	if re.search(pattern, f):
    	    if debug==2: print "killfile %s removed" % f
            os.remove(os.path.join(dir, f))


# -----------------------------------------------------------------
# Define a function to run when an interrupt is called
def my_callback(channel):
    global state,sleeptime,anzir;
    
#    sleeptime=0         # starting now we do not sleep in main loop
    if debug==2:   print "Shutdown Pin Rising: %d" % channel
    anzir=anzir+1
#    GPIO.remove_event_detect(channel);
#    state=2;
    return();   

#----------------------------------------------------
# what do we need to do: halt or reboot the Pi
def shutdown(what):
    GPIO.remove_event_detect(FROMPI);
    GPIO.setup(FROMPI, GPIO.OUT)       # Set GPIO Pin to output again


    if (what==1):                # high means halt, low means reboot
    
        if debug==1: print "\niswitchpi detected HALT"
        if debug==2:           # debug mode do nothing exept print
            print "Shutdown reached... Halting"
            GPIO.output(FROMPI,False)
            GPIO.cleanup(FROMPI)
            sys.exit(0)
        else:
            try:
                call (kill_shellscript)
            except:
                pass;
            sleep(0.2)
            GPIO.output(FROMPI,False)
            GPIO.cleanup(FROMPI)
            sleep(sleepbeforedown)
            call('halt', shell=False)   # halt the Linux System
            return(0)
    else:
        if debug==1: print "\niswitchpi detected REBOOT"

        if debug==2:           # debug mode do nothing exept print
            print "Shutdown reached... Rebooting"
            GPIO.output(FROMPI,False)
            GPIO.cleanup(FROMPI)
            sys.exit(0)
        else:
            try:
                call (kill_shellscript);
            except:
                pass;
            sleep(0.2)
            GPIO.output(FROMPI,False)
            GPIO.cleanup(FROMPI)
            sleep(sleepbeforedown)
            call('reboot', shell=False)   # reboot the Linux System
            return(0)
        

# Function initialize() ----------------------------------------
def initialize():
    global start_time, appname;
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    sleep(0.2)
    GPIO.setup(FROMPI, GPIO.IN)       # Set GPIO Pin to input
    GPIO.add_event_detect(FROMPI, GPIO.RISING, callback=my_callback)

    if debug==2: print ("%s initialize done") % appname;
    return();

# Function send pulses to ISWITCHPI
#-------------------------------------------------------
def sendpulse():  # blink led 
    GPIO.output(FROMPI, True)
    sleep(PULSE)
    GPIO.output(FROMPI, False)
    sleep(PULSE)

    
# Main starts here --------------------------------------
#--------------------------------------------------------
if __name__ == '__main__':
    killer = GracefulKiller()               # Instatiate Gracefull kill class
                                            # we need this to catch a Pi reboot or Pi halt
                                            # that was excuted from the commandline
    appname=os.path.basename(__file__)      # name des scripts holen
    options=argu()                          # get commandline args
    debug=options.debug                     # debug option
    pfad=os.path.dirname(os.path.realpath(__file__))    # pfad wo dieses script läuft
    purgekf(pfad,killfile);                 # delete all killfiles

    if debug==1: print ("\nStarted: %s  Path: %s\n") % (appname,pfad) 

    initialize()
    sleep(2)                                   # wait for things to settle dowb^n
    start_time = time.time();                # take time
    intervall=INTERVALLMAX+1;

#   Loop forever - until IR come in from iSwitchPi
    while True:    
        sleep(sleeptime)                # normal delay in loop
        if killer.kill_now:
            if debug==2: print "OK, Kill myself" 
            break;
        if (intervall > INTERVALLMAX):      # intervall to send pulses
            if (anzir>0):                   # did we have Ir's in the past intervall ?
                if debug==2: print "Number of IR: ", anzir      # yes we did, so shutdown or reboot
                shutdown(anzir)             # pass number of IR on to shutdown function
                break;                      # terminate script, OS will do the rest   
            anzir=0                         # clear old IR's 
            GPIO.remove_event_detect(FROMPI);
            GPIO.setup(FROMPI, GPIO.OUT)       # Set GPIO Pin to output 
            sendpulse()                        # send a pulse to iSwitchPi
            GPIO.setup(FROMPI, GPIO.IN)        # Set GPIO Pin back to input
            GPIO.add_event_detect(FROMPI, GPIO.RISING, callback=my_callback)
            start_time = time.time();           # take time
            
        intervall = time.time() - start_time       # check timer
    pass;
    
# End  do while --------------------------------------------
#
    GPIO.remove_event_detect(FROMPI);
    GPIO.setup(FROMPI, GPIO.OUT)            # Set GPIO Pin to output again
    GPIO.output(FROMPI,False)               # set GPIO low 
                                            # we reach this if Pi is halted/rebooted
                                            # from commandline or else
    if debug==2: print " End reached"
#-------------------------------------------------------------
#   end of program       
#-------------------------------------------------------------
