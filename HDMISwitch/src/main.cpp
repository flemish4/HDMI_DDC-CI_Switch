#include <Arduino.h>

 // --------------------------------------
// i2c_scanner
//
// Version 1
//    This program (or code that looks like it)
//    can be found in many places.
//    For example on the Arduino.cc forum.
//    The original author is not know.
// Version 2, Juni 2012, Using Arduino 1.0.1
//     Adapted to be as simple as possible by Arduino.cc user Krodal
// Version 3, Feb 26  2013
//    V3 by louarnold
// Version 4, March 3, 2013, Using Arduino 1.0.3
//    by Arduino.cc user Krodal.
//    Changes by louarnold removed.
//    Scanning addresses changed from 0...127 to 1...119,
//    according to the i2c scanner by Nick Gammon
//    https://www.gammon.com.au/forum/?id=10896
// Version 5, March 28, 2013
//    As version 4, but address scans now to 127.
//    A sensor seems to use address 120.
// Version 6, November 27, 2015.
//    Added waiting for the Leonardo serial communication.
// 
//
// This sketch tests the standard 7-bit addresses
// Devices with higher bit address might not be seen properly.
//

#include <Wire.h>

byte lenMagicNum = 0x80;
byte hostSlaveAddr = 0x51;
byte edidHeader[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

void setup()
{
  Wire.begin();
  TWBR = 158; // Make clock really slow!
  //TWSR |= bit (TWPS0);

  Serial.begin(9600);
  while (!Serial);             // Leonardo: wait for serial monitor
  Serial.println("\nI2C Scanner");
}


int i2cWrite(byte addr, int numBytes, byte *Buffer)
{
  Wire.beginTransmission(addr); 
  for (int i=0; i<numBytes; i++) {
    Wire.write(Buffer[i]);
  }
  return Wire.endTransmission();
}


bool i2cRead(byte addr, byte *Buffer, int startIdx, int numBytes)
{
  int i = startIdx;
  Wire.requestFrom(addr,numBytes);
  while (Wire.available()){
    Buffer[i] = Wire.read();
    i+=1;
  }
  //Serial.print("Read got bytes num: ");
  //Serial.print(i-startIdx);
  //Serial.print(", out of: ");
  //Serial.println(numBytes);

  if (i-startIdx != numBytes) {
    return 1;
  }
  return 0;
}


int ddcWrite(byte addr, int numBytes, byte *Buffer)
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


  Wire.beginTransmission(addr);
  Wire.write(hostSlaveAddr);
  Wire.write(trueLen);
  for (int i=0; i<numBytes; i++) {
    Wire.write(Buffer[i]);
    checkSum ^= Buffer[i];
    Serial.print("Buffer[i]: ");
    Serial.print(Buffer[i], HEX);
    Serial.print(", ");
    Serial.print("checkSum: ");
    Serial.print(checkSum, HEX);
    Serial.print(", ");
  }
    Wire.write(checkSum);
  
    Serial.println("\n\n");
  return Wire.endTransmission();
}

