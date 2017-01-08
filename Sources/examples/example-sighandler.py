#!/usr/bin/python
# coding: utf-8
#--------------------------------------------------------------------------
#   Example script with graceful termination done with signal handling
#
#   from
#   http://stackoverflow.com/questions/18499497/how-to-process-sigterm-signal-gracefully
#
#---------------------------------------------------------------------------
from time import sleep
import signal
import sys


# Fuction to hanlde Termination Signals -----------------
#--------------------------------------------------------
def sigterm_handler(_signo, _stack_frame):
    # Raises SystemExit(0):
    sys.exit(0)


# Main starts here --------------------------------------
#--------------------------------------------------------
if __name__ == '__main__':

    signal.signal(signal.SIGINT, sigterm_handler)
    signal.signal(signal.SIGTERM, sigterm_handler)

    try:
        print "Hello"
        i = 0
        while True:     # Main Loop
            #
            #           do something here
            #
            i += 1
            print "Iteration #%i" % i
            sleep(1)
    finally:
#       add termination code here
        print "End of the program. I terminated gracefully :)"

#  End of script----------------------------------------
