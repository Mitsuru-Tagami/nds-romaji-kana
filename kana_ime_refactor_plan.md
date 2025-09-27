# kana_ime.cpp リファクタリング計画

## 目的
`kana_ime.cpp` をローマ字かな変換の唯一の責任者とし、その結果を `SKK` エンジンに渡すことで、変換ロジックの一貫性を確保し、「gyuudon」問題のような競合を解消する。
これにより、ローマ字かな変換部分が独立したコンポーネントとなり、将来的に他の入力方式（IME）を開発・統合する際の拡張性を確保する。

## 変更点概要
1.  `kana_ime.cpp` 内で、`input_romaji_buffer` を完全にかな文字列（Shift-JISバイト列）に変換する。
2.  この変換されたかな文字列を `SKK` エンジンの `get_kouho_list` メソッドに渡す。
3.  表示用の `converted_kana_buffer` は、このかな文字列を元に生成する。

## 詳細手順

### 1. `kana_ime.cpp` の変更

#### 1.1. 新しいバッファの追加
-   `static char current_kana_input_sjis_bytes[256];` を追加し、完全に変換されたかな文字列（Shift-JISバイト列）を格納する。

#### 1.2. `kanaIME_update` 関数内の変換ロジックの修正
-   `// --- Conversion Logic ---` セクション内の `else { // No SKK candidates, fall back to romakana_map conversion` ブロックを修正する。
-   既存の `while` ループ内で、`converted_kana_buffer` (u16) ではなく、`current_kana_input_sjis_bytes` (char) にShift-JISバイト列を直接追加するように変更する。
    -   `best_match_sjis` (u16) が得られた場合、それが1バイト文字か2バイト文字かを判断し、対応するバイトを `current_kana_input_sjis_bytes` に追加する。
    -   `Cya`, `Cyu`, `Cyo` などの複合かなの場合も、それぞれの構成バイトを `current_kana_input_sjis_bytes` に追加する。
    -   カタカナ変換ロジックも、`best_match_sjis` (u16) に適用した後、バイト列に変換して追加する。
-   ループ終了後、`current_kana_input_sjis_bytes` をヌル終端する。

#### 1.3. `SKK` エンジン呼び出しの修正
-   `skk_engine.get_kouho_list(s_kouho_list, s_out_okuri, input_romaji_buffer);` の呼び出しを、`skk_engine.get_kouho_list(s_kouho_list, s_out_okuri, current_kana_input_sjis_bytes);` に変更する。

#### 1.4. 表示用バッファの更新
-   `converted_kana_buffer` を表示用に更新するため、`sjis_to_u16_array(converted_kana_buffer, current_kana_input_sjis_bytes);` を呼び出す。

### 2. `skk.cpp` の変更 (必要に応じて)
-   `skk_engine.get_kouho_list` がかな文字列を受け取ることを前提とするため、`skk.cpp` 内の `roma_to_kana` メソッドが `get_kouho_list` から呼び出されている場合は、その呼び出しを削除または無効化する。
-   `skk.h` の `get_kouho_list` のコメントを更新し、`in_token` がかな文字列であることを明記する。

## 検証
-   プロジェクトをコンパイルする。
-   「gyuudon」が「ぎゅうどん」と変換され、候補が表示されることを確認する。
-   「kya」「gya」などの拗音が正しく変換されることを確認する。
-   その他の通常のローマ字かな変換も引き続き機能することを確認する。