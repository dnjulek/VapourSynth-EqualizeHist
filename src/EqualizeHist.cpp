#include <memory>

#include "VapourSynth4.h"
#include "VSHelper4.h"

struct EqualizeHistData final {
	VSNode* node;
	const VSVideoInfo* vi;
};

static void process_c(const uint8_t* srcp, uint8_t* dstp, ptrdiff_t stride, int width, int height, const EqualizeHistData* const VS_RESTRICT d) noexcept {
	int total = (width * height);
	float scale = 255.0f / total;
	int hist[256]{};
	int lut[257]{};
	int sum = 0;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++)
			hist[srcp[x]]++;

		srcp += stride;
	}

	srcp -= total;

	for (int i = 0; i < 256; i++) {
		sum += hist[i];
		auto val = roundf(sum * scale);
		lut[i] = static_cast<int>(val);
	}

	lut[0] = 0;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++)
			dstp[x] = lut[srcp[x]];

		dstp += stride;
		srcp += stride;
	}
}

static const VSFrame* VS_CC EqualizeHistGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
	auto d{ static_cast<EqualizeHistData*>(instanceData) };

	if (activationReason == arInitial) {
		vsapi->requestFrameFilter(n, d->node, frameCtx);
	}
	else if (activationReason == arAllFramesReady) {
		const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);

		const VSVideoFormat* fi = vsapi->getVideoFrameFormat(src);
		int height = vsapi->getFrameHeight(src, 0);
		int width = vsapi->getFrameWidth(src, 0);

		VSFrame* dst = vsapi->newVideoFrame(fi, width, height, src, core);

		for (int plane = 0; plane < fi->numPlanes; plane++) {
			const uint8_t* srcp = vsapi->getReadPtr(src, plane);
			uint8_t* dstp = vsapi->getWritePtr(dst, plane);
			ptrdiff_t stride = vsapi->getStride(src, plane);
			int h = vsapi->getFrameHeight(src, plane);
			int w = vsapi->getFrameWidth(src, plane);
			process_c(srcp, dstp, stride, w, h, d);
		}

		vsapi->freeFrame(src);
		VSMap* dstProps = vsapi->getFramePropertiesRW(dst);
		vsapi->mapSetInt(dstProps, "_ColorRange", 0, maReplace);
		return dst;
	}
	return nullptr;
}

static void VS_CC EqualizeHistFree(void* instanceData, [[maybe_unused]] VSCore* core, const VSAPI* vsapi) {
	auto d{ static_cast<EqualizeHistData*>(instanceData) };
	vsapi->freeNode(d->node);
	delete d;
}

static void VS_CC EqualizeHistCreate(const VSMap* in, VSMap* out, [[maybe_unused]] void* userData, VSCore* core, const VSAPI* vsapi) {
	auto d{ std::make_unique<EqualizeHistData>() };
	int err{ 0 };

	d->node = vsapi->mapGetNode(in, "clip", 0, nullptr);
	d->vi = vsapi->getVideoInfo(d->node);

	if (!vsh::isConstantVideoFormat(d->vi) ||
		d->vi->format.sampleType != stInteger ||
		d->vi->format.bitsPerSample != 8) {
		vsapi->mapSetError(out, "EqualizeHist: only constant format 8-bit integer input supported");
		vsapi->freeNode(d->node);
		return;
	}

	VSFilterDependency deps[] = { {d->node, rpStrictSpatial} };
	vsapi->createVideoFilter(out, "EqualizeHist", d->vi, EqualizeHistGetFrame, EqualizeHistFree, fmParallel, deps, 1, d.get(), core);
	d.release();
}

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin* plugin, const VSPLUGINAPI* vspapi) {
	vspapi->configPlugin("com.julek.ehist", "ehist", "Histogram Equalization", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION, 0, plugin);
	vspapi->registerFunction("EqualizeHist",
		"clip:vnode;",
		"clip:vnode;",
		EqualizeHistCreate, nullptr, plugin);
}