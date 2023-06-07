#include <stddef.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <zlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <jpeglib.h>

static unsigned char* rawdata;

unsigned int MyGetTickCount() {
  struct timeval tim;
  gettimeofday(&tim, NULL);
  unsigned int t = ((tim.tv_sec * 1000) + (tim.tv_usec / 1000)) & 0xffffffff;
  return t;
}

unsigned char* jpeg_compress (int quality, uint32_t image_width, uint32_t image_height, int bpp, FILE *of)
{
    int start_ticks = MyGetTickCount();

    uint32_t jpeg_size;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];/* pointer to JSAMPLE row[s] */
    int row_stride;/* physical row width in image buffer */
    unsigned char* out_bufer=0;
    long unsigned int length=0;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);


    jpeg_mem_dest(&cinfo,&out_bufer, &length);

    cinfo.image_width = image_width;         /* image width and height, in pixels */
    cinfo.image_height = image_height;
    cinfo.input_components = bpp;            /* # of color components per pixel */
    if(bpp == 4)
        cinfo.in_color_space = JCS_EXT_BGRX;     /* colorspace of input image */
    else
        cinfo.in_color_space = JCS_EXT_RGB;     /* colorspace of input image */
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE); /* limit to baseline-JPEG values */
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = image_width * bpp;            /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height)
    {
        //row_pointer[0] = & rawdata[cinfo.next_scanline * row_stride];
        row_pointer[0] = rawdata+cinfo.next_scanline * row_stride;
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);

    int ms = MyGetTickCount() - start_ticks;
    printf("%d\n",ms);

    fwrite(out_bufer, 1, length, of);
    return out_bufer;
}

int main(int argc, char *argv[]){
    FILE *of;
    int width, height;
    //unsigned char* rawdata;
    int jpegQuality = 70;
    uint32_t compressed_size = 0;

    /* Check input arguments */
    if (argc < 2 || argc > 5) {
      fprintf(stderr, "Usage: %s rawfile width height outfile [quality]\n"
                      "Rawfile must be one raw frame of GRAY8, outfile will be "
                      "JPG encoded. 10 outfiles will be created\n",
              argv[0]);
      return -1;
    }
    // Read command line arguments
    width = atoi(argv[1]);
    height = atoi(argv[2]);

    // Validate command line arguments
    if (width < 100 || width > 4096 || height < 100 || height > 4096) {
      printf("width and/or height or quality is bad, not running conversion\n");
      return -1;
    }

    of = fopen("/root/Desktop/jpeg_testout.mp4", "wb");
    rawdata = malloc(width * height *3);

    // Init raw frame
    srand(time(NULL));
    for (int i = 0; i < width * height; ++i) {
      rawdata[i*3] = rand() % 255;     //R
      rawdata[i*3+1] = rand() % 255;   //G
      rawdata[i*3+2] = rand() % 255;   //B
      //rawdata[i*4+3] = 255;                  //A
    }

    for(int i=0; i<300; i++){
        memset(rawdata+i*width,0xff,width);
        memset(rawdata+i*width,0,width);
        jpeg_compress(jpegQuality, width, height, 3, of);
    }
    //jpeg_compress(jpegQuality, width, height, 3, of);

    fclose(of);

    return 0;

}