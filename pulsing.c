#include <stdio.h>
#include "wiringPi.h"
#include <math.h>


// list of input pins and output pins. 
// Output pins will be pulsed, and represent the row.
// Input pins will return data once pulsed, representing the active columns in that row.
const int inputPins[4] = { 11, 13, 19, 26 };
const int outputPins[4] = { 12, 16, 20, 21 };

// Key mapping to assigned key number. access like this: keys[row][column]; ex. keys[1][2] returns 6
const int keys[4][4] = { {0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15} };

// Arrays to keep track of which keys are active (pressed down)
int activeKeys[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };

// Returns a current snapshot of the physical key matrix.
int* getMatrixSnapshot(){
	int matrix[4][4];
	// Loop through each row
	for (int i = 0; i < 4; i++){
		
		// Pulse the row (set to high)
		digitalWrite(outputPins[i], HIGH);
		
		// Get each input response
		for (int j = 0; j < 4; j++){
			// Add to snapshot
			matrix[i][j] = digitalRead(inputPins[i]);
		}
		
		// "Unpulse" (set to low)
		digitalWrite(outputPins[i], LOW);
	}
	
	return matrix;
}

// Initializes GPIO pins for the matrix, buzzers, and octave control buttons.
void initPins(){
	// Init the four input pins (matrix)
	for (int i = 0; i < 4; i++){
		pinMode(inputPins[i], INPUT);
		digitalWrite(inputPins[i], PUD_UP);
	}
	// Init the four output pins (matrix)
	for (int i = 0; i < 4; i++){
		pinMode(outputPins[i], OUTPUT);
		digitalWrite(outputPins[i], LOW);
	}
}

int main(void){
	
	wiringPiSetupGpio();
	
	initPins();
	
	
	while(1){
		
		int* snapshot = getMatrixSnapshot();
		
		for (int i = 0; i < 4; i++){
			for (int j = 0; j < 4; j++){
				printf("%d ", snapshot[i][j]);
			}
			printf("\n");
		}
		printf("=======\n");
		delay(10);
	}
	
	
}