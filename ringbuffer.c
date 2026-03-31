/**
 ******************************************************************************
 * @file    ringbuffer.c
 * @brief   Lock-free ring buffer - Implementation
 ******************************************************************************
 */

#include "ringbuffer.h"
#include <assert.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Initialization
 * --------------------------------------------------------------------------- */

void ringbuffer_init(ringbuffer_t *rb, uint8_t *pool, uint16_t size)
{
    if (rb == NULL || pool == NULL || size == 0) return;

    rb->write_mirror = 0;
    rb->write_index  = 0;
    rb->read_mirror  = 0;
    rb->read_index   = 0;
    rb->buffer_ptr   = pool;
    rb->buffer_size  = size;
}

/* ---------------------------------------------------------------------------
 * Status / Length Queries
 * --------------------------------------------------------------------------- */

/* Fix #2: 'static inline' confines the inline definition to this translation
 * unit, avoiding the C99 requirement for a separate external definition. */
static inline ringbuffer_state_t ringbuffer_status(ringbuffer_t *rb)
{
    assert(rb != NULL);

    if (rb->read_index == rb->write_index)
    {
        /* Same mirror bit -> empty; different mirror bit -> full */
        if (rb->read_mirror == rb->write_mirror)
            return RINGBUFFER_EMPTY;
        else
            return RINGBUFFER_FULL;
    }
    return RINGBUFFER_HALFFULL;
}

size_t ringbuffer_data_len(ringbuffer_t *rb)
{
    assert(rb != NULL);

    switch (ringbuffer_status(rb))
    {
        case RINGBUFFER_EMPTY:
            return 0;

        case RINGBUFFER_FULL:
            return rb->buffer_size;

        case RINGBUFFER_HALFFULL:
        {
            size_t w = rb->write_index;
            size_t r = rb->read_index;
            /* Write has not wrapped relative to read */
            return (w > r) ? (w - r) : (rb->buffer_size - (r - w));
        }
    }
    /* Unreachable */
    return 0;
}

/* ---------------------------------------------------------------------------
 * Single-Byte Read / Write
 * --------------------------------------------------------------------------- */

_Bool ringbuffer_putchar(ringbuffer_t *rb, uint8_t ch)
{
    assert(rb != NULL);

    /* Fix #6: check status directly instead of computing space_len,
     * avoiding one subtraction for the common single-byte hot path. */
    if (ringbuffer_status(rb) == RINGBUFFER_FULL) return 0;

    rb->buffer_ptr[rb->write_index] = ch;

    /* Advance write pointer; flip mirror bit on wrap-around */
    if (rb->write_index == rb->buffer_size - 1)
    {
        rb->write_mirror ^= 1;
        rb->write_index   = 0;
    }
    else
    {
        rb->write_index++;
    }
    return 1;
}

_Bool ringbuffer_getchar(ringbuffer_t *rb, uint8_t *ch)
{
    assert(rb != NULL);

    /* Fix #6: check status directly instead of computing data_len,
     * avoiding one switch-based calculation for the common hot path. */
    if (ringbuffer_status(rb) == RINGBUFFER_EMPTY) return 0;

    *ch = rb->buffer_ptr[rb->read_index];

    /* Advance read pointer; flip mirror bit on wrap-around */
    if (rb->read_index == rb->buffer_size - 1)
    {
        rb->read_mirror ^= 1;
        rb->read_index   = 0;
    }
    else
    {
        rb->read_index++;
    }
    return 1;
}

/* ---------------------------------------------------------------------------
 * Block Read / Write  (non-overwrite)
 * --------------------------------------------------------------------------- */

/* Fix #4: buf is declared const - ringbuffer_put never modifies the source. */
size_t ringbuffer_put(ringbuffer_t *rb, const uint8_t *buf, uint16_t length)
{
    assert(rb != NULL && buf != NULL);

    size_t space_len = ringbuffer_space_len(rb);
    if (space_len == 0) return 0;

    /* Clamp to available space.
     * Fix #3: removed the redundant second clamp (length > buffer_size),
     * which was unreachable because space_len <= buffer_size always holds. */
    if (length > space_len) length = (uint16_t)space_len;

    if (rb->buffer_size - rb->write_index > length)
    {
        /* Data fits in the remaining linear region - single copy */
        memcpy(&rb->buffer_ptr[rb->write_index], buf, length);
        rb->write_index += length;
    }
    else
    {
        /* Data wraps around - two copies */
        uint16_t first  = rb->buffer_size - rb->write_index;
        uint16_t second = length - first;

        memcpy(&rb->buffer_ptr[rb->write_index], buf, first);
        rb->write_mirror ^= 1;
        rb->write_index   = 0;

        memcpy(&rb->buffer_ptr[0], buf + first, second);
        rb->write_index = second;
    }
    return length;
}

