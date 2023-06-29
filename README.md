# h264StreamEncoder
输入你想编码的图像宽度和高度，本项目中有两种图像编码方式（JPEG 和 H.264），将自动生成width*height尺寸的BGR图像并进行编码。

当然，你也可以修改gstreamer的相关参数，修改输入或者输出的数据格式，比如RGBA,BGRx等等。

H.264的编码基于GStreamer，在使用前请先安装

automatically encode RGBx streame to h264 stream using gstreamer

h264Encoder inputs: width * height, output: h264 NALs stream. you need to install gstreamer.

JpegEncoder inputs: width * height, output: encoded img. and it also print  the time of each single frame.

# make
h264:
gcc -o h264Encoder h264Encoder.c $(pkg-config --libs gstreamer-1.0 gstreamer-app-1.0) $(pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0)

use: ./h264Encoder width height

JPEG:
gcc -o JPEGEncoder JPEGEncoder.c -ljpeg

