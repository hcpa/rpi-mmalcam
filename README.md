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
- add/average multiple images
- Branch "permanent_run": optimize for permanent run, memory leaks, files, etc
  structure
  dbg load stars( padding 128 pixels each side ) - 1280x1280
    - first shot  (x frames)
  dbg copy center of stars to first frame
    - fft first shot
  	- loop 1..until interrupted
  	      - sleep some time 
          - new shot (x frames)
	    dbg copy stars (+shift_x +/-random_x, shift_y +/-ranyom_y) to new shot (log total amount of shift (x,y) someplace)
		  - phase correlation 1st and new shot -> (shift_x, shift_y)
		  - send (shift_x,shift_y) to motor control (nop)
		  - dbg: log (shift_x, shift_y), save it in global variables
	- end loop


Todo
- check for memory leaks etc
- it's time to connect it to arduino. uiuiui.
- low light performance
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

Open questions: how to detect rotation? Is it relevant?
