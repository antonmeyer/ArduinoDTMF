#include <Button.h>
#include <PinChangeInterruptSettings.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterrupt.h>


#include "ResiButton.h"
#include <U8glib.h>

//***************************************************************************
//** DTMF Generator based on ATMEL AVR-314 Application note
//***************************************************************************

#define  prescaler  1                // timer2 prescaler
#define  N_samples  128              // Number of samples in lookup table
#define  Fck        F_CPU/prescaler   // Timer2 working frequency

#define dtmfopin	3			//dtmf output pin 3 = OC2B, timer2
#define IWVpin		7				// counts the impulse

//************************************************************************
//** NSA switch is closed when the rotary disk is moved
//** if user hold the disk for a longer time than the state is changed
//** advantage: no need for extra button for extra function, 1 finger usage
//** disadvantage: tricky timing
//** stop nsa time measurement as soon IWVcounter > 0
//** giving user feedback with tone might interfer with first IWV impulse; both use ISR
//**	-> IWV ISR disable DTMF ISR
//*************************************************************************
Button nsa_switch(10);
unsigned long nsa_start; // used for measure the duration
unsigned short nsa_state = 0; //define the state
#define NSA_short 1500 //ms to enter state 1
#define NSA_long 3000 // ms to enter state 2

ResiButton earthB(9); // = new Button(9);				// flash or earth button for special functions
// if pressed short:
// short dialing phone numbers stored position 1 ..5, 0 = last number
// pressed short and dialed 7 -> *, 9 -> #
// if long pressed
// init number storage function, enter number, finish long press, dial position
//if you made a mistake, just hang up = boot the duino

#define SDA			A4
#define SCL			A5
#define I2C_SLA     (0x3c*2) //i2c address of the display

//the OELD Display
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_FAST);  // VDD=3V GND=GND SCL=A5 SDA=A4

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
//ToDo calculate values in the preprocessor ..but be careful rounding errors
// might violate 1.5% frequency tolerance of DTMF
//**************************************************************************

#define SWC(x)  ((x)*128.0*8*510/Fck)
//high frequency (column)
//1209hz  ---> x_SW = 79
//1336hz  ---> x_SW = 87
//1477hz  ---> x_SW = 96
//1633hz  ---> x_SW = 107

const char auc1KHz = SWC(1000);
const char auc600Hz = SWC(600);

#if Fck == 8000000 // 8 MHz
const unsigned char auc_frequencyH [4] = {79,87,96,107}; //8MHz
const unsigned char auc_frequencyL [4] = {46,50,56,62}; //8MHz
#else
const unsigned char auc_frequencyH [4] = {40, 44, 48,53}; //@16MHz
const unsigned char auc_frequencyL [4] = {23, 25, 28, 31}; //@16MHz
#endif
//const unsigned char auc_frequencyH [4] = {SWC(1209), SWC(1336), SWC(1477), SWC(1633)};
//low frequency (row)
//697hz  ---> x_SW = 46
//770hz  ---> x_SW = 50
//852hz  ---> x_SW = 56
//941hz  ---> x_SW = 61

//

const String alldigits = "123A456B789C*0#D"; // position for lookup into Freq. Table

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
// Timer overflow interrupt service routine for DTMF sine wave
//**************************************************************************
ISR (TIMER2_OVF_vect)
{
	// move Pointer about step width ahead
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

	//ToDo clean up; reset of counter is done outside; debouncing did not work reliable and done with capacitor
	//noInterrupts(); ToDo not sure if this is needed
	/*unsigned long diff = millis() - last_impulse_at;
	
	if (diff > 300){
	//if we have not been called that long it is a new digit
	iwvcounter = 1;
	last_impulse_at = millis();
	}
	else
	*/
	if ((millis() - last_impulse_at) > 25) {
		// we are in the middle of a rotating disk
		// we check for bouncing, normal impulse take 100 ms (60/40)
		// if diff is too short we ignore it
		iwvcounter++;
		last_impulse_at = millis();
	};
	//interrupts();

}

void ISR_earthButton (void)
{
	earthB.handle_change();
}


void dialNumber(String nrstr) {
	char digitchar;

	for (byte i=0; i< nrstr.length(); i++) {
		digitchar = nrstr.charAt(i); //get char by char from dial number string
		dialDigit(digitchar);
	}
}

void dialDigit(char digitchar) {

	unsigned short digitpos = alldigits.indexOf(digitchar); //get position

	x_SWa = auc_frequencyH[(digitpos & 0x03)];	// column of 4x4 DTMF Table
	x_SWb = auc_frequencyL[(digitpos /4)]; //row of DTMF Table

	pinMode(dtmfopin, OUTPUT); //output pin ready
	bitSet(TIMSK2,TOIE2); // timer interrupt on
	delay (100); //tone duration
	//tone off
	pinMode(dtmfopin, INPUT); //make it high impedance
	bitClear(TIMSK2,TOIE2);
	delay (50); // pause
}

