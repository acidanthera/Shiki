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
	
	// Disable unused SectionFSTREAM
	if (!patchStreamVideo)
		disableSection(SectionNSTREAM);
	
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
	bootargOff,
	sizeof(bootargOff)/sizeof(bootargOff[0]),
	bootargDebug,
	sizeof(bootargDebug)/sizeof(bootargDebug[0]),
	bootargBeta,
	sizeof(bootargBeta)/sizeof(bootargBeta[0]),
	KernelVersion::Mavericks,
	KernelVersion::Sierra,
	shikiStart
};
