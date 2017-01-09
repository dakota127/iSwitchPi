/************************************************************************/
/*                                                                      */
/*	Intelligent Power Switch for Raspberry Pi                           */
/*  																    */
/*	Implements Finite State Machine   with 7 States 	                */
/*	Pulsegeneration is done in square.c  [ functions pwm_xx() ]         */
/* 									                                    */
/*	See project description for full details				            */
/*                                                                      */
/*		ATTiny44  Pin Usage						                        */
/*	   				         ----o----                                  */
/*       	     	    VCC|       |GND									*/
/*     DIP 1   	    PB0|       |PA0		TESTPIN						    */
/*     DIP 2       	PB1|       |PA1		5V on/off						*/
/*     Reset       	PB3|       |PA2		TofromPi					    */
/*     DIP 3       	PB2|       |PA3		Led								*/
/*     TESTPIN      PA7|       |PA4		Pushbutton					    */
/*     Delay       	PA6|       |PA5		Squarewave out                  */
/*                	  	--------     					    		     */
/*                                                                      */
/*	Uses Timer0 for debouncing pushbutton and Timer1 for generation     */
/*  of square wave on Pin PA5  (Fast PWM mode)   						*/
/*											                            */
/* 	Includes Debouncing 8 Keys with Repeat Function by Peter Dannegger  */
/* 	Found here:  http://www.mikrocontroller.net/topic/48465             */
/*                                                                      */
/*                                                                      */
/* This c-Code runs on ATtiny44                                         */
/* Written by:                                                          */
/* Peter K. Boxler, December 2016                                        */
/************************************************************************/
//  Quelle und Diskussion des Debounce Code siehe hier:
//  http://www.mikrocontroller.net/topic/48465
//  Eintrag
//  Autor: Peter Dannegger (peda)
//  Datum: 16.06.2010 22:28
//------------------------------------------------------------------------

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <square.h>                         // pwm functions for pulse generation

// define VERSION if board iswitchpi Version 1
//#define VERSION1
// #define F_CPU           1000000             // processor clock frequency - defined in Makefile !!
#define KEY_PORT        PINA                // Port A on ATtiny44
#define VPOWER          PINA1               // 5 Volt Power on/off
#define FROMPI          PINA2               // signal from and to Pi
#define LED1            PINA3               // green led
#define LED2            PINA5               // orange led (TESTMODE ONLY)
                                            // in normal mode used as Output Compare Pin OC1B see square.c
#define KEY0            PINA4               // pushbutton on Pin4
#if defined VERSION1                       // board version 2 has different Pin assignment
// version 1 TESTPIN
#define TESTPIN         PINA7               // TESTPIN to simulate pins from Pi
// version2
#else
#define TESTPIN         PINA0              // TESTPIN no pulses fromPi required
#define DELAYTIME       PINA6              // long or short wait times (selectable by Dip-switch 4 Pos 2 ON=short)
                                            // use short for Raspberry Pi 3 which boots/shutsdown faster
#define SQUARE          PINA7               // Pulsgeneration on/off (DIP Switch 1)
#endif                                       // used in square.c
                                            // square wave output on  PINA5
// definitions for Debounce Code
#define REPEAT_MASK     (1<<KEY0)           // repeat: key0
#define REPEAT_START    100                 // after 1000ms
#define REPEAT_NEXT     20                  // every 200ms
// definitions for Debounce Code

#define POWEROFF_Delay_HALT_long     30           // seconds  (use 20 for test)
#define POWEROFF_Delay_HALT_short    15           // seconds  (use 20 for test)

#define POWEROFF_Delay_REBOOT_long   50           // seconds  (use 20 for test)
#define POWEROFF_Delay_REBOOT_short  20           // seconds  (use 20 for test)

#define PULSCHECK               4           // 10 times 10 ms intervall check pulses from Pi
#define PULSCHECK_SECONDS       6           // ccount pulses from Pi during this time

#define POWERON_Delay_long      50          // seconds (use 20 for test)
#define POWERON_Delay_short     20          // seconds (use 20 for test)

