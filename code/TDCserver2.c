/*
Red Pitaya TDC server/parser.
Server part from the one and only, Anton Potocnik.
Author: Michel Adamic
Date created: 22.9.2019 -> upgrade of v1.0
Modified 25.9.2019 -> TDC hardware is now accessible globally
2.11.2019 -> 64-bit timestamps
7.11.2019 -> Benchmarking

Version: 2.0

Supports configuring, reading and clearing individual and paired (START-STOP) TDC channels.
Can also read FPGA die temperature.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>

// AXI communication
// write shifts
#define CLR 0
#define RUN 1
// read masks
#define RDY 1  // 0b01
#define FULL 2 // 0b10

// GPIO address offsets
#define READ   0  // GPIO
#define WRITE  2  // GPIO2 (2*4 = 8 bytes offset)

#define BRAMsize 2048   // 2Kpoints BRAM implemented per channel
#define Nch 2  // 2 implemented TDC channels

// TDC Hardware (global variables)
typedef struct {
   uint64_t *bram;   // TDC channel BRAM pointer
   uint32_t *conf;   // TCD channel HW configuration pointer
   int BRAMaddr; // AXI side address (timestamp index)
   int enabled;   // the channel is operational
   int clearing;  // BRAM is clearing, don't touch!
} TDCchannel;

TDCchannel TDC[Nch]; // array of TDC channels
int pairedTDCs[] = {-1, -1};  // indices of paired TDC channels

// function for reading TDC channel data
int readTDC(int TDCindex, uint64_t *data){
      
   //int rdy = *(TDC[TDCindex].conf + READ) & RDY;   // read TDC rdy -> TRUE/FALSE
   //int full = *(TDC[TDCindex].conf + READ) & FULL; // read TDC full -> TRUE/FALSE
   int stopFull;  // full bit of stop channel
   
   int addr; // index for reading through BRAM
   int n = 0;  // number of read data
   uint64_t BRAMdata; // read BRAM data
   
   // read BRAM, starting at BRAMaddr and ending at 0 data or end of BRAM
   for (addr = TDC[TDCindex].BRAMaddr; addr < BRAMsize; addr++){
      
      BRAMdata = *(TDC[TDCindex].bram + addr);   // fetch 64-bit BRAM data at address 'addr'
      if (BRAMdata){  // BRAMdata != 0
         data[n] = ((uint64_t)addr << 48) | BRAMdata;  // append BRAM address and feed 64-bit data package into dataBuffer
         n++;
         //printf("%016" PRIx64 "\n",data[n]); // print sent timestamps
      }else{   // no more data available
         break;
      }
   }
   TDC[TDCindex].BRAMaddr = addr;   // update AXI address for the next reading session
   
   if (TDC[TDCindex].enabled == 2){ // for paired channels -> synced clearing
      if (TDCindex == pairedTDCs[0]){  // trigger channel (read last)
         stopFull = *(TDC[pairedTDCs[1]].conf + READ) & FULL;  // full bit of stop channel
         if (addr == BRAMsize || stopFull){   // clear if trigger full or stop channel full
         
            // clear both channels simultaneously
            *(TDC[pairedTDCs[0]].conf + WRITE) = (0 << RUN) | (1 << CLR); // set RUN=0 and CLR=1
            *(TDC[pairedTDCs[1]].conf + WRITE) = (0 << RUN) | (1 << CLR);
            TDC[pairedTDCs[0]].clearing = 1;   // these channels are now clearing
            TDC[pairedTDCs[1]].clearing = 1;
            TDC[pairedTDCs[0]].BRAMaddr = 0;   // restart AXI address
            TDC[pairedTDCs[1]].BRAMaddr = 0;
         }
      }
   }
   else{ // for single channels
      if (addr == BRAMsize){  // if the entire BRAM was read, it needs to be cleared
      
         *(TDC[TDCindex].conf + WRITE) = (0 << RUN) | (1 << CLR); // set RUN=0 and CLR=1
         TDC[TDCindex].clearing = 1;   // this channel is now clearing
         TDC[TDCindex].BRAMaddr = 0;   // restart AXI address
      }
   }
   
   return n;   // if 0 -> no data was available
}

// function for rearming cleared single TDCs
void rearmTDC(int TDCindex){
   
   //int rdy = *(TDC[TDCindex].conf + READ) & RDY;   // read TDC rdy -> TRUE/FALSE
   int full = *(TDC[TDCindex].conf + READ) & FULL; // read TDC full -> TRUE/FALSE
   
   if (TDC[TDCindex].clearing){  // is channel in the CLEAR state
      if (full == 0){   // is it done clearing
         
         *(TDC[TDCindex].conf + WRITE) = (0 << CLR);   // set CLR=0
         TDC[TDCindex].clearing = 0;
         if (TDC[TDCindex].enabled == 1){
            *(TDC[TDCindex].conf + WRITE) |= (1 << RUN); // set RUN back to 1
         }
      }
   }
}
   
// MAIN program
int main(void){
   
   printf("\n*** TDC Server Online ***\n");
   
   // TDC --------------------------------------------------------------------------------------------------------
   int HW;
   if((HW = open("/dev/mem", O_RDWR)) < 0)   // open Zynq memory
   {
      perror("open");
      return EXIT_FAILURE;
   }
   
   // channel 0
   TDC[0].conf = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, HW, 0x43C00000);   // 4K
   TDC[0].bram = mmap(NULL, 4*sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, HW, 0x43C10000);   // 16K ; _SC_PAGESIZE = 4K
   TDC[0].BRAMaddr = 0;
   TDC[0].enabled = 0;  // 1 - single, free running; 2 - paired, running in tandem
   TDC[0].clearing = 0;
   
   // channel 1
   TDC[1].conf = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, HW, 0x43C20000);   // 4K
   TDC[1].bram = mmap(NULL, 4*sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, HW, 0x43C30000);   // 16K
   TDC[1].BRAMaddr = 0;
   TDC[1].enabled = 0;  // 1 - single, free running; 2 - paired, running in tandem
   TDC[1].clearing = 0;
   
   int TDCindex, TDCindexSTART, TDCindexSTOP; // selected TDC channels
   int full1,full2;
   int speedtest = 0;   // speedtest flag
   uint64_t Nspeed = 0;   // benchmarking (number of collected stamps)
   // ------------------------------------------------------------------------------------------------------------
   
   // SERVER -----------------------------------------------------------------------------------------------------
   int sock_server, sock_client, yes = 1; // server stuff
   struct sockaddr_in addr;
   
   int rx;  // number of bytes received
   int tx;  // number of bytes sent
   int op;  // requested operation
   
   uint16_t command = 0;  // input buffer (2 bytes)
   uint64_t dataBuffer[BRAMsize]; // output buffer = the size of BRAM
   uint64_t emptyWord = 0; // empty 64-bit word
   uint64_t Nstamps; // number of sent data
   
   int raw_temp;   // chip temperature raw reading
   FILE *xadc; // raw temp file pointer for iio XADC
   
   if((sock_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)  // create TCP socket
   {
      perror("socket"); // error
      return EXIT_FAILURE;
   }

   setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, (void *)&yes , sizeof(yes));   // socket options

   /* setup listening address */
   memset(&addr, 0, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port = htons(1001);  // port 1001

   if(bind(sock_server, (struct sockaddr *)&addr, sizeof(addr)) < 0) // assign address to the socket
   {
      perror("bind");
      return EXIT_FAILURE;
   }

   listen(sock_server, 1024); // listen for connections to the server
   printf("Listening on port 1001 ...\n");
   // ----------------------------------------------------------------------------------------------------------------
   
   while(1) // server online
   {
      if((sock_client = accept(sock_server, NULL, NULL)) < 0)  // wait and accept client connections
      {
         perror("accept");
         return EXIT_FAILURE;
      }
      printf("Connection accepted!\n");
      
      while(1) // main program loop
      {
         rx = recv(sock_client, &command, 2, MSG_DONTWAIT);   // read commands from client (don't wait)
         if (rx == 0){  // client disconnected
            break;
         }
         if (rx > 0){   // received command
            
            TDCindex = command & 0xFF; // lower byte
            op = command >> 8;   // upper byte
            switch(op){
               
               case 0:  // disable channel
                  *(TDC[TDCindex].conf + WRITE) &= ~(1 << RUN); // set RUN=0
                  TDC[TDCindex].enabled = 0;
                  //send(sock_client, &emptyWord, 8, MSG_NOSIGNAL); // confirm back to the client
                  break;
                  
               case 1:  // enable channel to run continuously
                  *(TDC[TDCindex].conf + WRITE) |= (1 << RUN);  // set RUN=1
                  TDC[TDCindex].enabled = 1;
                  //send(sock_client, &emptyWord, 8, MSG_NOSIGNAL); // confirm back to the client
                  break;
                  
               case 2:  // read TDC channel
                  if ( !(TDC[TDCindex].clearing) ){ // make sure the channel is not clearing BRAM
                     Nstamps = readTDC(TDCindex, dataBuffer);  // fetch available data to dataBuffer -> N timestamps copied
                  }else{
                     Nstamps = 0;
                  }
                  
                  if (Nstamps > 0){ // send acquired data
                     send(sock_client, &Nstamps, 8, MSG_NOSIGNAL);   // number of stamps
                     send(sock_client, dataBuffer, 8*Nstamps, MSG_NOSIGNAL);  // data
                  }else{ // no available data at the moment
                     send(sock_client, &emptyWord, 8, MSG_NOSIGNAL);
                  }
                  break;   
                  
               case 3:  // manually clear single TDC channel BRAM
                  if ( !(TDC[TDCindex].clearing) ){ // check if the channel is not clearing already
                  
                     *(TDC[TDCindex].conf + WRITE) = (0 << RUN) | (0 << CLR); // set RUN=0 and CLR=0
                     while ( (*(TDC[TDCindex].conf + READ) & RDY) == 0 );  // wait for rdy=1 (IDLE state)
                     *(TDC[TDCindex].conf + WRITE) = (1 << CLR);  // set CLR
                     while ( (*(TDC[TDCindex].conf + READ) & RDY) == 1 );  // wait for rdy=0 (CLEAR state)
                     *(TDC[TDCindex].conf + WRITE) = (0 << CLR);   // set CLR back to 0
                     while ( (*(TDC[TDCindex].conf + READ) & RDY) == 0 );  // wait for rdy=1 (done clearing)
                     if (TDC[TDCindex].enabled == 1){
                        *(TDC[TDCindex].conf + WRITE) |= (1 << RUN); // set RUN back to 1
                     }
                     TDC[TDCindex].BRAMaddr = 0;   // restart AXI address
                  }
                  break;
                  
               case 4:  // synchronously start TWO channels in parallel -> two consecutive '4' operations
                  TDCindexSTART = TDCindex;  // START TDC index
                  rx = recv(sock_client, &command, 2, MSG_WAITALL);  // wait for the STOP index
                  TDCindexSTOP = command & 0xFF; // lower byte
                  op = command >> 8;   // upper byte
                  if (op == 4){
                     TDC[TDCindexSTART].clearing = 0;
                     TDC[TDCindexSTOP].clearing = 0;
                     *(TDC[TDCindexSTART].conf + WRITE) = (0 << RUN) | (0 << CLR);  // set both channels to IDLE
                     *(TDC[TDCindexSTOP].conf + WRITE) = (0 << RUN) | (0 << CLR);
                     while (!( ((*(TDC[TDCindexSTART].conf + READ) & RDY) == 1) && ((*(TDC[TDCindexSTOP].conf + READ) & RDY) == 1) )); // wait for both to be ready
                     // start both channels synchronously -> enable = 2
                     *(TDC[TDCindexSTART].conf + WRITE) |= (1 << RUN);
                     *(TDC[TDCindexSTOP].conf + WRITE) |= (1 << RUN);
                     TDC[TDCindexSTART].enabled = 2;
                     TDC[TDCindexSTOP].enabled = 2;
                     pairedTDCs[0] = TDCindexSTART;
                     pairedTDCs[1] = TDCindexSTOP;
                     
                  }else{
                     printf("ERROR: Unexpected command!\n");
                  }
                  break;
                  
               case 5:  // read die temperature (XADC)
                  xadc = fopen("/sys/bus/iio/devices/iio:device0/in_temp0_raw", "r");
                  if (xadc == NULL){
                     perror("Error reading XADC: ");
                     raw_temp = 0;
                  }else{
                     fscanf(xadc,"%d",&raw_temp);
                     fclose(xadc);
                  }
                  //printf("%d\n",raw_temp);                  
                  send(sock_client, &raw_temp, 4, MSG_NOSIGNAL);   // send raw reading (4 bytes)
                  break;
                  
               case 6:  // stop and break up a TDC channel pair
                  if ((pairedTDCs[0] > -1) && (pairedTDCs[1] > -1)){
                     *(TDC[pairedTDCs[0]].conf + WRITE) &= ~(1 << RUN); // RUN = 0
                     *(TDC[pairedTDCs[1]].conf + WRITE) &= ~(1 << RUN); // RUN = 0
                     TDC[pairedTDCs[0]].enabled = 0;
                     TDC[pairedTDCs[1]].enabled = 0;
                     pairedTDCs[0] = -1;
                     pairedTDCs[1] = -1;
                  }
                  else{
                     printf("ERROR: No pairs found.\n");
                  }
                  break;
                  
               case 7:  // benchmarking -> collecting TDC0 timestamps without sending them
                  *(TDC[0].conf + WRITE) |= (1 << RUN);  // set RUN=1
                  TDC[0].enabled = 1;
                  speedtest = 1;
                  Nspeed = 0; // restart Nspeed
                  break;
                  
               case 8:  // stop benchmark
                  *(TDC[0].conf + WRITE) &= ~(1 << RUN); // set RUN=0
                  TDC[0].enabled = 0;
                  speedtest = 0;
                  send(sock_client, &Nspeed, 8, MSG_NOSIGNAL); // send Nspeed
                  break;
            }
         }
         
         // maintenance for tandem channels
         if ((pairedTDCs[0] > -1) && (pairedTDCs[1] > -1)){   // for paired TDC channels
            full1 = (*(TDC[pairedTDCs[0]].conf + READ) & FULL);   // 1st TDC full flag
            full2 = (*(TDC[pairedTDCs[1]].conf + READ) & FULL);   // 2nd TDC full flag
            if (full1 || full2){ // stop both channels if one of them is full
               *(TDC[pairedTDCs[0]].conf + WRITE) &= ~(1 << RUN); // RUN = 0
               *(TDC[pairedTDCs[1]].conf + WRITE) &= ~(1 << RUN); // RUN = 0
            }
            else{ // channels not full
               if (TDC[pairedTDCs[0]].clearing && TDC[pairedTDCs[1]].clearing){   // if they were clearing -> restart
                  TDC[pairedTDCs[0]].clearing = 0;
                  TDC[pairedTDCs[1]].clearing = 0;
                  *(TDC[pairedTDCs[0]].conf + WRITE) = (0 << CLR);  // set both channels to IDLE
                  *(TDC[pairedTDCs[1]].conf + WRITE) = (0 << CLR);
                  while (!( ((*(TDC[pairedTDCs[0]].conf + READ) & RDY) == 1) && ((*(TDC[pairedTDCs[1]].conf + READ) & RDY) == 1) )); // wait for both to be ready
                  // start both channels synchronously
                  *(TDC[pairedTDCs[0]].conf + WRITE) |= (1 << RUN);
                  *(TDC[pairedTDCs[1]].conf + WRITE) |= (1 << RUN);
               }
            }
         }
         
         for (TDCindex = 0; TDCindex < Nch; TDCindex++){ // maintenance; rearming cleared single TDCs
            if (TDC[TDCindex].enabled == 2) continue; // skip paired channels
            rearmTDC(TDCindex);
         }
         
         if (speedtest == 1){ // continuously read TDC0 timestamps
            if ( !(TDC[0].clearing) ){ // make sure the channel is not clearing BRAM
               Nstamps = readTDC(0, dataBuffer);  // fetch available data to dataBuffer -> N timestamps copied
            }else{
               Nstamps = 0;
            }
            Nspeed = Nspeed + Nstamps;
         }
      }
      
      close(sock_client);  // terminate connection with the client
      printf("Disconnected.\n");
   }
   
   close(sock_server);  // close the server
   return EXIT_SUCCESS; // return 0; defined in <stdlib.h>
}