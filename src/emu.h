#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>

#define FLAG_Z (regs.F>>7&1)
#define FLAG_N (regs.F>>6&1)
#define FLAG_H (regs.F>>5&1)
#define FLAG_C (regs.F>>4&1)
#define IE mem[0xffff]
#define IF mem[0xff0f]
#define LCDSTAT mem[0xff41]
#define SCREEN_DEBUG_TILES_W (32 * (8 + 1))
#define SCREEN_DEBUG_TILES_H (24 * (8 + 1))
#define MAX_ROM_SIZE (8*1024*1024)

#ifndef O_BINARY
# define O_BINARY 0
#endif

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

typedef enum {
	MODE_DMG = 0,
	MODE_GBC = 0xC0,
	MODE_DMG_OR_GBC = 0x80,
} t_mode;

typedef union {
	struct { uint16_t AF, BC, DE, HL; };
	struct { uint8_t F, A, C, B, E, D, L, H; };
} t_regs;

typedef enum {
	NORMAL,
	HALT,
	HALT_BUG,
	STOP,
} t_cpuState;

extern bool gbmu_keys[GBMU_NUMBER_OF_KEYS];
extern uint32_t *screen_pixels, *screen_debug_tiles_pixels;
extern uint8_t *wram;
extern uint8_t *vram;
extern uint8_t *external_ram;
extern uint32_t dmg_screen_palette[4];
extern uint8_t gbc_backgr_palettes[8*4*2]; // 8 palettes of 4 colors of 2 bytes.
extern uint8_t gbc_sprite_palettes[8*4*2];
extern uint8_t *mem;
extern uint8_t *gamerom;
extern uint8_t bootrom_dmg[];
extern uint8_t bootrom_gbc[];
extern uint8_t cycleTable[512]; // Duration of each cpu instruction.
extern uint16_t PC, SP;
extern t_regs regs;
extern int scanlineClocks, frameClocks, divTimerClocks, counterTimerClocks, clocksIncrement;
extern bool IME;
extern bool isBootROMUnmapped;
extern int ROMBankNumber, externalRAMBankNumber;
extern int numROMBanks, extRAMSize;
extern void (*instrs[512])(void);
extern t_mode hardwareMode, gameMode;
extern bool doubleSpeed;
extern uint8_t mbc1_banking_mode, mbc1_bank1_reg, mbc1_bank2_reg;
extern bool mbc_ram_enable;
extern char *savefilename, *cartridgeTitle, *cartridgeTypeStr;
extern uint16_t hdma_src, hdma_dst;
extern size_t hdma_remaining_size;
extern bool cartridgeHasBattery;
extern bool isFrameReady;
extern t_cpuState cpuState;
extern bool isROMLoaded;
extern bool skipBootAnimation;
extern double emulation_speed;
extern int num_audio_channels;

void	gbmu_init();
void	gbmu_reset();
bool	gbmu_load_rom(char *filepath);
void	gbmu_fill_audio_buffer(uint8_t *buf, int len, void (*frame_is_ready)());
void	gbmu_run_one_instr();
void	gbmu_run_one_frame();
char	*gbmu_debug_info();
void	gbmu_update_debug_tiles_screen();
void	gbmu_save_ext_ram();
void	gbmu_load_ext_ram();
void	gbmu_quit();

// Internal
uint8_t		readJoypadRegister();
void		lcd_update();
void		lcd_clear();
void		lcd_draw_scanline();
void		oam_search(int array[40]);
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
char		*get_cartridge_title();
char		*get_cartridge_type();
int			get_cartridge_ram_size();
bool		get_cartridge_has_battery();
void		set_mbc_type();
void		hdma_transfer_continue();
char		*disassemble_instr(uint16_t addr);
bool		isDMG(uint8_t val);
void		snd_update();
uint8_t		snd_readRegister(uint16_t addr);
void		snd_writeRegister(uint16_t addr, uint8_t val);
void		debug_trace();
void		skip_boot_animation();
