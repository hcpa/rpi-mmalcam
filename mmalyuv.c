#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <gd.h>

#include <mcheck.h>

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

#define HC_DEBUG

static int keep_looping;

#ifdef HC_DEBUG
static int shift_x;
static int shift_y;
#endif /* HC_DEBUG */

#ifdef HC_DEBUG
/* 
 *   Returns number of milliseconds since whenever
 *   Only useful to calculate time differences
 */
static uint32_t millis()
{
	struct timespec tt;
	clock_gettime(CLOCK_MONOTONIC,&tt);
	return tt.tv_sec*1000l + tt.tv_nsec/1000000;
}
#endif /* HC_DEBUG */


/*
 *  Abort main loop on every signal
 */
static void signal_handler(int signal_number)
{
    // Going to abort on all other signals
	ERROR("Aborting program...");
	keep_looping = 0;
	return;
}


#ifdef HC_DEBUG
/*
 *  Load greyscale(!) pic from "sterne_pad.png", convert to array of bytes
 *  returns -1 if file cannot be opened or image not be created by gd.
 *  This is assuming a greyscale file. If RGB, it's only using the blue 
 *  component. 
 */
static int dbg_load_stars( pix_y_t *pic )
{
	int i, j, w, h;
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
	
	w = gdImageSX(image) < pic->width?gdImageSX(image):pic->width;
	h = gdImageSY(image) < pic->height?gdImageSY(image):pic->height;

	dat = pic->data;
	for( j = 0; j < h; j++ )
		for( i = 0; i < w; i++,dat++ )
		{
			// This is working fine for grayscale pics. For RGB it just takes the blue component into account.
			*dat = 0xff & gdImageGetPixel( image, i, j );
		}
	gdImageDestroy( image );
	return(0);
}


/*
 * copy (luminance) data from src to dst, offset by sh_x and sh_y both >= 0
 * returns -1 if index out of bounds
 */
static int dbg_copy_stars( pix_y_t *dst, pix_y_t *src, unsigned sh_x, unsigned sh_y )
{
	unsigned j;
	uint8_t *src_b, *dst_b;
	
	if( dst->width + sh_x > src->width )
		return (-1);
	if( dst->height + sh_y > src->height )
		return (-1);
	
	
	for( j = 0; j < dst->height; j++ )
	{
		src_b = src->data + (sh_y+j)*src->width + sh_x;
		dst_b = dst->data + j*dst->width;
		memcpy( dst_b, src_b, dst->width );
	}
	return (0);
}
#endif /* HC_DEBUG */


/*
 *  Callback to get YUV frame data from camera, average multiple frames
 *  
 *	Only pData->max_bytes bytes are saved, this is calculated to be only the 
 *  Y (luminance) data at the beginning of the memory block. 
 *   
 *  First frame (pData->current_frame) is copied, other frames are 
 *  added in using floating average
 */
static void y_writer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	int complete = 0;
	
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;	
	
	if( pData )
	{
		// 									copy max_bytes at most, calculated to be width*height(*sizeof(byte))  
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
		
		// TODO: Could I set complete=1 already if the Y-data is finished? Or do I have to wait until 
		// flags are set?  (MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED)
		if( buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED) )
			complete = 1;
	}
	else
	{
		ERROR("callback received encoder buffer with no user data");
	}

	mmal_buffer_header_release(buffer);
	
    // TODO I have no idea what this is doing, the "manual" in mmal.h doesn't mention it
	// but the thing breaks without so let's do it
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

/**
 *  buffer header callback function for camera control
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	
   WARN("Camera control callback  cmd=0x%08x", buffer->cmd);

   mmal_buffer_header_release(buffer);
}



/* 
 * Prepare the camera component and the still port, set the format for the still port, 
 * enable camera component, create buffer pool
 * 
 * This is more or less a direct copy from RaspiStillYUV.c. This is made a function 
 * to be out of the way in main()
 *
 * return -1 on error
 */
