/*****************************************************************************
 * 
 * Program a GLS29EE010 Page Write EEPROM
 * using a Teensy 3.5 connected via an ad hoc adapter
 * 
 * See external documentation for details on the adapter.
 *  
 *  Paul Williamson KB5MU  --  paul@mustbeart.com
 *  License: GPL 3
 *
 *  Build environment:
 *  Arduino IDE 1.8.10 (https://www.arduino.cc/en/main/software).
 *  Teensyduino 1.48 (https://www.pjrc.com/teensy/td_download.html).
 *  SdFat 1.1.0 (https://github.com/greiman/SdFat)
 *  Compile options - Teensy 3.5, USB Serial, 120 MHz, Fastest with LTO.
 * 
 *  This sketch interacts with the user over the USB serial port?
 *  
 *
 *********************************************************************/

#include "SdFat.h"

#define ADDRESS_BUS_WIDTH   17
#define DATA_BUS_WIDTH       8

// Teensy pin definitions
#define WE  23
#define CE  16
#define OE  18
#define LED 13

// Teensy pin mapping of address/data bus starting from the LSB
int address_pins[] = { 10,9,8,7,6,5,4,3,19,36,17,35,2,20,21,1,0 };
int dq_pins[] = { 11,12,24,37,38,39,14,15 };

SdFatSdioEX sd;
File imagefile;

// For now we'll require a fixed filename. Raw binary format.
char filename[] = "IMAGE.BIN";


// Drive the address bus with a specified value
void set_address(unsigned long addr) {
  
  for (int i=0; i < ADDRESS_BUS_WIDTH; i++) {
    digitalWrite(address_pins[i], (addr & (1UL << i)) ? HIGH : LOW);
  }
}


// Drive the data bus with a specified value
void drive_data(byte data) {
  
  for (int i=0; i < DATA_BUS_WIDTH; i++) {
    pinMode(dq_pins[i], OUTPUT);
    digitalWrite(dq_pins[i], (data & (1UL << i)) ? HIGH: LOW);
  }
}

// Set the data bus to tristate (undriven)
void tristate_data_pins(void) {
  
  for (int i=0; i < DATA_BUS_WIDTH; i++) {
    pinMode(dq_pins[i], INPUT);
  }
}


// Return the 8-bit value on the data bus
byte read_data(void) {
  byte val = 0;

  for (int i=0; i < DATA_BUS_WIDTH; i++) {
    if (digitalRead(dq_pins[i])) {
      val |= (1 << i);
    }
  }  
  return val;
}


// Perform a single read cycle on the bus and return the result.
// This implementation runs at Teensy bus speed, except for a
// single delay to allow for the memory device's access time.
byte bus_read_cycle(unsigned long address) {
  byte value;

  set_address(address);
  tristate_data_pins();
  digitalWrite(OE, LOW);
  digitalWrite(CE, LOW);
  delayMicroseconds(1);       // wait for the slow memory devices
  value = read_data();        // get the result
  digitalWrite(OE, HIGH);
  digitalWrite(CE, HIGH);

  return value;
}


// Perform a single write cycle on the bus.
void bus_write_cycle(unsigned long address, byte value) {

  set_address(address);
  digitalWrite(OE, HIGH);
  drive_data(value);
  digitalWrite(CE, LOW);
  digitalWrite(WE, LOW);
  digitalWrite(WE, HIGH);
  digitalWrite(CE, HIGH);
}


// Send a command to the device
void device_command(byte command) {
  bus_write_cycle(0x5555, 0xAA);
  bus_write_cycle(0x2AAA, 0x55);
  bus_write_cycle(0x5555, command);
}


// Try to read the device ID and print it to the console
void identify_device(void) {
  byte family, device;

  digitalWrite(LED, HIGH);           // LED indicator that something is happening
 
  device_command(0x90);
  tristate_data_pins();
  set_address(0x0000);

  delayMicroseconds(10);            // T(IDA)

  // Not this. Not conventional bus reads. (?)
  //family = bus_read_cycle(0x0000);
  //device = bus_read_cycle(0x0001);

  digitalWrite(CE, LOW);
  digitalWrite(OE, LOW);

  delayMicroseconds(1);
  family = read_data();       // after T(AA)

  set_address(0x0001);

  delayMicroseconds(1);
  device = read_data();       // after T(AA)

  digitalWrite(CE, HIGH);

  // Exit product ID read mode
  device_command(0xF0);
  delayMicroseconds(10);            // T(IDA)
  digitalWrite(CE, LOW);
  delayMicroseconds(10);            // unspecified delay
  digitalWrite(CE, HIGH);           // return to idle state (not shown in timing diagram)
  
  digitalWrite(LED, LOW);

  Serial.print("Device ID is ");
  Serial.print(family, HEX);
  Serial.print(" ");
  Serial.print(device, HEX);
  Serial.println(". Should be BF 7 for GLS29EE010.");
}


