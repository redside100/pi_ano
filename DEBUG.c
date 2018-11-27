#include <stdio.h>
#include <math.h>


// Define lengths, and pins
const int buzzers = 4;
const int matrixLength = 4;
const int maxActiveKeys = 4;
const int totalKeys = 13;

const int buzzerPins[4] = { 2, 3, 4, 17 };
const int matrixRow[4] = { 25, 8, 7, 12 };
const int matrixCol[4] = { 10, 9, 11, 5 };

// DEBUG
int inputRow[4] = { 0, 0, 0, 0 };
int inputCol[4] = { 0, 0, 0, 0 };

// Entry structure for matrix purposes
struct Entry{
	int x;
	int y;
};


struct Entry activeKeys[4];
int playing[4] = { 0, 0, 0, 0 };
int currentActiveKeys = 0;

// Checks if two entries are equal
int entryIsEqual(struct Entry entry1, struct Entry entry2){
	return (entry1.x == entry2.x && entry1.y == entry2.y);
}

// Returns the index of an active key.
// If the key isn't active, then it returns -1
int getActiveKeyIndex(struct Entry entry){
	for (int i = 0; i < maxActiveKeys; i++){
		if (entryIsEqual(entry, activeKeys[i])){
			return i;
		}
	}
	return -1;
}

// Sets a key to be active, if there is space
void setActiveKey(struct Entry entry){
	
	if (currentActiveKeys < 4){
		
		currentActiveKeys++;
		
		struct Entry none;
		none.x = -1;
		none.y = -1;
		
		// Find the next available space
		for (int i = 0; i < maxActiveKeys; i++){
			
			// Found a free space
			if (activeKeys[i].x == none.x){
				activeKeys[i] = entry;
				break;
			}
			
		}
	}
	
}

// Sets a key to be inactive, if it is active
void setInactiveKey(struct Entry entry){
	
	int index = getActiveKeyIndex(entry);
	
	if (index != -1){
		activeKeys[index].x = -1;
		activeKeys[index].y = -1;
		currentActiveKeys--;
	}
	
}


// DEBUG, CHECKS FAKE ARRAY NOT THE PI
int digitalRead(int pin){
	for (int i = 0; i < matrixLength; i++){
		if (matrixRow[i] == pin){
			return inputRow[i];
		}else if (matrixCol[i] == pin){
			return inputCol[i];
		}
	}
}

// Return 1 is an entry is active, 0 if it is not.
int isEntryActive(struct Entry entry){
	return (digitalRead(matrixRow[entry.x]) && digitalRead(matrixCol[entry.y]));
}

// Plays a frequency to a buzzer, based on what key was pressed, and what octave
void playFrequency(int key, int octave, int buzzerPin){
	// Octave must be between 1 and 7 (max 5000hz)
	if (octave > 0 && octave < 8){
		
		// Since we're calculating with A4 = 440, 
		// this gives the factor of what we should multiply the frequency by
		int trueOctave = octave - 4;
		
		// The formula with A4 = 440hz. This assumes that when key = 0, it is C4, and key = 12 is C5
		double basis = pow(2, (double) (1.0 / 12.0));
		int frequency = (int) ((440 * pow(basis, key - 9)) * pow(2, trueOctave));
		
		// Quick check
		if (currentActiveKeys < 4){
			printf("Playing frequency %d on pin %d\n", frequency, buzzerPin); // 2, 3, 4, 17
		}

	}
}

// Stops a buzzer from playing sound
void clearFrequency(int buzzerPin){
	printf("Stopped playing frequency on pin %d\n", buzzerPin); // 2, 3, 4, 17
}

// DEBUG
void printEntry(Entry entry){
	printf("%d, %d\n", entry.x, entry.y);
}


int main(void){
	
	

	printf("DEBUG Pi Ano is running... Press Ctrl+C to terminate the program.\n");

	struct Entry keyEntries[13];
	
	
	// Init active key array to all null at first
	struct Entry none;
	none.x = -1;
	none.y = -1;
	
	// Init all active keys to none at first
	for (int i = 0; i < maxActiveKeys; i++){
		activeKeys[i] = none;
	}

	// Set up all the matrix entries for keys
	int currCol = -1;
	for (int i = 0; i < totalKeys; i++){
		
		// Get what the index of the x should be in the matrix row (resets every 4, since it's 4x4)
		int rowIndex = i % 4;
		
		// Next column index if we're at the start of a new row
		if (rowIndex == 0)
			currCol++;
		
		struct Entry key;
		key.x = rowIndex;
		key.y = currCol;
		
		keyEntries[i] = key;
		
	}
	
	
	int octave = 4;
	
	
	// Endless loop
	while(1){
		
		
		// SIMULATE INPUTS
		char command = getchar();
		switch(command){
			// C4
			case 'a':
				inputRow[0] = 1;
				inputCol[0] = 1;
				break;
			case 'z':
				inputRow[0] = 0;
				inputCol[0] = 0;
				break;
			case 's':
				inputRow[1] = 1;
				inputCol[1] = 1;
				break;
			case 'x':
				inputRow[1] = 0;
				inputCol[1] = 0;
				break;
			
		}
		
		
		
		// Loop through all possible keys
		for (int i = 0; i < totalKeys; i++){
			
			struct Entry currentKeyEntry = keyEntries[i];
			// If this entry is newly active (meaning it was pressed down, but not processed yet)
			if (isEntryActive(currentKeyEntry)){

				printf("Entry is pressed down: ");
				printEntry(currentKeyEntry);
				// Check if it's not already active (index = -1)
				if (getActiveKeyIndex(currentKeyEntry) == -1){
					
					printf("Entry is NOW ACTIVE: ");
					printEntry(currentKeyEntry);
					// Set it to be active
					setActiveKey(currentKeyEntry);
				}
				
			}else{
				// If it's not, then just try disabling the key (if it was previously active)
				setInactiveKey(currentKeyEntry);
			}
			
		}
		
	
		
		// Loop through all active keys
		for (int i = 0; i < maxActiveKeys; i++){
		
			
			// If the current active key is something
			if (activeKeys[i].x != -1 && activeKeys[i].y != -1){
				
				// Loop through all key entries, and compare
				for (int j = 0; j < totalKeys; j++){
					
					if (entryIsEqual(activeKeys[i], keyEntries[j])){
						
						if (!playing[i]){
							
							printf("ACTIVE KEY: ");
							printEntry(activeKeys[i]);
					
							
							// Play the frequency
							playFrequency(j, octave, buzzerPins[i]);
							playing[i] = 1;
							
						}
						
					}
				}
			
			}else{ // If it is, try clearing it
			
				if (playing[i]){
					clearFrequency(buzzerPins[i]);
					playing[i] = 0;
				}
			}
		}
		
	}
	
}