static int prepare_camera( MMAL_COMPONENT_T **camera_component, MMAL_PORT_T **still_port, MMAL_POOL_T **pool_out, int night )
{
	MMAL_STATUS_T status;
	MMAL_ES_FORMAT_T *format_out;	
	
	MSG("prepare_camera");

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


    status = mmal_port_enable((*camera_component)->control, camera_control_callback);

    if (status)
	{
		ERROR("Unable to enable control port : error %d", status);
		mmal_component_destroy( *camera_component );
		return(-1);
    }

	if( night )
	{
		mmal_port_parameter_set_uint32( (*camera_component)->control, MMAL_PARAMETER_SHUTTER_SPEED, 500000);
		mmal_port_parameter_set_int32(  (*camera_component)->control, MMAL_PARAMETER_EXPOSURE_COMP, 25);
		mmal_port_parameter_set_uint32( (*camera_component)->control, MMAL_PARAMETER_ISO, 1600);

	    MMAL_PARAMETER_EXPOSUREMODE_T exp_mode = {{MMAL_PARAMETER_EXPOSURE_MODE,sizeof(exp_mode)}, MMAL_PARAM_EXPOSUREMODE_SPORTS};
	    mmal_port_parameter_set((*camera_component)->control, &exp_mode.hdr);
	}



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
		ERROR( "cannot commit image format" );
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

/*
 * Capture frames: set buffer pool, set capture parameter, wait for semaphore
 *
 * return (-1) if error with buffer pool or capture  
 */
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
	pix_y_t img1, img2;
	// struct GPU_FFT *frame1_fft_gpu;
	float peak;
	int32_t xloc, yloc;
	int night = 1;

#ifdef HC_DEBUG
	pix_y_t star_base;
	uint32_t bef, aft; 
#endif /* HC_DEBUG */
	
#ifdef HC_DEBUG
	// TODO verbose level
	log_verbose(100);
#else
	log_verbose(0);
#endif /* HC_DEBUG */

	bcm_host_init();
	
	if( geteuid() != 0 )
	{
		ERROR("This program needs r/w access to /dev/mem. It must be run as root (suid).\nExiting...");
		exit(-1);
	}
	
    signal(SIGINT, signal_handler);
	
	
	if( argc > 1 )
	{
		if( strncmp( argv[1], "-day", 4 ) == 0 )
			night = 0;
		else if( strncmp( argv[1], "-night", 6 ) == 0 )
			night = 1;
	}


#ifdef HC_DEBUG
	srandom(millis());


	star_base.width = MAX_CAM_WIDTH_PADDED + 2*DBG_PAD_X;
	star_base.height = MAX_CAM_HEIGHT_PADDED + 2*DBG_PAD_Y;
	star_base.data = calloc( (MAX_CAM_WIDTH_PADDED + 2*DBG_PAD_X) * (MAX_CAM_HEIGHT_PADDED + 2*DBG_PAD_Y), sizeof( uint8_t ) );
	if( !star_base.data ) {
		ERROR("out of memory data");
		exit(-1);
	}
	dbg_load_stars( &star_base );
#endif /* HC_DEBUG */	

	if( prepare_camera( &camera_component, &still_port, &pool_out, night ) )
	{
		ERROR( "failed to prepare camera" );
		goto error;
	}
	

	callback_data.camera_pool = pool_out;
	callback_data.image_buffer = NULL;

	DEBUG("creating semaphore");
    vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "mmalcam-sem", 0);
    vcos_assert(vcos_status == VCOS_SUCCESS);
	
	
	if( !night )
	{
		DEBUG("sleeping for some time have exposure adjust automatically");
		// results show: sleeping time can be much shorter, tested down to 2ms
		// although then exposure and awb seem to be somewhat off and image size huge 
    	vcos_sleep(300);
	} else {
    	vcos_sleep(2);		
	}

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
	
	
#ifdef HC_DEBUG
	// DEBUG
	// overwrite first frame with stars 
	dbg_copy_stars( &img1, &star_base, DBG_PAD_X, DBG_PAD_Y );
#endif /* HC_DEBUG */	
	
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


	keep_looping = 1;

#ifdef HC_DEBUG
	bef = millis();
#endif /* HC_DEBUG */

	do {
		callback_data.bytes_written = 0;
		callback_data.current_frame = 0;

		if( capture_frames( &callback_data, still_port, pool_out, MAX_FRAMES ) ) {
			ERROR( "failed to capture shot x" );
			goto error;
		}

#ifdef HC_DEBUG
		// DEBUG overwrite frame with stars
		shift_x += (random()&0x1f) - 16;
		shift_y += (random()&0x1f) - 16;

		if( abs(shift_x) > DBG_PAD_X - 15  )
			shift_x = 0;
		if( abs(shift_y) > DBG_PAD_Y - 15 )
			shift_y = 0;
		DEBUG( "shiftx: %d, shift_y: %d", shift_x, shift_y );

		dbg_copy_stars( &img2, &star_base, DBG_PAD_X + shift_x, DBG_PAD_Y + shift_y );
#endif /* HC_DEBUG */
		
	
		// TODO we could save a lot of time when calculating the FFT of the first pic in advance
		if( pixPhaseCorrelate_GPU( &img1, &img2, &peak, &xloc, &yloc ) )
		{
			ERROR("cannot phase correlate");
			goto error;		
		}
		else
	    	MSG("peak: %.2f, x: %d, y:%d", peak, xloc, yloc );

		
		// TODO Motor control goes here

#ifdef HC_DEBUG
		shift_x -= xloc;
		shift_y -= yloc;
		
		if( abs(shift_x) > DBG_PAD_X - 15  )
			shift_x = 0;
		if( abs(shift_y) > DBG_PAD_Y - 15 )
			shift_y = 0;
		
		aft = millis();
		DEBUG("loop in %5d millis", aft-bef );
		bef = aft;
#endif /* HC_DEBUG */
	} while( keep_looping );

	
	
	vcos_semaphore_delete(&callback_data.complete_semaphore);
	
#ifdef HC_DEBUG
	free( star_base.data );
#endif /* HC_DEBUG */

	
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

