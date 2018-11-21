//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HSOCK_H
#define HICCUP_HSOCK_H

hsock_t *hsock_new(hctx_t *ctx);

void hsock_destroy(hsock_t **self_p);

bool hsock_is(void *self);

bool hsock_bind(hsock_t *self, uint16_t endpoint);

bool hsock_connect(hsock_t *self, uint16_t node, uint16_t endpoint);

void hsock_unbind(hsock_t *self);

void hsock_disconnect(hsock_t *self);

void hsock_send(hsock_t *self, const char *picture, ...);

void hsock_recv(hsock_t *self, const char *picture, ...);

bool hsock_peek(hsock_t *self, const char *picture, ...);

#endif //HICCUP_HSOCK_H
