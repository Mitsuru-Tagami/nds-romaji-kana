#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include "kana_ime.h"
#include "draw_font.h"
#include "romakana_map.h"

#define DEBUG_MODE 1 // デバッグモード有効

// New function to convert ASCII numbers/symbols to full-width Shift-JIS
static u16 get_fullwidth_sjis(char ascii_char) {
    if (ascii_char >= '0' && ascii_char <= '9') {
        return 0x824f + (ascii_char - '0');
    }
    return (u16)ascii_char;
}

// Helper function to draw a string
static void drawString(int x, int y, u16* buffer, const char* str, u16 color) {
    int current_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        current_x += drawFont(current_x, y, buffer, (u16)str[i], color);
    }
}

static u16* mainScreenBuffer = NULL;
static char input_romaji_buffer[32] = {0};
static int input_romaji_len = 0;
static u16 converted_kana_buffer[256] = {0};
static int converted_kana_len = 0;

ImeMode currentImeMode = IME_MODE_HIRAGANA;

// Function to switch input modes
static void switchMode(void) {
    currentImeMode = (currentImeMode + 1) % 4;
    input_romaji_len = 0;
    input_romaji_buffer[0] = '\0';
    converted_kana_len = 0;
    converted_kana_buffer[0] = 0;
}

void kanaIME_init(void) {
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);
    mainScreenBuffer = (u16*)VRAM_A;

    consoleDemoInit();
    keyboardDemoInit();
    keyboardShow();
}

bool kanaIME_update(void) {
    scanKeys();
    int key = keyboardUpdate();
    touchPosition touch;

    // --- Input Handling ---
    if (keysDown() & KEY_START) {
        return false; // Exit main loop
    }

    if (keysDown() & KEY_TOUCH) {
        touchRead(&touch);
        // Menu area: top-right corner (56px width, 20px height)
        if (touch.px > (256 - 56) && touch.py < 20) {
            switchMode();
        }
    } else if (keysDown() & KEY_SELECT) {
        switchMode();
    }

    // --- Character Processing ---
    if (key > 0) {
        if (key == '\b') { 
            if (input_romaji_len > 0) {
                input_romaji_len--;
                input_romaji_buffer[input_romaji_len] = '\0';
            } else if (converted_kana_len > 0) {
                converted_kana_len--;
                converted_kana_buffer[converted_kana_len] = 0;
            }
        } else if (key == '\n') { 
            if (input_romaji_len == 1 && input_romaji_buffer[0] == 'n') {
                if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = 0x82f1; // ん
                }
                input_romaji_len = 0;
                input_romaji_buffer[0] = '\0';
            }
        } else if (key == ' ') {
            if (input_romaji_len == 1 && input_romaji_buffer[0] == 'n') {
                if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = 0x82f1; // ん
                }
            }
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';

            if(converted_kana_len < 255) {
                converted_kana_buffer[converted_kana_len++] = 0x8140; // Space
            }
        } else { 
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) {
                if (input_romaji_len < 30) {
                    input_romaji_buffer[input_romaji_len++] = (char)key;
                    input_romaji_buffer[input_romaji_len] = '\0';
                }
            } else { 
                if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = get_fullwidth_sjis((char)key);
                }
            }
        }
    }

    // --- Romaji to Kana Conversion ---
    bool converted_in_pass;
    do {
        converted_in_pass = false;
        if (input_romaji_len >= 2 && input_romaji_buffer[0] == input_romaji_buffer[1] && input_romaji_buffer[0] != 'n') {
            char c = input_romaji_buffer[0];
            if (strchr("kstnhmyrwgzdbpv", c) != NULL) {
                 if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = 0x82c1; // っ
                }
                memmove(input_romaji_buffer, input_romaji_buffer + 1, input_romaji_len);
                input_romaji_len--;
                converted_in_pass = true;
                continue;
            }
        }

        for (int i = 0; romakana_map[i].romaji != NULL; i++) {
            const char* romaji = romakana_map[i].romaji;
            int len = strlen(romaji);

            if (strncmp(input_romaji_buffer, romaji, len) == 0) {
                if (len == 1 && *romaji == 'n') {
                    char next_char = input_romaji_buffer[1];
                    if (next_char != '\0' && strchr("aiueoy',", next_char) == NULL) {
                    } else {
                        continue;
                    }
                }

                if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = romakana_map[i].sjis_code;
                }
                
                memmove(input_romaji_buffer, input_romaji_buffer + len, input_romaji_len - len + 1);
                input_romaji_len -= len;
                converted_in_pass = true;
                break;
            }
        }
    } while (converted_in_pass);

    // --- Drawing ---
    dmaFillWords(0, mainScreenBuffer, 256 * 192 * 2);
    
    // Draw converted kana and input romaji
    int x = 10;
    for (int i = 0; i < converted_kana_len; i++) {
        x += drawFont(x, 10, mainScreenBuffer, converted_kana_buffer[i], RGB15(31,31,31));
    }
    for (int i = 0; i < input_romaji_len; i++) {
        x += drawFont(x, 10, mainScreenBuffer, (u16)input_romaji_buffer[i], RGB15(31,31,31));
    }

    // Draw debug info
    char debug_str[128];

    const char* mode_prompt = "";
    switch (currentImeMode) {
        case IME_MODE_HIRAGANA: mode_prompt = "[HIRAGANA] "; break;
        case IME_MODE_KATAKANA: mode_prompt = "[KATAKANA] "; break;
        case IME_MODE_ENGLISH:  mode_prompt = "[ENGLISH] ";  break;
        case IME_MODE_DEBUG:    mode_prompt = "[DEBUG] ";    break;
    }

    sprintf(debug_str, "%sKey: %c", mode_prompt, (key > 31 && key < 127) ? key : '?');
    drawString(10, 30, mainScreenBuffer, debug_str, RGB15(31,31,31));

    sprintf(debug_str, "Romaji: %s (%d)", input_romaji_buffer, input_romaji_len);
    drawString(10, 40, mainScreenBuffer, debug_str, RGB15(31,31,31));

    sprintf(debug_str, "KanaLen: %d", converted_kana_len);
    drawString(10, 50, mainScreenBuffer, debug_str, RGB15(31,31,31));

    #if DEBUG_MODE
    // Simplified debug info for clarity
    #endif

    return true; // Continue main loop
}

void kanaIME_showKeyboard(void) { keyboardShow(); }
void kanaIME_hideKeyboard(void) { keyboardHide(); }
char kanaIME_getChar(void) { return 0; }
