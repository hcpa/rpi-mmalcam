#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <values.h>

#include <time.h>

#include "mmalyuv.h"
#include "log.h"
#include "fft_gpu.h"
#include "dbg_image.h"

#include "gpu_fft/mailbox.h"
#include "gpu_fft/gpu_fft.h"

static int mb = -1;

static long millis()
{
	struct timespec tt;
	clock_gettime(CLOCK_MONOTONIC,&tt);
	return tt.tv_sec*1000l + tt.tv_nsec/1000000;
}

static void in_place_transpose_square( struct GPU_FFT_COMPLEX *data, int w, int step )
{
	struct GPU_FFT_COMPLEX temp;
	struct GPU_FFT_COMPLEX *base, *trans;
	int i, j;
	
	for( j = 0; j <= w-2; j++ )
		for( i = j+1; i <= w-1; i++ )
		{
			base = data + j*step + i;
			trans = data + i*step + j;
			temp = *base;
			*base = *trans;
			*trans = temp;
		}
	
}

static void in_place_transpose_square_left_half( struct GPU_FFT_COMPLEX *data, int w, int step )
{
	struct GPU_FFT_COMPLEX temp;
	struct GPU_FFT_COMPLEX *base, *trans;
	int i, j;
	
	// transpose upper left quadrant of matrix
	for( j = 0; j <= w/2-2; j++ )
		for( i = j+1; i <= w/2-1; i++ )
		{
			base = data + j*step + i;
			trans = data + i*step + j;
			temp = *base;
			*base = *trans;
			*trans = temp;
		}
	
	// move lover left quadrant transposed to upper right quadrant
	// erase (fill with 0) lower half of matrix
	for( j = w/2; j < w; j++ )
	{
		for( i = 0; i < w/2; i++ )
		{
			base = data+j*step+i;
			trans= data+i*step+j;
			*trans = *base;
		}
		memset(data+j*step,0,sizeof(struct GPU_FFT_COMPLEX)*w);
	}
}

static void in_place_transpose_square_upper_half( struct GPU_FFT_COMPLEX *data, int w, int step )
{
	struct GPU_FFT_COMPLEX temp;
	struct GPU_FFT_COMPLEX *base, *trans;
	int i, j;
	
	// transpose upper left quadrant of matrix
	for( j = 0; j <= w/2-2; j++ )
		for( i = j+1; i <= w/2-1; i++ )
		{
			base = data + j*step + i;
			trans = data + i*step + j;
			temp = *base;
			*base = *trans;
			*trans = temp;
		}
	
	// move upper right quadrant of matrix transposed to lower left
	// erase right half of all lines
	for( j = 0; j < w/2; j++ )
	{
		for( i = w/2; i < w; i++ )
		{
			base = data+j*step+i;
			trans= data+i*step+j;
			*trans = *base;
		}
		// erase upper right quadrant after copy
		memset( data +       j*step + w/2, 0, sizeof(struct GPU_FFT_COMPLEX)*w );
		// erase lower right quadrant -- is this necessary at all?
		memset( data + (w/2+j)*step + w/2,0, sizeof(struct GPU_FFT_COMPLEX)*w );
	}
}



static struct GPU_FFT *pixDFT_GPU_no_final_transpose( pix_y_t *pic )
{
    int i, j, ret, log2_N;
    struct GPU_FFT_COMPLEX *base;
    struct GPU_FFT *fft;
	uint8_t *picdata;
	long bef,aft;
	
