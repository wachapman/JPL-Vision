/**********************************************************************
 * Placed in the public domain by the author, Daniel Clouse, November 15, 2014.
 */
#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include <stdbool.h>
#include "any_frame_queue.h"

/******************************************************************//**
 * @brief Implements a queue of frames.
 *
 * Pointers to frames are actually stored on the queue, but whoever
 * holds the pointer is assumed to have exclusive read/write control
 * of the frame itself.
 */
class Frame_Queue: public Any_Frame_Queue {
private:
    /** The maximum number of items that will fit in any queue. */
    static const int MAX_QUEUE_SIZE = 32;

    /** True indicates that if the queue is empty, pop() will block until a
        new item is available.  False indicates that if the queue is empty,
        pop() will return NULL immediately. */
    bool block_on_empty;

    /** True indicates that if the queue is full, push() will block until
        space is made available.  False indicates that if the queue is full,
        push will return immediately with an error. */
    bool block_on_full;

    /** The maximum number of items that will fit in this queue, and the
        size of the used portion of the ptr array. */
    int size;
    int head; /// Index in ptr array of the head of the queue.
    int tail; /// Index in ptr array of the tail of the queue.
    Usb_Frame* ptr[MAX_QUEUE_SIZE + 1];  /// Contains all items on the queue

    /** Before accessing any field of this class, a function must first acquire
        exclusive access by holding this mutex.  See pthread_mutex_lock(3). */
    pthread_mutex_t mutex;

    /** Implements a signal sent to a thread waiting for an empty queue to
        become non-empty. */
    pthread_cond_t empty_cond;

    /** Implements a signal sent to a thread waiting for a full queue to
        acquire some free space. */
    pthread_cond_t full_cond;

    /******************************************************************//**
     * @brief Return the current count of items in the queue.
     */
    int get_item_count();

public:


    /******************************************************************//**
     * @brief Construct a new Frame_Queue.
     *
     * @param [in] max_size        The maximum number of items the queue will
     *                             be able to hold.
     * @param [in] block_on_empty  True indicates that if the queue is empty,
     *                             pop() will block until a new item is
     *                             available.
     * @param [in] block_on_full   True indicates that if the queue is full,
     *                             push() will block until space is made
     *                             available.
     */
    Frame_Queue(int max_size = 1,
                bool block_on_empty = true,
                bool block_on_full = true);


    /******************************************************************//**
     * @brief Push a new item onto the queue.
     *
     * If the queue is already full, the behavior depends on the setting of
     * the block_on_full argument to the constructor.  If block_on_full is
     * true, this routine blocks until space is available for the new item in
     * the queue.  If block_on_full is false, this routine does not block, but
     * rather returns without changing the queue.  This condition is indicated
     * by a return value of -1.
     *
     * @param [in] frame_ptr  The item to push.
     * @return On success the number of items now on the queue; -1 on failure.
     *         Failure occurs if the queue is full.  In this case, the queue
     *         is left unchanged.
     */
    virtual int push(Usb_Frame* frame_ptr);

    /******************************************************************//**
     * @brief Pop the next item from the front of the queue.
     *
     * If the queue is emtpy, the behavior depends on the the setting of the
     * block_on_empty argument to the constructor.  If block_on_empty is true,
     * this routine blocks until someone pushes a new item onto the queue; it
     * then pops the new item.  If block_on_empty is false, this routine
     * returns immediately without changing the queue.  This is indicated by a
     * return value of NULL.
     *
     * @param [out] count  Returns the number of items on the queue after the
     *                     pop operation.
     *
     * @return If block_on_empty is true and the queue is empty, NULL is
     *         returned, and the queue is left unchanged.  Otherwise, the item
     *         at the front of the queue is removed from the queue, and a
     *         non-NULL pointer to the item is returned.
     */
    virtual Usb_Frame* pop(int& count);
};

#endif
