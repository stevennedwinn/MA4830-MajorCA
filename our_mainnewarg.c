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
#define 	DA_CTLREG	  iobase[1] + 8		// Badr1 + 8

#define	AD_DATA	  	  iobase[2] + 0		// Badr2 + 0
#define	AD_FIFOCLR	  iobase[2] + 2		// Badr2 + 2

#define	TIMER0			  iobase[3] + 0		// Badr3 + 0
#define	TIMER1		 	  iobase[3] + 1		// Badr3 + 1
#define	TIMER2		 	  iobase[3] + 2		// Badr3 + 2
#define	COUNTCTL		  iobase[3] + 3		// Badr3 + 3
#define	DIO_PORTA	  iobase[3] + 4		// Badr3 + 4
#define	DIO_PORTB	  iobase[3] + 5		// Badr3 + 5
#define	DIO_PORTC	  iobase[3] + 6		// Badr3 + 6
#define	DIO_CTLREG	  iobase[3] + 7		// Badr3 + 7
#define	PACER1		  	  iobase[3] + 8		// Badr3 + 8
#define	PACER2		  	  iobase[3] + 9		// Badr3 + 9
#define	PACER3		  	  iobase[3] + a		// Badr3 + a
#define	PACERCTL	  	  iobase[3] + b		// Badr3 + b

#define 	DA_Data		  iobase[4] + 0		// Badr4 + 0
#define	DA_FIFOCLR    iobase[4] + 2		// Badr4 + 2

#define BILLION 1000000000L
#define MILLION 1000000L
#define THOUSAND	1000L
#define numthreads	 4
#define steps	50
//#define amplitude  1    //default wave parameters
//#define average 1
//#define frequency 1
#define duty_cycle 50

//===================================
// Global Variable
//===================================
int badr[5];			//PCI 2.2 assigns 6 IO base addresses
void *hdl;
uintptr_t dio_in;
uintptr_t iobase[6];
uint16_t adc_in[2];
int chan;
pthread_t thread[numthreads];
char Select_c[10];
int Select;
float amplitude=1.0;
float average=1.0;
float frequency=1.0;

typedef struct {
  float amp,
        mean,
        freq;
} channel_para;   //store waveform parameters

typedef int wave_pt[steps];  //store points of wavefomrs with resolution steps

channel_para *ch;
wave_pt *wave_type;

int wave[2];  //store wave type for each channel

//===================================
// Functions
//===================================


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

void WaveGeneration(){
int i;
float dummy;

float sineTable[steps];
const float delta = (2.0*3.142)/steps;
 
const float deltaSaw = 2.0/steps;
const float deltaTri = 4.0/steps;

//Sine wave array
for(i=0; i<steps; i++){
	sineTable[i]=  (sinf(i*delta));
	}
for(i=0; i<steps; i++){
	wave_type[0][i] =( sineTable[i] * 0x7fff/5);
	}

//Square wave array
//the value of 2 in dummy when i<steps/2 indicates the value of 'ON'
//the value of 0 in dummy when i<steps indicates the value of 'OFF'
for(i=0;i<steps;i++){
	const float dummy = (i<=steps/2)?0x7fff:-0x7fff;
	wave_type[1][i] =  dummy /5;
	}
	
//Saw-tooth wave array
//the delta used is similar to the one used for sine wave
//the dummy is increased by multiple of delta when i <steps/2
//then the dummy is decrease by multiple of delta when i<steps
//the value of '1' in dummy when i<steps indicates the max value of wave form when i=steps/2
for ( i = 0; i < steps; i++) {
	const float dummy = (i <= steps / 2) ? i * deltaSaw * 0x7fff : (-2 + i * deltaSaw) * 0x7fff;
    wave_type[2][i] = dummy / 5;
    }

//Triangular wave array
//the value of 2 in delta indicates the max vertical value of the wave form when i = steps-1, the value is closed to 2
//the min vertical value of wave form is 0 when i = 0

for ( i = 0; i < steps; i++) {
	const float dummy = 	(i <= steps / 4) ? i * deltaTri * 0x7fff :
										(i <= steps * 3 / 4) ? (2 - i * deltaTri) * 0x7fff :
                        				(-4 + i * deltaTri) * 0x7fff;
	wave_type[3][i] = dummy / 5;
	}
}


