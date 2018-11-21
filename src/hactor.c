//
// Created by sayan on 11/21/18.
//

#include "hclasses.h"

#define HACTOR_HWM 512

const char* HACTOR_TAGSTR = "ACTR";

struct _hactor_t
{
    uint32_t tag;
    hpair_t *pair;
    hpipe_t *pipe;
    hpipe_t *chldpipe;
};

typedef struct
{
    hactor_fn *handler;
    hpipe_t *prntpipe;
    void *args;
}
        shim_t;

static void *run(void *args)
{
    assert(args);
    shim_t *shim = (shim_t *)args;
    hpipe_t *pipe = shim->prntpipe; //hpipe_new(shim->pair);
    shim->handler(pipe, shim->args);
    hpipe_sgnl(pipe);
    //hpipe_destroy(&pipe);
    hfree(shim);
    return NULL;
}

extern void hpipe_vsend(hpipe_t *self, const char *picture, va_list argptr);
extern void hpipe_vrecv(hpipe_t *self, const char *picture, va_list argptr);
extern bool hpipe_vpeek(hpipe_t *self, const char *picture, va_list argptr);

hactor_t *hactor_new(hactor_fn action, void *args)
{
    hactor_t *self = hmalloc(hactor_t, 1);
    self->tag = *((uint32_t *)HACTOR_TAGSTR);
    self->pair = hpair_new(HACTOR_HWM, HACTOR_HWM);
    self->pipe = hpipe_new(self->pair);

    self->chldpipe = hpipe_new(self->pair);
    shim_t *shim = hmalloc(shim_t, 1);
    shim->prntpipe = self->chldpipe; //shim->pair = self->pair;
    shim->args = args;
    shim->handler = action;

    pthread_t thread;
    pthread_create(&thread, NULL, run, shim);
    pthread_detach(thread);

    hpipe_wait(self->pipe);
    return self;
}

void hactor_destroy(hactor_t **self_p)
{
    if (self_p)
    {
        if (*self_p)
        {
            hactor_t *self = *self_p;
            hpipe_wait(self->pipe);
            hpipe_destroy(&self->pipe);
            hpipe_destroy(&self->chldpipe);
            hpair_destroy(&self->pair);
            self->tag = 0xdeadbeef;
            hfree(self);
            *self_p = NULL;
        }
    }
}

bool hactor_is(void *self)
{
    assert(self);
    return ((hactor_t *)self)->tag == *((uint32_t *)HACTOR_TAGSTR);
}

void hactor_send(hactor_t *self, const char *picture, ...)
{
    assert(hactor_is(self));
    va_list argptr;
    va_start(argptr, picture);
    hpipe_vsend(self->pipe, picture, argptr);
    va_end(argptr);
}

void hactor_recv(hactor_t *self, const char *picture, ...)
{
    assert(hactor_is(self));
    va_list argptr;
    va_start(argptr, picture);
    hpipe_vrecv(self->pipe, picture, argptr);
    va_end(argptr);
}

bool hactor_peek(hactor_t *self, const char *picture, ...)
{
    assert(hactor_is(self));
    va_list argptr;
    va_start(argptr, picture);
    bool exists = hpipe_vpeek(self->pipe, picture, argptr);
    va_end(argptr);
    return exists;
}

void hactor_wait(hactor_t *self)
{
    assert(hactor_is(self));
    hpipe_wait(self->pipe);
}

void hactor_sgnl(hactor_t *self)
{
    assert(hactor_is(self));
    hpipe_sgnl(self->pipe);
}