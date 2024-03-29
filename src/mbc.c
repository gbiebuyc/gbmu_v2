#include "emu.h"

uint8_t (*mbc_read)(uint16_t addr);
void    (*mbc_write)(uint16_t addr, uint8_t val);
uint8_t (*mbc_readExtRAM)(uint16_t addr);
void    (*mbc_writeExtRAM)(uint16_t addr, uint8_t val);
uint16_t hdma_src, hdma_dst;
size_t   hdma_remaining_size;

uint8_t readByte(uint16_t addr) {
	if (addr<0x100 && !isBootROMUnmapped && hardwareMode==MODE_DMG)
		return bootrom_dmg[addr];
	if (addr<0x900 && (addr >= 0x200 || addr < 0x100) &&
			!isBootROMUnmapped && hardwareMode==MODE_GBC)
		return bootrom_gbc[addr];
	else if (addr<0x8000) // ROM
		return mbc_read(addr);
	else if (addr<0xA000) // VRAM
		return vram[addr-0x8000 + 0x2000*(mem[0xFF4F]&1)];
	else if (addr<0xC000) // External RAM
		return mbc_readExtRAM(addr);
	else if (addr<0xD000 ||
			(addr<0xE000 && hardwareMode==MODE_DMG)) // Work RAM Bank 0-1
		return wram[addr-0xC000];
	else if (addr<0xE000 && hardwareMode==MODE_GBC)  // Work RAM Bank 1-7 (GBC)
		return wram[addr-0xD000 + 0x1000*max(1, mem[0xFF70]&7)];
	else if (addr<0xFE00) // Same as C000-DDFF (ECHO)
		return readByte(addr-0x2000);
	else if (addr>=0xFF10 && addr<=0xFF26)
		return snd_readRegister(addr);
	else if (addr==0xff00)
		return readJoypadRegister();
	else if (addr==0xff69) // GBC Background Palette Data
		return gbc_backgr_palettes[mem[0xff68] & 0x3f];
	else if (addr==0xff6b) // GBC Sprite Palette Data
		return gbc_sprite_palettes[mem[0xff6a] & 0x3f];
	else if (addr==0xff55) { // HDMA
		if (!hdma_remaining_size)
			return 0xFF;
		return (hdma_remaining_size / 16) - 1;
	}
	else
		return mem[addr];
}

void writeByte(uint16_t addr, uint8_t val) {
	if (addr<0x8000)
		mbc_write(addr, val);
	else if (addr<0xA000) // VRAM
		vram[addr-0x8000 + 0x2000*(mem[0xFF4F]&1)] = val;
	else if (addr<0xC000) // External RAM
		mbc_writeExtRAM(addr, val);
	else if (addr<0xD000 ||
			(addr<0xE000 && hardwareMode==MODE_DMG)) // Work RAM Bank 0-1
		wram[addr-0xC000] = val;
	else if (addr<0xE000 && hardwareMode==MODE_GBC)  // Work RAM Bank 1-7 (GBC)
		wram[addr-0xD000 + 0x1000*max(1, mem[0xFF70]&7)] = val;
	else if (addr<0xFE00) // Same as C000-DDFF (ECHO)
		writeByte(addr-0x2000, val);
	else if (addr>=0xFF10 && addr<=0xFF26)
		snd_writeRegister(addr, val);
	if (addr==0xff50) {
		isBootROMUnmapped = true;
		if (isROMLoaded && isDMG(gamerom[0x143]))
			gameMode = MODE_DMG;
	}
	else if (addr==0xff02 && val==0x81) // Serial Data Transfer (Link Cable)
		write(STDOUT_FILENO, mem+0xff01, 1);
	else if (addr==0xff46) { // DMA Transfer
		uint16_t source = min(0xF1, (uint16_t)val) << 8;
		for (int i=0; i<=0x9F; i++)
			mem[0xfe00+i] = readByte(source+i);
		// TODO: Transfer takes how many clocks?
	}
	else if (addr==0xff04) { // Divider Register
		mem[0xff04] = 0;
		divTimerClocks = 0;
		counterTimerClocks = 0;
	}
	else if (addr==0xff69) { // GBC Background Palette Data
		int palette_index = mem[0xff68] & 0x3f;
		bool auto_increment = mem[0xff68] >> 7;
		gbc_backgr_palettes[palette_index] = val;
		if (auto_increment) {
			mem[0xff68]++;
			mem[0xff68] &= 0x3f;
			mem[0xff68] |= 0x80;
		}
	}
	else if (addr==0xff6b) { // GBC Sprite Palette Data
		int palette_index = mem[0xff6a] & 0x3f;
		bool auto_increment = mem[0xff6a] >> 7;
		gbc_sprite_palettes[palette_index] = val;
		if (auto_increment) {
			mem[0xff6a]++;
			mem[0xff6a] &= 0x3f;
			mem[0xff6a] |= 0x80;
		}
	}
	else if (addr==0xff51) // HDMA Source High byte
		hdma_src = (hdma_src&0xFF) | (((uint16_t)val)<<8);
	else if (addr==0xff52) // HDMA Source Low byte
		hdma_src = ((hdma_src&0xFF00) | val) & 0xFFF0;
	else if (addr==0xff53) // HDMA Destination High byte
		hdma_dst = (hdma_dst&0xFF) | (((uint16_t)val)<<8);
	else if (addr==0xff54) // HDMA Destination Low byte
		hdma_dst = ((hdma_dst&0xFF00) | val) & 0xFFF0;
	else if (addr==0xff55) {
		hdma_dst &= 0X1FFF;
		hdma_dst |= 0x8000;
		int size = ((val & 0x7F) + 1) * 16;
		if (val & 0x80) { // HDMA
			hdma_remaining_size = size;
		}
		else { // GDMA
			// printf("GDMA src=%04X dst=%04X\n", hdma_src, hdma_dst);
			// while (size--) {
			// 	writeByte(hdma_dst++, readByte(hdma_src++));
			// }
			for (int i=0; i<size; i++)
				writeByte(hdma_dst+i, readByte(hdma_src+i));
		}
	}
	else if (addr==0xff40) {
		mem[addr] = val;
		if (!(val & 0x80)) { // LCD is disabled
			mem[0xff41] &= ~3; // Mode = 0
			mem[0xff44] = 0;   // LY = 0
			scanlineClocks = 0;
			lcd_clear();
		}
	}
	else
		mem[addr] = val;
}

