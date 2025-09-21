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

static char s_kouho_list[256] = {0}; // Stored candidate list from SKK
static char s_out_okuri[32] = {0};   // Stored okuri from SKK
static uint16_t s_current_candidate_index = 0; // Index of the currently selected candidate
static uint16_t s_num_candidates = 0; // Total number of candidates

static u16 s_final_output_buffer[256] = {0}; // Buffer for committed text
static int s_final_output_len = 0;

ImeMode currentImeMode = IME_MODE_HIRAGANA;

// Function to switch input modes
static void switchMode(void) {
    currentImeMode = (currentImeMode + 1) % 4;
    input_romaji_len = 0;
    input_romaji_buffer[0] = '\0';
    converted_kana_len = 0;
    converted_kana_buffer[0] = 0;
    s_current_candidate_index = 0;
    s_num_candidates = 0;
    s_kouho_list[0] = '\0';
    s_out_okuri[0] = '\0';
    s_final_output_len = 0;
    s_final_output_buffer[0] = 0;
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
    } else if (keysDown() & KEY_UP) { // Cycle through candidates
        if (s_num_candidates > 0) {
            s_current_candidate_index = (s_current_candidate_index + 1) % s_num_candidates;
        }
    } else if (keysDown() & KEY_DOWN) { // Cycle through candidates (reverse)
        if (s_num_candidates > 0) {
            s_current_candidate_index = (s_current_candidate_index + s_num_candidates - 1) % s_num_candidates;
        }
    }

    // --- Character Processing ---
    if (key > 0) {
        if (key == '\b') { 
            if (input_romaji_len > 0) {
                input_romaji_len--;
                input_romaji_buffer[input_romaji_len] = '\0';
            } else if (s_final_output_len > 0) { // Backspace on final output
                s_final_output_len--;
                s_final_output_buffer[s_final_output_len] = 0;
            }
            s_current_candidate_index = 0; // Reset candidate selection on backspace
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        } else if (key == '\n') { // Enter key to commit
            if (s_num_candidates > 0) {
                char selected_kouho[256];
                if (skk_engine.get_kouho(selected_kouho, s_kouho_list, s_current_candidate_index)) {
                    char final_output_utf8[256];
                    if (currentImeMode == IME_MODE_KATAKANA) {
                        skk_engine.kana_to_katakana(final_output_utf8, selected_kouho);
                    } else {
                        strcpy(final_output_utf8, selected_kouho);
                    }

                    int utf8_len = strlen(final_output_utf8);
                    int current_utf8_pos = 0;
                    while (current_utf8_pos < utf8_len && s_final_output_len < 255) {
                        char utf8_char_buf[5];
                        int char_bytes = JString::get(utf8_char_buf, &final_output_utf8[current_utf8_pos]);
                        if (char_bytes == 0) break;
                        uint32_t unicode_char = JString::utf8to32(utf8_char_buf);
                        s_final_output_buffer[s_final_output_len++] = (u16)unicode_char;
                        current_utf8_pos += char_bytes;
                    }
                    s_final_output_buffer[s_final_output_len] = 0;
                }
            }
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';
            s_current_candidate_index = 0;
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        } else if (key == ' ') { // Space key to commit or insert space
            if (s_num_candidates > 0) {
                char selected_kouho[256];
                if (skk_engine.get_kouho(selected_kouho, s_kouho_list, s_current_candidate_index)) {
                    char final_output_utf8[256];
                    if (currentImeMode == IME_MODE_KATAKANA) {
                        skk_engine.kana_to_katakana(final_output_utf8, selected_kouho);
                    }
                    else {
                        strcpy(final_output_utf8, selected_kouho);
                    }

                    int utf8_len = strlen(final_output_utf8);
                    int current_utf8_pos = 0;
                    while (current_utf8_pos < utf8_len && s_final_output_len < 255) {
                        char utf8_char_buf[5];
                        int char_bytes = JString::get(utf8_char_buf, &final_output_utf8[current_utf8_pos]);
                        if (char_bytes == 0) break;
                        uint32_t unicode_char = JString::utf8to32(utf8_char_buf);
                        s_final_output_buffer[s_final_output_len++] = (u16)unicode_char;
                        current_utf8_pos += char_bytes;
                    }
                    s_final_output_buffer[s_final_output_len] = 0;
                }
            } else { // No candidates, just insert a space
                if(s_final_output_len < 255) {
                    s_final_output_buffer[s_final_output_len++] = 0x0020; // Unicode Space
                }
                s_final_output_buffer[s_final_output_len] = 0;
            }
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';
            s_current_candidate_index = 0;
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        } else { 
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) {
                if (input_romaji_len < 30) {
                    input_romaji_buffer[input_romaji_len++] = (char)key;
                    input_romaji_buffer[input_romaji_len] = '\0';
                }
            } else { 
                if(s_final_output_len < 255) {
                    s_final_output_buffer[s_final_output_len++] = get_fullwidth_sjis((char)key);
                }
                s_final_output_buffer[s_final_output_len] = 0;
            }
            s_current_candidate_index = 0; // Reset candidate selection on new input
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        }
    }

    // --- Romaji to Kana Conversion (SKK Integration) ---
    // char kouho_list[256]; // Now static s_kouho_list
    // char out_okuri[32];   // Now static s_out_okuri
    uint8_t skk_rc = 0;

    // Only perform SKK lookup if input_romaji_buffer has changed or candidates are not yet loaded
    if (input_romaji_len > 0 && s_num_candidates == 0) {
        skk_rc = skk_engine.get_kouho_list(s_kouho_list, s_out_okuri, input_romaji_buffer);
        if (skk_rc > 0) {
            s_num_candidates = skk_engine.count_kouho_list(s_kouho_list);
        } else {
            s_num_candidates = 0;
        }
    }

    converted_kana_len = 0;
    converted_kana_buffer[0] = 0;

    switch (currentImeMode) {
        case IME_MODE_HIRAGANA:
        case IME_MODE_KATAKANA:
        case IME_MODE_DEBUG: // For now, DEBUG mode behaves like HIRAGANA
            if (s_num_candidates > 0) {
                char selected_kouho[256];
                if (skk_engine.get_kouho(selected_kouho, s_kouho_list, s_current_candidate_index)) {
                    char final_output_utf8[256];
                    if (currentImeMode == IME_MODE_KATAKANA) {
                        skk_engine.kana_to_katakana(final_output_utf8, selected_kouho);
                    } else {
                        strcpy(final_output_utf8, selected_kouho);
                    }

                    int utf8_len = strlen(final_output_utf8);
                    int buffer_idx = 0;
                    int current_utf8_pos = 0;
                    while (current_utf8_pos < utf8_len && buffer_idx < 255) {
                        char utf8_char_buf[5]; // Max 4 bytes + null terminator
                        int char_bytes = JString::get(utf8_char_buf, &final_output_utf8[current_utf8_pos]);
                        if (char_bytes == 0) break; // Error or end of string

                        uint32_t unicode_char = JString::utf8to32(utf8_char_buf);
                        converted_kana_buffer[buffer_idx++] = (u16)unicode_char; 
                        current_utf8_pos += char_bytes;
                    }
                    converted_kana_len = buffer_idx;
                    converted_kana_buffer[converted_kana_len] = 0; // Null terminate
                }
            } else if (input_romaji_len > 0) {
                // If no SKK candidates, just display the romaji input as hiragana (temporary)
                char temp_hiragana[32];
                JString::roma_to_kana(temp_hiragana, input_romaji_buffer);
                int utf8_len = strlen(temp_hiragana);
                int buffer_idx = 0;
                int current_utf8_pos = 0;
                while (current_utf8_pos < utf8_len && buffer_idx < 255) {
                    char utf8_char_buf[5];
                    int char_bytes = JString::get(utf8_char_buf, &temp_hiragana[current_utf8_pos]);
                    if (char_bytes == 0) break;
                    uint32_t unicode_char = JString::utf8to32(utf8_char_buf);
                    converted_kana_buffer[buffer_idx++] = (u16)unicode_char;
                    current_utf8_pos += char_bytes;
                }
                converted_kana_len = buffer_idx;
                converted_kana_buffer[converted_kana_len] = 0;
            }
            break;

        case IME_MODE_ENGLISH:
            // In English mode, just display the romaji input directly
            // No SKK lookup needed
            for (int i = 0; i < input_romaji_len && i < 255; i++) {
                converted_kana_buffer[i] = (u16)input_romaji_buffer[i];
            }
            converted_kana_len = input_romaji_len;
            converted_kana_buffer[converted_kana_len] = 0; // Null terminate
            break;
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

    // Draw candidates
    if (s_num_candidates > 0) {
        int candidate_y = 70;
        for (int i = 0; i < s_num_candidates; i++) {
            char candidate_utf8[256];
            if (skk_engine.get_kouho(candidate_utf8, s_kouho_list, i)) {
                char final_output_utf8[256];
                if (currentImeMode == IME_MODE_KATAKANA) {
                    skk_engine.kana_to_katakana(final_output_utf8, candidate_utf8);
                } else {
                    strcpy(final_output_utf8, candidate_utf8);
                }

                u16 candidate_u16[256];
                int utf8_len = strlen(final_output_utf8);
                int buffer_idx = 0;
                int current_utf8_pos = 0;
                while (current_utf8_pos < utf8_len && buffer_idx < 255) {
                    char utf8_char_buf[5];
                    int char_bytes = JString::get(utf8_char_buf, &final_output_utf8[current_utf8_pos]);
                    if (char_bytes == 0) break;
                    uint32_t unicode_char = JString::utf8to32(utf8_char_buf);
                    candidate_u16[buffer_idx++] = (u16)unicode_char;
                    current_utf8_pos += char_bytes;
                }
                candidate_u16[buffer_idx] = 0;

                u16 color = RGB15(31,31,31); // Default color
                if (i == s_current_candidate_index) {
                    color = RGB15(31,0,0); // Highlight selected candidate (Red)
                }
                drawStringU16(10, candidate_y, mainScreenBuffer, candidate_u16, color); 
                candidate_y += 12; // Move to next line
            }
        }
    }

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
