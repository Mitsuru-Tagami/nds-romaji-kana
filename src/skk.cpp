//
// SKK漢字入力支援クラスライブラリ  skk.cpp 
//

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <fat.h> // For NDS file I/O

#include "JString.h"
#include "skk.h"
#include "test_skk_dict_data.h" // Include embedded dictionary data

// #define SSK_BINDIC_FILE 	"ssk_mdic.bin" // Defined in skk.h
#define SSK_BIN_HEAD_SIZE 12

// skkの開始
//  引数 param_path: 辞書ファイルの格納ディレクトリ (ignored for embedded dict)
//
uint32_t SKK::begin(const char* param_path) {
	// char path[64] = ""; // Not needed for embedded dict
	uint32_t rc; // Declared here

	// For embedded dictionary, we don't need to open a file
	// We just set the internal pointer to the start of the array
	fp_skk_data = (const unsigned char*)embedded_skk_dict;

	rc = load_skk_header();
  //Serial.println("ok");
	return rc;
}

// skkの終了
uint8_t SKK::end() {
	// No file to close for embedded dict
	return 0;
}

// skk辞書ファイルのヘッダー読み込み
uint32_t SKK::load_skk_header() {
	// ヘッダー情報の格納
	// Read from embedded_skk_dict array
	memcpy(&size_keyword, fp_skk_data + 0, 4);
	memcpy(&keyword_index_top, fp_skk_data + 4, 4);
	memcpy(&keyword_data_top, fp_skk_data + 8, 4);
	return size_keyword;
}

// 指定キーワードインデックスのキーワードの取得
// 引数
//  keyowrd: キーワード格納先
//  inedx:   インデックステーブル参照位置
// 戻り値
//  1:キーワードが取得出来た 0:キーワードが取得出来ない
// 
uint8_t SKK::get_keyword(const char* keyword, uint32_t index) {
	uint32_t pos; // キーワード格納位置
	int rc;
	uint8_t c;

	// Read from embedded_skk_dict array
	memcpy(&pos, fp_skk_data + SSK_BIN_HEAD_SIZE + index*4, 4);

	// キーワードの取得
	const unsigned char* current_ptr = fp_skk_data + pos + keyword_data_top;

	int i = 0;
	for(;;) {
		c = *current_ptr++;
		if (c == ',') {
			((char *)keyword)[i] = '\0';
			break;
		}
		((char *)keyword)[i] = c;
		i++;
	}
	return 1;
}

// 指定キーワードインデックスのキーワード＋候補リストの取得
//  引数
//   data:   変換候補リスト(カンマ区切り)の格納先
//   index:  キーワードのインデックス番号
//  戻り値
//   正常終了:1
//   異常終了:0
//
uint8_t SKK::get_keywordData(const char* data , uint32_t index) {
	uint32_t pos, pos_next; // キーワード格納位置
	uint32_t size;
	int rc;
	// uint8_t c; // Not used
	if (index >= size_keyword)
		return 0;
	
	// Read from embedded_skk_dict array
	memcpy(&pos, fp_skk_data + SSK_BIN_HEAD_SIZE + index*4, 4);
	if (index != size_keyword-1) {
		memcpy(&pos_next, fp_skk_data + SSK_BIN_HEAD_SIZE + (index+1)*4, 4);
		size = pos_next - pos;
	} else {
		size = 30; // 最終データの場合、仮サイズを指定 (This might need adjustment based on actual last entry size)
	}

	memcpy((void *)data, fp_skk_data + pos + keyword_data_top, size);
	if (size > 0)
		((char *)data)[size] = '\0'; // Ensure null termination
	if (size > max_data_len) {
		max_data_len = size;
		max_data_len_index = index;
	}
	return 1;
}

// 論理リスト内２分検索
int32_t SKK::binfind(const char* key, uint32_t n) {
	int32_t t_p = 0;                   // 検索範囲上限
	int32_t e_p = n-1;                 // 検索範囲下限
	uint8_t flg_stop = 0;
	int32_t pos;
	char d[32] = "";
	int rc;

	for(;;) {
		pos = t_p + ((e_p - t_p+1)>>1);
		if (!get_keyword(d, pos)) {
			return -1;
		}
		rc = strcmp(key, d);
		if (rc == 0) {        // 等しい
			flg_stop = 1;  
			break;
		} else if (rc > 0) {  // 大きい
			t_p = pos + 1;   
			if (t_p > e_p)
				break;
		} else {              // 小さい
			e_p = pos -1;
			if (e_p < t_p)
				break;
		}
	}
	if (!flg_stop)
		return -1;
	return pos;
}

