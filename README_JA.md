<div align="center">

# ringbuffer

**組み込み C 向け軽量ロックフリー・リングバッファ**

[![Stars](https://img.shields.io/github/stars/W-Okaka/ringbuffer?style=flat-square)](https://github.com/W-Okaka/ringbuffer/stargazers)
[![Forks](https://img.shields.io/github/forks/W-Okaka/ringbuffer?style=flat-square)](https://github.com/W-Okaka/ringbuffer/forks)
[![License](https://img.shields.io/github/license/W-Okaka/ringbuffer?style=flat-square)](LICENSE)
[![C99](https://img.shields.io/badge/標準-C99-blue?style=flat-square)](https://en.wikipedia.org/wiki/C99)

[English](README.md) · [简体中文](README_ZH.md) · [日本語](README_JA.md)

</div>

---

## 特徴

- **ミラーインデックス方式** — O(1) で満杯/空を判定、1バイトも無駄にしない
- **非上書きモード** — バッファが満杯の場合、書き込みは黙って切り捨てられる
- **強制書き込みモード** — 最古のデータを破棄して新しいデータのための領域を確保する
- **ゼロコピー API** — `ringbuffer_peek` + `ringbuffer_consume` でインプレース処理が可能
- **ブロック操作・1バイト操作** の両方を読み書きでサポート
- 最大容量：**32767バイト**（15ビットインデックスフィールド）
- 動的メモリ確保なし；呼び出し元がストレージプールを提供する
- C99 準拠；C++ プロジェクト向けに `extern "C"` ガードを提供

## ファイル構成

| ファイル | 説明 |
|---------|------|
| `ringbuffer.h` | 公開 API とデータ型定義 |
| `ringbuffer.c` | 実装 |

## クイックスタート

```c
#include "ringbuffer.h"

#define BUF_SIZE 256
static uint8_t  storage[BUF_SIZE];
static ringbuffer_t rb;

int main(void)
{
    ringbuffer_init(&rb, storage, BUF_SIZE);

    /* 1バイト書き込み / 読み出し */
    ringbuffer_putchar(&rb, 0xAB);
    uint8_t ch;
    ringbuffer_getchar(&rb, &ch);

    /* ブロック書き込み / 読み出し */
    uint8_t data[] = {1, 2, 3, 4};
    ringbuffer_put(&rb, data, sizeof(data));
    uint8_t out[4];
    ringbuffer_get(&rb, out, sizeof(out));

    /* ゼロコピー peek + consume */
    uint8_t *ptr;
    size_t n = ringbuffer_peek(&rb, &ptr);
    /* ptr[0..n-1] をインプレースで処理 */
    ringbuffer_consume(&rb, n);

    return 0;
}
```

## API リファレンス

### 初期化とリセット

```c
void ringbuffer_init(ringbuffer_t *rb, uint8_t *pool, uint16_t size);
void ringbuffer_reset(ringbuffer_t *rb);
```

### 状態・長さ取得

```c
ringbuffer_state_t ringbuffer_status(ringbuffer_t *rb);   /* 現在の状態を取得 */
size_t             ringbuffer_data_len(ringbuffer_t *rb); /* 読み出し可能バイト数 */
ringbuffer_space_len(rb)          /* マクロ — 書き込み可能な空きバイト数 */
```

### 1バイト操作

| 関数 | 満杯 / 空のときの動作 |
|------|----------------------|
| `ringbuffer_putchar(rb, ch)` | 黙って破棄（0 を返す） |
| `ringbuffer_getchar(rb, &ch)` | 黙って無視（0 を返す） |
| `ringbuffer_putchar_force(rb, ch)` | 最古のバイトを上書き |

### ブロック操作

| 関数 | 空き不足のときの動作 |
|------|---------------------|
| `ringbuffer_put(rb, buf, len)` | 空き容量に切り捨て |
| `ringbuffer_get(rb, buf, len)` | 利用可能データに切り捨て |
| `ringbuffer_put_force(rb, buf, len)` | 最古のデータを上書き |

### ゼロコピー読み出し

```c
size_t ringbuffer_peek(ringbuffer_t *rb, uint8_t **ptr);
size_t ringbuffer_consume(ringbuffer_t *rb, size_t length);
```

`peek` はデータを消費せずに、最初の連続した読み出し可能領域への直接ポインタを返す。
インプレース処理後に `consume` を呼び出して読み出しポインタを進める。

## 動作原理

本実装は**ミラーインデックス方式**を採用している：

| `write_index` と `read_index` の関係 | ミラービットが等しいか | 状態 |
|---|---|---|
| 等しい | はい | **空（EMPTY）** |
| 等しい | いいえ | **満杯（FULL）** |
| 等しくない | — | **半分（HALFFULL）** |

各インデックスは15ビットフィールド（範囲 0〜32767）で格納され、ポインタが折り返すたびに1ビットのミラーフラグが反転する。これにより容量を犠牲にすることなく、満杯と空の状態をすっきりと区別できる。

## ライセンス

パブリックドメインとして公開。自由にご利用ください。
