#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include "kana_ime.h"
#include "draw_font.h"
#include "romakana_map.h"
#include "skk.h"
#include "JString.h"

#define DEBUG_MODE 1 // デバッグモード有効

SKK skk_engine; // Global SKK engine instance

// Helper function to convert SJIS char* string to u16* array
// Assumes SJIS is 1 or 2 bytes per character.
static int sjis_to_u16_array(u16* dst, const char* src) {
    int src_pos = 0;
    int dst_pos = 0;
    while (src[src_pos] != '\0') {
        unsigned char c1 = (unsigned char)src[src_pos];
        u16 sjis_char_code;
        if ((c1 >= 0x81 && c1 <= 0x9F) || (c1 >= 0xE0 && c1 <= 0xFC)) { // First byte of a 2-byte SJIS char
            unsigned char c2 = (unsigned char)src[src_pos + 1];
            sjis_char_code = (u16)((c1 << 8) | c2);
            src_pos += 2;
        } else { // 1-byte SJIS char (ASCII)
            sjis_char_code = (u16)c1;
            src_pos += 1;
        }
        dst[dst_pos++] = sjis_char_code;
    }
    dst[dst_pos] = 0; // Null terminate
    return dst_pos;
}

// Helper function to draw a string from a u16 array
static void drawStringU16(int x, int y, u16* buffer, const u16* str_u16, u16 color) {
    int current_x = x;
    for (int i = 0; str_u16[i] != 0; i++) {
        current_x += drawFont(current_x, y, buffer, str_u16[i], color);
    }
}

