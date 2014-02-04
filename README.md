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

Todo
- stabilize after failed fft_gpu -> seems like camera access is not returned in case
  of gpu error
- control callback / set camera parameters like exposure time, white balance, iso
- can it run w/o sudo?
- improve phase shift for higher resolutions. works fine up to 1536 x 1152
- MAKE MUCH FASTER!
- add/average multiple images
- low light performance
- denoise image
- optimize for permanent run
