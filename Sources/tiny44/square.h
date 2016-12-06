/* -----------------------------------------------------------------------
 * Title: Analog input changes Frequence of square wave - with PWM
 * Hardware: ATtiny44
 * Diese Version 7 Hz bis ca 160 Hz
 * -----------------------------------------------------------------------*/

/* Fast PWM */
void pwm_init(void);
void pwm_adc(void);
void pwm_stop(void);
void pwm_start(void);
void pwm_check(void);
static uint8_t  pinold, was, first=1;
