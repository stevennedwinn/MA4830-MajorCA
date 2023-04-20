#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#define	INTERRUPT	  iobase[1] + 0		// Badr1 + 0 : also ADC register
#define	MUXCHAN		  iobase[1] + 2		// Badr1 + 2
#define	TRIGGER		  iobase[1] + 4		// Badr1 + 4
#define	AUTOCAL		  iobase[1] + 6		// Badr1 + 6
#define DA_CTLREG	  iobase[1] + 8		// Badr1 + 8

#define	AD_DATA	    iobase[2] + 0		// Badr2 + 0
#define AD_FIFOCLR	iobase[2] + 2		// Badr2 + 2

#define	TIMER0		  iobase[3] + 0		// Badr3 + 0
#define	TIMER1		  iobase[3] + 1		// Badr3 + 1
#define	TIMER2		  iobase[3] + 2		// Badr3 + 2
#define	COUNTCTL	  iobase[3] + 3		// Badr3 + 3
#define	DIO_PORTA	  iobase[3] + 4		// Badr3 + 4
#define	DIO_PORTB	  iobase[3] + 5		// Badr3 + 5
#define	DIO_PORTC	  iobase[3] + 6		// Badr3 + 6
#define	DIO_CTLREG	iobase[3] + 7		// Badr3 + 7
#define	PACER1		  iobase[3] + 8		// Badr3 + 8
#define	PACER2		  iobase[3] + 9		// Badr3 + 9
#define	PACER3		  iobase[3] + a		// Badr3 + a
#define	PACERCTL	  iobase[3] + b		// Badr3 + b

#define DA_Data		  iobase[4] + 0		// Badr4 + 0
#define	DA_FIFOCLR	iobase[4] + 2		// Badr4 + 2

#define BILLION 1000000000L
#define MILLION 1000000L
#define THOUSAND	1000L
#define NUM_THREADS	 5
#define steps	50
#define amplitude  1    //default wave parameters
#define average 1
#define frequency 1
#define duty_cycle 50

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Global Variable
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int badr[5];			//PCI 2.2 assigns 6 IO base addresses
void *hdl;
uintptr_t dio_in;
uintptr_t iobase[6];
uint16_t adc_in[2];
int chan;
pthread_t thread[NUM_THREADS];
char Select_c[10];
int Select;

typedef struct {
  float amp,
        mean,
        freq;
} channel_para;   //store waveform parameters

typedef int wave_pt[steps];  //store points of wavefomrs with resolution steps

channel_para *ch;
wave_pt *wave_type;

int wave[2];  //store wave type for each channel

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Functions
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


void f_PCIsetup(){
  struct pci_dev_info info;
  unsigned int i;

  memset(&info,0,sizeof(info));
  if(pci_attach(0)<0) {
    perror("pci_attach");
    exit(EXIT_FAILURE);
  }

  // Vendor and Device ID
  info.VendorId=0x1307;
  info.DeviceId=0x01;

  if ((hdl=pci_attach_device(0, PCI_SHARE|PCI_INIT_ALL, 0, &info))==0) {
    perror("pci_attach_device");
    exit(EXIT_FAILURE);
  }

  for(i=0;i<5;i++) {
    badr[i]=PCI_IO_ADDR(info.CpuBaseAddress[i]);
  }

// map I/O base address to user space
  for(i=0;i<5;i++) {			// expect CpuBaseAddress to be the same as iobase for PC
    iobase[i]=mmap_device_io(0x0f,badr[i]);
  }
  // Modify thread control privity
  if(ThreadCtl(_NTO_TCTL_IO,0)==-1) {
    perror("Thread Control");
    exit(1);
  }

  // Initialise Board
  out16(INTERRUPT,0x60c0);		// sets interrupts	 - Clears
  out16(TRIGGER,0x2081);			// sets trigger control: 10MHz,clear,
  // Burst off,SW trig.default:20a0
  out16(AUTOCAL,0x007f);			// sets automatic calibration : default
  out16(AD_FIFOCLR,0); 			// clear ADC buffer
  out16(MUXCHAN,0x0900);		// Write to MUX register-SW trigger,UP,DE,5v,ch 0-0
  // x x 0 0 | 1  0  0 1  | 0x 7   0 | Diff - 8 channels
  // SW trig |Diff-Uni 5v| scan 0-7| Single - 16 channels

  //initialise port A, B
  out8(DIO_CTLREG,0x90);		// Port A : Input Port B: Output
}

