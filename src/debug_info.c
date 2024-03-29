#include "emu.h"

char *gbmu_debug_info() {
	static char buf[2000];
	char *b = buf;
	b += sprintf(b, "Title: %s\n", cartridgeTitle);
	b += sprintf(b, "Type: %s\n", cartridgeTypeStr);
	b += sprintf(b, "ROM banks: %d\n", numROMBanks);
	b += sprintf(b, "ROM size: %u bytes\n", ((unsigned)numROMBanks) * 0x4000);
	b += sprintf(b, "RAM size: %d bytes\n", extRAMSize);
	if (hardwareMode==MODE_DMG)
		b += sprintf(b, "Emulating as DMG            \n");
	else if (hardwareMode==MODE_GBC && gameMode==MODE_DMG)
		b += sprintf(b, "Emulating as GBC in DMG mode\n");
	else
		b += sprintf(b, "Emulating as GBC            \n");
	b += sprintf(b, "\n");
	char flag_str[4];
	flag_str[0] = (regs.F & 0x80) ? 'Z' : '-';
	flag_str[1] = (regs.F & 0x40) ? 'N' : '-';
	flag_str[2] = (regs.F & 0x20) ? 'H' : '-';
	flag_str[3] = (regs.F & 0x10) ? 'C' : '-';
	b += sprintf(b, "af= %04X  ", regs.AF);
	b += sprintf(b, "lcdc=%02X\n", mem[0xff40]);
	b += sprintf(b, "bc= %04X  ", regs.BC);
	b += sprintf(b, "stat=%02X\n", mem[0xff41]);
	b += sprintf(b, "de= %04X  ", regs.DE);
	b += sprintf(b, "ly=  %02X\n", mem[0xff44]);
	b += sprintf(b, "hl= %04X  ", regs.HL);
	b += sprintf(b, "tima=%02X\n", mem[0xff05]);
	b += sprintf(b, "sp= %04X  ", SP);
	b += sprintf(b, "ie=  %02X\n", IE);
	b += sprintf(b, "pc= %04X  ", PC);
	b += sprintf(b, "if=  %02X\n", IF);
	b += sprintf(b, "ime=%X     ", IME);
	b += sprintf(b, "spd= %X\n", doubleSpeed);
	b += sprintf(b, "Next instr: %s\n", disassemble_instr(PC));
	b += sprintf(b, "Flags: %.4s\n", flag_str);
	b += sprintf(b,
		"    _                     \n"
		"  _|W|_               (K) \n"
		" |A   D|          (J)     \n"
		"  ‾|S|‾                   \n"
		"    ‾  _____   _____      \n"
		"      |Shift| |Enter|     \n"
		"       ‾‾‾‾‾   ‾‾‾‾‾      ");
	return buf;
}

char *get_cartridge_title() {
	static char buf[16 + 1]; // 1 for null byte

	int i=0;
	while (i < 16) {
		char c = gamerom[0x134 + i];
		if (!isprint(c))
			break;
		buf[i] = c;
		i++;
	}
	buf[i] = 0;
	return buf;
}

char *get_cartridge_type() {
	switch (gamerom[0x147]) {
		case 0x00: return "ROM ONLY";
		case 0x01: return "MBC1";
		case 0x02: return "MBC1+RAM";
		case 0x03: return "MBC1+RAM+BATTERY";
		case 0x05: return "MBC2";
		case 0x06: return "MBC2+BATTERY";
		case 0x08: return "ROM+RAM";
		case 0x09: return "ROM+RAM+BATTERY";
		case 0x0B: return "MMM01";
		case 0x0C: return "MMM01+RAM";
		case 0x0D: return "MMM01+RAM+BATTERY";
		case 0x0F: return "MBC3+TIMER+BATTERY";
		case 0x10: return "MBC3+TIMER+RAM+BATTERY";
		case 0x11: return "MBC3";
		case 0x12: return "MBC3+RAM";
		case 0x13: return "MBC3+RAM+BATTERY";
		case 0x15: return "MBC4";
		case 0x16: return "MBC4+RAM";
		case 0x17: return "MBC4+RAM+BATTERY";
		case 0x19: return "MBC5";
		case 0x1A: return "MBC5+RAM";
		case 0x1B: return "MBC5+RAM+BATTERY";
		case 0x1C: return "MBC5+RUMBLE";
		case 0x1D: return "MBC5+RUMBLE+RAM";
		case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
		case 0xFC: return "POCKET CAMERA";
		case 0xFD: return "BANDAI TAMA5";
		case 0xFE: return "HuC3";
		case 0xFF: return "HuC1+RAM+BATTERY";
	}
	return "Unknown";
}

int get_cartridge_ram_size() {
	uint8_t type = gamerom[0x147];
	bool isMBC2 = (type==5 || type==6);
	switch (gamerom[0x149]) {
		case 0x00: return isMBC2 ? 512 : 0;
		case 0x01: return 2 * 1024;
		case 0x02: return 8 * 1024;
		case 0x03: return 32 * 1024;
	}
	return 0;
}

bool get_cartridge_has_battery() {
	switch (gamerom[0x147]) {
		case 0x03:
		case 0x06:
		case 0x09:
		case 0x0D:
		case 0x0F:
		case 0x10:
		case 0x13:
		case 0x17:
		case 0x1B:
		case 0x1E:
		case 0xFF:
			return true;
	}
	return false;
}