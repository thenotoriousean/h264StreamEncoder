#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include "h264Encoder.h"

#ifndef __cplusplus
typedef unsigned char Bool;
static const Bool False = 0;
static const Bool True = 1;
#endif

// Command line parameters
static char *infile = NULL;
static int width = 0;
static int height = 0;
static char *outfile = NULL;
static int quality = 60;

// Buffer for incoming data (FullHD is max, you can increase it as you need)
//static char rawdata[1920 * 1080 * 3];
static unsigned char* rawdata;
static unsigned char* pRet;
static int pSize;
static FILE *of;

// Flags to indicate to parent thread that GstreamerThread started and finished
static volatile Bool bGstreamerThreadStarted = False;
static volatile Bool bGstreamerThreadFinished = False;

static GstElement *appsrc;
static GstElement *appsink;
static GstElement *pipeline, *vidconv, *x264enc, *qtmux;
static guint bus_watch_id;
static GMainLoop *loop;

//new
static long idx;
static Bool new_sample;
static pthread_mutex_t sample_mutex;
static pthread_cond_t sample_cond;
static int out_num = 1;
static int time_out_count = 0;

static int t[300];
static int i=0;
static int j=0;

unsigned int MyGetTickCount() {
  struct timeval tim;
  gettimeofday(&tim, NULL);
  unsigned int t = ((tim.tv_sec * 1000) + (tim.tv_usec / 1000)) & 0xffffffff;
  return t;
}

// Puts raw data for encoding into gstreamer. Must put exactly width*height bytes.
void PushBuffer() {

  t[i++] = MyGetTickCount();
  //printf("push buffer! %d\n",idx);
  GstFlowReturn ret;
  GstBuffer *buffer;

  int size = width * height *4;
  buffer = gst_buffer_new_allocate(NULL, size, NULL);

  GstMapInfo info;
  gst_buffer_map(buffer, &info, GST_MAP_WRITE);
  unsigned char *buf = info.data;
  memmove(buf, rawdata, size);
  gst_buffer_unmap(buffer, &info);

  //GstClockTime current_time = gst_clock_get_time (gst_system_clock_obtain());

  // 设置视频帧的 PTS 和 DTS
  // GST_BUFFER_PTS(buffer) = current_time;
  // GST_BUFFER_DTS(buffer) = current_time;
  GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer) = idx * 33333333;
  idx++;

  gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
}

void PushStaticImg(){
  fwrite(pRet, 1, pSize, of);
}

// Reads compressed jpeg frame. Will block if there is nothing to read out.
void *PullBuffer() {
  
  unsigned long ms_to_wait=1000*1000;
  struct timespec ts;
  struct timeval tp;

 while(1){
    gettimeofday(&tp, NULL);
    /* Convert from timeval to timespec */
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000 + ms_to_wait * 1000UL;//wait ms_to_wait microseconds
    if(ts.tv_nsec>=1000000000UL)
    {
      ts.tv_nsec-=1000000000UL;
      ++ts.tv_sec;
    }
  pthread_mutex_lock(&sample_mutex);
  while (!new_sample) {  
    switch(pthread_cond_timedwait(&sample_cond, &sample_mutex, &ts)){
      case 0: //have a signal from other thread, continue execution
        time_out_count = 0;
        ms_to_wait=1000*1000; //reset timer
        break;
      case ETIMEDOUT: //timeout is ocured
        //printf("time out !\n");
        ms_to_wait=1000*1000;
        PushStaticImg();
        time_out_count++;
        if(time_out_count == 150){
          gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
          printf("end of stream~~~~~~\n");
          pthread_exit(0);
        }
        //ms_to_wait=20000000;
        //pthread_exit(0);
        break;
      default:
        //printf("Error is occured in pthread_cond_timedwait\n");
        ms_to_wait=1000*1000; //reset timer
        break;
    }
  }
  new_sample = FALSE;
  pthread_mutex_unlock(&sample_mutex);

  //printf("pull buffer!!! %d\n", out_num++);
  GstSample *sample;
  g_signal_emit_by_name(appsink, "pull-sample", &sample);
  //sample = gst_base_sink_get_last_sample(GST_BASE_SINK(appsink));

  // Will block until sample is ready. In our case "sample" is encoded picture.
  //GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));

  if (sample == NULL) {
    fprintf(stderr, "gst_app_sink_pull_sample returned null\n");
    // Tell Gstreamer thread to stop, pushing EOS into gstreamer
    //gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
    continue;
  }

  // Actual compressed image is stored inside GstSample.
  GstBuffer *buffer = gst_sample_get_buffer(sample);
  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);

  // Allocate appropriate buffer to store compressed image
  //char pRet[map.size];
  if(pRet){
    free(pRet);
  }
  pRet = malloc(map.size);
  pSize = map.size;
  // Copy image
  memmove(pRet, map.data, map.size);

  gst_buffer_unmap(buffer, &map);
  gst_sample_unref(sample);

  // Inform caller of image size
  //*outlen = map.size;

  int stop = MyGetTickCount();
  printf("%d\n",stop - t[j++]);
  
  fwrite(pRet, 1, map.size, of);

 }
 printf("pull buffer terminating~\n");
 pthread_exit(0);
 
}

