#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <gd.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_encodings.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_util_params.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_connection.h>
#include <bcm_host.h>
#include "mmalyuv.h"
#include "log.h"
#include "dbg_image.h"
#include "fft.h"
#include "fft_gpu.h"

static int keep_looping;
static int shift_x;
static int shift_y;

static long millis()
{
	struct timespec tt;
	clock_gettime(CLOCK_MONOTONIC,&tt);
	return tt.tv_sec*1000l + tt.tv_nsec/1000000;
}


static void signal_handler(int signal_number)
{
    // Going to abort on all other signals
	ERROR("Aborting program...");
	keep_looping = 0;
	return;
}


static int dbg_load_stars( pix_y_t *pic )
{
	int i, j;
	uint8_t *dat;
	FILE *f;
	gdImage *image;
	
	f = fopen("sterne_pad.png","rb");

	if( !f ) {
		ERROR( "cannot open padded stars PNG" );
		return (-1);
	}
	
	image = gdImageCreateFromPng( f );
	fclose( f );
	
	if( !image ) {
		ERROR( "cannot create image from PNG file" );
		return (-1);
	}

	dat = pic->data;
	for( j = 0; j < pic->height; j++ )
		for( i = 0; i < pic->width; i++ )
		{
			int color = gdImageGetPixel( image, i+DBG_PAD_X, j+DBG_PAD_Y );
			*dat = (((color>>16) & 0xff) + ((color>>8) & 0xff) + (color & 0xff))/3;
		}
	return(0);
}

static int dbg_copy_stars( pix_y_t *dst, pix_y_t *src, int sh_x, int sh_y )
{
	int j;
	uint8_t *src_b, *dst_b;
	
	if( src->width + sh_x < dst->width )
		return (-1);
	if( src->height + sh_y < dst->height )
		return (-1);
	
	
	for( j = 0; j < dst->height; j++ )
	{
		src_b = src->data + (sh_y+j)*src->width + sh_x;
		dst_b = dst->data + j*dst->width;
		memcpy( dst_b, src_b, dst->width );
	}
	return (0);
}


static void y_writer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	int complete = 0;
	
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;	
	
	if( pData )
	{
		if( buffer->length && pData->image_buffer && pData->bytes_written < pData->max_bytes )
		{
			uint32_t to_write = 0;
			
			if( pData->bytes_written + buffer->length > pData->max_bytes )
				to_write = pData->max_bytes - pData->bytes_written;
			 else
				to_write = buffer->length;
			
			mmal_buffer_header_mem_lock( buffer );
			if( pData->current_frame )
			{
				uint8_t *dst = pData->image_buffer+pData->bytes_written;
				uint8_t *src = buffer->data;
				while( to_write > 0 ) {
					*src = *src + (*dst-*src)/pData->current_frame;
					src++;
					dst++;
					to_write--;
				} 
			} else // first frame, just memcpy
				memcpy( pData->image_buffer+pData->bytes_written, buffer->data, to_write );
			mmal_buffer_header_mem_unlock( buffer );

			pData->bytes_written += to_write;
		}
		
	
		if( buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED) )
			complete = 1;
	}
	else
	{
		ERROR("callback received encoder buffer with no user data");
	}

	mmal_buffer_header_release(buffer);
	
    // TODO I have no idea what this is doing, the "manual" in mmal.h doesn't mention it
	// but the thing brakes without so let's do it for now
    if( port->is_enabled )
    {
       MMAL_STATUS_T status = MMAL_SUCCESS;
       MMAL_BUFFER_HEADER_T *new_buffer;

       new_buffer = mmal_queue_get( pData->camera_pool->queue );

       if( new_buffer )
          status = mmal_port_send_buffer(port, new_buffer);

       if (!new_buffer || status != MMAL_SUCCESS)
          ERROR("Unable to return a buffer to the encoder port");
    }
		
    if (complete)
       vcos_semaphore_post(&(pData->complete_semaphore));
	
}