#define POWERON_Blink_int  15               // 15 x 10 ms
#define POWEROFF_Blink_int 60               // 60 x 10 ms
#define TESTMODE_Blink_int 300              // 200 ms
#define REGULAR_Blink       1
#define PULSED_Blink        2
#define STANDBY_Blink_int_on   3
#define STANDBY_Blink_int_off  250          // can be max 255

#define PULSELENGTH1  80                    // Signal to PI , ms
#define PULSELENGTH2  500                    // Signal to PI , ms

static unsigned char sekunde;
static unsigned char sekunde2;

static unsigned char tick;				// tick Timer 0
static unsigned char tick2;				// tick Timer 0
static unsigned char tick3;				// tick Timer 0
uint8_t blinkint;                           // blink intervall
uint8_t blinkwhat;                          // blink intervall
uint8_t blinkon;                            // blink ON=1  (Standby blink only
uint8_t poweroff_delay;                     // either POWEROFF_Delay_HALT_long or POWEROFF_Delay_REBOOT_long
uint8_t key_state;                          // debounced and inverted key state:
uint8_t poweron_delay;                     //

                                            // bit = 1: key pressed
uint8_t key_press;                          // key press detect
uint8_t key_rpt;                            // key long press and repeat
volatile uint8_t key_release;               // key release detect

enum {                                      // finite state machins states
    state1,
    state2,
    state3,
    state4,
    state5,
    state6,
    state7
    };
static uint8_t first_time = 0xff;                // first_time run through a state, bitwise, bit 0 means state 1
static uint8_t state=state1;                       // state variable
static uint8_t pipulses=0;                          // key press detect
static uint8_t waslo=1;                  // key press detect
static uint8_t washi=0;                  // key press detect
volatile static uint8_t sendnow=0;
static uint8_t pastpulses=0;
void mytimer(void);
void sendtopi(int);
void sendtopi_2(int);

//-------------------------------------------------------------------
// debounce functions from Peter Dannegger
// http://www.mikrocontroller.net/topic/tasten-entprellen-bulletproof
// http://www.mikrocontroller.net/topic/48465
//-------------------------------------------------------------------
uint8_t key_clear( uint8_t key_mask )
{
  ATOMIC_BLOCK(ATOMIC_FORCEON){
    key_press &= ~key_mask;                      // clear key(s)
  }
  return (0x0);
}

uint8_t get_key_press( uint8_t key_mask )
{
  ATOMIC_BLOCK(ATOMIC_FORCEON){
    key_mask &= key_press;                      // read key(s)
    key_press ^= key_mask;                      // clear key(s)
  }
  return key_mask;
}

uint8_t get_key_rpt( uint8_t key_mask )
{
  ATOMIC_BLOCK(ATOMIC_FORCEON){
    key_mask &= key_rpt;                        // read key(s)
    key_rpt ^= key_mask;                        // clear key(s)
  }
  return key_mask;
}

uint8_t get_key_release( uint8_t key_mask )
{
  cli();                     // read and clear atomic !
  key_mask &= key_release;   // read key(s)
  key_release ^= key_mask;   // clear key(s)
  sei();
  return key_mask;
}

uint8_t get_key_short( uint8_t key_mask )
{
  uint8_t i;

  ATOMIC_BLOCK(ATOMIC_FORCEON)
    i = get_key_press( ~key_state & key_mask );
  return i;
}

uint8_t get_key_long( uint8_t key_mask )
{
  return get_key_press( get_key_rpt( key_mask ));
}

uint8_t get_key_long_r( uint8_t key_mask )      // if repeat function needed
{
  return get_key_press( get_key_rpt( key_press & key_mask ));
}

uint8_t get_key_rpt_l( uint8_t key_mask )       // if long function needed
{
  return get_key_rpt( ~key_press & key_mask );
}
// --- Ende debounce functions   ----------------------