void checkArgs(int argc, char* argv[]){
int i;
char **p_to_arg = &argv[1];
printf("Reading command line arguments...")
;
delay(100);
if(argc>=2){
  for(i=1;i<argc;i++,p_to_arg++){
    if(strcmp(*(p_to_arg),"-sine")==0){
      printf("\n%d. Sine wave chosen.\n",i);
      wave[i-1]=1;
    }
    else if(strcmp(*(p_to_arg),"-square")==0){
      printf("\n%d. Square wave chosen.\n",i);
      wave[i-1]=2;
    }
    else if(strcmp(*(p_to_arg),"-saw")==0){
      printf("\n%d. Saw wave chosen.\n",i);
      wave[i-1]=3;
    }
    else if(strcmp(*(p_to_arg),"-tri")==0){
      printf("\n%d. Triangular wave chosen.\n",i);
      wave[i-1]=4;
    }
    else {
      fprintf(stderr, "Error: Unknown argument \"%s\".\n", argv[i]);
      exit(EXIT_FAILURE);
    }
 
    }//end of for loop, checking argv[]
  }
  else if (argc==1) { //if no terminal argument is input, set sine wave as default waveform
    wave[0]=1; 
  }
}



void checkArgs(int argc, char* argv[]){
  int i;
  char argument;
  char* argument_value;
  printf("Reading command line arguments...\n");

  if(argc>=2){
    for (i = 1; i < argc; i++) {
        argument = argv[i][1];
        argument_value = &(argv[i][2]);

        switch (argument) {
            case 't':
                if (strcmp(argument_value, "sine") == 0) {
                    printf("\n%d. Sine wave chosen.\n", i);
                    wave[0] = 1;
                }
                else if (strcmp(argument_value, "square") == 0) {
                    printf("\n%d. Square wave chosen.\n", i);
                    wave[0] = 2;
                }
                else if (strcmp(argument_value, "saw") == 0) {
                    printf("\n%d. Saw wave chosen.\n", i);
                    wave[0] = 3;
                }
                else if (strcmp(argument_value, "tri") == 0) {
                    printf("\n%d. Triangular wave chosen.\n", i);
                    wave[0] = 4;
                }
                break;
            case 'a':
                amplitude = atof(argument_value);
                if(amplitude < 0.0 || amplitude > 5.0){
                	printf("\nERROR: Amplitude must be between 0 and 5.\
                	\nDefault amplitude is set to 1 instead\n");
                	amplitude = 1.00;
            	}
            	printf("\n%d. Amplitude: %f\n", i, amplitude);
                break;
            case 'f':
                frequency = atof(argument_value);
                if(frequency < 0.0 || frequency > 5.0){
                	printf("\nERROR: Frequency must be between 0 and 5.\
                	\nDefault frequency is set to 1 instead\n");
                	frequency = 1.00;
            	}
                printf("\n%d. Frequency: %f\n", i, frequency);
                break;
            case 'm':
                average = atoi(argument_value);
                if(average < 0.0 || average >2.0){
                	printf("\nERROR: Mean must be between 0 and 2.\
                	\nDefault mean is set to 1 instead\n");
                	average = 1.00;
            	}
                printf("\n%d. Mean: %f\n", i, average);
                break;
            default:
                printf("\nInvalid argument: %s\n", argv[i]);
                break;
        }
    }
  }
  if (argc==1) { //if no terminal argument is input, set sine wave as default waveform
    wave[0]=1;
    printf("No arguments are inserted. Values are set to default\n");
  }
  delay(3000);
}

