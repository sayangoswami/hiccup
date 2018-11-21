//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HMSG_H
#define HICCUP_HMSG_H

hmsg_t *hmsg_new();

void hmsg_destroy(hmsg_t **self_p);

bool hmsg_is(void *self);

void hmsg_set(hmsg_t *self, const char *picture, ...);

void hmsg_vset(hmsg_t *self, const char *picture, va_list argptr);

void hmsg_get(hmsg_t *self, const char *picture, ...);

void hmsg_vget(hmsg_t *self, const char *picture, va_list argptr);

void hmsg_picture(hmsg_t *self, void **data_p, size_t *size_p);

void hmsg_payload(hmsg_t *self, void **data_p, size_t *size_p);

/**
 * Sets the arguments in the message using the frame provided
 *
 * @param self
 * @param frame
 */
void hmsg_set_picture(hmsg_t *self, void *data, size_t size);

/**
 * Sets the payload in the message using the frame provided
 *
 * @param self
 * @param frame
 */
void hmsg_set_payload(hmsg_t *self, void *data, size_t size);

#endif //HICCUP_HMSG_H
