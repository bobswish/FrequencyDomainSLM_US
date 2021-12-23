// Created: Chip Audette, OpenAudio, June 2021
// Modified: Jennifer Cooper, JHU/APL, and Jordan Schleif, Sept 2021
// Modifications are copyright 2021 Johns Hopkins University Applied Physics Laboratory LLC

#ifndef State_h
#define State_h


// define structure for tracking state of system (for GUI)
class State : public TympanStateBase_UI {  // look in TympanStateBase for more state variables and helpful methods!!
  public:
    State(AudioSettings_F32 *given_settings, Print *given_serial) : TympanStateBase_UI(given_settings, given_serial) {}

    enum FREQ_WEIGHT {FREQ_A=0, FREQ_B, FREQ_C};
    int cur_freq_weight = FREQ_A; //default


};

#endif
