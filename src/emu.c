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
int ROMBankNumber, externalRAMBankNumber;
int numROMBanks, extRAMSize;
t_mode hardwareMode, gameMode;
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
	isFrameReady = false;
	while (!isFrameReady) {
		gbmu_run_one_instr();
	}
}

void gbmu_run_one_instr() {
	if (!isBootROMUnmapped)
		gameMode = hardwareMode;
	if (!gamerom)
		show_boot_animation = true;

	if (isStopped && gameMode == MODE_GBC && mem[0xFF4D] & 1) {
		mem[0xFF4D] = 0x80;
		doubleSpeed = true;
		isStopped = false;
	}
	if (isStopped && ((readJoypadRegister() & 0xF) != 0xF))
		isStopped = false;
	if (isHalted && IE && IF)
		isHalted = false;

	clocksIncrement = 0;

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
				clocksIncrement += 5*4; // +5 cycles (see TCAGBD.pdf)
				break;
			}
		}
	}

	debug_trace();

	uint8_t opcode;
	if (isHalted || isStopped) {
		opcode = 0; // NOP
	} else {
		opcode = fetchByte(); // Fetch opcode
	}
	if (!instrs[opcode])
		exit(printf("Invalid opcode: %#x, PC=%04X\n", opcode, PC));
	clocksIncrement += cycleTable[opcode] * 4;
	instrs[opcode](); // Execute

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

	snd_update();

	lcd_update();
}
