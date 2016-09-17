/*
* ResiButton.cpp
*
* Created: 20.08.2016 15:01:34
* Author: daubi
*/


#include "ResiButton.h"

//#include <EnableInterrupt.h>

// default constructor
ResiButton::ResiButton()
{
} //ResiButton

// default destructor
ResiButton::~ResiButton()
{
} //~ResiButton

ResiButton::ResiButton(unsigned int pin)
{
	this->pinNr = pin;
	pinMode(pinNr,INPUT_PULLUP);
	clearState();
}

void ResiButton::handle_change(void)
{
	//we try to minimize action here
	//state decision is done in getState
	//debouncing
	if (this->state < 2) { //we still looking for a final state
		unsigned long new_event = millis();
		if ((new_event - last_time) > 10) {
			this->diff_event = new_event - last_time;
			this->last_time = new_event;
			this->isr_cnt++;
		}
	}
}

unsigned char ResiButton::getState()
{

	if (isr_cnt ==0) { //no Interrupt since last clear
		state = 0;
		return state;
	}
	if (state >1) return state; // we have a state, new state only after clearState

	//we check the last event and decide the state
	if (isr_cnt & 0x01) {
		//we are still pressed
		this->state = 1;
		} else  if (diff_event > 900) {
		//button was down for longer then 900 ms
		state = 3;
	} else state = 2;
	return state;
}

void ResiButton::clearState() {
	// after state was consumed, clear it
	// we might run in race condition ...
	this->state = 0;
	diff_event = 0;
	isr_cnt=0;

}


