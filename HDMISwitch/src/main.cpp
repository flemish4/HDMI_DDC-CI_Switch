#include <Arduino.h>
#include <Bounce2.h>
#include <SoftWire.h>
//#include <AsyncDelay.h>


// Constants
const byte lenMagicNum = 0x80;
const byte hostSlaveAddr = 0x51;
const byte edidHeader[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

const int numMonitors = 2;
const int maxSupportedMonitorInputs = 8;

const int ledPin = 13;
const uint8_t setupButtonPin = 2;
const int numModes = 4;
const uint8_t modeButtonPins[numModes] = {3,4,5,6};
const int i2cPins[numMonitors][2] = {{7,8},{9,10}};
Bounce setupButton = Bounce();
Bounce * modeButtons = new Bounce[numModes];
SoftWire *i2cMonitorPorts[numMonitors];
byte i2cMonitorTxBuffers[numMonitors][32];
byte i2cMonitorRxBuffers[numMonitors][32];

const int loopDelay = 50; // 20 times per second
const int settingsMaxCount = 3*(1000/loopDelay);
const int settingsMenu[5][4] = {{1,2,4,0}, // Main - 0: Change setup button behaviour, 1: Change turn on behaviour, 2: Configure monitor input switching
                                {0,0,0,0}, // Setup button behaviour - 0: mode select, 1: input cycling
                                {0,0,3,2}, // Turn on behaviour - 0: As turn off, 1: Do not change, 2: choose
                                {0,0,0,0}, // Turn on behaviour mode select - 0: turn on to mode 0, 1 turn on to mode 1 etc.
                                {0,0,0,0}  // Monitor input switching config : 0: Automatic detection (order will likely be wrong), 1: Match mode order (likely in order but may miss extras), 2: Match mode order + autodetected (likely in order but may have extras)
                              };

// Eeprom loaded values
//REVISIT EEPROM loaded values - can change after
int idleBehaviour = 0; // 0 is mode select, 1 is input switching // REVISIT : Will be loaded from eeprom
int turnOnBehaviour = 0; // -1 is previous state, -2 is just leave, others are just button saves.  // REVISIT : Will be loaded from eeprom
byte monitorModes[numModes][numMonitors] = {{0xFF, 0xFF},{0xFF, 0xFF},{0xFF, 0xFF},{0xFF, 0xFF}}; // REVISIT : Will be loaded from eeprom
byte currentMonitorMode[numMonitors] = {0xFF, 0xFF}; // REVISIT : Will be loaded from eeprom
byte monitorInputs[numMonitors][maxSupportedMonitorInputs] = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // REVISIT : Will be loaded from eeprom
                                                              {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};


// State values
int state = 0; // 0 - idle, 1 - checking if settings mode, 2 - settings mode
bool doneAction = 0;
int settingsCounter = 0;
int settingsPage  = 0; // 0 is main menu, 1 is setup button behaviour (values 0, 1 as above), 2 is turn on behaviour 
bool modeSelected = false;
int selectedMode = -1;


// Temporary values
bool setupButtonStatus = false;
bool modeButtonStatus[5] = {false,false,false,false,false};





void setup()
{
  TWBR = 158; // Make clock really slow!
  //TWSR |= bit (TWPS0);

  Serial.begin(9600); // REVISIT - for debug only
  while (!Serial);             // Leonardo: wait for serial monitor
  Serial.println("\nI2C Scanner");

  // Set up input buttons
  setupButton.attach( setupButtonPin , INPUT_PULLUP  );       //setup the bounce instance for the current button
  setupButton.interval(25);              // interval in ms

  for (int i = 0; i < numModes; i++) {
    modeButtons[i].attach( modeButtonPins[i] , INPUT_PULLUP  );       //setup the bounce instance for the current button
    modeButtons[i].interval(25);              // interval in ms
  }


  for (int i = 0; i < numMonitors; i++) {
    i2cMonitorPorts[i] = new SoftWire(i2cPins[i][0], i2cPins[i][1]);
    i2cMonitorPorts[i]->setTxBuffer(i2cMonitorTxBuffers[i], sizeof(i2cMonitorTxBuffers[i]));
    i2cMonitorPorts[i]->setRxBuffer(i2cMonitorRxBuffers[i], sizeof(i2cMonitorRxBuffers[i]));
    i2cMonitorPorts[i]->setDelay_us(5);
    i2cMonitorPorts[i]->setTimeout(1000);
    i2cMonitorPorts[i]->begin(); // REVISIT : could begin and end as required? check usage/power
  }

}


bool i2cWrite(int monitor, byte addr, int numBytes, byte *Buffer)
{
  i2cMonitorPorts[monitor]->beginTransmission(addr); 
  for (int i=0; i<numBytes; i++) {
    i2cMonitorPorts[monitor]->write(Buffer[i]);
  }
  return i2cMonitorPorts[monitor]->endTransmission() == 0;
}


bool i2cRead(int monitor, byte addr, byte *Buffer, int startIdx, int numBytes)
{
  int i = startIdx;
  i2cMonitorPorts[monitor]->requestFrom(addr,numBytes);
  while (i2cMonitorPorts[monitor]->available()){
    Buffer[i] = i2cMonitorPorts[monitor]->read();
    i+=1;
  }
  if (i-startIdx != numBytes) {
    return false;
  }
  return true;
}


bool ddcWrite(int monitor, byte addr, int numBytes, byte *Buffer)
{
  byte trueAddr = addr << 1 | 0;
  byte trueLen = numBytes | lenMagicNum;
  byte checkSum = trueAddr ^ hostSlaveAddr ^ trueLen;

  Serial.print("trueAddr: ");
  Serial.print(trueAddr, HEX);
  Serial.print(", ");
  Serial.print("trueLen: ");
  Serial.print(trueLen, HEX);
  Serial.print(", ");
  Serial.print("checkSum: ");
  Serial.print(checkSum, HEX);
  Serial.print(", ");


  i2cMonitorPorts[monitor]->beginTransmission(addr);
  i2cMonitorPorts[monitor]->write(hostSlaveAddr);
  i2cMonitorPorts[monitor]->write(trueLen);
  for (int i=0; i<numBytes; i++) {
    i2cMonitorPorts[monitor]->write(Buffer[i]);
    checkSum ^= Buffer[i];
    Serial.print("Buffer[i]: ");
    Serial.print(Buffer[i], HEX);
    Serial.print(", ");
    Serial.print("checkSum: ");
    Serial.print(checkSum, HEX);
    Serial.print(", ");
  }
    i2cMonitorPorts[monitor]->write(checkSum);
  
    Serial.println("\n\n");
  return i2cMonitorPorts[monitor]->endTransmission() == 0;
}


bool ddcRead(int monitor, byte addr, byte offset, byte *Buffer, int startIdx, int numBytes, int incr = 32) 
{
  byte wrData[1];
  bool result = true;
  int thisNumBytes;
  int maxOffset = offset + numBytes; 


  for (int i=offset; i<maxOffset; i+=incr) {
    thisNumBytes = min(maxOffset-i, 32);
    wrData[0] = i;
    result = result & i2cWrite(monitor, addr, 1, wrData);
    result = result & i2cRead(monitor, addr, Buffer, i, thisNumBytes);
  }

  // REVISIT : Check checksum 
  return result;
}


bool doArraysMatch(int len, byte *a, byte *b)
{
  for (int i; i<len; i++) 
  {
    if (a[i] != b[i])  {
      return false;
    }
  }
  return true;
}


void print_byte_array(int len, byte *Buffer)
{
  for (int i; i<len; i++) 
  {
    Serial.print("0x");
    Serial.print(Buffer[i]>>4,  HEX);
    Serial.print(Buffer[i]&0x0F,HEX);
    Serial.print(" ");
  }
}


bool edid_read(int monitor, int len)
{
  byte Buffer[len];
  bool match;
  ddcRead(monitor, 0x50, 0x00, Buffer, 0, len, 32);
  match = doArraysMatch(8, Buffer, edidHeader);
  //Serial.print("EDID data: ");
  //print_byte_array(256, Buffer);
  //Serial.println("");
  return match;
}


int getSelectedMode() 
{
  for (int i=0; i<numModes; i++) {
    if (modeButtonStatus[i]) {
      return i;
    }
  }
  return -1;
}


void setSetupMode(int modeSelected) 
{
  // REVISIT: needs saving to eeprom
  idleBehaviour = modeSelected;
}


void setDefaultMode(int page, int modeSelected) 
{
  // REVISIT: needs saving to eeprom
  if (page==0) {
    turnOnBehaviour = -1-modeSelected;
  } else if (page==1) {
    turnOnBehaviour = modeSelected;
  }
}


bool is_monitor_connected(int monitor) {
  return edid_read(monitor, 8);
}


bool readMonitorInput(int monitor, byte *inputVal) 
{
  byte bufferInt[11];
  return ddcRead(0x37, 0x60, bufferInt, 0, 11, 32);
}


bool writeMonitorInput(int monitor, byte inputVal) 
{
  byte bufferInt[4] = {0x03, 0x60, 0x00, inputVal};
  return ddcWrite(monitor, 0x37, 4, bufferInt);
}


void getMonitorStates(byte *monitorStates) 
{
  byte result;
  byte inputVal;
  // For each monitor
  for (int i=0; i<numMonitors; i++) {
    result = 0xFF; // Default value -- invalid
    // Check if active
    if (is_monitor_connected(i)) { // Check if monitor is on -- maybe unnecessary
      if (readMonitorInput(i, inputVal)) // Read from ddc reg 60 (input) - only update result if it was successful
      {
        result = inputVal; // update result - monitor input value
      }
    }
    monitorStates[i] = result; // Populate array
  }
}


void saveMode(int selectedMode) 
{
  
  // Get monitor states, and set them in monitorModes for the selected mode
  getMonitorStates(monitorModes[selectedMode]);

  // REVISIT: needs saving to eeprom
}


bool isMonitorInputPresent(int monitor, byte inputVal) 
{
  byte inputValRead;
  // write to monitor to select input
  writeMonitorInput(monitor, inputVal);

  delay(50); // REVISIT : configurable?

  // read back from monitor, if the value has stuck then the input is present
  readMonitorInput(monitor, inputValRead);

  return inputVal == inputValRead;
}


void setMonitorInputsAuto(bool override) 
{
  // for each monitor
  for (int i=0; i<numMonitors; i++) {

    // If monitor not connected, skip
    if (!is_monitor_connected(i) ) {
      continue;
    }
    // If override, delete all values
    if (override) {
      for (int j=0; j<maxSupportedMonitorInputs; j++) {
        monitorInputs[i][j] = 0xFF;
      }
    }
    // for each possible input
      // is this input already present - if so skip
      // is the input connected? --  //isMonitorInputPresent(inputVal)
        // set this value
  }
}


void setMonitorInputsFromModes() {
  // For each mode
    // For each monitor 
      // If input value not already present in monitorInputs[numMonitors][maxSupportedMonitorInputs]
        // add value to monitorInputs[numMonitors][maxSupportedMonitorInputs]
}


void setMonitorInputs(int selectedMode) 
{
  //monitorInputs[numMonitors][maxSupportedMonitorInputs]
  switch (selectedMode) {
    case 0 : // Full auto
      setMonitorInputsAuto(true);
      break;

    case 1 : // Extract from modes
      setMonitorInputsFromModes();
      break;

    case 2 : // Extract from modes plus auto
      setMonitorInputsFromModes();
      setMonitorInputsAuto(false);
      break;

  }



}


void selectMonitorInput(int monitor, int mode) 
{
  // Select i2c port -- would be needed with hardware i2c switching, with software it is just an array select
  // Send i2c write command
  
  // Read back?
  // Retry????
}


void setMode(int selectedMode) 
{
  // For each monitor
    // Set input to monitorModes[selectedMode][monitorI] = selectMonitorInput

  // Set currentMonitorMode

  // if turn on mode is to set to previous value
    // Save to eeprom

}


void incrementInput(int selectedMonitor) 
{
  // find current position currentMonitorMode[selectedMonitor] in monitorInputs[selectedMonitor]
  // Find next value, N+1 or 0 if N+1==0xFF or out of range 
  // Set monitor to +1 of that index, loop around if next value is 0xFF
  // selectMonitorInput

}


void loop()
{

  // Interrupt routine for button press
  // function changeI2cAddr(idx);


  delay(loopDelay); // Sample inputs 20 times per second... Maybe sleep and wakeup on interrupt

  // Temporarily, just sample here
  setupButton.update();
  setupButtonStatus = setupButton.read();
  for (int i=0;i<numModes;i++)
  {
    modeButtons[i].update();
    modeButtonStatus[i] = modeButtons[i].read();
  }
  // end sample

  // Which mode button is pressed?
  selectedMode = getSelectedMode();
  modeSelected = selectedMode > -1 ;

  // Main state machine
  if (!doneAction) {
    if (state == 0) { // idle
        
      if (setupButtonStatus) {
        state = 1; // Check for update settings mode
      } else if (modeSelected) {
        if (idleBehaviour == 0) {
          setMode(selectedMode);
        } else if (idleBehaviour == 1) {
          incrementInput(selectedMode);
        }
        doneAction = true;
      }
    } else if (state == 1) { // Checking if going into settings
      settingsCounter += 1;
      if (settingsCounter >= settingsMaxCount) {
        doneAction = true;
        settingsCounter = 0;
        if (modeSelected) {
          saveMode(selectedMode);
          state = 0; // go back to idle
        } else {
          state = 2; // go into settings mode
        }
      }
    } else if (state == 2) { // Settings mode
      if (setupButtonStatus) {
        // Exit settings mode
        state = 0;
        settingsPage = 0;
        doneAction = true;
      } else if (modeSelected) {

        // Check menu state for action to perform
        switch (settingsPage) {
          case 1:
            setSetupMode(selectedMode);
            break;
          case 2:
            setDefaultMode(0, selectedMode);
            break;
          case 3:
            setDefaultMode(1, selectedMode);
            break;
          case 4:
            setMonitorInputs(selectedMode);
            break;
        }

        // Navigate menu
        settingsPage = settingsMenu[settingsPage][modeSelected];
        doneAction = true;
      } // If no button pressed, do nothing

    } else {
      // If in unknown state, go back to idle
      state = 0;
    }
  } else {
    if (!setupButtonStatus && !modeSelected) {
      doneAction = false;
    }
  }


}