#include "shared.h"


VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin* plugin, const VSPLUGINAPI* vspapi) {
	vspapi->configPlugin("com.julek.ehist", "ehist", "Histogram Equalization and CLAHE", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION, 0, plugin);
	vspapi->registerFunction("EqualizeHist", "clip:vnode;", "clip:vnode;", equalizeHistCreate, nullptr, plugin);
	vspapi->registerFunction("CLAHE", "clip:vnode;" "limit:float:opt;" "tile:int:opt;", "clip:vnode;", claheCreate, nullptr, plugin);
}