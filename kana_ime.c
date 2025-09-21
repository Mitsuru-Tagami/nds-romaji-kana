#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "kana_ime.h"
#include "draw_font.h"
#include "romakana_map.h"

#define DEBUG_MODE 1 // デバッグモード有効

// #ifdef ENABLE_DEBUG_LOG // Commented out
// void debug_log(const char* format, ...) {
//     va_list args;
//     va_start(args, format);
//     viprintf(format, args);
//     va_end(args);
// }
// #else
// void debug_log(const char* format, ...) { (void)format; }
// #endif // Commented out

// New function to convert ASCII numbers/symbols to full-width Shift-JIS
static u16 get_fullwidth_sjis(char ascii_char) {
    if (ascii_char >= '0' && ascii_char <= '9') {
        return 0x824f + (ascii_char - '0'); // Full-width '0' (0x824f) to '9' (0x8258)
    }
    // Add other full-width conversions here if needed (e.g., '!', '?', etc.)
    // For now, just return the original ASCII for other non-romaji characters
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

void kanaIME_init(void) {
    videoSetMode(MODE_FB0);
    vramSetBankA(VRAM_A_LCD);
    mainScreenBuffer = (u16*)VRAM_A;

    consoleDemoInit();
    keyboardDemoInit();
    keyboardShow();

    // #ifdef ENABLE_DEBUG_LOG // Commented out
    // iprintf("\x1b[2J");
    // debug_log("Kana IME Initialized.\n");
    // #endif // Commented out
}

void kanaIME_update(void) {
    int key = keyboardUpdate();

    if (key <= 0) {
        return;
    }

    #if DEBUG_MODE
    char dbg[128];
    sprintf(dbg, "[DBG] Key: %c", (key > 31 && key < 127) ? key : '?');
    drawString(10, 70, mainScreenBuffer, dbg, RGB15(31,15,15));
    sprintf(dbg, "[DBG] Before: Romaji='%s' (%d), KanaLen=%d", input_romaji_buffer, input_romaji_len, converted_kana_len);
    drawString(10, 80, mainScreenBuffer, dbg, RGB15(31,15,15));
    #endif

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
            converted_kana_buffer[converted_kana_len++] = 0x8140;
        }
    } else { // This is where regular key input is handled
        if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) { // It's a romaji character
            if (input_romaji_len < 30) {
                input_romaji_buffer[input_romaji_len++] = (char)key;
                input_romaji_buffer[input_romaji_len] = '\0';
            }
        } else { // It's a non-romaji character (number, symbol, etc.)
            // Add the non-romaji character directly to the converted buffer
            if(converted_kana_len < 255) {
                converted_kana_buffer[converted_kana_len++] = get_fullwidth_sjis((char)key);
            }
            // romajiバッファはクリアしない（数字や記号の後ろのローマ字も変換可能にする）
            #if DEBUG_MODE
            sprintf(dbg, "[DBG] Non-romaji key. Romaji buffer維持: '%s' (%d)", input_romaji_buffer, input_romaji_len);
            drawString(10, 90, mainScreenBuffer, dbg, RGB15(31,15,15));
            #endif
        }
    }

    #if DEBUG_MODE
    sprintf(dbg, "[DBG] Before conversion loop: Romaji='%s' (%d), KanaLen=%d", input_romaji_buffer, input_romaji_len, converted_kana_len);
    drawString(10, 100, mainScreenBuffer, dbg, RGB15(31,15,15));
    #endif
    bool converted_in_pass;
    do {
        converted_in_pass = false;
    #if DEBUG_MODE
    sprintf(dbg, "[DBG] Inside do-while. Romaji='%s' (%d)", input_romaji_buffer, input_romaji_len);
    drawString(10, 110, mainScreenBuffer, dbg, RGB15(31,15,15));
    #endif

        if (input_romaji_len >= 2 && input_romaji_buffer[0] == input_romaji_buffer[1] && input_romaji_buffer[0] != 'n') {
            char c = input_romaji_buffer[0];
            if (strchr("kstnhmyrwgzdbpv", c) != NULL) {
                 if(converted_kana_len < 255) {
                    converted_kana_buffer[converted_kana_len++] = 0x82c1; // っ
                }
                memmove(input_romaji_buffer, input_romaji_buffer + 1, input_romaji_len);
                input_romaji_len--;
                #if DEBUG_MODE
                sprintf(dbg, "[DBG] After sokuon memmove. Romaji='%s' (%d)", input_romaji_buffer, input_romaji_len);
                drawString(10, 120, mainScreenBuffer, dbg, RGB15(31,15,15));
                #endif
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
                #if DEBUG_MODE
                sprintf(dbg, "[MATCH] '%s' → 0x%x", romaji, romakana_map[i].sjis_code);
                drawString(10, 160 + 16 * i, mainScreenBuffer, dbg, RGB15(0,31,0));
                sprintf(dbg, "[DBG] Converted '%s' to 0x%x. Remaining Romaji='%s' (%d), KanaLen=%d", romaji, romakana_map[i].sjis_code, input_romaji_buffer, input_romaji_len, converted_kana_len);
                drawString(10, 130, mainScreenBuffer, dbg, RGB15(31,15,15));
                #endif
                
                memmove(input_romaji_buffer, input_romaji_buffer + len, input_romaji_len - len + 1);
                input_romaji_len -= len;
                #if DEBUG_MODE
                sprintf(dbg, "[DBG] After romaji memmove. Romaji='%s' (%d)", input_romaji_buffer, input_romaji_len);
                drawString(10, 140, mainScreenBuffer, dbg, RGB15(31,15,15));
                #endif
                converted_in_pass = true;
                break;
            }
        }
    } while (converted_in_pass);

    #if DEBUG_MODE
    sprintf(dbg, "[DBG] After conversion loop: Romaji='%s' (%d), KanaLen=%d", input_romaji_buffer, input_romaji_len, converted_kana_len);
    drawString(10, 150, mainScreenBuffer, dbg, RGB15(31,15,15));
    #endif

    dmaFillWords(0, mainScreenBuffer, 256 * 192 * 2);
    int x = 10;
    for (int i = 0; i < converted_kana_len; i++) {
        x += drawFont(x, 10, mainScreenBuffer, converted_kana_buffer[i], RGB15(31,31,31));
    }
    for (int i = 0; i < input_romaji_len; i++) {
        x += drawFont(x, 10, mainScreenBuffer, (u16)input_romaji_buffer[i], RGB15(31,31,31));
    }

    // On-screen debug display
    static char last_match_log[64] = "";
    char debug_str[64];
    sprintf(debug_str, "Key: %c", (key > 31 && key < 127) ? key : '?');
    drawString(10, 30, mainScreenBuffer, debug_str, RGB15(31,31,31));

    sprintf(debug_str, "Romaji: %s (%d)", input_romaji_buffer, input_romaji_len);
    drawString(10, 40, mainScreenBuffer, debug_str, RGB15(31,31,31));

    sprintf(debug_str, "KanaLen: %d", converted_kana_len);
    drawString(10, 50, mainScreenBuffer, debug_str, RGB15(31,31,31));

    // 4行目（y=60）に直近の変換マッチログを表示
    drawString(10, 60, mainScreenBuffer, last_match_log, RGB15(0,31,0));
}

void kanaIME_showKeyboard(void) { keyboardShow(); }
void kanaIME_hideKeyboard(void) { keyboardHide(); }
char kanaIME_getChar(void) { return 0; }
