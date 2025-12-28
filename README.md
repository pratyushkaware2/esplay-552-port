# Ported components from esplay firmware for input and display from espidf 4.4 to 5.x.
The original firmware: https://github.com/pebri86/esplay-base-firmware

- The project **hello_world** contains components folder with the input, ugui and display components.
- Simple snake game to test out the port. 
- espidf 5.x still supports legacy drivers for i2c, so didn't change anything that didn't need to be changed.
- Had to install idf_component for ILI9341 LCD display.
- Note: No joystick support for movement, only 4 buttons.

