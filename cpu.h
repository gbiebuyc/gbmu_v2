#include <unistd.h>

uint8_t readByte(uint16_t addr) {
	if (addr<0x100 && !isBootROMUnmapped)
		return bootrom[addr];
	else if (addr<0x4000) // ROM bank 00
		return gamerom[addr];
	else if (addr<0x8000) // ROM Bank 01~NN
		return gamerom[addr-0x4000 + 0x4000*selectedROMBank];
	else if (addr==0xff00) {
		const uint8_t *state = SDL_GetKeyboardState(NULL);
		uint8_t ret = mem[0xff00] | 0xf;
		if ((mem[0xff00]&0x20)==0) {
			if (state[JOYPAD_START]) ret &= ~8;
			if (state[JOYPAD_SELECT]) ret &= ~4;
			if (state[JOYPAD_B]) ret &= ~2;
			if (state[JOYPAD_A]) ret &= ~1;
		}
		else if ((mem[0xff00]&0x10)==0) {
			if (state[JOYPAD_DOWN]) ret &= ~8;
			if (state[JOYPAD_UP]) ret &= ~4;
			if (state[JOYPAD_LEFT]) ret &= ~2;
			if (state[JOYPAD_RIGHT]) ret &= ~1;
		}
		return ret;
	}
	else if (addr>=0x8000)
		return mem[addr];
}

uint16_t readWord(uint16_t addr) {
	return ((uint16_t)readByte(addr+1))<<8 | readByte(addr);
}

void writeByte(uint16_t addr, uint8_t val) {
	if (addr==0xff50)
		isBootROMUnmapped = true;
	else if (addr==0xff02 && val==0x81) // Serial Data Transfer (Link Cable)
		write(STDOUT_FILENO, mem+0xff01, 1);
	else if (addr==0xff46) // DMA Transfer
		memcpy(mem+0xfe00, mem+((uint16_t)val<<8), 0xa0);
	else if (addr==0xff04) // Divider Register
		mem[0xff04] = 0;
	else if (addr>=0x2000 && addr<0x4000)
		selectedROMBank = val & 0x1f;
	else if (addr>=0x8000)
		mem[addr] = val;
	else
		printf("warning: write at %#x\n", addr);
}

void writeWord(uint16_t addr, uint16_t val) {
	writeByte(addr, val);
	writeByte(addr+1, val>>8);
}

uint8_t fetchByte() {
	return readByte(PC++);
}

uint16_t fetchWord() {
	return readWord((PC+=2)-2);
}

void setFlags(char Z, char N, char H, char C) {
	if (Z==1) regs.F |= 0x80; else if (Z==0) regs.F &= ~0x80;
	if (N==1) regs.F |= 0x40; else if (N==0) regs.F &= ~0x40;
	if (H==1) regs.F |= 0x20; else if (H==0) regs.F &= ~0x20;
	if (C==1) regs.F |= 0x10; else if (C==0) regs.F &= ~0x10;
}

void add(int b) {
	int a = regs.A;
	setFlags(!((a+b)&0xff), 0, ((a&0xf)+(b&0xf))>0xf, (a+b)>0xff);
	regs.A += b;
}

void addc(int b) {
	int a = regs.A;
	int c = FLAG_C;
	setFlags(!((a+b+c)&0xff), 0, ((a&0xf)+(b&0xf)+c)>0xf, (a+b+c)>0xff);
	regs.A += b + c;
}

void compare(int b) {
	int a = regs.A;
	setFlags(a==b, 1, ((a&0xf)-(b&0xf))<0, b>a);
}

void sub(int b) {
	compare(b);
	regs.A -= b;
}

void subc(int b) {
	int a = regs.A;
	int c = FLAG_C;
	setFlags(!((a-b-c)&0xff), 1, ((a&0xf)-(b&0xf)-c)<0, (a-b-c)<0);
	regs.A = regs.A - b - c;
}

void and(uint8_t operand) {
	regs.A &= operand;
	setFlags(!regs.A, 0, 1, 0);
}

void xor(uint8_t operand) {
	regs.A ^= operand;
	setFlags(!regs.A, 0, 0, 0);
}

void or(uint8_t operand) {
	regs.A |= operand;
	setFlags(!regs.A, 0, 0, 0);
}

void push(uint16_t operand) {
	*(uint16_t*)(mem+(SP-=2)) = operand;
}

uint16_t pop() {
	return *(uint16_t*)(mem+(SP+=2)-2);
}

uint8_t rotate(uint8_t operand, char mnemonic[4]) {
	int shifted_out = (mnemonic[1]=='L') ? operand>>7 : operand&1;
	int shifted_in = (mnemonic[2]=='C') ? shifted_out : FLAG_C;
	operand = (mnemonic[1]=='L') ? operand<<1|shifted_in : operand>>1|shifted_in<<7;
	setFlags((mnemonic[3]=='A') ? 0 : !operand, 0, 0, shifted_out);
	return operand;
}

uint8_t SLA(uint8_t operand) {
	uint8_t carry = operand>>7;
	setFlags(!(operand<<=1), 0, 0, carry);
	return operand;
}

