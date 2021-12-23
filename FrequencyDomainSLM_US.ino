// FreqDomainSLM_US
//
// Based on Tympan Library example GenericFreqDomainExample_FD, this Ultrasound Dosimeter
// returns (uncalibrated) octave band sound levels to the serial monitor and alerts the 
// user if energy above 20 kHz is greater than the energy just below 20 kHz.  Ultrasound is also
// frequency shifted down into the audible band to assist the user in idenitifying the source (left ear),
// while low pass audio is sent to the right ear.
// Potentiometer = volume knob
// Bluetooth gui or Serial input allows setting of the frequency shift amount
// Bluetooth app shows plot of the levels in the bands
// Lights on the Tympan flash - red if no ultrasound detected, green if it is detected.  This information is also printed to the Serial monitor
//
// Created: Chip Audette, OpenAudio, June 2021
// Modified: Jennifer Cooper, JHU/APL, and Jordan Schleif, Sept 2021
// Modifications are copyright 2021 Johns Hopkins University Applied Physics Laboratory LLC
//
// Approach:
//    * Take samples in the time domain
//    * Take FFT to convert to frequency domain
//    * Calculate the levels in octave bands, normalized back to spectral (1Hz bin) levels
//    * On raw data, Frequency Shift
//    * On raw data, lowpass filter
//    * version 8 records full band data on L channel and freq shifted on R channel for later analysis
//
// This example code is in the public domain (MIT License)

#include <Tympan_Library.h>
#include "audioUltraSLM_FD_F32.h"  //the local file holding your custom function
#include "SerialManager.h"

#include <vector>


//set the sample rate and block size
const float sample_rate_Hz =96000.0f; //96000.f;// 24000 or 44117 (or other frequencies in the table in AudioOutputI2S_F32),
// 96kHz is a good balance between capturing ultrasound energy and having reasonable FFT bin sizes and reasonable memory use
const int audio_block_samples = 128;     //for freq domain processing choose a power of 2 (16, 32, 64, 128) but no higher than 128
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

//create audio library objects for handling the audio
Tympan                       myTympan(TympanRev::E);                //do TympanRev::D or TympanRev::E
AudioInputI2S_F32            i2s_in(audio_settings);                //Digital audio *from* the Tympan AIC.
AudioOutputI2S_F32           i2s_out(audio_settings);               //Digital audio *to* the Tympan AIC.

//the modules that do the work in this program
AudioEffectFreqShift_FD_F32     freqShift(audio_settings);       //Freq domain processing!  https://github.com/Tympan/Tympan_Library/blob/master/src/AudioEffectFreqShiftFD_F32.h
AudioEffectGain_F32             gain1;                           //Applies digital gain to audio data.
AudioEffectGain_F32             gain2;                           //Applies digital gain to audio data.
AudioFilterBiquad_F32           lp_filt1(audio_settings);        //IIR filter doing a lowpass filter.  
AudioSDWriter_F32             audioSDWriter(audio_settings); //this is stereo by default
AudioUltraSLM_FD_F32            audioUltraSLMFD(audio_settings);       //create a frequency-domain processing block to calculate octave band levels

//Make all of the audio connections
AudioConnection_F32       patchCord1(i2s_in, 0, audioUltraSLMFD, 0);   //connect the Left input to our algorithm
AudioConnection_F32       patchCord2(i2s_in, 0, freqShift, 0);        //left input to freq shift
AudioConnection_F32       patchCord3(freqShift, 0, gain1, 0);         //gain on freq shift to left
AudioConnection_F32       patchCord4(gain1, 0, i2s_out, 0);           //connect freq shift ->gain output to the left output

AudioConnection_F32       patchCord11(i2s_in, 0, lp_filt1, 0);      //connect the right input to low pass filter - this is the regular channel
AudioConnection_F32       patchCord41(lp_filt1, 0, gain2, 0);     //connect to gain
AudioConnection_F32       patchCord52(gain2, 0, i2s_out, 1);      //connect in+filter+gain to the right output


AudioConnection_F32       patchCord7(i2s_in, 0, audioSDWriter, 0); //connect raw audio to left channel of SD writer
//AudioConnection_F32       patchCord8(i2s_in, 1, audioSDWriter, 1); //connect Raw audio to right channel of SD writer //could also pass the band pass or freq shift to the writer
//Or, you could record the freq shifted data
//AudioConnection_F32       patchCord7(gain1, 0, audioSDWriter, 0); //connect freq shift audio to left channel of SD writer
AudioConnection_F32       patchCord8(gain1, 0, audioSDWriter, 1); //connect freq shift to right channel of SD writer //could also pass the band pass or freq shift to the writer

