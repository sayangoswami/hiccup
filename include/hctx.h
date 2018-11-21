//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HCTX_H
#define HICCUP_HCTX_H

hctx_t *hctx_new(int argc, char **argv);

void hctx_destroy(hctx_t **self_p);

bool hctx_is(void *self);

int hctx_mynode(hctx_t *self);

int hctx_num_nodes(hctx_t *self);

size_t hctx_segsize(hctx_t *self);

void* hctx_segaddr(hctx_t *self);

#endif //HICCUP_HCTX_H
