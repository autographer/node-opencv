
#include "VideoProc.h"
#include "Matrix.h"
#include <opencv2/gpu/gpu.hpp>

void VideoProc::Init(Handle<Object> target) {
  Persistent<Object> inner;
  Local<Object> obj = NanNew<Object>();
  NanAssignPersistent(inner, obj);

  NODE_SET_METHOD(obj, "resizeVideo", ResizeVideo);

  target->Set(NanNew("videoproc"), obj);
}

NAN_METHOD(VideoProc::ResizeVideo) {
  NanEscapableScope();

  try {
    NanAsciiString in_filename(args[0]);
    NanAsciiString out_filename(args[1]);

    int width = args[2]->IntegerValue();
    int height = args[3]->IntegerValue();
    int method = args[4]->IntegerValue();

    cv::VideoCapture in_vid(*in_filename);

    if(in_vid.isOpened()) {

      int fps = in_vid.get(CV_CAP_PROP_FPS);
      std::string filename = *out_filename;
      //filename += ".mjpeg";
      cv::VideoWriter out_vid(filename, CV_FOURCC('M','J','P','G'), fps, cv::Size(width,height));

      if(out_vid.isOpened()) {
        int num_frames = in_vid.get(CV_CAP_PROP_FRAME_COUNT);

        //decode the frame one by one and write to the new video
        for(int i=0; i < num_frames; i++)
        {
          cv::Mat img;
          in_vid.read(img);

          if(method == 0 || cv::gpu::getCudaEnabledDeviceCount() == 0) {        
            cv::Mat dst;
            cv::resize(img, dst, cv::Size(width, height), 0, 0, cv::INTER_CUBIC);
            out_vid.write(dst);

          } else if(method == 1) {
            if(cv::gpu::CudaMem::canMapHostMemory()) {
              cv::gpu::GpuMat dst;
              cv::gpu::CudaMem src(img, cv::gpu::CudaMem::ALLOC_ZEROCOPY);
              cv::gpu::resize(src, dst, cv::Size(width, height), 0, 0, cv::INTER_CUBIC);
              cv::Mat dst_host(dst);
              out_vid.write(dst_host);

            } else {
              cv::gpu::GpuMat dst, src;
              src.upload(img);
              cv::gpu::resize(src, dst, cv::Size(width, height), 0, 0, cv::INTER_CUBIC);
              cv::Mat dst_host(dst);
              out_vid.write(dst_host);
            }
          } else {
            throw("unknown resize method");
          }
        }
      } else {
        throw('input video could not opened');
      }
    }

  } catch(cv::Exception &e) {
    const char *err_msg = e.what();
    NanThrowError(err_msg);
    NanReturnUndefined();
  }
}