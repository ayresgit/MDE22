#include "adc.h"

volatile float batvoltage, position, batcurrent;

volatile uint8_t print_adc;

//#define ADC_8BITS
#define ADC_TIMER_PRESCALER 64

/**
 * @brief inicializa o ADC, configurado para conversão engatilhada com o timer0.
 */
void adc_init(void)
{
    clr_bit(PRR, PRADC);                           // Activates clock to adc

    // configuracao do ADC
    PORTC   =   0b00000000;                         // disables pull-up for adcs pins
    DDRC    =   0b00000000;                         // all adcs as inputs
    DIDR0   =   0b11111111;                         // ADC0 to ADC2 as adc (digital disable)

    ADMUX   =   (0 << REFS1)                        // AVcc with external capacitor at AREF pin
            | (1 << REFS0)
#ifdef ADC_8BITS
            | (1 << ADLAR);                         // ADC left adjusted -> using 8bits ADCH only
#else
            | (0 << ADLAR);                         // ADC left adjusted -> using all 10 bits
#endif

    ADMUX = (ADMUX & 0xF8) | ADC1;

    ADCSRA  =   (1 << ADATE)                        // ADC Auto Trigger Enable
            | (1 << ADIE)                           // ADC Interrupt Enable
            | (1 << ADEN)                           // ADC Enable
            | (1 << ADSC)                           // Do the first Start of Conversion
            | (1 << ADPS2)                          // ADC Prescaller = 128;
            | (1 << ADPS1)
            | (1 << ADPS0);

    ADCSRB  =   (0 << ADTS2)                        // Auto-trigger source: timer0 Compare Match A
            | (1 << ADTS1)
            | (1 << ADTS0);

    // TIMER configurations
    clr_bit(PRR, PRTIM1);                          // Activates clock to timer1 (timer0 is used by application PWM)
    // MODE 2 -> CTC with TOP on OCR1
    TCCR1A  =   (0 << WGM11) | (0 << WGM10)         // mode 2
            | (0 << COM1B1) | (0 << COM1B0)         // do nothing
            | (0 << COM1A1) | (0 << COM1A0);        // do nothing
    TCCR1B  =
#if ADC_TIMER_PRESCALER ==     1
                (0 << CS12) | (0 << CS11) | (1 << CS10) // Prescaler N=1
#elif ADC_TIMER_PRESCALER ==   8
                (0 << CS12) | (1 << CS11) | (0 << CS10) // Prescaler N=8
#elif ADC_TIMER_PRESCALER ==   64
                (0 << CS12) | (1 << CS11) | (1 << CS10) // Prescaler N=32
#elif ADC_TIMER_PRESCALER ==   256
                (1 << CS12) | (0 << CS11) | (0 << CS10) // Prescaler N=64
#elif ADC_TIMER_PRESCALER ==   1024
                (1 << CS12) | (0 << CS11) | (1 << CS10) // Prescaler N=128
#else
                0
#endif
                | (1 << WGM12) | (0 << WGM13)      // mode ctc
                | (0 << ICNC1) | (0 << ICES1);


    TCNT1 = 0;              // Disable read/write direct access to the timer counter
    OCR1A = 82;             // OCR1A = TOP = fcpu/(N*2*f) -1

    TIMSK1 |=   (1 << OCIE1A);//  | (0 << OCIE1B)        // Ativa a interrupcao na igualdade de comparação do TC1 com OCR1A, desativa OCR1B 
            //| (0 << ICIE1) ;                // Desativa input capture interrupt

#ifdef DEBUG_ON
    set_bit(DDRD, LED1);
#endif

}

/**
 * @brief MUX do ADC
 */
ISR(ADC_vect)
{
    cli(); 
    static const float batvoltage_coeff = 1; //0.06582490575070313f;
    static const float position_coeff = 1; //0.06717781789490249f;
    static const float batcurrent_coeff = 1; //0.01599315004f;

    uint16_t adc = ADC;                     // read adc
    uint8_t channel = ADMUX & 0x07;         // read channel

    /*
    cpl_bit(PORTD, LED1);
    usart_send_uint8(ADMUX);
    usart_send_char(':');
    usart_send_uint16(ADC);
    usart_send_char('\n');
    */

    switch(channel){
        case ADC1:
            batvoltage = adc * batvoltage_coeff;
            break;

        case ADC2:                       
            position = adc * position_coeff;
            break;

        case ADC3:
            batcurrent = adc * batcurrent_coeff;
        default:
            channel = 0;             // recycle

            print_adc = 1;
#ifdef DEBUG_ON
            set_bit(PORTD, LED1);
#endif
            // call control action 
#ifdef DEBUG_ON
            clr_bit(PORTB, LED1);
#endif  
            break;
    }

    ADMUX = (ADMUX & 0xF8) | ++channel;   // select next channel
    //ADCSRA = ADCSRA;                  // rearm for next conversion if TIMER0 not in use

    sei();
}

/**
 * @brief ISR necessária para auto-trigger do ADC. Caso contrário, dispara
 * BADISR_vect.
 */
EMPTY_INTERRUPT(TIMER1_COMPA_vect);