void MemoryAllocation(void){
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

//Reset DAC to 5V
void terminate(){
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

int getInt(int lowlimit, int highlimit) {
    int outnum;
    char c;

    while (1) {
        printf("Enter an integer between %d and %d: ", lowlimit, highlimit);
        if (scanf("%d", &outnum) != 1) {
            // The input is not a valid integer
            while ((c = getchar()) != '\n' && c != EOF) {}
            printf("Invalid input. Please enter an integer.\n");
            continue;
        }
        if (outnum < lowlimit || outnum > highlimit) {
            printf("Your number should be within %d and %d. Please enter a valid number.\n", lowlimit, highlimit);
            continue;
        }
        while ((c = getchar()) != '\n' && c != EOF) {}
        return outnum;
    }
}


void changeWaveform() {
    const char *wave_str[] = {"Sine", "Square", "Sawtooth", "Triangular"};
    int Select;

    while (1) {
        printf("Select a number 1-4:\n"
               "1. Sine Wave\n"
               "2. Square Wave\n"
               "3. Sawtooth Wave\n"
               "4. Triangular Wave\n\n"
               "0. Return to Main Menu\n\n");

        if (scanf("%d", &Select) != 1) {
            // The input is not a valid integer
            char c;
            while ((c = getchar()) != '\n' && c != EOF) {}
            printf("Invalid input. Please enter an integer.\n");
            continue;
        }

        switch (Select) {
            case 0:
                return;  // Return to main menu
            case 1:
            case 2:
            case 3:
            case 4:
                wave[0] = Select;
                printf("\n%s Wave Selected\n\n", wave_str[wave[0]-1]);
                return;
            default:
                printf("Invalid input! Please try again.\n");
        }
    }
}


void saveyourfile(char *filename, FILE *fp, char *data){
    strcat(filename, ".txt");
    printf("\nFile saving in progress, please wait...\n");

    if ((fp = fopen(filename, "w")) == NULL){
      perror("Cannot open\n\n");
      return;
    }
    if (fputs(data, fp) == EOF){
      perror("Cannot write\n\n");
      return;
    }
    fclose(fp);
    printf("File saved!\n\n");
	}

void saveyourfilePrompt() {
    const char *wave_str[] = {"Sine", "Square", "Sawtooth", "Triangular"};
    char filename[100];
    char data[200];
	  FILE *fp;
    int n;

    printf("\nYou have selected the option to save the output to a file. \n"
           "Please provide a file name, note that the extension '.txt' will be added. \n\n"
           "0. Return to Main Menu\n\n");

    n = scanf("%99s", filename);
    if (n != 1) {
        printf("Invalid input. Please enter a valid filename.\n");
        return;
    }

    if (strcmp(filename, "0") == 0) {
        return;  // Return to main menu
    }

    sprintf(data,
            "\t\t\t\t\tAmp. Mean Freq. Wave\n"
            "Channel Parameter:\t%2.2f\t%2.2f\t%2.2f\t%d.%s\n\n",
            ch[0].amp, ch[0].mean, ch[0].freq, wave[0], wave_str[wave[0]-1]);

    saveyourfile(filename,fp,data);
}


void readfile(char *filename, FILE *fp) {
    channel_para temp_ch;
    int temp_wave,count;

    strcat(filename, ".txt");
    printf("\nFile reading in progress, please wait...\n");

    if ((fp = fopen(filename, "r")) == NULL) {
        perror("Cannot open file");
        return;
    }

    count = fscanf(fp, "%*[^C]Channel Parameter: %f %f %f %d",
      &temp_ch.amp, &temp_ch.mean, &temp_ch.freq, &temp_wave
    );

    if (count == 4) {
        wave[0] = temp_wave;
        ch[0].amp = temp_ch.amp;
        ch[0].mean = temp_ch.mean;
        ch[0].freq = temp_ch.freq;
        printf("File Read Successfully\n\n");
    } else {
        printf("File Read Fail\n\n");
    }

    fclose(fp);
}

void readFilePrompt() {
    char filename[100];
    FILE *fp;

    printf("\n\nYou have indicated to read the file."
      "\nPlease name your file(.txt):\n\n"
      "0. Return Main Menu\n\n");

    scanf("%s", filename);

    //return main menu if input = 0
    if (strcmp(filename, "0") == 0)
      return;

    readfile(filename, fp);
}


//==================================
// P_Threads
//==================================


void *ThreadWave(void* arg) {
    const int channelIndex = 0; // We only use one channel

    unsigned int current[steps];
    struct timespec start, stop;
    double accum = 0;
    unsigned int i;
    
    while (true) {
        // Generate wave data for the current channel
        for (i = 0; i < steps; i++) {
            current[i] = (wave_type[wave[channelIndex]-1][i] * ch[channelIndex].amp)*0.6
 + (ch[channelIndex].mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
 // Critical Formula
            // Scale and offset the wave data
        }

        // Send the wave data to the DA converter
        if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
            perror("clock gettime");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < steps; i++) {
            out16(DA_CTLREG, 0x0a23);   // DA Enable, #0, #1, SW 5V unipolar
            out16(DA_FIFOCLR, 0);       // Clear DA FIFO buffer
            out16(DA_Data, (short)current[i]);
            delay((1.0 / ch[channelIndex].freq*1000 - accum) / steps);
 // Critical Formula
        }
        
        if (clock_gettime(CLOCK_REALTIME, &stop) == -1) {
            perror("clock gettime");
            exit(EXIT_FAILURE);
        }
        
        accum = (double)(stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / BILLION;
    }
}


void *ThreadforHardwareInput(void *arg){

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
        break;
        case 2:
        ch[0].freq = (float)adc_in[0] * 4.50 / 0xffff+0.5; //scale from 16 bits to 0.5 ~ 5
        break;
        case 3:
        ch[0].mean = (float)adc_in[0] * 2.00 / 0xffff; //scale from 16 bits to 0.00 ~ 2.00
        break;
      }
    }	// end if keyboard
    delay(100);
  } //end while
} //end thread

