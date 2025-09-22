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
        // This function is buggy for non-alphanumeric chars, but works for now.
        char c = str[i];
        uint16_t code = (uint16_t)c;
        current_x += drawFont(current_x, y, buffer, code, color);
    }
}

// Helper function to draw a string from a u16 array
static void drawStringU16(int x, int y, u16* buffer, const u16* str_u16, u16 color) {
    int current_x = x;
    for (int i = 0; str_u16[i] != 0; i++) {
        current_x += drawFont(current_x, y, buffer, str_u16[i], color);
    }
}

static u16* mainScreenBuffer = NULL;
static char input_romaji_buffer[32] = {0};
static int input_romaji_len = 0;
static u16 converted_kana_buffer[256] = {0};
static int converted_kana_len = 0;

static u16 s_final_output_buffer[256] = {0}; // Buffer for committed text
static int s_final_output_len = 0;

ImeMode currentImeMode = IME_MODE_HIRAGANA;

// Function to switch input modes
static void switchMode(void) {
    currentImeMode = (ImeMode)((currentImeMode + 1) % 4);
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
            } else if (s_final_output_len > 0) {
                s_final_output_len--;
                s_final_output_buffer[s_final_output_len] = 0;
            }
        } else if (key == '\n') { // Enter key: commit the converted text
            if (converted_kana_len > 0 && (s_final_output_len + converted_kana_len) < 255) {
                memcpy(&s_final_output_buffer[s_final_output_len], converted_kana_buffer, converted_kana_len * sizeof(u16));
                s_final_output_len += converted_kana_len;
                s_final_output_buffer[s_final_output_len] = 0;
            }
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';
        } else if (key == ' ') { // Space key: commit and add a space
             if (converted_kana_len > 0 && (s_final_output_len + converted_kana_len) < 255) {
                memcpy(&s_final_output_buffer[s_final_output_len], converted_kana_buffer, converted_kana_len * sizeof(u16));
                s_final_output_len += converted_kana_len;
            }
            if (s_final_output_len < 255) {
                s_final_output_buffer[s_final_output_len++] = (u16)' ';
            }
            s_final_output_buffer[s_final_output_len] = 0;
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';
        } else { 
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || key == '-' || key == '"') {
                if (input_romaji_len < 30) {
                    input_romaji_buffer[input_romaji_len++] = (char)key;
                    input_romaji_buffer[input_romaji_len] = '\0';
                }
            } else { // Directly commit other keys as-is (e.g. symbols)
                if(s_final_output_len < 255) {
                    s_final_output_buffer[s_final_output_len++] = (u16)key;
                }
                s_final_output_buffer[s_final_output_len] = 0;
            }
        }
    }

    // --- Conversion Logic ---
    converted_kana_len = 0;
    converted_kana_buffer[0] = 0;

    if (currentImeMode == IME_MODE_HIRAGANA || currentImeMode == IME_MODE_KATAKANA) {
        if (input_romaji_len > 0) {
            int current_romaji_pos = 0;
            int buffer_idx = 0;

            while (current_romaji_pos < input_romaji_len && buffer_idx < 255) {
                int best_match_len = 0;
                u16 best_match_sjis = 0;

                for (int i = 0; romakana_map[i].romaji != NULL; i++) {
                    int romaji_len = strlen(romakana_map[i].romaji);
                    if (romaji_len > best_match_len && 
                        (current_romaji_pos + romaji_len) <= input_romaji_len &&
                        strncmp(&input_romaji_buffer[current_romaji_pos], romakana_map[i].romaji, romaji_len) == 0) 
                    {
                        best_match_len = romaji_len;
                        best_match_sjis = romakana_map[i].sjis_code;
                    }
                }

                if (best_match_len > 0) {
                    u16 sjis_code = best_match_sjis;
                    if (currentImeMode == IME_MODE_KATAKANA) {
                        u16 hira_code = best_match_sjis; // Keep original hiragana code for the fix
                        // Convert Hiragana SJIS to Katakana SJIS by adding 0xA1 offset.
                        if (hira_code >= 0x829f && hira_code <= 0x82f1) {
                            sjis_code += 0xA1;
                            // Fix for the font data shift discovered by the user.
                            // The glyphs for MU and subsequent characters are shifted by 1.
                            if (hira_code >= 0x82de) { // む (mu) and onwards
                                sjis_code += 1;
                            }
                        }
                    }
                    converted_kana_buffer[buffer_idx++] = sjis_code;
                    current_romaji_pos += best_match_len;
                } else {
                    current_romaji_pos++;
                }
            }
            converted_kana_len = buffer_idx;
            converted_kana_buffer[converted_kana_len] = 0;
        }
    } else if (currentImeMode == IME_MODE_ENGLISH) {
        if (input_romaji_len > 0) {
            int buffer_idx = 0;
            for (int i = 0; i < input_romaji_len && buffer_idx < 255; i++) {
                converted_kana_buffer[buffer_idx++] = (u16)input_romaji_buffer[i];
            }
            converted_kana_len = buffer_idx;
            converted_kana_buffer[converted_kana_len] = 0;
        }
    }

    // --- Drawing ---
    dmaFillWords(0, mainScreenBuffer, 256 * 192 * 2);
    
    // Draw final committed output
    drawStringU16(10, 10, mainScreenBuffer, s_final_output_buffer, RGB15(31,31,31));

    // Draw current (uncommitted) text
    int x = 10;
    if(s_final_output_len > 0) {
        x = 10 + (s_final_output_len * 11); // Estimate width
    }
    drawStringU16(x, 10, mainScreenBuffer, converted_kana_buffer, RGB15(31,31,31));

    // Draw debug info
    char debug_str[128];
    const char* mode_prompt = "";
    switch (currentImeMode) {
        case IME_MODE_HIRAGANA: mode_prompt = "HIRAGANA: "; break;
        case IME_MODE_KATAKANA: mode_prompt = "KATAKANA: "; break;
        case IME_MODE_ENGLISH:  mode_prompt = "ENGLISH:  ";  break;
        case IME_MODE_DEBUG:    mode_prompt = "DEBUG:    ";    break;
    }

    sprintf(debug_str, "%sKey: %c", mode_prompt, (key > 31 && key < 127) ? key : '?');
    drawString(10, 30, mainScreenBuffer, debug_str, RGB15(31,31,31));

    sprintf(debug_str, "Romaji: %s", input_romaji_buffer);
    drawString(10, 40, mainScreenBuffer, debug_str, RGB15(31,31,31));

    return true; // Continue main loop
}

void kanaIME_showKeyboard(void) { keyboardShow(); }
void kanaIME_hideKeyboard(void) { keyboardHide(); }
char kanaIME_getChar(void) { return 0; }
