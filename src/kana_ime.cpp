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
#define MAX_DISPLAY_CANDIDATES 5 // 画面に表示する最大候補数

// Define a larger buffer size for SKK candidate lists to prevent overflow
#define SKK_KOUHO_BUFFER_SIZE 1024

SKK skk_engine; // Global SKK engine instance

// Helper function to convert char* SJIS string to u16* array
// Assumes SJIS is 1 or 2 bytes per character.
static int char_sjis_to_u16_array(u16* dst, const char* src) {
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

// Helper function to convert u16* array (SJIS char codes) to char* SJIS byte string
static int u16_to_char_sjis_array(char* dst, const u16* src) {
    int src_pos = 0;
    int dst_pos = 0;
    while (src[src_pos] != 0) {
        u16 sjis_char_code = src[src_pos];
        if (sjis_char_code > 0xFF) { // 2-byte SJIS char
            dst[dst_pos++] = (char)(sjis_char_code >> 8);
            dst[dst_pos++] = (char)(sjis_char_code & 0xFF);
        } else { // 1-byte SJIS char (ASCII)
            dst[dst_pos++] = (char)sjis_char_code;
        }
        src_pos++;
    }
    dst[dst_pos] = '\0'; // Null terminate
    return dst_pos;
}

// Helper function to convert Hiragana SJIS byte string to Katakana SJIS byte string
static int hiragana_sjis_to_katakana_sjis(char* dst, const char* src) {
    int src_pos = 0;
    int dst_pos = 0;
    while (src[src_pos] != '\0') {
        unsigned char c1 = (unsigned char)src[src_pos];
        if ((c1 >= 0x82 && c1 <= 0x83)) { // Likely a 2-byte SJIS char (Hiragana range)
            unsigned char c2 = (unsigned char)src[src_pos + 1];
            u16 sjis_char_code = (u16)((c1 << 8) | c2);
            
            // Convert Hiragana SJIS to Katakana SJIS by adding 0x00A0 offset.
            // Hiragana range 0x829F - 0x82F1
            // Katakana range 0x8340 - 0x8391
            if (sjis_char_code >= 0x829f && sjis_char_code <= 0x82f1) {
                sjis_char_code += 0x00A0; // This is the common offset for hiragana to katakana
            }
            dst[dst_pos++] = (char)(sjis_char_code >> 8);
            dst[dst_pos++] = (char)(sjis_char_code & 0xFF);
            src_pos += 2;
        } else { // 1-byte SJIS char (ASCII) or other 2-byte chars
            dst[dst_pos++] = c1;
            src_pos += 1;
        }
    }
    dst[dst_pos] = '\0'; // Null terminate
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
    u16 display_buffer[128];
    char_sjis_to_u16_array(display_buffer, str);
    drawStringU16(x, y, buffer, display_buffer, color);
}

static u16* mainScreenBuffer = NULL;
static char input_romaji_buffer[32] = {0};
static int input_romaji_len = 0;
static u16 converted_kana_buffer[256] = {0};
static int converted_kana_len = 0;
static char current_kana_input_sjis_bytes[256] = {0}; // Buffer for fully converted Kana in SJIS bytes

static char s_kouho_list[SKK_KOUHO_BUFFER_SIZE] = {0}; // Stored candidate list from SKK
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

    if (!skk_engine.begin(NULL)) {
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

    if (keysDown() & KEY_TOUCH) {
        touchRead(&touch);
        if (touch.px > (256 - 56) && touch.py < 20) {
            switchMode();
        }
    } else if (keysDown() & KEY_SELECT) {
        switchMode();
    } else if (keysDown() & KEY_UP) { 
        if (s_num_candidates > 0) {
            s_current_candidate_index = (s_current_candidate_index + 1) % s_num_candidates;
        }
    } else if (keysDown() & KEY_DOWN) { 
        if (s_num_candidates > 0) {
            s_current_candidate_index = (s_current_candidate_index + s_num_candidates - 1) % s_num_candidates;
        }
    }

    // --- Character Processing ---
    char debug_str[256]; // Increased size for safety

    if (key > 0) {
        snprintf(debug_str, sizeof(debug_str), "Key: %d (%c)", key, (char)key);
        drawString(10, 100, mainScreenBuffer, debug_str, RGB15(31,31,31));

        if (key == '\b') { 
            if (input_romaji_len > 0) {
                input_romaji_len--;
                input_romaji_buffer[input_romaji_len] = '\0';
            } else if (s_final_output_len > 0) {
                s_final_output_len--;
                s_final_output_buffer[s_final_output_len] = 0;
            }
            s_current_candidate_index = 0;
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        } else if (key == '\n') { // Enter key: commit
            if (s_num_candidates > 0) { 
                char candidate_sjis_bytes[SKK_KOUHO_BUFFER_SIZE];
                if (skk_engine.get_kouho(candidate_sjis_bytes, s_kouho_list, s_current_candidate_index, sizeof(candidate_sjis_bytes))) {
                    u16 candidate_sjis_u16[256];
                    int len = char_sjis_to_u16_array(candidate_sjis_u16, candidate_sjis_bytes);
                    if (len > 0 && (s_final_output_len + len) < 255) {
                        memcpy(&s_final_output_buffer[s_final_output_len], candidate_sjis_u16, len * sizeof(u16));
                        s_final_output_len += len;
                        s_final_output_buffer[s_final_output_len] = 0;
                    }
                }
            } else if (converted_kana_len > 0) { 
                if ((s_final_output_len + converted_kana_len) < 255) {
                    memcpy(&s_final_output_buffer[s_final_output_len], converted_kana_buffer, converted_kana_len * sizeof(u16));
                    s_final_output_len += converted_kana_len;
                    s_final_output_buffer[s_final_output_len] = 0;
                }
            }
            input_romaji_len = 0;
            input_romaji_buffer[0] = '\0';
            s_current_candidate_index = 0;
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        } else if (key == ' ') { // Space key: advance candidate or commit space
            if (s_num_candidates > 0) { 
                s_current_candidate_index = (s_current_candidate_index + 1) % s_num_candidates;
            } else if (converted_kana_len > 0) { 
                if ((s_final_output_len + converted_kana_len) < 255) {
                    memcpy(&s_final_output_buffer[s_final_output_len], converted_kana_buffer, converted_kana_len * sizeof(u16));
                    s_final_output_len += converted_kana_len;
                }
                if (s_final_output_len < 255) {
                    s_final_output_buffer[s_final_output_len++] = (u16)' ';
                }
                s_final_output_buffer[s_final_output_len] = 0;
                input_romaji_len = 0;
                input_romaji_buffer[0] = '\0';
            }
            if (s_num_candidates == 0) { 
                s_current_candidate_index = 0;
                s_num_candidates = 0;
                s_kouho_list[0] = '\0';
                s_out_okuri[0] = '\0';
            }
        } else { 
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || key == '-' || key == '\'') { 
                if (input_romaji_len < 30) {
                    input_romaji_buffer[input_romaji_len++] = (char)key;
                    input_romaji_buffer[input_romaji_len] = '\0';
                }
            } else { 
                if(s_final_output_len < 255) {
                    s_final_output_buffer[s_final_output_len++] = (u16)key;
                }
                s_final_output_buffer[s_final_output_len] = 0;
            }
            s_current_candidate_index = 0;
            s_num_candidates = 0;
            s_kouho_list[0] = '\0';
            s_out_okuri[0] = '\0';
        }
    } else { // If key is 0 (no key pressed), ensure debug_str is cleared or not drawn
        // Optionally clear debug_str or draw empty string if no key is pressed
        // snprintf(debug_str, sizeof(debug_str), "");
        // drawString(10, 100, mainScreenBuffer, debug_str, RGB15(31,31,31));
    }

    // --- Conversion Logic ---
    converted_kana_len = 0;
    converted_kana_buffer[0] = 0;
    current_kana_input_sjis_bytes[0] = '\0';

    if (input_romaji_len > 0) {
        int current_romaji_pos = 0;
        int sjis_buffer_idx = 0;

        while (current_romaji_pos < input_romaji_len && sjis_buffer_idx < SKK_KOUHO_BUFFER_SIZE - 1) { // Use SKK_KOUHO_BUFFER_SIZE for safety
            bool handled = false;

            // Check for sokuon (っ) from doubled consonants
            if ((current_romaji_pos + 1) < input_romaji_len) {
                char c1 = input_romaji_buffer[current_romaji_pos];
                char c2 = input_romaji_buffer[current_romaji_pos + 1];
                if (c1 == c2 && c1 != 'n' && strchr("bcdfghjklmpqrstvwxyz", tolower(c1)) != NULL) {
                    current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; // 'っ'
                    current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xc1;
                    current_romaji_pos++; // Consume one of the doubled chars
                    continue; // Re-run loop for the second char
                }
            }

            // Check for 3-character compound kana first
            if (!handled && (current_romaji_pos + 3) <= input_romaji_len) {
                char r_three[4];
                strncpy(r_three, &input_romaji_buffer[current_romaji_pos], 3);
                r_three[3] = '\0';

                if (strcmp(r_three, "kya") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xab; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe1; handled = true; }
                else if (strcmp(r_three, "kyu") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xab; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe3; handled = true; }
                else if (strcmp(r_three, "kyo") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xab; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe5; handled = true; }
                else if (strcmp(r_three, "gya") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xac; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe1; handled = true; }
                else if (strcmp(r_three, "gyu") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xac; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe3; handled = true; }
                else if (strcmp(r_three, "gyo") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xac; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe5; handled = true; }
                else if (strcmp(r_three, "sha") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xb5; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe1; handled = true; }
                else if (strcmp(r_three, "shu") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xb5; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe3; handled = true; }
                else if (strcmp(r_three, "sho") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xb5; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe5; handled = true; }
                else if (strcmp(r_three, "cha") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xbf; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe1; handled = true; }
                else if (strcmp(r_three, "chu") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xbf; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe3; handled = true; }
                else if (strcmp(r_three, "cho") == 0) { current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xbf; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0x82; current_kana_input_sjis_bytes[sjis_buffer_idx++] = 0xe5; handled = true; }
                
                if(handled) current_romaji_pos += 3;
            }

            if (!handled) {
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
                        u16 hira_code = best_match_sjis;
                        if (hira_code >= 0x829f && hira_code <= 0x82f1) {
                            sjis_code += 0x00A0; // Correct offset for hiragana to katakana
                        }
                    }
                    
                    if (sjis_code > 0xFF) {
                        current_kana_input_sjis_bytes[sjis_buffer_idx++] = (char)(sjis_code >> 8);
                        current_kana_input_sjis_bytes[sjis_buffer_idx++] = (char)(sjis_code & 0xFF);
                    } else {
                        current_kana_input_sjis_bytes[sjis_buffer_idx++] = (char)sjis_code;
                    }
                    current_romaji_pos += best_match_len;
                } else { 
                    current_kana_input_sjis_bytes[sjis_buffer_idx++] = input_romaji_buffer[current_romaji_pos];
                    current_romaji_pos++;
                }
            }
        }
        current_kana_input_sjis_bytes[sjis_buffer_idx] = '\0';
    }

    // --- Debug Info for Conversion Logic ---
    snprintf(debug_str, sizeof(debug_str), "Romaji: %s", input_romaji_buffer);
    drawString(10, 110, mainScreenBuffer, debug_str, RGB15(31,31,31));
    snprintf(debug_str, sizeof(debug_str), "SJIS: %s", current_kana_input_sjis_bytes);
    drawString(10, 120, mainScreenBuffer, debug_str, RGB15(31,31,31));

    // Step 2: Use the converted kana string for SKK lookup or display.
    if (currentImeMode == IME_MODE_HIRAGANA || currentImeMode == IME_MODE_KATAKANA) {
        if (strlen(current_kana_input_sjis_bytes) > 0) {
            // Debug print before clearing s_kouho_list
            snprintf(debug_str, sizeof(debug_str), "SKK List (before clear): %s", s_kouho_list);
            drawString(10, 160, mainScreenBuffer, debug_str, RGB15(31,31,31));

            // Clear s_kouho_list before populating
            s_kouho_list[0] = '\0';
            s_num_candidates = 0;

            // Debug print after clearing s_kouho_list
            snprintf(debug_str, sizeof(debug_str), "SKK List (after clear): %s", s_kouho_list);
            drawString(10, 170, mainScreenBuffer, debug_str, RGB15(31,31,31));

            uint8_t skk_rc = skk_engine.get_kouho_list(s_kouho_list, s_out_okuri, current_kana_input_sjis_bytes);
            if (skk_rc > 0) {
                s_num_candidates = skk_engine.count_kouho_list(s_kouho_list);
            }

            // Debug print after skk_engine.get_kouho_list
            snprintf(debug_str, sizeof(debug_str), "SKK List (after get_kouho_list): %s", s_kouho_list);
            drawString(10, 180, mainScreenBuffer, debug_str, RGB15(31,31,31));

            // After SKK lookup, append original hiragana and katakana to candidates
            if (strlen(current_kana_input_sjis_bytes) > 0) {
                char katakana_sjis_bytes[SKK_KOUHO_BUFFER_SIZE];
                hiragana_sjis_to_katakana_sjis(katakana_sjis_bytes, current_kana_input_sjis_bytes);

                // Append original hiragana if not already present as the first candidate
                bool hiragana_is_first_skk_candidate = false;
                if (s_num_candidates > 0) {
                    char first_skk_candidate[SKK_KOUHO_BUFFER_SIZE];
                    if (skk_engine.get_kouho(first_skk_candidate, s_kouho_list, 0, sizeof(first_skk_candidate))) {
                        if (strcmp(first_skk_candidate, current_kana_input_sjis_bytes) == 0) {
                            hiragana_is_first_skk_candidate = true;
                        }
                    }
                }

                if (!hiragana_is_first_skk_candidate) {
                    size_t current_kouho_len = strlen(s_kouho_list);
                    if (current_kouho_len + strlen(current_kana_input_sjis_bytes) + 1 < sizeof(s_kouho_list)) { // +1 for separator
                        if (current_kouho_len > 0) { 
                            strncat(s_kouho_list, "/", sizeof(s_kouho_list) - current_kouho_len - 1);
                            current_kouho_len = strlen(s_kouho_list);
                        }
                        strncat(s_kouho_list, current_kana_input_sjis_bytes, sizeof(s_kouho_list) - current_kouho_len - 1);
                    }
                }

                // Append katakana if different from hiragana and not already present
                if (strlen(katakana_sjis_bytes) > 0 && strcmp(current_kana_input_sjis_bytes, katakana_sjis_bytes) != 0) {
                    // Check if katakana is already present in the list
                    bool katakana_is_present = false;
                    for (int i = 0; i < s_num_candidates; ++i) {
                        char candidate_check[SKK_KOUHO_BUFFER_SIZE];
                        if (skk_engine.get_kouho(candidate_check, s_kouho_list, i, sizeof(candidate_check))) {
                            if (strcmp(candidate_check, katakana_sjis_bytes) == 0) {
                                katakana_is_present = true;
                                break;
                            }
                        }
                    }

                    if (!katakana_is_present) {
                        size_t current_kouho_len = strlen(s_kouho_list);
                        if (current_kouho_len + strlen(katakana_sjis_bytes) + 1 < sizeof(s_kouho_list)) { // +1 for separator
                            if (current_kouho_len > 0) { 
                                strncat(s_kouho_list, "/", sizeof(s_kouho_list) - current_kouho_len - 1);
                                current_kouho_len = strlen(s_kouho_list);
                            }
                            strncat(s_kouho_list, katakana_sjis_bytes, sizeof(s_kouho_list) - current_kouho_len - 1);
                        }
                    }
                }
                // Recount candidates after appending
                s_num_candidates = skk_engine.count_kouho_list(s_kouho_list);
            }

            if (s_num_candidates > 0) { // SKK candidates found
                char candidate_sjis_bytes[SKK_KOUHO_BUFFER_SIZE];
                if (skk_engine.get_kouho(candidate_sjis_bytes, s_kouho_list, s_current_candidate_index, sizeof(candidate_sjis_bytes))) {
                    converted_kana_len = char_sjis_to_u16_array(converted_kana_buffer, candidate_sjis_bytes);
                }
            } else { // No SKK candidates, just show the converted kana
                converted_kana_len = char_sjis_to_u16_array(converted_kana_buffer, current_kana_input_sjis_bytes);
            }
        }
    } else if (currentImeMode == IME_MODE_ENGLISH) {
        if (input_romaji_len > 0) {
            converted_kana_len = char_sjis_to_u16_array(converted_kana_buffer, input_romaji_buffer);
        }
    }

    // --- Drawing ---
    dmaFillWords(0, mainScreenBuffer, 256 * 192 * 2);
    
    // Draw final committed output
    drawStringU16(10, 10, mainScreenBuffer, s_final_output_buffer, RGB15(31,31,31));

    int x = 10;
    if(s_final_output_len > 0) {
        char temp_final_sjis[SKK_KOUHO_BUFFER_SIZE]; // Use larger buffer for safety
        u16_to_char_sjis_array(temp_final_sjis, s_final_output_buffer);
        x = 10 + (strlen(temp_final_sjis) * 10); // Estimate width based on SJIS byte length
    }
    drawStringU16(x, 10, mainScreenBuffer, converted_kana_buffer, RGB15(31,31,31));

    // Draw SKK candidates (if any)
    if (s_num_candidates > 0) {
        char candidate_sjis_bytes[SKK_KOUHO_BUFFER_SIZE];
        int start_idx = s_current_candidate_index - (MAX_DISPLAY_CANDIDATES / 2);
        if (start_idx < 0) start_idx = 0;
        if (start_idx + MAX_DISPLAY_CANDIDATES > s_num_candidates) {
            start_idx = s_num_candidates - MAX_DISPLAY_CANDIDATES; // Adjust start_idx to show last MAX_DISPLAY_CANDIDATES candidates
            if (start_idx < 0) start_idx = 0; // Ensure start_idx is not negative
        }

        for (int i = 0; i < MAX_DISPLAY_CANDIDATES && (start_idx + i) < s_num_candidates; i++) {
            int candidate_to_display_idx = start_idx + i;
            if (skk_engine.get_kouho(candidate_sjis_bytes, s_kouho_list, candidate_to_display_idx, sizeof(candidate_sjis_bytes))) {
                u16 display_buffer[256];
                char_sjis_to_u16_array(display_buffer, candidate_sjis_bytes);

                u16 color = RGB15(31,31,31);
                if (candidate_to_display_idx == s_current_candidate_index) {
                    color = RGB15(0,31,0);
                }
                drawStringU16(10, 60 + (i * 10), mainScreenBuffer, display_buffer, color);
            }
        }
    }

    // --- Debug Info for SKK Candidates ---
    snprintf(debug_str, sizeof(debug_str), "SKK List: %s", s_kouho_list);
    drawString(10, 130, mainScreenBuffer, debug_str, RGB15(31,31,31));
    snprintf(debug_str, sizeof(debug_str), "SKK Num: %d, Idx: %d", s_num_candidates, s_current_candidate_index);
    drawString(10, 140, mainScreenBuffer, debug_str, RGB15(31,31,31));

    // --- Debug Info for Final Output ---
    char final_output_sjis_debug[SKK_KOUHO_BUFFER_SIZE]; // Use larger buffer for safety
    u16_to_char_sjis_array(final_output_sjis_debug, s_final_output_buffer);
    snprintf(debug_str, sizeof(debug_str), "Final: %s", final_output_sjis_debug);
    drawString(10, 150, mainScreenBuffer, debug_str, RGB15(31,31,31));

    return true; // Continue main loop
}

void kanaIME_showKeyboard(void) { keyboardShow(); }
void kanaIME_hideKeyboard(void) { keyboardHide(); }
char kanaIME_getChar(void) { return 0; }