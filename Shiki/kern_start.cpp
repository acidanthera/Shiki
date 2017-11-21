//
//  kern_start.cpp
//  Shiki
//
//  Copyright © 2016-2017 vit9696. All rights reserved.
//

#include <Library/LegacyIOService.h>
#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_file.hpp>
#include <Headers/kern_iokit.hpp>
#include <IOKit/IODeviceTreeSupport.h>

#include "kern_resources.hpp"

enum ShikiGVAPatches {
	ForceOnlineRenderer = 1,
	AllowNonBGRA = 2,
	ForceCompatibleRenderer = 4,
	VDAExecutableWhitelist = 8,
	DisableHardwareKeyExchange = 16,
	ReplaceBoardID = 32
};

// 32 bytes should be reasonable for a safe comparison
static constexpr size_t NVPatchSize {32};
static uint8_t nvPatchFind[NVPatchSize] {};
static uint8_t nvPatchReplace[NVPatchSize] {};
static UserPatcher::BinaryModPatch *nvPatch {nullptr};

static bool autodetectIGPU {false};
static char customBoardID[64] {};

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

static uint32_t getModernFrameId() {
	uint32_t platform = 0;
	const char *tree[] {"AppleACPIPCI", "IGPU"};
	auto sect = WIOKit::findEntryByPrefix("/AppleACPIPlatformExpert", "PCI", gIOServicePlane);
	for (size_t i = 0; sect && i < arrsize(tree); i++) {
		sect = WIOKit::findEntryByPrefix(sect, tree[i], gIOServicePlane);
		if (sect && i+1 == arrsize(tree)) {
			if (WIOKit::getOSDataValue(sect, "AAPL,ig-platform-id", platform)) {
				DBGLOG("audio", "found IGPU with ig-platform-id %08x", platform);
				return platform;
			} else {
				SYSLOG("audio", "found IGPU with missing ig-platform-id, assuming old");
			}
		}
	}

	DBGLOG("audio", "failed to find IGPU ig-platform-id");
	return platform;
}

