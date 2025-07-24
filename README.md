## Current Status
**Second Beta Release** Some fixes deployed and additional functions added. Testing and feedback welcome!

# RPi_DepartureBoard
A Raspberry Pi powered HUB75-Matrix real-time UK train departure board with UI. Uses Staff Data for additional functionality. Supports up to 3x3 matrix panels.

A way to build your own HUB75 RGB matrix train departure board powered by a Raspberry Pi.

Configurable via a light-weight web-page or the command line.

You can select:
* All trains from a station
* All trains from a station on a specified platform
* Whether to show Network Rail alert message.
* The frequency of display update
* Frequency of toggle between 2nd and 3rd departure
* Many other options...
  
Default display:
* The next train with calling points, operator and formation
* Any delay-related messages and cancellation reasons
* Location of the next service
* The following two departures - destination and departure times
  
Options to display
* Station the departure board is configured for
* Departure platform
* Estinated departure time for calling points
* Any National Rail messages for the station

Components are a Raspberry Pi 4, a matrix-adapter (Adafruit RGB matrix bonnet), some RGB matrices and a power-supply.

There are some limitations with using this hardware which I suspect don't apply to commercially-available units.

However for the cost it's a great result!

# Motivation
Being a life-long train fan I've always wanted my own departure board, and as a regular commuter one which shows my usual route.

Over the years I've looked at flip-dot displays and various other types and saw LED dot-matrix displays become available for purchase.

These look amazing... however the price is offputting, as is the monthly-subscription some require.

My thought -  _"How hard can it be"_.

And here we are.

# Hardware

*I'll add a section on this - in the meantime see the detail on the [Original Raspberry Pi Departure Board repo](https://github.com/jonmorrissmith/RpiTrainDisplay/blob/main/README.md#hardware).*

# Setting up the Raspberry Pi #

## Install the OS ##

There are more tutorials than you can shake a stick at on how to install an OS on a Raspberry Pi.

Use the 'OS Lite (64bit)' to maximise CPU cycles the matrix driver can use. Set up ssh and Wifi in the Raspberry Pi Imager tool.

Once installed there was the usual upgrade/update and disable/uninstall anything unecessary.

