//
// Created by sayan on 11/21/18.
//

#include "hclasses.h"

const char* HPAIR_TAGSTR = "PAIR";

#define HPAIR_NANO_SLEEP_TIME 100

/**
 * A bidirectional channel to be shared between 2 threads.
 * Used for sending and receiving messages (memory addresses) and signals
 * between threads.
 */
struct _hpair_t
{
    uint32_t tag;
    hqueue_t *queue[2];             ///< Concurrent queues, one for each direction
    gasnett_atomic_t endpoint[2];   ///< Endpoints to connect to
    gasnett_atomic_t signal[2];     ///< Atomic signals, one for each direction
};

hpair_t *hpair_new(int hwm_0, int hwm_1)
{
    hpair_t *self = hmalloc(hpair_t, 1);
    self->tag = *((uint32_t *)HPAIR_TAGSTR);

    self->queue[0] = hqueue_new(hwm_0);
    self->queue[1] = hqueue_new(hwm_1);

    gasnett_atomic_set(&self->endpoint[0], 0, GASNETT_ATOMIC_REL);
    gasnett_atomic_set(&self->endpoint[1], 0, GASNETT_ATOMIC_REL);

    gasnett_atomic_set(&self->signal[0], 0, GASNETT_ATOMIC_REL);
    gasnett_atomic_set(&self->signal[1], 0, GASNETT_ATOMIC_REL);

    return self;
}

void hpair_destroy(hpair_t **self_p)
{
    if (*self_p)
    {
        hpair_t *self = *self_p;
        if (self)
        {
            hqueue_destroy(&self->queue[0]);
            hqueue_destroy(&self->queue[1]);
            self->tag = 0xdeadbeef;
            hfree(self);
            *self_p = NULL;
        }
    }
}

bool hpair_is(void *self)
{
    assert(self);
    return ((hpair_t *) self)->tag == *((uint32_t *)HPAIR_TAGSTR);
}

int hpair_connect(hpair_t *self)
{
    assert(hpair_is(self));
    if (gasnett_atomic_compare_and_swap(&self->endpoint[0], 0, 1, GASNETT_ATOMIC_ACQ_IF_TRUE))
    {
        /// connected to endpoint 0
        return 0;
    }
    else if (gasnett_atomic_compare_and_swap(&self->endpoint[1], 0, 1, GASNETT_ATOMIC_ACQ_IF_TRUE))
    {
        /// connected to endpoint 1
        return 1;
    }
    else {
        LOG_ERR("More than 2 threads are trying to connect to a pair at once.");
    }
}

void hpair_disconnect(hpair_t *self, int endpoint)
{
    assert(hpair_is(self));

    /// TODO make sure the outgoing signal (if any) is received
    gasnett_atomic_set(&self->endpoint[endpoint], 0, GASNETT_ATOMIC_REL);
}

void hpair_send_0(hpair_t *self, void *msg)
{
    assert(hpair_is(self));
    do
    {
        bool ok = hqueue_try_push(self->queue[0], msg);
        if (ok) break;
            //else gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
            //else cpu_relax();
        else gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_spinloop_hint();
    }
    while(true);
}

void hpair_send_1(hpair_t *self, void *msg)
{
    assert(hpair_is(self));
    do
    {
        bool ok = hqueue_try_push(self->queue[1], msg);
        if (ok) break;
            //else gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
            //else cpu_relax();
        else gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_spinloop_hint();
    }
    while(true);
}

void hpair_recv_0(hpair_t *self, void **msg_p)
{
    assert(hpair_is(self));
    assert(msg_p);
    do
    {
        bool ok = hqueue_try_pop(self->queue[0], msg_p);
        if (ok) break;
            //else gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
            //else cpu_relax();
        else gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_spinloop_hint();
        gasnett_spinloop_hint();
    }
    while (true);
}

void hpair_recv_1(hpair_t *self, void **msg_p)
{
    assert(hpair_is(self));
    assert(msg_p);
    do
    {
        bool ok = hqueue_try_pop(self->queue[1], msg_p);
        if (ok) break;
            //else gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
            //else cpu_relax();
        else gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_spinloop_hint();
    }
    while (true);
}

bool hpair_peek_0(hpair_t *self, void **msg_p)
{
    assert(hpair_is(self));
    assert(msg_p);
    return hqueue_try_pop(self->queue[0], msg_p);
}

bool hpair_peek_1(hpair_t *self, void **msg_p)
{
    assert(hpair_is(self));
    assert(msg_p);
    return hqueue_try_pop(self->queue[1], msg_p);
}

void hpair_sgnl_0(hpair_t *self)
{
    assert(hpair_is(self));
    while (!gasnett_atomic_compare_and_swap(&self->signal[0], 0, 1, GASNETT_ATOMIC_ACQ_IF_TRUE)) {
        //cpu_relax(); //gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_spinloop_hint();
    }
}

void hpair_sgnl_1(hpair_t *self)
{
    assert(hpair_is(self));
    while (!gasnett_atomic_compare_and_swap(&self->signal[1], 0, 1, GASNETT_ATOMIC_ACQ_IF_TRUE)) {
        //cpu_relax(); //gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_spinloop_hint();
    }
}

void hpair_wait_0(hpair_t *self)
{
    assert(hpair_is(self));
    while (!gasnett_atomic_compare_and_swap(&self->signal[0], 1, 0, GASNETT_ATOMIC_ACQ_IF_TRUE)) {
        //cpu_relax(); //gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_spinloop_hint();
    }
}

void hpair_wait_1(hpair_t *self)
{
    assert(hpair_is(self));
    while (!gasnett_atomic_compare_and_swap(&self->signal[1], 1, 0, GASNETT_ATOMIC_ACQ_IF_TRUE)) {
        //cpu_relax(); //gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_nsleep(HPAIR_NANO_SLEEP_TIME);
        gasnett_spinloop_hint();
    }
}