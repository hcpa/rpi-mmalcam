#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include "fft.h"
#include "dbg_image.h"
#include "log.h"

static long millis()
{
	struct timespec tt;
	clock_gettime(CLOCK_MONOTONIC,&tt);
	return tt.tv_sec*1000l + tt.tv_nsec/1000000;
}


/*
 * create float luminance image dimensions w x h
 */
fpix_y_t *fpixCreate( uint32_t w, uint32_t h )
{
	fpix_y_t *fpix;
	
	fpix = calloc(sizeof(fpix_y_t), 1 );
	if( fpix == NULL )
	{
		ERROR("out of memory");
		return NULL;
	}
	
	fpix->width = w;
	fpix->height = h;
	fpix->data = calloc(sizeof(float), w*h );
	if( !fpix->data )
	{
		free(fpix);
		ERROR("out of memory");
		return NULL;
	}
	return fpix;
}

/*
 * destroy float luminance image
 */
void fpixDestroy( fpix_y_t *fpix )
{
	if( !fpix )
		return;
	
	if( fpix->data )
		free( fpix->data );
	
	free( fpix );
}

/*
 * create 8-bit luminance image dimensions w x h
 */
pix_y_t *pixCreate( uint32_t w, uint32_t h )
{
	pix_y_t *pix;
	
	pix = calloc(sizeof(pix_y_t), 1 );
	if( pix == NULL )
	{
		ERROR("out of memory");
		return NULL;
	}
	
	pix->width = w;
	pix->height = h;
	pix->data = calloc(sizeof(uint8_t), w*h );
	if( !pix->data )
	{
		free(pix);
		ERROR("out of memory");
		return NULL;
	}
	return pix;
}

/*
 * destroy 8-bit luminance image
 */
void pixDestroy( pix_y_t *pix )
{
	if( !pix )
		return;
	
	if( pix->data )
		free( pix->data );
	
	free( pix );
}


/*--------------------------------------------------------------------*
 *                     FPix  <-->  Pix conversions                    *
 *--------------------------------------------------------------------*/
/*!
 *  pixConvertToFPix()
 *
 *      Input:  pix 8 bit per pixel, luminance only
 *      Return: fpix, or null on error
 *
 */
fpix_y_t *pixConvertToFPix(pix_y_t  *pixs )
{
	int32_t     w, h;
	int32_t     i, j;
	uint8_t     *data;
	float       *fdata;
	fpix_y_t       *fpixd;
	
	
    if (!pixs)
	{
		ERROR("pixs not defined");
		return ( NULL);
	}

	w = pixs->width;
	h = pixs->height;

    if ((fpixd = fpixCreate(w, h)) == NULL)
	{
		ERROR("out of memory");
		return( NULL);
	}
    data = pixs->data;
    fdata = fpixd->data;
    for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			*fdata = *data;
			fdata++;
			data++;
        }
    }
	
    return fpixd;
}

/*!
 *  fpixConvertToPix()
 *
 *      Input:  fpixs luminance only
 *              errorflag (1 to output error stats; 0 otherwise)
 *      Return: pixd, or null on error
 *
 *  Notes:
 *      (2) Because we are converting a float to an unsigned int
 *          with a specified dynamic range (8, 16 or 32 bits), errors
 *          can occur.  If errorflag == TRUE, output the number
 *          of values out of range, both negative and positive.
 *      (3) If a pixel value is positive and out of range, clip to
 *          the maximum value represented at the outdepth of 8, 16
 *          or 32 bits.
 */
