#ifndef MMALYUV_H
#define MMALYUV_H

#include <stdint.h>
#include <interface/mmal/mmal.h>


#define MMAL_CAMERA_CAPTURE_PORT 2

// #define MAX_CAM_WIDTH 2592
// #define MAX_CAM_WIDTH_PADDED 2592
// #define MAX_CAM_HEIGHT 1944
// #define MAX_CAM_HEIGHT_PADDED 1952

#define MAX_CAM_WIDTH 1024
#define MAX_CAM_WIDTH_PADDED 1024
#define MAX_CAM_HEIGHT 1024
#define MAX_CAM_HEIGHT_PADDED 1024

#define DBG_PAD_X 128
#define DBG_PAD_Y 128


#define MAX_FRAMES 3 



// TODO: WTF does this stand for?
#define STILLS_FRAME_RATE_NUM 3
#define STILLS_FRAME_RATE_DEN 1

typedef struct
{
   void *image_buffer;			/// File handle to write buffer data to
   uint32_t max_bytes;
   uint32_t bytes_written;
   uint32_t current_frame;
   VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
   MMAL_POOL_T *camera_pool;
} PORT_USERDATA;

typedef struct {
	uint32_t width;
	uint32_t height;
	float *data;
} fpix_y_t;

typedef struct {
	uint32_t width;
	uint32_t height;
	uint8_t *data;
} pix_y_t;



#endif // MMALYUV