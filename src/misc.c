#include "emu.h"

uint8_t readJoypadRegister() {
	uint8_t ret = mem[0xff00] | 0xf;
	if ((mem[0xff00]&0x20)==0) {
		if (gbmu_keys[GBMU_START]) ret &= ~8;
		if (gbmu_keys[GBMU_SELECT]) ret &= ~4;
		if (gbmu_keys[GBMU_B]) ret &= ~2;
		if (gbmu_keys[GBMU_A]) ret &= ~1;
	}
	else if ((mem[0xff00]&0x10)==0) {
		if (gbmu_keys[GBMU_DOWN]) ret &= ~8;
		if (gbmu_keys[GBMU_UP]) ret &= ~4;
		if (gbmu_keys[GBMU_LEFT]) ret &= ~2;
		if (gbmu_keys[GBMU_RIGHT]) ret &= ~1;
	}
	return ret;
}

int min(int a, int b) {
	return ((a < b) ? a : b);
}

int max(int a, int b) {
	return ((a > b) ? a : b);
}

void requestInterrupt(uint8_t interrupt) {
	IF |= interrupt;
	cpuState = NORMAL;
}

void hdma_transfer_continue() {
	if (!hdma_remaining_size)
		return;
	if (mem[0xff44] >= 144)
		return;
	for (int i=0; i<16; i++)
		writeByte(hdma_dst+i, readByte(hdma_src+i));
	hdma_src += 16;
	hdma_dst += 16;
	hdma_remaining_size -= 16;
}

bool isDMG(uint8_t val) {
	return (val != 0x80) && (val != 0xC0);
}

void skip_boot_animation() {
	// If the logo data is valid, the animation stops when the bootrom is disabled.
	// Otherwise, it goes goes in an infinite loop at address 00E9 (DMG) or 00DB (GBC).
	while (true) {
		if (isBootROMUnmapped)
			return;
		if (gameMode==MODE_DMG && PC==0x00E9 && !FLAG_Z)
			return;
		if (gameMode==MODE_GBC && PC==0x00DB && !FLAG_Z)
			return;
		gbmu_run_one_instr();
	}
}
