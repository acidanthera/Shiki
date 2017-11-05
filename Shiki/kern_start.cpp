//
//  kern_start.cpp
//  Shiki
//
//  Copyright © 2016-2017 vit9696. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_file.hpp>

#include "kern_resources.hpp"

enum ShikiGVAPatches {
	ForceOnlineRenderer = 1,
	AllowNonBGRA = 2,
	ForceCompatibleRenderer = 4,
	VDAExecutableWhitelist = 8,
    KillAppleGVA = 16
};

// 32 bytes should be reasonable for a safe comparison
static constexpr size_t NVPatchSize {32};
static uint8_t nvPatchFind[NVPatchSize] {};
static uint8_t nvPatchReplace[NVPatchSize] {};
static UserPatcher::BinaryModPatch *nvPatch {nullptr};

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

static void shikiNvidiaPatch(void *, KernelPatcher &) {
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
	} else {
		SYSLOG("shiki", "failed to find suitable nvpatch");
	}
}

static void shikiStart() {
	// Attempt to support fps.1_0 in Safari
	char tmp[16];
	bool patchStreamVideo = PE_parse_boot_argn("-shikifps", tmp, sizeof(tmp));
	bool leaveForceAccelRenderer = true;
	bool leaveBGRASupport = true;
	bool leaveNvidiaUnlock = true;
	bool leaveExecutableWhiteList = true;
    bool leaveAppleGVAAlone = true;
	
	if (PE_parse_boot_argn("shikigva", tmp, sizeof(tmp))) {
		leaveForceAccelRenderer = !(tmp[0] & ForceOnlineRenderer);
		leaveBGRASupport = !(tmp[0] & AllowNonBGRA);
		leaveNvidiaUnlock = !(tmp[0] & ForceCompatibleRenderer);
		leaveExecutableWhiteList = !(tmp[0] & VDAExecutableWhitelist);
        leaveAppleGVAAlone = !(tmp[0] & KillAppleGVA);
	}
	
	if (PE_parse_boot_argn("-shikigva", tmp, sizeof(tmp))) {
		SYSLOG("shiki", "-shikigva is deprecated use shikigva=1 instead");
		leaveForceAccelRenderer = false;
	}

	DBGLOG("shiki", "stream %d accel %d bgra %d nvidia %d/%d gva %d", patchStreamVideo, !leaveForceAccelRenderer,
		   !leaveBGRASupport, !leaveNvidiaUnlock, !leaveExecutableWhiteList, !leaveAppleGVAAlone);

	// Disable unused SectionFSTREAM
	if (!patchStreamVideo)
		disableSection(SectionNSTREAM);
	
	if (leaveForceAccelRenderer)
		disableSection(SectionOFFLINE);
	
	if (leaveBGRASupport)
		disableSection(SectionBGRA);
    
    if (leaveAppleGVAAlone)
        disableSection(SectionKILLGVA);

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

	bool nvPatchOk = false;
	if (nvPatch && !leaveNvidiaUnlock && getKernelVersion() >= KernelVersion::ElCapitan) {
		// Do not enable until we are certain it works
		nvPatch->section = SectionUnused;
		nvPatch->find    = nvPatchFind;
		nvPatch->replace = nvPatchReplace;
		nvPatch->size    = NVPatchSize;
		auto err = lilu.onPatcherLoad(shikiNvidiaPatch);
		if (err == LiluAPI::Error::NoError)
			nvPatchOk = true;
		else
			SYSLOG("shiki", "unable to attach to patcher load %d", err);
	}

	if (leaveExecutableWhiteList || !nvPatchOk)
		disableSection(SectionNVDA);

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
