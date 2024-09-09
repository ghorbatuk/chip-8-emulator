#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <assert.h>
#include <conio.h>
#include "raylib.h"

#define MAX_SAMPLES               512
#define MAX_SAMPLES_PER_UPDATE   4096

unsigned char memory[4096] = { 0 };
unsigned char V[16];
unsigned short I;
unsigned short programCounter = 0;
unsigned short opCode;
unsigned char delayTimer = 0;
unsigned char soundTimer = 0;
unsigned char display[64 * 32];
unsigned short stack[16];
unsigned short stackPointer;
KeyboardKey keys[16] = { KEY_X,         //0
						 KEY_ONE,       //1
						 KEY_TWO,       //2
						 KEY_THREE,     //3 
						 KEY_Q,         //4
	                     KEY_W,         //5
					  	 KEY_E,         //6
						 KEY_A,         //7
						 KEY_S,         //8
						 KEY_D,         //9
						 KEY_Z,         //A
						 KEY_C,         //B 
						 KEY_FOUR,      //C 
						 KEY_R,         //D
						 KEY_F,         //E
						 KEY_V };       //F

bool drawFlag = 0;
unsigned char fonts[80] = { 0xF0, 0x90, 0x90, 0x90, 0xF0,    // 0
							0x20, 0x60, 0x20, 0x20, 0x70,    // 1
							0xF0, 0x10, 0xF0, 0x80, 0xF0,    // 2
							0xF0, 0x10, 0xF0, 0x10, 0xF0,    // 3
							0x90, 0x90, 0xF0, 0x10, 0x10,    // 4
							0xF0, 0x80, 0xF0, 0x10, 0xF0,    // 5
							0xF0, 0x80, 0xF0, 0x90, 0xF0,    // 6
							0xF0, 0x10, 0x20, 0x40, 0x40,    // 7
							0xF0, 0x90, 0xF0, 0x90, 0xF0,    // 8
							0xF0, 0x90, 0xF0, 0x10, 0xF0,    // 9
							0xF0, 0x90, 0xF0, 0x90, 0x90,    // A
							0xE0, 0x90, 0xE0, 0x90, 0xE0,    // B
							0xF0, 0x80, 0x80, 0x80, 0xF0,    // C
							0xE0, 0x90, 0x90, 0x90, 0xE0,    // D
							0xF0, 0x80, 0xF0, 0x80, 0xF0,    // E
							0xF0, 0x80, 0xF0, 0x80, 0x80 };  // F};

float frequency = 440.0f;
float sineIdx = 0.0f;

// Audio input processing callback
void AudioInputCallback(void* buffer, unsigned int frames)
{
	float incr = frequency / 44100.0f;
	short* d = (short*)buffer;

	for (unsigned int i = 0; i < frames; i++)
	{
		d[i] = (short)(32000.0f * sinf(2 * PI * sineIdx)) * 0.1f;
		sineIdx += incr;
		if (sineIdx > 1.0f) sineIdx -= 1.0f;
	}
}

int init(char* programName)
{
	int initSuccess = 1;
	programCounter = 0x200;  // Program is loaded at 0x200
	opCode = 0;      
	I = 0;     
	stackPointer = 0;      
	
	int fontsArraySize = sizeof(fonts) / sizeof(fonts[0]);
	std::memcpy(&memory[0x050], &fonts[0], fontsArraySize);

	std::ifstream inFile(programName,std::ios::binary);

	std::vector<char> bytes(                       
		(std::istreambuf_iterator<char>(inFile)),
		(std::istreambuf_iterator<char>()));

	if (inFile.good()) {
		std::memcpy(&memory[0x200], &bytes[0], bytes.size());
	}
	else {
		printf("Could not load program %s.\n", programName);
		initSuccess = 0;
	}

	inFile.close();
	delayTimer = 0;
	soundTimer = 0;
	
	return initSuccess;
}

