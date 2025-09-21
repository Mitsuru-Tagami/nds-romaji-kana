#include "JString.h"

#define RKTBLSIZE (sizeof(r_table)/sizeof(r_table[0]))               // ローマ字テーブルサイズ

#if defined(ARDUINO_ARCH_AVR)
#include "romakana.h"
#include <avr/pgmspace.h>
#define pgm_str(str) pgm_read_word(&(str))                           // AVR PGM文字列アドレス参照マクロ関数
#define strlen_pgm(str) strlen_P(pgm_str(str))                       // AVR PGM文字列長さ取得参照マクロ関数
#define strcpy_pgm(dst,src)	strcpy_P((char *)&dst, pgm_str(src))     // AVR PGM strcpyマクロ関数  

#else
#define pgm_str(str) (str)                                           // AVR PGM互換文字列アドレス参照マクロ関数
#define strlen_pgm(str) strlen(str)                                  // AVR PGM互換文字列長さ取得参照マクロ関数
#define strcpy_pgm(dst,src)	strcpy((char *)&dst, src)                // AVR PGM互換 strcpyマクロ関数  


// ローマ字テーブル
static const char* r_table[] = {
    "a","ba","be","bi","bo","bu","bya","bye","byi","byo","byu","ca","ce","cha","che",
    "chi","cho","chu","ci","co","cu","cya","cye","cyi","cyo","cyu","da","de","dha","dhe",
    "dhi","dho","dhu","di","do","du","dya","dye","dyi","dyo","dyu","e","fa","fe","fi",
    "fo","fu","fya","fye","fyi","fyo","fyu","ga","ge","gi","go","gu","gya","gye","gyi",
    "gyo","gyu","ha","he","hi","ho","hu","hya","hye","hyi","hyo","hyu","i","ja","je",
    "ji","jo","ju","jya","jye","jyi","jyo","jyu","ka","ke","ki","ko","ku","kwa","kwe",
    "kwi","kwo","kwu","kya","kye","kyi","kyo","kyu","la","le","li","lo","ltsu","ltu","lu",
    "lya","lye","lyi","lyo","lyu","ma","me","mi","mo","mu","mya","mye","myi","myo","myu",
    "na","ne","ni","no","nu","nwa","nwe","nwi","nwo","nwu","nya","nye","nyi","nyo","nyu",
    "o","pa","pe","pi","po","pu","pya","pye","pyi","pyo","pyu","qa","qe","qi","qo",
    "qu","ra","re","ri","ro","ru","rya","rye","ryi","ryo","ryu","sa","se","sha","she",
    "shi","sho","shu","si","so","su","swa","swe","swi","swo","swu","sya","sye","syi","syo",
    "syu","ta","te","tha","the","thi","tho","thu","ti","to","tsa","tse","tsi","tso","tsu",
    "tu","tya","tye","tyi","tyo","tyu","u","va","ve","vi","vo","vu","vya","vye","vyi",
    "vyo","vyu","wa","we","wha","whe","whi","who","whu","wi","wo","wu","xa","xe","xi",
    "xo","xtsu","xtu","xu","xya","xye","xyi","xyo","xyu","ya","ye","yi","yo","yu","za",
    "ze","zi","zo","zu","zya","zye","zyi","zyo","zyu",
};

