#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

#include "mmalyuv.h"
#include "log.h"
#include "fft_gpu.h"

#include "gpu_fft/mailbox.h"
#include "gpu_fft/gpu_fft.h"

static int mb = -1;

struct GPU_FFT *pixDFT_GPU( pix_y_t *pic )
{
    int i, j, ret, log2_N;
    struct GPU_FFT_COMPLEX *base, *trans;
    struct GPU_FFT *fft1, *fft2;
	uint8_t *picdata;
	
	if( mb < 0 )
	{
		mb = mbox_open();
		if( mb < 0 ){
			ERROR( "cannot open mailbox" );
			return (NULL);
		}
	}


	log2_N = (int)round(log2(pic->width));
	// TODO: check if width is a power of 2

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pic->height, &fft1); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return NULL;
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return NULL;
        case -3: ERROR("Out of memory round1.  Try a smaller batch or increase GPU memory.\n");  return NULL;
    }

	for( j=0; j < pic->height; j++ )
	{
		base = fft1->in + j*fft1->step; // input buffer
		picdata = pic->data + j*pic->width;
        for( i=0; i<pic->width ; i++)
		{
			base[i].re = *(picdata++);
			base[i].im = 0;
		}
    }

	usleep(1); // Yield to OS
	gpu_fft_execute(fft1); // call one or many times



	log2_N = (int)round(log2(pic->height));
	// TODO: check if width is a power of 2

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pic->width, &fft2); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return NULL;
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return NULL;
        case -3: ERROR("Out of memory round 2.  Try a smaller batch or increase GPU memory.\n");  return NULL;
    }

	trans = fft2->in;
	for( j=0; j < pic->height; j++ )
	{
		base = fft1->out + j*fft1->step; // output buffer 1st fft becomes input buffer 2nd
		
        for( i=0; i<pic->width ; i++)
		{
			trans = fft2->in + i*fft2->step;
			trans[j].re = base[i].re;
			trans[j].im = base[i].im;
		}
    }
	gpu_fft_release( fft1 );

	usleep(1); // Yield to OS
	gpu_fft_execute(fft2); // call one or many times

	// TODO KACKE, ich muss den Scheiß ja noch zurücktransponieren. Wird es wohl echt langsamer werden...
	// Oder ich überlege, ob ich das wirklich muss 

    return fft2;

}

void free_fft_gpu( struct GPU_FFT *fft )
{
    gpu_fft_release(fft); // Videocore memory lost if not freed !	
}

/*
pixPhaseCorrelate( pixr, pixs, &peak, &x, &y )

	r = pixDFT_GPU( pixr ) --> do not transpose back!
	s = pixDFT_GPU( pixs ) --> do not transpose back!

	calculate cross-power spectrum
		o_{i,j} = sqrt((re(s_{j,i})*re(r_{j,i}) - im(s_{j,i})*-im(r__{j,i}))^2 + (re(s_{j,i})*-im(r_{j,i}) - im(s_{j,i})*re(r__{j,i}))^2)
	
	p = InverseDFT_GPU( o );

	identify peak, x, y

	clean up

*/