//Create BLE and serialManager
BLE ble = BLE(&Serial1); //&Serial1 is the serial connected to the Bluetooth module
SerialManager serialManager(&ble);
State myState(&audio_settings, &myTympan);

//calibration information for the microphone being used
//note that calibration information only applies in the audio band.  
//the mic has a resonance in the ultrasound band, so this calibration will not be accurate for ultrasound.  
//If true levels are desired, a calibration should be performed
float32_t mic_cal_dBFS_at94dBSPL_at_0dB_gain = -47.4f + 9.2175;  //PCB Mic baseline with manually tested adjustment.   Baseline:  http://openaudio.blogspot.com/search/label/Microphone

//control display and serial interaction
bool enable_printCPUandMemory = true;
bool enablePrintMemoryAndCPU(bool _enable) { return enable_printCPUandMemory = _enable; }
bool enable_printLoudnessLevels = true; 
bool enablePrintLoudnessLevels(bool _enable) { return enable_printLoudnessLevels = _enable; };
bool enable_printToBLE = false;
bool enablePrintingToBLE(bool _enable = true) {return enable_printToBLE = _enable; };

// define the setup() function, the function that is called once when the device is booting
const float input_gain_dB = 5.0f; //gain on the microphone
float vol_knob_gain_dB = 0.0;      //will be overridden by volume knob