void *MainPageOutput(void* arg){
  delay(100);
  printf("\nTo Exit Program, press Ctrl + C\nTo Enter Keyboard Menu, press Ctrl + \\ \n");
  printf("+--------------------------------------+\n");
  printf("|               Main Page              |\n");
  printf("+--------------------------------------+\n");
  printf("|            Real Time Inputs          |\n");
  printf("+------------+------------+------------+\n");
  printf("|    Amp     |    Mean    |    Freq    |\n");
  printf("+------------+------------+------------+\n");
  while(1){
    printf("\r|  %6.2f    |  %6.2f    |  %6.2f    |\n", ch[0].amp , ch[0].mean, ch[0].freq);
    printf("+------------+------------+------------+\n");
    fflush(stdout);
    delay(100);
  }
}

void *userinterface(){
  int input;

  //to stop a bug from happening
  if(pthread_cancel(thread[2]) == 0)

    while(true){
      printf("\n\
              \n+---------------------------------------------+");
      printf("\n|               WAVEFORM GENERATOR            |");
      printf("\n+---------------------------------------------+");
      printf("\n|                   MAIN MENU                 |");
      printf("\n| Please select an action (1-5):              |");
      printf("\n| 1. Change Waveform Settings                 |");
      printf("\n| 2. Save Current Settings to a File          |");
      printf("\n| 3. Read a File and Load Settings            |");
      printf("\n| 4. Return to Main Page                      |");
      printf("\n| 5. End the Program                          |");
      printf("\n+---------------------------------------------+\n\n");
      input = getInt(1, 5);

      if (input == 1){
        changeWaveform();
      }else if (input == 2){
        saveyourfilePrompt();
      }else if (input == 3){
       if((dio_in & 0x08) == 0x08){
        printf("\n\nPlease switch off first toggle switch\n\n");
        delay(1000);
       }
       else
        readFilePrompt();
    }else if (input == 4){
      //clear console screen
      system("clear");
      if(pthread_create(&thread[2], NULL, &MainPageOutput, NULL)){
        printf("ERROR; thread \"MainPageOutput\" not created.");
      }
      break;
    }else if (input == 5){
      printf("\nBye bye\nHope to see you again soon.\n");
      raise(SIGINT);
    }
  }
}
// End Keyboard

//=================================
//SIGNAL_HANDLER
//=================================

void signal_handler(){
  int t;
  pthread_t temp;

  temp = pthread_self();
  printf("\nHardware Termination Raised\n");

  terminate();

  for(t = 3; t >= 0; t--){
   if(thread[t] != temp) {
    pthread_cancel(thread[t]);
    printf("Thread %d is killed.\n",thread[t]);
  }
  }// for loop
  printf("Thread %d is killed.\n", temp);
  pthread_exit(NULL);
  delay(500);
}//end of SH


void signal_handler2(){
  pthread_create(NULL, NULL, &t_UserInterface, NULL);
}

//==================================
//MAIN
//==================================

