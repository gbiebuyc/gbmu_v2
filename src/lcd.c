#include "emu.h"

#define LCDC (mem[0xff40])
#define LY   (mem[0xff44])
#define LYC  (mem[0xff45])

uint32_t dmg_screen_palette[] = {0xffffffff, 0xffaaaaaa, 0xff555555, 0xff000000};
uint8_t gbc_backgr_palettes[8*4*2]; // 8 palettes of 4 colors of 2 bytes.
uint8_t gbc_sprite_palettes[8*4*2];
bool isFrameReady;

void lcd_update() {
	isFrameReady = false;

	if (!(mem[0xff40] & 0x80)) // LCD is disabled
		return;

	bool coincidenceFlag = false;
	int old = scanlineClocks;
	if ((scanlineClocks += clocksIncrement) >= 456) {
		scanlineClocks -= 456;
		if (++(LY) > 153)
			LY = 0;
		coincidenceFlag = LY==LYC;
		LCDSTAT = coincidenceFlag ? (LCDSTAT|4) : (LCDSTAT&~4);
	}

	// Set LCD mode flag
	int lcd_mode;
	if (LY >= 144)
		lcd_mode = 1;
	else if (scanlineClocks >= 252)
		lcd_mode = 0;
	else if (scanlineClocks >= 80)
		lcd_mode = 3;
	else
		lcd_mode = 2;
	LCDSTAT &= ~3;
	LCDSTAT |= lcd_mode;

	if (old < 252 && scanlineClocks >= 252 && LY < 144) { // H-Blank Interrupt
		if (LCDSTAT & 0x08)
			requestInterrupt(0x02);
		hdma_transfer_continue();
	}
	if (scanlineClocks < old && LY == 144) { // V-Blank Interrupt
		if (LCDSTAT & 0x10)
			requestInterrupt(0x02);
		requestInterrupt(0x01);
		isFrameReady = true;
	}
	if (scanlineClocks < old && LY < 144) { // OAM Interrupt
		if (LCDSTAT & 0x20)
			requestInterrupt(0x02);
		lcd_draw_scanline();
	}
	if (scanlineClocks < old && coincidenceFlag) { // Coincidence Interrupt
		if (LCDSTAT & 0x40)
			requestInterrupt(0x02);
	}
}

void lcd_clear() {
	for (int i=0; i<160*144; i++)
		screen_pixels[i] = dmg_screen_palette[0];
}

