/**********************************************************************
 * Placed in the public domain by the author, Daniel Clouse, November 15, 2014.
 */
#include <pthread.h>
#include "frame_queue.h"

Frame_Queue::Frame_Queue(int max_size,
                         bool do_block_on_empty,
                         bool do_block_on_full)
{
    block_on_empty = do_block_on_empty;
    block_on_full = do_block_on_full;
    size = max_size;
    if (size < 1) size = 1;
    if (size >= MAX_QUEUE_SIZE) size = MAX_QUEUE_SIZE;
    head = 0;
    tail = 0;
    pthread_mutex_init(&mutex, NULL);
}

// on success returns 0, else -1; failure occurs if the queue is full.
int Frame_Queue::push(Usb_Frame* frame_ptr)
{
    pthread_mutex_lock(&mutex);

    ptr[tail] = frame_ptr;
    int new_tail = tail + 1;
    if (new_tail == size + 1) new_tail = 0;
    if (head == new_tail) {
        if (block_on_full) {
            pthread_cond_wait(&full_cond, &mutex);
        } else {
            pthread_mutex_unlock(&mutex);
            return -1;  // would overflow
        }
    }
    if (block_on_empty && (head == tail)) {
        /* The queue was empty.
           Tell any waiter that the queue is now not empty. */
        pthread_cond_signal(&empty_cond);
    }
    tail = new_tail;

    int count = get_item_count();
    pthread_mutex_unlock(&mutex);
    return count;
}

// Calculate the number of items now in the queue.
int Frame_Queue::get_item_count()
{
    int count = tail - head;
    if (count < 0) count += size + 1;
    return count;
}

// on success returns ptr to the popped item; on failure return NULL;
// failure occurs if the queue is empty.
Usb_Frame* Frame_Queue::pop(int& count)
{
    pthread_mutex_lock(&mutex);
    if (head == tail) {
        // queue is empty
        if (block_on_empty) {
            pthread_cond_wait(&empty_cond, &mutex);
        } else {
            pthread_mutex_unlock(&mutex);
            count = 0;
            return NULL;
        }
    }

    count = get_item_count();
    if (block_on_full && count == size) {

        /* Queue is currently full, but will soon have space for one item.
           Notify any waiter. */

        pthread_cond_signal(&full_cond);
    }

    Usb_Frame* return_value = ptr[head];
    if (++head == size + 1) head = 0;
    pthread_mutex_unlock(&mutex);
    --count;
    return return_value;
}
