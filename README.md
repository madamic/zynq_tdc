# ZYNQ Time-to-digital converter
A fast high-resolution time-to-digital converter for the Red Pitaya Zynq-7010 SoC\
Tested on Red Pitaya STEMLab 125-10 and STEMLab 125-14

Author: Michel Adamic ada.mic94@gmail.com

**Performance**\
TDC core frequency: 350 MHz\
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

*figs*\
Various figures and schematics of the TDC design.

*matlab*\
- TDCgui4.mlapp - MATLAB App Designer graphical user interface application.

*setup*\
Files required to run the TDC system on the Red Pitaya board.\
- TDCServer2.c - a Linux-based C program for the Zynq ARM core, which communicates with the TDC channels via the "mmap" system call. Addresses are set in the Address Editor of the TDCsystem project.\
- PLclock script - contains bash commands for lowering the PL clock frequency from 125 to 100 MHz. Has to be executed before TDC implementation.\
- TDCsystem_wrapper.bit - FPGA bitstream.

*src*\
Source files for creating a two-channel TDC system example project.

**2-channel TDC system example project**
1. Open Vivado 2018.2
2. Using the Tcl Console, navigate to the "zynq_tdc/" folder and execute "source make_project.tcl"
3. Complete the synthesis & implementation steps

If you don't want to run these steps and create your own FPGA bitstream, you can use the one already provided in the *setup* folder.

**Setup on the Red Pitaya STEMLab 125-10 or 125-14**
1. Copy the contents of the *setup* folder (FPGA bitstream, PLclock script and C server) on the Red Pitaya system
2. Run PLclock ("./PLclock") to lower the Zynq PL frequency to 100 MHz
3. Load the FPGA configuration ("cat TDCsystem_wrapper.bit > /dev/xdevcfg")
4. Compile and run the C server ("gcc -o TDCserver TDCserver2.c" and "./TDCserver")
5. On a client PC, start the MATLAB GUI application in Matlab App Designer to connect to the TDC system

TDC inputs are located on E1 extension connector pins 17 & 18 (connected to FPGA pins M14 & M15), voltage standard = LVCMOS33 (3,3 V). The TDCs are rising-edge sensitive, i.e. a timestamp is generated for each 0->1 transition.

**Links**\
IEEE paper: https://ieeexplore.ieee.org/abstract/document/8904850 \
My thesis (in Slovene): https://repozitorij.uni-lj.si/IzpisGradiva.php?id=117846&lang=eng