// Erase the entire chip
void erase_device(void) {
  digitalWrite(LED, HIGH);
  device_command(0x80);
  device_command(0x10);
  delay(20);      // T(SCE)
  Serial.println("Device erased.");
  digitalWrite(LED, LOW);
}


// Program the device. This device uses page writes, so the writes are
// divided up into blocks of 128, with a wait for completion after each
// block is written.
void program_device(void) {
  unsigned long block_address;
  int buffer[128];
  int i;

  imagefile.seek(0);  // rewind

  Serial.println("Programming ...");

  for (block_address=0; block_address < 0x20000; block_address += 128) {

    // Read the file can interfere with the block timing. Read the whole
    // block first into a buffer, then program from there.
    for (i=0; i < 128; i++) {
      buffer[i] = imagefile.read();
      if (buffer[i] == -1) {
        Serial.print("\nError reading image file at address 0x");
        Serial.println(block_address+i, HEX);
        Serial.println("Aborting programming operation.");
        digitalWrite(LED, LOW);
        return;
      }
    }
    
    device_command(0xA0);     // software data protect write command

    digitalWrite(LED, HIGH);

    for (i=0; i < 128; i++) {
      bus_write_cycle(block_address+i, buffer[i]);
    }

    delayMicroseconds(200);     // T(BLCO)

    // Toggle Bit Polling method:
    // Keep reading the last address written until it comes back twice the same.
    // Only bit 6 is toggling, but it's easy to compare the whole byte.
    while (bus_read_cycle(block_address+127) != bus_read_cycle(block_address+127));

    digitalWrite(LED, LOW);
  }

  Serial.println("Programming complete!");
}


// Verify the device has been programmed successfully.
void verify_device(void) {
  unsigned long addr;
  int error_count = 0;
  int image_byte, device_byte;

  imagefile.seek(0);      // rewind to the top of the file

  digitalWrite(LED, HIGH);
  for (addr=0; addr < 0x20000; addr++) {
    device_byte = bus_read_cycle(addr);
    image_byte = imagefile.read();

    if (image_byte == -1) {
      Serial.println("Error reading image file for verify");
      digitalWrite(LED, LOW);
      return;
    }

    if (device_byte != image_byte) {
      Serial.print("Verify error at address 0x");
      Serial.print(addr, HEX);
      Serial.print(". Device contains 0x");
      Serial.print(device_byte, HEX);
      Serial.print(", should be 0x");
      Serial.println(image_byte, HEX);
      error_count++;
    }

    if (error_count > 10) {
      Serial.println("Too many errors, aborting verify.");
      digitalWrite(LED, LOW);
      return;
    }
  }

  digitalWrite(LED, LOW);
  if (error_count == 0) {
    Serial.println("Device verified successfully!");
  }
}


// Standard Arduino setup routine, runs once on powerup
void setup() {

  delay(2000);              // This seems necessary, but why?
  
  Serial.begin(9600);       // We will interact with the user on the USB serial port
  Serial.println("GLS29EE010 Programmer 0.1");
  
  // The address bus is always ours
  for (int i=0; i < ADDRESS_BUS_WIDTH; i++) {
    pinMode(address_pins[i], OUTPUT);
  }

  // So are the control signals
  pinMode(WE, OUTPUT);  digitalWrite(WE, HIGH);
  pinMode(CE, OUTPUT);  digitalWrite(CE, HIGH);
  pinMode(OE, OUTPUT);  digitalWrite(OE, HIGH);
  
  // We'll also be lighting up the Teensy's LED
  pinMode(LED_BUILTIN, OUTPUT);

  // Set up the SD card to read the image data
  if (!sd.begin()) {
    Serial.println("SD card not working!");
    sd.initErrorHalt("SdFatSdioEX begin() failed");
  }
}


// Standard Arduino loop function. Called repeatedly, forever.
void loop() {    
  Serial.println("\nHit Enter to check device ID.");
  while (Serial.read() != '\n');

  identify_device();

  Serial.println("\nHit Enter to erase the device.");
  while (Serial.read() != '\n');

  erase_device();

  // Make sure we have the image file
  imagefile = sd.open(filename, FILE_READ);
  if (!imagefile) {
    Serial.print("Image file ");
    Serial.print(filename);
    Serial.println(" is not available (failed to open it)");
    return; // loop back and try again
  }

  Serial.println("\nHit Enter to program device.");
  while (Serial.read() != '\n');

  program_device();
  
  Serial.println("\nHit Enter to verify device.");
  while (Serial.read() != '\n');

  verify_device();

  imagefile.close();
  // Loop back and prompt the user for another device
}