void setup() {
  //begin the serial comms (for debugging)
  myTympan.beginBothSerial();
  delay(1000);  //extra long delay ensures that user can see the setup info in the serial monitor, but causes slow setup
  Serial.println("FrequencyDomainSLM_US8: starting setup()...");
  Serial.print("    : sample rate (Hz) = ");  Serial.println(audio_settings.sample_rate_Hz);
  Serial.print("    : block size (samples) = ");  Serial.println(audio_settings.audio_block_samples);

  // Allocate working memory for audio
  AudioMemory_F32(120, audio_settings);

  // Configure the frequency-domain algorithm
  int N_FFT = 2048; //increasing N_FFT increases the memory usage.  Using N_FFT=4096 generates an error suggesting you use up to 2048

  //Custom Setup Function
  audioUltraSLMFD.customSetup(audio_settings,N_FFT); //do after AudioMemory_F32();
  
  float Hz_per_bin = sample_rate_Hz /((float)N_FFT); //sample_rate_Hz is from the base class AudioFreqDomainBase_FD_F32
  std::vector<float> bandFreqs = { 44, 88, 177, 355, 710, 1420, 2840, 5680, 11360, 17959,22627, 28508, 35918, 45253, 57015 }; //octave band edge frequencies, in Hz; one bin ~46 kHz, so skipping the very low frequency bins
  audioUltraSLMFD.setBandFreqs(bandFreqs);
  Serial.print("    : Hz per Bin  (Hz) = ");  Serial.println(Hz_per_bin);
  Serial.println("    : (1/3 octave bands above 20kHz)");
  Serial.print("    : Octave Band Edge Frequencies = ");
  for (const auto & bin : bandFreqs) {
    Serial.print((int) bin); Serial.print(", ");
  }
  Serial.println("");
  
  Serial.print("    : Octave Band Bin Indices   = ");
  for (const auto & bin : audioUltraSLMFD.getBandBins()) {
    Serial.print(bin); Serial.print(", ");
  }
  Serial.println("");
  delay(1000); //put a pause to make sure we get to see the start up



//configure gain settings
  float bass_boost_dB = 20.f;
  float treble_boost_dB = 0.f;
  gain1.setGain_dB(treble_boost_dB);  //set the gain of the Left-channel gain processor
  gain2.setGain_dB(bass_boost_dB);  //set the gain of the Right-channel gain processor
  myTympan.print("setting treble_boost to "); myTympan.print(treble_boost_dB); myTympan.println(" dB");
  myTympan.print("setting bass_boost to "); myTympan.print(bass_boost_dB); myTympan.println(" dB");





  int overlap_factor = 4;  //set to 2, 4 or 8...which yields 50%, 75%, or 87.5% overlap (8x)
  int NFFT_shift = audio_block_samples * overlap_factor;  
  Serial.print("    : N_FFT for shifting = "); Serial.println(NFFT_shift);
  freqShift.setup(audio_settings, NFFT_shift); //do after AudioMemory_F32();
  //freqShift.setup also confusingly (because we aren't doing a Formant Shift) prints the line "AudioEffectFormantShiftFD_F32: setting myIFFT to use hanning..."
  
  //configure the frequency shifting
  //TODO: pass shift_bins into setFreqShiftType rather than resetting it there.  Would require getting the values also into SerialManager.h
  float shiftFreq_Hz[3] = {-10000.f, -20000.f, -25000.f}; //shift audio downward from Ultrasound to audible
  float Hz_per_shiftBin = sample_rate_Hz / (float( NFFT_shift));
  int shift_bins[3];
  shift_bins[0]= (int)(shiftFreq_Hz[0] / Hz_per_shiftBin + 0.5);  //round to nearest bin
  shift_bins[1]= (int)(shiftFreq_Hz[1] / Hz_per_shiftBin + 0.5);  //round to nearest bin
  shift_bins[2]= (int)(shiftFreq_Hz[2] / Hz_per_shiftBin + 0.5);  //round to nearest bin
  shiftFreq_Hz[0] = shift_bins[0] * Hz_per_shiftBin;
  Serial.print("Setting shift to "); Serial.print(shiftFreq_Hz[0]); Serial.print(" Hz, which is "); Serial.print(shift_bins[0]);
  Serial.print(", ");Serial.print(shift_bins[1]); Serial.print(", ");Serial.print(shift_bins[2]);Serial.println(" bins");
  freqShift.setShift_bins(shift_bins[0]); //0Hz is no freq shifting.
  setFreqShiftType(State::FREQ_A);
 
    Serial.println("testing");

 //Enable the Tympan to start the audio flowing!
  myTympan.enable(); // activate AIC

  //Choose the desired input
  myTympan.inputSelect(TYMPAN_INPUT_ON_BOARD_MIC); // use the on board microphones
 //  myTympan.inputSelect(TYMPAN_INPUT_JACK_AS_MIC); // use the microphone jack - defaults to mic bias 2.5V
  // myTympan.inputSelect(TYMPAN_INPUT_JACK_AS_LINEIN); // use the microphone jack - defaults to mic bias OFF

  //Set the desired volume levels
  myTympan.volume_dB(0);                   // headphone amplifier.  -63.6 to +24 dB in 0.5dB steps.
  myTympan.setInputGain_dB(input_gain_dB); // set input volume, 0-47.5dB in 0.5dB setps

  //enable the Tympman to detect whether something was plugged into the pink mic jack
  //myTympan.enableMicDetect(true);
  
 //prepare the SD writer for the format that we want and any error statements
  audioSDWriter.setSerial(&myTympan);         //the library will print any error info to this serial stream (note that myTympan is also a serial stream)
  audioSDWriter.setWriteDataType(AudioSDWriter::WriteDataType::INT16);  //this is the built-in default, but here you could change it to FLOAT32
  audioSDWriter.setNumWriteChannels(2);       //this is also the built-in defaullt, but you could change it to 4 (maybe?), if you wanted 4 channels.
  
  // configure the blue potentiometer
  servicePotentiometer(millis(),0);  //update based on the knob setting the "0" is not relevant here.



 //Set the cutoff frequency for the lowpassfilter
  float cutoff_LP_Hz = 1000.f;  //frequencies above this will be attenuated
  int filt_bins = (int)(cutoff_LP_Hz / Hz_per_bin + 0.5);  //round to nearest bin
  Serial.print("Lowpass filter "); Serial.print(cutoff_LP_Hz); Serial.print("Hz, or "); Serial.print(filt_bins); Serial.println(" bins!");
  lp_filt1.setLowpass(0, cutoff_LP_Hz); //biquad IIR filter.  left channel



  Serial.println("Setup complete.");
  serialManager.printHelp();
}

bool LED = 0;
int isUltrasoundDetected = 0; // flag for if ultrasound was detected, true = detection

// define the loop() function, the function that is repeated over and over for the life of the device
void loop() {

  //check the potentiometer
  servicePotentiometer(millis(),100); //service the potentiometer every 100 msec

 //respond to Serial commands
  while (Serial.available()) serialManager.respondToByte((char)Serial.read());
  
  //respond to BLE
  if (ble.available() > 0) {
    String msgFromBle; int msgLen = ble.recvBLE(&msgFromBle);
    for (int i=0; i < msgLen; i++) serialManager.respondToByte(msgFromBle[i]);
  }

 //service the BLE advertising state; advertising means your phone can find it
  ble.updateAdvertising(millis(),5000); //check every 5000 msec to ensure it is advertising (if not connected)
  
  //printing of sound level
  if (enable_printLoudnessLevels) printLoudnessLevels(millis(),20);  //print a value every 300 msec

   // How long to hold when a cry is detected
  if (isUltrasoundDetected) {
    delay(3000);
  }
  //check to see whether to print the CPU and Memory Usage
  myTympan.printCPUandMemory(millis(),30000); //print every 30000 msec = 30 sec, very occasionally just to check that there are no issues

  //service the SD recording
  serviceSD();
} //end loop();