// かなテーブル
static const char* h_table[] = {
    "あ","ば","べ","び","ぼ","ぶ","びゃ","びぇ","びぃ","びょ","びゅ","か","せ","ちゃ","ちぇ",
    "ち","ちょ","ちゅ","し","こ","く","ちゃ","ちぇ","ちぃ","ちょ","ちゅ","だ","で","でゃ","でぇ",
    "でぃ","でょ","でゅ","ぢ","ど","づ","ぢゃ","ぢぇ","ぢぃ","ぢょ","ぢゅ","え","ふぁ","ふぇ","ふぃ",
    "ふぉ","ふ","ふゃ","ふぇ","ふぃ","ふょ","ふゅ","が","げ","ぎ","ご","ぐ","ぎゃ","ぎぇ","ぎぃ",
    "ぎょ","ぎゅ","は","へ","ひ","ほ","ふ","ひゃ","ひぇ","ひぃ","ひょ","ひゅ","い","じゃ","じぇ",
    "じ","じょ","じゅ","じゃ","じぇ","じぃ","じょ","じゅ","か","け","き","こ","く","くゎ","くぇ",
    "くぃ","くぉ","くぅ","きゃ","きぇ","きぃ","きょ","きゅ","ぁ","ぇ","ぃ","ぉ","っ","っ","ぅ",
    "ゃ","ぇ","ぃ","ょ","ゅ","ま","め","み","も","む","みゃ","みぇ","みぃ","みょ","みゅ",
    "な","ね","に","の","ぬ","ぬゎ","ぬぇ","ぬぃ","ぬぉ","ぬぅ","にゃ","にぇ","にぃ","にょ","にゅ",
    "お","ぱ","ぺ","ぴ","ぽ","ぷ","ぴゃ","ぴぇ","ぴぃ","ぴょ","ぴゅ","くぁ","くぇ","くぃ","くぉ",
    "く","ら","れ","り","ろ","る","りゃ","りぇ","りぃ","りょ","りゅ","さ","せ","しゃ","しぇ",
    "し","しょ","しゅ","し","そ","す","すゎ","すぇ","すぃ","すぉ","すぅ","しゃ","しぇ","し","しょ",
    "しゅ","た","て","てゃ","てぇ","てぃ","てょ","てゅ","ち","と","つぁ","つぇ","つぃ","つぉ","つ",
    "つ","ちゃ","ちぇ","ちぃ","ちょ","ちゅ","う","ゔぁ","ゔぇ","ゔぃ","ゔぉ","ゔ","ゔゃ","ゔぇ","ゔぃ",
    "ゔょ","ゔゅ","わ","うぇ","うぁ","うぇ","うぃ","うぉ","う","うぃ","を","う","ぁ","ぇ","ぃ",
    "ぉ","っ","っ","ぅ","ゃ","ぇ","ぃ","ょ","ゅ","や","いぇ","い","よ","ゆ","ざ",
    "ぜ","じ","ぞ","ず","じゃ","じぇ","じぃ","じょ","じゅ",
};
#endif

// 文字列バイト数の取得
uint16_t JString::bytes(const char* text) {
    return strlen(text);
}

//
// UTF8文字(1～4バイト)の文字数をカウントする
// pUTF8(in):   UTF8文字列格納アドレス
// 戻り値: 文字数
// 
uint16_t JString::len(const char* text) {
    uint8_t *pUTF8 = (uint8_t*)text;
    uint8_t c; 
    uint16_t cnt = 0;
    
    for(;;) {
        c = *pUTF8;
        if (c =='\0') break;
        if( c < 0x80 ) {
            // 1バイト文字
            cnt++; pUTF8++; 
        } else if( c >= 0xc0 && c < 0xe0 )  { 
            // 2バイト文字
            cnt++; pUTF8+=2;
        } else if( c >= 0xe0 && c < 0xf0 ) { 
            // 3バイト文字
            cnt++;  pUTF8+=3;
        } else if( c >= 0xf0 && c < 0xf5 ) { 
            // 4バイト文字
            cnt++; pUTF8+=4;
        } else { 
            break;
        }
    }
    return cnt;
}


//
// 先頭からの1文字取得
//  引数
//   dst(out): UTF16文字列格納アドレス
//   src(in):  UTF8文字列格納アドレス
// 戻り値
//   変換処理したUTF8文字バイト数
//
uint16_t JString::get(char *dst, char *src)  { 
    uint8_t c = *src;        
    if( c < 0x80 ) {
        // 1バイト文字の処理
        *dst++ = *src;
        *dst   = '\0';
        return(1);
    }

    if(c >= 0xc0 && c < 0xe0) {
        // 2バイト文字の処理
        *dst++ = *src++;
        *dst++ = *src;
        *dst   = '\0';
        return(2); 
    }

    if( c >= 0xe0 && c < 0xf0) {
        // 3バイト文字の処理
        *dst++ = *src++;
        *dst++ = *src++;
        *dst++ = *src;
        *dst = '\0';
        return(3);
    }
    
    if( c >= 0xf0 && c < 0xf5) {
        // 4バイト文字の処理
        *dst++ = *src++;
        *dst++ = *src++;
        *dst++ = *src++;
        *dst++ = *src;
        *dst = '\0';
        return(4);
    }
    *dst = '\0';
    return 0;
} 

