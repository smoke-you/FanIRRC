# Replacement IR Remote Control Hardware and Firmware

## Background

I felt the need to design and build a custom IR remote control because:

* My young dog decided that he’d chew my AM02 fan remote control, almost to the point that it no longer worked, and
* It’s been a while since I did any embedded systems development for work, and I’m bored with my work at the moment: authoring webservers and microservices with Python.  It’s not very challenging, a bit like assembling software Lego.

![damaged original](/Images/damaged_original.png)

## License

Per [the licence file supplied with the ST source code](/Firmware/Libraries/LICENSE.md), the following files and directories in this project are licensed by ST Microelectronics under the BSD-3 Clause:

* [/Firmware/Libraries](/Firmware/Libraries)
* [/Firmware/startup](/Firmware/startup)
* [/Firmware/Debug_STM32L011K4_FLASH.ld](/Firmware/Debug_STM32L011K4_FLASH.ld)
* [/Firmware/Release_STM32L011K4_FLASH.ld](/Firmware/Release_STM32L011K4_FLASH.ld)

I am also releasing the remainder of the project under [the BSD-3 Clause](/LICENSE.md).

## Design

### Top-Down, Bottom-Up

What do I need in order to design this device?

* An understanding of the IR protocol emitted by the fan remote control.
* An ultra-low power microcontroller, to wake up on button press; emit an IR signal using the above protocol; then go back to sleep.

### Understanding the IR Protocol

I hooked up an IR photo-transistor to my logic analyser, pointed the battered remains of the remote at it, and started pressing buttons.

It turns out that the remote control protocol is very simple.  A button press triggers the transmission of two identical frames, separated by about 103ms (the “Inter-frame Gap”, or IFG).  Each frame consists of a “Start of Frame” (SOF) symbol and 16 data symbols.  SOF and data symbols are each made up of two or three bit periods, each of which is ~775µs duration.  Bit periods may be active (transmitting) or idle (not transmitting).

| Symbol | Description |
| ------ | ----------- |
| SOF | 3 active bit periods |
| '0' bit | 1 idle bit period followed by 1 active bit period |
| '1' bit | 2 idle bit periods followed by 1 active bit period |
| IFG | 134 idle bit periods |

Note that this use of different active/idle durations to delineate the various symbols is quite typical for IR signalling.  It is also analogous to Morse code.

The data bits in a frame can be represented as a 16-bit word, encoded most-significant bit (MSB) first.  There are four buttons on the remote control (power toggle, speed down, speed up, rotate toggle), and four corresponding commands.  A command is transmitted as a static value, plus a 2-bit counter (value 0 – 3).  The purpose of the counter appears to be to allow clear differentiation between two separate transmissions of the same command, e.g. to ensure that a Power Toggle command is treated at the receiver as “ON” with one counter value, and “OFF” with the next.

| Command | Value |
| ------- | ----- |
| Power Toggle | 0x5000 |
| Speed Down | 0x50fA |
| Speed Up | 0x5054 |
| Rotate Toggle | 0x50A8 |

For example, a Power Toggle command with counter value 3 will be transmitted as 0x5003, or `0b0101000000000011`.  Including the SOF, this translates to a bitstream (MSB first) of `0b111010010100101010101010101010101001001`.

![Logic analyser bitstream](/Images/LA_bitstream.png)

Note also that during an “active” bit period, the IR is not simply turned on; it is modulated with a carrier frequency of approximately 37.4kHz.  This is an example of on-off keying (OOK) modulation, i.e. the carrier is emitted when the signal is active, and turned off when the signal is inactive.

![Logic analyser modulation](/Images/LA_modulation.png)

The exact carrier frequency is probably not terribly important; I suspect that the receiver is sensitive to a modulation frequency range of at least 36-40kHz, and it may well be wider.

### Ultra-Low-Power Microcontroller

I’ve done a fair bit with STM32F4’s, both professionally and academically, so I was conscious that a Cortex-M4 device clocked at ~180MHz is not the optimal choice for a low-power remote control.  Sticking with ST’s devices, the STM32L0 range seemed to fit the bill, so I picked the least capable one that I could find: the STM32L011K4, with 16K Flash, 2K RAM, up to 32MHz, in a 7x7mm LQFP32 package; and with an inexpensive Nucleo development board available; and started by verifying that it had the peripherals and other capabilities that I expected to need.

Minimal power consumption was my top priority.  I was planning to run the remote control off a CR2032 coin cell with a nominal capacity of ~200mAh, like the manufacturer’s remote, and I wanted it to last for more than a day (preferably, years) between battery changes, so I needed an average operating current in the low micro-amps.  For example, I would expect 3-4 years of operating life from a CR2032 given an average current consumption of 5µA.

