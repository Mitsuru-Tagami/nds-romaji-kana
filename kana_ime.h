#ifndef KANA_IME_H
#define KANA_IME_H

#ifdef __cplusplus
extern "C" {
#endif

// IMEの初期化関数
void kanaIME_init(void);

// IMEの更新関数（キーボード表示、入力処理など）
void kanaIME_update(void);

// キーボードを表示する関数
void kanaIME_showKeyboard(void);

// キーボードを非表示にする関数
void kanaIME_hideKeyboard(void);

// 入力された文字を取得する関数（仮）
char kanaIME_getChar(void);

#ifdef __cplusplus
}
#endif

#endif // KANA_IME_H