static void shikiPatcherLoad(void *, KernelPatcher &) {
	// We need to do magic here and prepare a patch for AppleGVA that would force
	// the assumption that NVIDIA is compatible with SKL/KBL processors.

	if (nvPatch) {
		size_t gvaSize {0};
		auto gvaBuf = FileIO::readFileToBuffer("/System/Library/PrivateFrameworks/AppleGVA.framework/Versions/A/AppleGVA", gvaSize);
		if (gvaBuf && gvaSize > PAGE_SIZE) {
			// We perform a two step search, firstly we find the relevant starting point, then we find the patch place
			size_t currOff {0}, endOff {0};

			// The real patch place should be very close
			static constexpr size_t MaxPatchOff {128};

			// 10.12 and newer support many Intel GPUs, which makes the check place very traceable
			if (getKernelVersion() > KernelVersion::ElCapitan) {
				// We need to find the initial check, which we expect to be or with stack var, because of the following:
				// - there are several GPUs that we need to test against (2xSKL, 2xKBL, thus not enough registers)
				// - the check is built as one of Intel and AMD (thus a number of ors in a row)
				static constexpr uint8_t OrOpcode[] {0xFF, 0xFF, 0x48, 0x0B /* XX, XX, XX, XX, XX */};

				endOff = gvaSize - sizeof(OrOpcode) + 5;
				while (currOff != endOff && memcmp(&gvaBuf[currOff], OrOpcode, sizeof(OrOpcode)))
					currOff++;

				if (currOff != endOff && endOff - currOff > MaxPatchOff)
					DBGLOG("shiki", "found pre or instructions at %lu (%02X %02X %02X %02X %02X %02X %02X)", currOff, gvaBuf[currOff+2],
						   gvaBuf[currOff+3], gvaBuf[currOff+4], gvaBuf[currOff+5], gvaBuf[currOff+6], gvaBuf[currOff+7], gvaBuf[currOff+8]);

				else
					currOff = endOff = 0;
			} else {
				// 10.11 is much more problematic, but the good thing is it no longer updates, we use an idea of
				// - few Intel GPUs
				// - test and jmp taking 3 and 2 bytes correspondingly
				static constexpr uint8_t TestOpcode[] {0x74, 0x05, 0x4D, 0x85 /* XX */};

				endOff = gvaSize - sizeof(TestOpcode) + 1;
				while (currOff != endOff && memcmp(&gvaBuf[currOff], TestOpcode, sizeof(TestOpcode)))
					currOff++;

				if (currOff != endOff && endOff - currOff > MaxPatchOff)
					DBGLOG("shiki", "found pre test instructions at %lu (%02X %02X %02X %02X %02X)", currOff, gvaBuf[currOff],
						   gvaBuf[currOff+1], gvaBuf[currOff+2], gvaBuf[currOff+3], gvaBuf[currOff+4]);

				else
					currOff = endOff = 0;
			}

			if (currOff && endOff) {
				// Now we need to get to 2 jmps, which we expect to be 0F 84 xx xx xx xx, because of the following:
				// - acceleration availability takes the main branch, and its code is large (thus 4 byte relative jmp)
				// - we have non-zero comparators (thus je will be used)
				static constexpr size_t JmpOpcodeSize {6};
				static constexpr size_t JmpOpcodeNum {2};
				static constexpr uint8_t JmpOpcode[] {0x0F, 0x84 /* XX, XX, XX, XX */};

				size_t jmpOffsets[JmpOpcodeNum] {};
				bool jmpOffsetsOk {true};

				endOff = currOff + MaxPatchOff - NVPatchSize;

				for (size_t i = 0; i < arrsize(jmpOffsets); i++) {
					while (currOff != endOff && memcmp(&gvaBuf[currOff], JmpOpcode, sizeof(JmpOpcode)))
						currOff++;

					if (currOff != endOff) {
						DBGLOG("shiki", "found %lu jmp instruction at %lu (%02X %02X %02X %02X %02X %02X)", i, currOff, gvaBuf[currOff],
							   gvaBuf[currOff+1], gvaBuf[currOff+2], gvaBuf[currOff+3], gvaBuf[currOff+4], gvaBuf[currOff+5]);

						if (i == 0 || (jmpOffsets[0] + (NVPatchSize - JmpOpcodeSize) >= jmpOffsets[i])) {
							DBGLOG("shiki", "offset %lu is within the bounds", i);
							jmpOffsets[i] = currOff;
						} else {
							jmpOffsetsOk = false;
						}
						currOff++;
					} else {
						jmpOffsetsOk = false;
					}
				}

				if (jmpOffsetsOk && currOff != endOff) {
					DBGLOG("shiki", "preparing find & replace nvidia buffers");
					lilu_os_memcpy(nvPatchFind, &gvaBuf[jmpOffsets[0]], NVPatchSize);
					lilu_os_memcpy(nvPatchReplace, &gvaBuf[jmpOffsets[0]], NVPatchSize);

					static constexpr uint8_t NopOpcode {0x90};
					for (size_t i = 0; i < arrsize(jmpOffsets); i++)
						memset(&nvPatchReplace[jmpOffsets[i] - jmpOffsets[0]], NopOpcode, JmpOpcodeSize);

					DBGLOG("shiki", "find %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", nvPatchFind[0], nvPatchFind[1], nvPatchFind[2],
						   nvPatchFind[3], nvPatchFind[4], nvPatchFind[5], nvPatchFind[6], nvPatchFind[7], nvPatchFind[8], nvPatchFind[9]);

					DBGLOG("shiki", "repl %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", nvPatchReplace[0], nvPatchReplace[1], nvPatchReplace[2],
						   nvPatchReplace[3], nvPatchReplace[4], nvPatchReplace[5], nvPatchReplace[6], nvPatchReplace[7], nvPatchReplace[8], nvPatchReplace[9]);

					// Ok, everything looks sane enough
					nvPatch->section = SectionNVDA;
				}
			} else {
				SYSLOG("shiki", "unable to find or instructions");
			}
		} else {
			SYSLOG("shiki", "failed to read AppleGVA framework %lu", gvaSize);
		}

		if (gvaBuf) Buffer::deleter(gvaBuf);
	}

	if (autodetectIGPU) {
		auto frame = getModernFrameId();
		DBGLOG("shiki", "detected igpu frame 0x%08x", frame);
		// Older IGPUs (and invalid frames) get no default hacks
		if (!frame)
			disableSection(SectionKEGVA);
	}

	if (customBoardID[0]) {
		auto entry = IORegistryEntry::fromPath("/", gIODTPlane);
		if (entry) {
			DBGLOG("shiki", "changing shiki-id to %s", customBoardID);
			entry->setProperty("shiki-id", OSData::withBytes(customBoardID, static_cast<uint32_t>(strlen(customBoardID)+1)));
			entry->release();
		} else {
			SYSLOG("shiki", "failed to obtain iodt tree");
		}
	}
}

