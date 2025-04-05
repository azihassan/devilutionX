#include "vmu.h"
#include <dc/vmu_fb.h>
// #include "sampman.h"

#if !defined(DC_TEXCONV)

#include <malloc.h>

#ifdef DC_SH4
#include <dc/sound/sound.h>
#endif

extern bool _bSampmanInitialised = true;

// ====== STATIC METHODS =====

VmuProfiler *VmuProfiler::getInstance()
{
	if (!instance_) {
		instance_ = std::unique_ptr<VmuProfiler>(new VmuProfiler);
	}
	return instance_.get();
}

void VmuProfiler::destroyInstance()
{
	if (instance_) {
		instance_->stopRequest_ = true;
		instance_.reset();
	}
}

float VmuProfiler::heapUtilization()
{
#ifdef DC_SH4
	// Query heap manager/allocator for info
	auto mallocInfo = mallinfo();

	// Used bytes are as resported
	size_t usedBlocks = mallocInfo.uordblks;
	// First component of free bytes are as reported
	size_t freeBlocks = mallocInfo.fordblks;

	// End address of region reserved for heap growth
	size_t brkEnd = _arch_mem_top - THD_KERNEL_STACK_SIZE - 1;
	// Amount of bytes the heap has yet to still grow
	size_t brkFree = brkEnd - reinterpret_cast<uintptr_t>(mm_sbrk(0));

	// Total heap space available is free blocks from allocator + unclaimed mm_sbrk() space
	freeBlocks += brkFree;

	// Return total utilization as a %
	return static_cast<float>(usedBlocks) / static_cast<float>(usedBlocks + freeBlocks) * 100.0f;
#else
	return 0.0f;
#endif
}

float VmuProfiler::vertexBufferUtilization()
{
#ifdef DC_SH4
	size_t start = PVR_GET(PVR_TA_VERTBUF_START);
	size_t end = PVR_GET(PVR_TA_VERTBUF_END);
	size_t pos = PVR_GET(PVR_TA_VERTBUF_POS);

	size_t used = pos - start;
	size_t free = end - pos;

	return static_cast<float>(used) / static_cast<float>(used + free) * 100.0f;
#else
	return 0.0f;
#endif
}

// ===== INSTANCE METHODS =====

VmuProfiler::VmuProfiler()
    : dc::Thread(
        "VMU Profiler",
        1024 * 2,
        false,
        [](void *param) -> void * {
	        static_cast<VmuProfiler *>(param)->run();
	        return nullptr;
        },
        this)
{
}

void VmuProfiler::run()
{
	while (!stopRequest_) {

#ifdef DC_SH4
		if (auto *dev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
		    dev && _bSampmanInitialised && updated_) {
			pvr_stats_t pvrStats;
			pvr_get_stats(&pvrStats);
			uint32_t sramStats = snd_mem_available();
			size_t pvrAvail = pvr_mem_available();
			float fps = std::accumulate(std::begin(fps_), std::end(fps_), 0.0f)
			    / static_cast<float>(fpsSamples);

			float sh4Mem = heapUtilization();
			float pvrMem = (8_MB - pvrAvail) / 8_MB * 100.0f;
			float armMem = (2_MB - sramStats) / 2_MB * 100.0f;
			float vtxBuf = vertBuffUse_;
			{
				std::shared_lock lk(mtx_);

				vmu_printf("FPS :%6.2f\n"
				           "SH4 :%6.2f%%\n"
				           "PVR :%6.2f%%\n"
				           "ARM :%6.2f%%\n"
				           "VTX :%6.2f%%",
				    fps, sh4Mem, pvrMem, armMem, vtxBuf);
			}
		}
#endif

		std::this_thread::sleep_for(updateRate_);
	}
}

void VmuProfiler::updateVertexBufferUsage()
{
	std::unique_lock lk(mtx_);
	updated_ = true;

#ifdef DC_SH4
	vertBuffUse_ = vertexBufferUtilization();

	pvr_stats_t pvrStats;
	pvr_get_stats(&pvrStats);
	fps_[fpsFrame_++] = pvrStats.frame_rate;

	if (fpsFrame_ >= fpsSamples)
		fpsFrame_ = 0;
#endif
}

#ifdef DC_SIM
#define vmu_beep_waveform(...) MAPLE_EOK
#define maple_enum_dev(...) nullptr
#endif

RAIIVmuBeeper::RAIIVmuBeeper(maple_device_t *dev, float period, float duty)
    : device_(dev)
{
	assert(period >= duty);

#ifdef WITH_BEEPS
	if (!dev) return;

	uint8_t period_raw = static_cast<uint8_t>(period * 255);
	uint8_t duty_raw = static_cast<uint8_t>(duty * 255);

	/* Give other threads a chance to unlock the maple frame,
	   and keep trying if we fail. */
	while (vmu_beep_waveform(dev, period_raw, duty_raw, 0, 0) != MAPLE_EOK)
		std::this_thread::yield();
#endif
}

RAIIVmuBeeper::RAIIVmuBeeper(std::string_view address, float period, float duty)
    : RAIIVmuBeeper(maple_enum_dev(address[0] - 'a', address[1] - '0'), period, duty)
{
}

RAIIVmuBeeper::~RAIIVmuBeeper()
{
	if (!device_) return;

	/* Oh dear god, MAKE IT STOP, BETTER EVENTUALLY SUCCEED! */
	while (vmu_beep_waveform(device_, 0, 0, 0, 0) != MAPLE_EOK)
		std::this_thread::yield();
}

#endif
