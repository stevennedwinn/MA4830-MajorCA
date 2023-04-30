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

#include <sys/resource.h>
#define	INTERRUPT	  iobase[1] + 0		// Badr1 + 0 : also ADC register
#define	MUXCHAN		  iobase[1] + 2		// Badr1 + 2
#define	TRIGGER		  iobase[1] + 4		// Badr1 + 4
#define	AUTOCAL		  iobase[1] + 6		// Badr1 + 6
#define DA_CTLREG	  iobase[1] + 8		// Badr1 + 8

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

#define numthreads	 4
#define steps	50
#define billion 1000000000L

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
float amplitude=1;
float average=1;
float frequency=1;

typedef struct {
  float amp,
        mean,
        freq;
} channel_para;   //store waveform parameters


//relabelling int datatype as wave_pt and wave_pt can store an array of ints of size steps
//so this is just an int array of 50 elements
typedef int wave_pt[steps];  //store points of waveforms with resolution steps

//initialise a pointer for datatype channel_para for variable ch
//to get address of the value stored in ch, printf(ch);
//*ch=1;  -->  *ch=1, then ch=address where 1 is stored,&ch=address of the pointer reference
channel_para *ch;

//create a 2D int array
wave_pt *wave_type;

//can be initialised as int *wave;
int wave[2];  //store wave type for each channel

//===================================
// Functions
//===================================


void PCIsetup(){
  struct pci_dev_info info;
  unsigned int i;
	// set all PCI_dev_info variables to 0 i.e. initialising
  memset(&info,0,sizeof(info));
  if(pci_attach(0)<0) {
    printf("\a");
    perror("pci_attach");
    exit(EXIT_FAILURE);
  }

  // Vendor and Device ID
  info.VendorId=0x1307;
  info.DeviceId=0x01;

  if ((hdl=pci_attach_device(0, PCI_SHARE|PCI_INIT_ALL, 0, &info))==0) {
    printf("\a");
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
    printf("\a");
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

float sineTable[steps];
const float delta = (2.0*3.142)/steps;
 
const float deltaSaw = 2.0/steps;
const float deltaTri = 4.0/steps;

//Sine wave array
for(i=0; i<steps; i++){
	sineTable[i]=  (sinf(i*delta));
	}
for(i=0; i<steps; i++){
	sineTable[i]=  (sinf(i*delta));
	wave_type[0][i] =( sineTable[i] * 0x7fff/5);
	}

//Square wave array
// if i less then or  equal to half the steps then dummy = 0x7fff else ~0x7fff
for(i=0;i<steps;i++){
	float dummy = (i<=steps/2)?0x7fff:-0x7fff;
	wave_type[1][i] =  dummy /5;
//Saw-tooth wave array
// if i less then or equal to steps/2 ,(true) i * deltasaw * 0x7fff, (False) (-2 + i * deltaSaw) * 0x7fff
	 dummy = (i <= steps / 2) ? i * deltaSaw * 0x7fff : (-2 + i * deltaSaw) * 0x7fff;
    wave_type[2][i] = dummy / 5;
//Triangular wave array
// if i <= steps/4 , (true)i * deltaTri * 0x7fff (else) check if (i <= steps * 3 / 4), (true) (2 - i * deltaTri) * 0x7fff , (False) (-4 + i * deltaTri) * 0x7fff
	 dummy = 	(i <= steps / 4) ? i * deltaTri * 0x7fff :
										(i <= steps * 3 / 4) ? (2 - i * deltaTri) * 0x7fff :
                        				(-4 + i * deltaTri) * 0x7fff;
	wave_type[3][i] = dummy / 5;
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
                delay(500);
                break;
            case 'a':
                amplitude = atof(argument_value);
                if(amplitude < 0.0 || amplitude > 5.0){
                  printf("\a");
                	printf("\nERROR: Amplitude must be between 0 and 5.\n");
                	printf("\nDefault amplitude is set to 1 instead\n");
                	amplitude = 1.00;
            	}
            	printf("\n%d. Amplitude: %f\n", i, amplitude);
            	delay(500);
                break;
            case 'f':
                frequency = atof(argument_value);
                if(frequency < 0.5 || frequency > 10.0){
                  printf("\a");
					printf("\nERROR: Frequency must be between 0.5 and 10.");
					printf("\nDefault frequency is set to 1 instead\n");
					frequency = 1.00;
            	}
                printf("\n%d. Frequency: %f\n", i, frequency);
                delay(500);
                break;
            case 'm':
                average = atof(argument_value);
                if(average < 0.0 || average >1.0){
                  printf("\a");
                	printf("\nERROR: Mean must be between 0 and 1.");
                	printf("\nDefault mean is set to 1 instead\n");
                	average = 1.00;
            	}
                printf("\n%d. Mean: %f\n", i, average);
                delay(500);
                break;
            default:
                printf("\a");
                printf("\nInvalid argument: %s\n", argv[i]);
                break;
        }
    }
  }
  if (argc==1) { //if no terminal argument is input, set sine wave as default waveform
    wave[0]=1;
    printf("No arguments are inserted. Values are set to default\n");
  }
  delay(2000);
}


