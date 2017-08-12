#define _XTAL_FREQ 16000000

#include <xc.h>

// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select (MCLR/VPP pin function is digital input)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = ON        // Internal/External Switchover (Internal/External Switchover mode is enabled)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is enabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = ON       // PLL Enable (4x PLL enabled)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

#define I2C_ADDRESS 111
#define DATA_SIZE_BYTES 12
#define DATA_SIZE_WORDS 6

typedef unsigned char byte;

volatile int adc_channel = 0;

volatile int adc_send_index = 0;

volatile int adc_result[ DATA_SIZE_WORDS ] = 0;

volatile int adc_pins[ DATA_SIZE_WORDS ] = { 3, 6, 2, 1, 0, 7 };


////////////////////////////////////////////////////////////
//
// INTERRUPT HANDLER
//
////////////////////////////////////////////////////////////

void interrupt ISR(void)
{
    //////////////////////////////////////////////////////////
    // I2C SLAVE INTERRUPT
    // Called when there is activity on the I2C bus
    if( PIR1bits.SSP1IF )
    {      
        PIR1bits.SSP1IF = 0; // clear interrupt flag

        if( !SSP1STATbits.D_nA ) // master has sent our slave address
        {
            byte d = SSP1BUF; // read and discard address to clear BF flag

            // Is the master setting up a data READ?
            if( SSP1STATbits.R_nW )
            {              
                SSP1BUF = ((byte*)adc_result)[0];
                adc_send_index = 1;
            }
            else
            {
                // dummy data
                SSP1BUF = 0;
            }
        }
        else // DATA
        {
            if( !SSP1STATbits.R_nW ) // MASTER IS WRITING TO SLAVE
            {
                    //
            }
            else // MASTER IS READING FROM SLAVE
            {                                              
                SSP1CON1bits.WCOL = 0; // clear write collision bit

                SSP1BUF = ((byte*)adc_result)[adc_send_index];

                if( ++adc_send_index >= DATA_SIZE_BYTES )
                {
                    adc_send_index = 0;
                }
            }
        }
        
        SSP1CON1bits.CKP = 1; // release clock
    }
}

////////////////////////////////////////////////////////////
//
// I2C SLAVE INITIALISATION
//
////////////////////////////////////////////////////////////

void i2c_init( byte addr )
{
    SSP1CON1    	= 0b00100110;   // I2C slave mode, with 7 bit address, enable i2c
    SSP1MSK     	= 0b01111111;   // address mask bits 0-6
    SSP1ADD     	= addr<<1;      // set slave address
    PIE1bits.SSP1IE	= 1;
	PIR1bits.SSP1IF	= 0;
}


enum
{
    ADC_CONNECT,
    ADC_ACQUIRE,
    ADC_CONVERT                            
};

byte adcState;

byte adcInput;

#define ADC_AQUISITION_DELAY 100

void doADC()
{
	switch( adcState )
	{
		// Connect ADC to the correct analog input
		case ADC_CONNECT: 
		{                     
            int adc_pin   = adc_pins[ adc_channel ];
			ADCON0        = 0b00000001 | (adc_pin<<2);
			TMR0          = 0;

			adcState      = ADC_ACQUIRE;

			// fall through
		}

		// Waiting for a delay while the ADC input settles
		case ADC_ACQUIRE:
		{
			if( TMR0 > ADC_AQUISITION_DELAY )
			{
				// Start the conversion
				ADCON0bits.GO_nDONE = 1;
				adcState            = ADC_CONVERT;                        
			}

			break;
		}

		// Waiting for the conversion to complete
		case ADC_CONVERT:
		{
			if( !ADCON0bits.GO_nDONE )
			{
				// store the result
				adc_result[adc_channel] = (unsigned int)ADRESH<<8;
				adc_result[adc_channel] |= ADRESL;

				// and prepare for the next ADC
				if( ++adc_channel>=DATA_SIZE_WORDS )
				{	
					adc_channel = 0;
				}

				adcState = ADC_CONNECT;                        
			}
			break;
		}
	}                      
}

void main()
{
	// osc control / 16MHz / internal
	OSCCON      = 0b01111010;

	// configure io
	TRISA       = 0b11111111;                    
	TRISC       = 0b11101111;             
	ANSELA      = 0b11111111;
	ANSELC      = 0b11111111;

	// timer0... configure source and prescaler
	OPTION_REG  = 0b10000100;

	// turn on the ADC
	ADCON1      = 0b10100000; //fOSC/32
	ADCON0      = 0b00000001; // Right justify / Vdd / AD on

	i2c_init(I2C_ADDRESS);

	adcState	= ADC_CONNECT;

	adcInput	= 0;

	// global enable interrupts

	INTCONbits.GIE  = 1;
    INTCONbits.PEIE = 1;

	while(1) 
	{
		doADC();
	}
}