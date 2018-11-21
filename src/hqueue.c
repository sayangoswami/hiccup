//
// Created by sayan on 11/21/18.
// Adapted from http://moodycamel.com/blog/2013/a-fast-lock-free-queue-for-c++
//

#include "hclasses.h"

#define MEMORY_ORDER_RELAXED 0
#define MEMORY_ORDER_ACQUIRE GASNETT_ATOMIC_ACQ
#define MEMORY_ORDER_RELEASE GASNETT_ATOMIC_REL

const char* HQUEUE_TAGSTR = "QIEU";

struct _hqueue_t
{
    gasnett_atomic_t head;
    size_t local_tail;
    char cachelinefiller0[GASNETI_CACHE_LINE_BYTES - sizeof(gasnett_atomic_t) - sizeof(size_t)];

    gasnett_atomic_t tail;
    size_t local_head;
    char cachelinefiller1[GASNETI_CACHE_LINE_BYTES - sizeof(gasnett_atomic_t) - sizeof(size_t)];

    char *data;
    size_t size_mask;

    char *raw_self;

    uint32_t tag;
};

typedef void* item_t;

hqueue_t *hqueue_new(size_t capacity)
{
    /// Round off capacity to nearest power of 2 (round up)
    --capacity;
    capacity |= capacity >> 1;
    capacity |= capacity >> 2;
    capacity |= capacity >> 4;
    for (size_t i = 1; i < sizeof(size_t); i <<= 1) {
        capacity |= capacity >> (i << 3);
    }
    ++capacity;

    /// find alignments
    size_t alignof_hq = __alignof__(hqueue_t);
    size_t alignof_it = __alignof__(item_t);
    size_t size = sizeof(hqueue_t) + alignof_hq - 1;
    size += sizeof(item_t) * capacity + alignof_it - 1;

    /// allocate memory
    char *new_hq_raw = hmalloc(char, size);
    char *hq_ptr_aligned =
            new_hq_raw + (alignof_hq - ((uintptr_t)new_hq_raw % alignof_hq)) % alignof_hq;
    char *data_ptr = hq_ptr_aligned + sizeof(hqueue_t);
    char *data_ptr_aligned =
            data_ptr + (alignof_it - ((uintptr_t)data_ptr % alignof_it)) % alignof_it;

    /// fix references
    hqueue_t *self = (hqueue_t *)hq_ptr_aligned;
    self->tag = *((uint32_t *)HQUEUE_TAGSTR);
    self->data = data_ptr_aligned;
    self->raw_self = new_hq_raw;

    self->local_tail = 0;
    self->local_head = 0;

    self->size_mask = capacity - 1;

    /// perform a full fence (seq_cst)
    gasnett_atomic_set(&self->head, 0, GASNETT_ATOMIC_MB_PRE); // NOLINT
    gasnett_atomic_set(&self->tail, 0, GASNETT_ATOMIC_MB_POST); // NOLINT

    return self;
}

void hqueue_destroy(hqueue_t **self_p)
{
    if (self_p)
    {
        hqueue_t *self = *self_p;
        if (self)
        {
            self->tag = 0xdeadbeef;
            hfree(self->raw_self);
            *self_p = NULL;
        }
    }
}

bool hqueue_is(void *self)
{
    assert(self);
    return ((hqueue_t *)self)->tag == *((uint32_t *)HQUEUE_TAGSTR);
}

bool hqueue_try_push(hqueue_t *self, item_t item)
{
    assert(hqueue_is(self));
    size_t head = self->local_head;
    size_t tail = gasnett_atomic_read(&self->tail, MEMORY_ORDER_RELAXED);
    size_t next = (tail + 1) & self->size_mask;

    if (next != head || next != (self->local_head = gasnett_atomic_read(&self->head, MEMORY_ORDER_ACQUIRE)))
    {
        /// there is room for at least one more element
        char *location = self->data + tail * sizeof(item_t);
        memcpy(location, (char *)&item, sizeof(item_t));
        gasnett_atomic_set(&self->tail, next, MEMORY_ORDER_RELEASE);
        return true;
    }
    return false;
}

bool hqueue_try_pop(hqueue_t *self, item_t *item_p)
{
    assert(hqueue_is(self));
    assert(item_p);

    size_t tail = self->local_tail;
    size_t head = gasnett_atomic_read(&self->head, MEMORY_ORDER_RELAXED);

    if (head != tail || head != (self->local_tail = gasnett_atomic_read(&self->tail, MEMORY_ORDER_ACQUIRE)))
    {
        char *location = self->data + head * sizeof(item_t);
        memcpy((char *)item_p, location, sizeof(item_t));
        head = (head + 1) & self->size_mask;
        gasnett_atomic_set(&self->head, head, MEMORY_ORDER_RELEASE);
        return true;
    }
    return false;
}