// 英字小文字変換(内部処理用)
void SKK::word2lower(char* token) {
	uint16_t len = strlen(token);
	for (uint16_t i=0; i < len; i++) {
		if (isupper(token[i])) {
			token[i] = tolower(token[i]);
		}
	}
}

// 送り指示ありローマ字をキーワードと送りに分ける(内部処理用)
//  例:OkuRu → 'oku', 'ru'
//   引数 
//    keyword(out) : キーワード部の格納領域
//    okuri(out)   : 送り部の格納領域
//    token(in)    : ローマ字文字列
//   戻り値
//    なし
void SKK::splitOkuri(char* keyword, char* okuri, char* token) {
	((char*)keyword)[0] = '\0';
	((char*)okuri)[0] = '\0';

	// 0文字の場合は、0文字を返す
	if (strlen(token) == 0) 
		return;
	
	// 送りモードの場合、送り文字を調べる
	if (isupper(token[0])) {
		for(uint16_t i=1; i< strlen(token); i++) {            
			if (isupper(token[i])) {
				strcpy((char*)okuri, &token[i]);
				strncpy((char*)keyword, token, i);
				((char*)keyword)[i] = '\0';
				break;
			}
		}
		// 送り文字がない場合、送りモードでないとする
		if (strlen(keyword) == 0) {
			strcpy((char*)keyword, token);
			((char*)okuri)[0] = '\0';
		}
	} else {
		strcpy((char*)keyword, token);
	}
	word2lower(keyword);
	word2lower(okuri);
	return;
}

// 日本語辞書変換(送り対応)
//  引数
//    out_kouho
//    out_okuri
//    in_token
//  戻り値
//    0:候補なし 1:候補あり(送りあり) 2:候補あり(送りなし) 3:候補あり(英単語)
//
uint8_t SKK::get_kouho_list(char* kouho_list, char* out_okuri, char* in_token) {
	char keyword[32];
	char okuri[32];
	char key[64];
	uint16_t key_len;
	int32_t pos;
	uint8_t rc;

	// 送り処理
	splitOkuri(keyword, okuri, in_token);
	if (strlen(okuri)) {
		// 送りがある場合
		
		// key にキーワードをセット
		JString::roma_to_kana(key, keyword);
		key_len = strlen(key);
		key[key_len] = okuri[0];
		key[key_len+1] = '\0';
		
		// 候補の位置を検索
		pos = binfind(key, size_keyword); 
		if (pos > 0) {
			// 該当データあり
			rc = get_keywordData(kouho_list, pos);     // 候補データの取得
			JString::roma_to_kana(out_okuri, okuri);   // 送りローマ字を「ひらがな」に変換
			return 1;
		} else {
			((char*)kouho_list)[0] = '\0';
			((char*)out_okuri)[0] = '\0';
			return 0;
		}
	} else {
        // 送りなし
		JString::roma_to_kana(key, keyword);
		pos = binfind(key, size_keyword);
		if (pos > 0) {
			rc = get_keywordData(kouho_list, pos);
			((char*)out_okuri)[0] = '\0';
			return 2;
		} else {
			// 候補がない場合、英単語として検索を試みる
			pos = binfind(in_token, size_keyword);
			if (pos > 0) {
				rc = get_keywordData(kouho_list, pos);
				((char*)out_okuri)[0] = '\0';
				return 3;         
			}
			((char*)kouho_list)[0] = '\0';
			((char*)out_okuri)[0] = '\0';
			return 0;
		}
	}
}

