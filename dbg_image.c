#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <gd.h>
#include <complex.h>
#include "log.h"
#include "dbg_image.h"

uint32_t y_int_save( uint8_t *data, uint32_t w, uint32_t h, char *name )
{
	FILE *f;
	gdImage *image;
	uint32_t i, j, color;
	uint8_t *dat, d;
	
	if(!name)
		return(-1);
	
	f = fopen(name, "wb");
	
	if(!f)
	{
		ERROR("Error opening file for output: %s", name);
		ERROR("fopen: %s", strerror(errno));
		return(-1);
	}
	
	image = gdImageCreateTrueColor(w,h);
	if(!image)
	{
		ERROR("Cannot create image");
		fclose(f);
		return(-1);
	}

	dat = data;
	for(j=0; j<h; j++){
		for(i=0; i<w; i++)
		{
			d = *(dat++);
			color = d + (d<<8) + (d<<16);
			color &= 0xffffff; 
			gdImageSetPixel(image,i,j,color);
		}
	}
	
	gdImageJpeg(image, f, 255);

	gdImageDestroy(image);
	return(0);
}

uint32_t y_float_save( float *data, uint32_t w, uint32_t h, char *name )
{
	FILE *f;
	gdImage *image;
	uint32_t i, j, g, color;
	float *dat, d, max;
	
	if(!name)
		return(-1);
	
	f = fopen(name, "wb");
	
	if(!f)
	{
		ERROR("Error opening file for output: %s", name);
		ERROR("fopen: %s", strerror(errno));
		return(-1);
	}
	
	image = gdImageCreateTrueColor(w,h);
	if(!image)
	{
		ERROR("Cannot create image");
		fclose(f);
		return(-1);
	}

	max=0;
	dat=data;
	for(j=0;j<h;j++)
		for(i=0;i<w;i++)
		{
			d = *(dat++);
			if(d>max) max=d;
		}		
	
	
	dat = data;
	for(j=0; j<h; j++){
		for(i=0; i<w; i++)
		{
			d = *(dat++);
			g = 0xff * d/max;
			color = g + (g<<8) + (g<<16);
			color &= 0xffffff; 
			gdImageSetPixel(image,i,j,color);
		}
	}
	
	gdImageJpeg(image, f, 255);

	gdImageDestroy(image);
	return(0);
}

uint32_t y_complex_save( fftwf_complex *data, uint32_t magnitude, uint32_t w, uint32_t h, char *name )
{
	FILE *f;
	gdImage *image;
	uint32_t i, j, g, color;
	fftwf_complex *dat;
	float d, max, re, im;
	
	if(!name)
		return(-1);
	
	f = fopen(name, "wb");
	
	if(!f)
	{
		ERROR("Error opening file for output: %s", name);
		ERROR("fopen: %s", strerror(errno));
		return(-1);
	}
	
	image = gdImageCreateTrueColor(w,h);


	if(!image)
	{
		ERROR("Cannot create image");
		fclose(f);
		return(-1);
	}

	max=0;
	dat=data;
	for(j=0;j<h;j++)
		for(i=0;i<w;i++)
		{
			if(magnitude)
			{
				re = creal(*dat);
				im = cimag(*dat);
				d = sqrt(pow(re,2)+pow(im,2));
			} else {
				re = creal(*dat);
				im = cimag(*dat);
				d = atan2(im, re);
			}
			dat++;
			if(d>max) max=d;
		}		
	


	dat = data;
	for(j=0; j<h; j++){
		for(i=0; i<w; i++)
		{
			if(magnitude)
			{
				re = creal(*dat);
				im = cimag(*dat);
				d = sqrt(re*re+im*im);
			} else {
				re = creal(*dat);
				im = cimag(*dat);
				d = atan2(im, re);
			}
			dat++;
			g = round(255.0 * d/max);
			color = g + (g<<8) + (g<<16);
			color &= 0xffffff; 
			gdImageSetPixel(image,i,j,color);
		}
	}
	
	gdImageJpeg(image, f, 255);

	gdImageDestroy(image);

	return(0);
}