//----------------------------------------------------
// --- Interrupt Service Routine for Timer/Counter 0
//  this code runs every 10 ms activated by Timer 0 compare_match
// this is Parer Danneggers Code
//----------------------------------------------------
ISR( TIM0_COMPA_vect )                          // every 10ms
{
  static uint8_t ct0 = 0xFF, ct1 = 0xFF, rpt;
  uint8_t i;

  i = key_state ^ ~KEY_PORT;                     // has key-input changed ?
  ct0 = ~( ct0 & i );                           // reset or count ct0
  ct1 = ct0 ^ (ct1 & i);                        // reset or count ct1
  i &= ct0 & ct1;                               // count until roll over ?
  key_state ^= i;                               // then toggle debounced state
  key_press |= key_state & i;                   // 0->1: key press detect
  key_release |= ~key_state & i;                // 1->0: key release detect

  if( (key_state & REPEAT_MASK) == 0 )          // check repeat function
     rpt = REPEAT_START;                        // start delay
  if( --rpt == 0 ){
    rpt = REPEAT_NEXT;                          // repeat delay
    key_rpt |= key_state & REPEAT_MASK;
  }

    mytimer();              // handle my own timer stuff

}

//----------------------------------------------------
// --- Interrupt SubRoutine - called by regular ISR
//  this code runs every 10 ms activated by Timer 0 compare_match
//----------------------------------------------------
void mytimer()   {                       // every 10m{
// count ticks (one tick every 10 ms)
    tick++;                             // tick is used for adding seconds
    tick2++;                            // tick2 is used for led blinking
    tick3++;                            // tick3 is used for Pi related stuff

    if(tick > 100) {                     // 100 times 10 ms equals a second
        sekunde++;                       // seconds used for poweron/off delays
        sekunde2++;                      // seconds used for pulses from pi
        tick = 0;
    }


// now do Pi related stuff ---------------------

    if (tick3 > PULSCHECK)  {            // every PULSCHECK times 10 ms: check pulses from Pi
        tick3=0;                         // and send signal to Pi if key was pressed (and only then)
                                          // check signal from Pi every 100 ms
                                          // Pin FROMPI ist set to Input
        if (PINA & (1<<FROMPI))       // pin ist high
            {
            if (waslo==1)  {             // Pi signals I am alive, pin was low before, goes to high
                washi=1;
                pipulses++;              // count pulses from Pi
                waslo=0;
                }
            }
        else if (washi==1) {            // Input is low again
            waslo=1;                    // if we have to send something, do it now!!
            washi=0;

            if (sendnow != 0)  {           // we need to send puls(es) to the Pi
                _delay_ms(100);             // give the Pi time to set up its IR Handler
                sendtopi(sendnow);         // variable sendnow says how many (one or two)
                sendnow=0;                 // no more to be sent
                }
            }
    }       // end check Pi

    // store number of pulses that came in within the last PULSCHECK_SECONDS seconds and reset counter
    if (sekunde2 > PULSCHECK_SECONDS) {           // for PULSCHECK_SECONDS seconds we count pulses from pi
        pastpulses=pipulses;            // store number of pulses
        pipulses=0;                     // reset puls counter
        sekunde2=0;
        }

// done with th Pi related stuff

}

//----------------------------------------------------
// --- Function blink orange led  (TESTMODE ONLY)
//----------------------------------------------------
void blink_led() {
    PORTA |= (1<<LED2);               //orange led on
    _delay_ms (TESTMODE_Blink_int);
    PORTA &= ~( 1<<LED2);             //one pulse
    _delay_ms (50);

    }

//----------------------------------------------------
// --- Fuction send pulses to pi
//  send simple pulses
//----------------------------------------------------
void sendtopi(int what) {

    DDRA  |= (1<<FROMPI)  ;                 // Switch line to Pi to output
    PORTA |= (1<<FROMPI);                   //signal to pi halt
     _delay_ms(PULSELENGTH1);
    PORTA &= ~( 1<<FROMPI);                 //one pulse

    if (what==2) {
        _delay_ms(PULSELENGTH2);            //pause
        PORTA |= (1<<FROMPI);               //signal to pi halt
        _delay_ms(PULSELENGTH1);
        PORTA &= ~( 1<<FROMPI);             //one pulse
        }
    DDRA  &= ~(1<<FROMPI);                  // set to Input again (from Pi)

 }


