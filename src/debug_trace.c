#include "emu.h"

void debug_trace() {
	static bool debug = false;

	// if (PC==0x0100)
	// 	debug = true;
	// if (PC==0x486e)
	// 	exit(0);
	if (!debug)
		return;
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
	printf("ff26=%02X ", readByte(0xff26));
	printf("spd=%d ", doubleSpeed);
	printf("\n");
	fflush(stdout);
}
