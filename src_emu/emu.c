#include "emu.h"

bool gbmu_keys[GBMU_NUMBER_OF_KEYS];
uint32_t *screen_pixels;
uint32_t palette[] = {0xffffffff, 0xffaaaaaa, 0xff555555, 0xff000000};
uint8_t *mem;
uint8_t *gamerom;
size_t   gamerom_size;
uint8_t bootrom[] = {
	0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb,
	0x21, 0x26, 0xff, 0x0e, 0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3,
	0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0, 0x47, 0x11, 0x04, 0x01,
	0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
	0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06, 0x08, 0x1a, 0x13, 0x22,
	0x23, 0x05, 0x20, 0xf9, 0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99,
	0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20, 0xf9, 0x2e, 0x0f, 0x18,
	0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
	0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20,
	0xf7, 0x1d, 0x20, 0xf2, 0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62,
	0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06, 0x7b, 0xe2, 0x0c, 0x3e,
	0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
	0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17,
	0xc1, 0xcb, 0x11, 0x17, 0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9,
	0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b, 0x03, 0x73, 0x00, 0x83,
	0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
	0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63,
	0x6e, 0x0e, 0xec, 0xcc, 0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e,
	0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c, 0x21, 0x04, 0x01, 0x11,
	0xa8, 0x00, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
	0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe,
	0x3e, 0x01, 0xe0, 0x50
};
uint8_t cycleTable[256] = { // Duration in cycles of each cpu instruction.
	4,12,8,8,4,4,8,4,20,8,8,8,4,4,8,4,
	4,12,8,8,4,4,8,4,12,8,8,8,4,4,8,4,
	8,12,8,8,4,4,8,4,8,8,8,8,4,4,8,4,
	8,12,8,8,12,12,12,4,8,8,8,8,4,4,8,4,
	4,4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,
	4,4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,
	4,4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,
	8,8,8,8,8,8,4,8,4,4,4,4,4,4,8,4,
	4,4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,
	4,4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,
	4,4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,
	4,4,4,4,4,4,8,4,4,4,4,4,4,4,8,4,
	8,12,12,16,12,16,8,16,8,16,12,4,12,24,8,16,
	8,12,12,0,12,16,8,16,8,16,12,0,12,0,8,16,
	12,12,8,0,0,16,8,16,16,4,16,0,0,0,8,16,
	12,12,8,4,0,16,8,16,12,8,16,4,0,0,8,16
};
uint16_t PC, SP;
t_regs regs;
int scanlineCycles;
int divTimerCycles;
int counterTimerCycles;
bool IME;
int cycles;
bool isBootROMUnmapped = false;
bool isHalted = false;
bool debug = false;
int selectedROMBank = 1;

void gbmu_reset() {
	PC = SP = scanlineCycles = divTimerCycles = counterTimerCycles = IME = isBootROMUnmapped = isHalted = selectedROMBank = 0;
	memset(&regs, 0, sizeof(regs));
	if (!screen_pixels)
		screen_pixels = malloc(160 * 144 * 4);
	for (int i=0; i<160*144; i++)
		screen_pixels[i] = palette[0];
	free(mem);
	mem = calloc(1, 0x10000);
}

bool gbmu_load_rom(char *filepath) {
	FILE *file;
	gbmu_reset();
	free(gamerom);
	gamerom = calloc(1, 8388608); // max 8 MB cartridges
	if (!(file = fopen(filepath, "rb")))
		return false;
	gamerom_size = 0;
	while (fread(gamerom + gamerom_size, 1, 0x100, file) == 0x100)
		gamerom_size += 0x100;
	fclose(file);
	return true;
}

void gbmu_run_one_frame() {
	bool isFrameReady = false;
	while (!isFrameReady) {
		isFrameReady = gbmu_run_one_instr();
	}
}

