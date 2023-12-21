# 30 mm Cage Plate LED Driver
![Top view of LED driver](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/main/Images/Top.jpg)
## Description:
This board is an entirely self-contained LED and driver circuitry designed to be compatible with the Thorlabs 30 mm cage plate system: https://www.thorlabs.com/navigation.cfm?guide_id=2004

## Design:
To view and modify the board, download KiCAD https://www.kicad.org/, and open the project file.

Make sure to modify the LED footprint to match the footprint for your desired LED, and to change the current limit resistor to the desired value (see AL8843Q datasheet, p. 11):

RSET (Ω) Nominal Average Output Current (mA)
0.033 3,000
0.05 2,000
0.067 1,500
0.1 1,000
0.15 667
0.3 333

## Ordering:
The BOM can be found here: https://docs.google.com/spreadsheets/d/1ZUOHqLCISnd-vsBcDY3z2q5rFzEznOQzioUf6a6fGwg/edit?usp=sharing

To order the PCBs, upload the Gerber files to your desired PCB vendor, such as: https://www.pcbway.com/project/shareproject/30_mm_Cage_Plate_LED_Driver_8c990405.html

## Assembly:
This board is assembled similarly to other reflow soldering boards. See the following manual: https://github.com/Llamero/Light_Color_and_Intensity_Datalogger/blob/master/Data%20logger%20assembly%20manual.pdf

Once the components are soldered on, use thermal epoxy to connect the PCB board and the heatsink to the Thorlabs cage plate. It is helpful to use cage rods as an alignment guide to ensure everything it positioned correctly:
![LED driver with cage rods](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/main/Images/Top%20with%20rods.jpg)
Press firmly and bake at 50°C overnight to cure.

## Operation:
Connect a 5V to 40V power supply, with the positive terminal connected to the Vin pin and the negative terminal connected to the Gnd pin. To control the LED current, you can send a +0.4V (10% current) to +2.5V (100% current) to the Ctrl pin. The LED can also be dimmed with PWM with a voltage up to 5V.
