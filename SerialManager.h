// Created: Chip Audette, OpenAudio, June 2021
// Modified: Jennifer Cooper, JHU/APL, and Jordan Schleif, Sept 2021
// Modifications are copyright 2021 Johns Hopkins University Applied Physics Laboratory LLC
#ifndef _SerialManager_h
#define _SerialManager_h

#include <Tympan_Library.h>
#include "State.h"

//externally defined objects
extern Tympan myTympan;
extern BLE ble;
extern State myState;
extern AudioUltraSLM_FD_F32 audioUltraSLMFD;
extern AudioSDWriter_F32 audioSDWriter;


//functions in the main sketch that I want to call from here
extern bool enablePrintMemoryAndCPU(bool);
extern bool enablePrintLoudnessLevels(bool);
extern bool enablePrintingToBLE(bool);
extern int setTimeAveragingType(int);
extern int setFreqShiftType(int);
//extern int startOrStopSDRecording( float);
//extern int audioSDWriterLED( int);
//Define a class to help manage the interactions with Serial comms (from SerialMonitor or from Bluetooth (BLE))
//see SerialManagerBase.h in the Tympan Library for some helpful supporting functions (like {"sendButtonState")
//https://github.com/Tympan/Tympan_Library/tree/master/src
class SerialManager : public SerialManagerBase {  
  public:
    SerialManager(BLE *_ble) : SerialManagerBase(_ble) { myGUI = new TympanRemoteFormatter; };
    void respondToByte(char c);
    void printHelp(void);

    //define the GUI for the App
    TympanRemoteFormatter *myGUI;  //Creates the GUI-writing class for interacting with TympanRemote App
    void createTympanRemoteLayout(void);
    void printTympanRemoteLayout(void);

    void updateFreqButtons(void);
    void updateRecordButtons(void);
//    void setButtonState(String btnId, bool newState);
//    void setButtonText(String btnId, String text);
  private:

};

void SerialManager::printHelp(void) {
  myTympan.println();
  myTympan.println("SerialManager Help: Available Commands:");
  myTympan.println("   h:   Print this help");
  myTympan.println("   c,C: Enable/Disable printing of CPU and Memory usage");
  myTympan.println("   f,F,G: Frequency shift down 10, 20, 25 kHz");  
  myTympan.println("   p: SD: prepare for recording");
  myTympan.println("   r: SD: begin recording");
  myTympan.println("   s: SD: stop recording");
  myTympan.println("   ],}: Start/Stop sending level to TympanRemote App.");
  myTympan.println("   l,L: Enable/Disable printing of loudness level");
  myTympan.println("   0:   Reset max loudness value.");
  myTympan.println();
}


//switch yard to determine the desired action
void SerialManager::respondToByte(char c) {
  //float old_val = 0.0, new_val = 0.0;
  switch (c) {
    case 'h': case '?':
      printHelp(); break;
    case 'c':
      myTympan.println("Command Received: enable printing of memory and CPU usage.");
      enablePrintMemoryAndCPU(true);
      break;
    case 'C':
      myTympan.println("Command Received: disable printing of memory and CPU usage.");
      enablePrintMemoryAndCPU(false);
      break;
    case 'f':
      myTympan.println("Command Received: setting frequency downshift to 10kHz");
      setFreqShiftType(State::FREQ_A);
      updateFreqButtons();
      break;      
    case 'F':
      myTympan.println("Command Received: setting frequency downshift to 20kHz");
      setFreqShiftType(State::FREQ_B);
      updateFreqButtons();      
      break;
    case 'G':
      myTympan.println("Command Received: setting frequency downshift to 25kHz");
      setFreqShiftType(State::FREQ_C);
      updateFreqButtons();      
      break;
        case 'p':
      myTympan.println("Received: prepare SD for recording");
      //prepareSDforRecording();
      audioSDWriter.prepareSDforRecording();
      break;
    case 'r':
      myTympan.println("Received: begin SD recording");
      //beginRecordingProcess();
      audioSDWriter.startRecording();
      updateRecordButtons();
      break;
    case 's':
      myTympan.println("Received: stop SD recording");
      audioSDWriter.stopRecording();
      updateRecordButtons();
      break;
    case 'l':
      myTympan.println("Command Received: enable printing of loudness levels.");
      enablePrintLoudnessLevels(true);
      break;
    case 'L':
      myTympan.println("Command Received: disable printing of loudness levels.");
      enablePrintLoudnessLevels(false);
      break;
    case ']':
      myTympan.println("Command Received: enable printing of loudness levels to BT.");
      enablePrintingToBLE(true);
      enablePrintLoudnessLevels(true);
      break;
    case '}':
      myTympan.println("Command Received: disable printing of loudness levels to BT.");
      enablePrintingToBLE(false);
      enablePrintLoudnessLevels(false);
      break;
//    case '0':
//      myTympan.println("Command Received: reseting max SPL.");
//      calcLevel1.resetMaxLevel();
//      break;     
    case 'J': case 'j':
      createTympanRemoteLayout();
      printTympanRemoteLayout();
      break; 
     
  }
};

