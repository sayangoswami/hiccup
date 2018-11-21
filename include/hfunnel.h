//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HFUNNEL_H
#define HICCUP_HFUNNEL_H

hfunnel_t *hfunnel_new();

void hfunnel_destroy(hfunnel_t **self_p);

bool hfunnel_is(void *self);

void hfunnel_push(hfunnel_t *self, void *value);

void *hfunnel_pop(hfunnel_t *self);

#endif //HICCUP_HFUNNEL_H
