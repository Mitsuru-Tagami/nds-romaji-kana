# nds-romaji-kana

A Japanese Romaji-to-Kana Input Method Editor (IME) library for Nintendo DS homebrew development. This project provides core functionality for converting Romaji input into Hiragana and Katakana, designed for integration into NDS applications.

## Features
-   **Romaji-to-Kana Conversion:** Converts Romaji input into corresponding Hiragana and Katakana characters.
-   **Full-width Character Support:** Automatically converts numbers and symbols to full-width characters for consistent Japanese display.
-   **Dynamic Character Spacing:** Adjusts character spacing based on character width (half-width/full-width) for improved readability.
-   **On-screen Debug Display:** Includes an optional on-screen debug display to monitor internal buffer states during development.

## Build Instructions

This project uses `devkitPro` and `make`.

1.  **Prerequisites:**
	*   [devkitPro](https://devkitpro.org/) installed and configured.
	*   `make` utility.

2.  **Clone the repository:**
git clone https://github.com/Mitsuru-Tagami/nds-romaji-kana.git
cd nds-romaji-kana

3.  **Build the project:**
make clean
make

 This will compile the source code, build the `libkanaime.a` static library, link the `kana_ime_test.elf` executable, and generate the final `kana_ime_test.nds` file in the `build/` directory.

4.  **Run on NDS:**
Copy `build/kana_ime_test.nds` to your NDS flashcart or load it in an emulator.

## Usage (as a library)

(This section would be more detailed if it were a standalone library, but for now, it's integrated into `main.c` for testing.)

 To integrate this library into your NDS homebrew project:

1.  Include `kana_ime.h` in your source files.
2.  Link against `libkanaime.a` in your `Makefile`.
3.  Call `kanaIME_init()` once at the start of your application.
4.  Call `kanaIME_update()` periodically in your main loop to process input and update the display.

## Debugging

During development, an on-screen debug display can be enabled to monitor the internal state of the input buffers. This display shows:

-   `Key:` The ASCII value of the last pressed key.
-   `Romaji:` The current content of the `input_romaji_buffer` and its length.
-   `KanaLen:` The current length of the `converted_kana_buffer`.

This information is displayed on the top screen at coordinates (10, 30), (10, 40), and (10, 50) respectively.

## License
MIT