pix_y_t *fpixConvertToPix( fpix_y_t    *fpixs )
{
	int32_t     w, h, maxval = 255;
	int32_t     i, j;
	uint32_t    vald;
	float       val;
	float      *datas;
	uint8_t    *datad;
	pix_y_t    *pixd;
	
    if (!fpixs)
	{
		ERROR("dpixs not defined");
		return (NULL);
	}
	
	w = fpixs->width;
	h = fpixs->height;
    datas = fpixs->data;
	
	
	/* Make the pix and convert the data */
    if ( (pixd = pixCreate(w, h)) == NULL )
	{
        ERROR("pixd not made");
		return (NULL);
	}
    datad = pixd->data;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
			val = *datas;
            if( val > 0.0 )
				vald = (uint32_t)(val + 0.5);
            else /* val <= 0.0 */
                vald = 0;
            if (vald > maxval)
                vald = maxval;
			*datad = (uint8_t)val;
			datas++;
			datad++;
        }
    }
	
    return pixd;
}



/*!
 *  fpixGetMax()
 *
 *      Input:  dpix
 *              &maxval (<optional return> max value)
 *              &xmaxloc (<optional return> x location of max)
 *              &ymaxloc (<optional return> y location of max)
 *      Return: 0 if OK; 1 on error
 */
int32_t
fpixGetMax(fpix_y_t	*dpix,
           float		*pmaxval,
           int32_t    	*pxmaxloc,
           int32_t    	*pymaxloc)
{
	int32_t     i, j, w, h, xmaxloc, ymaxloc;
	float  *data;
	float   maxval;
	
    if (!pmaxval && !pxmaxloc && !pymaxloc)
	{
        ERROR("nothing to do");
		return(-1);
	}
    if (pmaxval) *pmaxval = 0.0;
    if (pxmaxloc) *pxmaxloc = 0;
    if (pymaxloc) *pymaxloc = 0;
    if (!dpix)
	{
        ERROR("dpix not defined");
		return(-1);
	}
	
    maxval = -1.0e40;
    xmaxloc = 0;
    ymaxloc = 0;
    w = dpix->width;
	h = dpix->height;
    data = dpix->data;
    for (i = 0; i < h; i++)
	{
        for (j = 0; j < w; j++)
		{
            if( (*data) > maxval )
			{
                maxval = (*data);
                xmaxloc = j;
                ymaxloc = i;
            }
			data++;
        }
    }
	
    if (pmaxval) *pmaxval = maxval;
    if (pxmaxloc) *pxmaxloc = xmaxloc;
    if (pymaxloc) *pymaxloc = ymaxloc;
    return 0;
}


/*--------------------------------------------------------------------*
 *                 DPix normalization and shifting                    *
 *--------------------------------------------------------------------*/
/*!
 *  dpixNormalize()
 *
 *      Input:  dpixd  (<optional>; this can be null, equal to dpixs,
 *                     or different from dpixs)
 *              dpixs
 *      Return: dpixd, or null on error
 *
 *  Notes:
 *      (1) Normalize dpixs by dividing each value by the size of the
 *          image. This is to compensate for the FFTW library returning
 *          unnormalized DFT values.
 *      (2) There are 3 cases:
 *           (a) dpixd == null,   ~src --> new dpixd
 *           (b) dpixd == dpixs,  ~src --> src  (in-place)
 *           (c) dpixd != dpixs,  ~src --> input dpixd
 *      (3) For clarity, if the case is known, use these patterns:
 *           (a) dpixd = dpixNormalize(NULL, dpixs);
 *           (b) dpixNormalize(dpixs, dpixs);
 *           (c) dpixNormalize(dpixd, dpixs);
 */
int32_t
fpixNormalize(fpix_y_t *dpixs)
{
	int32_t     i, j;
	float  *data, n;
	
	
    if (!dpixs)
	{
		ERROR("dpixs not defined");
		return(-1);
	}
	
	
	n = dpixs->width * dpixs->height;
	data = dpixs->data;
	
	for (i = 0; i < dpixs->height; i++)
		for (j = 0; j < dpixs->width; j++)
		{
			*data = (*data)/n;
			data++;
		}
	
	return 0;
}


/*!
 *  fpixInverseDFT()
 *
 *      Input:  dft
 *              w, h (image size)
 *      Return: dpix (unnormalized), or null on error
 */
