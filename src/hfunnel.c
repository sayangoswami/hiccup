//
// Created by sayan on 11/21/18.
// adapted from http://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
//

#include "hclasses.h"

const char* HFUNNEL_TAGSTR = "FNNL";

struct _fnode_t
{
    gasnett_atomic64_t next_ptr;
    void *value;
}
    __attribute__((__packed__));

typedef struct _fnode_t fnode_t;

struct _hfunnel_t
{
    uint32_t tag;
    fnode_t *stub;
    gasnett_atomic64_t head_ptr;
    gasnett_atomic64_t tail_ptr;
};

hfunnel_t *hfunnel_new()
{
    hfunnel_t *self = hmalloc(hfunnel_t, 1);
    self->tag = *((uint32_t *)HFUNNEL_TAGSTR);

    self->stub = hmalloc(fnode_t, 1);

    /// stub->next = NULL;
    gasnett_atomic64_set(&self->stub->next_ptr, 0, GASNETT_ATOMIC_ACQ | GASNETT_ATOMIC_REL);

    /// head = stub, tail = stub
    gasnett_atomic64_set(&self->head_ptr, (uintptr_t)self->stub, GASNETT_ATOMIC_ACQ | GASNETT_ATOMIC_REL);
    gasnett_atomic64_set(&self->tail_ptr, (uintptr_t)self->stub, GASNETT_ATOMIC_ACQ | GASNETT_ATOMIC_REL);

    return self;
}

void hfunnel_destroy(hfunnel_t **self_p)
{
    if (*self_p)
    {
        hfunnel_t *self = *self_p;
        if (self)
        {
            self->tag = 0xdeadbeef;
            hfree(self);
            *self_p = NULL;
        }
    }
}

bool hfunnel_is(void *self)
{
    assert(self);
    return ((hfunnel_t *)self)->tag == *((uint32_t *)HFUNNEL_TAGSTR);
}

void hfunnel_push(hfunnel_t *self, void *value)
{
    assert(hfunnel_is(self));
    fnode_t *node = hmalloc(fnode_t, 1);
    node->value = value;

    /// node->next = NULL; memory order = relaxed
    gasnett_atomic64_set(&node->next_ptr, 0, 0);

    /// Node *prev = XCHG(self->tail, node); memory order = acq_rel
    uintptr_t node_ptr = (uintptr_t) node;
    uintptr_t prev_ptr = gasnett_atomic64_swap(&self->tail_ptr, node_ptr, GASNETT_ATOMIC_ACQ | GASNETT_ATOMIC_REL);
    fnode_t *prev = (fnode_t *) prev_ptr;

    /// prev->next = node; memory order = release
    gasnett_atomic64_set(&prev->next_ptr, node_ptr, GASNETT_ATOMIC_REL);
}

void *hfunnel_pop(hfunnel_t *self)
{
    assert(hfunnel_is(self));

    /// Node *head = self->head; memory order = relaxed
    uintptr_t head_ptr = gasnett_atomic64_read(&self->head_ptr, 0);
    fnode_t *head = (fnode_t *) head_ptr;

    /// Node *next = head->next; memory order = acquire
    uintptr_t next_ptr = gasnett_atomic64_read(&head->next_ptr, GASNETT_ATOMIC_ACQ);

    if (next_ptr)
    {
        /// head = next; memory order = relaxed
        gasnett_atomic64_set(&self->head_ptr, next_ptr, 0);

        fnode_t *next = (fnode_t *) next_ptr;
        void *value = next->value;
        hfree(head);
        return value;
    }
    return NULL;
}