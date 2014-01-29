#ifndef FFT_H
#define FFT_H

#ifndef FFTW3_H
#include <fftw3.h>
#endif // FFTW3_H

typedef struct {
	uint32_t width;
	uint32_t height;
	float *data;
} hc_fpix_t;

int32_t
pixPhaseCorrelation(hc_fpix_t       *pixr,
					hc_fpix_t       *pixs,
					float 			*ppeak,
					int32_t   		*pxloc,
					int32_t   		*pyloc);


hc_fpix_t *fpixInverseDFT(fftwf_complex *dft, int32_t w, int32_t h);
fftwf_complex *fpixDFT(hc_fpix_t *dpix);


#endif