void MemoryAllocation(void){
	// makes sure there is the space to store in pointer Ch 
	if((ch = (channel_para*)malloc(1 * sizeof(channel_para))) == NULL) {
    printf("\a");
	  printf("Not enough memory.\n");
}
  (*ch).amp  = amplitude;
  (*ch).mean = average;
  (*ch).freq = frequency;
// makes sure there is 4x the space to store in pointer wave_type for the 4 different wave generated. 
if((wave_type = (wave_pt*)malloc(4 * sizeof(wave_pt))) == NULL) {
  printf("\a");
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
  
  // unassign the memory of pointers ch and wave_type. 
  free((void *) ch);
  free((void *) wave_type);
  printf("Reset to Default Setting\nDetach PCI\nReleased DMA\n");

}

//number checker to ensure that it is within the limits.
int getInt(int lowlimit, int highlimit) {
    int outnum;
    char c;
	
    while (1) {
        printf("\nEnter an integer between %d and %d: \n", lowlimit, highlimit);
        if (scanf("%d", &outnum) != 1) {
            // The input is not a valid integer
            while ((c = getchar()) != '\n' && c != EOF) {}//loop forever until its not enter or EOF
            printf("\a");
            printf("Invalid input. Please enter an integer.\n");
            continue; // restart the loop in while 
        }
        if (outnum < lowlimit || outnum > highlimit) {
            printf("\a");
            printf("Your number should be within %d and %d. Please enter a valid number.\n", lowlimit, highlimit);
            continue; //restart the loop in while
        }
        while ((c = getchar()) != '\n' && c != EOF) {}//loop forever until its not enter or EOF
        return outnum;
    }
}

float getFloat(float lowlimit, float highlimit) {
    float outnum1 = 0.0;
    char c;
    printf("\nEnter a number between %.2f and %.2f\n", lowlimit, highlimit);
    while (1) {
        if (scanf("%f", &outnum1) != 1) {
            // The input is not a valid float
            while ((c = getchar()) != '\n' && c != EOF) {}//loop forever until its not enter or EOF
            printf("\a");
            printf("Invalid input. Please enter a number.\n");
            continue; // restart the loop in while 
        }
        if (outnum1 < lowlimit || outnum1 > highlimit) {
            printf("\a");
            printf("Your number should be within %f and %f. Please enter a valid number.\n", lowlimit, highlimit);
            continue; //restart the loop in while
        }
        while ((c = getchar()) != '\n' && c != EOF) {}//loop forever until its not enter or EOF
        return outnum1;
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
            printf("\a");
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
                return;// Return to main menu
            default:
                printf("\a");
                printf("Invalid input! Please try again.\n");
        }
    }
}

void changeFrequency() {
	float temp_freq;
	temp_freq = getFloat(0.5, 10.0);
	(*ch).freq = temp_freq;
}

void saveyourfile(char *filename, FILE *fp, char *data){
    strcat(filename, ".txt");
    printf("\nFile saving in progress, please wait...\n");

    if ((fp = fopen(filename, "w")) == NULL){
      printf("\a");
      perror("Cannot open\n\n");
      return;
    }
    if (fputs(data, fp) == EOF){
      printf("\a");
      perror("Cannot write\n\n");
      return;
    }
    fclose(fp);
    printf("File saved!\n\n");
	}

