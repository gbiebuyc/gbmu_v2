#include "emu.h"

void gbmu_quit() {
	gbmu_save_ext_ram();
	free(screen_pixels);
	free(screen_debug_tiles_pixels);
	free(mem);
	free(gbc_wram);
	free(external_ram);
	free(vram);
	free(gamerom);
	free(savefilename);
}