// 入力文字で辞書検索(該当候補のindexを返す)
//  引数
//    out_kouho_index: 候補リストの格納位置インデックス
//    out_okuri:       送り
//    in_token:        検索トークン
//  戻り値
//    0:候補なし 1:候補あり(送りあり) 2:候補あり(送りなし) 3:候補あり(英単語)
//
uint8_t SKK::get_kouho_list_index(uint32_t* out_kouho_index, char* out_okuri, char* in_token) {
	char keyword[32];
	char okuri[32];
	char key[64];
	uint16_t key_len;
	int32_t pos;
	uint8_t rc;

	// 送り処理
	splitOkuri(keyword, okuri, in_token);
	if (strlen(okuri)) {
		// 送りがある場合
		
		// key にキーワードをセット
		JString::roma_to_kana(key, keyword);
		key_len = strlen(key);
		key[key_len] = okuri[0];
		key[key_len+1] = '\0';
		
		// 候補の位置を検索
		pos = binfind(key, size_keyword); 
		if (pos > 0) {
			// 該当データあり
			*(uint32_t*)out_kouho_index = pos;
			JString::roma_to_kana(out_okuri, okuri);    // 送りローマ字を「ひらがな」に変換
			return 1;
		} else {
			((char*)out_okuri)[0] = '\0';
			return 0;
		}
	} else {
        // 送りなし
		JString::roma_to_kana(key, keyword);
		pos = binfind(key, size_keyword);
		if (pos > 0) {
			*(uint32_t*)out_kouho_index = pos;
			((char*)out_okuri)[0] = '\0';
			return 2;
		} else {
			// 候補がない場合、英単語として検索を試みる
			pos = binfind(in_token, size_keyword);
			if (pos > 0) {
				*(uint32_t*)out_kouho_index = pos;
				((char*)out_okuri)[0] = '\0';
				return 3;         
			}
			((char*)out_okuri)[0] = '\0';
			return 0;
		}
	}
}

//
// 候補リストの候補数のカウント
//  引数
//   kouho_list: カンマ区切りの候補リスト文字列
//  戻り値
//   データ数
//
uint16_t SKK::count_kouho_list(const char* kouho_list) {
	char *ptr = (char*)kouho_list;
	uint16_t cnt=0;
	for(cnt=0; *ptr!='\0'; ptr++) {
		if (*ptr == ',') cnt++;
	}
	return cnt;
}

// 直接辞書ファイルから候補リスト内の候補数のカウント

uint16_t  SKK::count_kouho_list_by_index(uint32_t key_index) {
	uint16_t cnt=0;
	uint32_t pos, pos_next; // キーワード格納位置
	uint32_t size;
	// int rc; // Not used
	// char c; // Not used
	
	if (key_index >= size_keyword)
		return 0;
	
	// インデックスの格納の位置の取得	
	memcpy(&pos, fp_skk_data + SSK_BIN_HEAD_SIZE + key_index*4, 4);
	if (key_index != size_keyword-1) {
		memcpy(&pos_next, fp_skk_data + SSK_BIN_HEAD_SIZE + (key_index+1)*4, 4);
		size = pos_next - pos;
	} else {
		size = 30; // 最終データの場合、仮サイズを指定
	}

	const unsigned char* current_ptr = fp_skk_data + pos + keyword_data_top;

	// ','の個数を数える
    for (uint16_t i = 0; i<size; i++) {
		if (*current_ptr == ',')
			cnt++;
		current_ptr++;
	}
	if (cnt>1)
		// 先頭のキーの分を引く
		cnt--;
	return cnt;
}

//
// 候補データからデータの取得
//  引数
//   kouho:       候補(単語)の格納先(out)
//   kouho_list:  候補リスト(in)
//   list_index:  候補データ内データ位置(in)
//  戻り値
//   0:データなし 1:データあり    
//
uint8_t SKK::get_kouho(const char* kouho, const char* kouho_list, uint16_t list_index) {
	char *src = (char*)kouho_list;
	uint16_t top = 0;
	uint16_t pos;
	uint16_t n = 0;

	uint16_t cnt = count_kouho_list(kouho_list);   // データ件数の取得
	if (cnt == 0) {
		// 候補がない場合
		((char *)kouho)[0] = '\0';
		return 0;
	}

	// 指定位置の候補データの取得
	while(true) {
		if (*src == ',') {
			if (n == list_index) {
				src++;
				break;
			}
			n++;
		} else if(*src == 0) {
			// 候補がない場合
			((char *)kouho)[0] = '\0';
			return 0;
		}
		src++;
	}

	// データと取り出し
	char* dst = (char*)kouho;
	while(true) {
		*dst++ = *src++;
		if (*src == ',' || *src == '\0') {
			*dst = '\0';
			break;
		}
	}
	return 1;
}