void emulateCycle() {
	//Fetch opCode
	opCode = (memory[programCounter] << 8) | memory[programCounter + 1];
	//Decode opCode
	programCounter += 2;
	switch (opCode & 0xF000)
	{
	case 0x1000:    //1NNN: Jump to address NNN
		programCounter = opCode & 0x0FFF;
		break;

	case 0x2000:    //2NNN: Execute subroutine starting at address NNN 
		stack[stackPointer] = programCounter;
		stackPointer++;
		programCounter = opCode & 0x0FFF;
		break;

	case 0x3000:    //3XNN: Skip the following instruction if the value of register VX equals NN
		if (V[(opCode & 0x0F00) >> 8] == (opCode & 0x00FF)) {
			programCounter += 2;
		}
		break;

	case 0x4000:    //4XNN: Skip the following instruction if the value of register VX is not equal to NN
		if (V[(opCode & 0x0F00) >> 8] != (opCode & 0x00FF)) {				
			programCounter += 2;
		}
		break;	

	case 0x5000:    //5XY0: Skip the following instruction if the value of register VX is equal to the value of register VY
		if (V[(opCode & 0x0F00) >> 8] == (opCode & 0x00F0) >> 4) {
			programCounter += 2;
		}
		break;

	case 0x6000:    //6XNN: Store number NN in register VX
	{
		int regIndex = (opCode & 0x0F00) >> 8;
		V[regIndex] = opCode & 0x00FF;
		break;
	}		
	case 0x7000:    //7XNN: Add the value NN to register VX
	{
		int regIndex = (opCode & 0x0F00) >> 8;
		V[regIndex] += opCode & 0x00FF;
		break;
	}	
	case 0x8000:
		switch (opCode & 0x000F)
		{
		case 0x0000:   //8XY0: Store the value of register VY in register VX
			V[(opCode & 0x0F00) >> 8] = V[(opCode & 0x00F0) >> 4];
			break;
		case 0x0001:   //8XY1: Set VX to VX OR VY
			V[(opCode & 0x0F00) >> 8] = V[(opCode & 0x0F00) >> 8] | V[(opCode & 0x00F0) >> 4];
			V[0xF] = 0;
			break;
		case 0x0002:   //8XY2: Set VX to VX AND VY
			V[(opCode & 0x0F00) >> 8] = V[(opCode & 0x0F00) >> 8] & V[(opCode & 0x00F0) >> 4];
			V[0xF] = 0;
			break;
		case 0x0003:   //8XY3: Set VX to VX XOR VY
			V[(opCode & 0x0F00) >> 8] = V[(opCode & 0x0F00) >> 8] ^ V[(opCode & 0x00F0) >> 4];
			V[0xF] = 0;
			break;
		case 0x0004:   //8XY4: Add the value of register VY to register VX.  Set VF to 1 if a carry occurs. Set VF to 0 if a carry does not occur.
		{
			unsigned short totalVxVy = V[(opCode & 0x0F00) >> 8] + V[(opCode & 0x0F0) >> 4];
			if (totalVxVy > 255) {
				V[(opCode & 0x0F00) >> 8] = totalVxVy & 0xFF;
				V[0xF] = 1;
			}
			else {
				V[(opCode & 0x0F00) >> 8] = totalVxVy;
				V[0xF] = 0;
			}
			break;
		}		
		case 0x0005:   //8XY5: Subtract the value of register VY from register VX.  Set VF to 0 if a borrow occurs. Set VF to 1 if a borrow does not occur.
		{
			unsigned char vxValue = V[(opCode & 0x0F00) >> 8];
			
			V[(opCode & 0x0F00) >> 8] = V[(opCode & 0x0F00) >> 8] - V[(opCode & 0x00F0) >> 4];

			if (vxValue >= V[(opCode & 0x00F0) >> 4]) {
				V[0xF] = 1;
			}
			else {
				V[0xF] = 0;
			}
			break;
		}		
		case 0x0006:  //8XY6: Store the value of register VY shifted right one bit in register VX.  Set register VF to the least significant bit prior to the shift. 
		{
			unsigned char vxValue = V[(opCode & 0x0F00) >> 8];
			
			V[(opCode & 0x0F00) >> 8] = V[(opCode & 0x0F00) >> 8] >> 1;
			if ((vxValue & 0x1) == 1) {
				V[0xF] = 1;
			}
			else {
				V[0xF] = 0;
			}
			break;
		}
		case 0x0007: //8XY7: Set register VX to the value of VY minus VX. Set VF to 0 if a borrow occurs. Set VF to 1 if a borrow does not occur
		{

			unsigned char vxValue = V[(opCode & 0x0F00) >> 8];

			V[(opCode & 0x0F00) >> 8] = V[(opCode & 0x00F0) >> 4] - V[(opCode & 0x0F00) >> 8];
			if (vxValue <= V[(opCode & 0x00F0) >> 4]) {
				V[0xF] = 1;
			}
			else {
				V[0xF] = 0;
			}
			break;
		}	
		case 0x000E: //8XYE: Store the value of register VY shifted left one bit in register VX.  Set register VF to the most significant bit prior to the shift. 
	    {
			unsigned char vyValue = V[(opCode & 0x00F0) >> 4];	
			
			V[(opCode & 0x0F00) >> 8] = V[(opCode & 0x00F0) >> 4] << 1;
				
			if ((vyValue & 0x80) == 128) {
				V[0xF] = 1;
			}
			else {
				V[0xF] = 0;
			}
			break;
		}
			
		default:
			printf("Unknown opCode: 0x%X\n", opCode);
		}
		break;

	case 0x9000: //9XY0: Skip the following instruction if the value of register VX is not equal to the value of register VY
		if (V[(opCode & 0x0F00) >> 8] != V[(opCode & 0x00F0) >> 4]) {
			programCounter += 2;
		}
		break;

	case 0xA000: //ANNN: Sets I to the address NNN
		I = opCode & 0x0FFF;
		break;

	case 0xB000: //BNNN: Jump to address NNN + V0
	{
		programCounter = (opCode & 0x0FFF) + V[0];
		break;
	}
	case 0xC000: //CXNN: Set VX to a random number with a mask of NN
	{
		std::random_device rd; 
		std::mt19937 gen(rd()); 
		std::uniform_int_distribution<> distr(0, 255); 
		unsigned char randomNumber = distr(gen);
		V[opCode & 0x0F00] = randomNumber & (opCode & 0x00FF);
		break;
	}
	case 0xD000: //DXYN: Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I     
		         //Set VF to 1 if any set pixels are changed to unset, and 0 otherwise
	{		
		int bytesToRead = opCode & 0x000F;
		unsigned char xPos = V[(opCode & 0x0F00) >> 8];
		unsigned char yPos = V[(opCode & 0x00F0) >> 4];

		V[0xF] = 0;
		unsigned char currentByte;
		for (int i = 0; i < bytesToRead; ++i) {

			currentByte = memory[I + i];
			int row = (yPos + i) % 32;
			
			for (int j = 0; j < 8; ++j) {
				int column = (xPos + j) % 64;
				int offset = row * 64 + column;
				
				unsigned char pixelOn = (currentByte & 0x80) >> 7;
				if (pixelOn == 1) {
					if (display[offset] == 1) {						
						display[offset] = 0;					
						V[0xF] = 1;
					}
					else{
						display[offset] = 1; 			
					}					
				}
				if (column >= 63 ) {
					break;
				}
				currentByte <<= 1;
			}
			if (row >= 31) {
				break;
			}
		}
		break;
	}
	case 0xE000:
		switch (opCode & 0x00FF)
		{
		case 0x009E: //EX9E: Skip the following instruction if the key corresponding to the hex value currently stored in register VX is pressed
		{	
 			if (IsKeyDown(keys[V[(opCode & 0x0F00) >> 8]])) {
				programCounter += 2;
			}	
			break;
		}
		case 0x00A1:  //EXAI: Skip the following instruction if the key corresponding to the hex value currently stored in register VX is not pressed
			if (!IsKeyDown(keys[V[(opCode & 0x0F00) >> 8]])) {
				programCounter += 2;
			}		
			break;

		default:
			printf("Unknown opCode: 0x%X\n", opCode);			
			break;
		}
		break;

	case 0xF000:
		switch (opCode & 0x00FF)
		{
		case 0x0007:   //FX07: Store the current value of the delay timer in register VX
			V[(opCode & 0x0F00) >> 8] = delayTimer;
			
			break;
		case 0x000A:   //FX0A: Wait for a keypress and store the result in register VX
		{
			bool keyPressed = false;
			do {
				int key = _getch();

				for (unsigned char i = 0; i < 16; ++i) {
					if (keys[i] == key) {
						keyPressed = true;
						V[(opCode & 0x0F00) >> 8] = i;
						break;
					}
				}

			} while (keyPressed == false);
			
			break;
		}	
		case 0x0015:   //FX15: Set the delay timer to the value of register VX
			delayTimer = V[(opCode & 0x0F00) >> 8];
			break;

		case 0x0018:   //FX18: Set the sound timer to the value of register VX
			soundTimer = V[(opCode & 0x0F00) >> 8];
			break;

		case 0x001E:   //FX1E: Add the value stored in register VX to register I 
			I += V[(opCode & 0x0F00) >> 8];
			break;

		case 0x0029:   //FX29: Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX
			I = 0x50 + 0x5 * V[(opCode & 0x0F00) >> 8];		
			break;

		case 0x0033:   //FX33: Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I + 1, and I + 2
		{
			unsigned char value = V[(opCode & 0x0F00) >> 8];
			for (int i = 2; i > -1; --i) {
				unsigned char digit = value % 10;
				memory[I + i] = digit;
				value /= 10;
			}
			break;
		}		
		case 0x0055:  //FX55: Store the values of registers V0 to VX inclusive in memory starting at address I. I is set to I + X + 1 after operation.
			std::memcpy(&memory[I], &V, ((opCode & 0x0F00) >> 8) + 1);
			I = I + ((opCode & 0x0F00) >> 8) + 1;
			break;

		case 0x0065:  //Fill registers V0 to VX inclusive with the values stored in memory starting at address I. I is set to I + X + 1 after operation.		
		{
			std::memcpy(&V, &memory[I], ((opCode & 0x0F00) >> 8) + 1);
			I = I + ((opCode & 0x0F00) >> 8) + 1;
			break;

		}	
		default:
			printf("Unknown opCode: 0x%X\n", opCode);
		}
		break;

	case 0x0000:  
		switch (opCode & 0x00FF)
		{
		case 0x00E0: //00E0: Clear the screen
			for (int i = 0; i < 2048; ++i)
			{
				display[i] = 0;
			}
			break;

		case 0x00EE: //00EE: Return from a subroutine
			if (stackPointer > 0)
			{
				stackPointer--;
			}			
			programCounter = stack[stackPointer];
			break;

		default:
			printf("Unknown opCode: 0x%X\n", opCode);
		}
		break;

	default:
		printf("Unknown opCode: 0x%X\n", opCode);
	}
}

