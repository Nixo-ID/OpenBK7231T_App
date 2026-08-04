# This fork (lab notice)

**Not a general multi-device OpenBeken product.**

This repository is a fork of [openshwprojects/OpenBK7231T_App](https://github.com/openshwprojects/OpenBK7231T_App).  
The working branch **`patch/bk7238-spidma-timeout`** is maintained for **one fixture only**:

## Target product

- **Brand:** Lumary  
- **Product:** Smart recessed light with gradient / auxiliary (ring) light — **8″** class  
- **OEM page:** https://www.lumarysmart.com/collections/smart-recessed-lights/products/lumary-smart-recessed-light-with-gradient-auxiliary-light?variant=41920423166157  
- **Lab model tags:** L-SD8E-1 V2  
- **Module / SoC:** Tuya **T1-U-HL** · **Beken BK7238**

## Hardware map (this light only)

| Pin | Role | Function |
|-----|------|----------|
| P9 | PWM_ScriptOnly ch1 | Warm white |
| P24 | PWM_ScriptOnly ch2 | Cold white |
| P16 | SM16703P_DIN | RGB ring data, **62** LEDs, RGB order |

Main features on the lab branch: SPI DMA multi-`SM16703P_Start` fix, on-device Notify/Ambient ring animations, Main/Halo commands, strip clear on restore, **BK7238 RF partition at stock Tuya `0x1E3000`** (factory MAC+cal; avoids shared fallback MAC `c8:47:8c:42:88:48`).

Lab notes, commissioning scripts, and HA drafts live in the private project repo **Cloudcutter-Lumary8** (not in this tree).

Upstream README below / in `README.md` still describes multi-platform OpenBeken; **do not treat this fork as tested on other devices.**
