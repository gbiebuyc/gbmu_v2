#include "emu.h"
#include <math.h>

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

int num_audio_channels;
int snd_clocks;
uint8_t frame_sequencer;
bool enable1, enable2;
bool len_enable1, len_enable2;
uint8_t len_counter1, len_counter2;
int freq1, freq2;
uint8_t volume1, volume2;
uint8_t envelope_timer1, envelope_timer2, sweep_timer;
bool sweep_enable;

void lengthClock();
void envelopeClock();
void sweepClock();

void snd_update() {
	if ((snd_clocks += clocksIncrement) >= 8192) {
		snd_clocks -= 8192;
		if (++frame_sequencer >= 8)
			frame_sequencer = 0;
		switch (frame_sequencer) {
			case 0:
				lengthClock();
				break;
			case 2:
				lengthClock();
				sweepClock();
				break;
			case 4:
				lengthClock();
				break;
			case 6:
				lengthClock();
				sweepClock();
				break;
			case 7:
				envelopeClock();
				break;
		}
	}
}

void lengthClock() {
	if (enable1 && len_enable1 && len_counter1 > 0) {
		if (--len_counter1 == 0)
			enable1 = false;
		// printf("len: %02X\n", len_counter1);
	}
	if (enable2 && len_enable2 && len_counter2 > 0) {
		if (--len_counter2 == 0)
			enable2 = false;
		// printf("len: %02X\n", len_counter2);
	}
}

void envelopeClock() {
	if (envelope_timer1) {
		envelope_timer1--;
	} else if (mem[NR12] & 7) {
		envelope_timer1 = mem[NR12] & 7;
		uint8_t new_volume = volume1 + ((mem[NR12] & 8) ? 1 : -1);
		if (new_volume < 16)
			volume1 = new_volume;
	}
	if (envelope_timer2) {
		envelope_timer2--;
	} else if (mem[NR22] & 7) {
		envelope_timer2 = mem[NR22] & 7;
		uint8_t new_volume = volume2 + ((mem[NR22] & 8) ? 1 : -1);
		if (new_volume < 16)
			volume2 = new_volume;
	}
}

void sweepClock() {
	/* TODO: Doesn't work
	if (!sweep_enable)
		return;
	if (sweep_timer) {
		sweep_timer--;
	} else if ((mem[NR10] >> 4) & 7) {
		sweep_timer = (mem[NR10] >> 4) & 7;
		uint8_t new_freq = freq1 >> (mem[NR10] & 7);
		if (mem[NR10] & 8)
			new_freq = freq1 - new_freq;
		else
			new_freq = freq1 + new_freq;
		freq1 = new_freq;
		mem[NR13] = (uint8_t)freq1;
		mem[NR14] &= ~7;
		mem[NR14] |= ((freq1 >> 8) & 7);
	}
	*/
}

// Runs a number of cpu cycles that corresponds to the duration of the audio buffer.
void gbmu_fill_audio_buffer(uint8_t *buf, int len, void (*frame_is_ready)()) {
	if (!isBootROMUnmapped &&
		(skipBootAnimation || gbmu_keys[GBMU_START]))
		skip_boot_animation();
	int num_samples = len/sizeof(float);
	// printf("num samples: %d\n", num_samples);
	static uint64_t i_sample = 0;
	static double sampleClocks = 0;
	double clocksPerSample = (4194304.0 / 44100.0) * emulation_speed;

	for (int i_buf=0; i_buf<num_samples; i_buf++) {
		while (sampleClocks < clocksPerSample) {
			gbmu_run_one_instr();
			sampleClocks += clocksIncrement;
			if (isFrameReady)
				frame_is_ready();
		}
		sampleClocks -= clocksPerSample;

		float freq1_actual = 131072.0f / (2048-freq1);
		float freq2_actual = 131072.0f / (2048-freq2);
		float samples_per_sine1 = 44100.0f / freq1_actual;
		float samples_per_sine2 = 44100.0f / freq2_actual;
		float sinewave1 = sinf(i_sample / samples_per_sine1 * M_PI * 2);
		float sinewave2 = sinf(i_sample / samples_per_sine2 * M_PI * 2);
		float squarewave1 = (volume1/16.0f) * (sinewave1 > 0 ? 1 : -1);
		float squarewave2 = (volume2/16.0f) * (sinewave2 > 0 ? 1 : -1);

		((float*)buf)[i_buf] = 0;
		if (enable1)
			((float*)buf)[i_buf] += squarewave1;
		if (enable2)
			((float*)buf)[i_buf] += squarewave2;
		((float*)buf)[i_buf] *= 0.1; //Volume
		if (num_audio_channels == 2) {
			((float*)buf)[i_buf+1] = ((float*)buf)[i_buf];
			i_buf++;
		}
		i_sample++;
	}
}

uint8_t snd_readRegister(uint16_t addr) {
	switch (addr) {
		case NR52: {
			uint8_t val = mem[NR52] & 0x80;
			if (enable1)
				val |= 0b0001;
			if (enable2)
				val |= 0b0010;
			return val;
		}
	}
}

void snd_writeRegister(uint16_t addr, uint8_t val) {
	switch (addr) {

		case NR11:
			len_counter1 = 64 - (val & 0b00111111);
			break;
		case NR14:
			enable1 = (val & 0x80);
			len_enable1 = (val & 0x40);
			freq1 = ((val&7) << 8) | mem[NR13];
			if (enable1 && !len_counter1)
				len_counter1 = 64;
			envelope_timer1 = mem[NR12] & 7;
			volume1 = mem[NR12] >> 4;
			sweep_timer = (mem[NR10] >> 4) & 7;
			sweep_enable = (sweep_timer || (mem[NR10] & 7));
			break;

		case NR21:
			len_counter2 = 64 - (val & 0b00111111);
			break;
		case NR24:
			enable2 = (val & 0x80);
			len_enable2 = (val & 0x40);
			freq2 = ((val&7) << 8) | mem[NR23];
			if (enable2 && !len_counter2)
				len_counter2 = 64;
			envelope_timer2 = mem[NR22] & 7;
			volume2 = mem[NR22] >> 4;
			break;

		default:
			mem[addr] = val;
			break;
	}
}
