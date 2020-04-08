# ADC-Stream

ADC-Stream is a simple demo application that captures a stream of data from 
ADC. Signal energy is calculated on the measured stream. ADC is triggered by 
TIMER and LDMA transfers results directly to memory. 

The application does not compile on its own. It is intented to be used with
https://github.com/thinnect/node-apps repo. Get the repo and add ADC-Stream
to node-apps/apps directory as a submodule. It should then compile after
source paths in the Makefile are resolved.

# Platforms
The application has been tested and should work with the following platforms:
 * Thinnect TestSystemBoard0 (tsb0)

# Build
Standard build options apply, check the main [README](../../README.md).
Additionally the device address can be set at compile time, see
[the next chapter](#device_address_/_signature) for details.

# Device address / signature

Devices are expected to carry a device signature using the format specified in
https://github.com/thinnect/euisiggen
and
https://github.com/thinnect/device-signature
.
The device then derives a short network address using the lowest 2 bytes of the
EUI64 contained in the signature.

If the device does not have a signature, then the application will
initialize the radio with the value defined with DEFAULT_AM_ADDR. For example
to set the address to 0xABCD in the firmware for a TestSystemBoard, make
can be called `make tsb DEFAULT_AM_ADDR=0xABCD`. It is necessary to
call `make clean` manually when changing the `DEFAULT_AM_ADDR` value as the
buildsystem is unable to recognize changes of environment variables.
