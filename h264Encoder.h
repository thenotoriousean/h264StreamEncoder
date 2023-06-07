#ifndef X2GOKDRIVE_SCRIPT_H
#define X2GOKDRIVE_SCRIPT_H

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>


unsigned int MyGetTickCount();
void *PullBuffer();
static GstFlowReturn NewSample(GstElement *sink, gpointer user_data);


#endif /* X2GOKDRIVE_SCRIPT_H */