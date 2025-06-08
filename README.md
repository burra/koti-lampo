# Valmet Kotilämpö
This is meant to be a backup for if the several decades old HVAC system to keep it running. 


## Hypothesis

* Make backups of the 2st B2716-6 EEPROM's and write into 28C16 or 27C16 proms
* Make pinout diagram of the connector on the back to make it possible to replace with a total different cpu and motherboard 
* Draw Schema/PCB in order to be able to make a new PCB if the old ones can't be repaired any more 
* Create drop in replacement in the cpu socket with e.g and ESP32
  

## UV-Eprom
UV-Eproms start to be rare and expesive as they where released in the begining of 1980 and the intel series I could't find where to buy 

* [Relatively-Universal-ROM-Programmer](https://github.com/AndersBNielsen/Relatively-Universal-ROM-Programmer)
* [Software](https://github.com/henols/firestarter_app)
* [Firmware](https://github.com/henols/firestarter)
* [The Collector’s guide to Vintage Intel Microchips](https://deramp.com/downloads/mfe_archive/050-Component%20Specifications/Intel/VintageIntelMicrochipsRev4.pdf) 

```bash
# install firestarter
pipx install firestarter
# program the programmer / arduino uno, this may fail first time when arduino is totally new (Should debug and make case if I get time)
firestarter -v fw --install
# avrdude -c arduino -P /dev/ttyUSB0  -b 115200 -p atmega328p -D -U flash:w:firestarter_uno.hex:i
```


### Read from UV-Eprom B2716-6
 
```bash
# On manjaro you may have to run to set rights for the usb port when you plug it in
# sudo chown $USER /dev/ttyUSB0
# Read out the software from the chips
firestarter read M2716 3EF2H.bin
firestarter verify M2716 3EF2H.bin

firestarter read M2716 A98EH.bin
firestarter verify M2716 A98EH.bin 
```

### Find a compatible EEprom replacement
As this chipset is not found easaly on the market one have to find an replacement that the read function is the same 

```bash
# Find the chipset or the one that is the same specification
firestarter search 2716
# This will give you and pinnout to compare the chipset you are thinking is possible replacement 
firestarter info M2716
```

### Write to EEprom CAT28C16A

```bash
# On manjaro you may have to run to set rights for the usb port when you plug it in
# sudo chown $USER /dev/ttyUSB0
firestarter write CAT28C16A 3EF2H.bin
firestarter verify CAT28C16A 3EF2H.bin

firestarter write CAT28C16A A98EH.bin
firestarter verify CAT28C16A A98EH.bin
```

## Disasmeble with [ghidra](https://github.com/NationalSecurityAgency/ghidra)


## Datasheets 
| Description | IC           |
| ---         |---           |
| EEPROM | [B2716-6](https://www.alldatasheet.com/datasheet-pdf/pdf/22809/STMICROELECTRONICS/M2716.html) |
| CPU | [P8035L](https://en.wikipedia.org/wiki/Intel_MCS-48) |






