#include "emu.h"

bool gbmu_keys[GBMU_NUMBER_OF_KEYS];
uint32_t *screen_pixels, *screen_debug_tiles_pixels;
uint8_t *mem;
uint8_t *gbc_wram;
uint8_t *vram;
uint8_t *external_ram;
uint8_t *gamerom;
size_t   gamerom_size;
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
int numROMBanks, extRAMSize;
t_mode hardwareMode, gameMode;
int LY;
bool doubleSpeed;
bool zelda_fix;
uint8_t mbc1_banking_mode, mbc1_bank1_reg, mbc1_bank2_reg;
bool mbc_ram_enable;
char *savefilename;
bool show_boot_animation = true;
bool enable_save_file = true;

void gbmu_reset() {
	PC = SP = scanlineClocks = divTimerClocks = counterTimerClocks = IME = isBootROMUnmapped = isHalted = isStopped = ROMBankNumber = externalRAMBankNumber = doubleSpeed = mbc1_banking_mode = mbc1_bank1_reg = mbc1_bank2_reg = mbc_ram_enable = 0;
	gameMode = hardwareMode;
	memset(&regs, 0, sizeof(regs));
	memset(&gbc_backgr_palettes, 0xff, sizeof(gbc_backgr_palettes));
	if (!screen_pixels) {
		if (!(screen_pixels = malloc(160 * 144 * 4)))
			exit(printf("malloc error\n"));
	}
	if (!screen_debug_tiles_pixels) {
		if (!(screen_debug_tiles_pixels = malloc(SCREEN_DEBUG_TILES_W * SCREEN_DEBUG_TILES_H * 4)))
			exit(printf("malloc error\n"));
	}
	if (!external_ram) {
		if (!(external_ram = calloc(1, 32*1024)))
			exit(printf("malloc error\n"));
	}
	lcd_clear();
	free(mem);
	if (!(mem = calloc(1, 0x10000)))
		exit(printf("malloc error\n"));
	free(gbc_wram);
	if (!(gbc_wram = calloc(1, 32*1024)))
		exit(printf("malloc error\n"));
	free(vram);
	if (!(vram = calloc(1, 16*1024)))
		exit(printf("malloc error\n"));
}

bool gbmu_load_rom(char *filename) {
	gbmu_reset();
	free(gamerom);
	if (!(gamerom = malloc(8*1024*1024))) // max 8 MB cartridges
		exit(printf("malloc error\n"));
	memset(gamerom, 0xFF, 8*1024*1024);
	FILE *file;
	if ((file = fopen(filename, "rb"))) {
		gamerom_size = 0;
		while (fread(gamerom + gamerom_size, 1, 0x100, file) == 0x100)
			if ((gamerom_size += 0x100) >= 8*1024*1024)
				break;
		fclose(file);
	} else {
		show_boot_animation = true;
	}
	hardwareMode = isDMG(gamerom[0x143]) ? MODE_DMG : MODE_GBC;
	gameMode = hardwareMode;
	set_mbc_type(file ? gamerom[0x147] : 0);
	zelda_fix = (strncmp(get_cartridge_title(), "ZELDA", 5) == 0);
	free(savefilename);
	if (filename && (savefilename = malloc(strlen(filename) + 42))) {
		strcpy(savefilename, filename);
		strcat(savefilename, ".sav");
	}
	gbmu_load_ext_ram();
	numROMBanks = (2 << min(8, gamerom[0x148]));
	extRAMSize = get_cartridge_ram_size();
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
	// if (!debug && PC==0x01da)
	// 	debug = true;
	// if (PC==0x486e)
	// 	exit(0);
	if (debug) {
		char flag_str[4];
		flag_str[0] = (regs.F & 0x80) ? 'Z' : '-';
		flag_str[1] = (regs.F & 0x40) ? 'N' : '-';
		flag_str[2] = (regs.F & 0x20) ? 'H' : '-';
		flag_str[3] = (regs.F & 0x10) ? 'C' : '-';
		printf("PC=%04X AF=%04X BC=%04X DE=%04X HL=%04X SP=%04X %.4s Opcode=%02X %02X lcdc=%02X IE=%02X IF=%02X cnt=%02X IME=%d ",
				PC, regs.AF, regs.BC, regs.DE, regs.HL, SP, flag_str, readByte(PC), readByte(PC+1), mem[0xFF40], IE, IF, mem[0xff05], IME);
		// for (int i=0; i<4; i++) {
		// 	printf("%04X ", ((uint16_t*)gbc_backgr_palettes)[i]);
		// }
		// for (int i=0; i<4; i++) {
		// 	printf("%04X ", ((uint16_t*)gbc_sprite_palettes)[i]);
		// }
		printf("\n");
		fflush(stdout);
	}

	if (isStopped && gameMode == MODE_GBC && mem[0xFF4D] & 1) {
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
		hdma_transfer_continue();
	}
	if (scanlineClocks < old && LY == 144) { // V-Blank Interrupt
		if (LCDSTAT & 0x10)
			requestInterrupt(0x02);
		if (isDisplayEnabled)
			requestInterrupt(0x01);
		if (show_boot_animation || isBootROMUnmapped) {
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