	log2_N = (int)round(log2(pic->width));

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pic->height, &fft); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return NULL;
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return NULL;
        case -3: ERROR("Out of memory round1.  Try a smaller batch or increase GPU memory.");  return NULL;
        case -4: ERROR("Cannot open /dev/mem, must run as root.");  return NULL;
    }

	bef = millis();
	for( j=0; j < pic->height; j++ )
	{
		base = fft->in + j*fft->step; // input buffer
		picdata = pic->data + j*pic->width;
        for( i=0; i<pic->width ; i++,picdata++ )
		{
			base[i].re = *(picdata);
			base[i].im = 0;			
		}
    }
	aft = millis();
	DEBUG("byte->complex %ld millis",aft-bef);

	usleep(1); // Yield to OS
	bef = millis();

	gpu_fft_execute(fft); 

	aft = millis();
	DEBUG("fft horiz %ld millis",aft-bef);

	bef=millis();

	// In-place transposition of the result
	in_place_transpose_square_left_half( fft->out, pic->height, fft->step );	
	
	aft = millis();
	DEBUG("transpose %ld millis",aft-bef);
	bef = aft;

	usleep(1); // Yield to OS
	// this execution will work from fft->out back to fft->in
	// If we just transpose the left half of the matrix to the upper half
	// it's not necessary to perform the FFT on all rows. half of the rows
	// would be sufficient. but since gpu_fft is so fast, it's not really an issue.
	gpu_fft_execute(fft); // call one or many times
	aft = millis();
	DEBUG("fft vert %ld millis",aft-bef);
	

	return fft;

}

struct GPU_FFT *pixDFT_GPU( pix_y_t *pic )
{
	struct GPU_FFT *fft;
	long bef,aft;
	
	if( !pic )
	{
		ERROR("nothing to do");
		return NULL;
	}

	if( (pic->width != pic->height) ||
		(pic->width <= 0) ||
		(pic->width & (pic->width-1)) != 0)
	{
		ERROR( "as of now, this function only works on square images with a lenght greater 0 of a power of 2. %d x %d is not correct", pic->width, pic->height );
		return (NULL);
	}
	
	if( mb < 0 )
	{
		mb = mbox_open();
		if( mb < 0 ){
			ERROR( "cannot open mailbox" );
			return (NULL);
		}
	}
	
	fft = pixDFT_GPU_no_final_transpose( pic );
	if( !fft )
	{
		ERROR("pixDFT_GPU_no_final_transpose failed");
		return NULL;
	}
	
	// Transpose back
	bef = millis();
	in_place_transpose_square_upper_half( fft->in, pic->height, fft->step );
	aft = millis();
	DEBUG("2nd transpose %ld millis",aft-bef);

	return fft;
}


void free_fft_gpu( struct GPU_FFT *fft )
{
    gpu_fft_release(fft); // Videocore memory lost if not freed !	
}


