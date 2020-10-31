# HDMI_DDC/CI_KVM

This project aims to replicate the functionality of a KVM switch using the DDC/CI capability built into most monitors. 
This avoids the need for any complex switching of high frequency HDMI/DP signals so is cheap to implement and does not constrain the resolution or frequency of the monitor.
The additional important benefit is that most KVM switches do not handle EDID/hot plug detection correctly. This means that when the switch connects PC2 to the monitor(s), PC1 forgets that the monitors exist, and rearranges all the windows - annoying! This project avoids this problem because the monitors are all directly connected to each PC, so the monitor always provides EDID data and HPD signal.

The project was tested on an arduino uno, with vscode and platformio as the ide. The only external components required are: 4 resistors (i2c pullups), 2 resistors (HDMI 5v current limiting - should really be an IC), 5 push buttons.

This project is currently dead because if a monitor is set to an input which is not active, most monitors go to sleep. From this sleep state, even when the input is changed using DDC/CI monitors often do not wake up.
I am not sure how monitors detect whether an input is active and if this could be fooled. I suspect it detects some of the high frequency HDMI signals. If these were replicated there wouldn't be much benefit over a normal KVM.
If anyone has a solution to this problem please get in touch and we can have cheap multi monitor KVMs!
If your monitors correctly switch away from idle inputs to active inputs, this project may work well.

Normal KVMs should implement a DDC/CI/HPD scheme, where each PC always believes it is connected to its monitors at all times, this project shows that it isn't very difficult. Simply read EDID from the monitor, then repeat this every time the PC asks for the information. If anyone is aware of a dual monitor KVM that handles this correctly please let me know! :)



The project implements 5 buttons, 1 'setup' button and 4 'mode' buttons.
At idle, the 'mode' buttons can either select a preset mode, or can relate to each monitor and each button will cycle through each input for that monitor. You can switch between these two idle states by pressing the 'setup' button.
To set presets, simply hold setup, then within 1 second press the button you wish to set a preset on, wait 3 seconds. This will set the preset to the current state of the monitors.
There is also a settings menu which can be accessed by pressing 'setup' for 4 seconds.
You can configure :
    default (turn on) idle state (preset selection or input switching)
    default (turn on) monitor state (leave monitors as they are, change them to the inputs selected when the switch turned off, or any of the presets)
    input selection options (0: Automatic detection (order will likely be wrong), 1: Match mode order (likely in order but may miss extras), 2: Match mode order + autodetected (likely in order but may have extras), 3: Automatic detection (ordering will likely be wrong) - test ALL possible values) 
At any time in the menu, press 'setup' to exit the menu.

The current version of this project uses two female HDMI connectors which only have 5v, GND, i2cDat, i2cClk signals connected. These are connected to a spare input on each monitor (so monitor requires 3+ inputs).
If the sleep problem could be fixed, the next phase was to implement HDMI passthrough. In this case, HDMI signals would be passed through so that no extra input is required. The only additional complexity is a gate between the HDMI input and output on the i2c lines, to enable the HDMI_DDC/CI_KVM to change the input (removing the PC master). This should be relatively simple, if PCs are able to notice this disconnect, it may be possible to hold the i2c clk line low until the input switching is complete, which should make the PC wait.