// 直接辞書ファイルから候補データ内の指定位置の単語の取得
//  引数
//    kouho:       候補(単語)の格納先(out)
//    list_index:  候補データ内データ位置(in)
//    key_index:   候補データ位置
//  戻り値
//   0:データなし 1:データあり
//
uint8_t SKK::get_kouho_by_index(const char* kouho, uint16_t list_index, uint16_t key_index) {
	uint16_t cnt=0;
	uint32_t pos, pos_next; // キーワード格納位置
	uint32_t size;
	// int rc; // Not used
	// char c; // Not used
	uint8_t flg_found = 0;
	char* ptr_kouho = (char*)kouho;

	if (key_index >= size_keyword)
		return 0;
	
	// インデックスの格納の位置の取得	
	memcpy(&pos, fp_skk_data + SSK_BIN_HEAD_SIZE + key_index*4, 4);
	if (list_index != size_keyword-1) {
		memcpy(&pos_next, fp_skk_data + SSK_BIN_HEAD_SIZE + (key_index+1)*4, 4);
		size = pos_next - pos;
	} else {
		size = 30; // 最終データの場合、仮サイズを指定
	}

	const unsigned char* current_ptr = fp_skk_data + pos + keyword_data_top;

	// ','の個数を数える
    for (uint16_t i = 0; i<size; i++) {
		if (cnt == list_index+1) {
			flg_found = 1; // 該当キー位置に到達した
		}
		char c = *current_ptr++;
		
		if (c == ',') {
			if (flg_found) break;  // 該当位置から次の位置に到達した
			cnt++;
		}

		if (flg_found) {
			*ptr_kouho = c;
			ptr_kouho++;	
		}
	}
	*ptr_kouho = '\0';
	return flg_found;
}

//
// ひらがな=>片仮名変換
// 引数
//  dst:変換したカタカナ格納先
//  src:変換対象文字列格納先
// 戻り値
//  カタカナに変換した文字数
//
uint16_t SKK::kana_to_katakana(const char* dst, const char* src) {
	uint16_t len=JString::len(src); // utf-8文字数の取得
	uint32_t code;                  // utf32コード
	char utf8char[5];               // utf-8 1文字分の一時バッファ
	uint16_t src_nbytes;            // src utf-8 1文字分のバイト数
	uint16_t dst_nbytes;            // dst utf-8 1文字分のバイト数
	char* src_ptr = (char*)src;     // 変換元文字列参照位置
	char* dst_ptr = (char*)dst;     // 変換先文字列格納位置
	uint16_t rc = 0;

	for (uint16_t i = 0; i < len; i++) {
		src_nbytes = JString::get(utf8char, src_ptr);      // 1文字取り出し
		src_ptr += src_nbytes;
		code = JString::utf8to32(utf8char);                // utf-8からutf32に変換
		if (code >= 0x3041 && code <= 0x3093) {
				code = code + 96;                              // カタカナコードに変換
				rc++;
		}
		// utf32からutf8に変換
		dst_nbytes = JString::utf32to8(dst_ptr, code);
		dst_ptr += dst_nbytes;
	}
	return rc;
}

// ローマ字ひらがな変換
//  引数
//   dst: 変換後のひらがな文字列
//   src: 変換対象ローマ字文字列
//  戻り値
//   ローマ字からひらがなに変換した文字数
//
uint16_t SKK::roma_to_kana(char* dst, char* src) {
	return JString::roma_to_kana(dst, src);
}

// 半角⇒全角変換
void SKK::han_to_zen(const char* dst, const char* src) {
	uint16_t len=JString::len(src); // utf-8文字数の取得
	uint32_t code;                  // utf32コード
	char utf8char[5];               // utf-8 1文字分の一時バッファ
	uint16_t src_nbytes;            // src utf-8 1文字分のバイト数
	uint16_t dst_nbytes;            // dst utf-8 1文字分のバイト数
	char* src_ptr = (char*)src;     // 変換元文字列参照位置
	char* dst_ptr = (char*)dst;     // 変換先文字列格納位置

	for (uint16_t i = 0; i < len; i++) {
		src_nbytes = JString::get(utf8char, src_ptr);      // 1文字取り出し
		src_ptr += src_nbytes;
		code = JString::utf8to32(utf8char);                // utf-8からutf32に変換
		if (code >= 0x21 && code <= 0x7e) {
				code = code + 65248;                           // カタカナコードに変換
		} else if (code == 0x20) {
			code = 0x3000;
		}
		// utf32からutf8に変換
		dst_nbytes = JString::utf32to8(dst_ptr, code);
		dst_ptr += dst_nbytes;
	}
}

