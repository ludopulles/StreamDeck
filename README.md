# StreamDeck
A minimal setup for programming button-press triggers for the Elgato Stream Deck

# Dependencies
The following packages are required to run the program:
* libusb-1.0
These are installable by the available package manager on Linux. Windows is not tested.

# Compiling
One can compile the program with the gcc compiler whilst including the libusb-1.0 library like so:

```bash
gcc main.c -lusb-1.0
```
