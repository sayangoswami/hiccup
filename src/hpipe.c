//
// Created by sayan on 11/21/18.
//

#include "hclasses.h"
#include "hmsg.h"

typedef void (*send_fnptr)(hpair_t*, void*);
typedef void (*recv_fnptr)(hpair_t*, void**);
typedef bool (*poll_fnptr)(hpair_t*, void**);
typedef void (*sync_fnptr)(hpair_t*);

const char* HPIPE_TAGSTR = "PIPE";

struct _hpipe_t
{
    hpair_t *pair;
    int endpoint;
    uint32_t tag;
    send_fnptr send_fn;
    recv_fnptr recv_fn;
    poll_fnptr peek_fn;
    sync_fnptr wait_fn;
    sync_fnptr sgnl_fn;
};

hpipe_t *hpipe_new(hpair_t *pair)
{
    if (!hpair_is(pair))
        LOG_ERR("Input argument is not a pair.");
    hpipe_t *self = hmalloc(hpipe_t, 1);
    self->tag = *((uint32_t *)HPIPE_TAGSTR);
    self->pair = pair;
    self->endpoint = hpair_connect(pair);
    switch (self->endpoint)
    {
        case 0:
            /// Send through pipe 0 and receive from pipe 1
            self->send_fn = &hpair_send_0;
            self->recv_fn = &hpair_recv_1;
            self->peek_fn = &hpair_peek_1;

            /// Signal on pipe 0 and wait on pipe 1
            self->sgnl_fn = &hpair_sgnl_0;
            self->wait_fn = &hpair_wait_1;
            break;
        case 1:
            /// Send through pipe 1 and receive from pipe 0
            self->send_fn = &hpair_send_1;
            self->recv_fn = &hpair_recv_0;
            self->peek_fn = &hpair_peek_0;

            /// Signal on pipe 1 and wait on pipe 0
            self->sgnl_fn = &hpair_sgnl_1;
            self->wait_fn = &hpair_wait_0;
            break;
        default:
            LOG_ERR("This should never have happened. Endpoint = %d", self->endpoint);
    }

    return self;
}

void hpipe_destroy(hpipe_t **self_p)
{
    assert(self_p);
    if (*self_p)
    {
        hpipe_t *self = *self_p;
        if (self)
        {
            // TODO What happens to the data in the channel is there's any?
            hpair_disconnect(self->pair, self->endpoint);
            self->tag = 0xdeadbeef;
            hfree(self);
            *self_p = NULL;
        }
    }
}

bool hpipe_is(void *self)
{
    assert(self);
    return ((hpipe_t*)self)->tag == *((uint32_t *)HPIPE_TAGSTR);
}

void hpipe_vsend(hpipe_t *self, const char *picture, va_list argptr)
{
    assert(hpipe_is(self));
    hmsg_t *msg = hmsg_new();
    hmsg_vset(msg, picture, argptr);
    (*self->send_fn)(self->pair, msg);
}

void hpipe_send(hpipe_t *self, const char *picture, ...)
{
    assert(hpipe_is(self));
    va_list argptr;
    va_start(argptr, picture);
    hpipe_vsend(self, picture, argptr);
    va_end(argptr);
}

void hpipe_vrecv(hpipe_t *self, const char *picture, va_list argptr)
{
    assert(hpipe_is(self));
    hmsg_t *msg;
    (*self->recv_fn)(self->pair, (void**)&msg);
    assert(msg);
    hmsg_vget(msg, picture, argptr);
    hmsg_destroy(&msg);
}

void hpipe_recv(hpipe_t *self, const char *picture, ...)
{
    assert(hpipe_is(self));
    va_list argptr;
    va_start(argptr, picture);
    hpipe_vrecv(self, picture, argptr);
    va_end(argptr);
}

bool hpipe_vpeek(hpipe_t *self, const char *picture, va_list argptr)
{
    assert(hpipe_is(self));
    hmsg_t *msg;
    bool exists = (*self->peek_fn)(self->pair, (void **)&msg);
    if (exists)
    {
        hmsg_vget(msg, picture, argptr);
        hmsg_destroy(&msg);
    }
    return exists;
}

bool hpipe_peek(hpipe_t *self, const char *picture, ...)
{
    assert(hpipe_is(self));
    va_list argptr;
    va_start(argptr, picture);
    bool exists = hpipe_vpeek(self, picture, argptr);
    va_end(argptr);
    return exists;
}

void hpipe_wait(hpipe_t *self)
{
    assert(hpipe_is(self));
    (*self->wait_fn)(self->pair);
}

void hpipe_sgnl(hpipe_t *self)
{
    assert(hpipe_is(self));
    (*self->sgnl_fn)(self->pair);
}