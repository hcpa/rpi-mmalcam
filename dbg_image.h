#ifndef DBG_IMAGE_H
#define DBG_IMAGE_H

uint32_t y_int_save( uint8_t *data, uint32_t w, uint32_t h, char *name );
uint32_t y_float_save( float *data, uint32_t w, uint32_t h, char *name );

#ifndef FFTW3_H
#include <fftw3.h>
#endif // FFTW3_H

uint32_t y_complex_save( fftwf_complex *data, uint32_t w, uint32_t h, char *name );

#endif // DBG_IMAGE_H