I would strongly recommend following [using a minimal raspbian distribution](https://github.com/hzeller/rpi-rgb-led-matrix?tab=readme-ov-file#use-minimal-raspbian-distribution) for more detail.

What is described what I did in March 2025. Detail in the rpi-rgb-matrix repository will be the most recent recommendations.

* Set `dtparam=audio=off` in `/boot/firmware/config.txt`
* Add `isolcpus=3` **to the end** of the parameters in `/boot/firmware/cmdline.txt` to isolate a CPU.
* Example - `console=serial0,115200 console=tty1 root=PARTUUID=9f23843a-02 rootfstype=ext4 fsck.repair=yes rootwait cfg80211.ieee80211_regdom=GB isolcpus=3`
* Remove unecessary services with `sudo apt-get remove bluez bluez-firmware pi-bluetooth triggerhappy pigpio`

run `lsmod` and to check for the snd_bcm2835 module.

Example:
```
$lsmod | grep snd_bcm2835
snd_bcm2835            24576  0
snd_pcm               139264  5 snd_bcm2835,snd_soc_hdmi_codec,snd_compress,snd_soc_core,snd_pcm_dmaengine
snd                   110592  6 snd_bcm2835,snd_soc_hdmi_codec,snd_timer,snd_compress,snd_soc_core,snd_pcm
```
If it's there then blacklist it:
```
cat <<EOF | sudo tee /etc/modprobe.d/blacklist-rgb-matrix.conf
blacklist snd_bcm2835
EOF

sudo update-initramfs -u
```
Then reboot - `sudo reboot` to get a nice clean and sparkly install.

**Note** At the rsk of stating the obvious, when adding options to `cmdline.txt` put them on the same line as the existing arguments. No newlines allowed.

## Installing packages you'll need
In no particular order:
* Git 
* JSON for modern C++ 
* curl (should be there already) 
* curl C++ wrappers 
```
sudo apt install git
sudo apt install nlohmann-json3-dev
sudo apt install libcurl4
sudo apt install libcurlpp-dev

```

## Install the RGB Matrix Software ##
Take a clone of the RPI RGB Matrix repository - `git clone https://github.com/hzeller/rpi-rgb-led-matrix`.

Compile the library
```
cd rpi-rgb-matrix
make
```
A pre-emptive read of the [Troubleshooting section](https://github.com/hzeller/rpi-rgb-led-matrix?tab=readme-ov-file#troubleshooting) will help you get ahead of issues.

Enjoy the demos in the `examples-api-use` directory.
If you've got the configuration described here then you can start with this:
```
sudo ./demo -D9 --led-rows=64 --led-cols=128 --led-chain=3 --led-gpio-mapping=adafruit-hat
```
Have fun!

# Subscribe to the Rail Data Marketplace Data Feeds #
You'll need to register for an account - do this as an individual rather than a company.

Once you've got an account you'll need to subscribe to two data-sets:
* Staff Data
* Reference Data

Find these via the [Data Products Catalogue](https://raildata.org.uk/dashboard/dataProducts).  

## Live Departure Board - Staff Version ##

Search for **Live Departure Board - Staff Version** - and go to the Specification tab on the page

Click on `/LDBSVWS/api/20220120/GetArrDepBoardWithDetails/{crs}/{time}` - this is the departure data we're using.

Copy the **Consumer Key** and save it to put in your config file.

## Reference Data ##

Search for **Reference Data** - and go to the Specification tab on the page.

Note that there are a number of Reference Data sources available - the one you want is just called 'Reference Data'.

Click on `/LDBSVWS/api/ref/20211101/GetReasonCodeList` - this is the delay/cancellation reason data we're using.

Copy the **Consumer Key** and save it to put in your config file.

# The final Steps #

## Installing the RGB Matrix Train Departure Board software ##
Download and build from this repository
```
git clone https://github.com/jonmorrissmith/RPi_DepartureBoard
cd RPi_DepartureBoard
chmod 755 setup.sh
./setup.sh
```
This will build the software, configure the UI, create config files and ensure permissions are correctly set on directories.

### And finally Cyril... and finally Esther ###
Start the UI server
```
./run.sh
```
You should see:
```
Starting server on port 80...
```
Which means you can go to `http://<IP address of your Raspberry Pi>` and start using your display!

# First Time Use - Basic Configuration
Set the following in the UI

## Debug Settings (as this is a Beta version)
```
debug_mode      \\ true/false for on/off
debug_log_dir   \\ where to store the debug logs and JSON output from the API
```

## Location and Destination
```
location   \\ The CRS code for the station whose departures you want to show
platform   \\ Leave blank for all platforms or populate for a specific platform
```
## Additional Information
```
ShowCallingPointETD   \\ If set to Yes will display departure times after each calling point
ShowMessages          \\ If set to Yes will display Network Rail message for your departure station
ShowPlatforms         \\ If set to Yes will display the platform for the departures
ShowLocation          \\ If set ('from') will display at the bottom (alternate with Messages)
```
## API and Font Configuration
```
StaffAPIKey           \\ The Staff Departure Board key from Rail Data Market Place
DelayCancelAPIKey     \\ The Delay/Cancellation Reference Data key from Rail Data Market Place
```
## Font configuration
```
fontPath  \\ Path to fonts - you can use the matrix package (/home/<your username>/rpi-rgb-led-matrix/fonts/7x14.bdf)
```
## Timing Configuration
```
calling_point_slowdown       \\ Lower the number, the faster the calling-points scroll
nrcc_message_slowdown        \\ Lower the number, the faster the Network Rail messages scroll
refresh_interval_seconds     \\ How often the API is called to refresh the train data
third_line_refresh_seconds   \\ How often the third line switches between 2nd and 3rd departure
Message_Refresh_interval     \\ How often any Network Rail messages are shown
ETD_coach_refresh_seconds    \\ How often the top right switches between ETD and number of coaches
third_line_scroll_in         \\ If true 2nd/3rd departures scroll in rapidly from the right when they change
```

## Hardware Configuration
```
matrixcols=128           \\ Number of columns in an LED matrix panel
matrixrows=64            \\ Number of rows in an LED matrix panel
matrixchain_length=3     \\ Number of panels you've got chained together
matrixparallel=1         \\ Number of chains you've got running in parallel
matrixhardware_mapping   \\ See hzeller documentation - set to adafruit-hat-pwm for the Adafruit bonnet
gpio_slowdown=2          \\ Sometimes the Pi is too fast for the matrix.  Fiddle with this to get the right setting.
```

## Display layout configuration (vertical positions)
```
first_line_y     \\ pixel-row for the first line of text
second_line_y    \\ pixel-row for the second line of text
third_line_y     \\ pixel-row for the third line of text
fourth_line_y    \\ pixel-row for the fourth line of text
```

## Once you're happy with your configuration
Scroll to the bottom and click on **Save and Restart**.

This saves your configuration (to 'config.txt') and (re)starts the display.

You can **Save as Default** and **Rest to Default** to allow you to revert changes.

# Advanced Configuration
It's unlikely that you'll need to change these, but they're available if you need to.

Detail of the parameters is available [in the RGB Matrix documentation](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/README.md#changing-parameters-via-command-line-flags).
```
led-multiplexing
led-pixel-mapper
led-pwm-bits
led-brightness
led-scan-mode
led-row-addr-type
led-show-refresh
led-limit-refresh
led-inverse
led-rgb-sequence
led-pwm-lsb-nanoseconds
led-pwm-dither-bits
led-no-hardware-pulse
led-panel-type
led-daemon
led-no-drop-privs
led-drop-priv-user
led-drop-priv-group
```

# Additional Information

## Making output less verbose

Simply remove the `-d` option from the `Executable_command_line` line in `ui-config.txt`

## Running on another port

Change the port in `ui-config.txt`.

Note that the executable needs root to access the RGB matrix.

## Setting your configuration in the code 

You can edit `config.h` to hard-code values for parameters which are in `config.txt`

If you do this then it's easiest to run
```
make clean
make all
```
which will recompile the executable with your defaults.

## Command Line Operation ##

Three options which also support a '-d' option for debugging information

Use the hard-coded configuration in the executable

`sudo ./traindisplay`

Use the hard-coded configuration in the executable and specify origin

`sudo ./traindisplay KGX`

Use the configuration file

`sudo ./traindisplay -f <config file>`

Combination of the above

`sudo ./traindisplay KGX -f <config file> -d`

# Troubleshooting #

Happy to help - drop me a line via github!

**Helpful Links for debugging RGB Matrix Issues**
* [Changing parameters](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/README.md#changing-parameters-via-command-line-flags)
* [Troubleshooting](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/README.md#troubleshooting)