uint16_t readWord(uint16_t addr) {
	return ((uint16_t)readByte(addr+1))<<8 | readByte(addr);
}

void writeWord(uint16_t addr, uint16_t val) {
	writeByte(addr, val);
	writeByte(addr+1, val>>8);
}

/*
 * No cartridge
 */

uint8_t noCartridge_read(uint16_t addr) {
	return 0xFF;
}

void    noCartridge_write(uint16_t addr, uint8_t val) {
}

uint8_t noCartridge_readExtRAM(uint16_t addr) {
}

void    noCartridge_writeExtRAM(uint16_t addr, uint8_t val) {
}

/*
 * ROM Only
 */

uint8_t ROMOnly_read(uint16_t addr) {
	return gamerom[addr];
}

void    ROMOnly_write(uint16_t addr, uint8_t val) {
}

uint8_t ROMOnly_readExtRAM(uint16_t addr) {
	return external_ram[addr-0xA000];
}

void    ROMOnly_writeExtRAM(uint16_t addr, uint8_t val) {
	external_ram[addr-0xA000] = val;
}

/*
 * MBC1
 */

uint8_t MBC1_read(uint16_t addr) {
	mbc1_bank1_reg &= 0b11111;
	mbc1_bank2_reg &= 0b11;
	if (mbc1_bank1_reg == 0)
		mbc1_bank1_reg++;
	if (addr < 0x4000 && mbc1_banking_mode == 0)
		ROMBankNumber = 0;
	else if (addr < 0x4000 && mbc1_banking_mode == 1)
		ROMBankNumber = mbc1_bank2_reg<<5;
	else {
		ROMBankNumber = mbc1_bank1_reg | (mbc1_bank2_reg<<5);
		addr -= 0x4000;
	}
	ROMBankNumber &= numROMBanks-1;
	return gamerom[addr + 0x4000*ROMBankNumber];
}

void    MBC1_write(uint16_t addr, uint8_t val) {
	if (addr<0x2000) // RAM Enable
		mbc_ram_enable = ((val & 0xF) == 0b1010);
	else if (addr<0x4000) // ROM Bank Number
		mbc1_bank1_reg = val & 0b11111;
	else if (addr<0x6000) { // RAM Bank Number - or - Upper Bits of ROM Bank Number
		mbc1_bank2_reg = val & 0b11;
	}
	else if (addr<0x8000) // ROM/RAM Mode Select
		mbc1_banking_mode = val & 1;
}

uint8_t MBC1_readExtRAM(uint16_t addr) {
	if (!mbc_ram_enable)
		return 0xff;
	addr -= 0xA000;
	if (mbc1_banking_mode == 1)
		addr += 0x2000*mbc1_bank2_reg;
	addr &= (extRAMSize - 1);
	return external_ram[addr];
}

void    MBC1_writeExtRAM(uint16_t addr, uint8_t val) {
	if (!mbc_ram_enable)
		return;
	addr -= 0xA000;
	if (mbc1_banking_mode == 1)
		addr += 0x2000*mbc1_bank2_reg;
	addr &= (extRAMSize - 1);
	external_ram[addr] = val;
}

/*
 * MBC2
 */

uint8_t MBC2_read(uint16_t addr) {
	if (addr < 0x4000)
		return gamerom[addr];
	if (ROMBankNumber==0)
		ROMBankNumber++;
	ROMBankNumber &= numROMBanks-1;
	return gamerom[addr-0x4000 + 0x4000*ROMBankNumber];
}

