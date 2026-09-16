## Using `gdb` with command line arguments and real-time data

To debug your project with `gdb`, first you will have to build your project in the [debug](cmake-build-debug.md) mode.

Then, run your program in `gdb` as follows:

`gdb ./project`

Then, enter the following to run your program in a debug environment:

`run`

The above assumes no command line arguments have been passed on to your project's program. If you wish to launch the project with command line arguments, e.g., mode 3 with mono audio, do the following:

`run 3 m`

When you get a segmentation fault, enter the following to print the stack trace, which lists all the function calls that led to the crash. In particular, look for what line(s) of code in **your files** the program crashed at after typing:

`backtrace`

When working with input/output redirections (e.g., prerecorded files during the 3DY4 project), you can use the following after launching `gdb`:

`run 3 m < iq_samples.raw > >(aplay -f S16_LE -r 48000) -c 1`

Note, however, that your program running in debug mode is unlikely to keep pace with `aplay` running in real time. If you still wish to save the audio samples produced by your debugged program, you can do so by breaking the UNIX pipe by first running your program from within `gdb` as follows:

`run 3 m < iq_samples.raw > audio.bin`

And then streaming the audio samples into `aplay`:

`cat audio.bin | aplay -f S16_LE -r 48000 -c 1`

When working with live data on the Raspberry Pi hardware, you can still run your program in the debug mode within `gdb` as follows:

`run 3 m < <(rtl_sdr -f 107.9M -s 2.4M -) > >(aplay -f S16_LE -r 48000 -c 1)`

The above examples assume the project was launched in mode 3 with mono audio. Adjust the command line arguments as you see fit. Note, however, that it is still recommended that you first record your samples:

`rtl_sdr -f 107.9M –s 2.4M - > iq_samples.raw`

Then, within `gdb`, debug your program as shown above. When finished using `gdb`, exit by entering `quit`.

