// 09September 2005
//******************************************************************************************************
// Performs basic I/O for the Omega PCI-DAS1602 
//
// Demonstration routine to demonstrate pci hardware programming
// Demonstrated the most basic DIO and ADC and DAC functions
// - excludes FIFO and strobed operations 
//
// Note :
//
//			22 Sept 2016 : Restructured to demonstrate Sine wave to DA
//
// G.Seet - 26 August 2005
//******************************************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
																
#define	INTERRUPT		iobase[1] + 0				// Badr1 + 0 : also ADC register
#define	MUXCHAN			iobase[1] + 2				// Badr1 + 2
#define	TRIGGER			iobase[1] + 4				// Badr1 + 4
#define	AUTOCAL			iobase[1] + 6				// Badr1 + 6
#define 	DA_CTLREG		iobase[1] + 8				// Badr1 + 8

#define	 AD_DATA			iobase[2] + 0				// Badr2 + 0
#define	 AD_FIFOCLR		iobase[2] + 2				// Badr2 + 2

#define	TIMER0				iobase[3] + 0				// Badr3 + 0
#define	TIMER1				iobase[3] + 1				// Badr3 + 1
#define	TIMER2				iobase[3] + 2				// Badr3 + 2
#define	COUNTCTL			iobase[3] + 3				// Badr3 + 3
#define	DIO_PORTA		iobase[3] + 4				// Badr3 + 4
#define	DIO_PORTB		iobase[3] + 5				// Badr3 + 5
#define	DIO_PORTC		iobase[3] + 6				// Badr3 + 6
#define	DIO_CTLREG		iobase[3] + 7				// Badr3 + 7
#define	PACER1				iobase[3] + 8				// Badr3 + 8
#define	PACER2				iobase[3] + 9				// Badr3 + 9
#define	PACER3				iobase[3] + a				// Badr3 + a
#define	PACERCTL			iobase[3] + b				// Badr3 + b

#define 	DA_Data			iobase[4] + 0				// Badr4 + 0
#define	DA_FIFOCLR		iobase[4] + 2				// Badr4 + 2


int badr[5];															// PCI 2.2 assigns 6 IO base addresses
int Select;
char Select_c[10];
unsigned int data[100];
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	
int main() {
struct pci_dev_info info;
void *hdl;

uintptr_t iobase[6];
uintptr_t dio_in;
uint16_t adc_in;
	
unsigned int i,count;
unsigned short chan;
//unsigned int data[100];
float delta,dummy;

printf("\fDemonstration Routine for PCI-DAS 1602\n\n");

memset(&info,0,sizeof(info));
if(pci_attach(0)<0) {
  perror("pci_attach");
  exit(EXIT_FAILURE);
  }

																		/* Vendor and Device ID */
info.VendorId=0x1307;
info.DeviceId=0x01;

if ((hdl=pci_attach_device(0, PCI_SHARE|PCI_INIT_ALL, 0, &info))==0) {
  perror("pci_attach_device");
  exit(EXIT_FAILURE);
  }
  																		// Determine assigned BADRn IO addresses for PCI-DAS1602			

printf("\nDAS 1602 Base addresses:\n\n");
for(i=0;i<5;i++) {
  badr[i]=PCI_IO_ADDR(info.CpuBaseAddress[i]);
  printf("Badr[%d] : %x\n", i, badr[i]);
  }
 
printf("\nReconfirm Iobase:\n");  						// map I/O base address to user space						
for(i=0;i<5;i++) {												// expect CpuBaseAddress to be the same as iobase for PC
  iobase[i]=mmap_device_io(0x0f,badr[i]);	
  printf("Index %d : Address : %x ", i,badr[i]);
  printf("IOBASE  : %x \n",iobase[i]);
  }													
																		// Modify thread control privity
if(ThreadCtl(_NTO_TCTL_IO,0)==-1) {
  perror("Thread Control");
  exit(1);
  }																											
   		
//**********************************************************************************************
// Setup waveform array
//**********************************************************************************************


MainMenu();

/*
printf("Select a number 1-4:\n1:square wave\n2:sawtooth wave\n3:triangle wave\n");
scanf("%d",&Select);

//error filter
while(Select<1 || Select>4){
Select =0;
printf("Error try again\n");
printf("Select a number 1-4:\n1:square wave\n2:sawtooth wave\n3:triangle wave\n");
scanf("%d",&Select);
} // when character is inserted it loops infinitely, unsure why.

printf("%d Is selected",Select);
 WaveGen(Select);
*/




// SINE WAVE ONLY																
/*delta=(2.0*3.142)/50.0;					// increment
for(i=0;i<50;i++) {
  dummy= ((sinf((float)(i*delta))) + 1.0) * 0x8000 ;  // 0X8000 IS 5V MAX
  data[i]= (unsigned) dummy;			// add offset +  scale
 }
*/
/*
// SQUARE WAVE ONLY
for(i=0;i<50;i++) {

	if (i <50/2){
	  dummy= 1* 0x8000 ;  // 0X8000 IS 5V MAX
  	data[i]= (unsigned) dummy;			// add offset +  scale
	}else{
	  dummy=0 * 0x8000 ;  // 0X8000 IS 5V MAX
	  data[i]= (unsigned) dummy;			// add offset +  scale
	}
	printf("Data[%d]: %d\n" ,i,data[i]);
 }
*/



//*********************************************************************************************
// Output wave
//*********************************************************************************************
printf("\nCtrl-C to exit\n");
printf("\n Q to go back\n");

while(1) {
for(i=0;i<50;i++) {
	out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar		
   	out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
   	out16(DA_Data,(short) data[i]);																																		
   	out16(DA_CTLREG,0x0a43);			// DA Enable, #1, #1, SW 5V unipolar		
  	out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
	out16(DA_Data,(short) data[i]);																																		
  	}
	Select_c = getchar();	
}
 
//**********************************************************************************************
// Reset DAC to default : 5v
//**********************************************************************************************

out16(DA_CTLREG,(short)0x0a23);	
out16(DA_FIFOCLR,(short) 0);			
out16(DA_Data, 0x8fff);						// Mid range - Unipolar																											
  
out16(DA_CTLREG,(short)0x0a43);	
out16(DA_FIFOCLR,(short) 0);			
out16(DA_Data, 0x8fff);				
																																						
printf("\n\nExit Demo Program\n");
pci_detach_device(hdl);

//**********************************************************************************************

return(0);
}

