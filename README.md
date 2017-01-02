# ArduinoDTMF
DTMF with arduino 

Based on Atmel application note AVR-314 and inspired by
https://www-user.tu-chemnitz.de/~heha/basteln/Haus/Telefon/Impulswahl%E2%86%92DTMF/

It is working with a W48 dialer and a Fritzbox.

PWM output on Pin 3 needs a passive filter.
I used a RC filter. R = 47 Ohm, C = 1 uF.
Coupled into the phone at microphone.
Spectral analysis (done with a sound card and audacity) looks terrible,
but noise is 15 to 20 dB off the signal. Frequency accuracy is < 1%.
Good enough.

Pin 7 is connected to the impulse rotary dialer contact. This "number" switch
is connected to the arduino only. Remove all conncetion to the old phone.
Pin 9 is the other dial switch, closed as soon the dial disk is moved.

Scheme is in the picture directory. Number switches pin should be reviewed for your layout.

If you want to adapt it to different HW you have to care:
- dtmfopin: look for the timer / PWM output pins
- timer register TCCR2A, TCCR2B, OCR2B, TIMSK2
- clock frequency : step width auc_frequencyH and auc_frequencyL
- clock frequency : low pass filter for the PWM output pin
- tone duration: look for delay(100) and delay(60) in dialNumber

Be careful: telephone uses voltage up to 75 Volt and inductors might generate nasty peaks, destroying your Arduino. I assume that you will use it for your in-house VoIP box.
If you connect it to the last mile to your phone service provider consider all the safety rules for outdoor lines.

Licence:
Do What (ever) You Like with that code (DWYL = devil licence). Except bother me or others.

ToDo
usage of delay() might not be the best solution, if you do not want to block the CPU.
But than you need to add callbacks and have to ensure the pause length. That makes it
tricky. 
And what else to do during dialing? DTMF ISR generates 31K timer interrupts per second.
The DTMF ISR might care about tone duration. But than you have to poll anyhow, whether 
tone duration is passed and ensure pause length.
During pause you might go into sleep mode to save some power.


