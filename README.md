## Welcome NotFastEnuf's RACEMODE in Betaflight project
## RACEMODE is a complete rewrite and replacement of the Horizon Flight Mode in betaflight intended for whoop racers.  The basic recipe for racemode is leveling assistance on the roll axis and pure acro (rate mode) behavior on the pitch axis. To activate racemode, flash a RACEMODE_Betaflight build and activate Horizon flight mode via switch as configured on the modes tab.

*There are 2 types of RACEMODE flight behavior selectable via CLI*

**RACEMODEangle** is a flight mode that will limit your roll axis to the max angle limit value set on the pids tab.  The leveling strength below max angle limit is adjustable by changing the horizon strength variable. Within angles below max angle limit, roll axis (only) will feel & behave exactly like angle mode does.  Because it will be possible to reach angles beyond max angle limit by pitching to inverted orientations - inverted behavior on the roll axis consists of 2 zones. In between 180 degrees inverted and max angle limit, there is a user set angle via CLI called racemode_tilt_effect.  Between 180 degrees inverted roll and racemode_tilt_effect value - behavior is pure acro (rate mode).  Passing racemode_tilt_effect angle will start to fade in leveling behaviour gradually till the craft reaches max angle limit and pops back into restricted angle mode type behavior. Default value for racemode_tilt_effect is 85 degrees and is suggested to be at least 20 degrees above max angle limit so a decent transition zone exists between acro and leveled zones.  RACEMODEangle is the default type of racemode and corresponds to CLI variable set racemode_horizon = OFF

**RACEMODEhorizon** is an unrestricted flight mode on roll axis and can be activated to replace RACEMODEangle on CLI with the command set racemode_horizon = ON.  There are 3 configurable zones of flight behavior for the roll axis.  Zone 1 has full leveled strength from a neutral orientation up to an angle set by the horizon transition value on the pids tab.  Zone 2 spans from the angle set by the horizon transition value (on pids tab) to the angle set on CLI with the racemode_tilt_effect value. Within zone 2 as the craft's roll angle of orientation increases, leveling strength is gradually reduced and is mixed in with a gradually increasing amount of acro (rate mode) behavior.   Once the racemode_tilt_effect angle is reached and beyond, behavior is pure acro with no leveling for zone 3.  Default value for horizon transition is set to 0 degrees effectively eliminating zone 1 for a more familiar but improved upon "horizon type feel".  Increase horizon transition to open zone 1's more aggressive leveling range if desired.  Default value for racemode_tilt_effect is set to 85 degrees on CLI - this is where all leveling stops.  Setting this variable to 0 or below horizon transition will turn off all leveling and setting it above 180 degrees will be constrained to a value of 180 degrees in firmware. 


Join discussion of RACEMODE on betaflight, or reach out to request your fc target be compiled and posted for download here:  insert mmc link here

Download your RACEMODE target here: https://github.com/NotFastEnuf/betaflight/releases/tag/v3.2.4

-NotFastEnuf




