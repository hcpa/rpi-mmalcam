#ifndef MMALYUV_H
#define MMALYUV_H

#define MMAL_CAMERA_CAPTURE_PORT 2

#define MAX_CAM_WIDTH 2592
#define MAX_CAM_HEIGHT 1944

// TODO: WTF does this stand for?
#define STILLS_FRAME_RATE_NUM 3
#define STILLS_FRAME_RATE_DEN 1

typedef struct
{
   FILE *file_handle;                   /// File handle to write buffer data to.
   VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
   MMAL_POOL_T *encoder_pool;
} PORT_USERDATA;


#endif // MMALYUV