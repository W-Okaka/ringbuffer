<div align="center">

# ringbuffer

**A lightweight, lock-free ring buffer for embedded C**

[![Stars](https://img.shields.io/github/stars/W-Okaka/ringbuffer?style=flat-square)](https://github.com/W-Okaka/ringbuffer/stargazers)
[![Forks](https://img.shields.io/github/forks/W-Okaka/ringbuffer?style=flat-square)](https://github.com/W-Okaka/ringbuffer/forks)
[![License](https://img.shields.io/github/license/W-Okaka/ringbuffer?style=flat-square)](LICENSE)
[![C99](https://img.shields.io/badge/standard-C99-blue?style=flat-square)](https://en.wikipedia.org/wiki/C99)

[English](README.md) · [简体中文](README_ZH.md)

<img src="https://count.getloli.com/@W-Okaka-ringbuffer?name=W-Okaka-ringbuffer&theme=gelbooru" alt="visitor counter" />

</div>

---

## Features

- **Mirror-index technique** — O(1) full/empty detection, no wasted slot required
- **Non-overwrite mode** — writes are silently truncated when the buffer is full
- **Force-write mode** — oldest data is discarded to make room for new data
- **Zero-copy API** — `ringbuffer_peek` + `ringbuffer_consume` for in-place processing
- **Block and single-byte** operations for both read and write
- Maximum capacity: **32767 bytes** (15-bit index fields)
- No dynamic memory allocation; caller supplies the storage pool
- C99 compatible; `extern "C"` guard for C++ projects

## Files

| File | Description |
|------|-------------|
| `ringbuffer.h` | Public API and data types |
| `ringbuffer.c` | Implementation |

## Quick Start

```c
#include "ringbuffer.h"

#define BUF_SIZE 256
static uint8_t  storage[BUF_SIZE];
static ringbuffer_t rb;

int main(void)
{
    ringbuffer_init(&rb, storage, BUF_SIZE);

    /* Single-byte write / read */
    ringbuffer_putchar(&rb, 0xAB);
    uint8_t ch;
    ringbuffer_getchar(&rb, &ch);

    /* Block write / read */
    uint8_t data[] = {1, 2, 3, 4};
    ringbuffer_put(&rb, data, sizeof(data));
    uint8_t out[4];
    ringbuffer_get(&rb, out, sizeof(out));

    /* Zero-copy peek + consume */
    uint8_t *ptr;
    size_t n = ringbuffer_peek(&rb, &ptr);
    /* process ptr[0..n-1] in place */
    ringbuffer_consume(&rb, n);

    return 0;
}
```

## API Reference

### Initialization & Reset

```c
void ringbuffer_init(ringbuffer_t *rb, uint8_t *pool, uint16_t size);
void ringbuffer_reset(ringbuffer_t *rb);
```

### Status & Length

```c
ringbuffer_state_t ringbuffer_status(ringbuffer_t *rb);
size_t             ringbuffer_data_len(ringbuffer_t *rb);
ringbuffer_space_len(rb)          /* macro — free bytes for writing */
```

### Single-Byte Operations

| Function | Behavior when full / empty |
|----------|---------------------------|
| `ringbuffer_putchar(rb, ch)` | Rejects silently (returns 0) |
| `ringbuffer_getchar(rb, &ch)` | Rejects silently (returns 0) |
| `ringbuffer_putchar_force(rb, ch)` | Overwrites oldest byte |

### Block Operations

| Function | Behavior when space is insufficient |
|----------|-------------------------------------|
| `ringbuffer_put(rb, buf, len)` | Truncated to available space |
| `ringbuffer_get(rb, buf, len)` | Truncated to available data |
| `ringbuffer_put_force(rb, buf, len)` | Overwrites oldest bytes |

### Zero-Copy Read

```c
size_t ringbuffer_peek(ringbuffer_t *rb, uint8_t **ptr);
size_t ringbuffer_consume(ringbuffer_t *rb, size_t length);
```

`peek` returns a direct pointer to the first contiguous readable region without consuming data.
Call `consume` after in-place processing to advance the read pointer.

## How It Works

The implementation uses the **mirror-index technique**:

| `write_index` vs `read_index` | Mirror bits equal? | State |
|---|---|---|
| Equal | Yes | **EMPTY** |
| Equal | No | **FULL** |
| Not equal | — | **HALFFULL** |

Each index occupies 15 bits (range 0–32767). A 1-bit mirror flag flips every time the pointer wraps around. This cleanly distinguishes full from empty without sacrificing a byte of capacity.

## License

Released into the public domain. Use it freely.
