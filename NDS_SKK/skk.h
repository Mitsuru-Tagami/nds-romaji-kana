//
// SKK漢字入力支援クラスライブラリ ヘッダーファイル skk.h
//
#ifndef __SKK_H__
#define __SKK_H__
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <fat.h> // For NDS file I/O

#define SSK_BINDIC_FILE 	  "ssk_dic_m.bin"
#define SSK_BIN_HEAD_SIZE 	12

class SKK {
 private:
  FILE*  fp_skk;                       // 辞書ファイルポインタ
  uint32_t size_keyword;              // 辞書登録単語数
  uint32_t keyword_index_top;         // キーワードインデックス先頭位置
  uint32_t keyword_data_top;          // キーワードデータ先頭位置
  uint32_t max_data_len = 0;          // キーワードデータ最大バイト数
  uint32_t max_data_len_index = 0;    // 最大キーワードデータのインデックス

 public:
  uint32_t  begin(const char* param_path);   // SKK辞書利用開始
  uint8_t   end();                                                         // SKK辞書利用終了

 private:   
  uint32_t  load_skk_header();                                             // SKK辞書ヘッダー情報の取得(内部処理用)
  uint8_t   get_keyword(const char* keyword, uint32_t index);              // 指定位置のキーワードの取得(内部処理用)
  uint8_t   get_keywordData(const char* data , uint32_t index);            // 指定位置のキーワード+候補リストの取得(内部処理用)
  int32_t   binfind(const char* key, uint32_t n);                          // SKK辞書検索((内部処理用)
  void      word2lower(char* token);                                       // 英字小文字変換((内部処理用)
  void      splitOkuri(char* keyword, char* okuri, char* token);           // 入力をキーワードと送りに分離(内部処理用)

 public:
  uint8_t   get_kouho_list(char* kouho_list, char* out_okuri, char* in_token);               // 入力文字で辞書検索
  uint8_t   get_kouho_list_index(uint32_t* kouho_list_index, char* out_okuri, char* token);  // 入力文字で辞書検索(該当候補のindexを返す)
  uint16_t  count_kouho_list(const char* kouho_list);                                        // 候補リスト内の単語数のカウント
  uint16_t  count_kouho_list_by_index(uint32_t key_index);                                   // 直接辞書ファイルから候補リスト内の単語数のカウント
  uint8_t   get_kouho(const char* kouho, const char* kouho_list, uint16_t list_index);       // 候補リスト内の指定位置の単語の取得
  uint8_t   get_kouho_by_index(const char* kouho, uint16_t list_ndex, uint16_t key_index);   // 直接辞書ファイルから候補リスト内の指定位置の単語の取得
  uint16_t  kana_to_katakana(const char* dst, const char* src);                              // かな⇒カタカナ変換
  void      han_to_zen(const char* dst, const char* src);                                    // 半角⇒全角変換
  uint16_t  roma_to_kana(char* dst, char* src);                                              // ローマ字かな変換
};

#endif