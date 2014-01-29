#include <stdio.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_encodings.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_util_params.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_connection.h>
#include <bcm_host.h>
#include "mmalyuv.h"
#include "log.h"



static void y_writer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	int complete = 0;
	
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;	
	
	// DEBUG("callback aufgerufen");
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
	
	// DEBUG("callback return");
}

int main(int argc, char *argv[])
{
	MMAL_COMPONENT_T *camera_component;
	MMAL_STATUS_T status;
	VCOS_STATUS_T vcos_status;
	MMAL_ES_FORMAT_T *format_out;
	MMAL_POOL_T *pool_out;
	MMAL_PORT_T *still_port = NULL;
    PORT_USERDATA callback_data;
	void *img1, *img2;
	int num, q;

	

	// TODO
	log_verbose(100);

	bcm_host_init();
	
	
	/* 
	*  Kamera-Komponente
	*
	*/
	DEBUG("Starting camera component creation stage");

	status = mmal_component_create( MMAL_COMPONENT_DEFAULT_CAMERA, &camera_component);
	if( status != MMAL_SUCCESS )
	{
		ERROR("cannot create component %s",MMAL_COMPONENT_DEFAULT_CAMERA);
		return(-1);
	}
	
	if( !camera_component->output_num )
	{
		ERROR("Camera doesn't have output ports - which ist really strange");
		mmal_component_destroy( camera_component );
		return(-1);
	}
	
	still_port = camera_component->output[MMAL_CAMERA_CAPTURE_PORT];

	format_out = still_port->format;
	
    format_out->encoding = MMAL_ENCODING_BGR24;
    format_out->encoding_variant = MMAL_ENCODING_BGR24;

	format_out->es->video.width = MAX_CAM_WIDTH;
	format_out->es->video.height = MAX_CAM_HEIGHT;
	format_out->es->video.crop.x = 0;
	format_out->es->video.crop.y = 0;
	format_out->es->video.crop.width = MAX_CAM_WIDTH;
	format_out->es->video.crop.height = MAX_CAM_HEIGHT;
	format_out->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
	format_out->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;
	
	if( still_port->buffer_num < still_port->buffer_num_min )
		still_port->buffer_num  = still_port->buffer_num_min;

	still_port->buffer_size = still_port->buffer_size_recommended;

	status = mmal_port_format_commit( still_port );
	if( status != MMAL_SUCCESS )
	{
		ERROR( "cannot create commit format" );
		mmal_component_destroy( camera_component );
		return(-1);
	}
	
	status = mmal_component_enable( camera_component );
	if( status != MMAL_SUCCESS )
	{
		ERROR( "Cannot enable camera component" );
		mmal_component_destroy( camera_component );
		return(-1);
	}
	
    pool_out = mmal_port_pool_create(still_port, still_port->buffer_num, still_port->buffer_size);
	
    if (!pool_out)
    {
       ERROR("Failed to create buffer header pool for encoder output port %s", still_port->name);
	   goto error;
    }
	
	/*
	* Ende Kamera–Komponente
	*
	*/
	

	callback_data.camera_pool = pool_out;
	callback_data.image_buffer = NULL;

	DEBUG("creating semaphore");
    vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "mmalcam-sem", 0);
    vcos_assert(vcos_status == VCOS_SUCCESS);
	
	
	DEBUG("sleeping for a second or so to have exposure adjust automatically");
	// results show: sleeping time can be much shorter, tested down to 2ms
	// although then exposure and awb seem to be somewhat off and image size huge 
	// next frames can go with a much shorter exposure like 30ms
    vcos_sleep(200);

	img1 = calloc( MAX_CAM_WIDTH_PADDED * MAX_CAM_HEIGHT_PADDED, sizeof( uint8_t ));
	if( !img1 )
	{
		ERROR("out of memory img1");
		goto error;
	}
	
	callback_data.image_buffer = img1;
	callback_data.max_bytes = MAX_CAM_WIDTH_PADDED * MAX_CAM_HEIGHT_PADDED;
	callback_data.bytes_written = 0;
	
    still_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;
	
	
	DEBUG("enabling encoder output port w/ callback");
	status = mmal_port_enable( still_port, y_writer_callback );
	if( status != MMAL_SUCCESS )
	{
		ERROR("failed to enable camera still output port");
		goto error;
	}

	
	DEBUG("queue_length-Kram");
    num = mmal_queue_length(pool_out->queue);

    for ( q=0; q<num; q++ )
    {
       MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get( pool_out->queue );

       if (!buffer)
	   {
          ERROR("Unable to get a required buffer %d from pool queue", q);
		  goto error;
	  }

       if (mmal_port_send_buffer(still_port, buffer)!= MMAL_SUCCESS)
	   {
          ERROR("Unable to send a buffer to encoder output port (%d)", q);
		  goto error;
	  }
    }
	
	DEBUG("starting capture");
    if ( mmal_port_parameter_set_boolean( still_port, MMAL_PARAMETER_CAPTURE, 1 ) != MMAL_SUCCESS)
    {
       ERROR("Failed to start capture");
    }
    else
    {
       // Wait for capture to complete
       // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
       // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
		DEBUG("wait semaphore");	
		vcos_semaphore_wait( &callback_data.complete_semaphore );
		DEBUG( "Finished capture" );
    }
	
	DEBUG("Callback_data max_bytes %d bytes_written %d", callback_data.max_bytes, callback_data.bytes_written );
	
	DEBUG("end capture first shot");
	
    vcos_sleep(1000);
	
	/*
	* Second Frame
	*
	*/
	img2 = calloc( MAX_CAM_WIDTH_PADDED * MAX_CAM_HEIGHT_PADDED, sizeof( uint8_t ));
	if( !img2 )
	{
		ERROR("out of memory img2");
		goto error;
	}
	
	callback_data.image_buffer = img2;
	callback_data.max_bytes = MAX_CAM_WIDTH_PADDED * MAX_CAM_HEIGHT_PADDED;
	callback_data.bytes_written = 0;
	
    still_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;
	
	// DEBUG("queue_length-Kram");
    num = mmal_queue_length(pool_out->queue);

	// TODO I don't really understand what this does.
    for ( q=0; q<num; q++ )
    {
       MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get( pool_out->queue );

       if (!buffer)
	   {
          ERROR("Unable to get a required buffer %d from pool queue", q);
		  goto error;
	  }

       if (mmal_port_send_buffer(still_port, buffer)!= MMAL_SUCCESS)
	   {
          ERROR("Unable to send a buffer to encoder output port (%d)", q);
		  goto error;
	  }
    }
	
	DEBUG("starting 2nd capture");
    if ( mmal_port_parameter_set_boolean( still_port, MMAL_PARAMETER_CAPTURE, 1 ) != MMAL_SUCCESS)
    {
       ERROR("Failed to start 2nd capture");
    }
    else
    {
       // Wait for capture to complete
       // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
       // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
		DEBUG("wait semaphore");	
		vcos_semaphore_wait( &callback_data.complete_semaphore );
		DEBUG( "Finished capture" );
    }

	DEBUG("Callback_data max_bytes %d bytes_written %d", callback_data.max_bytes, callback_data.bytes_written );	
	
	DEBUG("end capture second shot");

	free( img1 );
	free( img2 );

    /*
	* End second frame
	*
	*/
	

		
    mmal_component_destroy( camera_component );

	return 0;

error:	
	mmal_component_destroy( camera_component );
	
	return (-1);

}