uint8_t SRA(uint8_t operand) {
	uint8_t carry = operand&1;
	operand = (operand>>1) | (operand&0x80);
	setFlags(!operand, 0, 0, carry);
	return operand;
}

uint8_t SRL(uint8_t operand) {
	setFlags(!(operand>>1), 0, 0, operand&1);
	return operand >> 1;
}

uint8_t swap(uint8_t operand) {
	operand = (operand>>4) | (operand<<4);
	setFlags(!operand, 0, 0, 0);
	return operand;
}

uint8_t testBit(uint8_t operand, int bit) {
	setFlags(!(operand & (1 << bit)), 0, 1, '-');
}

uint16_t addHL(uint16_t n) {
	setFlags('-', 0, ((regs.HL&0xfff)+(n&0xfff))>0xfff, (((uint32_t)regs.HL)+n)>0xffff);
	regs.HL += n;
}

void ins00() {}
void ins01() {regs.BC = fetchWord(); }
void ins11() {regs.DE = fetchWord(); }
void ins21() {regs.HL = fetchWord(); }
void ins31() {SP = fetchWord(); }
void ins02() {writeByte(regs.BC, regs.A); }
void ins12() {writeByte(regs.DE, regs.A); }
void ins22() {writeByte(regs.HL++, regs.A); }
void ins32() {writeByte(regs.HL--, regs.A); }
void ins03() {regs.BC++; }
void ins13() {regs.DE++; }
void ins23() {regs.HL++; }
void ins33() {SP++; }
void ins0B() {regs.BC--; }
void ins1B() {regs.DE--; }
void ins2B() {regs.HL--; }
void ins3B() {SP--; }
void setFlagsInc(uint8_t operand) {setFlags(operand==0xff, 0, (((operand&0xf)+1)&0x10)==0x10, '-'); }
void ins04() {setFlagsInc(regs.B++); }
void ins14() {setFlagsInc(regs.D++); }
void ins24() {setFlagsInc(regs.H++); }
void ins34() {uint8_t val=readByte(regs.HL); setFlagsInc(val); writeByte(regs.HL, val+1); }
void ins0C() {setFlagsInc(regs.C++); }
void ins1C() {setFlagsInc(regs.E++); }
void ins2C() {setFlagsInc(regs.L++); }
void ins3C() {setFlagsInc(regs.A++); }
void setFlagsDec(uint8_t operand) {setFlags(operand==1, 1, ((((int)operand)&0xf)-1)<0, '-'); }
void ins05() {setFlagsDec(regs.B--); }
void ins15() {setFlagsDec(regs.D--); }
void ins25() {setFlagsDec(regs.H--); }
void ins35() {uint8_t val=readByte(regs.HL); setFlagsDec(val); writeByte(regs.HL, val-1); }
void ins0D() {setFlagsDec(regs.C--); }
void ins1D() {setFlagsDec(regs.E--); }
void ins2D() {setFlagsDec(regs.L--); }
void ins3D() {setFlagsDec(regs.A--); }
void ins06() {regs.B = fetchByte(); }
void ins16() {regs.D = fetchByte(); }
void ins26() {regs.H = fetchByte(); }
void ins36() {writeByte(regs.HL, fetchByte()); }
void ins0E() {regs.C = fetchByte(); }
void ins1E() {regs.E = fetchByte(); }
void ins2E() {regs.L = fetchByte(); }
void ins3E() {regs.A = fetchByte(); }
void ins07() { regs.A = rotate(regs.A, "RLCA"); }
void ins0F() { regs.A = rotate(regs.A, "RRCA"); }
void ins17() { regs.A = rotate(regs.A, "RL A"); }
void ins1F() { regs.A = rotate(regs.A, "RR A"); }
void ins27() {} // TODO
void ins37() { setFlags('-', 0, 0, 1); }
void ins2F() { regs.A = ~regs.A; setFlags('-', 1, 1, '-'); }
void ins3F() { setFlags('-', 0, 0, FLAG_C ? 0 : 1); }
void ins08() {writeWord(fetchWord(), SP); }
void ins09() { addHL(regs.BC); }
void ins19() { addHL(regs.DE); }
void ins29() { addHL(regs.HL); }
void ins39() { addHL(SP); }
void ins0A() { regs.A = readByte(regs.BC); }
void ins1A() { regs.A = readByte(regs.DE); }
void ins2A() { regs.A = readByte(regs.HL++); }
void ins3A() { regs.A = readByte(regs.HL--); }
void ins40() { regs.B = regs.B; }
void ins41() { regs.B = regs.C; }
void ins42() { regs.B = regs.D; }
void ins43() { regs.B = regs.E; }
void ins44() { regs.B = regs.H; }
void ins45() { regs.B = regs.L; }
void ins46() { regs.B = readByte(regs.HL); }
void ins47() { regs.B = regs.A; }
void ins50() { regs.D = regs.B; }
void ins51() { regs.D = regs.C; }
void ins52() { regs.D = regs.D; }
void ins53() { regs.D = regs.E; }
void ins54() { regs.D = regs.H; }
void ins55() { regs.D = regs.L; }
void ins56() { regs.D = readByte(regs.HL); }
void ins57() { regs.D = regs.A; }
void ins60() { regs.H = regs.B; }
void ins61() { regs.H = regs.C; }
void ins62() { regs.H = regs.D; }
void ins63() { regs.H = regs.E; }
void ins64() { regs.H = regs.H; }
void ins65() { regs.H = regs.L; }
void ins66() { regs.H = readByte(regs.HL); }
void ins67() { regs.H = regs.A; }
void ins70() { writeByte(regs.HL, regs.B); }
void ins71() { writeByte(regs.HL, regs.C); }
void ins72() { writeByte(regs.HL, regs.D); }
void ins73() { writeByte(regs.HL, regs.E); }
void ins74() { writeByte(regs.HL, regs.H); }
void ins75() { writeByte(regs.HL, regs.L); }
void ins76() { isHalted = true; }
void ins77() { writeByte(regs.HL, regs.A); }
void ins48() { regs.C = regs.B; }
void ins49() { regs.C = regs.C; }
void ins4A() { regs.C = regs.D; }
void ins4B() { regs.C = regs.E; }
void ins4C() { regs.C = regs.H; }
void ins4D() { regs.C = regs.L; }
void ins4E() { regs.C = readByte(regs.HL); }
void ins4F() { regs.C = regs.A; }
void ins58() { regs.E = regs.B; }
void ins59() { regs.E = regs.C; }
void ins5A() { regs.E = regs.D; }
void ins5B() { regs.E = regs.E; }
void ins5C() { regs.E = regs.H; }
void ins5D() { regs.E = regs.L; }
void ins5E() { regs.E = readByte(regs.HL); }
void ins5F() { regs.E = regs.A; }
void ins68() { regs.L = regs.B; }
void ins69() { regs.L = regs.C; }
void ins6A() { regs.L = regs.D; }
void ins6B() { regs.L = regs.E; }
void ins6C() { regs.L = regs.H; }
void ins6D() { regs.L = regs.L; }
void ins6E() { regs.L = readByte(regs.HL); }
void ins6F() { regs.L = regs.A; }
void ins78() { regs.A = regs.B; }
void ins79() { regs.A = regs.C; }
void ins7A() { regs.A = regs.D; }
void ins7B() { regs.A = regs.E; }
void ins7C() { regs.A = regs.H; }
void ins7D() { regs.A = regs.L; }
void ins7E() { regs.A = readByte(regs.HL); }
void ins7F() { regs.A = regs.A; }
void ins80() { add(regs.B); }
void ins81() { add(regs.C); }
void ins82() { add(regs.D); }
void ins83() { add(regs.E); }
void ins84() { add(regs.H); }
void ins85() { add(regs.L); }
void ins86() { add(readByte(regs.HL)); }
void ins87() { add(regs.A); }
void ins90() { sub(regs.B); }
void ins91() { sub(regs.C); }
void ins92() { sub(regs.D); }
void ins93() { sub(regs.E); }
void ins94() { sub(regs.H); }
void ins95() { sub(regs.L); }
void ins96() { sub(readByte(regs.HL)); }
void ins97() { sub(regs.A); }
void insA0() { and(regs.B); }
void insA1() { and(regs.C); }
void insA2() { and(regs.D); }
void insA3() { and(regs.E); }
void insA4() { and(regs.H); }
void insA5() { and(regs.L); }
void insA6() { and(readByte(regs.HL)); }
void insA7() { and(regs.A); }
void insB0() { or(regs.B); }
void insB1() { or(regs.C); }
void insB2() { or(regs.D); }
void insB3() { or(regs.E); }
void insB4() { or(regs.H); }
void insB5() { or(regs.L); }
void insB6() { or(readByte(regs.HL)); }
void insB7() { or(regs.A); }
void ins88() { addc(regs.B); }
void ins89() { addc(regs.C); }
void ins8A() { addc(regs.D); }
void ins8B() { addc(regs.E); }
void ins8C() { addc(regs.H); }
void ins8D() { addc(regs.L); }
void ins8E() { addc(readByte(regs.HL)); }
void ins8F() { addc(regs.A); }
void ins98() { subc(regs.B); }
void ins99() { subc(regs.C); }
void ins9A() { subc(regs.D); }
void ins9B() { subc(regs.E); }
void ins9C() { subc(regs.H); }
void ins9D() { subc(regs.L); }
void ins9E() { subc(readByte(regs.HL)); }
void ins9F() { subc(regs.A); }
void insA8() { xor(regs.B); }
void insA9() { xor(regs.C); }
void insAA() { xor(regs.D); }
void insAB() { xor(regs.E); }
void insAC() { xor(regs.H); }
void insAD() { xor(regs.L); }
void insAE() { xor(readByte(regs.HL)); }
void insAF() { xor(regs.A); }
void insB8() { compare(regs.B); }
void insB9() { compare(regs.C); }
void insBA() { compare(regs.D); }
void insBB() { compare(regs.E); }
void insBC() { compare(regs.H); }
void insBD() { compare(regs.L); }
void insBE() { compare(readByte(regs.HL)); }
void insBF() { compare(regs.A); }
void insC6() { add(fetchByte()); }
void insD6() { sub(fetchByte()); }
void insE6() { and(fetchByte()); }
void insF6() { or(fetchByte()); }
void insCE() { addc(fetchByte()); }
void insDE() { subc(fetchByte()); }
void insEE() { xor(fetchByte()); }
void insFE() { compare(fetchByte()); }
void insC0() { if (!FLAG_Z) PC=pop(); }
void insC8() { if (FLAG_Z) PC=pop(); }
void insD0() { if (!FLAG_C) PC=pop(); }
void insD8() { if (FLAG_C) PC=pop(); }
void insC9() { PC=pop(); }
void insD9() { PC=pop(); IME=1; }
void insC5() { push(regs.BC); }
void insD5() { push(regs.DE); }
void insE5() { push(regs.HL); }
void insF5() { push(regs.AF); }
void insC1() { regs.BC=pop(); }
void insD1() { regs.DE=pop(); }
void insE1() { regs.HL=pop(); }
void insF1() { regs.AF=pop(); regs.F &= 0xf0; }
void ins18() { int8_t i=fetchByte(); PC+=i; }
void ins20() { int8_t i=fetchByte(); if (!FLAG_Z) {PC+=i; cycles+=4;} }
void ins28() { int8_t i=fetchByte(); if (FLAG_Z) {PC+=i; cycles+=4;} }
void ins30() { int8_t i=fetchByte(); if (!FLAG_C) {PC+=i; cycles+=4;} }
void ins38() { int8_t i=fetchByte(); if (FLAG_C) {PC+=i; cycles+=4;} }
void insC3() { int addr=fetchWord(); PC=addr; }
void insC2() { int addr=fetchWord(); if (!FLAG_Z) {PC=addr; cycles+=4;} }
void insCA() { int addr=fetchWord(); if (FLAG_Z) {PC=addr; cycles+=4;} }
void insD2() { int addr=fetchWord(); if (!FLAG_C) {PC=addr; cycles+=4;} }
void insDA() { int addr=fetchWord(); if (FLAG_C) {PC=addr; cycles+=4;} }
void insCD() { int addr=fetchWord(); push(PC); PC=addr; }
void insC4() { int addr=fetchWord(); if (!FLAG_Z) {push(PC); PC=addr; cycles+=12;} }
void insCC() { int addr=fetchWord(); if (FLAG_Z) {push(PC); PC=addr; cycles+=12;} }
void insD4() { int addr=fetchWord(); if (!FLAG_C) {push(PC); PC=addr; cycles+=12;} }
void insDC() { int addr=fetchWord(); if (FLAG_C) {push(PC); PC=addr; cycles+=12;} }
void insE0() { writeByte(0xff00+fetchByte(), regs.A); }
void insF0() { regs.A = readByte(0xff00+fetchByte()); }
void insE2() { writeByte(0xff00+regs.C, regs.A); }
void insF2() { regs.A = readByte(0xff00+regs.C); }
void insF3() { IME = 0; }
void insFB() { IME = 1; }
void insC7() { push(PC); PC=0x00; }
void insD7() { push(PC); PC=0x10; }
void insE7() { push(PC); PC=0x20; }
void insF7() { push(PC); PC=0x30; }
void insCF() { push(PC); PC=0x08; }
void insDF() { push(PC); PC=0x18; }
void insEF() { push(PC); PC=0x28; }
void insFF() { push(PC); PC=0x38; }
void insE8() { uint8_t i=fetchByte(); setFlags(0, 0, ((SP&0xf)+(i&0xf))>0xf, ((SP&0xff)+i)>0xff); SP+=(int8_t)i; }
void insF8() { uint8_t i=fetchByte(); setFlags(0, 0, ((SP&0xf)+(i&0xf))>0xf, ((SP&0xff)+i)>0xff); regs.HL=SP+(int8_t)i; }
void insF9() { SP = regs.HL; }
void insEA() { writeByte(fetchWord(), regs.A); }
void insFA() { regs.A = readByte(fetchWord()); }
void insE9() { PC = regs.HL; }
void ins10() { isHalted = true; }
void insCB();
void cb00() { regs.B = rotate(regs.B, "RLC "); }
void cb01() { regs.C = rotate(regs.C, "RLC "); }
void cb02() { regs.D = rotate(regs.D, "RLC "); }
void cb03() { regs.E = rotate(regs.E, "RLC "); }
void cb04() { regs.H = rotate(regs.H, "RLC "); }
void cb05() { regs.L = rotate(regs.L, "RLC "); }
void cb06() { writeByte(regs.HL, rotate(readByte(regs.HL), "RLC ")); }
void cb07() { regs.A = rotate(regs.A, "RLC "); }
void cb08() { regs.B = rotate(regs.B, "RRC "); }
void cb09() { regs.C = rotate(regs.C, "RRC "); }
void cb0A() { regs.D = rotate(regs.D, "RRC "); }
void cb0B() { regs.E = rotate(regs.E, "RRC "); }
void cb0C() { regs.H = rotate(regs.H, "RRC "); }
void cb0D() { regs.L = rotate(regs.L, "RRC "); }
void cb0E() { writeByte(regs.HL, rotate(readByte(regs.HL), "RRC ")); }
void cb0F() { regs.A = rotate(regs.A, "RRC "); }
void cb10() { regs.B = rotate(regs.B, "RL  "); }
void cb11() { regs.C = rotate(regs.C, "RL  "); }
void cb12() { regs.D = rotate(regs.D, "RL  "); }
void cb13() { regs.E = rotate(regs.E, "RL  "); }
void cb14() { regs.H = rotate(regs.H, "RL  "); }
void cb15() { regs.L = rotate(regs.L, "RL  "); }
void cb16() { writeByte(regs.HL, rotate(readByte(regs.HL), "RL  ")); }
void cb17() { regs.A = rotate(regs.A, "RL  "); }
void cb18() { regs.B = rotate(regs.B, "RR  "); }
void cb19() { regs.C = rotate(regs.C, "RR  "); }
void cb1A() { regs.D = rotate(regs.D, "RR  "); }
void cb1B() { regs.E = rotate(regs.E, "RR  "); }
void cb1C() { regs.H = rotate(regs.H, "RR  "); }
void cb1D() { regs.L = rotate(regs.L, "RR  "); }
void cb1E() { writeByte(regs.HL, rotate(readByte(regs.HL), "RR  ")); }
void cb1F() { regs.A = rotate(regs.A, "RR  "); }
void cb20() { regs.B = SLA(regs.B); }
void cb21() { regs.C = SLA(regs.C); }
void cb22() { regs.D = SLA(regs.D); }
void cb23() { regs.E = SLA(regs.E); }
void cb24() { regs.H = SLA(regs.H); }
void cb25() { regs.L = SLA(regs.L); }
void cb26() { writeByte(regs.HL, SLA(readByte(regs.HL))); }
void cb27() { regs.A = SLA(regs.A); }
void cb28() { regs.B = SRA(regs.B); }
void cb29() { regs.C = SRA(regs.C); }
void cb2A() { regs.D = SRA(regs.D); }
void cb2B() { regs.E = SRA(regs.E); }
void cb2C() { regs.H = SRA(regs.H); }
void cb2D() { regs.L = SRA(regs.L); }
void cb2E() { writeByte(regs.HL, SRA(readByte(regs.HL))); }
void cb2F() { regs.A = SRA(regs.A); }
void cb30() { regs.B = swap(regs.B); }
void cb31() { regs.C = swap(regs.C); }
void cb32() { regs.D = swap(regs.D); }
void cb33() { regs.E = swap(regs.E); }
void cb34() { regs.H = swap(regs.H); }
void cb35() { regs.L = swap(regs.L); }
void cb36() { writeByte(regs.HL, swap(readByte(regs.HL))); }
void cb37() { regs.A = swap(regs.A); }
void cb38() { regs.B = SRL(regs.B); }
void cb39() { regs.C = SRL(regs.C); }
void cb3A() { regs.D = SRL(regs.D); }
void cb3B() { regs.E = SRL(regs.E); }
void cb3C() { regs.H = SRL(regs.H); }
void cb3D() { regs.L = SRL(regs.L); }
void cb3E() { writeByte(regs.HL, SRL(readByte(regs.HL))); }
void cb3F() { regs.A = SRL(regs.A); }
void cb40() { testBit(regs.B, 0); }
void cb41() { testBit(regs.C, 0); }
void cb42() { testBit(regs.D, 0); }
void cb43() { testBit(regs.E, 0); }
void cb44() { testBit(regs.H, 0); }
void cb45() { testBit(regs.L, 0); }
void cb46() { testBit(readByte(regs.HL), 0); }
void cb47() { testBit(regs.A, 0); }
void cb48() { testBit(regs.B, 1); }
void cb49() { testBit(regs.C, 1); }
void cb4A() { testBit(regs.D, 1); }
void cb4B() { testBit(regs.E, 1); }
void cb4C() { testBit(regs.H, 1); }
void cb4D() { testBit(regs.L, 1); }
void cb4E() { testBit(readByte(regs.HL), 1); }
void cb4F() { testBit(regs.A, 1); }
void cb50() { testBit(regs.B, 2); }
void cb51() { testBit(regs.C, 2); }
void cb52() { testBit(regs.D, 2); }
void cb53() { testBit(regs.E, 2); }
void cb54() { testBit(regs.H, 2); }
void cb55() { testBit(regs.L, 2); }
void cb56() { testBit(readByte(regs.HL), 2); }
void cb57() { testBit(regs.A, 2); }
void cb58() { testBit(regs.B, 3); }
void cb59() { testBit(regs.C, 3); }
void cb5A() { testBit(regs.D, 3); }
void cb5B() { testBit(regs.E, 3); }
void cb5C() { testBit(regs.H, 3); }
void cb5D() { testBit(regs.L, 3); }
void cb5E() { testBit(readByte(regs.HL), 3); }
void cb5F() { testBit(regs.A, 3); }
void cb60() { testBit(regs.B, 4); }
void cb61() { testBit(regs.C, 4); }
void cb62() { testBit(regs.D, 4); }
void cb63() { testBit(regs.E, 4); }
void cb64() { testBit(regs.H, 4); }
void cb65() { testBit(regs.L, 4); }
void cb66() { testBit(readByte(regs.HL), 4); }
void cb67() { testBit(regs.A, 4); }
void cb68() { testBit(regs.B, 5); }
void cb69() { testBit(regs.C, 5); }
void cb6A() { testBit(regs.D, 5); }
void cb6B() { testBit(regs.E, 5); }
void cb6C() { testBit(regs.H, 5); }
void cb6D() { testBit(regs.L, 5); }
void cb6E() { testBit(readByte(regs.HL), 5); }
void cb6F() { testBit(regs.A, 5); }
void cb70() { testBit(regs.B, 6); }
void cb71() { testBit(regs.C, 6); }
void cb72() { testBit(regs.D, 6); }
void cb73() { testBit(regs.E, 6); }
void cb74() { testBit(regs.H, 6); }
void cb75() { testBit(regs.L, 6); }
void cb76() { testBit(readByte(regs.HL), 6); }
void cb77() { testBit(regs.A, 6); }
void cb78() { testBit(regs.B, 7); }
void cb79() { testBit(regs.C, 7); }
void cb7A() { testBit(regs.D, 7); }
void cb7B() { testBit(regs.E, 7); }
void cb7C() { testBit(regs.H, 7); }
void cb7D() { testBit(regs.L, 7); }
void cb7E() { testBit(readByte(regs.HL), 7); }
void cb7F() { testBit(regs.A, 7); }
void cb80() { regs.B &= ~(1<<0); }
void cb81() { regs.C &= ~(1<<0); }
void cb82() { regs.D &= ~(1<<0); }
void cb83() { regs.E &= ~(1<<0); }
void cb84() { regs.H &= ~(1<<0); }
void cb85() { regs.L &= ~(1<<0); }
void cb86() { writeByte(regs.HL, readByte(regs.HL) & ~(1<<0)); }
void cb87() { regs.A &= ~(1<<0); }
void cb88() { regs.B &= ~(1<<1); }
void cb89() { regs.C &= ~(1<<1); }
void cb8A() { regs.D &= ~(1<<1); }
void cb8B() { regs.E &= ~(1<<1); }
void cb8C() { regs.H &= ~(1<<1); }
void cb8D() { regs.L &= ~(1<<1); }
void cb8E() { writeByte(regs.HL, readByte(regs.HL) & ~(1<<1)); }
void cb8F() { regs.A &= ~(1<<1); }
void cb90() { regs.B &= ~(1<<2); }
void cb91() { regs.C &= ~(1<<2); }
void cb92() { regs.D &= ~(1<<2); }
void cb93() { regs.E &= ~(1<<2); }
void cb94() { regs.H &= ~(1<<2); }
void cb95() { regs.L &= ~(1<<2); }
void cb96() { writeByte(regs.HL, readByte(regs.HL) & ~(1<<2)); }
void cb97() { regs.A &= ~(1<<2); }
void cb98() { regs.B &= ~(1<<3); }
void cb99() { regs.C &= ~(1<<3); }
void cb9A() { regs.D &= ~(1<<3); }
void cb9B() { regs.E &= ~(1<<3); }
void cb9C() { regs.H &= ~(1<<3); }
void cb9D() { regs.L &= ~(1<<3); }
void cb9E() { writeByte(regs.HL, readByte(regs.HL) & ~(1<<3)); }
void cb9F() { regs.A &= ~(1<<3); }
void cbA0() { regs.B &= ~(1<<4); }
void cbA1() { regs.C &= ~(1<<4); }
void cbA2() { regs.D &= ~(1<<4); }
void cbA3() { regs.E &= ~(1<<4); }
void cbA4() { regs.H &= ~(1<<4); }
void cbA5() { regs.L &= ~(1<<4); }
void cbA6() { writeByte(regs.HL, readByte(regs.HL) & ~(1<<4)); }
void cbA7() { regs.A &= ~(1<<4); }
void cbA8() { regs.B &= ~(1<<5); }
void cbA9() { regs.C &= ~(1<<5); }
void cbAA() { regs.D &= ~(1<<5); }
void cbAB() { regs.E &= ~(1<<5); }
void cbAC() { regs.H &= ~(1<<5); }
void cbAD() { regs.L &= ~(1<<5); }
void cbAE() { writeByte(regs.HL, readByte(regs.HL) & ~(1<<5)); }
void cbAF() { regs.A &= ~(1<<5); }
void cbB0() { regs.B &= ~(1<<6); }
void cbB1() { regs.C &= ~(1<<6); }
void cbB2() { regs.D &= ~(1<<6); }
void cbB3() { regs.E &= ~(1<<6); }
void cbB4() { regs.H &= ~(1<<6); }
void cbB5() { regs.L &= ~(1<<6); }
void cbB6() { writeByte(regs.HL, readByte(regs.HL) & ~(1<<6)); }
void cbB7() { regs.A &= ~(1<<6); }
void cbB8() { regs.B &= ~(1<<7); }
void cbB9() { regs.C &= ~(1<<7); }
void cbBA() { regs.D &= ~(1<<7); }
void cbBB() { regs.E &= ~(1<<7); }
void cbBC() { regs.H &= ~(1<<7); }
void cbBD() { regs.L &= ~(1<<7); }
void cbBE() { writeByte(regs.HL, readByte(regs.HL) & ~(1<<7)); }
void cbBF() { regs.A &= ~(1<<7); }
void cbC0() { regs.B |= (1<<0); }
void cbC1() { regs.C |= (1<<0); }
void cbC2() { regs.D |= (1<<0); }
void cbC3() { regs.E |= (1<<0); }
void cbC4() { regs.H |= (1<<0); }
void cbC5() { regs.L |= (1<<0); }
void cbC6() { writeByte(regs.HL, readByte(regs.HL) | (1<<0)); }
void cbC7() { regs.A |= (1<<0); }
void cbC8() { regs.B |= (1<<1); }
void cbC9() { regs.C |= (1<<1); }
void cbCA() { regs.D |= (1<<1); }
void cbCB() { regs.E |= (1<<1); }
void cbCC() { regs.H |= (1<<1); }
void cbCD() { regs.L |= (1<<1); }
void cbCE() { writeByte(regs.HL, readByte(regs.HL) | (1<<1)); }
void cbCF() { regs.A |= (1<<1); }
void cbD0() { regs.B |= (1<<2); }
void cbD1() { regs.C |= (1<<2); }
void cbD2() { regs.D |= (1<<2); }
void cbD3() { regs.E |= (1<<2); }
void cbD4() { regs.H |= (1<<2); }
void cbD5() { regs.L |= (1<<2); }
void cbD6() { writeByte(regs.HL, readByte(regs.HL) | (1<<2)); }
void cbD7() { regs.A |= (1<<2); }
void cbD8() { regs.B |= (1<<3); }
void cbD9() { regs.C |= (1<<3); }
void cbDA() { regs.D |= (1<<3); }
void cbDB() { regs.E |= (1<<3); }
void cbDC() { regs.H |= (1<<3); }
void cbDD() { regs.L |= (1<<3); }
void cbDE() { writeByte(regs.HL, readByte(regs.HL) | (1<<3)); }
void cbDF() { regs.A |= (1<<3); }
void cbE0() { regs.B |= (1<<4); }
void cbE1() { regs.C |= (1<<4); }
void cbE2() { regs.D |= (1<<4); }
void cbE3() { regs.E |= (1<<4); }
void cbE4() { regs.H |= (1<<4); }
void cbE5() { regs.L |= (1<<4); }
void cbE6() { writeByte(regs.HL, readByte(regs.HL) | (1<<4)); }
void cbE7() { regs.A |= (1<<4); }
void cbE8() { regs.B |= (1<<5); }
void cbE9() { regs.C |= (1<<5); }
void cbEA() { regs.D |= (1<<5); }
void cbEB() { regs.E |= (1<<5); }
void cbEC() { regs.H |= (1<<5); }
void cbED() { regs.L |= (1<<5); }
void cbEE() { writeByte(regs.HL, readByte(regs.HL) | (1<<5)); }
void cbEF() { regs.A |= (1<<5); }
void cbF0() { regs.B |= (1<<6); }
void cbF1() { regs.C |= (1<<6); }
void cbF2() { regs.D |= (1<<6); }
void cbF3() { regs.E |= (1<<6); }
void cbF4() { regs.H |= (1<<6); }
void cbF5() { regs.L |= (1<<6); }
void cbF6() { writeByte(regs.HL, readByte(regs.HL) | (1<<6)); }
void cbF7() { regs.A |= (1<<6); }
void cbF8() { regs.B |= (1<<7); }
void cbF9() { regs.C |= (1<<7); }
void cbFA() { regs.D |= (1<<7); }
void cbFB() { regs.E |= (1<<7); }
void cbFC() { regs.H |= (1<<7); }
void cbFD() { regs.L |= (1<<7); }
void cbFE() { writeByte(regs.HL, readByte(regs.HL) | (1<<7)); }
void cbFF() { regs.A |= (1<<7); }

