#include "wiringPi.h" // GPIO library
#include "softTone.h" // For playing tones on buzzers

#include <stdio.h> // For printf, fprintf, etc.
#include <stdlib.h> // Standard library
#include <math.h> // For frequency analysis
#include <stdint.h>

#include <fcntl.h>
#include <unistd.h> 
#include <linux/watchdog.h> // For watchdog
#include <sys/ioctl.h> // For watchdog
#include <time.h> // To get system time
#include <sys/time.h> // To get system time
#include <signal.h> // To catch Ctrl+C interruptions
#include <string.h> // To process log file location in config

// Name of the program
#define PROGRAM_NAME "pi_ano"


/* Name: pi_ano
 * Description: A program that uses a raspberry pi's GPIO pins, some piezo buzzers, and a key matrix to simulate a piano.
 * Although features are limited (can only support up to 4 notes at the same time, can't adjust volume), it is still functional.
 * Authors: Andrew Peng, Braden Baaker, Michael Zhu
 * Date: Dec. 1, 2018
 */

 
// List of input pins and output pins. 
// Output pins will be pulsed, and represent the column.
// Input pins will return data once pulsed, representing the active rows in that column.
// Buzzers pins are the pins for the buzzers.

const int inputPins[4] = { 11, 13, 19, 26 };
const int outputPins[4] = { 12, 16, 20, 21 };
const int buzzerPins[4] = { 14, 15, 18, 23 };

// Key mapping to assigned key number. access like this: keys[row][column]; ex. keys[1][2] returns 6
// 0 and 1 are octave up and down keys respectively, while 2-14 are the piano keys. 15 is unused.
const int keys[4][4] = { {0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15} };

// Array to keep track of which keys are active (pressed down)
int activeKeyMatrix[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };

// Array to keep track of which buzzers are playing
int activeBuzzerMatrix[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };
int activeBuzzers[4] = {0, 0, 0, 0};

// Buzzer counter (max 4)
int buzzerCount = 0;

// Global octave counter (default)
int currentOctave = 4;

// Global watchdog timer (default)
int watchDogTimer = 10;


// Global variable to catch keyboard interrupts
static volatile int keepRunning = 1;

// Size of time char array
#define TIME_SIZE 30

// Define rows and cols
#define MATRIX_ROWS 4
#define MATRIX_COLS 4
#define MAX_BUZZERS 4

// Macro to both print a message to a file.
#define LOG_MSG(file, programName, time, str) \
	do{ \
			fprintf(file, "[%s][%s]: %s\n", programName, time, str); \
			fflush(file); \
	}while(0);

int pulseEntry(int col, int row);
void initPins();
void clearFrequency(int buzzerPin, FILE* logFile, char time[TIME_SIZE]);
void playFrequency(int key, int octave, int buzzerPin, FILE* logFile, char time[TIME_SIZE]);
void disableBuzzer(int pin, FILE* logFile, char time[TIME_SIZE]);
void getTime(char* buffer);
void generateNewConfig();
void setValuesFromConfig(FILE* configFile, int* octave, int* wdTimer, char* logFileName);
int keyToFreq(int key, int octave);
void updateKeys(int updatedMatrix[MATRIX_ROWS][MATRIX_COLS], FILE* logFile, char time[TIME_SIZE]);
void interruptHandler(int sig);

	
/*
* ============================
* HARDWARE DEPENDENT FUNCTIONS
* ============================
*/


// Returns the value of a column in a row of the physical key matrix.
// Takes ~3 ms to process.
int pulseEntry(int col, int row){
	// Pulse the column (set to high)
	digitalWrite(outputPins[col], HIGH);
	
	// Wait 3 ms to make sure the results are accurate.
	// If we read the values too fast the pins won't have time to react to the pulse.
	delay(3);
	
	// Read the row value at that point
	int rowValue = digitalRead(inputPins[row]);
		
	// "Unpulse" (set to low)
	digitalWrite(outputPins[col], LOW);
	
	return rowValue;
	
}


// Initializes GPIO pins for the matrix, buzzers, and octave control buttons.
void initPins(){
	
	// Init the four input pins (matrix)
	for (int i = 0; i < MATRIX_ROWS; i++){
		pinMode(inputPins[i], INPUT);
		digitalWrite(inputPins[i], PUD_UP);
	}
	// Init the four output pins (matrix)
	for (int i = 0; i < MATRIX_COLS; i++){
		pinMode(outputPins[i], OUTPUT);
		digitalWrite(outputPins[i], LOW);
	}
	// Init buzzer pins as output pins
	for (int i = 0; i < MAX_BUZZERS; i++){
		softToneCreate(buzzerPins[i]);
	}
	
	
}

