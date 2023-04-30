# Waveform Generator Program

  

## Introduction

This is a program for major CA of MA4830 - Realtime Software For Mechatronic Systems (Academic Year 2022/2023 Sem 2). The program has a few functionality that are described below.

  

## Function 1: Sine Wave Generator

- Use the “sin” function to calculate and print the sine of zero to 2 in 100 steps

- Determine a reasonable number of points based on the accuracy of the waveform

- Print the values onto the screen to help debug the code

- Scale data to a range of 0x0000 to 0xffff (16 bit resolution)

- Mid range is 0x7fff

- Connect the D/A output to an oscilloscope

- Determine a method to change the output frequency and introduce a delay between points to correlate delay with frequency

- Change the amplitude and/or frequency

- Attempt to produce a function that can produce a continuous waveform with the ability to change frequency, mean, and amplitude.

  

## Function 2: Convert function to output other waveforms

- Write equivalent functions for square and triangular waveforms

- Enhance the previous function to allow a choice of waveforms

- The square wave has only two levels, 0x0000 for half of the cycle and a maximum value (0xffff) for the second half.

- The triangular wave increases from 0 to 0xffff for the first half of the cycle and decreases to 0x0000 for the second half of the cycle.

- The sawtooth wave ramps up from 0x0000 to 0xffff for one full cycle and repeats again for the next cycle.

  

## Function 3: Command line arguments

- Modify the program to extract ‘triangular’, ‘sawtooth’, ‘square’, and ‘sine’ from command line arguments

- Incorporate into the first program to allow a choice of waveforms at command line

- Try multi-parameter passing, such as type of waveform, frequency, and amplitude

- Check the values submitted at the command line for range correctness

  

## Function 4: Additional Inputs

- Use the A/D input to change the amplitude of the waveform by reading in the value and using it to scale the amplitude of the waveform

- Use the digital port as an alternative to terminating the program instead of using ctrl-C by reading in digital port pattern and exiting if pattern changes

- Consider the use of file I/O by reading a potentiometer port and writing out the data to disk, outputting this pattern to D/A, and providing an illustration on the Oscilloscope while the data is being read.

  

## Integration of Basic Functionality (Incorporating Realtime Programming features)

- Restructure the program to utilize real-time techniques such as multi-process and multi-threads

- One ‘ask’ could handle user I/O

- Other ‘tasks’ handles DAC, ADC, DIO and file I/O services

- The program should run till terminated by the user in a controlled manner, either from the terminal or switch

- The terminal would need to reflect board status

- Communication between processes may be realized by the MsgSend, MsgReceive, MsgReply mechanism or pipes or Global variables when implemented with Threads

- Threads can communicate using global parameters and coordinated using “Mutex”

- All threads and processes should be created by executing only ONE program.

  
  

## Additional Notes

### Copy from Lab PC to USB

1. Plug your USB drive

2. Check your USB directory, by running `ls /fs/`, usually it is hd10-dos1

3. Copy your files to the USB by running `cp /your/files/directory/file-name /your/destination/folder/file-name`.

4. For example: if the directory of your USB is hd10-dos1, and the file you want to copy is at /home/guest/ds/sine.c. You can run `cp /home/guest/ds/sine.c /fs/hd10-dos1/sine.c`

5. Ensure that the files are successfully copied by running `ls /fs/hd10-dos1`

6. Unmount the USB drive by runnning: `umount /fs/your_USB_drive_name_directory`, in this case it will be `umount /fs/hd10-dos1`

7. Done ~

OR

You can use the file manager in photon environment to copy the files to your USB drive.

Note: If you want to copy a directory use -R in the cp command: `cp -R /your/files/directory /your/destination/folder`