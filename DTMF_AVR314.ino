#include <PinChangeInterruptSettings.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterrupt.h>
//***************************************************************************
//** DTMF Generator based on ATMEL AVR-314 Application note
//***************************************************************************


//#define  Xtal       8000000          // system clock frequency
#define  prescaler  1                // timer1 prescaler
#define  N_samples  128              // Number of samples in lookup table
#define  Fck        F_CPU/prescaler   // Timer1 working frequency
#define  delaycyc   10               // port B setup delay cycles
#define dtmfopin	3			//dtmf output pin 3 = OC2B, timer2
#define ledpin 13
#define IWVpin	7				// counts the impulse
#define IWVstartpin = 8			// when low IWV on the way, high = ready

//************************** SIN TABLE *************************************
// Samples table : one period sampled on 128 samples and
// quantized on 7 bit
//**************************************************************************
const unsigned char auc_SinParam [128] = {
	64,67,70,73,76,79,82,85,88,91,94,96,99,102,104,106,109,111,113,115,117,118,120,
	121,123,124,125,126,126,127,127,127,127,127,127,127,126,126,125,124,123,121,120,
	118,117,115,113,111,109,106,104,102,99,96,94,91,88,85,82,79,76,73,70,67,64,60,57,
	54,51,48,45,42,39,36,33,31,28,25,23,21,18,16,14,12,10,9,7,6,4,3,2,1,1,0,0,0,0,0,
0,0,1,1,2,3,4,6,7,9,10,12,14,16,18,21,23,25,28,31,33,36,39,42,45,48,51,54,57,60};

//***************************  x_SW  ***************************************
//Table of x_SW (excess 8): x_SW = ROUND(8*N_samples*f*510/Fck)
//**************************************************************************

//#define SWC(fx)  (8*N_samples*fx*510/Fck)
//high frequency (coloun)
//1209hz  ---> x_SW = 79
//1336hz  ---> x_SW = 87
//1477hz  ---> x_SW = 96
//1633hz  ---> x_SW = 107
// at 16 MHz div by 2
const unsigned char auc_frequencyH [4] = {40, 44, 48,53};

//low frequency (row)
//697hz  ---> x_SW = 46
//770hz  ---> x_SW = 50
//852hz  ---> x_SW = 56
//941hz  ---> x_SW = 61
// at 16 MHz div by 2
const unsigned char auc_frequencyL [4] = {23, 25, 28, 31};

const String digits = "123A456B789C*0#D";

//**************************  global variables  ****************************
volatile unsigned char x_SWa = 0x00;               // step width of high frequency
volatile unsigned char x_SWb = 0x00;               // step width of low frequency
unsigned int  i_CurSinValA = 0;           // position freq. A in LUT (extended format)
unsigned int  i_CurSinValB = 0;           // position freq. B in LUT (extended format)
unsigned int  i_TmpSinValA;               // position freq. A in LUT (actual position)
unsigned int  i_TmpSinValB;               // position freq. B in LUT (actual position)

volatile unsigned char iwvcounter =0;
volatile unsigned long last_impulse_at = 0;

//**************************************************************************
// Timer overflow interrupt service routine
//**************************************************************************
ISR (TIMER2_OVF_vect)
{
	// move Pointer about step width aheaed
	i_CurSinValA += x_SWa;
	i_CurSinValB += x_SWb;
	// normalize Temp-Pointer

	i_TmpSinValA  =  (char)(((i_CurSinValA+4) >> 3)&(0x007F));
	i_TmpSinValB  =  (char)(((i_CurSinValB+4) >> 3)&(0x007F));
	// calculate PWM value: high frequency value + 3/4 low frequency value
	OCR2B = (auc_SinParam[i_TmpSinValA] + (auc_SinParam[i_TmpSinValB]-(auc_SinParam[i_TmpSinValB]>>2)));
	
}

// counts the HW impulse from IWVpin on the RISING edge
void ISR_countImpulse (void)
{
	//noInterrupts(); ToDo not sure if this is needed
	unsigned long diff = millis() - last_impulse_at;
	
	if (diff > 500){
		//if we have not been called that long it is a new digit
		iwvcounter = 1;
	last_impulse_at = millis();
	}
	else if (diff > 50) {
		// we are in the middle of a rotating disk
		// we check for bouncing, normal impulse take 100 ms (60/40)
		// if diff is too short we ignore it
		iwvcounter++;
		last_impulse_at = millis();
	};
	//interrupts();

}


//**************************************************************************
// Initialization
//**************************************************************************
void setup ()
{
	Serial.begin(9600);

	//setup impulse counter ISR
	pinMode(IWVpin, INPUT_PULLUP);
	attachPCINT(digitalPinToPinChangeInterrupt(IWVpin), ISR_countImpulse, RISING );

	//setup DTMF output
	pinMode(dtmfopin, OUTPUT);
	noInterrupts();           // disable all interrupts
	//ToDo is that correct?
	TCCR2A = 0;
	TCCR2B = 0;
	TCNT2  = 0;
	TCCR2A = (1<<COM2B1)+(1<<WGM20);   // non inverting / 8Bit PWM
	TCCR2B = (1 << CS20);  // CLK/1
	bitClear(TIMSK2,TOIE2);
	interrupts();             // enable all interrupts
	// timer interrupt is enabled during dialing: bitSet(TIMSK2,TOIE2)

}

void dialNumber(String nrstr) {

	char digitchar;
	unsigned short digitpos;

	for (int i=0; i< nrstr.length(); i++) {

		digitchar = nrstr.charAt(i); //get char by char from dial number string
		digitpos = digits.indexOf(digitchar); //get position

		x_SWa = auc_frequencyH[(digitpos & 0x03)];	// column of 4x4 DTMF Table
		x_SWb = auc_frequencyL[(digitpos /4)]; //row of DTMF Table

		bitSet(TIMSK2,TOIE2); // timer interrupt on
		delay (100); //tone duration
		//tone off
		bitClear(TIMSK2,TOIE2);
		delay (60); // pause
	}

}

unsigned long diff2 = 0;
int newdigit = 0;
void loop ()
{	

//we had a racecondition, so make it atomar
noInterrupts();
diff2 = millis () - last_impulse_at;
interrupts();

	if ((iwvcounter > 0) && (diff2 > 500)) {
		// we have a new digit
		newdigit = iwvcounter%10;
		Serial.print(newdigit);
		dialNumber(String(newdigit,DEC));
		iwvcounter = 0;
	}

}
