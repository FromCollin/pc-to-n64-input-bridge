Description

A Python + ESP32 bridge for sending controller input from a PC to original Nintendo 64 hardware with very low latency. It can be used for live PC-driven control and, with minor changes, as a hardware output bridge for TAS playback or AI-generated inputs.

Longer repo summary / README intro

This project lets a PC control a real Nintendo 64 by sending controller input to an ESP32 over USB, then translating that input into valid N64 controller signals. The result is a surprisingly low-latency bridge between Python and original hardware.

It can be used for normal PC-driven input, and with some modification it can also be used to feed inputs from a TAS pipeline or an AI model into a real N64.

A large amount of the timing research and protocol understanding came from qwertymodo’s N64 controller documentation, which was essential for getting the signaling right.

The N64 controller cable has three wires, but only two are needed here: ground and data. The red wire is the controller power line, but in this setup the ESP32 is powered directly over USB from the PC, so that line is not used. You can either sacrifice an old N64 controller or cut an extension cable and wire that to the ESP32 instead. In this build, the black wire is connected to GND, and the white data wire is connected to the ESP32’s receive pin and, through a Schottky diode, to the transmit pin.

The diode is needed because the N64 data line is effectively a shared single-wire communication line. The ESP32 needs to observe the line and also drive it, but you do not want the TX pin to fight the bus or back-feed into the RX side improperly. The Schottky diode helps isolate the transmit path so the ESP32 can pull the line in the intended direction during transmission without creating a direct conflict between TX, RX, and the bus. A Schottky diode is used because it has a low forward voltage drop and fast switching behavior, which is important for the tight N64 timing requirements.

On the software side, the implementation avoids slow software timing loops and instead relies on ESP32 hardware features for much tighter timing. In the RMT-based version, the ESP32 uses its Remote Control peripheral to decode incoming N64 command pulses and generate properly timed response pulses, while Python continuously updates the controller state over USB serial. This approach is much more reliable than trying to bit-bang the protocol entirely in software
