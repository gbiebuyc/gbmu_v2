#include "emu.h"

#define LCDC mem[0xff40]

uint32_t dmg_palette[] = {0xffffffff, 0xffaaaaaa, 0xff555555, 0xff000000};
uint8_t gbc_backgr_palettes[8*4*2]; // 8 palettes of 4 colors of 2 bytes.
uint8_t gbc_sprite_palettes[8*4*2];

void lcd_clear() {
	for (int i=0; i<160*144; i++)
		screen_pixels[i] = dmg_palette[0];
}

void lcd_draw_current_row() {
	// Draw BG & Window
	uint8_t *BGTileMap  = vram + ((LCDC&0x08) ? 0x1c00 : 0x1800);
	uint8_t *WinTileMap = vram + ((LCDC&0x40) ? 0x1c00 : 0x1800);
	uint8_t *tileData   = vram + ((LCDC&0x10) ? 0 : 0x800);
	bool isWindowDisplayEnabled = LCDC&0x20;
	bool isBGDisplayEnabled = (LCDC&0x01) || hardwareMode==MODE_GBC;
	bool BGPriority[160];
	memset(BGPriority, 0, sizeof(BGPriority));
	for (int screenX=0; screenX<160; screenX++) {
		int WY=mem[0xff4a], WX=mem[0xff4b]-7;
		int SCY=mem[0xff42], SCX=mem[0xff43];
		uint8_t *tileMap;
		int x, y; // Coordinates in the BG/Window
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
		uint8_t attributes = tileMap[0x2000 + tileY*32 + tileX]; // Map Attributes in Bank 1
		bool flipX = (hardwareMode==MODE_GBC) && (attributes & 0x20);
		bool flipY = (hardwareMode==MODE_GBC) && (attributes & 0x40);
		int tileIndex = tileMap[tileY*32 + tileX];
		if (!(LCDC&0x10))
			tileIndex = (int8_t)tileIndex + 128;
		uint8_t *tile = tileData + tileIndex*16;
		if (hardwareMode == MODE_GBC && (attributes & 0x8))
			tile += 0x2000; // Tile in Bank 1
		uint16_t pixels = ((uint16_t*)tile)[flipY ? (7-v) : v];
		uint32_t px = pixels >> (flipX ? u : (7-u));
		px = (px>>7&2) | (px&1);
		BGPriority[screenX] = (attributes & 0x80) && (LCDC&0x01) && (px != 0);
		if (hardwareMode == MODE_DMG){
			px = dmg_palette[(mem[0xff47]>>(px*2))&3];
		} else {
			px = ((uint16_t*)gbc_backgr_palettes)[px+4*(attributes&7)];
			uint8_t r = ((px >> 0 ) & 0x1f) * 256/32;
			uint8_t g = ((px >> 5 ) & 0x1f) * 256/32;
			uint8_t b = ((px >> 10) & 0x1f) * 256/32;
			px = 0xff000000 | (b << 16) | (g << 8) | (r << 0);
		}
		screen_pixels[LY*160 + screenX] = px;
	}

	// Draw Sprites
	uint8_t *spriteAttrTable = mem+0xfe00;
	bool isSpriteDisplayEnabled = LCDC&0x02;
	int spritePerLineLimit = 10;
	for (int i=0; i<40; i++) {
		if (!isSpriteDisplayEnabled || !spritePerLineLimit)
			break;
		uint8_t *sprite = spriteAttrTable + i*4;
		int tile_number = sprite[2];
		int spriteY = (int)sprite[0] - 16;
		int v = LY - spriteY;
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
		spritePerLineLimit--;
		int spriteX = (int)sprite[1] - 8;
		uint8_t *tile = vram + tile_number*16;
		if (hardwareMode==MODE_GBC && sprite[3]&8)
			tile += 0x2000;
		uint16_t pixels = ((uint16_t*)tile)[flipY ? (7-v) : v];
		uint8_t spritePalette = mem[(sprite[3]&10) ? 0xff49 : 0xff48];
		for (int screenX=max(0, spriteX); screenX<min(160, spriteX+8); screenX++) {
			if (hardwareMode==MODE_GBC && BGPriority[screenX])
				continue;
			int u = screenX - spriteX;
			uint32_t px = pixels >> (flipX ? u : (7-u));
			px = (px>>7&2) | (px&1);
			if (!px) // transparent
				continue;
			if (hardwareMode == MODE_DMG){
				px = dmg_palette[(spritePalette>>(px*2))&3];
			} else {
				px = ((uint16_t*)gbc_sprite_palettes)[px+4*(sprite[3]&7)];
				uint8_t r = ((px >> 0 ) & 0x1f) * 256/32;
				uint8_t g = ((px >> 5 ) & 0x1f) * 256/32;
				uint8_t b = ((px >> 10) & 0x1f) * 256/32;
				px = 0xff000000 | (b << 16) | (g << 8) | (r << 0);
			}
			screen_pixels[LY*160 + screenX] = px;
		}
	}
}