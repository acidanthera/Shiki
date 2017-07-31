//
//  kern_start.cpp
//  Shiki
//
//  Copyright © 2016-2017 vit9696. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "kern_resources.hpp"

static void disableSection(uint32_t section) {
	for (size_t i = 0; i < ADDPR(procInfoSize); i++) {
		if (ADDPR(procInfo)[i].section == section) {
			ADDPR(procInfo)[i].section = SectionUnused;
		}
	}
	
	for (size_t i = 0; i < ADDPR(binaryModSize); i++) {
		auto patches = ADDPR(binaryMod)[i].patches;
		for (size_t j = 0; j < ADDPR(binaryMod)[i].count; j++) {
			if (patches[j].section == section) {
				patches[j].section = SectionUnused;
			}
		}
	}
}

static void shikiStart() {
	// Attempt to support fps.1_0 in Safari
	char tmp[16];
	bool patchStreamVideo = PE_parse_boot_argn("-shikifps", tmp, sizeof(tmp));
	bool leaveForceAccelRenderer = true;
	bool leaveBGRASupport = true;
	
	if (PE_parse_boot_argn("shikigva", tmp, sizeof(tmp))) {
		leaveForceAccelRenderer = !(tmp[0] & 1);
		leaveBGRASupport = !(tmp[0] & 2);
	}
	
	if (PE_parse_boot_argn("-shikigva", tmp, sizeof(tmp))) {
		SYSLOG("shiki @ -shikigva is deprecated use shikgva=1 instead");
		leaveForceAccelRenderer = false;
	}
	
	
	// Disable unused SectionFSTREAM
	if (!patchStreamVideo)
		disableSection(SectionNSTREAM);
	
	if (leaveForceAccelRenderer)
		disableSection(SectionOFFLINE);
	
	if (leaveBGRASupport)
		disableSection(SectionBGRA);
	
	lilu.onProcLoad(ADDPR(procInfo), ADDPR(procInfoSize), nullptr, nullptr, ADDPR(binaryMod), ADDPR(binaryModSize));
}

static const char *bootargOff[] {
	"-shikioff",
	// Additionally disable during recovery/installation
	"rp0",
	"rp",
	"container-dmg",
	"root-dmg" // should we check for com.apple.recovery.boot or InstallESD.dmg?
};

static const char *bootargDebug[] {
	"-shikidbg"
};

static const char *bootargBeta[] {
	"-shikibeta"
};

PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
	parseModuleVersion(xStringify(MODULE_VERSION)),
	bootargOff,
	arrsize(bootargOff),
	bootargDebug,
	arrsize(bootargDebug),
	bootargBeta,
	arrsize(bootargBeta),
	KernelVersion::Mavericks,
	KernelVersion::HighSierra,
	shikiStart
};
