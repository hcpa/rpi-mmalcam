#ifndef FFT_GPU_H
#define FFT_GPU_H

#include "gpu_fft/mailbox.h"
#include "gpu_fft/gpu_fft.h"

struct GPU_FFT *pixDFT_GPU( pix_y_t *pic );
int pixPhaseCorrelate_GPU( pix_y_t *pixr, pix_y_t *pixs, float *ppeak, int *px, int *py );

void free_fft_gpu( struct GPU_FFT *fft_frame1_gpu );



#endif /* FFT_GPU_H */