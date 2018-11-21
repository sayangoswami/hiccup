//
// Created by sayan on 11/21/18.
//

#ifndef HICCUP_HQUEUE_H
#define HICCUP_HQUEUE_H

/**
 * Creates a new thread-safe single-producer single-consumer queue.
 * Its assumed that the producer and consumer threads will not switch roles, i.e., data flows only in one direction.
 * Both of these are user responsibilities and the results are undefined when they are not fulfilled.
 *
 * @param capacity - this is a suggestion. the actual capacity will be at least this.
 * @return Handle to the newly created queue.
 */
hqueue_t *hqueue_new(size_t capacity);

/**
 * Destroys the queue.
 *
 * @param self_p - address of queue handle
 */
void hqueue_destroy(hqueue_t **self_p);

bool hqueue_is(void *self);

/**
 * Tries to push an item at the tail of the queue.
 *
 * @param self - queue handle
 * @param item - pointer to the item being pushed
 * @return True if the push was successful, false if the queue is full
 */
bool hqueue_try_push(hqueue_t *self, void *item);

/**
 * Tries to pop an item from the head of the queue.
 *
 * @param self - queue handle
 * @param item_p - location where the pointer to the item popped will be stored
 * @return True if the pop was successful, false if the queue was empty.
 */
bool hqueue_try_pop(hqueue_t *self, void **item_p);

#endif //HICCUP_HQUEUE_H
