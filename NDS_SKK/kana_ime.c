#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <fat.h> // For NDS file I/O

#include "kana_ime.h"
#include "draw_font.h"
#include "romakana_map.h"
#include "skk.h"
#include "JString.h"

#define DEBUG_MODE 1 // デバッグモード有効

SKK skk_engine; // Global SKK engine instance

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

    // Initialize libfat
    if (!fatMountDefault()) {
        iprintf("fatMountDefault failed!\n");
        while (1) swiWaitForVBlank();
    }

    // Initialize SKK engine
    if (!skk_engine.begin("/data/")) {
        iprintf("SKK engine initialization failed!\n");
        while (1) swiWaitForVBlank();
    }
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

    // --- Romaji to Kana Conversion (SKK Integration) ---
    char kouho_list[256];
    char out_okuri[32];
    uint8_t skk_rc = 0;

    if (input_romaji_len > 0) {
        skk_rc = skk_engine.get_kouho_list(kouho_list, out_okuri, input_romaji_buffer);
    }

    converted_kana_len = 0;
    converted_kana_buffer[0] = 0;

    if (skk_rc > 0) {
        // For now, just take the first candidate
        char first_kouho[256];
        if (skk_engine.get_kouho(first_kouho, kouho_list, 0)) {
            // Convert UTF-8 to NDS display format (u16, likely Shift-JIS or similar)
            // This is a placeholder and needs proper UTF-8 to Shift-JIS/UTF-16 conversion
            // For now, just copy as-is, assuming font can handle it or it's ASCII
            int utf8_len = strlen(first_kouho);
            int buffer_idx = 0;
            int current_utf8_pos = 0;
            while (current_utf8_pos < utf8_len && buffer_idx < 255) {
                char utf8_char_buf[5]; // Max 4 bytes + null terminator
                int char_bytes = JString::get(utf8_char_buf, &first_kouho[current_utf8_pos]);
                if (char_bytes == 0) break; // Error or end of string

                uint32_t unicode_char = JString::utf8to32(utf8_char_buf);
                converted_kana_buffer[buffer_idx++] = (u16)unicode_char; 
                current_utf8_pos += char_bytes;
            }
            converted_kana_len = buffer_idx;
            converted_kana_buffer[converted_kana_len] = 0; // Null terminate
        }
    }

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