// Stops a buzzer from playing sound
void clearFrequency(int buzzerPin, FILE* logFile, char time[TIME_SIZE]){
	
	softToneWrite(buzzerPin, 0);
	
	// Log to file
    char str[] = "Stopped playing on buzzer pin %d";
	char str2[1000];
	sprintf(str2, str, buzzerPin);
			
	LOG_MSG(logFile, PROGRAM_NAME, time, str2);
}

// Plays a frequency to a buzzer, based on what key was pressed, and what octave
void playFrequency(int key, int octave, int buzzerPin, FILE* logFile, char time[TIME_SIZE]){
	// Octave must be between 1 and 7 (max 5000hz)
	if (octave > 0 && octave < 8){
		
		int frequency = keyToFreq(key, octave);
		
		// Quick check. This method shouldn't be called if 
		if (buzzerCount < 4){
			
			softToneWrite(buzzerPin, frequency);
			
			// Log to file
			char str[] = "Played frequency %d to buzzer pin %d";
			char str2[1000];
			sprintf(str2, str, frequency, buzzerPin);
			
			LOG_MSG(logFile, PROGRAM_NAME, time, str2);
			
		}

	}
}

// Disables a buzzer
void disableBuzzer(int pin, FILE* logFile, char time[TIME_SIZE]){
	
	// Set frequency of buzzer to 0
	clearFrequency(pin, logFile, time);
	for (int i = 0; i < MAX_BUZZERS; i++){
	    if (activeBuzzers[i] == pin){
			activeBuzzers[i] = 0;
		}
	}
	buzzerCount--;
}

/*
* ============================
* HARDWARE INDEPENDENT FUNCTIONS
* ============================
*/

//This function will get the current time using the gettimeofday function
void getTime(char* buffer) {
	//Create a timeval struct named tv
  	struct timeval tv;

	//Create a time_t variable named curtime
  	time_t curtime;


	//Get the current time and store it in the tv struct
  	gettimeofday(&tv, NULL); 

	//Set curtime to be equal to the number of seconds in tv
  	curtime=tv.tv_sec;

	//This will set buffer to be equal to a string that in
	//equivalent to the current date, in a month, day, year and
	//the current time in 24 hour notation.
  	strftime(buffer,30,"%m-%d-%Y | %T",localtime(&curtime));

}

// Generates a default configuration file
void generateNewConfig(){
	FILE* newConfigFile = fopen("/home/pi/pi_ano.cfg", "a");
	fprintf(newConfigFile, "#This configuration file is to store the program's octave (1-4), the timeout of the watchdog timer (1-15 seconds), and the log file location.\n");
	fprintf(newConfigFile, "#Note: The log file's location will ignore colons and spaces.\n");
	fprintf(newConfigFile, "initialOctave: 4\n");
	fprintf(newConfigFile, "watchDogTimer: 10\n");
	fprintf(newConfigFile, "logFileLocation: /home/pi/pi_ano.log\n");
	fclose(newConfigFile);
}

// Sets octave and watchdog timer values from a configuration file.
void setValuesFromConfig(FILE* configFile, int* octave, int* wdTimer, char* logFileName){
	// Buffer for the file
	char buffer[255];
	
	// Default values
	*octave = 0;
	*wdTimer = 0;
	
	// Counter to keep track of what we are looking for (first octave, then timer)
	int value = 0;


	char lastChar = 0;
	// Keep looping until end of file
	while (fgets(buffer, 255, configFile) != NULL){
		int i = 0; // Current character index
		
		// Comment line, ignore
		if (buffer[i] != '#'){
			
			while (buffer[i] != 0){
				// last char was a colon, and current is a space, so that means we're at our first value (octave)
				if (buffer[i] == ' ' && lastChar == ':' && value == 0){
				
					// Keep looping until null terminator (end of line)
					while(buffer[i] != 0) {
					
						// If the character is a number from 0 to 9
						if(buffer[i] >= '0' && buffer[i] <= '9') {
						
							// Add digit
							*octave = (*octave * 10) + (buffer[i] - '0');
						}
						lastChar = buffer[i];
						i++;
					}
					value++;
				
				}else if (buffer[i] == ' ' && lastChar == ':' && value == 1){ // We're at the watchdog timer value
				
					// Keep looping until null terminator (end of line)
					while(buffer[i] != 0) {
					
						// If the character is a number from 0 to 9
						if(buffer[i] >= '0' && buffer[i] <= '9') {
						
							// Add digit
							*wdTimer = (*wdTimer * 10) + (buffer[i] - '0');
						}
						lastChar = buffer[i];
						i++;
					}
					value++;
				
				}else if (buffer[i] == ' ' && lastChar == ':' && value == 2){
					
					int j = 0;
					// Keep looping until null terminator or new line (end of line)
					while(buffer[i] != 0 && buffer[i] != '\n') {
					
						// If the character is not a colon, or a space, then add it
						if(buffer[i] != ':' && buffer[i] != ' ') {
							logFileName[j] = buffer[i];
							j++;
						}
						lastChar = buffer[i];
						i++;
					}
					value++;
					logFileName[j] = 0;
					
				}else{
					lastChar = buffer[i];
					i++;
				}				
			}
		}
	}
	
	if (*octave < 1 || *octave > 4){
		*octave = 4;
		printf("[WARNING] Initial octave in the configuration file was invalid. Using default (4) instead!\n");
	}
	
	if (*wdTimer < 1 || *wdTimer > 15){
		*wdTimer = 10;
		printf("[WARNING] Watchdog Timer value in the configuration file was invalid. Using default (10) instead!\n");
	}

}

