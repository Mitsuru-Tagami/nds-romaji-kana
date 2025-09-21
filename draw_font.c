#include <nds.h>
#include "mplus_font_10x10.h"
#include "mplus_font_10x10alpha.h"

//FrameBuffer
int drawFont(int x, int y, u16* buffer, u16 code, u16 color) {
	int i, j;
	u16 bit;
	u16 block;
	
	buffer += y * SCREEN_WIDTH + x;
	
	// 1 バイト文字
	if ( code < 0x100 ) {
		u8 block8;
		u8 bit8;
		
		for ( i = 0; i < 13; i++ ) {
			u16* line = buffer + (SCREEN_WIDTH * i);
			bit8 = 0x80;
			block8 = FONT_MPLUS_10x10A[code][i];
		
			for ( j = 0; j < 8; j++ ) {
				if ( (block8 & bit8) > 0 ) {
					*(line + j) = color;
				}
				bit8 = bit8 >> 1;
			}
		}
        return 8; // Return width for single-byte characters
	} else {
	// 2バイト文字
		for ( i = 0; i < 11; i++ ) {
			uint16* line = buffer + (SCREEN_WIDTH * i);
			bit = 0x8000;
			block = FONT_MPLUS_10x10[code][i];
		
			for ( j = 0; j < 11; j++ ) {
				if ( (block & bit) > 0 ) {
					*(line + j) = color;
				}
				bit = bit >> 1;
			}
		}
        return 11; // Return width for two-byte characters
	}
    return 0; // Should not be reached, but for safety
}