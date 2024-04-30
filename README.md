encode RGBx frame to h.264 using gstreamer

h264Encoder inputs: width * height, output: h264 NALs stream. you need to install gstreamer.

JpegEncoder inputs: width * height, output: encoded img. and it also print  the time of each single frame.

# make
h264:
gcc -o h264Encoder h264Encoder.c $(pkg-config --libs gstreamer-1.0 gstreamer-app-1.0) $(pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0)

# use
./h264Encoder width height

JPEG:
gcc -o JPEGEncoder JPEGEncoder.c -ljpeg

