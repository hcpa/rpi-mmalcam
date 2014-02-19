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

#define CACHE_FFTR 0
#define CACHE_FFTS 1
#define CACHE_FFTI 2

static struct GPU_FFT *fft_c[CACHE_FFTI+1];

static long millis()
{
	struct timespec tt;
	clock_gettime(CLOCK_MONOTONIC,&tt);
	return tt.tv_sec*1000l + tt.tv_nsec/1000000;
}

/*
 *  In-place transposition of square matrix of complex values 
 */
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

static int init_fft_cache( pix_y_t *pix )
{
	int ret; 
	int log2_N = (int)round(log2(pix->width));	

	if( mb < 0 )
	{
		mb = mbox_open();
		if( mb < 0 ){
			ERROR( "cannot open mailbox" );
			return (-1);
		}
	}
	
    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pix->height, fft_c+CACHE_FFTR); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return (-1);
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return (-1);
        case -3: ERROR("Out of memory round1.  Try a smaller batch or increase GPU memory.");  return (-1);
        case -4: ERROR("Cannot open /dev/mem, must run as root.");  return (-1);
    }

    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_FWD, pix->height, fft_c+CACHE_FFTS); // call once

    switch(ret) {
        case -1: ERROR("Unable to enable V3D. Please check your firmware is up to date."); return (-1);
        case -2: ERROR("log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return (-1);
        case -3: ERROR("Out of memory round1.  Try a smaller batch or increase GPU memory.");  return (-1);
        case -4: ERROR("Cannot open /dev/mem, must run as root.");  return (-1);
    }


    ret = gpu_fft_prepare(mb, log2_N, GPU_FFT_REV, pix->height, fft_c+CACHE_FFTI); // call once

    switch(ret) {
        case -1: ERROR("CACHE_FFTI: Unable to enable V3D. Please check your firmware is up to date."); return (-1);
        case -2: ERROR("CACHE_FFTI: log2_N=%d not supported.  Try between 8 and 17.", log2_N);         return (-1);
        case -3: ERROR("CACHE_FFTI: Out of memory round1.  Try a smaller batch or increase GPU memory.");  return (-1);
        case -4: ERROR("CACHE_FFTI: Cannot open /dev/mem, must run as root.");  return (-1);
    }
	return (0);
}


/*
 *  Transpose in-place the left half of a square matrix to the upper half. 
 *  I.e. a w/2-by-w-Matrix becomes a w-x-w/2-matrix 
 *  The lower half of the result matrix is zeroed out.
 */
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

/*
 *  Transpose in-place the top half of a square matrix to the left half. 
 *  I.e. a w-by-w/2-Matrix becomes a w/2-x-w-matrix 
 *  The right half of the result matrix is zeroed out.
 */
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


/* 
 * Do a GPU-based FFT on a square(!) image, but leave out the final transposition.
 * This is very useful for the phase correlation below
 *
 * This function assumes the mailbox is already created.
 * 
 * 1: horizontal fft for all lines
 * 2: transpose results from left half to top half 
 * 3: (vertical) fft for all lines (former columns) (see comment below on performance)
 * 
 * TODO This function does no error checking 
 *
 * returns pointer to struct GPU_FFT (basically, an array of complex numbers) with results
 * return value needs to be free'd with free_fft_gpu
 * returns NULL on error
 */
static int pixDFT_GPU_no_final_transpose( unsigned cache_idx, pix_y_t *pic )
{
    int i, j, ret;
    struct GPU_FFT_COMPLEX *base;
	uint8_t *picdata;
	
	if( !fft_c[cache_idx] )
	{
		ret = init_fft_cache( pic );
		if( ret != 0 )
		{
			ERROR("Error Initializing GPU FFT plan cache");
			return (-1);		
		}
	}

	for( j=0; j < pic->height; j++ )
	{
		base = fft_c[cache_idx]->in + j*fft_c[cache_idx]->step; // input buffer
		picdata = pic->data + j*pic->width;
        for( i=0; i<pic->width ; i++,picdata++ )
		{
			base[i].re = *(picdata);
			base[i].im = 0;			
		}
    }

	usleep(1); // Yield to OS

	gpu_fft_execute(fft_c[cache_idx]); 


	// In-place transposition of the result
	in_place_transpose_square_left_half( fft_c[cache_idx]->out, pic->height, fft_c[cache_idx]->step );	
	
	usleep(1); // Yield to OS
	// this execution will work from fft->out back to fft->in
	// If we just transpose the left half of the matrix to the upper half
	// it's not necessary to perform the FFT on all rows. half of the rows
	// would be sufficient. but since gpu_fft is so fast, it's not really an issue.
	gpu_fft_execute(fft_c[cache_idx]); // call one or many times

	return 0;

}

/* 
 * Do a GPU-based FFT on a square(!) image.
 *
 * returns pointer to struct GPU_FFT (basically, an array of complex numbers) with results
 * return value needs to be free'd with free_fft_gpu
 * returns NULL on error
 *
 */
struct GPU_FFT *pixDFT_GPU( unsigned cache_idx, pix_y_t *pic )
{
	int ret;
	
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
	
