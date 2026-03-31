/**
 ******************************************************************************
 * @file    ringbuffer.h
 * @brief   Lock-free ring buffer (circular buffer) - Public API
 ******************************************************************************
 *
 * Implementation uses the mirror-index technique:
 *   write_mirror == read_mirror  &&  write_index == read_index  ->  EMPTY
 *   write_mirror != read_mirror  &&  write_index == read_index  ->  FULL
 *
 * Index fields are 15-bit, so the maximum buffer capacity is 32767 bytes.
 *
 ******************************************************************************
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Data Types
 * --------------------------------------------------------------------------- */

/**
 * @brief  Ring buffer control block.
 *
 * @note   Do NOT access structure members directly.
 *         Use the API functions provided by this library.
 */
typedef struct {
    uint8_t  *buffer_ptr;           /*!< Pointer to the underlying storage pool  */

    uint16_t  write_mirror : 1;     /*!< Write pointer mirror bit                */
    uint16_t  write_index  : 15;    /*!< Write offset in bytes, [0, buffer_size) */
    uint16_t  read_mirror  : 1;     /*!< Read pointer mirror bit                 */
    uint16_t  read_index   : 15;    /*!< Read offset in bytes,  [0, buffer_size) */

    uint16_t  buffer_size;          /*!< Capacity of the storage pool in bytes   */
} ringbuffer_t;

/**
 * @brief  Ring buffer state.
 */
typedef enum {
    RINGBUFFER_EMPTY,       /*!< No data available to read   */
    RINGBUFFER_FULL,        /*!< No space available to write */
    RINGBUFFER_HALFFULL,    /*!< Partially filled            */
} ringbuffer_state_t;

/* ---------------------------------------------------------------------------
 * Utility Macros
 * --------------------------------------------------------------------------- */

/**
 * @brief  Returns the number of free bytes available for writing.
 * @param  rb   Non-NULL pointer to a ringbuffer_t instance.
 * @return      Free space in bytes (size_t).
 */
#define ringbuffer_space_len(rb)  ((rb)->buffer_size - ringbuffer_data_len(rb))

/* ---------------------------------------------------------------------------
 * API Declarations
 * --------------------------------------------------------------------------- */

/**
 * @brief  Initialize a ring buffer with an external storage pool.
 *
 * Binds the control block to the provided memory pool and resets
 * all indices to zero.
 *
 * @param[in]  rb    Pointer to the ring buffer control block. Must not be NULL.
 * @param[in]  pool  Pointer to the external storage pool. Must not be NULL.
 * @param[in]  size  Size of the storage pool in bytes. Must be in (0, 32767].
 */
void ringbuffer_init(ringbuffer_t *rb, uint8_t *pool, uint16_t size);

/**
 * @brief  Query the current state of the ring buffer.
 *
 * @param[in]  rb  Pointer to the ring buffer control block. Must not be NULL.
 * @retval     RINGBUFFER_EMPTY     Buffer contains no data.
 * @retval     RINGBUFFER_FULL      Buffer has no free space.
 * @retval     RINGBUFFER_HALFFULL  Buffer is partially filled.
 */
ringbuffer_state_t ringbuffer_status(ringbuffer_t *rb);

/**
 * @brief  Return the number of bytes currently stored in the buffer.
 *
 * @param[in]  rb  Pointer to the ring buffer control block. Must not be NULL.
 * @return         Number of readable bytes.
 */
size_t ringbuffer_data_len(ringbuffer_t *rb);

/**
 * @brief  Write a single byte (non-overwrite mode).
 *
 * The write is rejected silently when the buffer is full.
 *
 * @param[in]  rb  Pointer to the ring buffer control block. Must not be NULL.
 * @param[in]  ch  Byte to write.
 * @retval     1   Byte written successfully.
 * @retval     0   Buffer full; byte discarded.
 */
_Bool ringbuffer_putchar(ringbuffer_t *rb, uint8_t ch);

/**
 * @brief  Read a single byte from the buffer.
 *
 * The read fails silently when the buffer is empty; *ch is left unchanged.
 *
 * @param[in]   rb  Pointer to the ring buffer control block. Must not be NULL.
 * @param[out]  ch  Destination for the byte read.
 * @retval      1   Byte read successfully.
 * @retval      0   Buffer empty; no data returned.
 */