fpix_y_t *
fpixInverseDFT(fftwf_complex *dft,
			   int32_t       w,
			   int32_t       h)
{
	fpix_y_t	*dpix;
	fftwf_plan	plan;
	
    if (!dft)
    {
    	ERROR("dft not defined");
		return NULL;
	}
	
	if( NULL == (dpix=calloc(sizeof(fpix_y_t),1)) )
	{
		ERROR("out of memory");
		return NULL;
	}
	
	dpix->width  = w;
	dpix->height = h;
	
	if( NULL == (dpix->data = malloc(sizeof(float) * w * h)) )
	{
		ERROR("out of memory");
		free(dpix);
		return NULL;
	}
	
	/* Compute the inverse DFT, storing the results into DPix */
	plan = fftwf_plan_dft_c2r_2d(h, w, dft, dpix->data, FFTW_ESTIMATE);
	fftwf_execute( plan );
	fftwf_destroy_plan( plan );
	
	fpixNormalize( dpix );
	
	return dpix;
}


/*--------------------------------------------------------------------*
 *      Low-level implementation of Discrete Fourier Transform        *
 *--------------------------------------------------------------------*/
/*!
 *  pixDFT()
 *
 *      Input:  pix (1 bpp or 8 bpp)
 *              shiftflag (L_NO_SHIFTING or L_WITH_SHIFTING)
 *      Return: complex array, or null on error
 *
 *  Notes:
 *      (1) The complex array returned has size (pixs->h) * (pixs->w / 2 + 1).
 *          This is to save space, given the fact the other half of the
 *          transform can be calculated by the complex conjugate.
 *      (2) By default, the DC of the DFT is in the top left corner (0, 0).
 *          Set @shiftflag to L_WITH_SHIFTING to move the DC to the center.
 *      (3) It is the responsibility of the caller to release the allocated
 *          complex array by invoking fftw_free().
 */
fftwf_complex *fpixDFT( fpix_y_t *fpix )
{
	fftwf_complex  *output;
	fftwf_plan      plan;
	
    if (!fpix)
	{
        ERROR("fpix not defined");
		return NULL;
	}

/* Compute the DFT of the DPix */
	output = (fftwf_complex *) fftwf_malloc(sizeof(fftwf_complex) * fpix->height * (fpix->width / 2 + 1));
	plan = fftwf_plan_dft_r2c_2d(fpix->height, fpix->width, fpix->data, output, FFTW_ESTIMATE);
	fftwf_execute(plan);
	
	fftwf_destroy_plan(plan);
	
	return output;
}

fftwf_complex *pixDFT(pix_y_t *pixs)
{
	fpix_y_t       *fpix;
	fftwf_complex   *output;
	fftwf_plan      plan;
	
    if (!pixs)
	{
		ERROR("pixs not defined");
		return( NULL );
	}

	/* Convert Pix to a DPix that can be fed to the FFTW library */
	if ((fpix = pixConvertToFPix( pixs )) == NULL)
	{
		ERROR("fpix not made");
		return( NULL );
	}

	/* Compute the DFT of the DPix */
	output = (fftwf_complex *) fftwf_malloc(sizeof(fftwf_complex) * pixs->height * (pixs->width / 2 + 1));
	plan = fftwf_plan_dft_r2c_2d(fpix->height, fpix->width, fpix->data, output, FFTW_ESTIMATE);
	fftwf_execute(plan);
	
	fpixDestroy( fpix );
	fftwf_destroy_plan(plan);
	
	return output;
}




/*--------------------------------------------------------------------*
 *                         Phase correlation                          *
 *--------------------------------------------------------------------*/