void saveyourfilePrompt() {
    const char *wave_str[] = {"Sine", "Square", "Sawtooth", "Triangular"};
    char filename[100]; // 100 character long filename
    char data[200];
	FILE *fp;
    int n;

    printf("\nYou have selected the option to save the output to a file.\n"
           "Please provide a file name, note that the extension '.txt' will be added):\n\n"
           "0. Return to Main Menu\n\n");

    n = scanf("%99s", filename);
    if (n != 1) {
        printf("\a");
        printf("Invalid input. Please enter a valid filename.\n");
        return;
    }

    if (strcmp(filename, "0") == 0) {
        return;  // Return to main menu
    }

    sprintf(data,
            "\t\t\t\t\t\tAmp. Mean Freq. Wave\n"
            "Ch Parameter:\t%2.2f\t%2.2f\t%2.2f\t%d.%s\n\n",
            (*ch).amp, (*ch).mean, (*ch).freq, wave[0], wave_str[wave[0]-1]);
    saveyourfile(filename,fp,data);
}


void readfile(char *filename, FILE *fp) {
    channel_para chtemp;
    int wavetemp,count;

    strcat(filename, ".txt");
    printf("\nFile reading in progress, please wait...\n");

    if ((fp = fopen(filename, "r")) == NULL) {
        printf("\a");
        perror("Cannot open file");
        return;
    }
	
    count = fscanf(fp, "%*[^C]Ch Parameter: %f %f %f %d",
      &chtemp.amp, &chtemp.mean, &chtemp.freq, &wavetemp
    ); //save float to address of chtemp.amp and others.

    if (count == 4) {
        wave[0] = wavetemp;
        (*ch).amp = chtemp.amp;
      (*ch).mean = chtemp.mean;
         (*ch).freq = chtemp.freq;
        printf("File Read Successfully\n\n");
    } else {
        printf("\a");
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

    unsigned int current[steps];
    struct timespec start, stop;
    double accum = 0;
    unsigned int i;
    
    while (true) {
        // Generate wave data for the current channel
        for (i = 0; i < steps; i++) {
            current[i] = (wave_type[wave[0]-1][i] * (*ch).amp)*0.6 + ((*ch).mean*0.8 + 1.2)*0x7fff/5 * 2.5 ;
            // Critical Formula
            // Scale and offset the wave data
        }

        // Send the wave data to the DA converter
        if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
            printf("\a");
            perror("clock gettime");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < steps; i++) {
            out16(DA_CTLREG, 0x0a23);   // DA Enable, #0, #1, SW 5V unipolar
            out16(DA_FIFOCLR, 0);       // Clear DA FIFO buffer
            out16(DA_Data, (short)current[i]);
            delay((1.0 / (*ch).freq*1000 - accum) / steps);
 // Critical Formula
        }
        
        if (clock_gettime(CLOCK_REALTIME, &stop) == -1) {
            printf("\a");
            perror("clock gettime");
            exit(EXIT_FAILURE);
        }
        
        accum = (double)(stop.tv_sec - start.tv_sec) + (double)(stop.tv_nsec - start.tv_nsec) / billion;
    }
}

void *ThreadforHardwareInput(void *arg){

   int mode;
   unsigned int count;

   struct timespec delay_time = {0,100000000}; //100ms delay
   
   while(1)  {
    dio_in=in8(DIO_PORTA); 					// Read Port A


    if( (dio_in & 0x08) == 0x08) {
      out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B
      if((dio_in & 0x04) == 0x04) {
        raise(SIGINT);
      }
      else if ((mode = dio_in & 0x03) != 0) {
        count=0x00;

        while(count <0x02) {
          chan= ((count & 0x0f)<<4) | (0x0f & count);
          out16(MUXCHAN,0x0D00|chan);		// Set channel	 - burst mode off.
          nanosleep(&delay_time, NULL);     // allow mux to settle
          out16(AD_DATA,0); 				// start ADC
          while(!(in16(MUXCHAN) & 0x4000));
          adc_in[(int)count]=in16(AD_DATA);
          count++;
          nanosleep(&delay_time, NULL); 	// Write to MUX register - SW trigger, UP, DE, 5v, ch 0-7
        }
      }

      switch ((int)mode) {
       case 1:
        (*ch).freq = (float)adc_in[0] * 9.5 / 0xffff + 0.5; //scale from 16 bits to 0.5 ~ 10
        break;
        case 2:
        (*ch).amp = (float)adc_in[0] * 5.00 / 0xffff; //scale from 16 bits to 0 ~ 5
        break;
        case 3:
      (*ch).mean = (float)adc_in[0] * 1.00 / 0xffff; //scale from 16 bits to 0.00 ~ 1.00
        break;
      }
    }	// end if keyboard
    nanosleep(&delay_time, NULL);
  } //end while
} //end thread

