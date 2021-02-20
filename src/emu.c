#include "emu.h"

bool gbmu_keys[GBMU_NUMBER_OF_KEYS];
uint32_t *screen_pixels, *screen_debug_tiles_pixels;
uint8_t *mem;
uint8_t *gbc_wram;
uint8_t *vram;
uint8_t *external_ram;
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
uint8_t clocksTable[256] = { // Duration of each cpu instruction.
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
int scanlineClocks, divTimerClocks, counterTimerClocks, clocksIncrement;
bool IME;
bool isBootROMUnmapped;
bool isHalted, isStopped;
bool debug;
int ROMBankNumber, externalRAMBankNumber;
t_mode hardwareMode;
int LY;
bool doubleSpeed;
bool zelda_fix;
uint8_t mbc1_banking_mode;
char *savefilename;

void gbmu_reset() {
	PC = SP = scanlineClocks = divTimerClocks = counterTimerClocks = IME = isBootROMUnmapped = isHalted = isStopped = ROMBankNumber = externalRAMBankNumber = doubleSpeed = mbc1_banking_mode = 0;
	memset(&regs, 0, sizeof(regs));
	memset(&gbc_backgr_palettes, 0xff, sizeof(gbc_backgr_palettes));
	if (!screen_pixels)
		screen_pixels = malloc(160 * 144 * 4);
	if (!screen_debug_tiles_pixels)
		screen_debug_tiles_pixels = malloc(SCREEN_DEBUG_TILES_W * SCREEN_DEBUG_TILES_H * 4);
	lcd_clear();
	free(mem);
	mem = calloc(1, 0x10000);
	free(gbc_wram);
	gbc_wram = calloc(1, 32*1024);
	free(external_ram);
	external_ram = calloc(1, 32*1024);
	free(vram);
	vram = calloc(1, 16*1024);
}

bool gbmu_load_rom(char *filename) {
	FILE *file;
	gbmu_reset();
	free(gamerom);
	gamerom = calloc(1, 8388608); // max 8 MB cartridges
	if (!(file = fopen(filename, "rb")))
		return false;
	gamerom_size = 0;
	while (fread(gamerom + gamerom_size, 1, 0x100, file) == 0x100)
		gamerom_size += 0x100;
	fclose(file);
	if (gamerom[0x143] == MODE_DMG)
		hardwareMode = MODE_DMG;
	else
		hardwareMode = MODE_GBC;
	set_mbc_type();
	zelda_fix = (strncmp(get_cartridge_title(), "ZELDA", 5) == 0);
	free(savefilename);
	if ((savefilename = malloc(strlen(filename) + 42))) {
		strcpy(savefilename, filename);
		strcat(savefilename, ".sav");
	}
	gbmu_load_ext_ram();
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
	// if (PC==0x4083)
	// 	exit(0);
	if (debug) {
		char flag_str[4];
		flag_str[0] = (regs.F & 0x80) ? 'Z' : '-';
		flag_str[1] = (regs.F & 0x40) ? 'N' : '-';
		flag_str[2] = (regs.F & 0x20) ? 'H' : '-';
		flag_str[3] = (regs.F & 0x10) ? 'C' : '-';
		printf("PC=%04X AF=%04X BC=%04X DE=%04X HL=%04X SP=%04X %.4s Opcode=%02X %02X lcdc=%02X IE=%02X IF=%02X cnt=%02X IME=%d ",
				PC, regs.AF, regs.BC, regs.DE, regs.HL, SP, flag_str, readByte(PC), readByte(PC+1), mem[0xFF40], IE, IF, mem[0xff05], IME);
		for (int i=0; i<4; i++) {
			printf("%04X ", ((uint16_t*)gbc_backgr_palettes)[i]);
		}
		for (int i=0; i<4; i++) {
			printf("%04X ", ((uint16_t*)gbc_sprite_palettes)[i]);
		}
		printf("\n");
		fflush(stdout);
	}

	if (isStopped && hardwareMode == MODE_GBC && mem[0xFF4D] & 1) {
		mem[0xFF4D] = 0x80;
		doubleSpeed = true;
		isStopped = false;
	}
	if (isStopped && ((readJoypadRegister() & 0xF) != 0xF))
		isStopped = false;
	if (isHalted && IE && IF)
		isHalted = false;

	if (isHalted || isStopped)
		clocksIncrement = 4;
	else {
		uint8_t opcode = fetchByte(); // Fetch opcode
		clocksIncrement = clocksTable[opcode];
		if (!instrs[opcode])
			exit(printf("Invalid opcode: %#x, PC=%04X\n", opcode, PC));
		instrs[opcode]();             // Execute
	}

	bool coincidenceFlag = false;
	bool isDisplayEnabled = mem[0xff40] & 0x80;
	int scanlineClockThreshold = doubleSpeed ? 912 : 456;
	int old = scanlineClocks;
	if ((scanlineClocks += clocksIncrement) >= scanlineClockThreshold) {
		scanlineClocks -= scanlineClockThreshold;
		mem[0xff44]++;
		if (mem[0xff44] > 153)
			mem[0xff44] = 0;
		LY = mem[0xff44];
		int LYC = mem[0xff45];
		coincidenceFlag = LY==LYC;
		LCDSTAT = coincidenceFlag ? (LCDSTAT|4) : (LCDSTAT&~4);
	}

	// Set LCD mode flag
	int lcd_mode;
	if (LY >= 144)
		lcd_mode = 1;
	else if (scanlineClocks >= 252)
		lcd_mode = 0;
	else if (scanlineClocks >= 80)
		lcd_mode = 3;
	else
		lcd_mode = 2;
	LCDSTAT &= ~3;
	LCDSTAT |= lcd_mode;

	if (old < 252 && scanlineClocks >= 252) { // H-Blank Interrupt
		if (LCDSTAT & 0x08)
			requestInterrupt(0x02);
	}
	if (scanlineClocks < old && LY == 144) { // V-Blank Interrupt
		if (LCDSTAT & 0x10)
			requestInterrupt(0x02);
		if (isDisplayEnabled)
			requestInterrupt(0x01);
		if (SHOW_BOOT_ANIMATION || isBootROMUnmapped) {
			if (!isDisplayEnabled)
				lcd_clear();
			isFrameReady = true;
		}
	}
	if (scanlineClocks < old && LY < 144) { // OAM Interrupt
		if (LCDSTAT & 0x20)
			requestInterrupt(0x02);
		lcd_draw_scanline();
	}
	if (scanlineClocks < old && coincidenceFlag) { // Coincidence Interrupt
		if (LCDSTAT & 0x40)
			requestInterrupt(0x02);
	}

	// Update timers
	if ((divTimerClocks += clocksIncrement) >= 256) {
		divTimerClocks -= 256;
		mem[0xff04]++;
	}

	if (mem[0xff07] & 0b100) {
		int threshold = (int[]){1024, 16, 64, 256}[mem[0xff07] & 0b11];
		if ((counterTimerClocks += clocksIncrement) >= threshold) {
			counterTimerClocks -= threshold;
			mem[0xff05]++;
			if (mem[0xff05] == 0) {
				mem[0xff05] = mem[0xff06];
				if (!zelda_fix)
					requestInterrupt(0x04); // Timer Interrupt
			}
		}
	}

	// Execute interrupts
	if (!isStopped && IME && IE && IF) {
		for (int i=0; i<5; i++) {
			int interrupt_vector = 0x40 + 8*i;
			int mask = 1<<i;
			if (IE & IF & mask) {
				push(PC);
				PC = interrupt_vector;
				IF &= ~mask;
				IME = 0;
				break;
			}
		}
	}
	return isFrameReady;
}