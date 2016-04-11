
#include "OpenCV.h"
#include <nan.h>
#include <vector>
#include "exifparser.h"
#include "Utils.h"

void Utils::Init(Handle<Object> target) {
  Nan::Persistent<Object> inner;
  Local<Object> obj = Nan::New<Object>();
  inner.Reset( obj);

  Nan::SetMethod(obj, "copyExif", CopyExif);

  target->Set(Nan::New("utils").ToLocalChecked(), obj);
}

NAN_METHOD(Utils::CopyExif) {
  SETUP_FUNCTION(Utils)

  if (!info[0]->IsString()) {
    Nan::ThrowTypeError("filename required");
  }

  if (!info[1]->IsString()) {
    Nan::ThrowTypeError("exif filename required");
  }

  Nan::Utf8String in_filename(info[0]);
  Nan::Utf8String out_filename(info[1]);

  exif::ExifParser::CopyExifData(in_filename, out_filename);
}