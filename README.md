# 29EE010 EEPROM Programmer for Teensy 3.5

This Arduino sketch runs on a Teensy 3.5. Wired up to an appropriate
fixture, it checks the device ID, erase, programs, and verifies a
128Kbyte image in a Greenliant GLS29EE010 128Kx8 Page-Write EEPROM.

## The Story

I needed a programmable replacement for a KM23C1010G masked ROM in a
vintage device I was adapting. Specifically, an IBM/Lexmark
Wheelwriter 1000 typewriter, which I was converting into a printing
terminal, following the lead of the
[Cadetwriter subproject](https://github.com/IBM-1620/Cadetwriter) from
the IBM 1620 Jr. project sponsored by the
[Computer History Museum](https://computerhistory.org) in Mountain View,
California. A quick search of the usual parts supply houses turned up
the [Greenliant GLS29EE010](GLS29EE010_datasheet.pdf), in stock at a
very low price, along with lots of unobtainium military-rated parts
for hundreds of dollars each. The choice was pretty easy.

The original masked ROM in the Wheelwriter was a SOP32 surface-mount device,
but the logic board had an alternate footprint for a 32-pin DIP package.
I was able to desolder the masked ROM once without damage, but I didn't
want to make a habit of it. I installed a DIP socket in the alternate
footprint and bought EEPROMs in DIP packages. Thanks to some smart
standardization by [JEDEC](https://www.jedec.org) back in the day, DIP
and SOP memory devices of this type all have compatible pinouts.

I have one of the popular, cheap PROM programmers that are readily available
on eBay, the TL866CS. It had the equivalent SST29EE010 device from Silicon
Storage Technology in its list of supported devices. I think this is
probably an identical part, since Greenliant is a successor of SST.
I expected to be able to use the TL866CS to read out the masked ROM using
the settings for the EEPROM (using a
[SOP to DIP adapter](https://www.digikey.com/product-detail/en/aries-electronics/32-650000-10/A323-ND/123754))
and again to program the actual DIP EEPROMs. It didn't work out that way.
Neither operation worked.

I was able to use the TL866CS to read out the masked ROM, by choosing the
settings for an older UV/OTP EPROM with the same JEDEC-standard pinout, the
M27C1001. Reading works pretty much the same on any of these devices, and
the settings for that older device worked just fine. That wouldn't work for
programming, though, unless I switched to UV EPROMs. I didn't really want
to mess around with UV EPROMs, though, and by this time I'd already
received a supply of GLS29EE010 devices. The TL866CS proved unable
to program them. It wasn't anything subtle, either. The TL866CS wasn't
even using the right signals to read the device ID. I have to conclude
that the settings for that device are broken or incompatible with the
TL866CS hardware.

I chose the [Teensy 3.5](https://www.pjrc.com/store/teensy35.html) as the
platform for a DIY programmer for this chip, mainly because it is the
processor used on the Cadetwriter project. It's a good choice, because it
has enough pins, and those pins are 5V-tolerant. I'd already been using it
in a [related project](https://github.com/MustBeArt/V20_ROM_Dump), an attempt
to dump the contents of that same masked ROM without removing it from the
logic board. That didn't work, but as a side effect of that other attempt,
I had a Teensy 3.5 already wired up to a 40-pin DIP clip. With minor
modifications, that same wiring would work to connect the Teensy 3.5 to
the DIP EEPROM. The DIP clip serves in place of the more usual ZIF socket
used on programmers. See the [Wiring Chart](29EE010_Programmer_Wiring_Chart.pdf)
for details, and a [photo](29EE010_Programmer_adapter.jpg) of my build.
I used female headers to connect to the DIP clip, so the DIP clip can still
be used for other purposes.

## The Code

This sketch is a reasonably straightforward translation of the programming
requirements set out in the data sheet into the Arduino environment.
I used version 1.8.10 of the
[Arduino IDE](https://www.arduino.cc/en/Main/Software), but any modern
version should work. You'll also need to install
[Teensy support for Arduino](https://www.pjrc.com/teensy/teensyduino.html);
I used version 1.48. The last thing you'll need is the
[SdFat library](https://github.com/greiman/SdFat), since this sketch reads
the binary image to be programmed into the chip from a file named IMAGE.BIN
on the micro SD card in the Teensy 3.5. I used version 1.1.0. The Arduino
IDE must be set to use the Teensy 3.5 with USB Serial. I used clock settings
of 120 MHz and "Fastest with LTO".

Wire up the EEPROM to the Teensy. You don't have to do it with a DIP clip
the way I did. You can use a solderless breadboard or even an actual ZIF
socket, or anything else you can rig up. Aside from power and ground, you
can hook up any signal to any digital I/O pin on the Teensy 3.5. If you
don't follow the pin assignments in my wiring chart, you'll need to alter
the pin assignments near the top of the code to match. Trust me, if you get
the pin assignments wrong, it won't work. I've proven that. Double check.

The Teensy 3.5 can power itself and the EEPROM from the USB cable, provided
the microscopically tiny jumper between two gold pads on the underside of
the board has not been cut. If it has, you can reconnect it with a blob of
solder.

The sketch interacts with you through the USB serial port. The serial
monitor function built into the Arduino IDE works fine for this. The sketch
walks you through the steps of checking the device ID, then erasing the
device, then programming the device, then verifying the device. It pauses
for you to hit Enter before each step. If at any point you don't want to
proceed, just unplug the Teensy. There's no code for doing anything but
marching through all the steps. The UI is primitive but sufficient for
what I needed.

The image to be programmed into the device must be in the root directory
of a micro SD card plugged in to the Teensy 3.5, and it must be named
IMAGE.BIN. It must be an exact 128K byte image to exactly fill the EEPROM,
though the code does not check for that. There's no code for partial device
programming or for picking any other file name.

## Licensing

The code and original documentation in this project are licensed under
GPL 3. I'm also including for convenience a copy of the GLS29EE010 datasheet,
which is publicly available from Greenliant.