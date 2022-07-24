#include "shared.h"

struct EqualizeHistData final {
	VSNode* node;
	const VSVideoInfo* vi;
};

template<typename pixel_t>
static void process_c(const VSFrame* src, VSFrame* dst, const EqualizeHistData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
	for (int plane = 0; plane < d->vi->format.numPlanes; plane++) {
		const auto w{ vsapi->getFrameWidth(src, plane) };
		const auto h{ vsapi->getFrameHeight(src, plane) };
		const auto stride{ vsapi->getStride(src, plane) / d->vi->format.bytesPerSample };
		auto srcp{ reinterpret_cast<const pixel_t*>(vsapi->getReadPtr(src, plane)) };
		auto dstp{ reinterpret_cast<pixel_t*>(vsapi->getWritePtr(dst, plane)) };

		const int buffersize = (1 << d->vi->format.bitsPerSample);
		uint32_t* hist = new uint32_t[buffersize];
		uint32_t* lut = new uint32_t[buffersize];
		const int peak = buffersize - 1;
		const float scale = static_cast<float>(peak) / (w * h);
		int sum = 0;

		std::fill_n(hist, buffersize, 0);
		std::fill_n(lut, buffersize, 0);

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				hist[srcp[x]]++;
			}
			srcp += stride;
		}

		srcp -= (stride * h);

		for (int i = 0; i < buffersize; i++) {
			sum += hist[i];
			auto val = roundf(sum * scale);
			lut[i] = static_cast<int>(val);
		}

		lut[0] = 0;

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++)
				dstp[x] = lut[srcp[x]];

			dstp += stride;
			srcp += stride;
		}

		delete[] hist;
		delete[] lut;
	}
}

static const VSFrame* VS_CC equalizeHistGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
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

		if (d->vi->format.bytesPerSample == 1) {
			process_c<uint8_t>(src, dst, d, vsapi);
		}
		else {
			process_c<uint16_t>(src, dst, d, vsapi);
		}

		vsapi->freeFrame(src);
		VSMap* dstProps = vsapi->getFramePropertiesRW(dst);
		vsapi->mapSetInt(dstProps, "_ColorRange", 0, maReplace);
		return dst;
	}
	return nullptr;
}

static void VS_CC equalizeHistFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
	auto d{ static_cast<EqualizeHistData*>(instanceData) };
	vsapi->freeNode(d->node);
	delete d;
}

void VS_CC equalizeHistCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
	auto d{ std::make_unique<EqualizeHistData>() };
	int err{ 0 };

	d->node = vsapi->mapGetNode(in, "clip", 0, nullptr);
	d->vi = vsapi->getVideoInfo(d->node);

	if (!vsh::isConstantVideoFormat(d->vi) ||
		d->vi->format.sampleType != stInteger ||
		(d->vi->format.bytesPerSample != 1 && d->vi->format.bytesPerSample != 2)) {
		vsapi->mapSetError(out, "EqualizeHist: only constant 8-16 bit int formats supported");
		vsapi->freeNode(d->node);
		return;
	}

	VSFilterDependency deps[] = { {d->node, rpStrictSpatial} };
	vsapi->createVideoFilter(out, "EqualizeHist", d->vi, equalizeHistGetFrame, equalizeHistFree, fmParallel, deps, 1, d.get(), core);
	d.release();
}