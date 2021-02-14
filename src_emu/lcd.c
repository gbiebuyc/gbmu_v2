#include "emu.h"

uint32_t dmg_palette[] = {0xffffffff, 0xffaaaaaa, 0xff555555, 0xff000000};
uint8_t gbc_backgr_palettes[8*4*2]; // 8 palettes of 4 colors of 2 bytes.
uint8_t gbc_sprite_palettes[8*4*2];

void lcd_clear() {
	for (int i=0; i<160*144; i++)
		screen_pixels[i] = dmg_palette[0];
}

void lcd_draw_current_row() {
	// Draw BG & Window
	uint8_t *BGTileMap  = mem + ((mem[0xff40]&0x08) ? 0x9c00 : 0x9800);
	uint8_t *WinTileMap = mem + ((mem[0xff40]&0x40) ? 0x9c00 : 0x9800);
	uint8_t *tileData   = mem + ((mem[0xff40]&0x10) ? 0x8000 : 0x8800);
	bool isWindowDisplayEnabled = mem[0xff40]&0x20;
	bool isBGDisplayEnabled = mem[0xff40]&0x01;
	for (int screenX=0; screenX<160; screenX++) {
		int WY=mem[0xff4a], WX=mem[0xff4b]-7;
		int SCY=mem[0xff42], SCX=mem[0xff43];
		uint8_t *tileMap;
		int x, y;
		if (isWindowDisplayEnabled && WY<=LY && WX<=screenX) {
			x = (screenX-WX)&0xff;
			y = (LY-WY)&0xff;
			tileMap = WinTileMap;
		} else if (isBGDisplayEnabled) {
			x = (screenX+SCX)&0xff;
			y = (LY+SCY)&0xff;
			tileMap = BGTileMap;
		} else {
			break;
		}
		int tileX = x>>3;
		int tileY = y>>3;
		int u = x&7;
		int v = y&7;
		int tileIndex = tileMap[tileY*32 + tileX];
		if (!(mem[0xff40]&0x10))
			tileIndex = (int8_t)tileIndex + 128;
		uint8_t *tile = tileData + tileIndex*16;
		uint16_t pixels = ((uint16_t*)tile)[v];
		uint32_t px = pixels >> (7-u);
		px = (px>>7&2) | (px&1);
		if (hardwareMode == MODE_DMG){
			px = dmg_palette[(mem[0xff47]>>(px*2))&3];
		} else {
			px = ((uint16_t*)gbc_backgr_palettes)[px];
			uint8_t r = ((px >> 0 ) & 0x1f) * 256/32;
			uint8_t g = ((px >> 5 ) & 0x1f) * 256/32;
			uint8_t b = ((px >> 10) & 0x1f) * 256/32;
			px = (r << 16) | (g << 8) | (b << 0);
		}
		screen_pixels[LY*160 + screenX] = px;
	}

	// Draw Sprites
	uint8_t *spriteAttrTable = mem+0xfe00;
	uint8_t *spriteData = mem+0x8000;
	bool isSpriteDisplayEnabled = mem[0xff40]&0x02;
	for (int i=0; i<40; i++) {
		if (!isSpriteDisplayEnabled)
			break;
		uint8_t *sprite = spriteAttrTable + i*4;
		int spriteY = (int)sprite[0] - 16;
		int v = LY - spriteY;
		if (v < 0 || v > 7)
			continue;
		bool flipX = sprite[3]&0x20;
		bool flipY = sprite[3]&0x40;
		int spriteX = (int)sprite[1] - 8;
		uint8_t *tile = spriteData + sprite[2]*16;
		uint16_t pixels = ((uint16_t*)tile)[flipY ? (7-v) : v];
		uint8_t spritePalette = mem[(sprite[3]&10) ? 0xff49 : 0xff48];
		for (int screenX=max(0, spriteX); screenX<min(160, spriteX+8); screenX++) {
			int u = screenX - spriteX;
			uint32_t px = pixels >> (flipX ? u : (7-u));
			px = (px>>7&2) | (px&1);
			if (!px) // transparent
				continue;
			px = dmg_palette[(spritePalette>>(px*2))&3];
			screen_pixels[LY*160 + screenX] = px;
		}
	}
}