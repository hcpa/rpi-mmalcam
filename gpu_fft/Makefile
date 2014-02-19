H = hex/shader_256.hex \
    hex/shader_512.hex \
    hex/shader_1k.hex \
    hex/shader_2k.hex \
    hex/shader_4k.hex \
    hex/shader_8k.hex \
    hex/shader_16k.hex \
    hex/shader_32k.hex \
    hex/shader_64k.hex \
    hex/shader_128k.hex

C = mailbox.c gpu_fft.c gpu_fft_twiddles.c gpu_fft_shaders.c
O = $(C:.c=.o)

B = libgpu_fft.a

all: $(B)

$(B):	$(H) $(O)
	ar rcs $(B) $(O)

clean:
	rm -f $(B) $(O)
