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
	mmal_buffer_header_release(buffer);
}

int main(int argc, char *argv[])
{
	MMAL_COMPONENT_T *camera_component;
	MMAL_COMPONENT_T *encoder_component;
    MMAL_CONNECTION_T *encoder_connection;
	MMAL_STATUS_T status;
	MMAL_ES_FORMAT_T *format_out;
	MMAL_POOL_T *pool_out;
	MMAL_PORT_T *still_port = NULL;
	MMAL_PORT_T *encoder_input_port = NULL, *encoder_output_port = NULL;
	

	// TODO
	log_verbose(100);

	bcm_host_init();
	
	
	/* 
	*  Kamera-Komponente
	*
	*/

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
	// format_out->encoding = MMAL_ENCODING_I420;
	// format_out->encoding_variant = MMAL_ENCODING_I420;
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
		
	mmal_component_destroy( encoder_component );
    mmal_component_destroy( camera_component );

	printf("Hello World!\n");
	return 0;

error:	
	mmal_component_destroy( encoder_component );
	mmal_component_destroy( camera_component );
	
	return (-1);

}

