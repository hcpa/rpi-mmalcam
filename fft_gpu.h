#ifndef FFT_GPU_H
#define FFT_GPU_H

#include "gpu_fft/mailbox.h"
#include "gpu_fft/gpu_fft.h"

struct GPU_FFT *pixDFT_GPU( pix_y_t *pic );

void free_fft_gpu( struct GPU_FFT *fft_frame1_gpu );



#endif /* FFT_GPU_H */