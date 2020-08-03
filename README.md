# ZYNQ Time-to-digital converter
A fast high-resolution time-to-digital converter in the Red Pitaya Zynq-7010 SoC

Author: Michel Adamic ada.mic94@gmail.com

**Performance**\
Core frequency: 350 MHz\
No. of delay line taps: 192 (configurable)\
Time resolution per channel: >11 ps\
Accuracy: <10 ppm\
DNL: -1 to +4.5 LSB\
INL: +0.5 to +8.5 LSB\
Measurement range: 47.9 ms\
Dead time: ~14 ns\
Max speed: ~70 MS/s

**Included folders**\
*AXITDC*\
TDC channel IP. Includes VHDL source files, test benches and customized Xilinx IP cores.

*board*\
Red Pitaya board definition files.

*src*\
Source files for creating a two-channel TDC system example project.

*code*\
Associated software for the TDC, including:\
TDCServer2.c - a Linux-based C program for the Zynq ARM core, which communicates with the TDC channels via the "mmap" system call. Addresses are set in the Address Editor of the TDCsystem project.\
PLclock script - contains bash commands for lowering the PL clock frequency from 125 to 100 MHz. Has to be executed before TDC implementation.\
TDCgui3.mlapp - MATLAB App Designer graphical user interface application.

*figs*\
Various figures and schematics of the TDC design.

**2-channel TDC system example project**\
1. Open Vivado
2. Using the Tcl Console, navigate to the "zynq_tdc" folder and execute "source make_project.tcl"
3. Complete the synthesis & implementation steps

**Setup on the Red Pitaya**\
1. Copy the generated bitstream, PLclock script and C server on the Red Pitaya system
2. Run PLclock ("./PLclock") to lower the Zynq PL frequency to 100 MHz
3. Load the FPGA configuration ("cat TDCsystem_wrapper.bit > /dev/xdevcfg")
4. Compile and run the C server ("gcc TDCserver2.c -o TDCserver.exe" and "./TDCserver")
5. On a client PC, start the MATLAB GUI to connect to the TDC system

**Links**\
IEEE paper: https://ieeexplore.ieee.org/abstract/document/8904850 \
My thesis (in Slovene): https://repozitorij.uni-lj.si/IzpisGradiva.php?id=117846&lang=eng