_Bool ringbuffer_getchar(ringbuffer_t *rb, uint8_t *ch);

/**
 * @brief  Write a block of bytes (non-overwrite mode).
 *
 * If @p length exceeds the available free space, the write is truncated
 * to fit exactly the remaining capacity.
 *
 * @param[in]  rb      Pointer to the ring buffer control block. Must not be NULL.
 * @param[in]  buf     Source data buffer. Must not be NULL.
 * @param[in]  length  Number of bytes to write.
 * @return             Actual number of bytes written.
 */
size_t ringbuffer_put(ringbuffer_t *rb, const uint8_t *buf, uint16_t length);

/**
 * @brief  Read a block of bytes from the buffer.
 *
 * If @p length exceeds the number of available bytes, the read is truncated
 * to the actual data length.
 *
 * @param[in]   rb      Pointer to the ring buffer control block. Must not be NULL.
 * @param[out]  buf     Destination buffer. Must not be NULL.
 *                      Caller must ensure it is at least @p length bytes large.
 * @param[in]   length  Number of bytes to read.
 * @return              Actual number of bytes read.
 */
size_t ringbuffer_get(ringbuffer_t *rb, uint8_t *buf, uint16_t length);

/**
 * @brief  Write a single byte, overwriting the oldest byte if the buffer is full.
 *
 * The read pointer advances to keep the buffer consistent whenever an
 * overwrite occurs.
 *
 * @param[in]  rb  Pointer to the ring buffer control block. Must not be NULL.
 * @param[in]  ch  Byte to write.
 * @return         Always returns 1.
 */
size_t ringbuffer_putchar_force(ringbuffer_t *rb, uint8_t ch);

/**
 * @brief  Write a block of bytes, overwriting oldest data if necessary.
 *
 * When the buffer cannot hold all @p length bytes, the oldest bytes are
 * silently discarded to make room.
 * If @p length > buffer_size, only the last @p buffer_size bytes of @p buf
 * are written.
 *
 * @param[in]  rb      Pointer to the ring buffer control block. Must not be NULL.
 * @param[in]  buf     Source data buffer. Must not be NULL.
 * @param[in]  length  Number of bytes to write.
 * @return             Actual number of bytes written (min(@p length, buffer_size)).
 */
size_t ringbuffer_put_force(ringbuffer_t *rb, const uint8_t *buf, uint16_t length);

/**
 * @brief  Zero-copy peek at the first contiguous readable region.
 *
 * Returns a pointer into the internal buffer and the length of the first
 * contiguous readable region without consuming any data.
 * Because the ring buffer may wrap around, this function returns only the
 * first contiguous segment.
 * Use ringbuffer_consume() to advance the read pointer after processing,
 * or call ringbuffer_get() if a full copy is preferred.
 *
 * @warning  The caller must NOT write through the returned pointer.
 *
 * @param[in]   rb   Pointer to the ring buffer control block. Must not be NULL.
 * @param[out]  ptr  Set to the start address of the first contiguous region.
 * @return           Length of the first contiguous readable region in bytes;
 *                   returns 0 if the buffer is empty.
 */
size_t ringbuffer_peek(ringbuffer_t *rb, uint8_t **ptr);

/**
 * @brief  Advance the read pointer without copying data (zero-copy consume).
 *
 * Intended to be paired with ringbuffer_peek(): peek to obtain a direct
 * pointer, process the data in-place, then call this function to mark
 * those bytes as consumed.
 *
 * If @p length exceeds the number of available bytes, it is clamped to
 * the actual data length.
 *
 * @param[in]  rb      Pointer to the ring buffer control block. Must not be NULL.
 * @param[in]  length  Number of bytes to mark as consumed.
 * @return             Actual number of bytes consumed.
 */
size_t ringbuffer_consume(ringbuffer_t *rb, size_t length);

/**
 * @brief  Reset the ring buffer to the empty state.
 *
 * All read/write indices and mirror bits are cleared to zero.
 * The underlying storage pool is NOT zeroed; stale bytes remain in memory.
 *
 * @param[in]  rb  Pointer to the ring buffer control block. Must not be NULL.
 */
void ringbuffer_reset(ringbuffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RINGBUFFER_H */