static int prepare_camera( MMAL_COMPONENT_T **camera_component, MMAL_PORT_T **still_port, MMAL_POOL_T **pool_out )
{
	MMAL_STATUS_T status;
	MMAL_ES_FORMAT_T *format_out;	
	
	DEBUG("prepare_camera");

	status = mmal_component_create( MMAL_COMPONENT_DEFAULT_CAMERA, camera_component );
	if( status != MMAL_SUCCESS )
	{
		ERROR("cannot create component %s",MMAL_COMPONENT_DEFAULT_CAMERA);
		return(-1);
	}
	
	if( !(*camera_component)->output_num )
	{
		ERROR("Camera doesn't have output ports - which ist really strange");
		mmal_component_destroy( *camera_component );
		return(-1);
	}
	
	*still_port = (*camera_component)->output[MMAL_CAMERA_CAPTURE_PORT];

	format_out = (*still_port)->format;
	
    format_out->encoding = MMAL_ENCODING_I420;
    format_out->encoding_variant = MMAL_ENCODING_I420;

	format_out->es->video.width = MAX_CAM_WIDTH;
	format_out->es->video.height = MAX_CAM_HEIGHT;
	format_out->es->video.crop.x = 0;
	format_out->es->video.crop.y = 0;
	format_out->es->video.crop.width = MAX_CAM_WIDTH;
	format_out->es->video.crop.height = MAX_CAM_HEIGHT;
	format_out->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
	format_out->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;
	
	if( (*still_port)->buffer_num < (*still_port)->buffer_num_min )
		(*still_port)->buffer_num  = (*still_port)->buffer_num_min;

	(*still_port)->buffer_size = (*still_port)->buffer_size_recommended;

	status = mmal_port_format_commit( *still_port );
	if( status != MMAL_SUCCESS )
	{
		ERROR( "cannot create commit format" );
		mmal_component_destroy( *camera_component );
		return(-1);
	}
	
	status = mmal_component_enable( *camera_component );
	if( status != MMAL_SUCCESS )
	{
		ERROR( "Cannot enable camera component" );
		mmal_component_destroy( *camera_component );
		return(-1);
	}
	
    *pool_out = mmal_port_pool_create(*still_port, (*still_port)->buffer_num, (*still_port)->buffer_size);
	
    if (!(*pool_out))
    {
       ERROR("Failed to create buffer header pool for encoder output port %s", (*still_port)->name);
	   return (-1);
    }
	
	return 0;
}

static int capture_frames( PORT_USERDATA *callback_data, MMAL_PORT_T *still_port, MMAL_POOL_T *pool_out, int frames )
{
	for( callback_data->current_frame = 0; callback_data->current_frame < frames; callback_data->current_frame++ )
	{
		callback_data->bytes_written = 0;

		int q, num = mmal_queue_length(pool_out->queue);

		for ( q=0; q<num; q++ )
		{
			MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get( pool_out->queue );

			if (!buffer) { 
				ERROR("Unable to get a required buffer %d from pool queue", q); return (-1); }

			if (mmal_port_send_buffer(still_port, buffer)!= MMAL_SUCCESS) {
				ERROR("Unable to send a buffer to encoder output port (%d)", q); return (-1); }
		}
	
		if ( mmal_port_parameter_set_boolean( still_port, MMAL_PARAMETER_CAPTURE, 1 ) != MMAL_SUCCESS)
		{
			ERROR("Failed to start capture");
			return (-1);
		}
		else
		{
			// Wait for capture to complete
			// For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
			// even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
			vcos_semaphore_wait( &callback_data->complete_semaphore );
		}
	}
	
	return (0);
}



