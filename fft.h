#ifndef FFT_H
#define FFT_H

#ifndef FFTW3_H
#include <fftw3.h>
#endif // FFTW3_H

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

int32_t
pixPhaseCorrelation(fpix_y_t       *pixr,
					fpix_y_t       *pixs,
					float 			*ppeak,
					int32_t   		*pxloc,
					int32_t   		*pyloc);


fpix_y_t *fpixInverseDFT(fftwf_complex *dft, int32_t w, int32_t h);
fftwf_complex *fpixDFT(fpix_y_t *dpix);
fftwf_complex *pixDFT(pix_y_t *pixs);

fpix_y_t *pixConvertToFPix(pix_y_t *pixs );
pix_y_t *fpixConvertToPix( fpix_y_t *fpixs );

void fpixDestroy( fpix_y_t *fpix );
void pixDestroy( pix_y_t *fpix );

#endif