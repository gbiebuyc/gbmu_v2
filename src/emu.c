#include "emu.h"

bool gbmu_keys[GBMU_NUMBER_OF_KEYS];
uint32_t *screen_pixels, *screen_debug_tiles_pixels;
uint8_t *mem;
uint8_t *gbc_wram;
uint8_t *vram;
uint8_t *external_ram;
uint8_t *gamerom;
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
char *savefilename, *cartridgeTitle, *cartridgeTypeStr;
bool show_boot_animation = true;
bool enable_save_file = true;
bool cartridgeHasBattery;

void gbmu_reset() {
	PC = SP = scanlineClocks = divTimerClocks = counterTimerClocks = IME = isBootROMUnmapped = isHalted = isStopped = ROMBankNumber = externalRAMBankNumber = doubleSpeed = mbc1_banking_mode = mbc1_bank1_reg = mbc1_bank2_reg = mbc_ram_enable = 0;
	gameMode = hardwareMode;
	memset(&regs, 0, sizeof(regs));
	memset(&gbc_backgr_palettes, 0xff, sizeof(gbc_backgr_palettes));
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
	if (external_ram)
		gbmu_save_ext_ram();
	if (!external_ram && !(external_ram = calloc(1, 32*1024)))
		exit(printf("malloc error\n"));
	if (!screen_pixels && !(screen_pixels = malloc(160 * 144 * 4)))
		exit(printf("malloc error\n"));
	if (!screen_debug_tiles_pixels && !(screen_debug_tiles_pixels = malloc(SCREEN_DEBUG_TILES_W * SCREEN_DEBUG_TILES_H * 4)))
		exit(printf("malloc error\n"));
	gbmu_reset();
	free(gamerom); gamerom = NULL;
	free(savefilename); savefilename = NULL;
	set_mbc_type();
	hardwareMode = MODE_GBC;
	cartridgeTitle = cartridgeTypeStr = NULL;
	numROMBanks = extRAMSize = zelda_fix = cartridgeHasBattery = 0;
	FILE *file;
	if (!filename || !(file = fopen(filename, "rb"))) {
		return false;
	}
	if (!(gamerom = malloc(8*1024*1024))) { // max 8 MB cartridges
		fclose(file); exit(printf("malloc error\n"));
	}
	int gamerom_size = 0;
	while (fread(gamerom + gamerom_size, 1, 0x100, file) == 0x100)
		if ((gamerom_size += 0x100) >= 8*1024*1024)
			break;
	fclose(file);
	if (filename && (savefilename = malloc(strlen(filename) + 42))) {
		strcpy(savefilename, filename);
		strcat(savefilename, ".sav");
	}
	numROMBanks = (2 << min(8, gamerom[0x148]));
	extRAMSize = get_cartridge_ram_size();
	cartridgeTitle = get_cartridge_title();
	cartridgeTypeStr = get_cartridge_type();
	cartridgeHasBattery = get_cartridge_has_battery();
	zelda_fix = (strncmp(cartridgeTitle, "ZELDA", 5) == 0);
	set_mbc_type();
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
	if (!isBootROMUnmapped)
		gameMode = hardwareMode;
	if (!gamerom)
		show_boot_animation = true;

	// if (!debug && PC==0x0100)
	// 	debug = true;
	// if (PC==0x486e)
	// 	exit(0);
	if (debug) {
		char flag_str[4];
		flag_str[0] = (regs.F & 0x80) ? 'Z' : '-';
		flag_str[1] = (regs.F & 0x40) ? 'N' : '-';
		flag_str[2] = (regs.F & 0x20) ? 'H' : '-';
		flag_str[3] = (regs.F & 0x10) ? 'C' : '-';
		printf("PC=%04X ", PC);
		printf("%-10s ", disassemble_instr(PC));
		printf("AF=%04X ", regs.AF);
		printf("BC=%04X ", regs.BC);
		printf("DE=%04X ", regs.DE);
		printf("HL=%04X ", regs.HL);
		printf("SP=%04X ", SP);
		printf("%.4s ", flag_str);
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
		clocksIncrement = cycleTable[opcode] * 4;
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

	if (old < 252 && scanlineClocks >= 252 && LY < 144) { // H-Blank Interrupt
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
		counterTimerClocks += clocksIncrement;
		while (counterTimerClocks >= threshold) {
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