int fftwf_result_save( fftwf_complex *data, int spectrum, int phase_angle, int power_spectrum, uint32_t w, uint32_t h, char *basename )
{
	FILE *f_s, *f_pa, *f_ps;
	gdImage *image_s, *image_pa, *image_ps;
	uint32_t i, j, g, color;
	fftwf_complex *dat;
	float d, max_s, max_pa, max_ps, power;
	
	char fn_s[256],fn_pa[256],fn_ps[256];
	
	if( !spectrum && !phase_angle && !power_spectrum )
	{
		WARN("nothing to do");
		return(0);
	}
	
	if( spectrum )
	{
		snprintf( fn_s, 255, "%s-spectrum.png", basename);
		f_s = fopen(fn_s,"wb");
		if(!f_s)
		{
			ERROR("Error opening file for output: %s", fn_s);
			ERROR("fopen: %s", strerror(errno));
			return(-1);
		}

		image_s = gdImageCreateTrueColor(w,h);
		if(!image_s)
		{
			ERROR("Cannot create image");
			fclose(f_s);
			return(-1);
		}
	}

	if( phase_angle )
	{
		snprintf( fn_pa, 255, "%s-phaseang.png", basename);
		f_pa = fopen(fn_pa,"wb");
		if(!f_pa)
		{
			ERROR("Error opening file for output: %s", fn_pa);
			ERROR("fopen: %s", strerror(errno));
			return(-1);
		}

		image_pa = gdImageCreateTrueColor(w,h);
		if(!image_pa)
		{
			ERROR("Cannot create image");
			fclose(f_pa);
			return(-1);
		}
	}

	if( phase_angle )
	{
		snprintf( fn_ps, 255, "%s-powspect.png", basename);
		f_ps = fopen(fn_ps,"wb");
		if(!f_ps)
		{
			ERROR("Error opening file for output: %s", fn_ps);
			ERROR("fopen: %s", strerror(errno));
			return(-1);
		}

		image_ps = gdImageCreateTrueColor(w,h);
		if(!image_ps)
		{
			ERROR("Cannot create image");
			fclose(f_ps);
			return(-1);
		}
	}

	
	max_s = max_pa = max_ps =0;
	dat = data;
	for(j=0;j<h;j++)
		for(i=0;i<w;i++,dat++)
		{
			if( spectrum || power_spectrum )
				power = pow(creal(*dat),2) + pow(cimag(*dat),2);
			if( spectrum ) {
				d = sqrt(power);
				if( d> max_s )
					max_s = d;
			}
			if( phase_angle ) {
				d = atan2(cimag(*dat), creal(*dat));
				if( d> max_pa )
					max_pa = d;
			}
			if( power_spectrum ) {
				d = power;
				if( d> max_ps )
					max_ps = d;
			}
		}		
	


	dat = data;
	for(j=0; j<h; j++){
		for(i=0; i<w; i++)
		{
			if( spectrum || power_spectrum )
				power = pow(creal(*dat),2) + pow(cimag(*dat),2);


			if( spectrum ){
				d = sqrt(power);
				g = round(255.0 * d/max_s);
				color = g + (g<<8) + (g<<16);
				color &= 0xffffff; 
				gdImageSetPixel(image_s,i,j,color);
			}
			if( phase_angle ){
				d = atan2(cimag(*dat), creal(*dat));
				g = round(255.0 * d/max_pa);
				color = g + (g<<8) + (g<<16);
				color &= 0xffffff; 
				gdImageSetPixel(image_pa,i,j,color);
			}
			if( power_spectrum ){
				d = power;
				g = round(255.0 * d/max_ps);
				color = g + (g<<8) + (g<<16);
				color &= 0xffffff; 
				gdImageSetPixel(image_ps,i,j,color);
			}

			dat++;
		}
	}
	
	if( spectrum ){ 
		gdImagePng(image_s, f_s);
		fclose( f_s );
		gdImageDestroy(image_s);
	}
	if( phase_angle ){ 
		gdImagePng(image_pa, f_pa);
		fclose( f_pa );
		gdImageDestroy(image_pa);
	}
	if( power_spectrum ){ 
		gdImagePng(image_ps, f_ps);
		fclose( f_ps );
		gdImageDestroy(image_ps);
	}

	return(0);
}

