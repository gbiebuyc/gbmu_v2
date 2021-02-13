#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define SHOW_BOOT_ANIMATION false
#define FLAG_Z (regs.F>>7&1)
#define FLAG_N (regs.F>>6&1)
#define FLAG_H (regs.F>>5&1)
#define FLAG_C (regs.F>>4&1)

enum {
	GBMU_SELECT,
	GBMU_START,
	GBMU_A,
	GBMU_B,
	GBMU_LEFT,
	GBMU_RIGHT,
	GBMU_UP,
	GBMU_DOWN,
	GBMU_NUMBER_OF_KEYS,
};

extern bool gbmu_keys[GBMU_NUMBER_OF_KEYS];
extern uint32_t *screen_pixels;
extern uint32_t palette[];
extern uint8_t *mem;
extern uint8_t *gamerom;
extern size_t   gamerom_size;
extern uint8_t bootrom[];
extern uint8_t cycleTable[256]; // Duration in cycles of each cpu instruction.
extern uint16_t PC, SP;
typedef union {
	struct { uint16_t AF, BC, DE, HL; };
	struct { uint8_t F, A, C, B, E, D, L, H; };
} t_regs;
extern t_regs regs;
extern int scanlineCycles;
extern int divTimerCycles;
extern int counterTimerCycles;
extern bool IME;
extern int cycles;
extern bool isBootROMUnmapped;
extern bool isHalted;
extern bool debug;
extern int selectedROMBank;
extern void (*instrs[512])(void);

void	gbmu_reset();
bool	gbmu_load_rom(char *filepath);
bool	gbmu_run_one_instr();
void	gbmu_run_one_frame();

// Internal
uint8_t		readByte(uint16_t addr);
uint16_t	readWord(uint16_t addr);
void		writeByte(uint16_t addr, uint8_t val);
void		writeWord(uint16_t addr, uint16_t val);
uint8_t		fetchByte();
uint16_t	fetchWord();
void		requestInterrupt(uint8_t interrupt);
void		push(uint16_t operand);
uint16_t	pop();
int			min(int a, int b);
int			max(int a, int b);