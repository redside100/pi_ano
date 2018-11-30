#include <stdio.h>
#include "wiringPi.h"
#include "softTone.h"
#include <stdlib.h>
#include <math.h>


// list of input pins and output pins. 
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

// Global octave counter
int currentOctave = 4;

// Define rows and cols
#define MATRIX_ROWS 4
#define MATRIX_COLS 4
#define MAX_BUZZERS 4


// Returns the value of a column in a row of the physical key matrix.
int pulseEntry(int col, int row){
	// Pulse the column (set to high)
	digitalWrite(outputPins[col], HIGH);
	
	delay(3);
	
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
	
	printf("[INFO] Pins initialized.\n");
	
	
}

// Stops a buzzer from playing sound
void clearFrequency(int buzzerPin){
	printf("[INFO] Stopped buzzer %d\n", buzzerPin); 
	softToneWrite(buzzerPin, 0);
}

// Plays a frequency to a buzzer, based on what key was pressed, and what octave
void playFrequency(int key, int octave, int buzzerPin){
	// Octave must be between 1 and 7 (max 5000hz)
	if (octave > 0 && octave < 8){
		
		// Since we're calculating with A4 = 440, 
		// this gives the factor of what we should multiply the frequency by
		int trueOctave = octave - 4;
		
		// The formula with A4 = 440hz. This assumes that when key = 2, it is C4, and key = 14 is C5
		double basis = pow(2, (double) (1.0 / 12.0));
		// Add 0.5 to round (casting to int truncates, so adding 0.5 would make it round)
		int frequency = (int) (((440 * pow(basis, key - 11)) * pow(2, trueOctave)) + 0.5);
		
		// Quick check
		if (buzzerCount < 4){
			printf("[INFO] Playing frequency %d with buzzer %d\n", frequency, buzzerPin); 
			softToneWrite(buzzerPin, frequency);	
		}

	}
}

void disableBuzzer(int pin){
	
	// Set frequency of buzzer to 0
	clearFrequency(pin);
	for (int i = 0; i < MAX_BUZZERS; i++){
	    if (activeBuzzers[i] == pin){
			activeBuzzers[i] = 0;
		}
	}
	buzzerCount--;
}

void updateKeys(int updatedMatrix[MATRIX_ROWS][MATRIX_COLS]){
	for (int i = 0; i < MATRIX_ROWS; i++){
		for (int j = 0; j < MATRIX_COLS; j++){
			// New key is being played, play the note
			if (updatedMatrix[i][j] == 1 && activeKeyMatrix[i][j] == 0){
				
				// printf("Old: %d\nNew: %d\n", activeKeyMatrix[i][j], updatedMatrix[i][j]);

				// Check if there are available buzzers
				if (buzzerCount < MAX_BUZZERS){
					// We only care about keys here, not the octave buttons. Check the entry (2-14)
					if (keys[i][j] >= 2 && keys[i][j] <= 14){
						
						activeKeyMatrix[i][j] = 1;
						
						// Check for an available spot
						int buzzerPin = 0;
						for (int i = 0; i < MAX_BUZZERS; i++){
							if (!activeBuzzers[i]){
								buzzerPin = buzzerPins[i];
								activeBuzzers[i] = buzzerPin;
								break;
							}
						}
						
						// Play the freq 
						playFrequency(keys[i][j], currentOctave, buzzerPin);
						activeBuzzerMatrix[i][j] = buzzerPin;
						buzzerCount++;
					}
				}
				
				// Octave up key was pressed
				if (keys[i][j] == 0){
					// Check if the octave is within range
					if (currentOctave < 7){
						currentOctave++;
						printf("[INFO] Octave up: New octave is %d\n", currentOctave);
						activeKeyMatrix[i][j] = 1;
					}
				}else if (keys[i][j] == 1){ // Octave down key was pressed
				    // Check if the octave is within range
					if (currentOctave > 1){
						currentOctave--;
						printf("[INFO] Octave down: New octave is %d\n", currentOctave);
						activeKeyMatrix[i][j] = 1;
					}
				}
				
			}else if (updatedMatrix[i][j] == 0 && activeKeyMatrix[i][j] == 1){ // Key is being released, clear the note`
				if (keys[i][j] >= 2 && keys[i][j] <= 14){
					disableBuzzer(activeBuzzerMatrix[i][j]);
					activeBuzzerMatrix[i][j] = 0;
					activeKeyMatrix[i][j] = 0;
				}else if (keys[i][j] >= 0 && keys[i][j] <= 2){ // Octave up/down
					activeKeyMatrix[i][j] = 0;
				}
			}
		}
	}
}


int main(void){
	
	wiringPiSetupGpio();
	
	printf("[INFO] GPIO initialized.\n");
	
	initPins();
	
	printf("[INFO] Pi_ano is running... Press Ctrl + C to exit.\n");
	
	while(1){
		
		// Construct a snapshot of the current physical key matrix.
		int snapshot[MATRIX_ROWS][MATRIX_COLS];

		
		for (int i = 0; i < MATRIX_ROWS; i++){
			for (int j = 0; j < MATRIX_COLS; j++){
				// Tuilding it by columns "vectors"
				snapshot[j][i] = pulseEntry(i, j);
			}
		}
		
		// Print out matrix
		// for (int i = 0; i < MATRIX_ROWS; i++){
			// for (int j = 0; j < MATRIX_COLS; j++){
				// printf("%d ", snapshot[i][j]);
			// }
			// printf("\n");
		// }
		// printf("=======\n");
		
		updateKeys(snapshot);
		
	}
	
	
}