![BetaFlight Notice, version 3.2 will be the last version of Betaflight to support STM32F1 based flight controllers, this includes NAZE, CC3D (original) and CJMCU like flight controllers](https://raw.githubusercontent.com/wiki/betaflight/betaflight/images/betaflight/bf3_2_notice.png)

![BetaFlight](https://raw.githubusercontent.com/wiki/betaflight/betaflight/images/betaflight/bf_logo.png)

Betaflight is flight controller software (firmware) used to fly multi-rotor craft and fixed wing craft.

This fork differs from Baseflight and Cleanflight in that it focuses on flight performance, leading-edge feature additions, and wide target support.

## Events

| Date  | Event |
| - | - |
| 01 February 2018 | Start of feature freeze / Release Candidate window for Betaflight 3.3 |
| 01 March 2018 | Planned [release](https://github.com/betaflight/betaflight/milestone/6) date for Betaflight 3.3 |

## Features

Betaflight has the following features:

* Multi-color RGB LED strip support (each LED can be a different color using variable length WS2811 Addressable RGB strips - use for Orientation Indicators, Low Battery Warning, Flight Mode Status, Initialization Troubleshooting, etc)
* DShot (150, 300, 600 and 1200), Multishot, and Oneshot (125 and 42) motor protocol support
* Blackbox flight recorder logging (to onboard flash or external microSD card where equipped)
* Support for targets that use the STM32 F7, F4, F3 and F1 processors
* PWM, PPM, and Serial (SBus, SumH, SumD, Spektrum 1024/2048, XBus, etc) RX connection with failsafe detection
* Multiple telemetry protocols (CSRF, FrSky, HoTT smart-port, MSP, etc)
* RSSI via ADC - Uses ADC to read PWM RSSI signals, tested with FrSky D4R-II, X8R, X4R-SB, & XSR
* OSD support & configuration without needing third-party OSD software/firmware/comm devices
* OLED Displays - Display information on: Battery voltage/current/mAh, profile, rate profile, mode, version, sensors, etc
* In-flight manual PID tuning and rate adjustment
* Rate profiles and in-flight selection of them
* Configurable serial ports for Serial RX, Telemetry, ESC telemetry, MSP, GPS, OSD, Sonar, etc - Use most devices on any port, softserial included
* VTX support for Unify Pro and IRC Tramp
* and MUCH, MUCH more.

## Installation & Documentation

See: https://github.com/betaflight/betaflight/wiki

## IRC Support and Developers Channel

There's a dedicated Slack chat channel here:

http://www.betaflight.ch/

Etiquette: Don't ask to ask and please wait around long enough for a reply - sometimes people are out flying, asleep or at work and can't answer immediately.

## Configuration Tool

To configure Betaflight you should use the Betaflight-configurator GUI tool (Windows/OSX/Linux) that can be found here:

https://chrome.google.com/webstore/detail/betaflight-configurator/kdaghagfopacdngbohiknlhcocjccjao

The source for it is here:

https://github.com/betaflight/betaflight-configurator

## Contributing

Contributions are welcome and encouraged.  You can contribute in many ways:

* Documentation updates and corrections.
* How-To guides - received help? Help others!
* Bug reporting & fixes.
* New feature ideas & suggestions.

The best place to start is the IRC channel on gitter (see above), drop in, say hi. Next place is the github issue tracker:

https://github.com/betaflight/betaflight/issues
https://github.com/betaflight/betaflight-configurator/issues

Before creating new issues please check to see if there is an existing one, search first otherwise you waste peoples time when they could be coding instead!

## Developers

Please refer to the development section in the `docs/development` folder.

TravisCI is used to run automatic builds

https://travis-ci.org/betaflight/betaflight

[![Build Status](https://travis-ci.org/betaflight/betaflight.svg?branch=master)](https://travis-ci.org/betaflight/betaflight)

## Betaflight Releases

https://github.com/betaflight/betaflight/releases

## Open Source / Contributors

Betaflight is software that is **open source** and is available free of charge without warranty to all users.

Betaflight is forked from Cleanflight, so thanks goes to all those whom have contributed to Cleanflight and its origins.

Origins for this fork (Thanks!):
* **Alexinparis** (for MultiWii),
* **timecop** (for Baseflight),
* **Dominic Clifton** (for Cleanflight), and
* **Sambas** (for the original STM32F4 port).

The Betaflight Configurator is forked from Cleanflight Configurator and its origins.

Origins for Betaflight Configurator:
* **Dominic Clifton** (for Cleanflight configurator), and
* **ctn** (for the original Configurator).

Big thanks to current and past contributors:
* Budden, Martin (martinbudden)
* Bardwell, Joshua (joshuabardwell)
* Blackman, Jason (blckmn)
* ctzsnooze
* Höglund, Anders (andershoglund)
* Ledvina, Petr (ledvinap) - **IO code awesomeness!**
* kc10kevin
* Keeble, Gary (MadmanK)
* Keller, Michael (mikeller) - **Configurator brilliance**
* Kravcov, Albert (skaman82) - **Configurator brilliance**
* MJ666
* Nathan (nathantsoi)
* ravnav
* sambas - **bringing us the F4**
* savaga
* Stålheim, Anton (KiteAnton)

And many many others who haven't been mentioned....
