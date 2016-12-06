/************************************************************************/
/*  Pulse Generation on output Pin PINA5  on the ATtiny44               */
/*                                                                      */
/*  Frequency is determined by input pins PINB0/PINB1/PINB2             */
/*  Timer1 mode ist Fast PWM                                            */
/*  PINB2   PINB1   PINB0       FREQUENCY                               */
/*  0       0       0           use ADC to set frequency  with OCR1A    */
/*  0       0       1           0.5 HZ                                  */
/*  0       1       0           1 HZ                                    */
/*  0       1       1           5 HZ                                    */
/*  1       0       0           10 HZ                                   */
/*  1       0       1           50 HZ                                   */
/*  1       1       0           100 HZ                                  */
/*  1       1       1           200 HZ                                  */
/*                                                                      */
/*  Duty Cycle is ~10%, set with  OCR1B                                 */
/*                                                                      */
/*  Functions are  called by iswitch.c                                  */
/*                                                                      */
/*  ----> PINA6 is analog In                                            */                                                                */
/*  ----> PINA5 is pulse out                                            */
/*  ----> PINA7 is pulse generation on/off                              */
/* This c-Code runs on ATtiny44                                         */
/* Written by:                                                          */
/* Peter K. Boxler, Januar 2016                                         */
/************************************************************************/
// 
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* Fast PWM */
#include <square.h>

//---------------------------------------------------------
// Function pwm_init()                          
//  Initialize Ports and Timer on the ATtiny44
//
void pwm_init(void) {
	DDRA |= (1<<PINA5);	                // Outpu Compare Pin OC1B als Ausgang (PA5)

	DDRB &= ~(1<<PINB0);                 // input DIP Switch
	DDRB &= ~(1<<PINB1);                 // input DIP Switch
	DDRB &= ~(1<<PINB2);                 // input DIP Switch
//    DDRA |= (1<<PINA0);                  // TestLED nur für Test
    	 
	PORTB |= (1<<PINB0)| (1<<PINB1)  | (1<<PINB2);    // pull upp
	PORTA |= (1<<PINA7);                                // pull upp

	TCCR1A |=  (1<<COM1B1) ;             // OC1A als nicht invertierender Ausgang
	OCR1A=5;                       // Toggle OC1A/OC1B on Compare Match, set OC0A/OC0B at BOTTOM
                                        // (non-inverting mode)
    TCCR1B |= (1<<WGM12)| (1<<WGM13);               // CTC (Clear Timer on Compare), see Table 12-5
    TCCR1A |= (1<<WGM10)| (1<<WGM11);               // CTC (Clear Timer on Compare), see Table 12-5

	TCCR1B |=  (1<<CS10) |  (1<<CS12);    // Clock select for Counter: Prescaler /1024  Table 12-6
	TIMSK1 &= ~((1<<OCIE1A));            // Timer Interrupt OC1A deaktivieren , no IR needed

	ADCSRA |= (1 << ADEN)|          // Analog-Digital enable bit
	(1 << ADPS1);     // set prescaler to divison 8  (Table 17-5)
	
	ADCSRB &= ~(1<<ADLAR);  // clear Adlar D result store in (more significant bit in ADCH)
 
	ADMUX |=   (1 << MUX1) | (1 << MUX2);   // Choose AD input (single ended) (PA6)  
	
	                          
}

