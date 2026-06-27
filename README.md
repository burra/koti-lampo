# Valmet Kotilämpö
This is meant to be a backup for if the several decades old HVAC system to keep it running. 


## Hypothesis

* ~~Make backups of the 2st B2716-6 EEPROM's from the spare pcb and write into 28C16 or 27C16 proms~~
  *  ~~Read out prom from the original and compare if they match in checksum, Yes they do~~
  *  Disassemble the code from the proms ? 
* Make pinout diagram of the DB25 connector on the back to make it possible to replace with a total different cpu and motherboard 
* Draw Schema/PCB in order to be able to make a new PCB if the old ones can't be repaired any more 
* Create drop in replacement in the cpu socket with e.g and ESP32
  

## UV-Eprom
UV-Eproms start to be rare and expensive as they where released in the beginning of 1980 and the intel series I couldn't find where to buy 

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
As this chipset is not found easily on the market one have to find a replacement that the read function is the same 

```bash
# Find the chipset or the one that is the same specification
firestarter search 2716
# This will give you and pinout to compare the chipset you are thinking is possible replacement 
firestarter info M2716
```

### Clear EEprom and verify it's empty
```bash
# Some EEprom support to be erased but I haven't found any 
# firestarter erase AM27C512
# Create a empty file with 16k bits of data 
dd if=/dev/zero bs=1 count=2048 | tr '\0' '\377' > empty.bin 
firestarter write CAT28C16A empty.bin
firestarter verify CAT28C16A empty.bin
```
### Write UV-Eprom that is bigger 

Happened to have lying around from the past M2732AF6 so tried to program these also.   

```bash
# Make binary bigger so software don't complain
dd if=/dev/zero bs=1 count=2048 | tr '\0' '\377' >> 3EF2H_32k.bin
# Tune up voltage to max with turning potentiometer RV1 to 21V in my case 17.36v for max
firestarter vpp #VPP: 17.36v, Internal VCC: 4.90v
firestarter vpe #VPE: 22.10v, Internal VCC: 4.92v   
firestarter -v write  --vpe-as-vpp M2732A 3EF2H_32k.bin
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

## Disassemble with [ghidra](https://github.com/NationalSecurityAgency/ghidra)


## Datasheets 
| Description | IC           |
| ---         |---           |
| EEPROM | [B2716-6](https://www.alldatasheet.com/datasheet-pdf/pdf/22809/STMICROELECTRONICS/M2716.html) |
| CPU | [P8035L](https://en.wikipedia.org/wiki/Intel_MCS-48) |






