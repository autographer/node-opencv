#ifndef __NODE_VIDEOPROC_H
#define __NODE_VIDEOPROC_H

#include "OpenCV.h"

/**
 * Implementation of imgproc.hpp functions
 */
class VideoProc: public node::ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static NAN_METHOD(ResizeVideo);
};

#endif