	ret = pixDFT_GPU_no_final_transpose( cache_idx, pic );
	if( ret != 0 )
	{
		ERROR("pixDFT_GPU_no_final_transpose failed");
		return NULL;
	}
	
	// Transpose back
	in_place_transpose_square_upper_half( fft_c[cache_idx]->in, pic->height, fft_c[cache_idx]->step );

	return fft_c[cache_idx];
}


/*
 * free GPU FFT result 
 */ 
void free_fft_gpu( struct GPU_FFT *fft )
{
    gpu_fft_release(fft); // Videocore memory lost if not freed !	
}

void cleanup_fft_gpu()
{
	if( fft_c[CACHE_FFTR] ) { 
		free_fft_gpu( fft_c[CACHE_FFTR] ); fft_c[CACHE_FFTR] = NULL;
	}
	if( fft_c[CACHE_FFTS] ) {
		free_fft_gpu( fft_c[CACHE_FFTS] ); fft_c[CACHE_FFTS] = NULL;
	}
	if( fft_c[CACHE_FFTI] ) {
		free_fft_gpu( fft_c[CACHE_FFTI] ); fft_c[CACHE_FFTI] = NULL;
	}
}


/* 
 * Calculate phase correlation for 2 luminance images
 *
 * Sequence: FFT reference image pixr, FFT shifted image pixs, calculate cross-power spectrum,
 * inverse FFT on result, identify maximum+coordinates. See below for details
 * 
 * ppeak, px and py are filled
 * returns -1 on error, 0 otherwise 
 */
int pixPhaseCorrelate_GPU( pix_y_t *pixr, pix_y_t *pixs, float *ppeak, int *px, int *py )
{
    int i, j, ret;
    struct GPU_FFT_COMPLEX *base_r, *base_s, *base_i;
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
	
	if( !fft_c[CACHE_FFTR] || !fft_c[CACHE_FFTS] || !fft_c[CACHE_FFTI] )
	{
		unsigned long t = millis();
		ret = init_fft_cache( pixr );
		WARN("init_fft_cache %ld ms", millis()-t );
		if( ret != 0 )
		{
			ERROR("Error Initializing GPU FFT plan cache");
			return (-1);		
		}
	}


	// FFT pixr
	ret = pixDFT_GPU_no_final_transpose( CACHE_FFTR, pixr );
	if( ret != 0 )
	{
		ERROR("pixDFT_GPU_no_final_transpose failed for pixr");
		return (-1);
	}
	// RESULT IS NOW TRANSPOSED IN fft_c[CACHE_FFTR]->in 

	
	// FFT pixs
	ret = pixDFT_GPU_no_final_transpose( CACHE_FFTS, pixs );
	if( ret != 0 )
	{
		ERROR("pixDFT_GPU_no_final_transpose failed for pixs");
		return (-1);
	}
	// RESULT IS NOW TRANSPOSED IN fft_c[CACHE_FFTS]->in 



	// calculate cross-power spectrum
	// 	o_{i,j} = sqrt((re(s_{i,j})*re(r_{i,j}) - im(s_{i,j})*-im(r__{i,j}))^2 + (re(s_{i,j})*-im(r_{i,j}) - im(s_{i,j})*re(r__{i,j}))^2)
	// 
	// TODO Why is the cross power spectrum in fft.c 30% faster?
	for( j = 0; j < pixr->width/2; j++ )
	{
		base_r = fft_c[CACHE_FFTR]->in + j*fft_c[CACHE_FFTR]->step;
		base_s = fft_c[CACHE_FFTS]->in + j*fft_c[CACHE_FFTS]->step;
		base_i = fft_c[CACHE_FFTI]->in + j*fft_c[CACHE_FFTI]->step;
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
		memset(fft_c[CACHE_FFTI]->in + j*fft_c[CACHE_FFTI]->step, 0, pixr->height*sizeof(struct GPU_FFT_COMPLEX));
	
	
	// 
	// p = InverseDFT_GPU( o );
	usleep(1); // Yield to OS

	gpu_fft_execute(fft_c[CACHE_FFTI]); // call one or many times


	// In-place transposition of the result
	// this may be incorrect for INVERSE FFT
	// but it works for now, see above and below
	in_place_transpose_square_upper_half( fft_c[CACHE_FFTI]->out, pixr->height, fft_c[CACHE_FFTI]->step );	
	

	usleep(1); // Yield to OS
	// this execution will work from out back to in
	// This may fail. I've set the right half of the matrix to 0 but this still may not be correct
	// 2014-02-06 It works for now, for synthesized and real pics, so I leave it as is is. 
	gpu_fft_execute(fft_c[CACHE_FFTI]); // call one or many times
	// RESULT IS NOW NOT-TRANSPOSED IN fft_c[CACHE_FFTI]->in 
	
	// 
	// identify peak, x, y
	maxval = -MAXFLOAT;
	xmaxloc = 0;
	ymaxloc = 0;
	for( j = 0; j < pixr->height; j++ )
	{
		base_i = fft_c[CACHE_FFTI]->in + j*fft_c[CACHE_FFTI]->step;
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
	
	{
		unsigned long t = millis();
	cleanup_fft_gpu();
	WARN("cleanup_fft_gpu %ld ms", millis()-t );
    }

	return (0);
}
