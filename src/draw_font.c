#include <nds.h>
#include "mplus_font_10x10.h"
#include "mplus_font_10x10alpha.h"

int drawFont(int x, int y, u16* buffer, u16 code, u16 color) {
	int i, j;
	u16 bit;
	u16 block;
	u8 block8;
	u8 bit8;
	int empty;
	buffer += y * SCREEN_WIDTH + x;

	// 1バイト文字（未定義なら必ず豆腐表示）
	if (code < 0x100) {
		empty = 1;
		for (i = 0; i < 13; i++) {
			if (FONT_MPLUS_10x10A[code][i] != 0) empty = 0;
		}
		if (empty) code = 0xA1; // □ (U+25A1) に強制
		// 再判定：豆腐自体も未定義なら完全に空になるので、最低限0xA1は定義しておくこと
		for (i = 0; i < 13; i++) {
			u16* line = buffer + (SCREEN_WIDTH * i);
			bit8 = 0x80;
			block8 = FONT_MPLUS_10x10A[code][i];
			for (j = 0; j < 8; j++) {
				if ((block8 & bit8) > 0) {
					*(line + j) = color;
				}
				bit8 = bit8 >> 1;
			}
		}
		return 8; // Return width for single-byte characters
	}
	// 2バイト文字（未定義なら必ず豆腐表示）
	empty = 1;
	for (i = 0; i < 11; i++) {
		if (FONT_MPLUS_10x10[code][i] != 0) empty = 0;
	}
	if (empty) code = 0x25A1; // □ (U+25A1) に強制
	// 再判定：豆腐自体も未定義なら完全に空になるので、最低限0x25A1は定義しておくこと
	for (i = 0; i < 11; i++) {
		uint16* line = buffer + (SCREEN_WIDTH * i);
		bit = 0x8000;
		block = FONT_MPLUS_10x10[code][i];
		for (j = 0; j < 11; j++) {
			if ((block & bit) > 0) {
				*(line + j) = color;
			}
			bit = bit >> 1;
		}
	}
	return 11; // Return width for two-byte characters
}