bool gbmu_run_one_instr() {
	bool isFrameReady = false;
	// if (!debug && PC==0x100)
	// 	debug = true;
	if (debug) {
		char flag_str[4];
		flag_str[0] = (regs.F & 0x80) ? 'Z' : '-';
		flag_str[1] = (regs.F & 0x40) ? 'N' : '-';
		flag_str[2] = (regs.F & 0x20) ? 'H' : '-';
		flag_str[3] = (regs.F & 0x10) ? 'C' : '-';
		printf("PC=%04X AF=%04X BC=%04X DE=%04X HL=%04X SP=%04X %.4s Opcode=%02X %02X IE=%04X IF=%04X cnt=%02X IME=%d\n",
				PC, regs.AF, regs.BC, regs.DE, regs.HL, SP, flag_str, readByte(PC), readByte(PC+1), mem[0xffff], mem[0xff0f], mem[0xff05], IME);
		fflush(stdout);
	}

	if (isHalted)
		cycles = 4;
	else {
		uint8_t opcode = fetchByte(); // Fetch opcode
		cycles = cycleTable[opcode];
		if (!instrs[opcode])
			exit(printf("Invalid opcode: %#x\n", opcode));
		instrs[opcode]();             // Execute
	}

	if ((scanlineCycles += cycles) >= 456) {
		scanlineCycles -= 456;
		mem[0xff44]++;
		if (mem[0xff44] > 153)
			mem[0xff44] = 0;
		int LY  = mem[0xff44];
		int LYC = mem[0xff45];
		bool isDisplayEnabled = mem[0xff40] & 0x80;
		bool coincidenceFlag = LY==LYC;
		mem[0xff41] = coincidenceFlag ? (mem[0xff41]|4) : (mem[0xff41]&~4);
		if ((mem[0xff41]&0x40) && coincidenceFlag) {
			requestInterrupt(0x02); // Coincidence Interrupt
		}
		if (LY==144) {
			requestInterrupt(0x01); // V-Blank Interrupt
			if (SHOW_BOOT_ANIMATION || isBootROMUnmapped) {
				if (!isDisplayEnabled) { // Clear the display if disabled
					for (int i=0; i<160*144; i++)
						screen_pixels[i] = palette[0];
				}
				isFrameReady = true;
			}
		}

		if (LY < 144 && isDisplayEnabled) {

			// Draw BG & Window
			uint8_t *BGTileMap  = mem + ((mem[0xff40]&0x08) ? 0x9c00 : 0x9800);
			uint8_t *WinTileMap = mem + ((mem[0xff40]&0x40) ? 0x9c00 : 0x9800);
			uint8_t *tileData   = mem + ((mem[0xff40]&0x10) ? 0x8000 : 0x8800);
			bool isWindowDisplayEnabled = mem[0xff40]&0x20;
			bool isBGDisplayEnabled = mem[0xff40]&0x01;
			for (int screenX=0; screenX<160; screenX++) {
				int WY=mem[0xff4a], WX=mem[0xff4b]-7;
				int SCY=mem[0xff42], SCX=mem[0xff43];
				uint8_t *tileMap;
				int x, y;
				if (isWindowDisplayEnabled && WY<=LY && WX<=screenX) {
					x = (screenX-WX)&0xff;
					y = (LY-WY)&0xff;
					tileMap = WinTileMap;
				} else if (isBGDisplayEnabled) {
					x = (screenX+SCX)&0xff;
					y = (LY+SCY)&0xff;
					tileMap = BGTileMap;
				} else {
					break;
				}
				int tileX = x>>3;
				int tileY = y>>3;
				int u = x&7;
				int v = y&7;
				int tileIndex = tileMap[tileY*32 + tileX];
				if (!(mem[0xff40]&0x10))
					tileIndex = (int8_t)tileIndex + 128;
				uint8_t *tile = tileData + tileIndex*16;
				uint16_t pixels = ((uint16_t*)tile)[v];
				uint32_t px = pixels >> (7-u);
				px = (px>>7&2) | (px&1);
				px = palette[(mem[0xff47]>>(px*2))&3];
				screen_pixels[LY*160 + screenX] = px;
			}

			// Draw Sprites
			uint8_t *spriteAttrTable = mem+0xfe00;
			uint8_t *spriteData = mem+0x8000;
			bool isSpriteDisplayEnabled = mem[0xff40]&0x02;
			for (int i=0; i<40; i++) {
				if (!isSpriteDisplayEnabled)
					break;
				uint8_t *sprite = spriteAttrTable + i*4;
				int spriteY = (int)sprite[0] - 16;
				int v = LY - spriteY;
				if (v < 0 || v > 7)
					continue;
				bool flipX = sprite[3]&0x20;
				bool flipY = sprite[3]&0x40;
				int spriteX = (int)sprite[1] - 8;
				uint8_t *tile = spriteData + sprite[2]*16;
				uint16_t pixels = ((uint16_t*)tile)[flipY ? (7-v) : v];
				uint8_t spritePalette = mem[(sprite[3]&10) ? 0xff49 : 0xff48];
				for (int screenX=max(0, spriteX); screenX<min(160, spriteX+8); screenX++) {
					int u = screenX - spriteX;
					uint32_t px = pixels >> (flipX ? u : (7-u));
					px = (px>>7&2) | (px&1);
					if (!px) // transparent
						continue;
					px = palette[(spritePalette>>(px*2))&3];
					screen_pixels[LY*160 + screenX] = px;
				}
			}
		}
	}

	// Set LCD mode flag
	int lcd_mode;
	if (mem[0xff44] >= 144)
		lcd_mode = 1;
	else if (scanlineCycles >= 252)
		lcd_mode = 0;
	else if (scanlineCycles >= 80)
		lcd_mode = 3;
	else
		lcd_mode = 2;
	mem[0xff41] &= ~3;
	mem[0xff41] |= lcd_mode;

	// Update timers
	if ((divTimerCycles += cycles) >= 256) {
		divTimerCycles -= 256;
		mem[0xff04]++;
	}

	int maxTimerCycles = (int[]){1024, 16, 64, 256}[mem[0xff07]&3];
	if ((counterTimerCycles += cycles) >= maxTimerCycles) {
		counterTimerCycles -= maxTimerCycles;
		mem[0xff05]++;
		if (mem[0xff05] == 0) {
			mem[0xff05] = mem[0xff06];
			requestInterrupt(0x04); // Timer Interrupt
		}
	}

	// Execute interrupts
	uint8_t IE = mem[0xffff];
	uint8_t IF = mem[0xff0f];
	if (IME && IE && IF) {
		for (int i=0; i<5; i++) {
			int mask = 1<<i;
			if (IE & IF & mask) {
				push(PC);
				PC = "@HPX`"[i];
				mem[0xff0f] &= ~mask;
				IME = 0;
				break;
			}
		}
	}
	return isFrameReady;
}

int min(int a, int b) {
	return ((a < b) ? a : b);
}

int max(int a, int b) {
	return ((a > b) ? a : b);
}

void requestInterrupt(uint8_t interrupt) {
	if (mem[0xffff] & interrupt)
		mem[0xff0f] |= interrupt;
	isHalted = false;
}