int pixPhaseCorrelate_GPU( pix_y_t *pixr, pix_y_t *pixs, float *ppeak, int *px, int *py )
{
    int i, j, ret, log2_N;
    struct GPU_FFT_COMPLEX *base_r, *base_s, *base_i;
    struct GPU_FFT *fftr, *ffts, *ffti;
	long bef,aft;
	float maxval;
	int xmaxloc, ymaxloc;


	if( (pixr->width != pixr->height) ||
		(pixr->width != pixs->width)  ||
		(pixr->height!= pixs->height) ||
		(pixr->width <= 0) ||
		(pixr->width & (pixr->width-1)) != 0)
	{
		ERROR( "as of now, this function only works on square images with a lenght greater 0 of a power of 2.\n%d x %d (pixr) and %d x %d (pixs) is not correct", 
			   pixr->width, pixr->height, pixr->width, pixr->height );
		return (-1);
	}
	
	if( !ppeak || !px || !py )
	{
		ERROR("missing parameter: ppeak:%ld px:%ld py:%ld", ppeak, px, py );
		return (-1);
	}
	
	if( mb < 0 )
	{
		mb = mbox_open();
		if( mb < 0 ){
			ERROR( "cannot open mailbox" );
			return (-1);
		}
	}


	// FFT pixr
	fftr = pixDFT_GPU_no_final_transpose( pixr );
	if( !fftr )
	{
		ERROR("pixDFT_GPU_no_final_transpose failed");
		return (-1);
	}
	// RESULT IS NOW TRANSPOSED IN fftr->in 

	
	// FFT pixs
	ffts = pixDFT_GPU_no_final_transpose( pixs );
	if( !ffts )
	{
		ERROR("pixDFT_GPU_no_final_transpose failed");
		return (-1);
	}
	// RESULT IS NOW TRANSPOSED IN ffts->in 


	log2_N = (int)round(log2(pixr->width));

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_REV, pixr->height, &ffti); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return (-1);
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return (-1);
        case -3: ERROR("Out of memory round1.  Try a smaller batch or increase GPU memory.");  return (-1);
        case -4: ERROR("Cannot open /dev/mem, must run as root.");  return (-1);
    }


	bef = millis();

	// calculate cross-power spectrum
	// 	o_{i,j} = sqrt((re(s_{i,j})*re(r_{i,j}) - im(s_{i,j})*-im(r__{i,j}))^2 + (re(s_{i,j})*-im(r_{i,j}) - im(s_{i,j})*re(r__{i,j}))^2)
	// 
	// TODO Why is the cross power spectrum in fft.c 30% faster?
	for( j = 0; j < pixr->width/2; j++ )
	{
		base_r = fftr->in + j*fftr->step;
		base_s = ffts->in + j*ffts->step;
		base_i = ffti->in + j*ffti->step;
		for( i = 0; i < pixr->height; i++,base_r++,base_s++,base_i++ )
		{
			float ac, bd, bc, ad, r;
			
			// multiply element in reference matrix and element at same position complex-conjugated shifted matrix
			// (a+bi)(c-di) = (ac+bd) + (bc-ad)i
			// 
			// normalize result by division by absolute value of product of both elements (non-congugated)
			// |(a+bi)(c+di)| = |(ac-bd) + (bc+ad)i| = sqrt( (ac-bd)^2 + (bc+ad)^2 )
			ac = base_r->re * base_s->re;
			bd = base_r->im * base_s->im;
			bc = base_r->im * base_s->re;
			ad = base_r->re * base_s->im;
			r = sqrtf(powf(ac-bd,2) + powf(bc+ad,2)); 
			base_i->re = (ac+bd)/r;
			base_i->im = (bc-ad)/r;
		}
	}
	// set the lower half to zero
	// This might be wrong. I may need to symmetrically fill it with the results of the calculation above and do the ffts on the full matrix.
	// But in fact it works as it is, with just half of the matrix filled. So, I leave it as it is.
	for( j = pixr->width/2; j < pixr->width; j++ )
		memset(ffti->in + j*ffti->step, 0, pixr->height*sizeof(struct GPU_FFT_COMPLEX));
	
	aft = millis();
	DEBUG("cross-power spectrum %ld millis",aft-bef);
	
	// Free fftr, ffts
	free_fft_gpu( fftr );
	free_fft_gpu( ffts );
	

	// 
	// p = InverseDFT_GPU( o );
	usleep(1); // Yield to OS
	bef = millis();

	gpu_fft_execute(ffti); // call one or many times

	aft = millis();
	DEBUG("inverse fft vert %ld millis",aft-bef);

	bef=millis();

	// In-place transposition of the result
	// this may be incorrect for INVERSE FFT
	// but it works for now, see above and below
	in_place_transpose_square_upper_half( ffti->out, pixr->height, ffti->step );	
	
	aft = millis();
	DEBUG("transpose %ld millis",aft-bef);
	bef = aft;

	usleep(1); // Yield to OS
	// this execution will work from out back to in
	// This may fail. I've set the right half of the matrix to 0 but this still may not be correct
	// 2014-02-06 It works for now, for synthesized and real pics, so I leave it as is is. 
	gpu_fft_execute(ffti); // call one or many times
	aft = millis();
	DEBUG("inverse fft horiz %ld millis",aft-bef);
	// RESULT IS NOW NOT-TRANSPOSED IN ffts->in 
	
	// 
	// identify peak, x, y
	maxval = -MAXFLOAT;
	xmaxloc = 0;
	ymaxloc = 0;
	for( j = 0; j < pixr->height; j++ )
	{
		base_i = ffti->in + j*ffti->step;
		for( i = 0; i < pixr->width; i++,base_i++ )
		{
			if( base_i->re > maxval )
			{
				maxval = base_i->re;
				xmaxloc = i;
				ymaxloc = j;
			}
		}
	}

	if (xmaxloc >= pixr->width / 2)
		xmaxloc -= pixr->width;
	if (ymaxloc >= pixr->height / 2)
		ymaxloc -= pixr->height;

	*ppeak = maxval;
	*px = xmaxloc;
	*py = ymaxloc;

	// 
	// clean up
	free_fft_gpu( ffti );
	
	return (0);
}