// utf8 1文字をutf32に変換する
uint32_t JString::utf8to32(char* src) {
    uint32_t code;

    uint8_t c = *src;        
    if( c < 0x80 ) {
        // 1バイト文字の処理
        return (uint32_t)c;
    }

    if(c >= 0xc0 && c < 0xe0) {
        // 2バイト文字の処理
        code = 0x1f & c; 
        code = (code<<6)+(0x3f & src[1]); 
        return code; 
    }

    if( c >= 0xe0 && c < 0xf0) {
        // 3バイト文字の処理
        code = 0x0f & c; 
        code = (code<<6)+(0x3f & src[1]); 
        code = (code<<6)+(0x3f & src[2]); 
        return code; 
    }
    
    if( c >= 0xf0 && c < 0xf5) {
        // 4バイト文字の処理
        code = 0x07 & c; 
        code = (code<<6)+(0x3f & src[1]); 
        code = (code<<6)+(0x3f & src[2]); 
        code = (code<<6)+(0x3f & src[3]); 
        return code; 
    }
    return 0;        
}


// utf32 1文字をutf8 1文字に変換する
// 引数
//  dst: 変換したutf-8 1文字(1～4バイト+'\0')
//  code: utf32コード
// 戻り値
//  変換したutf-8 1文字のバイト数
//
uint8_t JString::utf32to8(char* dst, uint32_t code) {
    if (code < 0x80) {
        // 1バイト utf-8 に変換  0xxx-xxxx
        dst[0] = (char)code;
        dst[1] = '\0';
        return 1;
    }
    if (code < 0x800) {
        // 2バイト utf-8 に変換  110y-yyyx 10xx-xxxx (11ビット 上位5ヒット 下位6ヒット)
        dst[0] = (uint8_t)(0b11000000 | code>>6);                // 1バイト目 110y-yyyx
        dst[1] = (uint8_t)(0b10000000 | (code & 0b111111));      // 2バイト目 10xx-xxxx	
        dst[2] = '\0';
        return 2;
    }
    if (code < 0x10000) {
        // 3バイト utf-8 に変換 1110-yyyy 10yx-xxxx 10xx-xxxx (16ビット: 上位4ビット 中位6ビット 下位6ビット)
        dst[0] = (uint8_t)(0b11100000 | code>>12);               // 1バイト目 1110-yyyy
        dst[1] = (uint8_t)(0b10000000 | (code>>6 & 0b111111));   // 2バイト目 10yx-xxxx	
        dst[2] = (uint8_t)(0b10000000 | (code & 0b111111));      // 3バイト目 10xx-xxxx	
        dst[3] = '\0';
        return 3;
    }
    // 4バイト utf-8 に変換 1111-0yyy 10yy-xxxx 10xx-xxxx 10xx-xxxx	 (21ビット: 3ビット 6ビット 6ビット 6ビット)
    dst[0] = (uint8_t)(0b11110000 | code>>18);               // 1バイト目 1111-0yyy
    dst[1] = (uint8_t)(0b10000000 | (code>>12 & 0b111111));  // 2バイト目 10yx-xxxx	
    dst[2] = (uint8_t)(0b10000000 | (code>>6  & 0b111111));  // 3バイト目 10xx-xxxx	
    dst[3] = (uint8_t)(0b10000000 | (code & 0b111111));      // 4バイト目 10xx-xxxx	
    dst[4] = '\0';
    return 4;
}

