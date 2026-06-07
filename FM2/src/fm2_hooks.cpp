#include "generated/fm2_init.h"

#include <cstdint>
#include <limits>

#include <rex/memory/utils.h>
#include <rex/ppc.h>
#include <rex/system/kernel_state.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <mach/mach_time.h>
#include <unistd.h>
#include <thread>
#endif

namespace {

uint8_t* GuestBase() {
  auto* kernelState = rex::system::kernel_state();
  if (!kernelState || !kernelState->memory()) {
    return nullptr;
  }
  return kernelState->memory()->virtual_membase();
}

bool GuestReadableByte(uint8_t* base, uint32_t guestAddress) {
  size_t length = 1;
  rex::memory::PageAccess access = rex::memory::PageAccess::kNoAccess;
  if (!rex::memory::QueryProtect(REX_RAW_ADDR(guestAddress), length, access)) {
    return false;
  }
  return access != rex::memory::PageAccess::kNoAccess;
}

bool GuestReadableRange(uint8_t* base, uint32_t guestAddress, uint32_t byteCount) {
  if (byteCount == 0) {
    return true;
  }

  const uint64_t last = uint64_t(guestAddress) + byteCount - 1;
  if (last > std::numeric_limits<uint32_t>::max()) {
    return false;
  }

  return GuestReadableByte(base, guestAddress) &&
         GuestReadableByte(base, static_cast<uint32_t>(last));
}

bool HasCallableVtableSlot(uint8_t* base, uint32_t object, uint32_t slotOffset) {
  if (object == 0 || (object & 3) != 0 || !GuestReadableRange(base, object, 4)) {
    return false;
  }

  const uint32_t vtable = REX_LOAD_U32(object);
  const uint64_t slot = uint64_t(vtable) + slotOffset;
  if ((vtable & 3) != 0 || slot > std::numeric_limits<uint32_t>::max() ||
      !GuestReadableRange(base, static_cast<uint32_t>(slot), 4)) {
    return false;
  }

  const uint32_t target = REX_LOAD_U32(static_cast<uint32_t>(slot));
  constexpr uint64_t codeEnd = uint64_t(REX_CODE_BASE) + REX_CODE_SIZE;
  return (target & 3) == 0 && target >= REX_CODE_BASE && target < codeEnd;
}

}  // namespace

bool FM2SkipBadChildSlot(PPCRegister& r11, PPCRegister& r31) {
  uint8_t* base = GuestBase();
  if (!base || HasCallableVtableSlot(base, r11.u32, 12)) {
    return false;
  }

  REX_STORE_U32(r31.u32, 0);
  return true;
}

bool FM2ReturnZeroOnBadNestedVcall(PPCRegister& r3) {
  uint8_t* base = GuestBase();
  return base && !HasCallableVtableSlot(base, r3.u32, 44);
}

bool FM2ReturnOnBadListHead(PPCRegister& r10) {
  uint8_t* base = GuestBase();
  return base && !GuestReadableRange(base, r10.u32, 4);
}

bool FM2ReturnOnBadD5A8ListHead(PPCRegister& r11) {
  uint8_t* base = GuestBase();
  return base && !GuestReadableRange(base, r11.u32, 4);
}

bool FM2ReturnOnBadD4F8ListHead(PPCRegister& r10) {
  uint8_t* base = GuestBase();
  return base && !GuestReadableRange(base, r10.u32, 4);
}

bool FM2ReturnOnBad75A40Object(PPCRegister& r30) {
  uint8_t* base = GuestBase();
  return base && !GuestReadableRange(base, r30.u32, 172);
}

bool FM2ReturnZeroOnBad76A58Object(PPCRegister& r30, PPCRegister& r29) {
  uint8_t* base = GuestBase();
  if (!base || GuestReadableRange(base, r30.u32, 156)) {
    return false;
  }

  r29.u64 = 0;
  return true;
}

bool FM2ReturnOnBad75ED0Mask(PPCRegister& r25) {
  uint8_t* base = GuestBase();
  return base && !GuestReadableRange(base, r25.u32, 124);
}

bool FM2ReturnOnBad40160PrimaryResult(PPCRegister& r25) {
  uint8_t* base = GuestBase();
  return base && (!HasCallableVtableSlot(base, r25.u32, 20) ||
                  !HasCallableVtableSlot(base, r25.u32, 8));
}

bool FM2SpinWaitYield(PPCRegister& f0, PPCRegister& f31) {
    double remaining_sec = f31.f64 - f0.f64;
    if (remaining_sec > 0.0) {
#ifdef _WIN32
        LARGE_INTEGER start, end;
        QueryPerformanceCounter(&start);
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        int64_t target_ticks = static_cast<int64_t>(remaining_sec * static_cast<double>(freq.QuadPart));
        if (target_ticks > 0) {
            if (remaining_sec >= 0.001) {
                DWORD ms = static_cast<DWORD>(remaining_sec * 1000.0);
                Sleep(ms);
            }
            QueryPerformanceCounter(&end);
            while ((end.QuadPart - start.QuadPart) < target_ticks) {
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                QueryPerformanceCounter(&end);
            }
        }
#else
        uint64_t start = mach_absolute_time();
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        uint64_t target_ticks = static_cast<uint64_t>(
            remaining_sec * 1e9 * timebase.denom / timebase.numer);
        if (target_ticks > 0) {
            if (remaining_sec >= 0.001) {
                auto ms = static_cast<unsigned>(remaining_sec * 1000.0);
                usleep(ms * 1000);
            }
            uint64_t end = mach_absolute_time();
            while ((end - start) < target_ticks) {
                __asm__ volatile("yield");
                __asm__ volatile("yield");
                __asm__ volatile("yield");
                __asm__ volatile("yield");
                end = mach_absolute_time();
            }
        }
#endif
    }
    return false;
}
