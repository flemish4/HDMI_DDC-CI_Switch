#include <Arduino.h>
#include <Bounce2.h>
#include <SoftWire.h>
#define ARDBUFFER 16
#include <stdarg.h>

//#include <AsyncDelay.h>

// REVISITS:
// Add optional serial debug function and reporting
// Check all revisits
// Prevent attempting to set monitor state to FF



// Constants
  // Fundamental 
    // i2c/ddc values
      const byte lenMagicNum = 0x80;
      const byte hostSlaveAddr = 0x51;
      const byte edidHeader[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
      const byte maxDdcInputValSpec = 0x12;
      const byte maxDdcInputValMax  = 0xFF;

  // Configurable
    // Design Constraints 
      const int numMonitors = 2;
      const int maxSupportedMonitorInputs = 8;
      const int numModes = 4;

    // Pin Definitions
      const int ledPin = 13;
      const int setupButtonPin = 2;
      const int modeButtonPins[numModes] = {3,4,5,6};
      const int i2cPins[numMonitors][2] = {{7,8},{9,10}};

    // Timing delays
      const int i2cOperationDelay = 250; // ms between operations
      const int i2cReadWriteDelay = 50; // ms between writing and reading from a location
      const int loopDelay = 50; // 20 times per second
      const int settingsMaxCount = 3*(1000/loopDelay);

    // Settings menu
      const int settingsMenu[6][4] = {{1,2,4,5}, // Main - 0: Change setup button behaviour, 1: Change turn on behaviour, 2: Configure monitor input switching, 3: enable or disable serial
                                      {0,0,0,0}, // Setup button behaviour - 0: mode select, 1: input cycling
                                      {0,0,3,2}, // Turn on behaviour - 0: As turn off, 1: Do not change, 2: choose
                                      {0,0,0,0}, // Turn on behaviour mode select - 0: turn on to mode 0, 1 turn on to mode 1 etc.
                                      {0,0,0,0}, // Monitor input switching config : 0: Automatic detection (order will likely be wrong), 1: Match mode order (likely in order but may miss extras), 2: Match mode order + autodetected (likely in order but may have extras), 3: Automatic detection (ordering will likely be wrong) - test ALL possible values
                                      {0,0,0,0}  // Enable or disable serial debug levels : 0 : None, 1: Errors, 2: Warnings, 3: Debug
                                    };

    // Logging
      const char debugLevelStr[4][5] = {"NORM", "ERR ", "WARN", "INFO"};

// Semi constants - loaded from eeprom REVISIT
  int  idleBehaviour = 0; // 0 is mode select, 1 is input switching                                                 // REVISIT : Will be loaded from eeprom
  int  turnOnBehaviour = 0; // -1 is previous state, -2 is just leave, others are just button saves.                // REVISIT : Will be loaded from eeprom
  byte monitorModes[numModes][numMonitors] = {{0xFF, 0xFF},{0xFF, 0xFF},{0xFF, 0xFF},{0xFF, 0xFF}};                // REVISIT : Will be loaded from eeprom
  byte currentMonitorMode[numMonitors] = {0xFF, 0xFF};                                                             // REVISIT : Will be loaded from eeprom
  byte monitorInputs[numMonitors][maxSupportedMonitorInputs] = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // REVISIT : Will be loaded from eeprom
                                                                {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};


// State values
  int  state = 2; // 0 - idle, 1 - checking if settings mode, 2 - settings mode
  bool doneAction = 0;
  int  settingsCounter = 0;
  int  settingsPage  = 0; // 0 is main menu, 1 is setup button behaviour (values 0, 1 as above), 2 is turn on behaviour 
  bool modeSelected = false;
  int  selectedMode = -1;
  int  debugLevel = 3;
  bool ledState = false;

// Input objects
  // Buttons
    Bounce setupButton = Bounce();
    Bounce * modeButtons = new Bounce[numModes];

  // I2C
    SoftWire *i2cMonitorPorts[numMonitors];
    byte i2cMonitorTxBuffers[numMonitors][32];
    byte i2cMonitorRxBuffers[numMonitors][32];



//// Generic functions START
  void debugPrint(int level, const char *str, ...)
  {
    Serial.println("Attempt to use debugPrint");
    if (debugLevel<level) return; // Only print messages that are equal to or less than the debug level
    Serial.print(debugLevelStr[level]);
    Serial.print(": ");
    char buffer[256];
    va_list args;
    va_start(args, str);
    vsprintf(buffer, str, args);
    va_end(args);
    Serial.println(buffer);
  }

  bool isValueInArray(byte arr[], int len, byte val)
  {
    //debugPrint(3, "isValueInArray val: %X, len: %d, arr: %X", val, len, arr);
    for (int i = 0; i < len; i++) {
        if (arr[i] == val) {
            //debugPrint(3, "isValueInArray Evaluated as true");
            return true;
        }
      }
    //debugPrint(3, "isValueInArray Evaluated as false");
    return false;
  }

  int whereIsValueInArray(byte arr[], int len, byte val)
  {
    //debugPrint(3, "whereIsValueInArray val: %X, len: %d, arr: %X", val, len, arr);
    for (int i = 0; i < len; i++) {
        if (arr[i] == val) {
            //debugPrint(3, "whereIsValueInArray returned: %d", i);
            return i;
        }
      }
    //debugPrint(3, "whereIsValueInArray Evaluated as false");
    return -1;
  }

  bool doArraysMatch(int len, byte a[], const byte b[])
  {
    //debugPrint(3, "doArraysMatch len: %d, a: %X, b: %X", len, a, b);
    for (int i=0; i<len; i++) 
    {
      if (a[i] != b[i])  {
        //debugPrint(3, "doArraysMatch Evaluated as false");
        return false;
      }
    }
    //debugPrint(3, "doArraysMatch Evaluated as true");
    return true;
  }

  void print_byte_array(byte Buffer[], int len)
  {
    for (int i; i<len; i++) 
    {
      Serial.print("0x");
      Serial.print(Buffer[i]>>4,  HEX);
      Serial.print(Buffer[i]&0x0F,HEX);
      Serial.print(" ");
    }
  }

  int getMaxSetBit(bool arr[], int len) 
  {
    //debugPrint(3, "getMaxSetBit len: %d, arr: %X", len, arr);
    for (int i=0; i<len; i++) {
      if (arr[i]) {
        //debugPrint(3, "getMaxSetBit returned: %d", i);
        return i;
      }
    }
    //debugPrint(3, "getMaxSetBit returned: -1");
    return -1;
  }
//// Generic functions END


//// Arduino Debug START

void toggleLed()
{
  ledState = !ledState;
  if (ledState) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
}



//// Arduino Debug END


//// I2C Functions START

  bool i2cWrite(int monitor, byte addr, int numBytes, byte Buffer[])
  {
    //debugPrint(3, "i2cWrite numBytes: %d, monitor: %d, addr: %X, buffer: %X", numBytes, monitor, addr, Buffer);
    int result;
    i2cMonitorPorts[monitor]->beginTransmission(addr); 
    for (int i=0; i<numBytes; i++) {
      i2cMonitorPorts[monitor]->write(Buffer[i]);
    }
    result = i2cMonitorPorts[monitor]->endTransmission();
    //debugPrint(3, "i2cWrite result: %d", result);
    return result == 0;
  }

  bool i2cRead(int monitor, byte addr, byte Buffer[], int startIdx, int numBytes)
  {
    //debugPrint(3, "i2cRead startIdx: %d, numBytes: %d, monitor: %d, addr: %X", startIdx, numBytes, monitor, addr);
    int i = startIdx;
    i2cMonitorPorts[monitor]->requestFrom((uint8_t)addr, (uint8_t)numBytes, (uint8_t)true);
    while (i2cMonitorPorts[monitor]->available()){
      Buffer[i] = i2cMonitorPorts[monitor]->read();
      i+=1;
    }
    //debugPrint(3, "i2cRead read num: %d, out of: %d", i-startIdx, numBytes);
    Serial.print("numBytesRead = ");
    Serial.println(i-startIdx);
    if (i-startIdx != numBytes) {
      return false;
    }
    return true;
  }
//// I2C Functions END


//// DDC Functons

  bool isChecksumValid(byte Buffer[], int len) {
    byte calcChecksum = 0;
    for (int i=0; i<len-1; i+=1) {
      calcChecksum ^= Buffer[i];
    }
    //debugPrint(3, "isChecksumValid checksum: %X, from: %X", calcChecksum, Buffer);
    return calcChecksum == Buffer[len-1];
  }

  bool ddcWrite(int monitor, byte addr, int numBytes, byte Buffer[])
  {
    //debugPrint(3, "ddcWrite monitor: %d, numBytes: %d, addr: %X, Buffer: %X", monitor, numBytes, addr, Buffer);
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
    
      Serial.println("end");
    return i2cMonitorPorts[monitor]->endTransmission() == 0;
  }

//ddcRead(monitor, 0x37, 0x60, bufferInt, 11, 32);
  bool ddcRead(int monitor, byte addr, byte offset, byte Buffer[], int numBytes, bool isDdcWrite = true, int incr = 32) 
  {
    Serial.print("ddcRead: ");
    Serial.print("monitor: ");
    Serial.print(monitor);
    Serial.print(", addr: ");
    Serial.print(addr, HEX);
    Serial.print(", offset: ");
    Serial.print(offset, HEX);
    Serial.print(", numBytes: ");
    Serial.print(numBytes);
    Serial.print(", incr: ");
    Serial.println(incr);
    //debugPrint(3, "ddcRead monitor: %d, numBytes: %d, addr: %X, offset: %X", monitor, numBytes, addr, offset);
    byte wrData[2] = {};
    bool result = true;
    int thisNumBytes;

    for (int i=0; i<numBytes; i+=incr) {
      if (i>0) {
        delay(i2cOperationDelay);
      }
      thisNumBytes = min(numBytes-i, 32);
      wrData[1] = i+offset;

      if (isDdcWrite) {
        wrData[0] = 0x01;
        result = result & ddcWrite(monitor, addr, 2, wrData);
      } else {
        result = result & i2cWrite(monitor, addr, 1, &wrData[1]);
      }
      delay(i2cReadWriteDelay);
      //result = result & i2cWrite(monitor, addr, 1, wrData);
      result = result & i2cRead(monitor, addr, Buffer, i, thisNumBytes);

    }

    // Check checksum 
    Serial.print(", result: ");
    Serial.println(result);
    result = result ; //& isChecksumValid(0x6E, Buffer, numBytes); REVISIT : Checksum not working correctly
    Serial.print(", result: ");
    Serial.println(result);

    return result;
  }

  bool edid_read(int monitor, int len)
  {
    byte Buffer[len];
    bool match;
    ddcRead(monitor, 0x50, 0x00, Buffer, len, false, 32);
    match = doArraysMatch(8, Buffer, edidHeader);
    //Serial.print("EDID data: ");
    //print_byte_array(Buffer, 256);
    //Serial.println("");
    //debugPrint(3, "edid_read monitor: %d, len: %d, match: %X", monitor, len, match);
    return match;
  }
//// DDC Functons END


//// Monitor Control Functions START
  bool is_monitor_connected(int monitor) {
    bool result = edid_read(monitor, 8);
    //debugPrint(3, "is_monitor_connected monitor: %d, connected?: %X", monitor, result);
    return result;
  }

  bool readMonitorInput(int monitor, byte *inputVal) 
  {
    byte bufferInt[11] = {};
    bool result = ddcRead(monitor, 0x37, 0x60, bufferInt, 11, true, 32);
    Serial.print("readMonitorInput bufferInt = ");
    print_byte_array(bufferInt, 11);
    Serial.println("");
    Serial.println(bufferInt[9]);
    *inputVal = bufferInt[9];
    //debugPrint(3, "readMonitorInput monitor: %d, inputVal: %X, success?: %X", monitor, *inputVal, result);
    return result;
  }

  bool writeMonitorInput(int monitor, byte inputVal) 
  {
    if (inputVal == (byte)0xFF) return false;
    byte bufferInt[4] = {0x03, 0x60, 0x00, inputVal};
    bool result = ddcWrite(monitor, 0x37, 4, bufferInt);
    //debugPrint(3, "writeMonitorInput monitor: %d, inputVal: %X, success?: %X", monitor, inputVal, result);
    return result;
  }


  bool selectMonitorInput(int monitor, byte inputVal) 
  {
    byte inputValRead = 0xFF;
    Serial.println("selectMonitorInput pre ");
    Serial.println(monitor);
    Serial.println(inputVal);

    // write to monitor to select input
    writeMonitorInput(monitor, inputVal);

    delay(i2cOperationDelay); 

    // read back from monitor, if the value has stuck then the input is present
    readMonitorInput(monitor, &inputValRead);

    Serial.print("selectMonitorInput post ");
    bool result = inputVal == inputValRead;
    //debugPrint(3, "selectMonitorInput monitor: %d, inputVal: %X, success?: %X", monitor, inputVal, result);
    Serial.print(inputVal);
    Serial.print(" ");
    Serial.print(inputValRead);
    Serial.print(" ");
    Serial.println(result);
    return result;
  }
//// Monitor Control Functions END


//// Settings Helper Functions START
  byte getMonitorInputsMaxValidIdx(int monitor) {
    for (int i=0; i<maxSupportedMonitorInputs; i++) {
      if (monitorInputs[monitor][i] == (byte)0xFF) {
        //debugPrint(3, "getMonitorInputsMaxValidIdx monitor: %d, return: %d, success?: %X", monitor, i-1);
        return i-1;
      }
    }
    //debugPrint(1, "getMonitorInputsMaxValidIdx This is very odd");
    return 255;
  }

  void getMonitorStates(byte monitorStates[]) 
  {
    byte result;
    byte inputVal;
    // For each monitor
    for (int i=0; i<numMonitors; i++) {
      Serial.print("GetMonitorStates: ");
      Serial.println(i);
      result = 0xFF; // Default value -- invalid
      // Check if active
      if (is_monitor_connected(i)) { // Check if monitor is on -- maybe unnecessary
        Serial.println("is connected");
        if (readMonitorInput(i, &inputVal)) // Read from ddc reg 60 (input) - only update result if it was successful
        {
          Serial.println("Got monitor input read successfully");
          Serial.println(inputVal);
          result = inputVal; // update result - monitor input value
        }
      }
      monitorStates[i] = result; // Populate array
      //debugPrint(3, "getMonitorStates monitorStates[%d]: %X", i, result);
    }
  }

  int getPossibleInputs(int monitor, int maxValue, byte possibleInputs[]) {
    int inputCount = 0;
    bool inputPresent;
    for (int i=0; i<=maxValue; i++) {
      inputPresent = selectMonitorInput(monitor, i);
      if (inputPresent) {
        possibleInputs[inputCount] = i;
        inputCount += 1;
      }
      delay(i2cOperationDelay);
    }
    //debugPrint(3, "getPossibleInputs inputCount: %d, possibleInputs: %X", inputCount, possibleInputs);
    return inputCount;
  }
//// Settings Helper Functions END


//// Settings Functions START

  void setMonitorInputsAuto(bool override, bool checkAllValues) 
  {
    //debugPrint(3, "setMonitorInputsAuto override: %d, checkAllValues: %d", override, checkAllValues);
    byte monitorInputsCurIdx;
    int maxValue = checkAllValues ? maxDdcInputValMax : maxDdcInputValSpec;
    int monitorMaxInputs;
    byte possibleInputs[maxValue+1] = {};

    // for each monitor
    for (int i=0; i<numMonitors; i++) {
      //debugPrint(3, "setMonitorInputsAuto monitor loop before monitorInputs[%d]=%X", i, monitorInputs[i]);
      // If monitor not connected, skip
      Serial.print("Auto detecting monitor: ");
      Serial.println(i);
      if (!is_monitor_connected(i) ) {
        Serial.println("Not connected");
        continue;
      }
      // If override, delete all values
      if (override) {
        Serial.println("Overriding");
        for (int j=0; j<maxSupportedMonitorInputs; j++) {
          monitorInputs[i][j] = 0xFF;
        }
      }
      // Get possible inputs from monitor
      monitorMaxInputs = getPossibleInputs(i, maxValue, possibleInputs);
      Serial.print("monitorMaxInputs: ");
      Serial.println(monitorMaxInputs);
      
      // Get current max index for inputs
      monitorInputsCurIdx = getMonitorInputsMaxValidIdx(i) + 1 ;
      Serial.print("monitorInputsCurIdx: ");
      Serial.println(monitorInputsCurIdx);

      // For each possibleInput
      for (int j=0; j<monitorMaxInputs; j++) {
      Serial.print("j: ");
      Serial.println(j);
        // Check if the input is already in monitorInputs 
        if (!isValueInArray(monitorInputs[i], maxSupportedMonitorInputs, possibleInputs[j])) 
        {
          Serial.println("isValueInArray");
          if (monitorInputsCurIdx >= maxSupportedMonitorInputs) {
            Serial.println("Hit max inputs");
            return;
          }
          Serial.println("Not present - adding");
          // If it isn't, add it
          monitorInputs[i][monitorInputsCurIdx] = possibleInputs[j];
          // Increment the counter and ensure no more than maxSupportedMonitorInputs are added (8)
          monitorInputsCurIdx += 1;
        }
      }
      //debugPrint(3, "setMonitorInputsAuto monitor loop after monitorInputs[%d]=%X", i, monitorInputs[i]);
      Serial.print("monitorInputs[i] = ");
      print_byte_array(monitorInputs[i], maxSupportedMonitorInputs);
      Serial.println("");
    }
  }

  // REVISIT : Share with function above setMonitorInputsAuto
  void setMonitorInputsFromModes() {
    byte monitorInputsCurIdx;
    // For each mode
    for (int modeNum=0; modeNum<numModes; modeNum++) 
    {
      // For each monitor 
      for (int monNum=0; monNum<numMonitors; monNum++)
      {
        //debugPrint(3, "setMonitorInputsFromModes monitor loop before monitorInputs[%d]=%X", monNum, monitorInputs[monNum]);
        // Get current max index for inputs
        monitorInputsCurIdx = getMonitorInputsMaxValidIdx(monNum) + 1 ;
        // If mode monitor input not already present in monitorInputs[numMonitors][maxSupportedMonitorInputs]
        if (!isValueInArray(monitorInputs[monNum], maxSupportedMonitorInputs, monitorModes[modeNum][monNum])) 
        {   
          // If it isn't, add it
          monitorInputs[monNum][monitorInputsCurIdx] = monitorModes[modeNum][monNum];
          // Increment the counter and ensure no more than maxSupportedMonitorInputs are added (8)
          monitorInputsCurIdx += 1;
          if (monitorInputsCurIdx >= maxSupportedMonitorInputs) {
            return;
          }

        }
        //debugPrint(3, "setMonitorInputsFromModes monitor loop after monitorInputs[%d]=%X", monNum, monitorInputs[monNum]);
      }
    }
  }

  void setMonitorInputs(int selectedMode) 
  {
    //debugPrint(3, "setMonitorInputs");
    switch (selectedMode) {
      case 0 : // Full auto
        setMonitorInputsAuto(true, false);
        break;

      case 1 : // Extract from modes
        setMonitorInputsFromModes();
        break;

      case 2 : // Extract from modes plus auto
        setMonitorInputsFromModes();
        setMonitorInputsAuto(false, false);
        break;

      case 3 : // Full FULL auto
        setMonitorInputsAuto(true, true);
        break;
        
    }
  }

  void setSetupMode(int idleBehaviourVal) 
  {
    //debugPrint(3, "setSetupMode");
    // REVISIT: needs saving to eeprom
    idleBehaviour = idleBehaviourVal;
  }

  void setDefaultMode(int page, int turnOnBehaviourVal)  
  {
    //debugPrint(3, "setDefaultMode");
    // REVISIT: needs saving to eeprom
    if (page==0 && turnOnBehaviourVal<=1) {
      turnOnBehaviour = -1-turnOnBehaviourVal;
    } else if (page==1) {
      turnOnBehaviour = turnOnBehaviourVal;
    }
  }

  void setDebugLevel(int debugLevelVal)
  {
    debugLevel = debugLevelVal;
  }


  void saveMode(int selectedMode) 
  {
    //debugPrint(3, "saveMode");
    
    // Get monitor states, and set them in monitorModes for the selected mode
    getMonitorStates(monitorModes[selectedMode]);

    Serial.print("monitorModes[selectedMode] = ");
    print_byte_array(monitorModes[selectedMode], numMonitors);
    Serial.println("");
    // REVISIT: needs saving to eeprom
  }
//// Settings Functions END


//// Normal Operation Functions START

  void setMode(int selectedMode) 
  {
    Serial.print("setMode: ");
    Serial.print(selectedMode);

    Serial.print("monitorModes[i] b= ");
    print_byte_array(monitorModes[selectedMode], numMonitors);
    Serial.println("");

    //debugPrint(3, "setMode");
    // For each monitor 
    for (int monNum=0; monNum<numMonitors; monNum++)
    {
      // Set input to monitorModes[selectedMode][monitorI] = selectMonitorInput
      selectMonitorInput(monNum, monitorModes[selectedMode][monNum]);
      currentMonitorMode[monNum] = monitorModes[selectedMode][monNum];
    }
    // REVISIT: if turn on mode is to set to previous value
      // Save to eeprom

  }

  void incrementInput(int selectedMonitor) 
  {
    Serial.print("incrementInput: ");
    Serial.print(selectedMonitor);
    
    Serial.print("monitorInputs[i] b= ");
    print_byte_array(monitorInputs[selectedMonitor], maxSupportedMonitorInputs);
    Serial.println("");

    Serial.print("currentMonitorMode[i] b= ");
    Serial.print(currentMonitorMode[selectedMonitor], HEX);
    Serial.println("");

    //debugPrint(3, "incrementInput");
    // find current position currentMonitorMode[selectedMonitor] in monitorInputs[selectedMonitor]
    byte curInputIdx = whereIsValueInArray(monitorInputs[selectedMonitor], maxSupportedMonitorInputs, currentMonitorMode[selectedMonitor]);

    Serial.println(curInputIdx);
    // Find next value, N+1 or 0 if N+1==0xFF or out of range 
    byte maxInputIdx = getMonitorInputsMaxValidIdx(selectedMonitor);
    Serial.println(maxInputIdx);

    byte nxtInputIdx = curInputIdx == maxInputIdx ? 0 : curInputIdx + 1;
    Serial.println(nxtInputIdx);
    byte nxtInputVal = monitorInputs[selectedMonitor][nxtInputIdx];
    Serial.print("nextinputval: ");
    Serial.println(nxtInputVal);
    // Set monitor to +1 of that index, loop around if next value is 0xFF
    selectMonitorInput(selectedMonitor, nxtInputVal);
    currentMonitorMode[selectedMonitor] = nxtInputVal; // REVISIT : Move into select monitor input

    // REVISIT: if turn on mode is to set to previous value
      // Save to eeprom

  }
//// Normal Operation Functions END


// Main setup
void setup()
{
  pinMode(ledPin, OUTPUT);
  toggleLed();
  
  TWBR = 158; // Make clock really slow!
  //TWSR |= bit (TWPS0);

  Serial.begin(115200); // REVISIT - could change speed
  Serial.println("HDMI Switch");
  //debugPrint(3, "setup");





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
  
  
  // REVIST: Load variables from eeprom, initialize
  for (int monNum=0; monNum<numMonitors; monNum++) // This will be the - don't change option
  {
    readMonitorInput(monNum, &currentMonitorMode[monNum]);
  }
  //debugPrint(3, "done setup");
  Serial.println("Done setup\n\n");
}


// Main loop
void loop()
{
  delay(loopDelay); // Sample inputs 20 times per second... Maybe sleep and wakeup on interrupt
  toggleLed();
  //debugPrint(3, "Top of loop");
  //Serial.println("Top of loop");

  // REVISIT : Interrupt routine for button press
  
  // Temporary values
  bool setupButtonStatus = false;
  bool modeButtonStatus[numModes] = {false,false,false,false};

  // Temporarily, just sample here
  setupButton.update();
  setupButtonStatus = !setupButton.read();
  for (int i=0;i<numModes;i++)
  {
    modeButtons[i].update();
    modeButtonStatus[i] = !modeButtons[i].read();
  }
  // end sample

  // Which mode button is pressed?
  selectedMode = getMaxSetBit(modeButtonStatus, numModes) ;
  modeSelected = selectedMode > -1 ;

  //debugPrint(3, "loop done reads");
  //Serial.println("Done reads");

  Serial.print  ("state          : ");
  Serial.print  (state          );
  Serial.print  ("\tidleBehaviour   : ");
  Serial.print  (idleBehaviour  );
  Serial.print  ("\tturnOnBehaviour : ");
  Serial.print  (turnOnBehaviour);
  Serial.print  ("\tdoneAction     : ");
  Serial.print  (doneAction     );
  Serial.print  ("\tsettingsCounter: ");
  Serial.print  (settingsCounter);
  Serial.print  ("\tsettingsPage   : ");
  Serial.print  (settingsPage   );
  Serial.print  ("\tmodeSelected   : ");
  Serial.print  (modeSelected   );
  Serial.print  ("\tselectedMode   : ");
  Serial.print  (selectedMode   );
  Serial.print  ("\tdebugLevel     : ");
  Serial.print  (debugLevel     );
  Serial.print  ("\tledState       : ");
  Serial.println(ledState       );



  //byte monitorModes[numModes][numMonitors] = {{0xFF, 0xFF},{0xFF, 0xFF},{0xFF, 0xFF},{0xFF, 0xFF}};                // REVISIT : Will be loaded from eeprom
  //byte currentMonitorMode[numMonitors] = {0xFF, 0xFF};                                                             // REVISIT : Will be loaded from eeprom
  //byte monitorInputs[numMonitors][maxSupportedMonitorInputs] = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // REVISIT : Will be loaded from eeprom
  //                                                              {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};




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
      if (!setupButtonStatus) {
        settingsCounter = 0;
        state = 0;
        idleBehaviour = !idleBehaviour;
        doneAction = true;
      } else {
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
          case 5:
            setDebugLevel(selectedMode);
            break;
        }

        // Navigate menu
        //Serial.println(settingsMenu[settingsPage]);
        Serial.println(modeSelected);
        Serial.println(settingsMenu[settingsPage][selectedMode]);
        settingsPage = settingsMenu[settingsPage][selectedMode];
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
  //Serial.println("End of loop");


}