//-------------------------------------------------------
// ---- MAIN Program
//-------------------------------------------------------
int main( void )
{
// Set all Ports
    DDRA   =  1<<LED1 | 1<<VPOWER ;                 // output signals
    DDRA  &= ~(1<<FROMPI);                          // set to Input (from Pi)
    DDRA  &= ~(1<<TESTPIN);                        // set to Input (TESTPIN) is used for simulation without pulses from Pi
                                                    // used for Testing ONLY, must be not connected
                                                    // for normal operation !!
    PORTA |= (1<<TESTPIN);                         //  set pullup
    DDRA  &= ~(1<<DELAYTIME);                       // Select Timer values for on/off  (use short for Pi 3)
    PORTA |= (1<<DELAYTIME);                        //  set pullup
    PORTA &= ~(1<<FROMPI);                         // no pullup - has external pulldown
    PORTA &= ~( 1<<LED1 | 1<<VPOWER );             // all outputs off

    PORTA &= ~(1<<KEY0);                           // no pullup on Key-Input, has external pullup

    // TIMER 0 konfig - used for Peter Dannegger's debounce-Functions
    TCCR0A = 1<<WGM01;                             // Timer0 Mode CTC
    TCCR0B = 1<<CS02 | 1<<CS00;                    // Timer/Counter0 source is F_CPU / 1024
    OCR0A = F_CPU / 1024.0 * 10e-3 - 0.5;          // 10 ms compare Value for counter/timer (Prescaler: 10ms interrupt)
    TIMSK0 = 1<<OCIE0A;                             // Enable compare Match interrupt on Timer/Counter 0
                                                    // Achtung: TIMSK für tiny85 und TIMSK0 für tiny44

    pwm_init();                                     // setup Timer 1 for variable pulse on PA5
    sei();                                          // Interrupt enable
    blinkwhat=0x00;                                 // do not blink
    first_time = 0xff;                                   //bits for first_time run thru the states
                                                    // bit 0: state 1
                                                    // bit 1: state 2
                                                    // bit 2: state 3
                                                    // etc.
//
// ---- Main Loop. forever ---------------------
// ---- Implements State Machine ---------------
  for(;;)
  {
 //   _delay_ms(10);                                // for testing

    switch (state) {
/*------------------------------------------------------------------*/
/*  state 1  Stand-by State, all is off, waiting for short keypress  */
/*  Power to Pi ist off, led blinks short pulses                                 */
/*  Waiting for short keypress                                      */
/*------------------------------------------------------------------*/
    case state1:

        if (first_time & (1<<0))     // first_time time throu ?
            {
            PORTA &= ~( 1<<LED1 | 1<<VPOWER);   // all outputs off
            key_clear( 1<<KEY0 );
            blinkwhat=PULSED_Blink;
            poweron_delay=POWERON_Delay_long;
            if ( !(PINA & (1<<DELAYTIME)) ) {
              poweron_delay=POWERON_Delay_short;            // check pin PA6 for delay times (Dip-switch 4 Pos 2 ON)
            }
            tick2=0;
            blinkon=1;
            pwm_stop();							// stop pulse genaration output on PA5
            pastpulses=0;                       // pulse counter reset (pulses from Pi)
            first_time =0xff;                        // set first_time all other states
            first_time &= ~(1<<0);                   // clear first_time this state
            }
        if  (get_key_short( 1<<KEY0 )) {         // get debounced keypress
            if (!(PINA & (1<<TESTPIN)) )         // if Testpin is low: signalling TESTMODE
                state=state7;                       // next state is state 7
            else
                state=state2;                       // next state is state 2
            }                                   // leaving standby
        break;

/*------------------------------------------------------------------*/
/*  state 2  Tentative Power on, waiting for Pi to come up          */
/*  Power to Pi ist on, led is blinking fast                        */
/*  Short keypress switches to state 4   (power on without checking */
/*  whether Pi is on)                                               */
/*------------------------------------------------------------------*/
    case state2:

        if (first_time & (1<<1))   {            // first_time time throu ?
            PORTA |= (1<<VPOWER);               //switch 5 volt power on
            blinkwhat=REGULAR_Blink;
            blinkint=POWERON_Blink_int;
            tick2=0;                            //start timer
            sekunde=0;
            pwm_start();						            // start pulse generation output on PA5
            first_time =0xff;                   // set first_time all other states
            first_time &= ~( 1<<1)  ;           // clear first_time this state
            }

        pwm_check();

        if ((blinkwhat>0) && (sekunde > poweron_delay))  {
            blinkwhat=0;
            state=state1;                       // Pi did not come on, so gaback to stand by
            }

        if  (get_key_short( 1<<KEY0 ))  {        // get debounced keypress short
            blinkwhat=0;
            state=state4;
           }
                    // how many pi pulses have we received ? if we have Pi is alive
                    // so we go to state 3 (normal operation state)
            if (pastpulses > 3) {state=state3;}

    break;

/*------------------------------------------------------------------*/
/*  state 3  Power ON Number 1, regular operating state            */
/*  Power to Pi ist on, led is on                                   */
/*  Loss of signal from Pi changes state to 5                       */
/*  Short keypress  signals Pi to shut down, changes state to 5     */
/*  Long Keypress signals Pi to reboot, stays in state 3            */
/*------------------------------------------------------------------*/
    case state3:
        if (first_time & (1<<3))  {                   // first_time time throu ?
            PORTA |= (1<<LED1);                // led full on
            blinkwhat=0;
            key_clear( 1<<KEY0 );
            first_time =0xff;                        // set first_time all other states
            first_time &= ~( 1<<3)  ;                // clear first_time this state
            }
            pwm_check();						// check various inputs for frequency of pulse on PA5

        if  (get_key_short( 1<<KEY0 )) {         // get debounced keypress short
            cli();
            sendnow=1;                          // set flag so IR handler can send signal
            sei();                               // Interrupt enable
            state=state5;                          // next state 5
            poweroff_delay=POWEROFF_Delay_HALT_long;  // Poweroff delay for halt
            if ( !(PINA & (1<<DELAYTIME)) ) {
              poweroff_delay=POWEROFF_Delay_HALT_short;        // check pin PA6 for delay times (Dip-switch 4 Pos 2 ON)
              }

            }

        if( get_key_long( 1<<KEY0 ))   {         // get debounced keypress long
            cli();
            sendnow=2;                          // set flag so IR handler can send signal
            sei();                                 // Interrupt enable
            state=state5;                          // next state 5
                                                    // Pi will reboot
            poweroff_delay=POWEROFF_Delay_REBOOT_long;  // Poweroff delay for halt
            if ( !(PINA & (1<<DELAYTIME)) ) {
              poweroff_delay=POWEROFF_Delay_REBOOT_short;   // check pin PA6 for delay times (Dip-switch 4 Pos 2 ON)
              }

            }

            // check numer of pulses from Pi, zero means: Pi is not alive
             if (pastpulses < 2 )  {
                state=state5;                           // ok, start power off sequence
                poweroff_delay=POWEROFF_Delay_HALT_long;   // Poweroff delay for halt
                }
        break;

/*------------------------------------------------------------------*/
/*  state 4  Power ON Number 2, special operating state             */
/*  we do not care about pulses from Pi                             */
/*  Power to Pi ist on, led is on                                   */
/*  Short keypress  signals Pi to shut down, changes state to 5     */
/*------------------------------------------------------------------*/
    case state4:
        if (first_time & (1<<4))   {                  // first_time time throu ?
            PORTA |= (1<<LED1);                // led full on
            blinkwhat=0;
            key_clear( 1<<KEY0 );               // ignore keypresses that might have come

            first_time =0xff;                        // set first_time all other states
            first_time &= ~( 1<<4)  ;                // clear first_time this state
            }
            pwm_check();                        //  Pulse generation
                                                //  Check DIP-Switch (PINB0 to PINB2)

        if  (get_key_short( 1<<KEY0 ))  {        // get debounced keypress short
            cli();
            sendtopi(1);                        // set flag so IR handler can send signal
            sei();                               // Interrupt enable
            state=state5;
            _delay_ms(10);
            poweroff_delay=POWEROFF_Delay_HALT_long;  // Poweroff delay for halt

            }
        break;

/*------------------------------------------------------------------*/
/*  state 5  Activate Power off, prepare to shut off                */
/*  irrelevant of signals from Pi                                   */
/*  Power to Pi ist still on, led is blinkng slow                   */
/*  Short keypress  changes to state 4 (keep power on regardless    */
/*  of signal from Pi)                                              */
/*  Long keypress switches off immediately (goto state 1)           */
/*  goto stand by state if timer runs out                           */
/*------------------------------------------------------------------*/
    case state5:
        if (first_time & (1<<5))   {                  // first_time time throu ?
            blinkwhat=REGULAR_Blink;            // set led to blink
            blinkint=POWEROFF_Blink_int;
            key_clear( 1<<KEY0 );               // ignore keypresses that might have come
            pastpulses=0;
            tick2=0;                            //start timer
            sekunde=0;
            first_time =0xff;                        // set first_time all other states
            first_time &= ~( 1<<5)  ;                // clear first_time this state
            }
  //          pwm_check();                        //  Pulse generation
                                                //  Check DIP-Switch (PINB0 to PINB2)

       if  (get_key_short( 1<<KEY0 )) {         // get debounced keypress short
            state=state3;
            }
        if( get_key_long( 1<<KEY0 ))  {          // get debounced keypress long
            state=state1;                          // next state 5
                                                    // Pi will reboot
            }

/*   removed this - not in state diagram
            // check Signal from Pi, how many pulses have we received
        if (pastpulses > 4)   {                    // Pi seems to ba alive, ok keep power on
            state=state3;
            }
*/
            // wait for POWEROFF_Delay sec before switching off 5 Volt supply
        if ((blinkwhat>0) && (sekunde > poweroff_delay))   {
            blinkwhat=0;
            state=state6;                       // next state is state 5
            }

        break;
/*------------------------------------------------------------------*/
/*  state 6  Last Chance    (check if Pi rebooted)                  */
/*  we are about to switch 5 Volt power off                         */
/*  but before we do that we check the signal from Pi again :       */
/*  If further pulses came in (Pi rebooted) we keep power on        */
/*  If no more pulses came in we go to state 1                      */
/*------------------------------------------------------------------*/
    case state6:
        first_time =0xff;                        // set first_time all other states
        key_clear( 1<<KEY0 );

            // check Signal from Pi
        if (pastpulses > 3)  {
            state=state3;
            }
        else state=state1;

      break;

/*------------------------------------------------------------------*/
/*  state 7  TESTMODE only                                          */
/*  Power to Pi is switched on, green led is on                     */
/*  Orange Led blinks every 3 seconds IF and only if pulses from Pi */
/*  are ok received.                                                */
/*  Short keypress: next state is state1 (off)                      */
/*------------------------------------------------------------------*/
    case state7:
        if (first_time & (1<<7))   {            // first_time time throu ?
            PORTA |= (1<<VPOWER);               //switch 5 volt power on
            PORTA |= (1<<LED1);                // led full on
            blinkwhat=0;
            key_clear( 1<<KEY0 );
            tick2=0;                            //start timer
            sekunde=0;
            blink_led();
            first_time =0xff;                        // set first_time all other states
            first_time &= ~( 1<<7)  ;                // clear first_time this state
            }
       if  (get_key_short( 1<<KEY0 )) {         // get debounced keypress short
            state=state1;
            }

            // check Signal from Pi blink orange led when 2 pulses have been received
            if (pastpulses > 2)  {
                blink_led();
                pastpulses=0;
             }
    }

//----  End of Switch Statement -----------------------------

//----------------------------------------------------------
//  Handle Led blinking - fast, slow or pulse

    switch (blinkwhat)
    {

    case  REGULAR_Blink:            // blink tempo regular
        {
        if (tick2==blinkint)        // this is intervall
            {
            PORTA ^= (1<<LED1);
            tick2=0;
            }
        }

    case PULSED_Blink:              // pulse blink (short pulse)
        {
        if (blinkon==1)
            {
            if (tick2==STANDBY_Blink_int_off)  {
                PORTA ^= (1<<LED1);
                tick2=0;
                blinkon=0;
                }
            }
        else
            {
            if (tick2==STANDBY_Blink_int_on)   {
                PORTA ^= (1<<LED1);
                tick2=0;
                blinkon=1;
                }
            }
        }

    }
//---- End Switch blink
  }
//---- End of for loop
}
//---- End of Program
/*------------------------------------------------------------*/
/*------------------------------------------------------------*/
