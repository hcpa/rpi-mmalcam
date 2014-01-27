#include <stdio.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/mmal_encodings.h>
#include <interface/mmal/util/mmal_util.h>
#include <bcm_host.h>
#include "mmaltest.h"
#include "log.h"


static void input_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	mmal_buffer_header_release(buffer);
}

int main(int argc, char *argv[])
{
	MMAL_COMPONENT_T *camera_component;
	MMAL_STATUS_T status;
	MMAL_ES_FORMAT_T *format_out;
	MMAL_POOL_T *pool_out;
	MMAL_PORT_T *still_port = NULL;
	
	bcm_host_init();

	status = mmal_component_create( MMAL_COMPONENT_DEFAULT_CAMERA, &camera_component);
	if( status != MMAL_SUCCESS )
	{
		ERROR("cannot create component %s",MMAL_COMPONENT_DEFAULT_CAMERA);
		return(-1);
	}
	
	if( !camera_component->output_num )
	{
		ERROR("Camera doesn't have output ports - which ist really strange");
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
	still_port->buffer_size = still_port->buffer_size_min;

	status = mmal_port_format_commit( still_port );
	if( status != MMAL_SUCCESS )
	{
		ERROR( "cannot create commit format" );
		return(-1);
	}
	
	status = mmal_component_enable( camera_component );
	if( status != MMAL_SUCCESS )
	{
		ERROR( "Cannot enable camera component" );
		return(-1);
	}
	
	
	pool_out = mmal_port_pool_create( still_port, still_port->buffer_num, still_port->buffer_size );
	if( !pool_out )
	{
		ERROR("Cannot create buffer header pool (concept not fully understood) for camera still port %s", still_port->name );
		return(-1);
	}
	

    mmal_component_destroy( camera_component );

	printf("Hello World!\n");
	return 0;
}