int gpufft_result_save( struct GPU_FFT_COMPLEX *data, int step, int spectrum, int phase_angle, int power_spectrum, uint32_t w, uint32_t h, char *basename )
{
	FILE *f_s, *f_pa, *f_ps;
	gdImage *image_s, *image_pa, *image_ps;
	uint32_t i, j, g, color;
	struct GPU_FFT_COMPLEX *dat;
	float d, max_s, max_pa, max_ps, power;
	
	char fn_s[256],fn_pa[256],fn_ps[256];
	
	if( !spectrum && !phase_angle && !power_spectrum )
	{
		WARN("nothing to do");
		return(0);
	}
	
	if( spectrum )
	{
		snprintf( fn_s, 255, "%s-spectrum.png", basename);
		f_s = fopen(fn_s,"wb");
		if(!f_s)
		{
			ERROR("Error opening file for output: %s", fn_s);
			ERROR("fopen: %s", strerror(errno));
			return(-1);
		}

		image_s = gdImageCreateTrueColor(w,h);
		if(!image_s)
		{
			ERROR("Cannot create image");
			fclose(f_s);
			return(-1);
		}
	}

	if( phase_angle )
	{
		snprintf( fn_pa, 255, "%s-phaseang.png", basename);
		f_pa = fopen(fn_pa,"wb");
		if(!f_pa)
		{
			ERROR("Error opening file for output: %s", fn_pa);
			ERROR("fopen: %s", strerror(errno));
			return(-1);
		}

		image_pa = gdImageCreateTrueColor(w,h);
		if(!image_pa)
		{
			ERROR("Cannot create image");
			fclose(f_pa);
			return(-1);
		}
	}

	if( phase_angle )
	{
		snprintf( fn_ps, 255, "%s-powspect.png", basename);
		f_ps = fopen(fn_ps,"wb");
		if(!f_ps)
		{
			ERROR("Error opening file for output: %s", fn_ps);
			ERROR("fopen: %s", strerror(errno));
			return(-1);
		}

		image_ps = gdImageCreateTrueColor(w,h);
		if(!image_ps)
		{
			ERROR("Cannot create image");
			fclose(f_ps);
			return(-1);
		}
	}

	
	max_s = max_pa = max_ps =0;
	for(j=0;j<h;j++)
	{
		dat = data + j*step;
		for(i=0;i<w;i++,dat++)
		{
			if( spectrum || power_spectrum )
				power = pow(dat[i].re,2) + pow(dat[i].im,2);
			if( spectrum ) {
				d = sqrt(power);
				if( d> max_s )
					max_s = d;
			}
			if( phase_angle ) {
				d = atan2(dat[i].im, dat[i].re);
				if( d> max_pa )
					max_pa = d;
			}
			if( power_spectrum ) {
				d = power;
				if( d> max_ps )
					max_ps = d;
			}
		}		
	}
	

	
	
	for( j = 0; j < h; j++ )
	{
		dat = data + j*step;
		for( i = 0; i < w; i++,dat++ )
		{
			if( spectrum || power_spectrum )
				power = pow(dat->re,2) + pow(dat->im,2);
			if( spectrum )
			{
				d = sqrt(power);
				g = round(0xff * d/max_s);
				color = g | (g<<8) | (g<<16);
				gdImageSetPixel( image_s, i, j, color & 0xffffffl );
			}
			if( phase_angle )
			{
				d = atan2(dat->im, dat->re);
				g = round(0xff * d/max_pa);
				color = g | (g<<8) | (g<<16);
				gdImageSetPixel(image_pa, i, j, color & 0xffffffl );
			}
			if( power_spectrum )
			{
				d = power;
				g = round(0xff * d/max_ps);
				color = g | (g<<8) | (g<<16);
				gdImageSetPixel(image_ps, i, j, color & 0xffffffl );
			}
		}
	}
	

	
	if( spectrum ){ 
		gdImagePng(image_s, f_s);
		fclose( f_s );
		gdImageDestroy(image_s);
	}
	if( phase_angle ){ 
		gdImagePng(image_pa, f_pa);
		fclose( f_pa );
		gdImageDestroy(image_pa);
	}
	if( power_spectrum ){ 
		gdImagePng(image_ps, f_ps);
		fclose( f_ps );
		gdImageDestroy(image_ps);
	}

	return(0);
}