//---------------------------------------------------------
// Function pwm_start()                          
//  Start Timer1
//
void pwm_start(void) {
    TCCR1B |= (1<<WGM12)| (1<<WGM13);           // Set fast pwm mode, see Table 12-5
    TCCR1A |= (1<<WGM10)| (1<<WGM11);           
	TCCR1B |=  (1<<CS10) |  (1<<CS12);          // Clock select for Counter: Prescaler /1024  Table 12-6
	TCCR1A |=  (1<<COM1B1) ;                    // OC1A als nicht invertierender Ausgang
	TIMSK1 &= ~((1<<OCIE1A));                   // Timer Interrupt OC1A deaktivieren , no IR needed
	
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
// Function pwm_adc()                          
//  Do ADC , analog Input from potentiometer is on PINA6
//  Set OCR1A with the result
void pwm_adc(void) {

uint8_t adc_lobyte,adc_hibyte;
uint16_t raw_adc;
static uint16_t raw_adc_old;

            
            TCCR1B |=  (1<<CS10) |  (1<<CS12);      // Clock select for Counter: Prescaler /1024  Table 12-6

		    ADCSRA |= (1 << ADEN);                  // Analog-Digital enable bit im ADC Control und Status Register A
		                                            // dies bit null setzten turns ADC off
		    ADCSRA |= (1 << ADSC);                  // Discarte first conversion
		                                            // solange dies one ist, läuft conversion
		                                            // es gibt auuch noch ADCSRB ADC Control und Status Register B 

		    while (ADCSRA & (1 << ADSC)) { 
			// wait until conversion is done
		    }
//		    raw_adc = ADC; // add lobyte and hibyte
		    ADCSRA |= (1 << ADSC);                  // start single conversion

		    while (ADCSRA & (1 << ADSC)) 
		    {
			// wait until conversion is done
			ADCSRA &= ~(1<<ADEN);                   // shut down the ADC
		    }

		    adc_lobyte = ADCL;
		    adc_hibyte = ADCH;   
		    raw_adc = adc_hibyte<<8 | adc_lobyte;   // add lobyte and hibyte
            if (raw_adc != raw_adc_old)             // has analog input changed ?
		        {
//		        PORTA ^= (1<<PINA0);                // toggle test led  (for testing signal to Pi)

		        OCR1AH = adc_hibyte;   
		        OCR1AL = adc_lobyte;   
    		    OCR1A =   raw_adc;                  // set OCR1A value for Timer1 (sets frequency)

		        OCR1B =   raw_adc/10;               // set OCR1B value for Timer1 (sets duty cycle)
                raw_adc_old=raw_adc;
                }
}

//---------------------------------------------------------
// Function pwm_acheck)                          
//  Check if Inputs from DIP-Switch (PINB0/PINB1/PINB2) has changed
//  Set OCR1A accordingly or activate ADC
void pwm_check(void) {


//    if (TCCR1B== 0x00) {return;}      // Timer not on, do nothing

	//    _delay_ms(10);                // for testing
        was= (PINB ^ 0x7);              // get input from 3 DipSwitches
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
            case 0:
            {
            TCNT1=0 ; 
            pwm_adc();
            return;
            }

            case 1:
            {

            TCCR1B |=   (1<<CS12);              // Clock select for Counter: Prescaler /256  Table 12-6
            OCR1A=7812;                         // 0.5 HZ
            OCR1B=700;                          // set OCR1B value for Timer1 (sets duty cycle)
            ADCSRA &= ~(1<<ADEN);               // shut down the ADC
            return;
            }
            case 2:
            {
            TCCR1B |=  (1<<CS10) |  (1<<CS11);  // Clock select for Counter: Prescaler /64  Table 12-6
            OCR1A=15625;                        // 1 HZ
            OCR1B=1400;                         // set OCR1B value for Timer1 (sets duty cycle)
            ADCSRA &= ~(1<<ADEN);               // shut down the ADC
            return;
            }
            
            case 3:
            {
            TCCR1B |=  (1<<CS10) |  (1<<CS11);  // Clock select for Counter: Prescaler /64  Table 12-6
            OCR1A=3125;                         // 5 Hz
            OCR1B=300;                          // set OCR1B value for Timer1 (sets duty cycle)
            ADCSRA &= ~(1<<ADEN);               // shut down the ADC
            return;

            }
            case 4:
            
            {
            TCCR1B |=  (1<<CS10) |  (1<<CS11);  // Clock select for Counter: Prescaler /64  Table 12-6
            OCR1A=1562;                         // 10HZ
            OCR1B=140;                          // set OCR1B value for Timer1 (sets duty cycle)
            ADCSRA &= ~(1<<ADEN);               // shut down the ADC
            return;
            }
            case 5:
            
            {
            TCCR1B |=  (1<<CS11) ;              // Clock select for Counter: Prescaler /8 Table 12-6
            OCR1A=2500;                         // 50HZ
            OCR1B=250;                          // set OCR1B value for Timer1 (sets duty cycle)
            ADCSRA &= ~(1<<ADEN);               // shut down the ADC
            return;
   
            }
            case 6:
            {
            TCCR1B |=   (1<<CS11);              // Clock select for Counter: Prescaler /8  Table 12-6
            OCR1A=1250;                         // 100HZ
            OCR1B=100;                          // set OCR1B value for Timer1 (sets duty cycle)
            ADCSRA &= ~(1<<ADEN);               // shut down the ADC
            return;
            }
            
            case 7:
            {
            TCCR1B |=   (1<<CS10);              // Clock select for Counter: Prescaler /1  Table 12-6
            OCR1A=5000;                         // 200HZ
            OCR1B=500;                          // set OCR1B value for Timer1 (sets duty cycle)
            ADCSRA &= ~(1<<ADEN);               // shut down the ADC
            return;
            }
            
            default:                            // do 1 sec (as in case 2)
            {
            TCCR1B |=  (1<<CS10) |  (1<<CS11);  // Clock select for Counter: Prescaler /64  Table 12-6
            OCR1A=15625;                        // 1 HZ
            OCR1B=1400;                         // set OCR1B value for Timer1 (sets duty cycle)
            ADCSRA &= ~(1<<ADEN);               // shut down the ADC
            return;
            }

        }   // end switch
        }   // end change input
            
        else // no input change
        {
        
        if (was == 0)                       // all three inputs are low 
            {
            pwm_adc();                      // ADC is used to set OCR1A
            return;
            }        
           
		}       // ende was==0
}