// Given a key and an octave, this function returns the related frequency.
int keyToFreq(int key, int octave){
	
	// Since we're calculating with A4 = 440 hz, 
	// this gives the factor of what we should multiply the frequency by
	int trueOctave = octave - 4;
		
	// The formula with A4 = 440hz. This assumes that when key = 2, it is C4, and key = 14 is C5
	double basis = pow(2, (double) (1.0 / 12.0));
	
	// Add 0.5 to round (casting to int truncates, so adding 0.5 would make it round)
	int frequency = (int) (((440 * pow(basis, key - 11)) * pow(2, trueOctave)) + 0.5);
	
	return frequency;
}

// Updates the current active key matrix with another one (most likely the physical one).
// If a change is detected, it represents a key was pressed/released
// Requires a pointer to a logFile and a time char array to log down octave changes and key presses.

void updateKeys(int updatedMatrix[MATRIX_ROWS][MATRIX_COLS], FILE* logFile, char time[TIME_SIZE]){
	
	// Compare each individual entry in the matrices
	for (int i = 0; i < MATRIX_ROWS; i++){
		for (int j = 0; j < MATRIX_COLS; j++){
			
			// New key is being played, play the note
			if (updatedMatrix[i][j] == 1 && activeKeyMatrix[i][j] == 0){

				// Check if there are available buzzers
				if (buzzerCount < MAX_BUZZERS){
					// We only care about keys here, not the octave buttons. Check the entry (2-14)
					if (keys[i][j] >= 2 && keys[i][j] <= 14){
						
						activeKeyMatrix[i][j] = 1;
						
						// Check for an available spot, and mark as active buzzer
						int buzzerPin = 0;
						for (int i = 0; i < MAX_BUZZERS; i++){
							if (!activeBuzzers[i]){
								buzzerPin = buzzerPins[i];
								activeBuzzers[i] = buzzerPin;
								break;
							}
						}
						
						// Play the frequency, and update the buzzer matrix (keep track of which buzzers are playing)
						playFrequency(keys[i][j], currentOctave, buzzerPin, logFile, time);
						activeBuzzerMatrix[i][j] = buzzerPin;
						buzzerCount++;
					}
				}
				
				// Octave up key was pressed
				if (keys[i][j] == 0){
					// Check if the octave is within range
					if (currentOctave < 4){
						currentOctave++;
						activeKeyMatrix[i][j] = 1;
						printf("[INFO] Octave up: New octave is %d\n", currentOctave);
						
						// Log to file
						char str[] = "Octave changed to %d";
						char str2[1000];
						sprintf(str2, str, currentOctave);
						
						LOG_MSG(logFile, PROGRAM_NAME, time, str2);
						
					}
				}else if (keys[i][j] == 1){ // Octave down key was pressed
				    // Check if the octave is within range
					if (currentOctave > 1){
						currentOctave--;
						activeKeyMatrix[i][j] = 1;
						printf("[INFO] Octave down: New octave is %d\n", currentOctave);
						
						// Log to file
						char str[] = "Octave changed to %d";
						char str2[1000];
						sprintf(str2, str, currentOctave);
						
						LOG_MSG(logFile, PROGRAM_NAME, time, str2);
					}
				}
				
			}else if (updatedMatrix[i][j] == 0 && activeKeyMatrix[i][j] == 1){ // Key is being released, clear the note`
				if (keys[i][j] >= 2 && keys[i][j] <= 14){
					// Disable the buzzer, and update the respective matrices
					disableBuzzer(activeBuzzerMatrix[i][j], logFile, time);
					activeBuzzerMatrix[i][j] = 0;
					activeKeyMatrix[i][j] = 0;
				}else if (keys[i][j] >= 0 && keys[i][j] <= 2){ // Octave up/down
				
					// Get rid of that entry in the active key matrix
					activeKeyMatrix[i][j] = 0;
				}
			}
		}
	}
}

// This function is called whenever the program is interrupted with Ctrl+C
// It sets the keepRunning global flag to 0, so that the program knows to exit properly
void interruptHandler(int sig){
	keepRunning = 0;
}

