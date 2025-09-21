#include <nds.h>
#include <stdio.h>
#include "kana_ime.h" // 新しく追加

int main(void) {
    // メインスクリーンの初期化はkanaIME_initで行うので、ここでは不要
    // videoSetMode(MODE_5_2D);
    // vramSetBankA(VRAM_A_MAIN_BG);
    // bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    // u16* mainScreenBuffer = (u16*)BG_BMP_RAM(3);

    kanaIME_init(); // IMEの初期化を呼び出す (メインスクリーンを設定する)

    // キーボードをすぐに表示してみる（テスト用）
    kanaIME_showKeyboard();

    while(1) {
        scanKeys();
        int pressed = keysDown();

        if(pressed & KEY_START) break;

        kanaIME_update(); // IMEの更新処理を呼び出す
        swiWaitForVBlank();
    }

    return 0;
}