void (*instrs[512])(void) = {
	ins00, ins01, ins02, ins03, ins04, ins05, ins06, ins07, ins08, ins09, ins0A, ins0B, ins0C, ins0D, ins0E, ins0F,
	ins10, ins11, ins12, ins13, ins14, ins15, ins16, ins17, ins18, ins19, ins1A, ins1B, ins1C, ins1D, ins1E, ins1F,
	ins20, ins21, ins22, ins23, ins24, ins25, ins26, ins27, ins28, ins29, ins2A, ins2B, ins2C, ins2D, ins2E, ins2F,
	ins30, ins31, ins32, ins33, ins34, ins35, ins36, ins37, ins38, ins39, ins3A, ins3B, ins3C, ins3D, ins3E, ins3F,
	ins40, ins41, ins42, ins43, ins44, ins45, ins46, ins47, ins48, ins49, ins4A, ins4B, ins4C, ins4D, ins4E, ins4F,
	ins50, ins51, ins52, ins53, ins54, ins55, ins56, ins57, ins58, ins59, ins5A, ins5B, ins5C, ins5D, ins5E, ins5F,
	ins60, ins61, ins62, ins63, ins64, ins65, ins66, ins67, ins68, ins69, ins6A, ins6B, ins6C, ins6D, ins6E, ins6F,
	ins70, ins71, ins72, ins73, ins74, ins75, ins76, ins77, ins78, ins79, ins7A, ins7B, ins7C, ins7D, ins7E, ins7F,
	ins80, ins81, ins82, ins83, ins84, ins85, ins86, ins87, ins88, ins89, ins8A, ins8B, ins8C, ins8D, ins8E, ins8F,
	ins90, ins91, ins92, ins93, ins94, ins95, ins96, ins97, ins98, ins99, ins9A, ins9B, ins9C, ins9D, ins9E, ins9F,
	insA0, insA1, insA2, insA3, insA4, insA5, insA6, insA7, insA8, insA9, insAA, insAB, insAC, insAD, insAE, insAF,
	insB0, insB1, insB2, insB3, insB4, insB5, insB6, insB7, insB8, insB9, insBA, insBB, insBC, insBD, insBE, insBF,
	insC0, insC1, insC2, insC3, insC4, insC5, insC6, insC7, insC8, insC9, insCA, insCB, insCC, insCD, insCE, insCF,
	insD0, insD1, insD2, NULL , insD4, insD5, insD6, insD7, insD8, insD9, insDA, NULL , insDC, NULL , insDE, insDF,
	insE0, insE1, insE2, NULL , NULL , insE5, insE6, insE7, insE8, insE9, insEA, NULL , NULL , NULL , insEE, insEF,
	insF0, insF1, insF2, insF3, NULL , insF5, insF6, insF7, insF8, insF9, insFA, insFB, NULL , NULL , insFE, insFF,
	cb00, cb01, cb02, cb03, cb04, cb05, cb06, cb07, cb08, cb09, cb0A, cb0B, cb0C, cb0D, cb0E, cb0F,
	cb10, cb11, cb12, cb13, cb14, cb15, cb16, cb17, cb18, cb19, cb1A, cb1B, cb1C, cb1D, cb1E, cb1F,
	cb20, cb21, cb22, cb23, cb24, cb25, cb26, cb27, cb28, cb29, cb2A, cb2B, cb2C, cb2D, cb2E, cb2F,
	cb30, cb31, cb32, cb33, cb34, cb35, cb36, cb37, cb38, cb39, cb3A, cb3B, cb3C, cb3D, cb3E, cb3F,
	cb40, cb41, cb42, cb43, cb44, cb45, cb46, cb47, cb48, cb49, cb4A, cb4B, cb4C, cb4D, cb4E, cb4F,
	cb50, cb51, cb52, cb53, cb54, cb55, cb56, cb57, cb58, cb59, cb5A, cb5B, cb5C, cb5D, cb5E, cb5F,
	cb60, cb61, cb62, cb63, cb64, cb65, cb66, cb67, cb68, cb69, cb6A, cb6B, cb6C, cb6D, cb6E, cb6F,
	cb70, cb71, cb72, cb73, cb74, cb75, cb76, cb77, cb78, cb79, cb7A, cb7B, cb7C, cb7D, cb7E, cb7F,
	cb80, cb81, cb82, cb83, cb84, cb85, cb86, cb87, cb88, cb89, cb8A, cb8B, cb8C, cb8D, cb8E, cb8F,
	cb90, cb91, cb92, cb93, cb94, cb95, cb96, cb97, cb98, cb99, cb9A, cb9B, cb9C, cb9D, cb9E, cb9F,
	cbA0, cbA1, cbA2, cbA3, cbA4, cbA5, cbA6, cbA7, cbA8, cbA9, cbAA, cbAB, cbAC, cbAD, cbAE, cbAF,
	cbB0, cbB1, cbB2, cbB3, cbB4, cbB5, cbB6, cbB7, cbB8, cbB9, cbBA, cbBB, cbBC, cbBD, cbBE, cbBF,
	cbC0, cbC1, cbC2, cbC3, cbC4, cbC5, cbC6, cbC7, cbC8, cbC9, cbCA, cbCB, cbCC, cbCD, cbCE, cbCF,
	cbD0, cbD1, cbD2, cbD3, cbD4, cbD5, cbD6, cbD7, cbD8, cbD9, cbDA, cbDB, cbDC, cbDD, cbDE, cbDF,
	cbE0, cbE1, cbE2, cbE3, cbE4, cbE5, cbE6, cbE7, cbE8, cbE9, cbEA, cbEB, cbEC, cbED, cbEE, cbEF,
	cbF0, cbF1, cbF2, cbF3, cbF4, cbF5, cbF6, cbF7, cbF8, cbF9, cbFA, cbFB, cbFC, cbFD, cbFE, cbFF,
};

void insCB() {
	uint8_t op = fetchByte();
	cycles += ((op&7)==6) ? 16 : 8;
	instrs[op+256]();
}