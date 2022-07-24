#pragma once

#include <memory>
#include <opencv2/imgproc.hpp>
#include "VapourSynth4.h"
#include "VSHelper4.h"


extern void VS_CC claheCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);
extern void VS_CC equalizeHistCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi);