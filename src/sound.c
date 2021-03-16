#include "emu.h"

//      Name Addr      7654 3210 Function
//      - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//              Square 1
#define NR10 0xFF10 // -PPP NSSS Sweep period, negate, shift
#define NR11 0xFF11 // DDLL LLLL Duty, Length load (64-L)
#define NR12 0xFF12 // VVVV APPP Starting volume, Envelope add mode, period
#define NR13 0xFF13 // FFFF FFFF Frequency LSB
#define NR14 0xFF14 // TL-- -FFF Trigger, Length enable, Frequency MSB

//              Square 2
//           0xFF15 // ---- ---- Not used
#define NR21 0xFF16 // DDLL LLLL Duty, Length load (64-L)
#define NR22 0xFF17 // VVVV APPP Starting volume, Envelope add mode, period
#define NR23 0xFF18 // FFFF FFFF Frequency LSB
#define NR24 0xFF19 // TL-- -FFF Trigger, Length enable, Frequency MSB

//              Wave
#define NR30 0xFF1A // E--- ---- DAC power
#define NR31 0xFF1B // LLLL LLLL Length load (256-L)
#define NR32 0xFF1C // -VV- ---- Volume code (00=0%, 01=100%, 10=50%, 11=25%)
#define NR33 0xFF1D // FFFF FFFF Frequency LSB
#define NR34 0xFF1E // TL-- -FFF Trigger, Length enable, Frequency MSB

//              Noise
//           0xFF1F // ---- ---- Not used
#define NR41 0xFF20 // --LL LLLL Length load (64-L)
#define NR42 0xFF21 // VVVV APPP Starting volume, Envelope add mode, period
#define NR43 0xFF22 // SSSS WDDD Clock shift, Width mode of LFSR, Divisor code
#define NR44 0xFF23 // TL-- ---- Trigger, Length enable

//              Control/Status
#define NR50 0xFF24 // ALLL BRRR Vin L enable, Left vol, Vin R enable, Right vol
#define NR51 0xFF25 // NW21 NW21 Left enables, Right enables
#define NR52 0xFF26 // P--- NW21 Power control/status, Channel length statuses

//              Wave Table
//           0xFF30 // 0000 1111 Samples 0 and 1
//           ....
//           0xFF3F // 0000 1111 Samples 30 and 31

int    snd_clocks;
bool   snd1_len_enable;
int8_t snd1_len_counter;
bool   snd2_len_enable;
int8_t snd2_len_counter;

void snd_update() {
	int threshold = doubleSpeed ? 16384*2 : 16384;
	if ((snd_clocks += clocksIncrement) >= threshold) {
		snd_clocks -= threshold;
		if (snd1_len_enable && snd1_len_counter > 0) {
			if (--snd1_len_counter == 0)
				snd1_len_enable = false;
			// printf("len: %02X\n", snd1_len_counter);
		}
		if (snd2_len_enable && snd2_len_counter > 0) {
			if (--snd2_len_counter == 0)
				snd2_len_enable = false;
			// printf("len: %02X\n", snd2_len_counter);
		}
	}
}

uint8_t snd_readRegister(uint16_t addr) {
	switch (addr) {
		case NR52: {
			uint8_t val = mem[NR52] & 0x80;
			if (snd1_len_counter > 0)
				val |= 0b0001;
			if (snd2_len_counter > 0)
				val |= 0b0010;
			return val;
		}
	}
}

void snd_writeRegister(uint16_t addr, uint8_t val) {
	switch (addr) {
		case NR11:
			snd1_len_counter = 64 - (val & 0b00111111);
			break;

		case NR14:
			snd1_len_enable = (val & 0x40);
			break;

		case NR21:
			snd2_len_counter = 64 - (val & 0b00111111);
			break;

		case NR24:
			snd2_len_enable = (val & 0x40);
			break;
	}
}
