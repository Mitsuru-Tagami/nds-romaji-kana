//
// マルチバイト文字列支援クラス
//
#ifndef __JSTRING_H__
#define __JSTRING_H__
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

class JString {
  public:
    static uint16_t bytes(const char* text);                                 // 文字列バイト数の取得
    static uint16_t len(const char* text);                                   // UTF8文字(1～4バイト)の文字数をカウントする
    static uint16_t get(char *dst, char *src);                               // 先頭からの1文字取得
    static uint32_t utf8to32(char* src);                                     // utf8 1文字をutf32に変換する
    static uint8_t  utf32to8(char* dst, uint32_t code);                      // utf32 1文字をutf8 1文字に変換する
    static uint16_t roma_to_kana(char* dst, char* src);                      // ローマ字かな変換
};
#endif