/*!
 *  pixPhaseCorrelation()
 *
 *      Input:  pixr, pixs (float)
 *              &peak (<optional return> phase correlation peak)
 *              &xloc (<optional return> x location of the peak)
 *              &yloc (<optional return> y location of the peak)
 *      Return: 0 if OK; 1 on error
 *
 *  Notes:
 *      (1) Phase correlation is a method of image registration,
 *          and uses a fast frequency-domain approach to estimate the
 *          relative translative offset between two similar images.
 *      (2) The reference and input images must have same width
 *          and height.
 *      (3) Determine the location of the highest value (peak) in @r,
 *          defined as the phase cross-correlated 2D array:
 *          r = inverse Fourier (F * G / |F * G|)
 *          where F is the Fourier transform of the reference image and
 *          G is the complex conjugate of the transform of the input.
 *          The peak in @r corresponds to the object translation movement
 *          from the reference to the input images. A value of 1.0  at
 *          location (0, 0) means that the images are identical.
 *      (4) If colormapped, remove to grayscale.
 */
int32_t
pixPhaseCorrelation(fpix_y_t       *pixr,
					fpix_y_t       *pixs,
					float 			*ppeak,
					int32_t   		*pxloc,
					int32_t   		*pyloc)
{
	int32_t        	i, j, k;
	float      		cr, ci, r;
	fftwf_complex  	*outputr, *outputs, *outputd;
	fpix_y_t     	*dpix;
	long			before, after;
	
    if (!pixr && !pixs)
	{
    	ERROR("pixr or pixs not defined");
		return(-1);
	}
	if (pixr->width != pixs->width || pixr->height != pixs->height)
    {
		ERROR("pixr and pixs unequal size");
		return(-1);
	}
	if (!ppeak && !pxloc && !pyloc)
	{
        ERROR("nothing to do");
		return(-1);
	}
	
	/* Calculate the DFT of pixr and pixs */
	before = millis();
	if ((outputr = fpixDFT(pixr)) == NULL)
	{
		ERROR("outputr not made");
		return(-1);
	}
	after = millis();
	DEBUG( "fft pixr %ld milliseconds", after-before );
	before = after;
	if ((outputs = fpixDFT(pixs)) == NULL) {
		fftwf_free(outputr);
		ERROR("outputs not made");
		return(-1);
	}
	after = millis();
	DEBUG( "fft pixs %ld milliseconds", after-before );
	outputd = (fftwf_complex *) fftwf_malloc(sizeof(fftwf_complex) * pixr->height * (pixr->width / 2 + 1));
	if (outputd == NULL) {
		fftwf_free(outputr);
		fftwf_free(outputs);
		ERROR("outputd not made");
		return(-1);
	}
	
	before = millis();
	/* Calculate the cross-power spectrum */
	for (i = 0, k = 0; i < pixr->height; i++) {
		for (j = 0; j < pixr->width / 2 + 1; j++, k++) {
			cr = creal(outputs[k]) * creal(outputr[k]) - cimag(outputs[k]) * (-cimag(outputr[k]));
			ci = creal(outputs[k]) * (-cimag(outputr[k])) + cimag(outputs[k]) * creal(outputr[k]);
			r = sqrtf(powf(cr, 2.) + powf(ci, 2.));
			outputd[k] = (cr / r) + I * (ci / r);
		}
	}
	after = millis();
	DEBUG( "cross-power spectrum %ld milliseconds", after-before );
	
	/* Compute the inverse DFT of the cross-power spectrum
	    and find its peak */
	before = millis();
	dpix = fpixInverseDFT(outputd, pixr->width, pixr->height);
	after = millis();
	DEBUG( "inverse DFT %ld milliseconds", after-before );
	
	before = millis();
	fpixGetMax(dpix, ppeak, pxloc, pyloc);
	after = millis();
	DEBUG( "find max %ld milliseconds", after-before );
	
	if (*pxloc >= pixr->width / 2)
		*pxloc -= pixr->width;
	if (*pyloc >= pixr->height / 2)
		*pyloc -= pixr->height;
		
	/* Release the allocated resources */
	fftwf_free(outputr);
	fftwf_free(outputs);
	fftwf_free(outputd);
	
	free(dpix->data);
	free(dpix);
	
	return(0);
}