/*
printf("Select a number 1-4:\n1:square wave\n2:sawtooth wave\n3:triangle wave\n");
	scanf("%d",&Select);

	//error filter
	while(Select_C<1 || Select>4){
	//Select =0;
	printf("Error try again\n");
	printf("Select a number 1-4:\n1:square wave\n2:sawtooth wave\n3:triangle wave\n");
	Select_C = getchar();
	//scanf("%d",&Select);
	} // when character is inserted it loops infinitely, unsure why.

	printf("%d Is selected",Select);
*/

void MainMenu()
{
	printf("Select a number 1-4:\n1:square wave\n2:sawtooth wave\n3:triangle wave\n");
	
	do{
	fgets(Select_c,sizeof(Select_c),stdin);
	Select = strtol(Select_c,NULL,10);
	//system("clear");
	if(Select<1|| Select>4)printf("Error try again\n");printf("Select a number 1-4:\n1:square wave\n2:sawtooth wave\n3:triangle wave\n");
	}while(Select<1|| Select>4);
	
	switch(Select){
	case 1:
	printf("Test 1");
	SineWave();
	break;
	case 2:
	printf("Test 2");
	SquareWave();
	break;
	case 3:
	printf("Test 3");
	break;
	case 4:
	printf("Test 4");
	break;
	default:
		printf("Error 1");
	}//Switch
	
}//End of mainmenu



void SineWave()
{
unsigned int i;
float delta,dummy;

// SINE WAVE ONLY																
delta=(2.0*3.142)/50.0;					// increment
for(i=0;i<50;i++) {
  dummy= ((sinf((float)(i*delta))) + 1.0) * 0x7fff ;  // 0X8000 IS 5V MAX
  data[i]= dummy;			// add offset +  scale
  printf("Data [%d] : %.2f \n ",i, data[i]);
  printf("%.2f",dummy);
 }
  
 } // EOF SineWave
 
 void SquareWave()
{
unsigned int i;
float delta,dummy;
													
delta=(2.0*3.142)/50.0;					// increment
for(i=0;i<50;i++) {
	if (i<25){  data[i]= 1 *0x7fff;			// add offset +  scale
	}else { data[i]= 0 *0x7fff;}
 }
  
 } // EOF SineWave





