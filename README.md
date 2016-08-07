# ArduinoDTMF
DTMF with arduino 

Based on Atmel application note AVR-314 and inspired by
https://www-user.tu-chemnitz.de/~heha/basteln/Haus/Telefon/Impulswahl%E2%86%92DTMF/

It is working with a W48 dialer and a Fritzbox.

PWM output on Pin 4 needs a passive filter.
I used a RLC low pass filter. R = 46 Ohm, L unknown, C = 4.7 uF.
Coupled into the phone at microphone, with a 100 nF capacitor.
Spectral analysis (done with a sound card and audacity) looks terrible,
but noise is 15 to 20 dB off the signal. Frequency accuracy is < 1%.
Good enough.

If you want to adapt it to different HW you have to care:
- timer register TCCR2A, TCCR2B, OCR2B, TIMSK2
- clock frequency : step width auc_frequencyH and auc_frequencyL
- clock frequency : low pass filter for the PWM output pin
- tone duration: look for delay(100) and delay(60) in dialNumber

Be careful: telephone uses voltage up to 75 Volt and inductors might generate nasty peaks, destroying your Arduino. I assume that you will use it for your in-house VoIP box.
If you connect it to the last mile to your phone service provider consider all the safety rules for outdoor lines.

Licence:
Do what ever you like with that code. Except bother me nor others.

ToDo
usage of delay() might not be the best solution, if you do not want to block the CPU.
But you need to add callbacks and have to ensure the pause length. That makes it
tricky. 
And what else to do during dialing? DTMF ISR generates 31K timer interrupts per second.
The DTMF ISR might care about tone duration. But than you have to poll anyhow, whether 
tone duration is passed.
During pause you might go into sleep mode to save some power.