void f_WaveGen(){
 int i;
 float dummy;

  //Sine wave array
 float delta = (2.0*3.142)/steps;
 for(i=0; i<steps; i++){
  dummy = (sinf(i*delta))*0x8000/5;
  wave_type[0][i] = dummy;
}
  //Square wave array
  //the value of 2 in dummy when i<steps/2 indicates the value of 'ON'
  //the value of 0 in dummy when i<steps indicates the value of 'OFF'
for(i=0;i<steps;i++){
  if(i<=steps/2) dummy = 1*0x8000;
  if(i>steps/2) dummy = -1*0x8000;
  wave_type[1][i] =  dummy /5;
}
//Saw-tooth wave array
  //the delta used is similar to the one used for sine wave
  //the dummy is increased by multiple of delta when i <steps/2
  //then the dummy is decrease by multiple of delta when i<steps
  //the value of '1' in dummy when i<steps indicates the max value of wave form when i=steps/2
delta = 2.0/steps;
for(i=0;i<steps;i++){
  if(i<=steps/2) dummy = i*delta*0x8000;
  if(i>steps/2 && i<steps) dummy = (-2+i*delta)*0x8000;
  wave_type[2][i] =  dummy /5;
}
  //Triangular wave array
  //the value of 2 in delta indicates the max vertical value of the wave form when i = steps-1, the value is closed to 2
  //the min vertical value of wave form is 0 when i = 0
delta = 4.0/steps;
for(i=0;i<steps;i++){
  if(i<=steps/4) dummy = i*delta*0x8000;
  if(i>steps/4 && i<=steps*3/4) dummy = (2-i*delta)*0x8000;
  if(i>steps*3/4 && i<steps) dummy = (-4+i*delta)*0x8000;
  wave_type[3][i] =  dummy /5;
}
}

void f_ArgCheck(int argc, char* argv[]){
int i;
char **p_to_arg = &argv[1];
printf("Reading command line arguments...")
if(argc>=2){
  for(i=1;i<argc;i++,p_to_arg++){
    if(strcmp(*(p_to_arg),"-sine")==0){
      printf("\n%d. Sine wave chosen.\n",i);
      wave[i-1]=1;
      while (i<argc || i!=0){
        if(check_additional(i,argc,argv)>i) {
          i=check_additional(i,argc,argv);
          p_to_arg = &argv[i];
        }
      }
    }
    if(strcmp(*(p_to_arg),"-square")==0){
      printf("\n%d. Square wave chosen.\n",i);
      wave[i-1]=2;
      while (i<argc || i!=0){
        if(check_additional(i,argc,argv)>i) {
          i=check_additional(i,argc,argv);
          p_to_arg = &argv[i];
        }
      }
    }
    if(strcmp(*(p_to_arg),"-saw")==0){
      printf("\n%d. Saw wave chosen.\n",i);
      wave[i-1]=3;
      while (i<argc || i!=0){
        if(check_additional(i,argc,argv)>i) {
          i=check_additional(i,argc,argv);
          p_to_arg = &argv[i];
        }
      }
    }
    if(strcmp(*(p_to_arg),"-tri")==0){
      printf("\n%d. Triangular wave chosen.\n",i);
      wave[i-1]=4;
      while (i<argc || i!=0){
        if(check_additional(i,argc,argv)>i) {
          i=check_additional(i,argc,argv);
          p_to_arg = &argv[i];
        }
      }
    }
    }//end of for loop, checking argv[]
  }
  if (argc==1) { //if no terminal argument is input, set sine wave as default waveform for both channel
    wave[0]=1; wave[1]=2;
  }
  if (argc==2) { //if only 1 argument is input, set sine wave as default waveform the other channel
    wave[1]=2;
  }
}

//read additional input for -amplitude value, -frequency value, -average value
//read multiple additional input
//return the current index of argv[]
int check_additional(int i, int argc, char* argv[]){
  if (i+1<argc) {
    printf("Additional arguments detected. Reading additional arguments...");
    if (strcmp(argv[i+1],"-amplitude")==0) {
      amplitude = atof(argv[i+2]);
      printf("Amplitude value is %f.\n",amplitude);
      return i+2;
    }
    if (strcmp(argv[i+1],"-frequency")==0) {
      frequency = atof(argv[i+2]);
      printf("Frequency value is %f.\n",frequency);
      return i+2;
    }
    if (strcmp(argv[i+1],"-average")==0) {
      average = atof(argv[i+2]);
      printf("Average value is %f.\n",average);
      return i+2;
    }
  }
  else{
    return 0;
  }
}

