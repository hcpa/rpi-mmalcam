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

Todo
- stabilize after failed fft_gpu -> seems like camera access is not returned in case
  of gpu error
- control callback / set camera parameters like exposure time, white balance, iso
- improve phase shift for higher resolutions. works fine up to 1536 x 1152
- optimize
- add/average multiple images
- low light performance
- denoise image
- optimize for permanent run, memory leaks, files, etc

Open questions: how to detect rotation? Is it relevant?