The STM32L011, as an explicitly low-power microcontroller, supports a wide range of clock sources and frequencies; a number of low-power modes; and is able to switches between them quickly and on demand.  The most appealing low-power mode for my purposes was STOP mode: Flash can be powered off; RAM contents are retained; and the system clock is stopped so the CPU is completely halted.  Operating current at 3V and 25ºC is rated as typically ~0.5µA, which was ideal.  Further, both component cost and runtime power consumption could be minimized by using the integrated medium-speed internal oscillator (MSI), which gives a typical RUN-mode current consumption of about 70µA at a (nominal) frequency of 256kHz (actually about 2<sup>18</sup> Hz, with a nominal error of ±0.5% and a worst-case error of perhaps ±8%).  Given the timing flexibility of the IR protocol, I did not expect the MSI’s imprecision to be problematic.

There are (at least) two peripherals that are often found in the STM32 range that I find particularly useful for communication protocols: a range of highly flexible and interconnected hardware timers; and Direct Memory Access (DMA) controllers that can be triggered by those timers.  These features allow complex communication signalling to be largely devolved to hardware, removing the signal timing determinism issues that software implementations tend to suffer from: the software configures the hardware, starts it, and lets it free-run to completion, as flagged by a DMA “transfer complete” interrupt.

Chaining timers together allows an OOK-modulated bitstream to be synthesized directly in hardware: one timer is configured as the slave and produces the modulation carrier signal with pulse-width modulation (PWM); and a second timer is configured as the master, and produces the data signal that is used to gate the slaved timer output off and on, again using PWM.  The PWM signal produced by the slave has constant frequency (37.4kHz, in this case) and duty cycle (50%, typically), while the PWM signal produced by the master must be continuously varied to generate the bitstream that gates the slave.  This can be achieved by using various master timer events to trigger DMA transactions that update the master timer’s PWM period and duty.

This helped me to identify my first peripheral requirement: two chainable timers with PWM output capability, and a DMA controller that can be triggered by update and CCP (Capture-Compare-PWM) events from one of those timers.  The STM32L011 meets both of these requirements; it has:

* Two 16-bit timers, TIM2 and TIM21.
	* TIM2 has 4 CCP channels;
	* TIM21 has 2 CCP channels;
	* TIM2 and TIM21 can be chained together with either as master or slave.
* An 8-channel DMA controller that can be triggered by TIM2 (only) updates, or any of its CCP channel events.

The next requirement is the user interface, i.e. four buttons.  Buttons can be read with GPIO pins, of course, and conveniently the STM32’s GPIO pins provide configurable weak pullups or pulldowns per-pin.  However, buttons need to be debounced, and there is no direct hardware support for this.  It is trivial to do debouncing in software, but ideally a timer is also required.  Unfortunately, the STM32L011 only has two timer peripherals, and both are required for IR signal synthesis.  Fortunately, the Cortex M0+ core also has another timer: the system tick (SYSTICK) timer, primarily intended for RTOS timing.  In this case, as I will not be using an RTOS for such a simple device, the SYSTICK timer is available.

Finally, given the need to keep the microcontroller in STOP mode the majority of the time, the buttons must be able to wake the device up from STOP mode to RUN mode.  The STM32L011’s external interrupt controller (EXTI) can be configured to interact with the M0+ CPU core, monitoring GPIO pins and triggering CPU events and/or interrupts that can wake up the CPU.

## Firmware Development

The complete firmware, including minimal ST CMSIS libraries and associated requirements (linker scripts and startup code), can be found [here](/Firmware).  Note that the ST code is copied as-is from ST's published STM32L01xx libraries.  ST's licence file can be found [here](/Firmware/Libraries/LICENSE.md).

### Environment

I prefer to use the old Atollic ARM IDE because it is reasonably unintrusive and it still supports (most) STM32’s.

### Coding Style

I use direct register access, to e.g. configure and control peripherals, rather than ST’s CMSIS libraries.

Given that I am not using an RTOS, a superloop was the only practical task management option.  There are, arguably, other options, such as a purely interrupt-driven design, where the CPU is woken out of STOP mode straight into interrupt handlers and remains in interrupt handlers until it returns to STOP mode.  It might be possible to lower power consumption even further using such an approach, but I was not convinced that the return on investment stacked up.

### Modules

#### Configuration

The configuration module [config.h](/Firmware/src/inc/config.h) defines several somewhat-configurable parameters via `#define` preprocessor statements.  It also performs bounds checking and other calculations on some of those constants.

#### System

The system module [system.c](/Firmware/src/system.c), [system.h](/Firmware/src/inc/system.h) defines system setup, independent of the Buttons and IRRC modules.  In particular, it configures, reads and sets the GPIO’s used by the other modules.  It also exposes functions to initialize the system; place the system into STOP mode; and retrieve the current count of systick interrupts.

Decoupling is almost, but not quite, perfect between the System and IRRC modules.  The System module assumes that the GPIO choices for IR modulation and bitstream outputs are suitable for the timer channels assigned to these purposes by the IRRC module.

#### Buttons

The buttons module [buttons.c](/Firmware/src/buttons.c), [buttons.h](/Firmware/src/inc/buttons.h) detects and signals button activity.  Each time the service function is executed, it uses a per-button state machine to determine whether it should assert a signal or trigger indicating that an IR signal should be generated for that button.  The state machine performs software debouncing for initial trigger generation, and emits repeated triggers if the button is held down for an extended period.  Buttons are prioritized by index, with the lowest index (0) having the highest priority.  If multiple buttons are pressed simultaneously, a trigger signal will be emitted only for the highest priority active button.

