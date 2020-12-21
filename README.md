# EncoderQualityCheck
This project was created to help identify unexpected results coming from a rotary encoder.
Whilst the code can identify an encoder with a instrinsic fault it has primarily been used to identify when external forces were causing the encoder to misreport.
Specifically external EMI from a VFD were travelling back up through the mains supply and causing the encoder to generate spurious siganls

This is written to handle an encoder that generates 3 distinct signals; A and B signals that are generated as the encoder is turned and a Z (index) signal that 
is generated once per revolution.

The code is designed to run on an Arduino that uses interrupts to handle the above 3 signals. Hence a requirement is an Arduino that can has 3 or more pins that can generate
interrupts. All testing was done on an Arduino Mega2560 and a OMRON E6B2-CWZ6C encoder.

Testing has shown that pullups are necessary on the input signals from the encoder to get a good reading. If external pullups are present then the code needs to not use those 
buily into the Ardiuno and the ACHANNEL_MODE, bCHANNEL_MODE AND ACHANNEL_MODE #defines should be set to INPUT, if external pullups are not present then these defines need to be
set to INPUT_PULLUP.

Processing overview

Initially the program waits until it sees a stable rev rate. Once this is achieved it calculates the stable time per revolution and the expected interval between A and B signals.
The OMRON encoder tested here generates 800 A and B signals per revolution, so whatever the stable time determined for a revolution, the interval between each respective A or B
signal is the revolution time / 800. The 800 number is a constant and can be changed to match other encoder models. See the COUNT_PER_REV #define.

Having determined the stable timings the program then looks at the intervals between respective A, B and Z signals. If the timings deviate from that expected an event is logged.

What is considered to be a timing deviation is set by adjusting two constants ACCEPTABLE_ZMARGIN_PERCENT and ACCEPTABLE_ABMARGIN_PERCENT. These are set to the prcentage deviation
from the expected value that is considered an error. For example setting ACCEPTABLE_ABMARGIN_PERCENT to 5 means if the actual time between consecutive A or B signals is > 105% or 
< 95% of the expected time it is treated as an error.

The program will terminate if the rev rate becomes unstable (as this makes any checking useless) or when the event buffer become full. The log is printed to the serial port when
the program terminates. This output is done at the end to minimise any other work being done that might impact responding to encoder signals in the most timely fashion.

NB Memory is a scarce resource on an Arduino hence the log is limited - currently 475 entries.

This code is free to use / copy. No warranty implied and used at your own risk.
