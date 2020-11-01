

# HDMI_DDC/CI_KVM

## Introduction

This project aims to replicate the functionality of a KVM switch using the DDC/CI capability built into most monitors. 
This avoids the need for any complex switching of high frequency HDMI/DP signals so is cheap to implement and does not constrain the resolution or frequency of the monitor.
The additional important benefit is that most KVM switches do not handle EDID/hot plug detection correctly. This means that when the switch connects PC2 to the monitor(s), PC1 forgets that the monitors exist, and rearranges all the windows - annoying! This project avoids this problem because the monitors are all directly connected to each PC, so the monitor always provides EDID data and HPD signal.
It also serves as a demonstrator for communicating with monitors over DDC/CI.

## Status

<b>This project is currently dead because if a monitor is set to an input which is not active, most monitors go to sleep. From this sleep state, even when the input is changed using DDC/CI monitors often do not wake up.
I am not sure how monitors detect whether an input is active and if this could be fooled. I suspect it detects some of the high frequency HDMI signals. If these were replicated there wouldn't be much benefit over a normal KVM.
<h3>If anyone has a solution to this problem please get in touch and we can have cheap multi monitor KVMs!</h3>
If your monitors correctly switch away from idle inputs to active inputs, this project may work well.</b>

Normal KVMs should implement a DDC/CI/HPD scheme, where each PC always believes it is connected to its monitors at all times, this project shows that it isn't very difficult. Simply read EDID from the monitor, then repeat this every time the PC asks for the information. If anyone is aware of a dual monitor KVM that handles this correctly please let me know! :)


## Setup

The project was tested on an arduino uno, with vscode and platformio as the ide. The only external components required are: 4 resistors (i2c pullups), 2 resistors (HDMI 5v current limiting - should really be an IC), 5 push buttons and two female HDMI ports with only have 5v, GND, I2CDAT, I2CCLK signals connected. These are connected to a spare input on each monitor (so each monitor requires 3+ inputs).
See end of readme for schematic and diagram.
## Usage - Overview

The project implements 5 buttons, 1 'setup' button and 4 'mode' buttons.
See end of readme for flow diagram.
At idle, the 'mode' buttons can either select a preset mode, or can relate to each monitor and each button will cycle through each input for that monitor. You can switch between these two idle states by pressing the 'setup' button.
To set presets, simply hold setup, then within 1 second press the button you wish to set a preset on, wait 3 seconds. This will set the preset to the current state of the monitors.
There is also a settings menu which can be accessed by pressing 'setup' for 4 seconds.
You can configure :
    default (turn on) idle state (preset selection or input switching)
    default (turn on) monitor state (leave monitors as they are, change them to the inputs selected when the switch turned off, or any of the presets)
    input selection options (0: Automatic detection (order will likely be wrong), 1: Match mode order (likely in order but may miss extras), 2: Match mode order + autodetected (likely in order but may have extras), 3: Automatic detection (ordering will likely be wrong) - test ALL possible values) 
At any time in the menu, press 'setup' to exit the menu.

## Initial Setup and Normal Use

 1. Turn on for the first time with monitors connected and PCs turned on
 2. Press and hold 'setup' for 5 seconds, until the LED flashes in short single beat pulses - this is the settings menu
 3. Press Button 2 (the third mode button, the 4th button if you include the setup button) - this selects input setup
 4. Press Button 0 for automatic input detection - this will take a few seconds to cycle through every input on every monitor
3. Press 'setup' to return to idle
4. Press 'setup' again to switch to input selection state
5. Press Buttons 0 and 1, the monitors should cycle through their inputs
6. Press 'setup' then one of the mode buttons and hold for 5 seconds - this sets the current input configuration as that mode - repeat until all desired modes are set
1. Press 'setup' to enter mode selection state
1. Press any of the set mode buttons - the monitors should switch to that configuration!

## Full settings menu description
Pressing 'setup' at any time will return you to idle, not in settings menu.
Completing any operation in settings menu will return you to Main settings menu.
|Level 0|Level 1|Level 2|Level 3|
|--|--|--|--|
|Main|
||0. Change setup button behaviour||
|||0. Mode select|
||| 1. Input select|
||1. Change turn on behaviour||
|||0. Do not change|
|||1. As turn off state|
|||2. Select mode|
||||0,1,2,3. Respective mode is set|
||2. Configure monitor input switching||
|||0. Automatic detection|
|||1. Extract in order from preset modes|
|||2. Extract from modes then add any extras found during automatic detection|
||3. (Not fully implemented) Change serial debug levels||
|||0. No debug|
|||1. Errors only|
|||2. Warnings and errors|
|||3. All debug|

## Future if sleep issue is overcome
If the sleep problem could be fixed, the next phase was to implement HDMI passthrough. In this case, HDMI signals would be passed through so that no extra input is required. The only additional complexity is a gate between the HDMI input and output on the i2c lines, to enable the HDMI_DDC/CI_KVM to change the input (removing the PC master). This should be relatively simple, if PCs are able to notice this disconnect, it may be possible to hold the i2c clk line low until the input switching is complete, which should make the PC wait.

## References
|Name|Description/use|Link|
|--|--|--|
|DDC/CI VESA specification|Contains semi helpful descriptions of how DDC/CI i2c communication works - actual VCP codes are in appendix|https://milek7.pl/ddcbacklight/ddcci.pdf|
|MCCS (VCP) VESA specification|Contains VCP descriptions, layout of data|https://milek7.pl/ddcbacklight/mccs.pdf|
|ControlMyMonitor|Useful utility for using DDC/CI from windows - used to trigger communication which captured for reference|https://www.nirsoft.net/utils/control_my_monitor.html|
|ddcutil|Similar to ControlMyMonitor but linux only looks well maintained - documentation contains useful snippets|https://www.ddcutil.com/|

## Diagrams

![Schematic](blob/master/HDMISwitchHardware/basic_schem.png?raw=true "Schematic")
![Fritzing](blob/master/HDMISwitchHardware/basic_bb.png?raw=true "Fritzing")
![Flow diagram](blob/master/HDMISwitch/flow_diagram.png?raw=true "Flow diagram")