// ///////////////// Servicing routines
void printLoudnessLevels(unsigned long curTime_millis, unsigned long updatePeriod_millis) {
  static bool firstTime = true;
  static unsigned long lastUpdate_millis = 0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?
    if (firstTime) {
      myTympan.println("Printing: current SPL (dB) in each octave band");
      firstTime = false;
    }
    
    float32_t cal_factor_dB = -mic_cal_dBFS_at94dBSPL_at_0dB_gain + 94.0f - input_gain_dB;
    auto & cur_SPL_dB = audioUltraSLMFD.getCurrentLevel_dB();

    String msg = " SPL Level: ";
    //cur_SPL_dB is a reference to the vector stored in the class.
    //SPL is a reference to the elements of cur_SPL_dB.
    //So the following loop edits the elements in-place.
    for (auto & SPL : cur_SPL_dB) {
       SPL += cal_factor_dB;
       msg = msg  + String( SPL )+ ", ";
    }

    //print the text string to the Serial
    myTympan.println(msg);


// if the level in the ultrasound band is higher than in other bands, turn on the lights
 auto n_bands = cur_SPL_dB.size();
   float maxVal = max(cur_SPL_dB[n_bands-1], cur_SPL_dB[n_bands-2]);
   maxVal = max( maxVal, cur_SPL_dB[n_bands-3]);
 if (maxVal >= (cur_SPL_dB[n_bands-4]) ){ //3 1/3 octave bands in ultrasound 
    toggleLEDs( true, false);
   // Serial.println("ultrasound detected");
   isUltrasoundDetected = 1;
   Serial.print("Ultrasound detected: "); Serial.print(maxVal); Serial.print(" is greater than "); Serial.println( cur_SPL_dB[n_bands-4]); //test that the check works correctly
    } else {
      isUltrasoundDetected = 0;
      toggleLEDs( false, true);
    }


    //if allowed, send it over BLE with the special prefix to allow it to be printed by the SerialPlotter
    //https://github.com/Tympan/Docs/wiki/Making-a-GUI-in-the-TympanRemote-App
    if (enable_printToBLE) ble.sendMessage(String("P ") + msg); //prepend "P " for the serial plotter
   
    lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  }
}
//Set the frequency Shifting.  Options include State::FREQ_A_WEIGHT and State::FREQ_C_WEIGHT
int setFreqShiftType(int type) {
  int shift_bins2[3] = {-52, -106, -132}; //TODO: set this above and use it here rather than hardcoding
  switch (type) {
    case State::FREQ_A:
      freqShift.setShift_bins(shift_bins2[0]); //0Hz is no ffreq shifting.
      break;
    case State::FREQ_B:
      freqShift.setShift_bins(shift_bins2[1]); //0H is no ffreq shifting.
      break;
    case State::FREQ_C:
      freqShift.setShift_bins(shift_bins2[2]); //0H is no ffreq shifting.
      break;
    default:
      type = State::FREQ_A;
      freqShift.setShift_bins(shift_bins2[0]); //0Hz is no ffreq shifting.
      break;
  }
  return myState.cur_freq_weight = type;
}

