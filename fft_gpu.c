#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

#include <time.h>

#include "mmalyuv.h"
#include "log.h"
#include "fft_gpu.h"

#include "gpu_fft/mailbox.h"
#include "gpu_fft/gpu_fft.h"

static int mb = -1;

static long millis()
{
	struct timespec tt;
	clock_gettime(CLOCK_MONOTONIC,&tt);
	return tt.tv_sec*1000l + tt.tv_nsec/1000000;
}

struct GPU_FFT *pixDFT_GPU( pix_y_t *pic )
{
    int i, j, ret, log2_N;
    struct GPU_FFT_COMPLEX *base, *trans;
    struct GPU_FFT *fft1, *fft2;
	uint8_t *picdata;
	long bef,aft;
	
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

	bef = millis();
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
	aft = millis();
	MSG("int->complex %ld millis",aft-bef);

	usleep(1); // Yield to OS
	bef = aft;
	gpu_fft_execute(fft1); // call one or many times
	aft = millis();
	MSG("fft horiz %ld millis",aft-bef);
	bef=aft;

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
		base = fft1->out + j* fft1->step; // output buffer 1st fft becomes input buffer 2nd
		
		for( i=0; i<pic->width ; i++)
		{
			trans = fft2->in + i*fft2->step;
			trans[j] = base[i];
		}
	}
	aft = millis();
	MSG("transpose %ld millis",aft-bef);
	bef = aft;

	gpu_fft_release( fft1 );

	usleep(1); // Yield to OS
	gpu_fft_execute(fft2); // call one or many times
	aft = millis();
	MSG("fft vert %ld millis",aft-bef);
	

	// TODO KACKE, ich muss den Scheiß ja noch zurücktransponieren. Wird es wohl echt langsamer werden...
	// Oder ich überlege, ob ich das wirklich muss 

	return fft2;

}

void free_fft_gpu( struct GPU_FFT *fft )
{
    gpu_fft_release(fft); // Videocore memory lost if not freed !	
}


int pixPhaseCorrelate( pix_y_t *pixr, pix_y_t *pixs, float *peak, int *x, int *y )
{
    int i, j, ret, log2_N;
    struct GPU_FFT_COMPLEX *base, *trans;
    struct GPU_FFT *fftr1, *fftr2;
    struct GPU_FFT *ffts1, *ffts2;
    struct GPU_FFT *ffto1, *ffto2;	
	uint8_t *picdata;

	if( mb < 0 )
	{
		mb = mbox_open();
		if( mb < 0 ){
			ERROR( "cannot open mailbox" );
			return -1;
		}
	}

	log2_N = (int)round(log2(pixr->width));


	// FFT pixr
	// 	r = pixDFT_GPU( pixr ) --> do not transpose back!

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pixr->height, &fftr1); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return -1;
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return -1;
        case -3: ERROR("Out of memory round1.  Try a smaller batch or increase GPU memory.\n");  return -1;
    }

	for( j=0; j < pixr->height; j++ )
	{
		base = fftr1->in + j*fftr1->step; // input buffer
		picdata = pixr->data + j*pixr->width;
        for( i=0; i<pixr->width ; i++)
		{
			base[i].re = *(picdata++);
			base[i].im = 0;
		}
    }

	usleep(1); // Yield to OS
	gpu_fft_execute(fftr1); // call one or many times


	log2_N = (int)round(log2(pixr->height));
	// TODO: check if width is a power of 2

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pixr->width, &fftr2); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return -1;
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return -1;
        case -3: ERROR("Out of memory round 2.  Try a smaller batch or increase GPU memory.\n");  return -1;
    }

	trans = fftr2->in;
	for( j=0; j < pixr->height; j++ )
	{
		base = fftr1->out + j* fftr1->step; // output buffer 1st fft becomes input buffer 2nd
		
        for( i=0; i<pixr->width ; i++)
		{
			trans = fftr2->in + i*fftr2->step;
			trans[j].re = base[i].re;
			trans[j].im = base[i].im;
		}
    }
	gpu_fft_release( fftr1 );

	usleep(1); // Yield to OS
	gpu_fft_execute(fftr2); // call one or many times


	// FFT pixs
	// s = pixDFT_GPU( pixs ) --> do not transpose back!
    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pixs->height, &ffts1); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return -1;
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return -1;
        case -3: ERROR("Out of memory round1.  Try a smaller batch or increase GPU memory.\n");  return -1;
    }

	for( j=0; j < pixs->height; j++ )
	{
		base = ffts1->in + j*ffts1->step; // input buffer
		picdata = pixs->data + j*pixs->width;
        for( i=0; i<pixs->width ; i++)
		{
			base[i].re = *(picdata++);
			base[i].im = 0;
		}
    }

	usleep(1); // Yield to OS
	gpu_fft_execute(ffts1); // call one or many times


	log2_N = (int)round(log2(pixs->height));
	// TODO: check if width is a power of 2

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pixs->width, &ffts2); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return -1;
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return -1;
        case -3: ERROR("Out of memory round 2.  Try a smaller batch or increase GPU memory.\n");  return -1;
    }

	trans = ffts2->in;
	for( j=0; j < pixs->height; j++ )
	{
		base = ffts1->out + j* ffts1->step; // output buffer 1st fft becomes input buffer 2nd
		
        for( i=0; i<pixs->width ; i++)
		{
			trans = ffts2->in + i*ffts2->step;
			trans[j].re = base[i].re;
			trans[j].im = base[i].im;
		}
    }
	gpu_fft_release( ffts1 );

	usleep(1); // Yield to OS
	gpu_fft_execute(ffts2); // call one or many times

	//////////////////////

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pixr->width, &ffto1); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return -1;
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return -1;
        case -3: ERROR("Out of memory round 2.  Try a smaller batch or increase GPU memory.\n");  return -1;
    }


	// calculate cross-power spectrum
	// 	o_{i,j} = sqrt((re(s_{j,i})*re(r_{j,i}) - im(s_{j,i})*-im(r__{j,i}))^2 + (re(s_{j,i})*-im(r_{j,i}) - im(s_{j,i})*re(r__{j,i}))^2)
	// 
	// p = InverseDFT_GPU( o );
	// 
	// identify peak, x, y
	// 
	// clean up
}
		