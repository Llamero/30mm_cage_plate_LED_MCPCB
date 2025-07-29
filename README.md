# 30 mm Cage Plate LED Driver
![Top view of LED driver](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/LED%20demo.gif)
## 4x LED 30 mm Cage Plate Board Description:
This board is an entirely self-contained LED and driver circuitry designed to be compatible with the Thorlabs 30 mm cage plate system: https://www.thorlabs.com/navigation.cfm?guide_id=2004  

![Top view of LED driver](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/4x%20LED%20board%20front.png)

The LEDs on the board are closely spaced so that all four LEDs efficiently couple into a 5 mm liquid light guide or other optical paths.

![Side view of LED with opticas](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/4x%20LED%20board%20with%20optics.jpg)

Each board can drive up to four individual LEDs, and includes a thermistor for temperature monitoring.  The the LED current is regulated by an [AL8843](https://www.diodes.com/part/view/AL8843), allowing LEDs to be driven with up to three amps of current.  The [AL8843](https://www.diodes.com/part/view/AL8843) also allows for both PWM control as well as analog current control down to 10% of the current limit (see below for setting max current limit).

Four p-channel mosfets on the board create a 1x4 multiplexer that allows individual LEDs to be toggled on by pulling the corresponding LED channel's gate to ground.  Each gate has a 4.7 kOhm pullup resistor so that each channel defaults to being off if not connected.

To turn the LEDs off more rapidly, there is also a 38V TVS diode parallel to the inductor that can be used to dump the residual energy stored in the inductor by setting all four multiplexer channels high.

A heatsink and fan can also be mounted to the backside of the cage plate for active cooling of high-current LEDs.

![Side view of LED driver](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/LED%20board%20side.png)
## 3x 30 mm Cage Plate Controller Description:
This repository also includes the design files for a controller that can operate up to three 30 mm cage plates for a total of 12 LEDs.

![LED controller top](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/LED%20controller%20top.jpg)

The controller is powered by a Teensy 4.1 which allows for precise timing control (<10 ns hysteresis) of each LED, as well a complex programmable control.  

On the back of the driver, each board connects to the driver via a shielded cat5 ethernet cable.  Additionally, there is a cat5 port with both a 12V supply and three channels of 5V PWM outputs for both powering the fans for each board as well as controlling the fan speed.  Finally, there are four protected digital inputs allowing for control signals up to -15V/+18V.

On the front of the driver, there is a potentiometer for controlling the intensity of each of the three LED boards, as well as a pushbutton to select the channel for each board.  Additionally, each pushbutton includes a red-green LED indicator to indicate which of the four LED channels is active.

## Driver Software:
There is software prewritten for the Teensy 4.1 that allows for both manual and programmable control, for syncing to triggers or other inputs.

Additionally, there is a GUI that allows for software control of the LED driver, we well as access to five preset programmable sync modes (digital, analog, confocal, serial, and custom), as well as custom configuration of the LED driver.

![GUI](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/GUI1.PNG)
## Design:
To view and modify the board, download KiCAD https://www.kicad.org/, and open the project file.

Make sure to modify the LED footprint to match the footprint for your desired LEDs, and to change the current limit resistor to the desired value (see [AL8843](https://www.diodes.com/part/view/AL8843) datasheet, p. 11):
```
RSET (Ω)  Nominal Average Output Current (mA)
0.033     3,000
0.05      2,000
0.067     1,500
0.1       1,000
0.15      667
0.3       333
```
When changing the LED footprints, ensure that the LEDs remain closely spaced for efficient coupling into a liquid light guide.

## Ordering:
The BOM can be found here: ![30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/BOM.xlsx](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/BOM.xlsx)

This BOM includes all the parts needed to build both the LED driver boards as well as the LED controller board.

To order the PCBs, upload the Gerber files to your desired PCB vendor, such as: [https://www.pcbway.com/project/shareproject/W55957ASK98_30_mm_Cage_Plate_LED_MCPCB_kicad_pcb_3d2c1975.html](https://www.pcbway.com/project/shareproject/W55957ASK98_30_mm_Cage_Plate_LED_MCPCB_kicad_pcb_3d2c1975.html)  Make sure that the LED boards are MCPCBs and not FR4.  The driver board can be ordered as an FR4 board.

## Assembly:
To start, spread [thermal epoxy](https://www.digikey.com/en/products/detail/mg-chemicals/8329TCM-6ML/6035511) onto the flat side of a [Thorlabs 30 mm blank cage plate](https://www.thorlabs.com/thorproduct.cfm?partnumber=CP31).

![Spread epoxy](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/Spread%20thermal%20epoxy%20on%20cage%20plate.jpg)

Press the MCPCB board firmly to the cage plate using a vise or heavy weights.
![vise](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/Vise%20to%20press%20board%20to%20plate.jpg)

Use a pipette tip or other fine tipped tool to remove excess epoxy from the cage rod holes.
![Remove excess epoxy](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/Use%20pipette%20to%20remove%20excess%20epoxy.jpg)

Slide the board across the cage plate as needed to align the cage rod holes and allow the epoxy to cure overnight.

Next, assemble the board similarly to other reflow soldering boards. See the following manual: https://github.com/Llamero/Light_Color_and_Intensity_Datalogger/blob/master/Data%20logger%20assembly%20manual.pdf

Finally, use thermal epoxy to connect the heatsink to the Thorlabs cage plate, press firmly, and bake at 50°C overnight to cure.

![heatsink](https://github.com/Llamero/30mm_cage_plate_LED_MCPCB/blob/4x-LED-array/Images/Back%20(no%20fan).jpg)

## Operation:
Connect a 5V to 20V power supply, with the positive terminal connected to the Vin pin and the negative terminal connected to the Gnd pin. To control the LED current, you can send a +0.4V (10% current) to +2.5V (100% current) to the Ctrl pin. The LED can also be dimmed with PWM with a voltage up to 5V.  To control the mux, pull the channel you want to turn on to ground (0V).  To monitor temperature, measure the resistance of the thermistor.
