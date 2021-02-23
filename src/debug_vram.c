#include "emu.h"

void gbmu_update_debug_tiles_screen() {
	for (int i=0; i<SCREEN_DEBUG_TILES_W*SCREEN_DEBUG_TILES_H; i++)
		screen_debug_tiles_pixels[i] = 0xffdddddd;
	for (int bank=0; bank<2; bank++) {
		int tileIndex = 0;
		for (int y=0; y<24; y++) {
			for (int x=0; x<16; x++) {
				uint8_t *tile = vram + tileIndex*16 + 0x2000*bank;
				for (int v=0; v<8; v++) {
					for (int u=0; u<8; u++) {
						uint16_t pixels = ((uint16_t*)tile)[v];
						uint32_t px = pixels >> (7-u);
						px = (px>>7&2) | (px&1);
						px = dmg_screen_palette[px];
						int screenX = x*9 + u + bank*(SCREEN_DEBUG_TILES_W/2);
						int screenY = y*9 + v;
						screen_debug_tiles_pixels[screenY*SCREEN_DEBUG_TILES_W + screenX] = px;
					}
				}
				tileIndex++;
			}
		}
	}
}