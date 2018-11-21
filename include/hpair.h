//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HPAIR_H
#define HICCUP_HPAIR_H

/**
 * Creates a new pair object consisting of two threadsafe queues.
 * @param hwm_0
 * @param hwm_1
 * @return
 */
hpair_t *hpair_new(int hwm_0, int hwm_1);

void hpair_destroy(hpair_t **self_p);

bool hpair_is(void *self);

int hpair_connect(hpair_t *self);

void hpair_disconnect(hpair_t *self, int endpoint);

void hpair_send_0(hpair_t *self, void *msg);

void hpair_send_1(hpair_t *self, void *msg);

void hpair_recv_0(hpair_t *self, void **msg_p);

void hpair_recv_1(hpair_t *self, void **msg_p);

bool hpair_peek_0(hpair_t *self, void **msg_p);

bool hpair_peek_1(hpair_t *self, void **msg_p);

void hpair_sgnl_0(hpair_t *self);

void hpair_sgnl_1(hpair_t *self);

void hpair_wait_0(hpair_t *self);

void hpair_wait_1(hpair_t *self);

#endif //HICCUP_HPAIR_H
