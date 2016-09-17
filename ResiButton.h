/* 
* ResiButton.h
*
* Created: 20.08.2016 15:01:34
* Author: daubi
* ResiButton class with Interrupt an state
*
*/


#ifndef __ResiButton_H__
#define __ResiButton_H__

#include "Arduino.h"


class ResiButton
{
//variables
public:

volatile unsigned long last_time; //ISR last time call debounced
volatile unsigned char last_level; // high or low
 
volatile unsigned long diff_event; //for debuging
volatile unsigned char isr_cnt;
unsigned int pinNr;

protected:
private:
volatile unsigned char state; //0 nothing, 1 pressed, 2 short, 3 long, 

//functions
public:
	ResiButton();
	ResiButton(unsigned int pin);
	~ResiButton();
	unsigned char getState();
	void clearState();

void handle_change(void);


protected:
private:
	ResiButton( const ResiButton &c );
	ResiButton& operator=( const ResiButton &c );


}; //ResiButton

#endif //__ResiButton_H__
