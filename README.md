# Valmet Kotilämpö
This is meant to be a backup for if the several decades old HVAC system to keep it running. 


## Hypothesis

* Make backups of the 2st B2716-6 EEPROM's and write into 28C16 or 27C16 proms
* Make pinout diagram of the connector on the back to make it possible to replace with a total different cpu and motherboard 
* Draw Schema/PCB in order to be able to make a new PCB if the old ones can't be repaired any more 
* Create drop in replacement in the cpu socket with e.g and ESP32
  

## Reading out content from EEPROM 27C16

Use the programmer [Relatively-Universal-ROM-Programmer](https://github.com/AndersBNielsen/Relatively-Universal-ROM-Programmer) and [firestarter](https://github.com/henols/firestarter)

* Program the eeprom [programmer](https://github.com/henols/firestarter)
* First time "avrdude -c arduino -P /dev/ttyUSB0  -b 115200 -p atmega328p -D -U flash:w:firestarter_uno.hex:i"
* Second time it works with firestarter -v fw --install (Should debug and make case if I get time)
* Read out id from the PROMS  "firestarter id M2716" (firestarter does not have B2716 but M2716 version, but should be the same)
* Read out data from the PROMS  "firestarter read M2716 chip1.hex"
* Read out data from the PROMS  "firestarter write M2716 chip1.hex"



