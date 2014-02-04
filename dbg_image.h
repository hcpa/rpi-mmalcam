#ifndef DBG_IMAGE_H
#define DBG_IMAGE_H

#ifndef FFTW3_H
#include <fftw3.h>
#endif // FFTW3_H

#ifndef GPU_FFT_H
#include "gpu_fft/gpu_fft.h"
#endif // GPU_FFT_H

uint32_t y_int_save( uint8_t *data, uint32_t w, uint32_t h, char *name );
uint32_t y_float_save( float *data, uint32_t w, uint32_t h, char *name );
uint32_t y_complex_save( fftwf_complex *data, uint32_t real, uint32_t w, uint32_t h, char *name );
int fftwf_result_save( fftwf_complex *data, int spectrum, int phase_angle, int power_spectrum, uint32_t w, uint32_t h, char *basename );
int gpufft_result_save( struct GPU_FFT_COMPLEX *data, int step, int spectrum, int phase_angle, int power_spectrum, uint32_t w, uint32_t h, char *basename );

#endif // DBG_IMAGE_H