void lcd_draw_scanline() {
	static int windowInternalLineCounter = -1;
	if (LY == 0)
		windowInternalLineCounter = -1;
	// Draw BG & Window
	uint8_t *BGTileMap  = vram + ((LCDC&0x08) ? 0x1c00 : 0x1800);
	uint8_t *WinTileMap = vram + ((LCDC&0x40) ? 0x1c00 : 0x1800);
	uint8_t *tileData   = vram + ((LCDC&0x10) ? 0 : 0x800);
	bool isWindowDisplayEnabled = LCDC&0x20;
	bool isBGDisplayEnabled = (LCDC&0x01) || gameMode==MODE_GBC;
	bool BGPriority[160];
	bool BGTransparent[160];
	int WY=mem[0xff4a], WX=mem[0xff4b]-7;
	int SCY=mem[0xff42], SCX=mem[0xff43];
	bool incrementWindowInternalLineCounter = false;
	for (int screenX=0; screenX<160; screenX++) {
		uint8_t *tileMap;
		int x, y; // Coordinates in the BG/Window
		if (isWindowDisplayEnabled && WY<=LY && WX<=screenX) {
			x = (screenX-WX)&0xff;
			y = ((int)LY-WY)&0xff;
			tileMap = WinTileMap;
			if (windowInternalLineCounter >= 0) {
				y = windowInternalLineCounter;
				incrementWindowInternalLineCounter = true;
			} else {
				windowInternalLineCounter = y;
			}
		} else if (isBGDisplayEnabled) {
			x = (screenX+SCX)&0xff;
			y = ((int)LY+SCY)&0xff;
			tileMap = BGTileMap;
		} else {
			break;
		}
		int tileX = x>>3;
		int tileY = y>>3;
		int u = x&7;
		int v = y&7;
		uint8_t attributes = tileMap[0x2000 + tileY*32 + tileX]; // Map Attributes in Bank 1
		bool flipX = (gameMode==MODE_GBC) && (attributes & 0x20);
		bool flipY = (gameMode==MODE_GBC) && (attributes & 0x40);
		int tileIndex = tileMap[tileY*32 + tileX];
		if (!(LCDC&0x10))
			tileIndex = (int8_t)tileIndex + 128;
		uint8_t *tile = tileData + tileIndex*16;
		if (gameMode == MODE_GBC && (attributes & 0x8))
			tile += 0x2000; // Tile in Bank 1
		uint16_t pixels = ((uint16_t*)tile)[flipY ? (7-v) : v];
		uint32_t px = pixels >> (flipX ? u : (7-u));
		px = (px>>7&2) | (px&1);
		BGTransparent[screenX] = (px == 0);
		BGPriority[screenX] = (attributes & 0x80) && (LCDC&0x01);
		if (gameMode == MODE_DMG) {
			px = (mem[0xff47]>>(px*2))&3;
		} else {
			px = px+4*(attributes&7);
		}
		if (hardwareMode == MODE_DMG){
			px = dmg_screen_palette[px];
		} else {
			px = ((uint16_t*)gbc_backgr_palettes)[px];
			uint8_t r = ((px >> 0 ) & 0x1f) * 256/32;
			uint8_t g = ((px >> 5 ) & 0x1f) * 256/32;
			uint8_t b = ((px >> 10) & 0x1f) * 256/32;
			px = 0xff000000 | (b << 16) | (g << 8) | (r << 0);
		}
		screen_pixels[(int)LY*160 + screenX] = px;
	}
	if (incrementWindowInternalLineCounter)
		windowInternalLineCounter++;

	// Draw Sprites
	int sorted_sprite_indexes[40];
	oam_search(sorted_sprite_indexes);
	uint8_t *spriteAttrTable = mem+0xfe00;
	bool isSpriteDisplayEnabled = LCDC&0x02;
	for (int i=9; i>=0; i--) {
		if (!isSpriteDisplayEnabled)
			break;
		uint8_t *sprite = spriteAttrTable + sorted_sprite_indexes[i]*4;
		int tile_number = sprite[2];
		int spriteY = (int)sprite[0] - 16;
		int v = (int)LY - spriteY;
		bool flipX = sprite[3]&0x20;
		bool flipY = sprite[3]&0x40;
		if (v < 0)
			continue;
		if (LCDC & 0b100) { // 8x16
			if (v >= 16)
				continue;
			bool isUpperTile = (v < 8);
			if (flipY)
				isUpperTile = !isUpperTile;
			if (isUpperTile)
				tile_number &= 0xFE;
			else
				tile_number |= 1;
			v %= 8;
		}
		else { // 8x8
			if (v >= 8)
				continue;
		}
		int spriteX = (int)sprite[1] - 8;
		uint8_t *tile = vram + tile_number*16;
		if (gameMode==MODE_GBC && sprite[3]&8)
			tile += 0x2000;
		uint16_t pixels = ((uint16_t*)tile)[flipY ? (7-v) : v];
		uint8_t dmgSpritePalette = mem[(sprite[3]&0x10) ? 0xff49 : 0xff48];
		for (int screenX=max(0, spriteX); screenX<min(160, spriteX+8); screenX++) {
			if (gameMode==MODE_GBC && !BGTransparent[screenX] &&
				(BGPriority[screenX] || (sprite[3]&0x80)))
				continue;
			if (gameMode==MODE_DMG && !BGTransparent[screenX] && (sprite[3]&0x80))
				continue;
			int u = screenX - spriteX;
			uint32_t px = pixels >> (flipX ? u : (7-u));
			px = (px>>7&2) | (px&1);
			if (!px) // transparent
				continue;
			if (gameMode == MODE_DMG) {
				px = (dmgSpritePalette>>(px*2))&3;
			} else {
				px = px+4*(sprite[3]&7);
			}
			if (hardwareMode == MODE_DMG){
				px = dmg_screen_palette[px];
			} else {
				px = ((uint16_t*)gbc_sprite_palettes)[px];
				uint8_t r = ((px >> 0 ) & 0x1f) * 256/32;
				uint8_t g = ((px >> 5 ) & 0x1f) * 256/32;
				uint8_t b = ((px >> 10) & 0x1f) * 256/32;
				px = 0xff000000 | (b << 16) | (g << 8) | (r << 0);
			}
			screen_pixels[(int)LY*160 + screenX] = px;
		}
	}
}

bool sprite_is_visible(uint8_t x, uint8_t y) {
	uint8_t h = (LCDC & 0b100) ? 16 : 8;
	if (x == 0)
		return false;
	if (LY < y)
		return false;
	if (LY >= (y + h))
		return false;
	return true;
}

int sprite_compare(const void *a, const void *b) {
	int i_a = *(int*)a;
	uint8_t *sprite_a = mem+0xfe00 + i_a*4;
	uint8_t y_a = sprite_a[0] - 16;
	uint8_t x_a = sprite_a[1];
	bool vis_a = sprite_is_visible(x_a, y_a);
	int i_b = *(int*)b;
	uint8_t *sprite_b = mem+0xfe00 + i_b*4;
	uint8_t y_b = sprite_b[0] - 16;
	uint8_t x_b = sprite_b[1];
	bool vis_b = sprite_is_visible(x_b, y_b);

	if (vis_a != vis_b)
		return vis_a ? -1 : 1;
	int x_diff = (int)x_a - x_b;
	if (gameMode==MODE_DMG && x_diff && abs(x_diff) < 8)
		return x_diff;
	return i_a - i_b;
}

void oam_search(int array[40]) {
	for (int i=0; i<40; i++)
		array[i] = i;
	qsort(array, 40, sizeof(int), sprite_compare);
}