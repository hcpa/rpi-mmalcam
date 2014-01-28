#include <stdio.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_encodings.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_util_params.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_connection.h>
#include <bcm_host.h>
#include "mmaltest.h"
#include "log.h"



static void encoder_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	int complete = 0;
	
    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;	
	
	DEBUG("callback aufgerufen");
	if( pData )
	{
		int bytes_written = buffer->length;
		if( buffer->length && pData->file_handle )
		{
			mmal_buffer_header_mem_lock( buffer );
			bytes_written = fwrite(buffer->data, 1, buffer->length, pData->file_handle );
			mmal_buffer_header_mem_unlock( buffer );
		}
		
		if( bytes_written != buffer->length )
		{
			ERROR("unable to write %d bytes from buffer to file", buffer->length );
			complete = 1;
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

       new_buffer = mmal_queue_get( pData->encoder_pool->queue );

       if( new_buffer )
          status = mmal_port_send_buffer(port, new_buffer);

       if (!new_buffer || status != MMAL_SUCCESS)
          ERROR("Unable to return a buffer to the encoder port");
    }
		
    if (complete)
       vcos_semaphore_post(&(pData->complete_semaphore));
	
	DEBUG("callback return");
}

int main(int argc, char *argv[])
{
	MMAL_COMPONENT_T *camera_component;
	MMAL_COMPONENT_T *encoder_component;
    MMAL_CONNECTION_T *encoder_connection;
	MMAL_STATUS_T status;
	VCOS_STATUS_T vcos_status;
	MMAL_ES_FORMAT_T *format_out;
	MMAL_POOL_T *pool_out;
	MMAL_PORT_T *still_port = NULL;
	MMAL_PORT_T *encoder_input_port = NULL, *encoder_output_port = NULL;
    PORT_USERDATA callback_data;
	FILE *output_file;
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
	
    format_out->encoding = MMAL_ENCODING_OPAQUE;

	format_out->es->video.width = MAX_CAM_WIDTH;
	format_out->es->video.height = MAX_CAM_HEIGHT;
	format_out->es->video.crop.x = 0;
	format_out->es->video.crop.y = 0;
	format_out->es->video.crop.width = MAX_CAM_WIDTH;
	format_out->es->video.crop.height = MAX_CAM_HEIGHT;
	format_out->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
	format_out->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;
	
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
	
	/*
	* Ende Kamera–Komponente
	*
	*/
	

	
	/*
	* Encoder-Komponente
	*
	*/
	DEBUG("Starting encoder component creation stage");
	
	status = mmal_component_create( MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &encoder_component );
	if( status != MMAL_SUCCESS )
	{
		ERROR("cannot create component %s", MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER );
		mmal_component_destroy( camera_component );
		return (-1);
	}
	
    if (!encoder_component->input_num || !encoder_component->output_num)
    {
       ERROR("JPEG encoder doesn't have input/output ports");
       goto error; //TODO: we have to close down the camera in case of error
    }
	
    encoder_input_port = encoder_component->input[0];
    encoder_output_port = encoder_component->output[0];
	
    // TODO: what does that mean? 
   // We want same format on input and output
    mmal_format_copy(encoder_output_port->format, encoder_input_port->format);
	
    encoder_output_port->format->encoding = MMAL_ENCODING_JPEG;

	encoder_output_port->buffer_num  = encoder_output_port->buffer_num_min;
	encoder_output_port->buffer_size = encoder_output_port->buffer_size_recommended;

    status = mmal_port_format_commit(encoder_output_port);

    if (status != MMAL_SUCCESS)
    {
       ERROR("Unable to set format on encoder output port");
       goto error;
    }

    // Set the JPEG quality level
    status = mmal_port_parameter_set_uint32(encoder_output_port, MMAL_PARAMETER_JPEG_Q_FACTOR, 85 );

    if (status != MMAL_SUCCESS)
    {
       ERROR("Unable to set JPEG quality");
       goto error;
    }

    //  Enable component
    status = mmal_component_enable( encoder_component );

    if (status  != MMAL_SUCCESS )
    {
       ERROR("Unable to enable video encoder component");
       goto error;
    }

    /* Create pool of buffer headers for the output port to consume */
    pool_out = mmal_port_pool_create(encoder_output_port, encoder_output_port->buffer_num, encoder_output_port->buffer_size);
	
    if (!pool_out)
    {
       ERROR("Failed to create buffer header pool for encoder output port %s", encoder_output_port->name);
	   goto error;
    }

	/*
	*  Ende Encoder-Komponente
	*
	*/
	
	
	/* 
	* Verbindung Still-Port Kamera zu Encoder Input-Port
	*
	*/
	DEBUG("Starting component connection stage");
	
    status =  mmal_connection_create( &encoder_connection, still_port, encoder_input_port, 
										MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
	if (status != MMAL_SUCCESS)
	{
		ERROR("unable to create connection between still port and encoder input port");
		goto error;
	}
	
	status = mmal_connection_enable( encoder_connection );
	if( status != MMAL_SUCCESS )
	{
		ERROR("unable to enable connection");
		goto error;
	}
	
    if (status != MMAL_SUCCESS)
    {
       ERROR("%s: Failed to connect camera video port to encoder input", __func__);
       goto error;
    }

	/*
	* Ende Verbindung Still-Port Kamera zu Encoder Input-Port
	*
	*/


	callback_data.encoder_pool = pool_out;
	callback_data.file_handle = 0;

	DEBUG("creating semaphore");
    vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "mmalcam-sem", 0);
    vcos_assert(vcos_status == VCOS_SUCCESS);
	
	
	DEBUG("sleeping for a second or so to have exposure adjust automatically");
	DEBUG("next frames can go with a much shorter exposure like 30ms");
    vcos_sleep(50);

	output_file = fopen("mmalimage.jpg", "wb");
	if( !output_file )
	{
		ERROR("unable to open mmalimage.jpg");
		goto error;
	}
	
	callback_data.file_handle = output_file;
	
    encoder_output_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;
	
	
	DEBUG("enabling encoder output port w/ callback");
	mmal_port_enable( encoder_output_port, encoder_callback );
	
	
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

       if (mmal_port_send_buffer(encoder_output_port, buffer)!= MMAL_SUCCESS)
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
	
	
	DEBUG("end capture first shot");

		
	mmal_component_destroy( encoder_component );
    mmal_component_destroy( camera_component );

	return 0;

error:	
	mmal_component_destroy( encoder_component );
	mmal_component_destroy( camera_component );
	
	return (-1);

}

