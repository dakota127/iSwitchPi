/************************************************************************/
/*  Pulse Generation on output Pin PINA5  on the ATtiny44               */
/*                                                                      */
/*  PINA7 defines Pulse Generation on/off                               */
/*  PINB2/PINB1 define frequency                                        */
/*  PINB0 defines auto power on                                         */
/*  Frequency is determined by input pins PINB1/PINB2                   */
/*  Timer1 mode ist Fast PWM                                            */
/*  PINA7 PINB2   PINB1        FREQUENCY                                */
/*    0      0       0         Pulse generation off                     */
/*    1      0       0         1 HZ                                   */
/*    1      0       1         10 HZ                                     */
/*    1      1       0         50 HZ                                     */
/*    1      1       1         100 HZ                                    */
/*                                                                      */ 
/*                                                                      */
/*                                                                      */
/*  Duty Cycle is ~10%, set with  OCR1B                                 */
/*                                                                      */
/*  Functions are  called by iswitch.c                                  */
/*                                                                      */
/*  ----> PINA5 is pulse out (and orange LEd)                           */
/* This c-Code runs on ATtiny44                                         */
/* Written by:                                                          */
/* Peter K. Boxler, December 2016                                         */
/************************************************************************/

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* Fast PWM */
#include <square.h>

static uint8_t  pinold,was, first=1, generate_pulse;

//---------------------------------------------------------
// Function pwm_init()                          
//  Initialize Ports and Timer on the ATtiny44
//
void pwm_init(void) {
	DDRA |= (1<<PINA5);	                // Outpu Compare Pin OC1B als Ausgang (PA5)

	DDRA &= ~(1<<PINA7);                 // input DIP 2 Position 1
//	DDRB &= ~(1<<PINB0);                 // input DIP 2 Position 4   defined in iswitchpi.c
	DDRB &= ~(1<<PINB1);                 // input DIP 2 Position 3
	DDRB &= ~(1<<PINB2);                 // input DIP 2 Position 2
//    DDRA |= (1<<PINA0);                  // TestLED nur für Test

//	PORTB |= (1<<PINB0)| (1<<PINB1)  | (1<<PINB2);    // pull upp
    	 
	PORTB |= (1<<PINB1)  | (1<<PINB2);    // pull upp
	PORTA |= (1<<PINA7);                              // pull upp

  generate_pulse=0;
  if ( !(PINA & (1<<PINA7)) ) {
    generate_pulse=1;                 // check if pulse generation is required (Dip switch Pos 1 ON)
  
    TCCR1A |=  (1<<COM1B1) ;            // OC1A als nicht invertierender Ausgang
    OCR1A=5;                            // Toggle OC1A/OC1B on Compare Match, set OC0A/OC0B at BOTTOM
                                      // (non-inverting mode)
    TCCR1B |= (1<<WGM12)| (1<<WGM13);   // CTC (Clear Timer on Compare), see Table 12-5
    TCCR1A |= (1<<WGM10)| (1<<WGM11);   // CTC (Clear Timer on Compare), see Table 12-5

    TCCR1B |=  (1<<CS10) |  (1<<CS12);  // Clock select for Counter: Prescaler /1024  Table 12-6
    TIMSK1 &= ~((1<<OCIE1A));           // Timer Interrupt OC1A deaktivieren , no IR needed

	}
	
  else {
	TCCR1B = 0x00;                     // Timer 0 not running if no pulses are required
    TCCR1A = 0x00;                       // 
    PORTA &= ~( 1<<PINA5 );             // orange Led off 
  }
	                          
}

