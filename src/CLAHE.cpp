#include "shared.h"

struct CLAHEData final {
	VSNode* node;
	const VSVideoInfo* vi;
	float limit;
	int tile;
};

template<typename pixel_t, int cv_t>
static void process_c(const VSFrame* src, VSFrame* dst, const CLAHEData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
	for (int plane = 0; plane < d->vi->format.numPlanes; plane++) {
		const auto w{ vsapi->getFrameWidth(src, plane) };
		const auto h{ vsapi->getFrameHeight(src, plane) };
		const auto stride{ vsapi->getStride(src, plane) / d->vi->format.bytesPerSample };
		auto srcp{ reinterpret_cast<const pixel_t*>(vsapi->getReadPtr(src, plane)) };
		auto dstp{ reinterpret_cast<pixel_t*>(vsapi->getWritePtr(dst, plane)) };

		cv::Mat srcImg(cv::Size(w, h), cv_t);
		cv::Mat dstImg(cv::Size(w, h), cv_t);

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				srcImg.at<pixel_t>(y, x) = srcp[x];
			}
			srcp += stride;
		}

		cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(d->limit, cv::Size(d->tile, d->tile));
		clahe->apply(srcImg, dstImg);

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				dstp[x] = dstImg.at<pixel_t>(y, x);
			}
			dstp += stride;
		}
	}
}

static const VSFrame* VS_CC claheGetFrame(int n, int activationReason, void* instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
	auto d{ static_cast<CLAHEData*>(instanceData) };

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
			process_c<uint8_t, CV_8UC1>(src, dst, d, vsapi);
		}
		else {
			process_c<uint16_t, CV_16UC1>(src, dst, d, vsapi);
		}

		vsapi->freeFrame(src);
		VSMap* dstProps = vsapi->getFramePropertiesRW(dst);
		vsapi->mapSetInt(dstProps, "_ColorRange", 0, maReplace);
		return dst;
	}
	return nullptr;
}

static void VS_CC claheFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
	auto d{ static_cast<CLAHEData*>(instanceData) };
	vsapi->freeNode(d->node);
	delete d;
}

void VS_CC claheCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
	auto d{ std::make_unique<CLAHEData>() };
	int err{ 0 };

	d->node = vsapi->mapGetNode(in, "clip", 0, nullptr);
	d->vi = vsapi->getVideoInfo(d->node);

	d->limit = vsapi->mapGetFloatSaturated(in, "limit", 0, &err);
	if (err)
		d->limit = 7.0f;

	d->tile = vsapi->mapGetIntSaturated(in, "tile", 0, &err);
	if (err)
		d->tile = 3;

	if (!vsh::isConstantVideoFormat(d->vi) ||
		d->vi->format.sampleType != stInteger ||
		d->vi->format.bytesPerSample != 1 && d->vi->format.bytesPerSample != 2) {
		vsapi->mapSetError(out, "CLAHE: only constant 8-16 bit int formats supported");
		vsapi->freeNode(d->node);
		return;
	}

	VSFilterDependency deps[] = { {d->node, rpStrictSpatial} };
	vsapi->createVideoFilter(out, "CLAHE", d->vi, claheGetFrame, claheFree, fmParallel, deps, 1, d.get(), core);
	d.release();
}