int main(int argc, char* argv[]) {

  int j=0; //thread count
  f_PCIsetup();
  checkArgs(argc, argv);
  MemoryAllocation();
  WaveGeneration();
  

  signal(SIGINT, signal_handler);
  signal(SIGQUIT, signal_handler2);

  system("clear");
  printf("\n===========CA2 Assignment=============\n\n");

  if(pthread_create(&thread[j], NULL, &ThreadforHardwareInput, NULL)){
    printf("ERROR; thread \"ThreadforHardwareInput\" not created.");
  }  j++;


  if(pthread_create(&thread[j], NULL, &ThreadWave, NULL)){
    printf("ERROR; thread \"ThreadWave\" not created.");
  }  j++;

  if(pthread_create(&thread[j], NULL, &MainPageOutput, NULL)){
    printf("ERROR; thread \"MainPageOutput\" not created.");
  }  j++;

  pthread_exit(NULL);
}

#/** PhEDIT attribute block
#-11:16777215
#0:1586:TextFont9:-3:-3:0
#1586:1623:TextFont9:0:-1:0
#1623:1644:TextFont9:-3:-3:0
#1644:1681:TextFont9:0:-1:0
#1681:1889:TextFont9:-3:-3:0
#1889:1943:TextFont9:0:-1:0
#1943:2232:TextFont9:-3:-3:0
#2232:2269:TextFont9:0:-1:0
#2269:3693:TextFont9:-3:-3:0
#3693:5237:TextFont9:0:-1:0
#5237:7331:TextFont9:-3:-3:0
#7331:7555:TextFont9:0:-1:0
#7555:7569:TextFont9:-3:-3:0
#7569:7616:TextFont9:0:-1:0
#7616:7922:TextFont9:-3:-3:0
#7922:7939:TextFont9:0:-1:0
#7939:8132:TextFont9:-3:-3:0
#8132:8339:TextFont9:0:-1:0
#8339:8762:TextFont9:-3:-3:0
#8762:8769:TextFont9:0:-1:0
#8769:8899:TextFont9:-3:-3:0
#8899:8922:TextFont9:0:-1:0
#8922:9718:TextFont9:-3:-3:0
#9718:9738:TextFont9:0:-1:0
#9738:9761:TextFont9:-3:-3:0
#9761:9779:TextFont9:0:-1:0
#9779:10762:TextFont9:-3:-3:0
#10762:10763:TextFont9:0:-1:0
#10763:11891:TextFont9:-3:-3:0
#11891:11910:TextFont9:0:-1:0
#11910:13254:TextFont9:-3:-3:0
#13254:13260:TextFont9:0:-1:0
#13260:13365:TextFont9:-3:-3:0
#13365:13383:TextFont9:0:-1:0
#13383:13512:TextFont9:-3:-3:0
#13512:13522:TextFont9:0:-1:0
#13522:14170:TextFont9:-3:-3:0
#14170:14182:TextFont9:0:-1:0
#14182:14208:TextFont9:-3:-3:0
#14208:14220:TextFont9:0:-1:0
#14220:14296:TextFont9:-3:-3:0
#14296:14301:TextFont9:0:-1:0
#14301:15257:TextFont9:-3:-3:0
#15257:15265:TextFont9:0:-1:0
#15265:15288:TextFont9:-3:-3:0
#15288:15337:TextFont9:0:-1:0
#15337:15338:TextFont9:-3:-3:0
#15338:15374:TextFont9:0:-1:0
#15374:15378:TextFont9:-3:-3:0
#15378:15394:TextFont9:0:-1:0
#15394:15470:TextFont9:-3:-3:0
#15470:15581:TextFont9:0:-1:0
#15581:15713:TextFont9:-3:-3:0
#15713:15864:TextFont9:0:-1:0
#15864:15865:TextFont9:-3:-3:0
#15865:15873:TextFont9:0:-1:0
#15873:19631:TextFont9:-3:-3:0
#19631:19666:TextFont9:0:-1:0
#19666:20157:TextFont9:-3:-3:0
#20157:20193:TextFont9:0:-1:0
#20193:20325:TextFont9:-3:-3:0
#20325:20348:TextFont9:0:-1:0
#20348:21006:TextFont9:-3:-3:0
#**  PhEDIT attribute block ends (-0001955)**/
