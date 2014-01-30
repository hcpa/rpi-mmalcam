#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
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

uint32_t y_compley_save( fftwf_complex *data, uint32_t real, uint32_t w, uint32_t h, char *name)
{
	FILE *f;
	gdImage *image;
	uint32_t i, j, g, color;
	fftwf_complex *dat;
	float d, max;
	
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
			if(real)
				d = creal(*dat);
			else
				d = cimag(*dat);
			dat++;
			if(d>max) max=d;
		}		
	
	
	dat = data;
	for(j=0; j<h; j++){
		for(i=0; i<w; i++)
		{
			if(real)
				d = creal(*dat);
			else
				d = cimag(*dat);
			dat++;
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

