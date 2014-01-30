rpi-mmalcam
===========

RPI MMAL camera

Done
- clone from mmaltest
- remove encoder
- write to memory buffer
- debug-write Y-data to jpeg w/ gd
- capture second frame
- fft Y-data first frame
- timing fft
- calculate phase shift

Todo
- control callback / set camera parameters like exposure time, white balance, iso
- improve phase shift for higher resolutions. works fine up to 1536 x 1152
- MAKE MUCH FASTER!
- add/average multiple images
- low light performance
