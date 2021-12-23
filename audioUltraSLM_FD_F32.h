// Created: Chip Audette, OpenAudio, June 2021
// Modified: Jennifer Cooper, JHU/APL, and Jordan Schleif, Sept 2021
// Modifications are copyright 2021 Johns Hopkins University Applied Physics Laboratory LLC
#ifndef AudioUltraSLM_FD_F32_h
#define AudioUltraSLM_FD_F32_h


#include <AudioFreqDomainBase_FD_F32.h> //from Tympan_Library: inherit all the good stuff from this!
#include <arm_math.h>  //fast math library for our processor
#include <vector>
#include <algorithm>

// THIS IS AN EXAMPLE OF HOW TO CREATE YOUR OWN FREQUENCY-DOMAIN ALGRITHMS
// TRY TO DO YOUR OWN THING HERE! HAVE FUN!

//Create an audio processing class to do the lowpass filtering in the frequency domain.
// Let's inherit from  the Tympan_Library class "AudioFreqDomainBase_FD_F32" to do all of the
// audio buffering and FFT/IFFT operations.  That allows us to just focus on manipulating the
// FFT bins and not all of the detailed, tricky operations of going into and out of the frequency
// domain.
class AudioUltraSLM_FD_F32 : public AudioFreqDomainBase_FD_F32   //AudioFreqDomainBase_FD_F32 is in Tympan_Library
{
  public:
    //constructor
    AudioUltraSLM_FD_F32(const AudioSettings_F32 &settings) : AudioFreqDomainBase_FD_F32(settings) {};


    // get/set methods specific to this particular frequency-domain algorithm

    //this is the method from AudioFreqDomainBase that we are overriding where we will
    //put our own code for manipulating the frequency data.  This is called by update()
    //from the AudioFreqDomainBase_FD_F32.  The update() method is itself called by the
    //Tympan (Teensy) audio system, as with every other Audio processing class.
    virtual void processAudioFD(float32_t *complex_2N_buffer, const int NFFT);

    
     std::vector<float32_t> & getCurrentLevel() {
      return total_level;
    }

    std::vector<float32_t> & getCurrentLevel_dB() {
      return total_level_dB;
    }
    
    std::vector<int> & getBandBins(void) {     
      return bandBins;
    }

    int customSetup(const AudioSettings_F32 &settings, const int _N_FFT) {
      this->NFFT = this->setup(settings, _N_FFT);
      N_2 =  this->NFFT / 2 + 1;
      Hz_per_bin = sample_rate_Hz / ((float)this->NFFT); //sample_rate_Hz is from the base class AudioFreqDomainBase_FD_F32
      scaleFactor = 1/(sample_rate_Hz * ((float)this->NFFT)); //get appropriately scaled FFT output
      return this->NFFT;
    }

    inline int freq_to_bin(float freq) {
      return (int) freq / Hz_per_bin + 0.5;
    }
    void setBandFreqs(std::vector<float> & inFreqs) {
      bandBins.clear();

      for (const auto & freq : inFreqs) {
        auto bin = freq_to_bin(freq);
        bandBins.push_back(bin);
      }
      
      //Replace the last element with Nyquist
      bandBins.back() = this->N_2-1;
      
      bandFreqs = inFreqs;
      total_level = std::vector<float32_t>(bandBins.size()-1, 0);
    }
  private:
    //create some data members specific to our processing
    std::vector<int> bandBins;
    std::vector<float> bandFreqs;
    std::vector<float32_t> total_level;
    std::vector<float32_t> total_level_dB;
    int NFFT;
    int N_2;
    float Hz_per_bin;
    float scaleFactor;


};


//Here is the method we are overriding with our own algorithm...REPLACE THIS WITH WHATEVER YOU WANT!!!!
//  Argument 1: complex_2N_buffer is the float32_t array that holds the FFT results that we are going to
//     manipulate.  It is 2*NFFT in length because it contains the real and imaginary data values
//     for each FFT bin.  Real and imaginary are interleaved.  We only need to worry about the bins
//     up to Nyquist because AudioFreqDomainBase will reconstruct the freuqency bins above Nyquist for us.
//  Argument 2: NFFT, the number of FFT bins
//
//  We get our data from complex_2N_buffer
void AudioUltraSLM_FD_F32::processAudioFD(float32_t *complex_2N_buffer,  const int NFFT) {



 
  float32_t orig_mag[N_2]; 
  arm_cmplx_mag_squared_f32(complex_2N_buffer, orig_mag, N_2-1);//get the magnitude squared for each FFT bin and store somewhere safes


//Reset total_level to zero
std::fill(total_level.begin(), total_level.end(), 0);

  //Loop over each bin and sum the level in that band
for (unsigned int indB = 0; indB< bandBins.size()-1; indB++) {//loop over the bands, remembering that the last index is the top of the last bin
   for (int ind = bandBins[indB]; ind < bandBins[indB+1]; ind++) { 

       total_level[indB] += orig_mag[ind] * scaleFactor /(bandBins[indB+1]-bandBins[indB]);  //report a band level translated back into spectral level units for easier comparison

    }
}
 total_level_dB.clear();
 for (float32_t & level : total_level) {
  total_level_dB.push_back(10.0f * log10f(level));
 }

}

#endif