void f_Malloc(void){
 int i;
 if((ch = (channel_para*)malloc(2 * sizeof(channel_para))) == NULL) {
  printf("Not enough memory.\n");
}

  ch[0].amp  = amplitude;
  ch[0].mean = average;
  ch[0].freq = frequency;

if((wave_type = (wave_pt*)malloc(4 * sizeof(wave_pt))) == NULL) {
  printf("Not enough memory.\n");
  exit(1);
}
}

void f_termination(){
  out16(DA_CTLREG,(short)0x0a23);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_Data, 0x8fff);					// Mid range - Unipolar

  out16(DA_CTLREG,(short)0x0a43);
  out16(DA_FIFOCLR,(short) 0);
  out16(DA_Data, 0x8fff);
  pci_detach_device(hdl);

  free((void *) ch);
  free((void *) wave_type);
  printf("Reset to Default Setting\nDetach PCI\nReleased DMA\n");
}

// Start Keyboard
int f_GetInt(int lowLim, int highLim) {
  int outnum;
  while (true)
  {
    char c;

    //get int input
    scanf("%d", &outnum);
    //flush input buffer
    while ((c = getchar()) != '\n' && c != EOF) { }

    //check if outnum is within low and high limit
    //otherwise continue loop
      if (outnum >= lowLim && outnum <= highLim)
        return outnum;
      else
        printf("Your number should be within %d and %d
        \nPlease input a valid number
        \n", lowLim, highLim);
    }
  }

  void f_ChangeWavePrompt(){
    const char *wave_str[] = {"Sine","Square","Sawtooth","Triangular"};

    printf("Select a number 1-4:
      \n1. Sine Wave
      \n2. Square Wave
      \n3. Sawtooth Wave
      \n4. Triangle Wave
      \n\n0. Return Main Menu\n\n");
    //set wave for channel
    //return main menu if input = 0
    //if ((wave[0] = f_GetInt(0, 4)) == 0)
      //return;
    
    do{
		fgets(Select_c,sizeof(Select_c),stdin);
		Select = strtol(Select_c,NULL,10);
		//system("clear");
		if(Select<1|| Select>4){
			printf("Invalid input! Please try again\n");
			printf("Select a number 1-4:
			\n1: Sine Wave
			\n2: Square Wave
			\n3: Sawtooth Wave
			\n4: Triangle wave\n");
		}
	}while(Select<1|| Select>4);
	wave[0] = Select;

  // print selected wave
    printf("\n%s Wave Selected\n\n", wave_str[wave[0]-1]);
  }

  void f_SaveFile(char *filename, FILE *fp, char *data){
    strcat(filename, ".txt");
    printf("\nFile saving in progress, please wait...\n");

    if ((fp = fopen(filename, "w")) == NULL){
      printf("Cannot open\n\n");
      return;
    }
    if (fputs(data, fp) == EOF){
      printf("Cannot write\n\n");
      return;
    }
    fclose(fp);
    printf("File saved!\n\n");
  }

  void f_SaveFilePrompt(){
    const char *wave_str[] = {"Sine","Square","Sawtooth","Triangular"};
    char filename[100];
    FILE *fp;
    char data[200];
    printf("\n\nYou have indicated to save the output to a file.
      \nPlease name your file(.txt):
      \n\n0. Return Main Menu\n\n");
    scanf("%s", &filename);

    //return main menu if input = 0
    if(strcmp(filename,"0")==0)
      return;

  //set data to store into file
    sprintf(data,
     "\t\t\t\t\tAmp. Mean Freq. Wave\n
     Channel 1: \t%2.2f\t%2.2f\t%2.2f\t%d.%s\n
\n",
     ch[0].amp , ch[0].mean, ch[0].freq, wave[0], wave_str[wave[0]-1]
    );
    f_SaveFile(filename, fp, data);
  }

  void f_ReadFile(char *filename, FILE *fp){
    int count;
    channel_para temp_ch[2];
    int temp_wave[2];

    strcat(filename, ".txt");
    printf("\nFile reading in progress, please wait...\n");

    if ((fp = fopen(filename, "r")) == NULL){
      printf("Cannot open\n\n");
      return;
    }
  //get variables from file
    count = fscanf(fp, "%*[^C]Channel 1: %f %f %f %d",
      &temp_ch[0].amp, &temp_ch[0].mean, &temp_ch[0].freq, &temp_wave[0]
    );
  //if received all 8 values successfully, set values to variables
    if (count == 4){
      int i;
      for(i = 0; i < 2; i++){
        wave[i] = temp_wave[i];
        ch[i].amp = temp_ch[i].amp;
        ch[i].mean = temp_ch[i].mean;
        ch[i].freq = temp_ch[i].freq;
      //printf("%f, %f, %f", temp_ch[i].amp, temp_ch[i].mean, temp_ch[i].freq);
      }
      printf("File Read Successfully\n\n");
    }else{
      printf("File Read Fail\n\n");
    }
    fclose(fp);
  }

  void f_ReadFilePrompt(){
    char filename[100];
    FILE *fp;
    char data[100];
    printf("\n\nYou have indicated to read the file.
      \nPlease name your file(.txt):
      \n\n0. Return Main Menu\n\n");
    scanf("%s", &filename);

    //return main menu if input = 0
    if(strcmp(filename,"0")==0)
      return;

    f_ReadFile(filename, fp);
  }



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Threads
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  void *t_Wave1(void* arg){

   unsigned int i;
   unsigned int current1[steps];
   struct timespec start1, stop1;
   double accum1=0;

   while(1){

    //Channel 1
    if (clock_gettime(CLOCK_REALTIME,&start1)==-1){
     perror("clock gettime");
     exit(EXIT_FAILURE);
   }

   for (i=0; i<steps; i++){
       current1[i]= (wave_type[wave[0]-1][i] * ch[0].amp)*1.2 + (ch[0].mean*1.6 + 1.2)*0x8000/5 * 2.5 ;
       // scale + offset
   }
   for (i=0; i<steps; i++){
        out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
        out16(DA_Data,(short) current1[i]);
        delay (   (   (1/ch[0].freq*1000)-(accum1)   )/steps);
      }

      if (clock_gettime(CLOCK_REALTIME,&stop1)==-1){
       perror("clock gettime");
       exit(EXIT_FAILURE);
     }

     accum1=(double)(stop1.tv_sec-start1.tv_sec)+(double)(stop1.tv_nsec-start1.tv_nsec)/BILLION;
   }
 }

/* void *t_Wave2(void* arg){

   unsigned int i;
   unsigned int current2[steps];
   struct timespec start2, stop2;
   double accum2=0;


   while(1){

    //Channel 2
    if (clock_gettime(CLOCK_REALTIME,&start2)==-1){
     perror("clock gettime");
     exit(EXIT_FAILURE);
   }

   for (i=0; i<steps; i++){
     current2[i]= (wave_type[wave[1]-1][i] * ch[1].amp)*0.6 + (ch[1].mean*0.8 + 1.2)*0x8000/5 * 2.5 ;
      // scale + offset
   }
   for (i=0; i<steps; i++){
        out16(DA_CTLREG,0x0a43);			// DA Enable, #0, #1, SW 5V unipolar
        out16(DA_FIFOCLR, 0);				// Clear DA FIFO  buffer
        out16(DA_Data,(short) current2[i]);
        delay (((1/ch[1].freq*1000)-(accum2))/steps);
      }

      if (clock_gettime(CLOCK_REALTIME,&stop2)==-1){
       perror("clock gettime");
       exit(EXIT_FAILURE);
     }

     accum2=(double)(stop2.tv_sec-start2.tv_sec)+(double)(stop2.tv_nsec-start2.tv_nsec)/BILLION;
   }
 }
*/

 void *t_HardwareInput(void* arg){

   int mode;
   unsigned int count;


   while(1)  {
    dio_in=in8(DIO_PORTA); 					// Read Port A


    if((dio_in & 0x08) == 0x08) {
      out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B
      if((dio_in & 0x04) == 0x04) {
        raise(SIGINT);
      }
      else if ((mode = dio_in & 0x03) != 0) {
        count=0x00;

        while(count <0x02) {
          chan= ((count & 0x0f)<<4) | (0x0f & count);
          out16(MUXCHAN,0x0D00|chan);		// Set channel	 - burst mode off.
          delay(1);					// allow mux to settle
          out16(AD_DATA,0); 				// start ADC
          while(!(in16(MUXCHAN) & 0x4000));
          adc_in[(int)count]=in16(AD_DATA);
          count++;
          delay(5);		// Write to MUX register - SW trigger, UP, DE, 5v, ch 0-7
        }
      }

      switch ((int)mode) {
       case 1:
        ch[0].amp = (float)adc_in[0] * 5.00 / 0xffff; //scale from 16 bits to 0 ~ 5
        //ch[1].amp =(float)adc_in[1] * 5.00 / 0xffff; //scale from 16 bits to 0 ~ 5
        break;
        case 2:
        ch[0].freq = (float)adc_in[0] * 4.50 / 0xffff+0.5; //scale from 16 bits to 0.5 ~ 5
        //ch[1].freq = (float)adc_in[1] * 4.50 / 0xffff+0.5; //scale from 16 bits to 0.5 ~ 5
        break;
        case 3:
        ch[0].mean = (float)adc_in[0] * 2.00 / 0xffff; //scale from 16 bits to 0.00 ~ 2.00
        //ch[1].mean = (float)adc_in[1] * 2.00 / 0xffff; //scale from 16 bits to 0.00 ~ 2.00
        break;
      }
    }	//if take input from keyboard
    delay(100);
  } //end of while
} //end of thread

void *t_ScreenOutput(void* arg){
  delay(100);
  printf("\nTo Exit Program, press Ctrl + C\nTo Enter Keyboard Menu, press Ctrl + \\ \n");
  printf("\nReal Time Inputs are as follow:-\n\n");
  printf("Channel Parameters\n");
  printf("Amp.\tMean\tFreq\n");
  while(1){
    printf("\r%2.2f\t%2.2f\t%2.2f", ch[0].amp , ch[0].mean, ch[0].freq);
    fflush(stdout);
    delay(100);
  }
}

void *t_UserInterface(){
  int input;

  //to stop screen output
  if(pthread_cancel(thread[2]) == 0)

    while(true){
      printf("\n\n\nMAIN MENU\nPlease choose your next action:\n\n");
      printf("1. Change Waveform
        \n2. Save and Output File
        \n3. Read File
        \n4. Return to Display
        \n5. End the Program\n\n");
      input = f_GetInt(1, 5);

      if (input == 1){
        f_ChangeWavePrompt();
      }else if (input == 2){
        f_SaveFilePrompt();
      }else if (input == 3){
       if((dio_in & 0x08) == 0x08){
        printf("\n\nPlease switch off first toggle switch\n\n");
        delay(1000);
       }
       else
        f_ReadFilePrompt();
    }else if (input == 4){
      //clear console screen
      system("clear");
      if(pthread_create(&thread[3], NULL, &t_ScreenOutput, NULL)){
        printf("ERROR; thread \"t_ScreenOutput\" not created.");
      }
      break;
    }else if (input == 5){
      printf("\nBye bye\nHope to see you again soon :p\n");
      raise(SIGINT);
    }
  }
}
// End Keyboard

//++++++++++++++++++++++++++++++++++
//SIGNAL_HANDLER
//++++++++++++++++++++++++++++++++++


void signal_handler(){
  int t;
  pthread_t temp;

  temp = pthread_self();
  printf("\nHardware Termination Raised\n");

  f_termination();

  for(t = 3; t >= 0; t--){
   if(thread[t] != temp) {
    pthread_cancel(thread[t]);
    printf("Thread %d is killed.\n",thread[t]);
  }
  }// for loop
  printf("Thread %d is killed.\n", temp);
  pthread_exit(NULL);
  delay(500);
} // handler


void signal_handler2(){
  pthread_create(NULL, NULL, &t_UserInterface, NULL);
}

//++++++++++++++++++++++++++++++++++
//MAIN
//++++++++++++++++++++++++++++++++++

int main(int argc, char* argv[]) {

  int j=0; //thread count
  f_PCIsetup();
  f_Malloc();
  f_WaveGen();
  f_ArgCheck(argc, argv);

  signal(SIGINT, signal_handler);
  signal(SIGQUIT, signal_handler2);

  system("clear");
  printf("\n--------------Initialisation Completed.--------------\n\n");

  if(pthread_create(&thread[j], NULL, &t_HardwareInput, NULL)){
    printf("ERROR; thread \"t_HardwareInput\" not created.");
  }  j++;


  if(pthread_create(&thread[j], NULL, &t_Wave1, NULL)){
    printf("ERROR; thread \"t_Wave1\" not created.");
  }  j++;

 /* if(pthread_create(&thread[j], NULL, &t_Wave2, NULL)){
    printf("ERROR; thread \"t_Wave2\" not created.");
  }  j++;

*/
  if(pthread_create(&thread[j], NULL, &t_ScreenOutput, NULL)){
    printf("ERROR; thread \"t_ScreenOutput\" not created.");
  }  j++;

  pthread_exit(NULL);
}

#/** PhEDIT attribute block
#-11:16777215
#0:11219:TextFont9:-3:-3:0
#11219:11323:TextFont9:0:-1:0
#11323:17519:TextFont9:-3:-3:0
#**  PhEDIT attribute block ends (-0000177)**/
