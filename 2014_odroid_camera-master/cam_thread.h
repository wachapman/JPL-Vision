/**********************************************************************
 * Placed in the public domain by the author, Daniel Clouse, November 15, 2014.
 */
#ifndef CAM_THREAD_H
#define CAM_THREAD_H

#include "usb_camera.h"

/**********************************************************************
 * @brief Camera thread.  One thread per camera.
 *
 * @param [in,out] thread_arg_ptr Points to the single arugment to this
 *                                thread.  See pthread_create(3).  The caller
 *                                must point this at Usb_Camera that has
 *                                already been initialized via a call to
 *                                Usb_Camera::init().
 * @return Return value is meaningless.
 */
void* cam_thread(void* thread_arg_ptr);

#endif