#### Infrared Remote Control (IRRC)

The IRRC module [iirc.c](/Firmware/src/irrc.c), [iirc.h](/Firmware/src/inc/irrc.h) synthesizes the modulated IR output that is used to drive an IRED (Infra-red Emitting Diode) and, ultimately, control the fan.

When a trigger signal for a particular fan command is received, the 16-bit data word to be transmitted is calculated from the base command value, plus a pre-incremented 2-bit counter.  A separate counter is maintained for each command.

Due to the behaviour of ST’s general-purpose timers when configured as a slave with gated output (slave mode 5), it is essential that the active bit period be an integral multiple of the modulation period.  The timer clock input is gated, not its output or reset – when the gate control is de-asserted, the timer simply stops, it is not reset, and if it is emitting a PWM signal then the PWM state does not change because the timer’s counter is not changing.  If the master timer’s period is not an integral multiple of the slaves, then the IR signal may remain active (but unmodulated) in nominally inactive portions of the bitstream.  The modulation period and duty, and the active bit period and duty base values, are pre-calculated to ensure that this requirement is always met.

The data word to be transmitted is encoded into an IR frame.  Each symbol (SOF, ‘0’, ‘1’ and IFG) is treated as a PWM pulse with a particular period and duty cycle.  A pair of arrays of uint16_t values store the period and duty values for each symbol.  Frames may be transmitted several times (the fan manufacturer transmits their frames twice), so the initial frame is duplicated a configurable number of times.  Finally, the IFG duty cycle is set to its maximum possible value (0xffff) for all of the frames except the last copy; this prevents a glitch from occurring between frames.

The slave timer, TIM21, can be activated at this point, as it will not start (its gate input will not be asserted) until the master timer, TIM2, begins generating a PWM output.
Two DMA channels are also activated, one (DMA1_Channel2) triggered by TIM2’s Update events, and the other (DMA1_Channel5) triggered by one of TIM2’s CCP channels (TIM2_CH1), noting that this is not the same CCP channel used for the bitstream output (TIM2_CH3).  Using a second CCP channel allows DMA channel 5 to be triggered shortly after TIM2 updates, but well before the CH3 duty cycle expires, which ensures that glitches and DMA collisions do not occur.  DMA channel 2 reads bitstream duty cycle values out of the duty cycle array and loads them directly into the duty cycle register (TIM2→CCR3).  The new duty cycle is imposed immediately, i.e. the duty cycle applies to the current timer period.  However, the updates loaded into the period register (TIM2→ARR) by TIM2_CH1 events are buffered and take effect only when the current period expires.

## Hardware Development

All of the files necessary to replicate my custom board design can be found [here](/Hardware).  Note that this is a later iteration of the board, to add in an extra test point for the modulated IR signal that drives the IRED, marked on the PCB silkscreen as A3.  The layout shown below is slightly different, but the pin usage on the microcontroller is identical.

![assembled device](/Images/assembled.png)

### Prototyping

A Nucleo-L011K4 was used as a software development platform.  The Nucleo board was mounted on a small breadboard with four tactile SPST buttons and a 940nm IRED, with a 100Ω series resistor for current limiting.  Pin assignments are:

| Function | Nucleo Pin | STM32 Port |
| ---- | ---- | ---- |
| Button 0 | CN4_12 | PA0 |
| Button 1 | CN4_11 | PA1 |
| Button 2 | CN3_15 | PB4 |
| Button 3 | CN3_14 | PB5 |
| Bitstream Out | CN3_8 | PB6 |
| Modulation Out | CN3_13 | PA11 |
| Transmit Active | User LED | PB3 |
| RUN mode | CN3_1 | PA9 |

### Custom Board

I designed and constructed a PCB based on the same microcontroller as the protoyping design: an STM32L011K4, in an LQPF32 package.  The PCB was designed to mount into a [small ABS enclosure that I obtained from a local hobby electronics store](https://www.jaycar.com.au/plastic-molded-enclosures-dark-grey-abs-90-x-50-x-16mm/p/HB6030).  Power is provided by a CR2032 coin cell, and a 6-pin header exposes the microcontroller’s SWD port for load and debug.  Apart from mostly using SMT components, the build uses almost identical parts to the prototype design.

Pin assignments are:

| Function | QFP32 Pin | STM32 Port |
| ---- | ---- | ---- |
| Button 0 | 19 | PA9 |
| Button 1 | 21 | PA11 |
| Button 2 | 20 | PA10 |
| Button 3 | 22 | PA12 |
| Bitstream Out | 8 | PA2 |
| Modulation Out | 9 | PA3 |
| Transmit Active | 7 | PA1 |
| RUN Mode | 6 | PA0 |

## Comparison

My replacement IR remote works perfectly.  It's a little larger than the one from the manufacturer, but I can live with that.

![compare](/Images/compare.png)