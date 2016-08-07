# ArduinoDTMF
DTMF with arduino 

Based on Atmel application note AVR-314 and inspired by
https://www-user.tu-chemnitz.de/~heha/basteln/Haus/Telefon/Impulswahl%E2%86%92DTMF/

It is working with a W48 dialer and a Fritzbox.

PWM output on Pin 4 needs a passive filter.
I used a RLC low pass filter. R = 46 Ohm, L unknown, C = 4.7 uF.
Coupled into the phone at microphone, with a 100 nF capacitor.
Spectral analysis (done with a sound card an audacity) looks terrible,
but noise is 15 to 20 dB of the signal. Frequency accuracy is < 1%.
Good enough.

If you want to adapt it to different HW you have to care:
- timer register TCCR2A, TCCR2B, OCR2B, TIMSK2
- clock frequency : step width auc_frequencyH and auc_frequencyL
- clock frequency : low pass filter for the PWM output pin

Be careful: telephone uses voltage up to 75 Volt and inductors might generate nasty peaks, destroying your Arduino. I assume that you will use it for your in-house VoIP box.
If you connect it to the last mile to your phone service provider consider all the safety rules for outdoor lines.


