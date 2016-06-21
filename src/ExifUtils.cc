
#include "OpenCV.h"
#include <nan.h>
#include <vector>
#include "exifparser.h"
#include "ExifUtils.h"

void ExifUtils::Init(Handle<Object> target) {
  Nan::Persistent<Object> inner;
  Local<Object> obj = Nan::New<Object>();
  inner.Reset( obj);

  Nan::SetMethod(obj, "copyExif", CopyExif);
  Nan::SetMethod(obj, "readRotation", ReadRotation);

  target->Set(Nan::New("exifutils").ToLocalChecked(), obj);
}

NAN_METHOD(ExifUtils::CopyExif) {
  Nan::HandleScope scope;

  if (!info[0]->IsString()) {
    Nan::ThrowTypeError("filename required");
  }

  if (!info[1]->IsString()) {
    Nan::ThrowTypeError("exif filename required");
  }

  Nan::Utf8String in_filename(info[0]);
  Nan::Utf8String out_filename(info[1]);

  exif::ExifParser parser;
  parser.CopyExifData(*in_filename, *out_filename);
}

NAN_METHOD(ExifUtils::ReadRotation) {
  Nan::EscapableHandleScope scope;

  if (!info[0]->IsString()) {
    Nan::ThrowTypeError("ReadRotation needs a filepath");
  }

  Nan::Utf8String in_filename(info[0]);

  try {
    exif::ExifParser parser;
    
    if(parser.ParseExifData(*in_filename)) {
      const exif::ExifInfo& imgexif = GetExifInfo();
      info.GetReturnValue().Set(Nan::New<Number>((int)imgexif.Orientation));
    } else {
      info.GetReturnValue().Set(Nan::New<Number>((int)exif::ExifParser::UNKNOWN_ORIENTATION));
    }

  } catch(cv::Exception &e) {
    const char *err_msg = e.what();
    Nan::ThrowError(err_msg);
    return;
  }
}