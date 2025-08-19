# Hardware
## The TL:DR
* [Three P2, 5V, 128*64 pixel colour modules with a HUB75E interface from Ali Express](https://www.aliexpress.com/item/32913063042.html) (you can have more - up to three rows of more!)
* [A 1GB Raspberry Pi 4 from Pimoroni](https://shop.pimoroni.com/products/raspberry-pi-4?variant=31856486416467)
* Either [Adafruit RGB Matrix Bonnet for Raspberry Pi from Pimoroni](https://shop.pimoroni.com/products/adafruit-rgb-matrix-bonnet-for-raspberry-pi?variant=2257849155594)...
* ... or [Electrodragon 3-port RGB Matrix board for Raspberry Pi](https://www.electrodragon.com/product/rgb-matrix-panel-drive-board-raspberry-pi)
* A 5V power-supply capable of delivering at least 5 Amps

## Some RGB matrix boards.
I purchased [three P2, 5V, 128*64 pixel colour modules with a HUB75E interface from Ali Express](https://www.aliexpress.com/item/32913063042.html).

Three is a good size to start with. The matrix library I used ([hzeller RGB Matrix library](https://github.com/hzeller/rpi-rgb-led-matrix)supports more in each row and you can have up to three rows of panels.

There are a myriad sellers on Ali Express and elsewhere. I suspect there's little to differentiate between offerings.

## A Raspberry Pi
You could purchase [a 1GB Raspberry Pi 4 from Pimoroni](https://shop.pimoroni.com/products/raspberry-pi-4?variant=31856486416467) - I'm not plugging Pimoroni, it's just they also stock the Bonnet.

I used a Raspberry Pi 4 which was unloved and needed a new purpose. 

**Note** that the RGB matrix library doesn't yet work with a Pi 5. I'll update when it does as the increased power of the Pi 5 will be welcome!

## An RGB matrix-driver 
In the latest build I've used a 3-port driver - at $3 each they're hard to beat from [Electrodragon](https://www.electrodragon.com/product/rgb-matrix-panel-drive-board-raspberry-pi).
More detail on [Hzeller's adapter page](https://github.com/hzeller/rpi-rgb-led-matrix/tree/master/adapter)

This first version of the project was built with an [Adafruit RGB Matrix Bonnet for Raspberry Pi from Pimoroni](https://shop.pimoroni.com/products/adafruit-rgb-matrix-bonnet-for-raspberry-pi?variant=2257849155594)

Specification on the [Adafruit site](https://learn.adafruit.com/adafruit-rgb-matrix-bonnet-for-raspberry-pi).   

## 16 way ribbon cable
Most sellers provide these with the matrix panels. If you're making your own then your mantra has to be "Short Is Good".

## Some power!
I used a bench-top adjustable power supply to provide 5v and up to 5A for the matrix boards and the Raspberry Pi.

The nature of the panels means that current can vary quite wildly.  For the departure board with three panels it runs at about 1.5A to 2.0A.

I'd recommend getting something substantial as the "all pixels on" power is just around 3.5A with my configuration... and let's face it, you're going to want to play about with it!

The [Words about power](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/wiring.md#a-word-about-power) in the RGB Matrix documentation is worth a read.

## Anything else on hardware?
I'd highly recommend reading the detail on the [hzeller rpi-rgb-led-matrix - Let's Do It!](https://github.com/hzeller/rpi-rgb-led-matrix?tab=readme-ov-file#lets-do-it) documentation as it answers every question you can think of.
It's an awesome resource and fabulous software. More of that below!

## Putting it all together
Details below are for the Adafruit Bonnet

For other adapters see the [RGB Matrix - wiring](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/wiring.md) documentation.

### Improving Flicker ###
As highlighted [here](https://github.com/hzeller/rpi-rgb-led-matrix?tab=readme-ov-file#improving-flicker) in the RGB documentation, an optional change to connect pins 4 and 18 on the hat/bonnet.

I did this by soldering a row of pins to the bonnet and using a modified connector to make it easy to add/remove the modification.
![Picture of pins and connector](https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board/blob/main/Images/Bonnet_Jumpers.jpg)

### Address E pad modification ###
As highlighted [here](https://github.com/hzeller/rpi-rgb-led-matrix?tab=readme-ov-file#new-adafruit-rgb-matrix-hat-with-address-e-pads) in the RBG documentation, a change required for the size and type of panels I used.
![Picture of soldere blob connecting the two pads](https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board/blob/main/Images/Bonnet_Soldering.jpg)

As a side note, I had to chop some pins off a connector on the Pi as they hit the bonnet - I suspect I may regret that at some point when the Pi gets used for something else.

### Connecting to the panels ###

Only thing to watch for is to make sure that your ribbon cables are short and connect from 'Outputs' to 'Inputs' (should be labelled or have an arrow on the matrix boards

### Connecting the power ###

Noting the [advice about power](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/wiring.md#a-word-about-power) in the RGB Matrix documentation.

Connect to the panels and the 5v screw-connectors on the Adafruit bonnet to power to the Raspberry Pi.

As noted in the documentation, the Raspberry Pi doesn't have enough juice to power the panels.

### Joining the panels together ###

A chance to be creative!  

To get you started I've provided a very basic solution to 3D print joiners.

You can set the key parameters to create a joiner specific to your panels.

More detail in the [Joiner Directory](https://github.com/jonmorrissmith/RGB_Matrix_Train_Departure_Board/blob/main/Joiner/Readme.md)
