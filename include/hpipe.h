//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HPIPE_H
#define HICCUP_HPIPE_H

hpipe_t *hpipe_new(hpair_t *pair);

void hpipe_destroy(hpipe_t **self_p);

bool hpipe_is(void *self);

void hpipe_send(hpipe_t *self, const char *picture, ...);

void hpipe_recv(hpipe_t *self, const char *picture, ...);

bool hpipe_peek(hpipe_t *self, const char *picture, ...);

void hpipe_wait(hpipe_t *self);

void hpipe_sgnl(hpipe_t *self);

#endif //HICCUP_HPIPE_H
