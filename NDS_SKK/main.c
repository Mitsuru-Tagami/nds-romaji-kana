#include <nds.h>
#include <stdio.h>
#include "kana_ime.h"

int main(void) {
    kanaIME_init();
    kanaIME_showKeyboard();

    // kanaIME_update()がfalseを返すまでループする
    while(kanaIME_update()) {
        swiWaitForVBlank();
    }

    return 0;
}
