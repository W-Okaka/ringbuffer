<div align="center">

# ringbuffer

**为嵌入式 C 设计的轻量级无锁环形缓冲区**

[![Stars](https://img.shields.io/github/stars/W-Okaka/ringbuffer?style=flat-square)](https://github.com/W-Okaka/ringbuffer/stargazers)
[![Forks](https://img.shields.io/github/forks/W-Okaka/ringbuffer?style=flat-square)](https://github.com/W-Okaka/ringbuffer/forks)
[![License](https://img.shields.io/github/license/W-Okaka/ringbuffer?style=flat-square)](LICENSE)
[![C99](https://img.shields.io/badge/标准-C99-blue?style=flat-square)](https://en.wikipedia.org/wiki/C99)

[English](README.md) · [简体中文](README_ZH.md) · [日本語](README_JA.md)

</div>

---

## 特性

- **镜像索引技术** — O(1) 时间复杂度判断满/空状态，无需浪费一个字节
- **非覆盖写模式** — 缓冲区满时写入被静默截断
- **强制写（覆盖）模式** — 自动丢弃最旧数据以腾出空间
- **零拷贝 API** — `ringbuffer_peek` + `ringbuffer_consume` 支持原地处理
- **块操作与单字节操作** 均支持读写两个方向
- 最大容量：**32767 字节**（15 位索引字段）
- 无动态内存分配，由调用者提供存储池
- 兼容 C99 标准；提供 `extern "C"` 保护，支持 C++ 项目

## 文件说明

| 文件 | 说明 |
|------|------|
| `ringbuffer.h` | 公共 API 与数据类型定义 |
| `ringbuffer.c` | 功能实现 |

## 快速上手

```c
#include "ringbuffer.h"

#define BUF_SIZE 256
static uint8_t  storage[BUF_SIZE];
static ringbuffer_t rb;

int main(void)
{
    ringbuffer_init(&rb, storage, BUF_SIZE);

    /* 单字节写 / 读 */
    ringbuffer_putchar(&rb, 0xAB);
    uint8_t ch;
    ringbuffer_getchar(&rb, &ch);

    /* 块写 / 块读 */
    uint8_t data[] = {1, 2, 3, 4};
    ringbuffer_put(&rb, data, sizeof(data));
    uint8_t out[4];
    ringbuffer_get(&rb, out, sizeof(out));

    /* 零拷贝 peek + consume */
    uint8_t *ptr;
    size_t n = ringbuffer_peek(&rb, &ptr);
    /* 在 ptr[0..n-1] 上原地处理数据 */
    ringbuffer_consume(&rb, n);

    return 0;
}
```

## API 参考

### 初始化与重置

```c
void ringbuffer_init(ringbuffer_t *rb, uint8_t *pool, uint16_t size);
void ringbuffer_reset(ringbuffer_t *rb);
```

### 状态与长度查询

```c
ringbuffer_state_t ringbuffer_status(ringbuffer_t *rb);   /* 查询当前状态 */
size_t             ringbuffer_data_len(ringbuffer_t *rb); /* 可读字节数 */
ringbuffer_space_len(rb)          /* 宏 — 可写空闲字节数 */
```

### 单字节操作

| 函数 | 满/空时的行为 |
|------|-------------|
| `ringbuffer_putchar(rb, ch)` | 静默丢弃（返回 0） |
| `ringbuffer_getchar(rb, &ch)` | 静默忽略（返回 0） |
| `ringbuffer_putchar_force(rb, ch)` | 覆盖最旧字节 |

### 块操作

| 函数 | 空间不足时的行为 |
|------|----------------|
| `ringbuffer_put(rb, buf, len)` | 截断至可用空间 |
| `ringbuffer_get(rb, buf, len)` | 截断至可用数据 |
| `ringbuffer_put_force(rb, buf, len)` | 覆盖最旧数据 |

### 零拷贝读取

```c
size_t ringbuffer_peek(ringbuffer_t *rb, uint8_t **ptr);
size_t ringbuffer_consume(ringbuffer_t *rb, size_t length);
```

`peek` 返回指向首段连续可读区域的直接指针，不消耗数据。
原地处理完毕后调用 `consume` 推进读指针。

## 实现原理

本实现采用**镜像索引技术**：

| `write_index` 与 `read_index` | 镜像位是否相等 | 状态 |
|---|---|---|
| 相等 | 是 | **空（EMPTY）** |
| 相等 | 否 | **满（FULL）** |
| 不等 | — | **半满（HALFFULL）** |

每个索引以 15 位字段存储（范围 0–32767），并附带 1 位镜像标志，每当指针绕回时翻转。这在不损失任何容量的前提下，干净地区分了满与空两种状态。

## 许可证

以公有领域方式发布，可自由使用。