//---------------------------------------------------------
// Function pwm_start()                          
//  Start Timer1
//
void pwm_start(void) {

  if ( !(PINA & (1<<PINA7)) ) {
    generate_pulse=1;                 // check if pulse generation is required (Dip switch Pos 1 ON)
    TCCR1B |= (1<<WGM12)| (1<<WGM13);           // Set fast pwm mode, see Table 12-5
    TCCR1A |= (1<<WGM10)| (1<<WGM11);           
	TCCR1B |=  (1<<CS10) |  (1<<CS12);          // Clock select for Counter: Prescaler /1024  Table 12-6
	TCCR1A |=  (1<<COM1B1) ;                    // OC1A als nicht invertierender Ausgang
	TIMSK1 &= ~((1<<OCIE1A));                   // Timer Interrupt OC1A deaktivieren , no IR needed
  }
  else {
    TCCR1B = 0x00;                              //do not start timer, not requested
    TCCR1A = 0x00;
    generate_pulse=0;     
  }
  
}


//---------------------------------------------------------
// Function pwm_stop()                          
//  Stop Timer 1 -> stops pulse generation
//
void pwm_stop(void) {
    TCCR1B = 0x00;                              //stop Timer/Counter 1
    TCCR1A = 0x00;
    first=1;
}


//---------------------------------------------------------
// Function pwm_acheck)                          
//  Check if Inputs from DIP-Switch (PINB0/PINB1/PINB2) has changed
//  Set OCR1A accordingly or activate ADC
void pwm_check(void) {

  if (!generate_pulse)  {
        return; }       // DIP Switch 1 is off, do nothing

//    if (TCCR1B== 0x00) {return;}      // Timer not on, do nothing

	//    _delay_ms(10);                // for testing
        was= (PINB & 0x6);              // get input from 2 DipSwitches
        if ((first==1) || (pinold != PINB))             // has input changed
        {  
            pinold=PINB;
            first=0;                    // first time thru
//			PORTA &= ~(1<<PINA0);       // shut down test led
            TCCR1B &= ~(1<<CS10) ;      // Clock select for Counter: Prescaler /256  Table 12-6
            TCCR1B &= ~(1<<CS11) ;      // Clock select for Counter: Prescaler /256  Table 12-6
            TCCR1B &= ~(1<<CS12) ;      // Clock select for Counter: Prescaler /256  Table 12-6

          switch (was)                  // what is set on the Dip Switch ? Selects frequency
          {
            case 6:                     // switch 0 0
              TCCR1B |=  (1<<CS10) |  (1<<CS11);  // Clock select for Counter: Prescaler /64  Table 12-6
              OCR1A=15624;                        // 1 HZ / 1000 ms
              OCR1B=1400;                         // set OCR1B value for Timer1 (sets duty cycle)
              break;
            
            case 4:                      // switch 0 1
            
              TCCR1B |=  (1<<CS10) |  (1<<CS11);  // Clock select for Counter: Prescaler /64  Table 12-6
              OCR1A=1562;                         // 10HZ / 100ms
              OCR1B=140;                          // set OCR1B value for Timer1 (sets duty cycle)
              break;
            
            case 2:                     // switch 1 0 
            
              TCCR1B |=  (1<<CS11);             // Clock select for Counter: Prescaler /8  Table 12-6
              OCR1A=2499;                         // 50HZ / 20ms
              OCR1B=250;                          // set OCR1B value for Timer1 (sets duty cycle)
              break;
            
            case 0:                     // switch 1 1
            
              TCCR1B |=  (1<<CS11);         // Clock select for Counter: Prescaler /8  Table 12-6
              OCR1A=1249;                         // 100HZ / 10 ms
              OCR1B=125;                          // set OCR1B value for Timer1 (sets duty cycle)
              break;
            
            default:                            // do 1 sec (as in case 2)
            
              TCCR1B |=  (1<<CS10) |  (1<<CS11);  // Clock select for Counter: Prescaler /64  Table 12-6
              OCR1A=15624;                        // 1 HZ
              OCR1B=1400;                         // set OCR1B value for Timer1 (sets duty cycle)
              break;
            }
            
            return;

        }   // end change input
            
        else // no input change
        {
        
           }       // ende 
}
//  End of Code
//
