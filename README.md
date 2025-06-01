# Valmet Kotilämpö
This is meant to be a backup for if the several decades old HVAC system to keep it running. 


## Hypothesis

* Make backups of the 2st B2716-6 EEPROM's and write into 28C16 or 27C16 proms
* Make pinout diagram of the connector on the back to make it possible to replace with a total different cpu and motherboard 
* Draw Schema/PCB in order to be able to make a new PCB if the old ones can't be repaired any more 
* Create drop in replacement in the cpu socket with e.g and ESP32
  

## UV-Eprom
### Read from UV-Eprom B2716-6

Use the programmer [Relatively-Universal-ROM-Programmer](https://github.com/AndersBNielsen/Relatively-Universal-ROM-Programmer) and [firestarter](https://github.com/henols/firestarter). Usefull information about comptible chips can be found from 
[The Collector’s guide to Vintage Intel Microchips](https://deramp.com/downloads/mfe_archive/050-Component%20Specifications/Intel/VintageIntelMicrochipsRev4.pdf)

* Program the eeprom [programmer](https://github.com/henols/firestarter)
* First time "avrdude -c arduino -P /dev/ttyUSB0  -b 115200 -p atmega328p -D -U flash:w:firestarter_uno.hex:i"
* Second time it works with firestarter -v fw --install (Should debug and make case if I get time)
* Read out id from the EPROM  "firestarter id M2716" (firestarter does not have B2716 but M2716 version, but should be the same)
* Read out data from the EPROM  "firestarter read M2716 chip1.hex"
* Read out data from the EPROMS  "firestarter write M2716 chip1.hex"

### Write to EEprom CAT28C16A

```bash
# Download and run the installer

firestarter write CAT28C16A 3EF2H.bin
firestarter verify CAT28C16A 3EF2H.bin

firestarter write CAT28C16A A98EH.bin
firestarter verify CAT28C16A A98EH.bin

sudo chmod +x install_bjorn.sh && sudo ./install_bjorn.sh
# Choose the choice 1 for automatic installation. It may take a while as a lot of packages and modules will be installed. You must reboot at the end.
```



## Datasheets 
| No       | Description | IC           |
| ---      | ---         |---           |
| U1       | EEPROM | [B2716-6](https://www.alldatasheet.com/datasheet-pdf/pdf/22809/STMICROELECTRONICS/M2716.html) |





