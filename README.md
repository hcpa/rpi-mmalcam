rpi-mmalcam
===========

RPI MMAL camera, RPI GPU calculations

OPEN QUESTION: WHAT DOES THIS MEAN?
  GPU_FFT performs multiple passes between ping-pong buffers.  The final output
  lands in the same buffer as input after an even number of passes.  Transforms
  where log2_N=12...16 use an odd number of passes and the final result is left
  out-of-place.  The input data is never preserved.


Puh, this was a tough one - and still is.

I think I have to do an excursion into FFT-land.
Things to do:
- Make sure, I understand the FFT results
	- Explore L_WITH_SHIFTING from ipl example
       --> alternates pos/neg Values by multiplication with pow(-1,i+j)
       --> passed through all funktions, evaluated only in conversion between |N and |R
       --> set to L_WITH_SHIFTING *only* in pixApplyFilterGray
	- find samples of code which dumps FFT-results to file
       --> 
- Make sure, FFTW and GPU_FFT deliver the same results
	- run a complex-to-complex 2D-FFT with FFTW
- Why do the results from GPU_FFT look like every other line is missing?

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

Todo
- implement phase correlation with fft
- test with sample image w/o noise
- stabilize after failed fft_gpu -> seems like camera access is not returned in case
  of gpu error
- control callback / set camera parameters like exposure time, white balance, iso
- improve phase shift for higher resolutions. works fine up to 1536 x 1152
- MAKE MUCH FASTER!
- add/average multiple images
- low light performance
- denoise image
- optimize for permanent run