void drawGraphics() {
	ClearBackground(BLACK);
    int cellWidth = 10;
	int cellHeight = 10;
	//Draw Cells
	for (int i = 0; i < 32; ++i)
	{
		for (int j = 0; j < 64; ++j)
		{
			if (display[i * 64 + j] == 1) {
				DrawRectangle( cellWidth * j, cellHeight * i, cellWidth, cellHeight, WHITE);
			}
		}
	}	
}

int main(int argc, char* argv[]) {
	int cyclesToExecute = 11;
	bool isAudioPlaying = false;
	char* programName = argv[1];
	if (programName == NULL)
		return 0;

	assert(!GetWindowHandle());  //Window not already created

	InitWindow(640, 320, "CHIP-8");
	SetTargetFPS(60);

	InitAudioDevice();       

	SetAudioStreamBufferSizeDefault(MAX_SAMPLES_PER_UPDATE);

	AudioStream stream = LoadAudioStream(44100, 16, 1);

	SetAudioStreamCallback(stream, AudioInputCallback);

	short* data = (short*)malloc(sizeof(short) * MAX_SAMPLES);
	short* writeBuf = (short*)malloc(sizeof(short) * MAX_SAMPLES_PER_UPDATE);

	if (init(programName)) 
	{
		while (!WindowShouldClose())    // Detect window close button or ESC key
		{

			for (int i = 0; i < cyclesToExecute; ++i) {
				emulateCycle();
			}
			BeginDrawing();
			drawGraphics();
			EndDrawing();

			//Update Timers
			if (delayTimer > 0)
				--delayTimer;

			if (soundTimer > 1)
			{
				if (!isAudioPlaying) {
					PlayAudioStream(stream);
					isAudioPlaying = true;
				}
				--soundTimer;
			}
			else {
				if (isAudioPlaying) {
					StopAudioStream(stream);
					isAudioPlaying = false;
				}
			}
		}
	}
	
	free(data);                 
	free(writeBuf);             
	UnloadAudioStream(stream);   
	CloseAudioDevice();
	CloseWindow();

	return 0;
}