size_t ringbuffer_get(ringbuffer_t *rb, uint8_t *buf, uint16_t length)
{
    assert(rb != NULL && buf != NULL);

    size_t data_len = ringbuffer_data_len(rb);
    if (data_len == 0) return 0;

    /* Clamp to available data */
    if (length > data_len) length = (uint16_t)data_len;

    if (rb->buffer_size - rb->read_index > length)
    {
        /* Data is contiguous - single copy */
        memcpy(buf, &rb->buffer_ptr[rb->read_index], length);
        rb->read_index += length;
    }
    else
    {
        /* Data wraps around - two copies */
        uint16_t first  = rb->buffer_size - rb->read_index;
        uint16_t second = length - first;

        memcpy(buf, &rb->buffer_ptr[rb->read_index], first);
        rb->read_mirror ^= 1;
        rb->read_index   = 0;

        memcpy(buf + first, &rb->buffer_ptr[0], second);
        rb->read_index = second;
    }
    return length;
}

/* ---------------------------------------------------------------------------
 * Force Write (overwrite oldest data when full)
 * --------------------------------------------------------------------------- */

size_t ringbuffer_putchar_force(ringbuffer_t *rb, uint8_t ch)
{
    assert(rb != NULL);

    ringbuffer_state_t old_state = ringbuffer_status(rb);

    rb->buffer_ptr[rb->write_index] = ch;

    if (rb->write_index == rb->buffer_size - 1)
    {
        rb->write_mirror ^= 1;
        rb->write_index   = 0;
        if (old_state == RINGBUFFER_FULL)
        {
            /* Read pointer must follow write pointer on overwrite */
            rb->read_index  = rb->write_index;
            rb->read_mirror ^= 1;
        }
    }
    else
    {
        rb->write_index++;
        if (old_state == RINGBUFFER_FULL)
            rb->read_index = rb->write_index;
    }
    return 1;
}

/* Fix #4: buf is declared const - ringbuffer_put_force never modifies the source. */
size_t ringbuffer_put_force(ringbuffer_t *rb, const uint8_t *buf, uint16_t length)
{
    assert(rb != NULL && buf != NULL);

    size_t old_space_len = ringbuffer_space_len(rb);

    /* Keep only the newest buffer_size bytes when input exceeds capacity */
    if (length > rb->buffer_size)
    {
        buf    = buf + (length - rb->buffer_size);
        length = rb->buffer_size;
    }

    if (rb->buffer_size - rb->write_index > length)
    {
        /* Single copy - no wrap-around */
        memcpy(&rb->buffer_ptr[rb->write_index], buf, length);
        rb->write_index += length;

        if (length > old_space_len)
        {
            /* Fix #1: flip read_mirror when needed so that the state resolves
             * to FULL (read_index == write_index AND read_mirror != write_mirror).
             * Without this flip, the state would incorrectly read as EMPTY. */
            if (rb->read_mirror == rb->write_mirror)
                rb->read_mirror ^= 1;
            rb->read_index = rb->write_index;
        }
    }
    else
    {
        /* Two copies - write wraps around */
        uint16_t first  = rb->buffer_size - rb->write_index;
        uint16_t second = length - first;

        memcpy(&rb->buffer_ptr[rb->write_index], buf, first);
        rb->write_mirror ^= 1;
        rb->write_index   = 0;

        memcpy(&rb->buffer_ptr[0], buf + first, second);
        rb->write_index = second;

        if (length > old_space_len)
        {
            /* Flip read mirror bit if write pointer crossed read pointer */
            if (rb->write_index <= rb->read_index)
                rb->read_mirror ^= 1;
            rb->read_index = rb->write_index;
        }
    }
    return length;
}

/* ---------------------------------------------------------------------------
 * Zero-Copy Peek + Consume
 * --------------------------------------------------------------------------- */

size_t ringbuffer_peek(ringbuffer_t *rb, uint8_t **ptr)
{
    assert(rb != NULL);

    size_t data_len = ringbuffer_data_len(rb);
    if (data_len == 0) return 0;

    *ptr = &rb->buffer_ptr[rb->read_index];

    /* Return only the first contiguous segment */
    if (rb->buffer_size - rb->read_index > data_len)
        return data_len;
    return rb->buffer_size - rb->read_index;
}

/* Fix #5: added ringbuffer_consume() to pair with ringbuffer_peek(),
 * completing the zero-copy read pattern without a redundant memcpy. */
size_t ringbuffer_consume(ringbuffer_t *rb, size_t length)
{
    assert(rb != NULL);

    size_t data_len = ringbuffer_data_len(rb);
    if (data_len == 0) return 0;

    /* Clamp to available data */
    if (length > data_len) length = data_len;

    size_t remaining = rb->buffer_size - rb->read_index;
    if (remaining > length)
    {
        /* Read pointer stays within the same linear region */
        rb->read_index += (uint16_t)length;
    }
    else
    {
        /* Read pointer wraps around */
        rb->read_mirror ^= 1;
        rb->read_index   = (uint16_t)(length - remaining);
    }
    return length;
}

/* ---------------------------------------------------------------------------
 * Reset
 * --------------------------------------------------------------------------- */

void ringbuffer_reset(ringbuffer_t *rb)
{
    assert(rb != NULL);

    rb->read_index   = 0;
    rb->read_mirror  = 0;
    rb->write_index  = 0;
    rb->write_mirror = 0;
}