void sendTone (char auc_tone, unsigned int duration) {
	
	// a bit ugly hack to reuse the DTMF tone generation ISR
	x_SWa = auc_tone;
	x_SWb = auc_tone;

	pinMode(dtmfopin, OUTPUT); //output pin ready
	bitSet(TIMSK2,TOIE2); // timer interrupt on
	delay (duration); //tone duration
	//tone off
	pinMode(dtmfopin, INPUT); //make it high impedance
	bitClear(TIMSK2,TOIE2); //switch off ISR

}

//**************************************************************************
// Initialization
//**************************************************************************
void setup ()
{
	Serial.begin(57600); //for debug purpose

	//setup impulse counter ISR
	pinMode(IWVpin, INPUT_PULLUP);
	attachPCINT(digitalPinToPCINT(IWVpin),  ISR_countImpulse, RISING);

	//init button ISR
	attachPCINT(digitalPinToPCINT(earthB.pinNr),  ISR_earthButton, CHANGE);
	
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
	
	nsa_switch.begin();
}

unsigned long diff2 = 0;
unsigned short newdigit = 0;
String numberstr;
String nr_str2 = String("");
unsigned char B_state = 0;

boolean newdraw= true; //we have to draw the display for the first time


void loop ()
{

	//nsa switch check and timing check here; do not forget ivw counter

	if (nsa_switch.toggled()) nsa_start = millis(); // we do not care so much about direction of toggle as extra checks follows
	if (iwvcounter == 0) {
		
		if (nsa_switch.pressed() && (nsa_state < 2)) {
			
			unsigned long nsa_diff = millis() - nsa_start;
			if (nsa_diff > NSA_long ) { // we enter 2nd level
				nsa_state = 2;
				sendTone(auc1KHz, 50); //notify the user that he can release the disk
			} else if ( nsa_diff > NSA_short) {
				// we enter 1st level
				nsa_state = 1;
				sendTone(auc600Hz, 50); //notify the user that he can release the disk
			}
			// it is to short we have to wait
		} //nsa_switch pressed		

	} else { // there has been IWV impulse
		//we had a race condition, so make it atomic
		// ToDo could we double check the race??
		noInterrupts();
		diff2 = millis () - last_impulse_at;
		interrupts();
		if ( (diff2 > 300)) {
			// next impulse would belong to a new digit
			newdigit = iwvcounter%10; // 10 -> 0
			switch (nsa_state) {

			case 0 : numberstr = String(newdigit); break;

			case 1 : numberstr = shift_func (newdigit); 
					nsa_state = 0; // one shot function
					break;

			case 2 : nsa_state = 0; break; // we handle here the storage
			}
			/*
			B_state = earthB.getState();

			if (B_state == 0)
			{ //normal dialing
				numberstr = String(newdigit);
			} else if (B_state == 2)
			{ // shift function
				earthB.clearState(); //clear status as it is a single action
				B_state = 0;
				numberstr = shift_func (newdigit);
			}
			*/
			
			dialNumber(numberstr);
			Serial.print(numberstr);
			nr_str2.concat(numberstr);
			
			iwvcounter = 0;
			newdraw = true;
		}
		}

		if ((B_state < 2) && (earthB.getState() == 2)) { //we´ve  just catch a state change to 2)
			B_state = earthB.getState();
			sendTone(auc1KHz, 50); //signal tone to the user
		}
		
		//picture loop
		if (newdraw || (iwvcounter > 0) ){
			//someone told to draw new, or number disk is just dialed
			u8g.firstPage();
			do {
				draw();
			} while( u8g.nextPage() );
			newdraw = false ; // we have done so
		}

		
		//dialNumber(digits); //test
		//delay(1000);

	}

	void draw(void) {
		// graphic commands to redraw the complete screen should be placed here
		
		u8g.setFont(u8g_font_ncenR14r);
		u8g.setPrintPos(0,14);
		if (nr_str2.length() == 0) {
			u8g.print("Please Dial: ");
		} else u8g.print(nr_str2);

		/*u8g.setPrintPos(0,64);
		u8g.print(earthB.isr_cnt);
		u8g.setPrintPos(45,64);
		u8g.print(earthB.getState());
		u8g.setPrintPos(60,64);
		u8g.print(earthB.diff_event);
		*/

		if (earthB.getState() == 2) {
			u8g.setPrintPos(5,50);
			u8g.print("^");
		}

		if (iwvcounter > 0) {
			u8g.setFont(u8g_font_ncenR24n);
			u8g.setPrintPos(45,64);
			u8g.print(iwvcounter%10);
		}

		//getStrWidth
	}

	String shift_func (unsigned short digit) {

		if (digit == 7) return "*";
		
		if (digit == 9) return "#";

		

		//if 1..5 get stored number

		return "";
	}




