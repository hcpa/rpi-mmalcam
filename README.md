rpi-mmalcam
===========

RPI MMAL camera, RPI GPU calculations

Done
- clone from mmaltest
- remove encoder
- write to memory buffer
- debug-write Y-data to jpeg w/ gd
- capture second frame
- fft Y-data first frame
- timing fft
- calculate phase shift
- use gpu_fft code provided http://www.raspberrypi.org/archives/5934
- implement phase correlation with gpu_fft
- test with sample image w/o noise
- otimized GPU_FFT by using the symmetry of real-to-complex DFTs, saves around 50%
- makefile sets suid for mmalyuv. It's convenient but a security risk
- use real-world star pics. YEAH!
- stabilize after failed fft_gpu -> seems like camera access is not returned in case
  of gpu error

Todo
- add/average multiple images
- denoise image
- control callback / set camera parameters like exposure time, white balance, iso
  see discussion here http://www.raspberrypi.org/forum/viewtopic.php?f=43&t=61445
	raspistill -w 1024 -h 1024 -t 1000 -ss 500000 -ex sports -ev 25 
	mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_SHUTTER_SPEED, 500000);
	mmal_port_parameter_set_int32(camera->control, MMAL_PARAMETER_EXPOSURE_COMP , 25);
	// mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_ISO, 1600);
    MMAL_PARAMETER_EXPOSUREMODE_T exp_mode = {{MMAL_PARAMETER_EXPOSURE_MODE,sizeof(MMAL_PARAM_EXPOSUREMODE_SPORTS)}, MMAL_PARAM_EXPOSUREMODE_SPORTS};
    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &exp_mode.hdr));
- improve phase shift for higher resolutions. works fine up to 1536 x 1152
- optimize
- low light performance
- optimize for permanent run, memory leaks, files, etc

Open questions: how to detect rotation? Is it relevant?