//servicePotentiometer: listens to the blue potentiometer and sends the new pot value
//  to the audio processing algorithm as a control parameter
void servicePotentiometer(unsigned long curTime_millis, unsigned long updatePeriod_millis) {
 // static unsigned long updatePeriod_millis = 100; //how many milliseconds between updating the potentiometer reading?
  static unsigned long lastUpdate_millis = 0;
  static float prev_val = 0.f;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?

    //read potentiometer
    float val = float(myTympan.readPotentiometer()) / 1023.0; //0.0 to 1.0
    val = (1.0/15.0) * (float)((int)(15.0 * val + 0.5)); //quantize so that it doesn't chatter...0 to 1.0
    float tmpComp = val - prev_val; //put the comparison in a temp variable for ease
    //send the potentiometer value to your algorithm as a control parameter
    //  if (abs(tmpComp) > 0.05) { //TODO: not sure why abs(tmpComp) doesn't work - must have something to do with one of the imported files
    if (tmpComp >= 0.05 || tmpComp <= -0.05) { //is it different than before? 
      prev_val = val;  //save the value for comparison for the next time around
        //use the potentiometer as a volume knob
        const float min_val = 0, max_val = 40.0; //set desired range
        float new_value = min_val + (max_val - min_val)*val;
        
         myTympan.volume_dB(new_value); // set input volume, 0-47.5dB in 0.5dB setps
        Serial.print("servicePotentiometer: Input Gain (dB) = "); Serial.println(new_value); //print text to Serial port for debugging   
    }
    lastUpdate_millis = curTime_millis;
  } // end if
} //end servicePotentiometer();



void toggleLEDs(const bool &useAmber, const bool &useRed) {
  static bool LED = false;
  LED = !LED;
  if (LED) {
    if (useAmber) myTympan.setAmberLED(true);
    if (useRed) myTympan.setRedLED(false);
  } else {
    if (useAmber) myTympan.setAmberLED(false);
    if (useRed) myTympan.setRedLED(true);
  }

  if (!useAmber) myTympan.setAmberLED(false);
  if (!useRed) myTympan.setRedLED(false);
  
}


void serviceSD(void) {
  static int max_max_bytes_written = 0; //for timing diagnotstics
  static int max_bytes_written = 0; //for timing diagnotstics
  static int max_dT_micros = 0; //for timing diagnotstics
  static int max_max_dT_micros = 0; //for timing diagnotstics

  unsigned long dT_micros = micros();  //for timing diagnotstics
  int bytes_written = audioSDWriter.serviceSD();
  dT_micros = micros() - dT_micros;  //timing calculation

  if ( bytes_written > 0 ) {
    
    max_bytes_written = max(max_bytes_written, bytes_written);
    max_dT_micros = max((int)max_dT_micros, (int)dT_micros);
   
    if (dT_micros > 10000) {  //if the write took a while, print some diagnostic info
      
      max_max_bytes_written = max(max_bytes_written,max_max_bytes_written);
      max_max_dT_micros = max(max_dT_micros, max_max_dT_micros);
      
      Serial.print("serviceSD: bytes written = ");
      Serial.print(bytes_written); Serial.print(", ");
      Serial.print(max_bytes_written); Serial.print(", ");
      Serial.print(max_max_bytes_written); Serial.print(", ");
      Serial.print("dT millis = "); 
      Serial.print((float)dT_micros/1000.0,1); Serial.print(", ");
      Serial.print((float)max_dT_micros/1000.0,1); Serial.print(", "); 
      Serial.print((float)max_max_dT_micros/1000.0,1);Serial.print(", ");      
      Serial.println();
      max_bytes_written = 0;
      max_dT_micros = 0;     
    }

#define PRINT_OVERRUN_WARNING 1   //set to 1 to print a warning that the there's been a hiccup in the writing to the SD.
    //print a warning if there has been an SD writing hiccup
    if (PRINT_OVERRUN_WARNING) {
      //if (audioSDWriter.getQueueOverrun() || i2s_in.get_isOutOfMemory()) {
      if (i2s_in.get_isOutOfMemory()) {
        float approx_time_sec = ((float)(millis()-audioSDWriter.getStartTimeMillis()))/1000.0;
        if (approx_time_sec > 0.1) {
          myTympan.print("SD Write Warning: there was a hiccup in the writing.");//  Approx Time (sec): ");
          myTympan.println(approx_time_sec );
        }
      }
    }
    i2s_in.clear_isOutOfMemory();
  }
}

void serviceMicDetect(unsigned long curTime_millis, unsigned long updatePeriod_millis) {
  static unsigned long lastUpdate_millis = 0;
  static unsigned int prev_val = 1111; //some sort of weird value
  unsigned int cur_val = 0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?

    cur_val = myTympan.updateInputBasedOnMicDetect(); //if mic is plugged in, defaults to TYMPAN_INPUT_JACK_AS_MIC
    if (cur_val != prev_val) {
      if (cur_val) {
        myTympan.println("serviceMicDetect: detected plug-in microphone!  External mic now active.");
      } else {
        myTympan.println("serviceMicDetect: detected removal of plug-in microphone. On-board PCB mics now active.");
      }
    }
    prev_val = cur_val;
    lastUpdate_millis = curTime_millis;
  }
}
