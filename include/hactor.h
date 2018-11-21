//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HACTOR_H
#define HICCUP_HACTOR_H

typedef void (hactor_fn) (hpipe_t *pipe, void *args);

/**
 * Create a new actor
 * @param[in] action - actor function
 * @param[in] args - pointer to arguments
 * @return A new actor object
 */
hactor_t *hactor_new(hactor_fn action, void *args);

/**
 * Destroy the actor
 * @param[in,out] self_p - address of the actor object
 */
void hactor_destroy(hactor_t **self_p);

/**
 * Verify object type
 * @param[in] self - object
 * @return True if object is an actor
 */
bool hactor_is(void *self);

void hactor_send(hactor_t *self, const char *picture, ...);

void hactor_recv(hactor_t *self, const char *picture, ...);

bool hactor_peek(hactor_t *self, const char *picture, ...);

void hactor_wait(hactor_t *self);

void hactor_sgnl(hactor_t *self);

#endif //HICCUP_HACTOR_H
