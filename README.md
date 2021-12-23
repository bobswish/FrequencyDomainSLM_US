Readme for FreqDomainSLM_US
This code intended for use with the Tympan Rev-E.
It is a prototype (uncalibrated) detector of unexpected audio energy above 20 kHz.
Audio is input via the onboard mic, and output through the headphone jack.  The left channel
audio output has a variable frequency down shift (10, 20, or 25 kHz) while the right channel has a lowpass filter.
This allows the user to determine for themselves if any high frequency energy is irregular.  
Audio can be recorded to the SD card.
If the level in any of the 1/3 octave bands between 20 and 40 kHz is higher 
than the level in the 1 octave band just below 20 kHz, the green LED turns on 
and the message "Ultrasound detected" prints on the serial monitor.
The band levels are corrected by the bandwidth such that the levels 
will be equivalent despite the varied bandwidth.

Control of the frequency shift and SD card recording are available via the 
custom interface for the bluetooth Tympan app as well as via keyboard controls from the serial monitor.
In the serial monitor, type `h` for a list of commands.



Note that the microphone is not calibrated above 20kHz and there is a known 
resonance between 20 and 40 kHz.  For a true dosimeter, a frequency dependent calibration
is required.