void SerialManager::createTympanRemoteLayout(void) { 
  if (myGUI) delete myGUI; //delete any pre-existing GUI from memory (on the Tympan, not on the App)
  myGUI = new TympanRemoteFormatter(); 

  // Create some temporary variables
  TR_Page *page_h;  TR_Card *card_h; 

  //Add first page to GUI
  page_h = myGUI->addPage("Sound Level Meter");
      //Add card and buttons under the first page
      card_h = page_h->addCard("Frequency Shifting");
          card_h->addButton("ShiftA","f","ShiftA",4);  //displayed string, command, button ID, button width (out of 12)
          card_h->addButton("ShiftB","F","ShiftB",4);  //displayed string, command, button ID, button width (out of 12)
          card_h->addButton("ShiftC","G","ShiftC",4);  //displayed string, command, button ID, button width (out of 12)
      card_h = page_h->addCard("SD card Recording");
          card_h->addButton("Stop","s","Stop",6);  //displayed string, command, button ID, button width (out of 12)
          card_h->addButton("Start","r","Start",6);  //displayed string, command, button ID, button width (out of 12)
//      card_h = page_h->addCard("Maximum SPL");
//          card_h->addButton("Reset Max","0","resetMax",12);  //displayed string, command, button ID, button width (out of 12)
//          
 
  //add some pre-defined pages to the GUI
  myGUI->addPredefinedPage("serialPlotter");  
  myGUI->addPredefinedPage("serialMonitor");
}

void SerialManager::printTympanRemoteLayout(void) { 
  myTympan.println(myGUI->asString()); 
  ble->sendMessage(myGUI->asString());

  updateFreqButtons();
  updateRecordButtons();
}

void SerialManager::updateFreqButtons(void) {
  switch (myState.cur_freq_weight) {
    case State::FREQ_A:
      setButtonState("ShiftA",true);
      setButtonState("ShiftB",false);
      setButtonState("ShiftC",false);
      break;
    case State::FREQ_B:
      setButtonState("ShiftA",false);
      setButtonState("ShiftB",true);
      setButtonState("ShiftC",false);
      break;
    case State::FREQ_C:
      setButtonState("ShiftA",false);
      setButtonState("ShiftB",false);
      setButtonState("ShiftC",true);
      break;
  }
}

void SerialManager::updateRecordButtons(void) {
  switch (audioSDWriter.getState()) {
    case AudioSDWriter::STATE::RECORDING:
      setButtonState("Start",true);
      setButtonState("Stop",false);
      break;
    case AudioSDWriter::STATE::STOPPED:
      setButtonState("Start",false);
      setButtonState("Stop",true);
      break; 
   case AudioSDWriter::STATE::UNPREPARED:
      setButtonState("Stop", true);
      setButtonState("Start", false);
      break; 
  }
}
//void SerialManager::setButtonState(String btnId, bool newState) {
//  String str = "STATE=BTN:" + btnId + ":";
//  if (newState) {
//    str += "1";
//  } else {
//    str += "0";
//  }
//  myTympan.println(str);  //send to USB for debugging on PC
// // ble.sendMessage(str);   //send to App via BLE
//}
//
//void SerialManager::setButtonText(String btnId, String text) {
//  String str = "TEXT=BTN:" + btnId + ":"+text;
//  myTympan.println(str); //send to USB for debugging on PC
// // ble.sendMessage(str);  //send to App via BLE
//}


#endif