void    MBC2_write(uint16_t addr, uint8_t val) {
	if (addr < 0x4000) {
		if (addr & 0x100)
			ROMBankNumber = val & 0xF;
		else 
			mbc_ram_enable = ((val & 0xF) == 0b1010);
	}
}

uint8_t MBC2_readExtRAM(uint16_t addr) {
	if (!mbc_ram_enable)
		return 0xff;
	return external_ram[(addr-0xA000) & 0x1FF] | 0xF0;
}

void    MBC2_writeExtRAM(uint16_t addr, uint8_t val) {
	if (mbc_ram_enable)
		external_ram[(addr-0xA000) & 0x1FF] = val;
}

/*
 * MBC3
 */

uint8_t MBC3_read(uint16_t addr) {
	if (addr < 0x4000)
		return gamerom[addr];
	return gamerom[addr-0x4000 + 0x4000*max(1,ROMBankNumber)];
}

void    MBC3_write(uint16_t addr, uint8_t val) {
	if (addr<0x2000) // External RAM enable
		;
	else if (addr<0x4000)  // ROM Bank Number
		ROMBankNumber = val;
	else if (addr<0x6000) { // External RAM Bank Number
		if (val >= 8)
			; // printf("warning: write at %#x - rtc register not implemented\n", addr);
		else
			externalRAMBankNumber = val;
	}
	else if (addr<0x8000)
		; //printf("warning: write at %#x - Latch Clock Data not implemented\n", addr);
}

uint8_t MBC3_readExtRAM(uint16_t addr) {
	return external_ram[addr-0xA000 + 0x2000*externalRAMBankNumber];
}

void    MBC3_writeExtRAM(uint16_t addr, uint8_t val) {
	external_ram[addr-0xA000 + 0x2000*externalRAMBankNumber] = val;
}

/*
 * MBC5
 */

uint8_t MBC5_read(uint16_t addr) {
	if (addr < 0x4000)
		return gamerom[addr];
	ROMBankNumber &= numROMBanks-1;
	return gamerom[addr-0x4000 + 0x4000*max(1, ROMBankNumber)];
}

void    MBC5_write(uint16_t addr, uint8_t val) {
	if (addr<0x2000) // RAM Enable
		;
	else if (addr<0x3000) { // ROM Bank (Low bits)
		ROMBankNumber &= 0xFF00;
		ROMBankNumber |= val;
	}
	else if (addr<0x4000) { // ROM Bank (High bits)
		ROMBankNumber &= 0x00FF;
		ROMBankNumber |= (((uint16_t)val&1)<<8);
	}
	else if (addr<0x6000) { // RAM Bank/Enable Rumble
		externalRAMBankNumber = (val & 0xF);
	}
}

uint8_t MBC5_readExtRAM(uint16_t addr) {
	return external_ram[addr-0xA000 + 0x2000*externalRAMBankNumber];
}

void    MBC5_writeExtRAM(uint16_t addr, uint8_t val) {
	external_ram[addr-0xA000 + 0x2000*externalRAMBankNumber] = val;
}

void set_mbc_type() {
	if (!isROMLoaded) {
		mbc_read        = noCartridge_read;
		mbc_readExtRAM  = noCartridge_readExtRAM;
		mbc_write       = noCartridge_write;
		mbc_writeExtRAM = noCartridge_writeExtRAM;
		return;
	}
	uint8_t type = gamerom[0x147];
	if (type>=1 && type<=3) {
		printf("debug: MBC1\n");
		mbc_read        = MBC1_read;
		mbc_readExtRAM  = MBC1_readExtRAM;
		mbc_write       = MBC1_write;
		mbc_writeExtRAM = MBC1_writeExtRAM;
	}
	else if (type>=5 && type<=6) {
		printf("debug: MBC2\n");
		mbc_read        = MBC2_read;
		mbc_readExtRAM  = MBC2_readExtRAM;
		mbc_write       = MBC2_write;
		mbc_writeExtRAM = MBC2_writeExtRAM;
	}
	else if (type>=0x0F && type<=0x13) {
		printf("debug: MBC3\n");
		mbc_read        = MBC3_read;
		mbc_readExtRAM  = MBC3_readExtRAM;
		mbc_write       = MBC3_write;
		mbc_writeExtRAM = MBC3_writeExtRAM;
	}
	else if (type>=0x19 && type<=0x1E) {
		printf("debug: MBC5\n");
		mbc_read        = MBC5_read;
		mbc_readExtRAM  = MBC5_readExtRAM;
		mbc_write       = MBC5_write;
		mbc_writeExtRAM = MBC5_writeExtRAM;
	}
	else {
		if (type==0 || type==8 || type==9)
			printf("debug: rom only\n");
		else
			printf("Unsupported MBC chip: %s\n", get_cartridge_type());
		mbc_read        = ROMOnly_read;
		mbc_readExtRAM  = ROMOnly_readExtRAM;
		mbc_write       = ROMOnly_write;
		mbc_writeExtRAM = ROMOnly_writeExtRAM;
	}
}