// Bus messages processing, similar to all gstreamer examples
gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
  GMainLoop *loop = (GMainLoop *)data;

  switch (GST_MESSAGE_TYPE(msg)) {

  case GST_MESSAGE_EOS:
    fprintf(stderr, "End of stream\n");
    g_main_loop_quit(loop);
    break;

  case GST_MESSAGE_ERROR: {
    gchar *debug;
    GError *error;

    gst_message_parse_error(msg, &error, &debug);
    g_free(debug);

    printf("Error: %s\n", error->message);
    g_error_free(error);

    g_main_loop_quit(loop);

    break;
  }
  default:
    break;
  }

  return True;
}

static GstFlowReturn NewSample(GstElement *sink, gpointer user_data) {
    pthread_mutex_lock(&sample_mutex);
    new_sample = TRUE;
    pthread_cond_signal(&sample_cond);
    pthread_mutex_unlock(&sample_mutex);

    return GST_FLOW_OK;
}

// Creates and sets up Gstreamer pipeline for JPEG encoding.
void *GstreamerThread(void *pThreadParam) {
  
  GstBus *bus;
  

  //char *strPipeline = new char[8192];

  pipeline = gst_pipeline_new("mypipeline");
  appsrc = gst_element_factory_make("appsrc", "mysource");
  appsink = gst_element_factory_make("appsink", "mysink");

  vidconv = gst_element_factory_make("videoconvert", "myvideoconvert");
  x264enc = gst_element_factory_make("x264enc", "myx264enc");

  qtmux = gst_element_factory_make("mp4mux", "myqtmux");

  // Check if all elements were created
  if (!pipeline || !appsrc || !x264enc || !vidconv || !appsink) {
    fprintf(stderr, "Could not gst_element_factory_make, terminating\n");
    bGstreamerThreadStarted = bGstreamerThreadFinished = True;
    return (void *)0xDEAD;
  }

  // appsrc should be linked to jpegenc with these caps otherwise jpegenc does
  // not know size of incoming buffer
  GstCaps *cap_appsrc_to_x264enc;
  cap_appsrc_to_x264enc =
      gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGRx",
                          "width", G_TYPE_INT, width, "height", G_TYPE_INT,
                          height, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

  GstCaps *cap_x264enc_to_sink;
  cap_x264enc_to_sink = gst_caps_new_simple(
      "video/x-h264", "stream-format", G_TYPE_STRING, "byte-stream",
      "alignment", G_TYPE_STRING, "au", NULL);

  // blocksize is important for jpegenc to know how many data to expect from
  // appsrc in a single frame, too
  char szTemp[64];
  sprintf(szTemp, "%d", width * height);
  g_object_set(G_OBJECT(appsrc), "blocksize", szTemp, NULL);
  g_object_set(G_OBJECT(x264enc), "sync-lookahead", 2, NULL);
  g_object_set(G_OBJECT(x264enc), "rc-lookahead", 2, NULL);
  g_object_set(G_OBJECT(x264enc), "bframes", 0, NULL);
  //g_object_set(G_OBJECT(x264enc), "speed-preset", "ultrafast", NULL);
  g_object_set(G_OBJECT(x264enc), "key-int-max", 10, NULL);
  g_object_set(G_OBJECT(x264enc), "bitrate", 3000, NULL);
  

  g_object_set(G_OBJECT(appsrc), "stream-type", 0, "format", GST_FORMAT_TIME,
               NULL);
  g_object_set(G_OBJECT(appsrc), "caps",
               gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING,
                                   "BGRx", "width", G_TYPE_INT, width,
                                   "height", G_TYPE_INT, height, "framerate",
                                   GST_TYPE_FRACTION, 30, 1, NULL),
               NULL);

  // Create gstreamer loop
  loop = g_main_loop_new(NULL, False);

  // add a message handler
  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
  gst_object_unref(bus);

  gst_bin_add_many(GST_BIN(pipeline), appsrc, vidconv, x264enc, appsink, NULL);

  gst_element_link_filtered(appsrc, vidconv, cap_appsrc_to_x264enc);
  gst_element_link(vidconv, x264enc);
  gst_element_link_filtered(x264enc, appsink, cap_x264enc_to_sink);

  //new~~~
  g_object_set(appsink, "emit-signals", TRUE, NULL);
  g_signal_connect(appsink, "new-sample", G_CALLBACK(NewSample), NULL);

  printf("Setting g_main_loop_run to GST_STATE_PLAYING\n");
  // Start pipeline so it could process incoming data
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  // Indicate that thread was started
  bGstreamerThreadStarted = True;

  // Loop will run until receiving EOS (end-of-stream), will block here
  g_main_loop_run(loop);

  printf("g_main_loop_run returned, stopping playback\n");

  // Stop pipeline to be released
  gst_element_set_state(pipeline, GST_STATE_NULL);

  printf("Deleting pipeline\n");
  // THis will also delete all pipeline elements
  gst_object_unref(GST_OBJECT(pipeline));

  g_source_remove(bus_watch_id);
  g_main_loop_unref(loop);

  // Indicate that thread was finished
  bGstreamerThreadFinished = True;

  printf("gst thread terminating~\n");
  pthread_exit(0);

  return NULL;
}

