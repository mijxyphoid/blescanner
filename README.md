### blescanner.c
Linux C Code for Scanning Bluetooth Devices, and returning the Devices MAC Address, UUID, Major, Minor Versions, TX Power and RSSID.

This is a fork of https://github.com/damian-kolakowski/intel-edison-playground/blob/master/scan.c with very minor additions.

To compile, use gcc -o blescanner blescanner.c -lbluetooth

Requires bluez, and libbluetooth-dev for compilation
More information can be found at 
https://www.justsmarthomes.com/viewtopic.php?f=52&t=10