static void shikiStart() {
	// Attempt to support fps.1_0 in Safari
	int bootarg {0};
	bool patchStreamVideo = PE_parse_boot_argn("-shikifps", &bootarg, sizeof(bootarg));
	bool forceAccelRenderer = false;
	bool allowNonBGRA = false;
	bool forceNvidiaUnlock = false;
	bool addExeWhiteList = false;
	bool killKeyExchange = false;
	bool replaceBoardID = false;
	
	if (PE_parse_boot_argn("shikigva", &bootarg, sizeof(bootarg))) {
		forceAccelRenderer = bootarg & ForceOnlineRenderer;
		allowNonBGRA       = bootarg & AllowNonBGRA;
		forceNvidiaUnlock  = bootarg & ForceCompatibleRenderer;
		addExeWhiteList    = bootarg & VDAExecutableWhitelist;
		killKeyExchange    = bootarg & DisableHardwareKeyExchange;
		replaceBoardID     = bootarg & ReplaceBoardID;
	} else {
		// By default enable iTunes hack for 10.13 and higher for Ivy+ IGPU
		killKeyExchange = autodetectIGPU = getKernelVersion() >= KernelVersion::HighSierra;

		if (PE_parse_boot_argn("-shikigva", &bootarg, sizeof(bootarg))) {
			SYSLOG("shiki", "-shikigva is deprecated use shikigva=%d instead", ForceOnlineRenderer);
			forceAccelRenderer = true;
		}
	}

	DBGLOG("shiki", "stream %d accel %d bgra %d nvidia %d/%d ke1 %d id %d", patchStreamVideo, forceAccelRenderer,
		   allowNonBGRA, forceNvidiaUnlock, addExeWhiteList, killKeyExchange, replaceBoardID);

	// Disable unused SectionFSTREAM
	if (!patchStreamVideo)
		disableSection(SectionNSTREAM);
	
	if (!forceAccelRenderer)
		disableSection(SectionOFFLINE);
	
	if (!allowNonBGRA)
		disableSection(SectionBGRA);

	if (!replaceBoardID) {
		disableSection(SectionBOARDID);
	} else {
		if (!PE_parse_boot_argn("shiki-id", customBoardID, sizeof(customBoardID)))
			snprintf(customBoardID, sizeof(customBoardID), "Mac-27ADBB7B4CEE8E61"); // iMac14,2
		DBGLOG("shiki", "Requesting %s board-id for gva", customBoardID);
	}

	// Do not enable until we are certain it works
	bool requireNvidiaPatch = forceNvidiaUnlock && getKernelVersion() >= KernelVersion::ElCapitan;
	if (requireNvidiaPatch) {
		for (size_t i = 0; i < ADDPR(binaryModSize); i++) {
			auto patches = ADDPR(binaryMod)[i].patches;
			for (size_t j = 0; j < ADDPR(binaryMod)[i].count; j++) {
				if (patches[j].section == SectionNVDA) {
					nvPatch = &patches[j];
					DBGLOG("shiki", "found nvidia patch at %lu:%lu with size %lu", i, j, nvPatch->size);
					break;
				}
			}
		}

		if (nvPatch) {
			nvPatch->section = SectionUnused;
			nvPatch->find    = nvPatchFind;
			nvPatch->replace = nvPatchReplace;
			nvPatch->size    = NVPatchSize;
		} else {
			SYSLOG("shiki", "failed to find nvidia patch though requested");
			requireNvidiaPatch = false;
		}
	}

	bool patcherLoadOk = false;
	if (requireNvidiaPatch || autodetectIGPU || replaceBoardID) {
		auto err = lilu.onPatcherLoad(shikiPatcherLoad);
		if (err == LiluAPI::Error::NoError)
			patcherLoadOk = true;
		else
			SYSLOG("shiki", "unable to attach to patcher load %d", err);
	}

	if (!addExeWhiteList || !patcherLoadOk)
		disableSection(SectionNVDA);

	if (!killKeyExchange || (autodetectIGPU && !patcherLoadOk))
		disableSection(SectionKEGVA);

	auto err = lilu.onProcLoad(ADDPR(procInfo), ADDPR(procInfoSize), nullptr, nullptr, ADDPR(binaryMod), ADDPR(binaryModSize));
	if (err != LiluAPI::Error::NoError)
		SYSLOG("shiki", "unable to attach to process load %d", err);
}

static const char *bootargOff[] {
	"-shikioff"
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
	LiluAPI::AllowNormal,
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
