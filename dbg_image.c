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