void *MainPageOutput(void* arg){
  delay(100);
  printf("\nTo Exit Program, press Ctrl + C\nTo Enter Keyboard Menu, press Ctrl + Z \n");
  printf("+--------------------------------------+\n");
  printf("|           WAVEFORM GENERATOR         |\n");
  printf("+--------------------------------------+\n");
  printf("|            Real Time Inputs          |\n");
  printf("+------------+------------+------------+\n");
  printf("|    Amp     |    Mean    |    Freq    |\n");
  printf("+------------+------------+------------+\n");
  while(1){
    printf("\r|  %6.2f    |  %6.2f    |  %6.2f    |\n", ch[0].amp , ch[0].mean, ch[0].freq);
    printf("+------------+------------+------------+\033[A");
    fflush(stdout);
    delay(100);
  }
  
}


void *userinterface() {
    int input;
    bool running = true;

    // Stop a possible bug by cancelling the screen output thread
    if (pthread_cancel(thread[2]) == 0) {
        while (running) {
      printf("\n");
	  printf("\n+---------------------------------------------+");
      printf("\n|                   SETTINGS                  |");
      printf("\n+---------------------------------------------+");
      printf("\n|                   MAIN MENU                 |");
      printf("\n| Please select an action (1-6):              |");
      printf("\n| 1. Change Waveform Settings                 |");
      printf("\n| 2. Change Frequency of the wave             |");
      printf("\n| 3. Save Current Settings to a File          |");
      printf("\n| 4. Read a File and Load Settings            |");
      printf("\n| 5. Return to Main Page                      |");
      printf("\n| 6. End the Program                          |");
      printf("\n+---------------------------------------------+\n\n");
            
            input = getInt(1, 6);

            switch (input) {
                case 1:
                    changeWaveform();
                    break;
                case 2:
            		changeFrequency();
                    break;
                case 3:
                    saveyourfilePrompt();
                    break;
                case 4:
                    if ((dio_in & 0x08) == 0x08) {
                        printf("\a");
                        printf("\n\nPlease switch off first toggle switch\n\n");
                        delay(1000);
                    }
                    else {
                        readFilePrompt();
                    }
                    break;
                case 5:
                    // Clear console screen
                    system("clear");
                    if (pthread_create(&thread[2], NULL, &MainPageOutput, NULL)) {
                        printf("\a");
                        printf("ERROR: thread \"MainPageOutput\" not created.");
                    }
                    running = false;
                    break;
                case 6:
                    printf("\nTerminating program.\n");
                    raise(SIGINT);
                    break;
            }
        }
    }
}


//=================================
//signalHandler
//=================================

void signalHandler() {
	int i;
    printf("\n\n\nHardware Termination Raised\n");

    terminate();

    // Cancel all threads except for the current one
    for (i = 0; i < numthreads; i++) {
        if (pthread_self() != thread[i]) {
            pthread_cancel(thread[i]);
            printf("Thread %ld is killed.\n", thread[i]);
        }
    }
    // Exit the current thread
    printf("Thread %ld is killed.\n", pthread_self());
    pthread_exit(NULL);
 
}


void signalHandler2(){
  pthread_create(NULL, NULL, &userinterface, NULL);
}

//==================================
//MAIN
//==================================

int main(int argc, char* argv[]) {

  struct rlimit rlim;
  int j=0; //thread count
  rlim.rlim_cur=0;
  rlim.rlim_max=0;
  setrlimit(RLIMIT_CORE,&rlim);
  PCIsetup();
  checkArgs(argc, argv);
  MemoryAllocation();
  WaveGeneration();
  
  signal(SIGINT, signalHandler);

  signal(SIGTSTP, signalHandler2);

  system("clear");
  printf("\n=============CA2 Assignment===============\n\n");

  if(pthread_create(&thread[j], NULL, &ThreadforHardwareInput, NULL)){
    printf("\a");
    printf("ERROR; thread \"ThreadforHardwareInput\" not created.");
  }  j++;


  if(pthread_create(&thread[j], NULL, &ThreadWave, NULL)){
    printf("\a");
    printf("ERROR; thread \"ThreadWave\" not created.");
  }  j++;

  if(pthread_create(&thread[j], NULL, &MainPageOutput, NULL)){
    printf("\a");
    printf("ERROR; thread \"MainPageOutput\" not created.");
  }  j++;

  pthread_exit(NULL);
}