// ローマ字テーブル２分検索
int16_t binfind(const char* key, uint8_t cmplen) {
	int16_t t_p = 0;                   // 検索範囲上限
	int16_t e_p = RKTBLSIZE-1;         // 検索範囲下限
	uint16_t flg_stop = 0;
	int16_t pos;
	int rc;
    char roma_str[10];

	for(;;) {
		pos = t_p + ((e_p - t_p+1)>>1);
#if defined(ARDUINO_ARCH_AVR)
    strcpy_pgm(roma_str, r_table[pos]);
		rc = strncasecmp(key, roma_str, cmplen);
#else
		rc = strncasecmp(key, r_table[pos], cmplen);
#endif
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

// ローマ字テーブルのインデックスを返す
// 引数
//   tokens
// 戻り値
// 　見つかった場合：	0以上
//	 見つからない場合： -1
//
int16_t get_roma_index(const char* tokens) {
    int16_t tokens_len = strlen(tokens);
    int16_t index = -1;
    int16_t tmp_index = -1;

    for (uint16_t i=1; i<=4; i++) {
        if (i > tokens_len)
            break;
        tmp_index = binfind(tokens, i);
        if (tmp_index < 0)
            break;
        if ( i != strlen_pgm(r_table[tmp_index]) ) {
        continue;
        }
        index = tmp_index;
    }
    return index;
}

// 撥音 "ん" に変換すべきかどうかを判定する
//  引数
//    tokens: 文字列
//  戻り値
//    1:変換可 0:変換不可
uint8_t isHatsuon(const char* tokens) {
	if (strlen(tokens) < 1)
		return 0;
	if (*tokens == 'n' || *tokens == 'm')
		return 1;
	return 0;
}


// 促音 "っ" に変換すべきかどうかを判定する
//  引数
//    tokens: 文字列
//  戻り値
//    1:変換可 0:変換不可
uint8_t isSokuon(const char* tokens) {
	if (strlen(tokens) < 2)
		return 0;
	if (tokens[0] != tokens[1])
		return 0;
	if (isalpha(tokens[0]))
		return 1;

	return 0;
}


// ローマ字ひらがな変換
//  引数
//   dst: 変換後のひらがな文字列
//   src: 変換対象ローマ字文字列
//  戻り値
//   ローマ字からひらがなに変換した文字数
//
uint16_t JString::roma_to_kana(char* dst, char* src) {
	uint16_t dst_pos = 0;
	uint16_t src_pos = 0;
	uint16_t src_len = strlen(src);
	int16_t index = 0;
	uint16_t rc = 0;

	for(;;) {
		if (src_pos >= src_len)
			break;
    index = get_roma_index(&src[src_pos]);
		if (index  >= 0) {
		//if ((index = get_roma_index(&src[src_pos])) >= 0) {
			// ローマ字変換可能
			uint16_t rm_len = strlen_pgm(r_table[index]);
			uint16_t hk_len = strlen_pgm(h_table[index]);
            strcpy_pgm(dst[dst_pos], h_table[index]);

			dst_pos += hk_len;
			src_pos += rm_len;
			rc++;
		} else if (isHatsuon(&src[src_pos])) {
			// 撥音 "ん"に変換可能
			uint16_t rm_len = strlen("n");
			uint16_t hk_len = strlen("ん");
			strcpy((char *)&dst[dst_pos], "ん");
			dst_pos += hk_len;
			src_pos += rm_len;
			rc++;
		} else if (isSokuon(&src[src_pos])) {
			// 促音 "っ"に変換可能
			uint16_t rm_len = strlen("t");
			uint16_t hk_len = strlen("っ");
			strcpy((char *)&dst[dst_pos], "っ");
			dst_pos += hk_len;
			src_pos += rm_len;
			rc++;
		} else {
			// ローマ字に変換不可の場合、先頭1文字をそのままコピーする
			((char*)dst)[dst_pos] = src[src_pos];
			dst_pos++;
			src_pos++;
		}
	}
	((char *)dst)[dst_pos] = '\0';
	return rc;
}