int main(int argc, char *argv[])
{
	MMAL_COMPONENT_T *camera_component;
	VCOS_STATUS_T vcos_status;
	MMAL_POOL_T *pool_out;
	MMAL_PORT_T *still_port = NULL;
    PORT_USERDATA callback_data;
	pix_y_t img1, img2, star_base;
	// struct GPU_FFT *frame1_fft_gpu;
	float peak;
	int32_t xloc, yloc;
	

	

	// TODO verbose level
	log_verbose(100);

	bcm_host_init();
	
	if( geteuid() != 0 )
	{
		ERROR("This program needs r/w access to /dev/mem. It must be run as root (suid).\nExiting...");
		exit(-1);
	}
	
	srandom(millis());
    signal(SIGINT, signal_handler);
	
	
	star_base.width = MAX_CAM_WIDTH_PADDED + 2*DBG_PAD_X;
	star_base.height = MAX_CAM_HEIGHT_PADDED + 2*DBG_PAD_Y;
	star_base.data = calloc( (MAX_CAM_WIDTH_PADDED + 2*DBG_PAD_X) * (MAX_CAM_HEIGHT_PADDED + 2*DBG_PAD_Y), sizeof( uint8_t ) );
	if( !star_base.data ) {
		ERROR("out of memory data");
		exit(-1);
	}
	dbg_load_stars( &star_base );
	

	if( prepare_camera( &camera_component, &still_port, &pool_out ) )
	{
		ERROR( "failed to prepare camera" );
		goto error;
	}
	

	callback_data.camera_pool = pool_out;
	callback_data.image_buffer = NULL;

	DEBUG("creating semaphore");
    vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "mmalcam-sem", 0);
    vcos_assert(vcos_status == VCOS_SUCCESS);
	
	
	DEBUG("sleeping for some time have exposure adjust automatically");
	// results show: sleeping time can be much shorter, tested down to 2ms
	// although then exposure and awb seem to be somewhat off and image size huge 
	// next frames can go with a much shorter exposure like 30ms
    vcos_sleep(300);

	img1.width = MAX_CAM_WIDTH_PADDED;
	img1.height = MAX_CAM_HEIGHT_PADDED;
	img1.data = calloc( MAX_CAM_WIDTH_PADDED * MAX_CAM_HEIGHT_PADDED, sizeof( uint8_t ));
	if( !img1.data )
	{
		ERROR("out of memory img1");
		goto error;
	}
	
	callback_data.image_buffer = img1.data;
	callback_data.max_bytes = MAX_CAM_WIDTH_PADDED * MAX_CAM_HEIGHT_PADDED;
	callback_data.current_frame = 0;
	
    still_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;
	
	
	if( mmal_port_enable( still_port, y_writer_callback ) != MMAL_SUCCESS )
	{
		ERROR("failed to enable camera still output port");
		goto error;
	}


	if( capture_frames( &callback_data, still_port, pool_out, MAX_FRAMES ) ) {
		ERROR( "failed to capture first shot" );
		goto error;
	}
	
	
	// DEBUG
	// overwrite first frame with stars 
	dbg_copy_stars( &img1, &star_base, DBG_PAD_X, DBG_PAD_Y );
	y_int_save( img1.data, img1.width, img1.height, "img1.jpg" );
	
	
	// GPU FFT
	/*
	DEBUG("start fft gpu frame 1");

	frame1_fft_gpu = pixDFT_GPU( &img1 );
	if( !frame1_fft_gpu )
	{
		ERROR("first GPU FFT failed");
		goto error;
	}
	
	*/


	img2.width = MAX_CAM_WIDTH_PADDED;
	img2.height = MAX_CAM_HEIGHT_PADDED;
	img2.data = calloc( MAX_CAM_WIDTH_PADDED * MAX_CAM_HEIGHT_PADDED, sizeof( uint8_t ));
	if( !img2.data )
	{
		ERROR("out of memory img2");
		goto error;
	}
	
	callback_data.image_buffer = img2.data;
	callback_data.max_bytes = MAX_CAM_WIDTH_PADDED * MAX_CAM_HEIGHT_PADDED;


	keep_looping = 0;
	do {
		callback_data.bytes_written = 0;
		callback_data.current_frame = 0;

		if( capture_frames( &callback_data, still_port, pool_out, MAX_FRAMES ) ) {
			ERROR( "failed to capture shot x" );
			goto error;
		}

		// DEBUG overwrite frame with stars
		shift_x += random()&&0x1f - 16;
		shift_y += random()&&0x1f - 16;
		if( abs(shift_x) > DBG_PAD_X - 15  )
			shift_x = 0;
		if( abs(shift_y) > DBG_PAD_Y - 15 )
			shift_y = 0;
		DEBUG( "shiftx: %d, shift_y: %d", shift_x, shift_y );

		dbg_copy_stars( &img2, &star_base, DBG_PAD_X + shift_x, DBG_PAD_Y + shift_y );
		
		y_int_save( img2.data, img2.width, img2.height, "img2.jpg" );
		
		// TODO we could save a lot of time when calculating the FFT of the first pic in advance
		if( pixPhaseCorrelate_GPU( &img1, &img2, &peak, &xloc, &yloc ) )
		{
			ERROR("cannot phase correlate");
			goto error;		
		}
		else
	    	DEBUG("peak: %f, x: %d, y:%d", peak, xloc, yloc );

		
		// TODO Motor control goes here

		shift_x += xloc;
		shift_y += yloc;
		
		if( abs(shift_x) > DBG_PAD_X - 15  )
			shift_x = 0;
		if( abs(shift_y) > DBG_PAD_Y - 15 )
			shift_y = 0;

	} while( keep_looping );

	
	
	vcos_semaphore_delete(&callback_data.complete_semaphore);
	

	
	// free_fft_gpu( frame1_fft_gpu );

	free( img1.data );
	free( img2.data );

		
    mmal_component_destroy( camera_component );

	return 0;

error:
	vcos_semaphore_delete(&callback_data.complete_semaphore);


	if( still_port ) {
		mmal_port_disable( still_port );
	}
	if( camera_component )
	{
		mmal_component_disable( camera_component );
		mmal_component_destroy( camera_component );
	}
	
	return (-1);

}