int ddcRead(byte addr, byte offset, byte *Buffer, int startIdx, int numBytes, int incr = 32) 
{
  //Serial.print("Start of ddcRead: ");
  //Serial.print(addr, HEX);
  //Serial.print(", ");
  //Serial.print(offset, HEX);
  //Serial.print(", ");
  //Serial.print(startIdx);
  //Serial.print(", ");
  //Serial.print(numBytes);
  //Serial.print(", ");
  //Serial.println(incr);

  byte wrData[1];
  int wrResult;
  int thisNumBytes;
  int maxOffset = offset + numBytes; 


  for (int i=offset; i<maxOffset; i+=incr) {
    thisNumBytes = min(maxOffset-i, 32);
    //Serial.print("ddcReadLoop: ");
    //Serial.print(i);
    //Serial.print(", ");
    //Serial.print(i, HEX);
    //Serial.print(", ");
    //Serial.println(thisNumBytes);
    wrData[0] = i;
    wrResult = i2cWrite(addr, 1, wrData);
    //Serial.print("i2c write result: ");
    //Serial.println(wrResult);
    //Serial.print("i2c read result: ");
    i2cRead(addr, Buffer, i, thisNumBytes);
  }
  //Serial.println("ddcReadDone");
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


bool edid_read()
{
  byte Buffer[256];
  bool match;
  //int wrResult;
  //byte wrData[1];
  //
  //for (int i=0; i<255; i+=32) {
  //  wrData[0] = i;
  //  wrResult = i2cWrite(0x50, 1, wrData);
  //  i2cRead(0x50, Buffer, i, 32);
  //}
  ddcRead(0x50, 0x00, Buffer, 0, 256, 32);

  match = doArraysMatch(8, Buffer, edidHeader);

  Serial.print("EDID data: ");
  print_byte_array(256, Buffer);
  Serial.println("");
}


bool strange_thing() 
{
  byte Buffer00[5];
  byte Buffer40[1];

  ddcRead(0x3A, 0x00, Buffer00, 0, 5, 32);
  ddcRead(0x3A, 0x40, Buffer40, 0, 1, 32);
  Serial.print("3A, 00: ");
  print_byte_array(5, Buffer00);
  Serial.println("");
  Serial.print("3A, 40: ");
  print_byte_array(1, Buffer40);
  Serial.println("");

}



int monitorConnect() {

  // Check monitor is working
  byte Buffer[4];
  bool monitorCheck;
  Buffer[0] = 0x00;
  while (Buffer[0] == 0x00) {
    monitorCheck = i2cRead(0x50, Buffer, 0, 1);
    print_byte_array(4, Buffer);
  Serial.println("");
  }
  Serial.print("Monitor check: ");
  Serial.println(monitorCheck);
  Serial.print("Monitor check value: ");
  print_byte_array(4, Buffer);
  Serial.println("");

  delay(250);

  // Read all EDID
  edid_read();


  // Some other stuff with 0x3A , 0x00, 0x40, 0x80
  for (int i=0; i<16; i++) {
    strange_thing();
    delay(300);
  }
//
}


void loop()
{
  //byte error, address;
  //int nDevices;
//
  //Serial.println("Scanning...");
//
  //nDevices = 0;
  //for(address = 1; address < 127; address++ ) 
  //{
  //  // The i2c_scanner uses the return value of
  //  // the Write.endTransmisstion to see if
  //  // a device did acknowledge to the address.
  //  Wire.beginTransmission(address);
  //  error = Wire.endTransmission();
//
  //  if (error == 0)
  //  {
  //    Serial.print("I2C device found at address 0x");
  //    if (address<16) 
  //      Serial.print("0");
  //    Serial.print(address,HEX);
  //    Serial.println("  !");
//
  //    nDevices++;
  //  }
  //  else if (error==4) 
  //  {
  //    Serial.print("Unknown error at address 0x");
  //    if (address<16) 
  //      Serial.print("0");
  //    Serial.println(address,HEX);
  //  }    
  //}
  //if (nDevices == 0)
  //  Serial.println("No I2C devices found\n");
  //else
  //  Serial.println("done\n");


  //for (int i=0; i<256; i++) {
  //  Wire.beginTransmission(0x3A); 
  //  Wire.write(i);        //address of the byte  to read from
  //  Wire.endTransmission();
  //  delay(5);
//
  //  Wire.requestFrom(0x3A,1); // gets the value from the address mentioned above
  //  delay(10);
  //  if(Wire.available()){
  //    byte data = Wire.read();
  //    Serial.print(data, HEX);
  //    Serial.print(" ");
  //  } 
  //  delay(50);           // wait .05 seconds for next 
  //}






  // THIS SHOULD SUCCESSFULLY READ SOME DDC DATA
          //Wire.beginTransmission(0x37); 
          //Wire.write(0x51); // Host slave address
          //Wire.write(0x84); // Length 3?
          //Wire.write(0x03); // Capabilities request COMMAND
          //Wire.write(0x60); // Offset byte?
          //Wire.write(0x00); // Offset byte?
          //Wire.write(0x03); // Offset byte?
          //Wire.write(0xD9); // Checksum
          //Serial.println(Wire.endTransmission());
        //byte Buffer0[4] = {0x03, 0x60, 0x00, 0x03};
        //ddcWrite(0x37, 4, Buffer0);
//
//
        //delay(2000);
        //byte Buffer1[4] = {0x03, 0x60, 0x00, 0x01};
        //ddcWrite(0x37, 4, Buffer1);






        //byte Buffer1[4]

        // Read state of input select register
        //ddcRead(0x37, 0x60, Buffer2, 0, 11, 32);
        // Check checksum
        // Get input value from position 9






          //Wire.requestFrom(0x37,12);
          //delay(50);
          //while (Wire.available()){
          //  byte data = Wire.read();
          //  Serial.print(data, HEX);
          //  Serial.print(" ");
          //} 
  // END THIS SHOULD SUCCESSFULLY READ SOME DDC DATA






// write to 0x3A ack data: 0x00 
// read to 0x3A ack data: 0xAC 0xB8 0x1D 0x78 0x66
// write to 0x3A ack data: 0x40 
// read to 0x3A ack data: 0x80
// write to 0x3A ack data: 0x00 
// read to 0x3A ack data: 0xAC 0xB8 0x1D 0x78 0x66


  //monitorConnect();


  delay(5000);           // wait 5 seconds for next scan







  // Interrupt routine for button press





  // normal
    // Wait for button presses
    // On button press enter that mode
    // If press was setup button, enter, input select mode
    // If press and hold setup and a button for 3 seconds, record that as a new mode
    





















}