int main(void){
	
	// Setup interrupt handler
	signal(SIGINT, interruptHandler);

	
	// Set up a config file.
	FILE* configFile;
	configFile = fopen("/home/pi/pi_ano.cfg", "r");
	
	// Init time array
	char time[30];
	getTime(time);
	
	char logFileLocation[100];
	
	// Init values from config
	if (configFile){
		setValuesFromConfig(configFile, &currentOctave, &watchDogTimer, logFileLocation);
	}else{
		generateNewConfig(); // If config doesn't exist, just make a new one, and keep default values
		
		// Set default log file loc
		strncpy(logFileLocation, "/home/pi/pi_ano.log", 100);
		
	}
	
	// Done with configFile, so close it (if it existed before)
	if (configFile){
		fclose(configFile);
	}
	
	// Open up a log file (from config, or default)
	FILE* logFile;
	logFile = fopen(logFileLocation, "a");
	
	// If for any reason the log file doesn't open, throw an error
	if (!logFile){
		printf("[ERROR] Log file was not loaded properly. Exiting! (are you running the program with sudo?) \n");
		return -1;
	}
	
	wiringPiSetupGpio();
	
	printf("[INFO] GPIO initialized.\n");
	
	// Log GPIO initialized
	getTime(time);
	LOG_MSG(logFile, PROGRAM_NAME, time, "GPIO successfully initialized");
	
	initPins();
	printf("[INFO] All GPIO pins initialized\n");
	
	// Log pins initialized
	getTime(time);
	LOG_MSG(logFile, PROGRAM_NAME, time, "All GPIO pins successfully initialized");
	
	printf("[INFO] Pi_ano is running... Press Ctrl + C to exit.\n");
	
	
	// Set up the watch dog
	int watchdog;


	if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0) {
		printf("[ERROR] Couldn't open watchdog device! (%d)\n", watchdog);
		return -1;
	}
	
	//Get the current time
	getTime(time);
	
	//Log that the watchdog file has been opened
	LOG_MSG(logFile, PROGRAM_NAME, time, "Watchdog file was successfully opened");


	// Set watchdog limit to timer
	ioctl(watchdog, WDIOC_SETTIMEOUT, &watchDogTimer);
	
	//Get the current time
	getTime(time);
	
	//Log that the Watchdog time limit has been set
	LOG_MSG(logFile, PROGRAM_NAME, time, "Watchdog time limit successfully set");

	// Set the current timeout to whatever the watchdog set it to (in case it's over 15 sec somehow)
	ioctl(watchdog, WDIOC_GETTIMEOUT, &watchDogTimer);
	
	// Log that the program was launched
	getTime(time);
	LOG_MSG(logFile, PROGRAM_NAME, time, "Pi_ano successfully launched");
	
	// Attempt to run program at a higher priority
	piHiPri(1);
	
	// Timer to tell when we should update the watchdog
	int timer = 0;
	
	while(keepRunning){
		
		// Construct a snapshot of the current physical key matrix.
		// Since we're pulsing 16 different times, it would take 16 * 3 ms = 48 ms.
		int snapshot[MATRIX_ROWS][MATRIX_COLS];

		for (int i = 0; i < MATRIX_ROWS; i++){
			for (int j = 0; j < MATRIX_COLS; j++){
				// Building it by column "vectors"
				snapshot[j][i] = pulseEntry(i, j);
			}
		}
		
		// Add 48 ms to the timer
		timer += 48;
		
		// Update the snapshot of the physical matrix to the one stored in the program
		// Handle buzzer playing, buzzer stopping, octave shifts, etc.
		getTime(time);
		updateKeys(snapshot, logFile, time);
		
		
		// Watchdog timeout in ms
		int wdTimeMs = watchDogTimer * 1000;
		

		// If the timer hits the halfway point for the watchdog, update it
		if (timer >= wdTimeMs / 2){
			
			// Update the watchdog to keep the system alive
			ioctl(watchdog, WDIOC_KEEPALIVE, 0);
			
			// Log that the Watchdog was updated
			getTime(time);
			LOG_MSG(logFile, PROGRAM_NAME, time, "Watchdog updated");
			
			// Reset timer
			timer = 0;
		}
		
	}
	
	// Keyboard interrupt, loop is exited (since keepRunning = 0)
	
	// Kill the watchdog, and log it
	write(watchdog, "V", 1);
	close(watchdog);
	
	getTime(time);
	LOG_MSG(logFile, PROGRAM_NAME, time, "Watchdog device successfully shut down");
	LOG_MSG(logFile, PROGRAM_NAME, time, "Pi_ano successfully shutdown");
	
	printf("\n[INFO] Pi_ano shutting down!\n");
	
	// Close log file
	fclose(logFile);
	
	// GPIO pins should be freed automatically by wiringPi library)
	return 0;
	
	
}