// Helper function to draw a string
static void drawString(int x, int y, u16* buffer, const char* str, u16 color) {
    // This function is buggy for non-alphanumeric chars, but works for now.
    // It should ideally use sjis_to_u16_array and drawStringU16.
    u16 display_buffer[128];
    sjis_to_u16_array(display_buffer, str);
    drawStringU16(x, y, buffer, display_buffer, color);
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
    currentImeMode = (ImeMode)((currentImeMode + 1) % 4);
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

    // Initialize SKK engine (assuming embedded dictionary is ready)
    // The path is ignored for embedded dict, but skk_engine.begin still needs to be called
    if (!skk_engine.begin(NULL)) { // Pass NULL as path is not needed for embedded dict
        // Handle error if SKK engine fails to initialize
        // For now, just loop indefinitely
        iprintf("SKK Init Failed!\n");
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

    // Mode switching and candidate cycling
    if (keysDown() & KEY_TOUCH) {
        touchRead(&touch);
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
            } else if (s_final_output_len > 0) {
                s_final_output_len--;
                s_final_output_buffer[s_final_output_len] = 0;
            }
            // Reset SKK candidates on backspace
            s_current_candidate_index = 0;
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        } else if (key == '\n') { // Enter key: commit
            if (s_num_candidates > 0) { // If SKK candidates exist, commit the selected one
                char candidate_sjis_bytes[256]; // SKK now returns SJIS bytes
                if (skk_engine.get_kouho(candidate_sjis_bytes, s_kouho_list, s_current_candidate_index)) {
                    u16 candidate_sjis_u16[256];
                    int len = sjis_to_u16_array(candidate_sjis_u16, candidate_sjis_bytes);
                    if (len > 0 && (s_final_output_len + len) < 255) {
                        memcpy(&s_final_output_buffer[s_final_output_len], candidate_sjis_u16, len * sizeof(u16));
                        s_final_output_len += len;
                        s_final_output_buffer[s_final_output_len] = 0;
                    }
                }
            } else if (converted_kana_len > 0) { // If no SKK candidates, commit romakana conversion
                if ((s_final_output_len + converted_kana_len) < 255) {
                    memcpy(&s_final_output_buffer[s_final_output_len], converted_kana_buffer, converted_kana_len * sizeof(u16));
                    s_final_output_len += converted_kana_len;
                    s_final_output_buffer[s_final_output_len] = 0;
                }
            }
            // Reset input and candidates after commit
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';
            s_current_candidate_index = 0;
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        } else if (key == ' ') { // Space key: advance candidate or commit space
            if (s_num_candidates > 0) { // If SKK candidates exist, advance to next
                s_current_candidate_index = (s_current_candidate_index + 1) % s_num_candidates;
            } else if (converted_kana_len > 0) { // If no SKK candidates, commit romakana conversion
                if ((s_final_output_len + converted_kana_len) < 255) {
                    memcpy(&s_final_output_buffer[s_final_output_len], converted_kana_buffer, converted_kana_len * sizeof(u16));
                    s_final_output_len += converted_kana_len;
                }
                if (s_final_output_len < 255) {
                    s_final_output_buffer[s_final_output_len++] = (u16)' '; // Half-width space
                }
                s_final_output_buffer[s_final_output_len] = 0;
                input_romaji_len = 0;
                input_romaji_buffer[0] = '\0';
            }
            // Reset candidates after space (unless advancing candidate)
            if (s_num_candidates == 0) { // Only reset if not advancing candidate
                s_current_candidate_index = 0;
                s_num_candidates = 0;
                s_kouho_list[0] = '\0';
                s_out_okuri[0] = '\0';
            }
        } else { 
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || key == '-' || key == '\'') { // Corrected: escaped single quote
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
            // Reset SKK candidates on new input
            s_current_candidate_index = 0;
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        }
    }

    // --- Conversion Logic ---
    converted_kana_len = 0;
    converted_kana_buffer[0] = 0;

    if (currentImeMode == IME_MODE_HIRAGANA || currentImeMode == IME_MODE_KATAKANA) {
        if (input_romaji_len > 0) {
            // Perform SKK lookup only if input_romaji_buffer has changed or candidates are not yet loaded
            // and if there are no candidates currently displayed (e.g. from previous input)
            if (s_num_candidates == 0) { 
                uint8_t skk_rc = skk_engine.get_kouho_list(s_kouho_list, s_out_okuri, input_romaji_buffer);
                if (skk_rc > 0) {
                    s_num_candidates = skk_engine.count_kouho_list(s_kouho_list);
                } else {
                    s_num_candidates = 0;
                }
            }

            if (s_num_candidates > 0) { // If SKK candidates are loaded, display the selected one
                char candidate_sjis_bytes[256];
                if (skk_engine.get_kouho(candidate_sjis_bytes, s_kouho_list, s_current_candidate_index)) {
                    sjis_to_u16_array(converted_kana_buffer, candidate_sjis_bytes);
                    // Recalculate converted_kana_len based on the actual number of u16 chars
                    int count = 0;
                    for(int i=0; converted_kana_buffer[i] != 0; i++) count++;
                    converted_kana_len = count;
                }
            } else { // No SKK candidates, fall back to romakana_map conversion
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
                    } else { // If no match, display raw romaji char
                        converted_kana_buffer[buffer_idx++] = (u16)input_romaji_buffer[current_romaji_pos];
                        current_romaji_pos++;
                    }
                }
                converted_kana_len = buffer_idx;
                converted_kana_buffer[converted_kana_len] = 0;
            }
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

    // Draw SKK candidates (if any)
    if (s_num_candidates > 0) {
        char candidate_sjis_bytes[256];
        for (int i = 0; i < s_num_candidates; i++) {
            if (skk_engine.get_kouho(candidate_sjis_bytes, s_kouho_list, i)) {
                u16 display_buffer[256]; // Temporary buffer for display
                sjis_to_u16_array(display_buffer, candidate_sjis_bytes);

                u16 color = RGB15(31,31,31);
                if (i == s_current_candidate_index) {
                    color = RGB15(0,31,0); // Highlight selected candidate
                }
                drawStringU16(10, 60 + (i * 10), mainScreenBuffer, display_buffer, color);
            }
        }
    }

    // Draw debug info
    char debug_str[128];
    const char* mode_prompt = "";
    switch (currentImeMode) {
        case IME_MODE_HIRAGANA: mode_prompt = "HIRAGANA: "; break;
        case IME_MODE_KATAKANA: mode_prompt = "KATAKANA: "; break;
        case IME_MODE_ENGLISH:  mode_prompt = "ENGLISH:  ";  break;
        case IME_MODE_DEBUG:    mode_prompt = "DEBUG:    ";    break;
    }

    sprintf(debug_str, "%sRomaji: %s", mode_prompt, input_romaji_buffer);
    drawString(10, 30, mainScreenBuffer, debug_str, RGB15(31,31,31));

    sprintf(debug_str, "SKK List: %s", s_kouho_list);
    drawString(10, 40, mainScreenBuffer, debug_str, RGB15(31,31,31));

    sprintf(debug_str, "SKK Num: %d, Idx: %d", s_num_candidates, s_current_candidate_index);
    drawString(10, 50, mainScreenBuffer, debug_str, RGB15(31,31,31));

    return true; // Continue main loop
}

void kanaIME_showKeyboard(void) { keyboardShow(); }
void kanaIME_hideKeyboard(void) { keyboardHide(); }
char kanaIME_getChar(void) { return 0; }