// Starts GstreamerThread that remains in memory and compresses frames as being
// fed by user app.
Bool StartGstreamer() {
  // GstreamerThread(NULL);
  // return True;
  unsigned long GtkThreadId;
  pthread_attr_t GtkAttr;
  int result;

  // Start thread
  // int result = pthread_attr_init(&GtkAttr);
  // if (result != 0) {
  //   fprintf(stderr, "pthread_attr_init returned error %d\n", result);
  //   return False;
  // }

  void *pParam = NULL;
  //result = pthread_create(&GtkThreadId, &GtkAttr, GstreamerThread, pParam);
  result = pthread_create(&GtkThreadId,NULL,GstreamerThread,(void *)GtkThreadId);
  if (result != 0) {
    printf("pthread_create returned error %d\n", result);
    return False;
  }

  unsigned long SampleThreadId;
  pthread_mutex_init(&sample_mutex, NULL);
  pthread_cond_init(&sample_cond, NULL);
  pthread_create(&SampleThreadId,NULL,PullBuffer,(void *)SampleThreadId);

  return True;
}

int main(int argc, char *argv[]) {

  printf("Testing Gstreamer-1.0 Jpeg encoder.\n");

  /* Check input arguments */
  if (argc < 2 || argc > 5) {
    fprintf(stderr, "Usage: %s rawfile width height outfile [quality]\n"
                    "Rawfile must be one raw frame of GRAY8, outfile will be "
                    "JPG encoded. 10 outfiles will be created\n",
            argv[0]);
    return -1;
  }
  /* Initialization */
  gst_init(NULL, NULL); // Will abort if GStreamer init error found

  // Read command line arguments
  width = atoi(argv[1]);
  height = atoi(argv[2]);
  //outfile = argv[3];

  // Validate command line arguments
  if (width < 100 || width > 4096 || height < 100 || height > 4096) {
    printf("width and/or height or quality is bad, not running conversion\n");
    return -1;
  }

  of = fopen("/root/Desktop/h264_testout.mp4", "wb");

  rawdata = malloc(width * height *4);
  //memset(rawdata,0,width*height*3);

  // Init raw frame
  srand(time(NULL));
  for (int i = 0; i < width * height; ++i) {
    rawdata[i*3] = rand() % 255;     //R
    rawdata[i*3+1] = rand() % 255;   //G
    rawdata[i*3+2] = rand() % 255;   //B
    rawdata[i*4+3] = rand() % 255;   //x
  }

  // Start conversion thread
  StartGstreamer();

  // Ensure thread is running (or ran and stopped)
  while (bGstreamerThreadStarted == False)
    usleep(10000); // Yield to allow thread to start
  if (bGstreamerThreadFinished == True) {
    printf( "Gstreamer thread could not start, terminating\n");
    return -1;
  }

  int ticks = MyGetTickCount();

  //FILE *of = fopen("/root/Desktop/testout.mp4", "wb");
  // Compress raw frame 10 times, adding horizontal stripes to ensure resulting
  // images are different
  for (int i = 0; i < 150; i++) {
    // write stripes into image to see they are really different
    // memset(rawdata + (i * 2) * width * 3, 0xff, width*3);
    // memset(rawdata + ((i + 3) * 2) * width * 3, 0x00, width*3);

    // Push raw buffer into gstreamer
    //memset(rawdata,i,width*height*3);
    PushBuffer();
    usleep(1000000/30);
  }

  // Get total conversion time
  int ms = MyGetTickCount() - ticks;


  // Wait until GstreamerThread stops
  for (int i = 0; i < 100000; i++) {
    if (bGstreamerThreadFinished == True)
      break;
    usleep(100000); // Yield to allow thread to start
    if (i == 999)
      printf( "GStreamer thread did not finish\n");
  }

  fclose(of);
  //destroy pthread things
  pthread_mutex_destroy(&sample_mutex);
  pthread_cond_destroy(&sample_cond);

  return 0;
}