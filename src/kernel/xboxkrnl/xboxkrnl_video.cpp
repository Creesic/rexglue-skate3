/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

// Disable warnings about unused parameters for kernel functions
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <algorithm>
#include <string>

#include <rex/cvar.h>
#include <rex/graphics/flags.h>
#include <rex/graphics/command_processor.h>
#include <rex/graphics/graphics_system.h>
#include <rex/graphics/pipeline/texture/info.h>
#include <rex/graphics/video_mode_util.h>
#include <rex/graphics/xenos.h>
#include <rex/kernel/xboxkrnl/private.h>
#include <rex/kernel/xboxkrnl/rtl.h>
#include <rex/kernel/xboxkrnl/video.h>
#include <rex/logging.h>
#include <rex/hook.h>
#include <rex/types.h>
#include <rex/runtime.h>
#include <rex/system/export_resolver.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xtypes.h>
#include <rex/ui/flags.h>

namespace {
// Display gamma type: 0 - linear, 1 - sRGB (CRT), 2 - BT.709 (HDTV), 3 - power
constexpr uint32_t kDisplayGammaType = 2;
// Display gamma power (used with gamma type 3)
constexpr double kDisplayGammaPower = 2.22222233;

uint32_t GetConfiguredVideoModeWidth() {
  int32_t configured_width = REXCVAR_GET(video_mode_width);
  if (!rex::cvar::HasNonDefaultValue("video_mode_width")) {
    if (rex::cvar::HasNonDefaultValue("window_width") && REXCVAR_GET(window_width) > 0) {
      configured_width = REXCVAR_GET(window_width);
    } else {
      int32_t preset_width = 0;
      int32_t preset_height = 0;
      if (rex::graphics::video_mode_util::TryGetResolutionPresetFromCVar(preset_width,
                                                                         preset_height)) {
        configured_width = preset_width;
      }
    }
  }
  return uint32_t(std::clamp(configured_width, 640, 0x0FFF));
}

uint32_t GetConfiguredVideoModeHeight() {
  int32_t configured_height = REXCVAR_GET(video_mode_height);
  if (!rex::cvar::HasNonDefaultValue("video_mode_height")) {
    if (rex::cvar::HasNonDefaultValue("window_height") && REXCVAR_GET(window_height) > 0) {
      configured_height = REXCVAR_GET(window_height);
    } else {
      int32_t preset_width = 0;
      int32_t preset_height = 0;
      if (rex::graphics::video_mode_util::TryGetResolutionPresetFromCVar(preset_width,
                                                                         preset_height)) {
        configured_height = preset_height;
      }
    }
  }
  return uint32_t(std::clamp(configured_height, 480, 0x0FFF));
}

float GetConfiguredVideoModeRefreshRate() {
  double refresh_rate_hz = std::clamp(REXCVAR_GET(video_mode_refresh_rate), 24.0, 240.0);
  return float(refresh_rate_hz);
}
}  // namespace

namespace rex::kernel::xboxkrnl {
using namespace rex::system;

REX_EXPORT_STUB(__imp__VdBlockUntilGUIIdle);
REX_EXPORT_STUB(__imp__VdDisplayFatalError);
REX_EXPORT_STUB(__imp__VdEnableClosedCaption);
REX_EXPORT_STUB(__imp__VdEnableDisablePowerSavingMode);
REX_EXPORT_STUB(__imp__VdGenerateGPUCSCCoefficients);
REX_EXPORT_STUB(__imp__VdGetClosedCaptionReadyStatus);
REX_EXPORT_STUB(__imp__VdGetDisplayModeOverride);
REX_EXPORT_STUB(__imp__VdInitializeScaler);
REX_EXPORT_STUB(__imp__VdQuerySystemCommandBuffer);
REX_EXPORT_STUB(__imp__VdReadDVERegisterUlong);
REX_EXPORT_STUB(__imp__VdReadWriteHSIOCalibrationFlag);
REX_EXPORT_STUB(__imp__VdRegisterGraphicsNotification);
REX_EXPORT_STUB(__imp__VdRegisterXamGraphicsNotification);
REX_EXPORT_STUB(__imp__VdSendClosedCaptionData);
REX_EXPORT_STUB(__imp__VdSetCGMSOption);
REX_EXPORT_STUB(__imp__VdSetColorProfileAdjustment);
REX_EXPORT_STUB(__imp__VdSetCscMatricesOverride);
REX_EXPORT_STUB(__imp__VdSetHDCPOption);
REX_EXPORT_STUB(__imp__VdSetMacrovisionOption);
REX_EXPORT_STUB(__imp__VdSetSystemCommandBuffer);
REX_EXPORT_STUB(__imp__VdSetWSSData);
REX_EXPORT_STUB(__imp__VdSetWSSOption);
REX_EXPORT_STUB(__imp__VdTurnDisplayOff);
REX_EXPORT_STUB(__imp__VdTurnDisplayOn);
REX_EXPORT_STUB(__imp__VdWriteDVERegisterUlong);
REX_EXPORT_STUB(__imp__VdInitializeEDRAM);
REX_EXPORT_STUB(__imp__VdReadEEDIDBlock);
REX_EXPORT_STUB(__imp__VdEnumerateVideoModes);
REX_EXPORT_STUB(__imp__VdEnableHDCP);
REX_EXPORT_STUB(__imp__VdRegisterHDCPNotification);
REX_EXPORT_STUB(__imp__VdGetDisplayDiscoveryData);
REX_EXPORT_STUB(__imp__VdStartDisplayDiscovery);
REX_EXPORT_STUB(__imp__VdSetHDCPRevocationList);
REX_EXPORT_STUB(__imp__VdEnableWMAProOverHDMI);
REX_EXPORT_STUB(__imp__VdQueryRealVideoMode);
REX_EXPORT_STUB(__imp__VdSetCGMSState);
REX_EXPORT_STUB(__imp__VdSetSCMSState);
REX_EXPORT_STUB(__imp__VdGetOption);
REX_EXPORT_STUB(__imp__VdSetOption);
REX_EXPORT_STUB(__imp__VdQueryVideoCapabilities);
REX_EXPORT_STUB(__imp__VdGet3dVideoFormat);
REX_EXPORT_STUB(__imp__VdGetWSS2Data);
REX_EXPORT_STUB(__imp__VdSet3dVideoFormat);
REX_EXPORT_STUB(__imp__VdSetWSS2Data);
REX_EXPORT_STUB(__imp__VdSetStudioRGBMode);

// https://web.archive.org/web/20150805074003/https://www.tweakoz.com/orkid/
// http://www.tweakoz.com/orkid/dox/d3/d52/xb360init_8cpp_source.html
// https://github.com/Free60Project/xenosfb/
// https://github.com/Free60Project/xenosfb/blob/master/src/xe.h
// https://github.com/gligli/libxemit
// https://web.archive.org/web/20090428095215/https://msdn.microsoft.com/en-us/library/bb313877.aspx
// https://web.archive.org/web/20100423054747/https://msdn.microsoft.com/en-us/library/bb313961.aspx
// https://web.archive.org/web/20100423054747/https://msdn.microsoft.com/en-us/library/bb313878.aspx
// https://web.archive.org/web/20090510235238/https://msdn.microsoft.com/en-us/library/bb313942.aspx
// https://svn.dd-wrt.com/browser/src/linux/universal/linux-3.8/drivers/gpu/drm/radeon/radeon_ring.c?rev=21595
// https://www.microsoft.com/en-za/download/details.aspx?id=5313 -- "Stripped
// Down Direct3D: Xbox 360 Command Buffer and Resource Management"

void VdGetCurrentDisplayGamma_entry(mapped_u32 type_ptr, mapped_f32 power_ptr) {
  // 1 - sRGB.
  // 2 - TV (BT.709).
  // 3 - use the power written to *power_ptr.
  // Anything else - linear.
  // Used in D3D SetGammaRamp/SetPWLGamma to adjust the ramp for the display.
  *type_ptr = kDisplayGammaType;
  *power_ptr = float(kDisplayGammaPower);
}

struct X_D3DPRIVATE_RECT {
  rex::be<uint32_t> x1;  // 0x0
  rex::be<uint32_t> y1;  // 0x4
  rex::be<uint32_t> x2;  // 0x8
  rex::be<uint32_t> y2;  // 0xC
};
static_assert_size(X_D3DPRIVATE_RECT, 0x10);

struct X_D3DFILTER_PARAMETERS {
  rex::be<float> nyquist;         // 0x0
  rex::be<float> flicker_filter;  // 0x4
  rex::be<float> beta;            // 0x8
};
static_assert_size(X_D3DFILTER_PARAMETERS, 0xC);

struct X_D3DPRIVATE_SCALER_PARAMETERS {
  X_D3DPRIVATE_RECT scaler_source_rect;                 // 0x0
  rex::be<uint32_t> scaled_output_width;                // 0x10
  rex::be<uint32_t> scaled_output_height;               // 0x14
  rex::be<uint32_t> vertical_filter_type;               // 0x18
  X_D3DFILTER_PARAMETERS vertical_filter_parameters;    // 0x1C
  rex::be<uint32_t> horizontal_filter_type;             // 0x28
  X_D3DFILTER_PARAMETERS horizontal_filter_parameters;  // 0x2C
};
static_assert_size(X_D3DPRIVATE_SCALER_PARAMETERS, 0x38);

struct X_DISPLAY_INFO {
  rex::be<uint16_t> front_buffer_width;              // 0x0
  rex::be<uint16_t> front_buffer_height;             // 0x2
  uint8_t front_buffer_color_format;                 // 0x4
  uint8_t front_buffer_pixel_format;                 // 0x5
  X_D3DPRIVATE_SCALER_PARAMETERS scaler_parameters;  // 0x8
  rex::be<uint16_t> display_window_overscan_left;    // 0x40
  rex::be<uint16_t> display_window_overscan_top;     // 0x42
  rex::be<uint16_t> display_window_overscan_right;   // 0x44
  rex::be<uint16_t> display_window_overscan_bottom;  // 0x46
  rex::be<uint16_t> display_width;                   // 0x48
  rex::be<uint16_t> display_height;                  // 0x4A
  rex::be<float> display_refresh_rate;               // 0x4C
  rex::be<uint32_t> display_interlaced;              // 0x50
  uint8_t display_color_format;                      // 0x54
  rex::be<uint16_t> actual_display_width;            // 0x56
};
static_assert_size(X_DISPLAY_INFO, 0x58);

void VdGetCurrentDisplayInformation_entry(ppc_ptr_t<X_DISPLAY_INFO> display_info) {
  X_VIDEO_MODE mode;
  VdQueryVideoMode(&mode);
  display_info.Zero();
  display_info->front_buffer_width = (uint16_t)mode.display_width;
  display_info->front_buffer_height = (uint16_t)mode.display_height;

  display_info->scaler_parameters.scaler_source_rect.x2 = mode.display_width;
  display_info->scaler_parameters.scaler_source_rect.y2 = mode.display_height;
  display_info->scaler_parameters.scaled_output_width = mode.display_width;
  display_info->scaler_parameters.scaled_output_height = mode.display_height;
  display_info->scaler_parameters.horizontal_filter_type = 1;
  display_info->scaler_parameters.vertical_filter_type = 1;

  uint16_t overscan_x = uint16_t(uint32_t(mode.display_width) / 4);
  uint16_t overscan_y = uint16_t(uint32_t(mode.display_height) / 4);
  display_info->display_window_overscan_left = overscan_x;
  display_info->display_window_overscan_top = overscan_y;
  display_info->display_window_overscan_right = overscan_x;
  display_info->display_window_overscan_bottom = overscan_y;
  display_info->display_width = (uint16_t)mode.display_width;
  display_info->display_height = (uint16_t)mode.display_height;
  display_info->display_refresh_rate = mode.refresh_rate;
  display_info->actual_display_width = (uint16_t)mode.display_width;
}

void VdQueryVideoMode(X_VIDEO_MODE* video_mode) {
  // Exposed as CVARs so the guest can observe custom display settings.
  uint32_t display_width = GetConfiguredVideoModeWidth();
  uint32_t display_height = GetConfiguredVideoModeHeight();
  float refresh_rate_hz = GetConfiguredVideoModeRefreshRate();

  std::memset(video_mode, 0, sizeof(X_VIDEO_MODE));
  video_mode->display_width = display_width;
  video_mode->display_height = display_height;
  video_mode->is_interlaced = 0;
  video_mode->is_widescreen = display_width * 3 >= display_height * 4;
  video_mode->is_hi_def = display_width >= 1280 || display_height >= 720;
  video_mode->refresh_rate = refresh_rate_hz;
  video_mode->video_standard = 1;  // NTSC
  video_mode->unknown_0x8a = 0x4A;
  video_mode->unknown_0x01 = 0x01;
}

void VdQueryVideoMode_entry(ppc_ptr_t<X_VIDEO_MODE> video_mode) {
  VdQueryVideoMode(video_mode);
}

u32 VdQueryVideoFlags_entry() {
  X_VIDEO_MODE mode;
  VdQueryVideoMode(&mode);

  uint32_t flags = 0;
  flags |= mode.is_widescreen ? 1 : 0;
  flags |= mode.display_width >= 1024 ? 2 : 0;
  flags |= mode.display_width >= 1920 ? 4 : 0;

  return flags;
}

u32 VdSetDisplayMode_entry(u32 flags) {
  // Often 0x40000000.

  // 0?ccf000 00000000 00000000 000000r0

  // r: 0x00000002 |     1
  // f: 0x08000000 |    27
  // c: 0x30000000 | 28-29
  // ?: 0x40000000 |    30

  // r: 1 = Resolution is 720x480 or 720x576
  // f: 1 = Texture format is k_2_10_10_10 or k_2_10_10_10_AS_16_16_16_16
  // c: Color space (0 = RGB, 1 = ?, 2 = ?)
  // ?: (always set?)

  return 0;
}

u32 VdSetDisplayModeOverride_entry(u32 unk0, u32 unk1, f64 refresh_rate, u32 unk3, u32 unk4) {
  // refresh_rate = 0, 50, 59.9, etc.
  return 0;
}

u32 VdInitializeEngines_entry(u32 unk0, u32 callback, mapped_void arg, mapped_u32 pfp_ptr,
                              mapped_u32 me_ptr) {
  // r3 = 0x4F810000
  // r4 = function ptr (cleanup callback?)
  // r5 = function arg
  // r6 = PFP Microcode
  // r7 = ME Microcode
  return 1;
}

void VdShutdownEngines_entry() {
  // Ignored for now.
  // Games seem to call an Initialize/Shutdown pair to query info, then
  // re-initialize.
}

u32 VdGetGraphicsAsicID_entry() {
  // Games compare for < 0x10 and do VdInitializeEDRAM, else other
  // (retrain/etc).
  return 0x11;
}

u32 VdEnableDisableClockGating_entry(u32 enabled) {
  // Ignored, as it really doesn't matter.
  return 0;
}

void VdSetGraphicsInterruptCallback_entry(u32 callback, mapped_void user_data) {
  // callback takes 2 params
  // r3 = bool 0/1 - 0 is normal interrupt, 1 is some acquire/lock mumble
  // r4 = user_data (r4 of VdSetGraphicsInterruptCallback)
  auto* graphics_system =
      static_cast<graphics::GraphicsSystem*>(REX_KERNEL_STATE()->emulator()->graphics_system());
  if (!graphics_system)
    return;
  graphics_system->SetInterruptCallback(callback, user_data.guest_address());
}

void VdInitializeRingBuffer_entry(mapped_void ptr, i32 size_log2) {
  // r3 = result of MmGetPhysicalAddress
  // r4 = log2(size)
  // Buffer pointers are from MmAllocatePhysicalMemory with WRITE_COMBINE.
  auto* graphics_system =
      static_cast<graphics::GraphicsSystem*>(REX_KERNEL_STATE()->emulator()->graphics_system());
  if (!graphics_system)
    return;
  REXKRNL_WARN("VdInitializeRingBuffer ptr={:08X} size_log2={}", ptr.guest_address(), size_log2);
  graphics_system->InitializeRingBuffer(ptr.guest_address(), size_log2);
}

void VdEnableRingBufferRPtrWriteBack_entry(mapped_void ptr, i32 block_size_log2) {
  // r4 = log2(block size), 6, usually --- <=19
  auto* graphics_system =
      static_cast<graphics::GraphicsSystem*>(REX_KERNEL_STATE()->emulator()->graphics_system());
  if (!graphics_system)
    return;
  graphics_system->EnableReadPointerWriteBack(ptr.guest_address(), block_size_log2);
}

void VdGetSystemCommandBuffer_entry(mapped_void p0_ptr, mapped_void p1_ptr) {
  p0_ptr.Zero(0x94);
  memory::store_and_swap<uint32_t>(p0_ptr, 0xBEEF0000);
  memory::store_and_swap<uint32_t>(p1_ptr, 0xBEEF0001);
}

void VdSetSystemCommandBufferGpuIdentifierAddress_entry(mapped_void unk) {
  // r3 = 0x2B10(d3d?) + 8
}

// VdVerifyMEInitCommand
// r3
// r4 = 19
// no op?

u32 VdInitializeScalerCommandBuffer_entry(
    u32 scaler_source_xy,                                      // ((uint16_t)y << 16) | (uint16_t)x
    u32 scaler_source_wh,                                      // ((uint16_t)h << 16) | (uint16_t)w
    u32 scaled_output_xy,                                      // ((uint16_t)y << 16) | (uint16_t)x
    u32 scaled_output_wh,                                      // ((uint16_t)h << 16) | (uint16_t)w
    u32 front_buffer_wh,                                       // ((uint16_t)h << 16) | (uint16_t)w
    u32 vertical_filter_type,                                  // 7?
    ppc_ptr_t<X_D3DFILTER_PARAMETERS> vertical_filter_params,  //
    u32 horizontal_filter_type,                                // 7?
    ppc_ptr_t<X_D3DFILTER_PARAMETERS> horizontal_filter_params,  //
    mapped_void unk9,                                            //
    mapped_void dest_ptr,  // Points to the first 80000000h where the memcpy
                           // sources from.
    u32 dest_count         // Count in words.
) {
  // We could fake the commands here, but I'm not sure the game checks for
  // anything but success (non-zero ret).
  // For now, we just fill it with NOPs.
  auto dest = dest_ptr.as_array<uint32_t>();
  for (size_t i = 0; i < dest_count; ++i) {
    dest[i] = 0x80000000;
  }
  return (uint32_t)dest_count;
}

struct BufferScaling {
  rex::be<uint16_t> fb_width;
  rex::be<uint16_t> fb_height;
  rex::be<uint16_t> bb_width;
  rex::be<uint16_t> bb_height;
};
void AppendParam(string::StringBuffer* string_buffer, ppc_ptr_t<BufferScaling> param) {
  string_buffer->AppendFormat("{:08X}(scale {}x{} -> {}x{}))", param.guest_address(),
                              uint16_t(param->bb_width), uint16_t(param->bb_height),
                              uint16_t(param->fb_width), uint16_t(param->fb_height));
}

u32 VdCallGraphicsNotificationRoutines_entry(u32 unk0, ppc_ptr_t<BufferScaling> args_ptr) {
  assert_true(unk0 == 1);

  // TODO(benvanik): what does this mean, I forget:
  // callbacks get 0, r3, r4

  return 0;
}

u32 VdIsHSIOTrainingSucceeded_entry() {
  // BOOL return value
  return 1;
}

u32 VdPersistDisplay_entry(u32 unk0, mapped_u32 unk1_ptr) {
  // unk1_ptr needs to be populated with a pointer passed to
  // MmFreePhysicalMemory(1, *unk1_ptr).
  if (unk1_ptr) {
    auto heap = REX_KERNEL_MEMORY()->LookupHeapByType(true, 16 * 1024);
    uint32_t unk1_value;
    heap->Alloc(64, 32, memory::kMemoryAllocationReserve | memory::kMemoryAllocationCommit,
                memory::kMemoryProtectNoAccess, false, &unk1_value);
    *unk1_ptr = unk1_value;
  }

  return 1;
}

u32 VdRetrainEDRAMWorker_entry(u32 unk0) {
  return 0;
}

u32 VdRetrainEDRAM_entry(u32 unk0, u32 unk1, u32 unk2, u32 unk3, u32 unk4, u32 unk5) {
  return 0;
}

void VdSwap_entry(mapped_void buffer_ptr,      // ptr into primary ringbuffer
                  mapped_void fetch_ptr,       // frontbuffer Direct3D 9 texture header fetch
                  mapped_void unk2,            // system writeback ptr
                  mapped_void unk3,            // buffer from VdGetSystemCommandBuffer
                  mapped_void unk4,            // from VdGetSystemCommandBuffer (0xBEEF0001)
                  mapped_u32 frontbuffer_ptr,  // ptr to frontbuffer address
                  mapped_u32 texture_format_ptr, mapped_u32 color_space_ptr, mapped_u32 width,
                  mapped_u32 height) {
  // All of these parameters are REQUIRED.
  assert(buffer_ptr);
  assert(fetch_ptr);
  assert(frontbuffer_ptr);
  assert(texture_format_ptr);
  assert(width);
  assert(height);

  namespace xenos = rex::graphics::xenos;

  static uint32_t vd_swap_log_count = 0;

  xenos::xe_gpu_texture_fetch_t gpu_fetch;
  memory::copy_and_swap_32_unaligned(&gpu_fetch,
                                     reinterpret_cast<uint32_t*>(fetch_ptr.host_address()), 6);

  // The fetch constant passed is not a true GPU fetch constant, but rather, the
  // fetch constant stored in the Direct3D 9 texture header, which contains the
  // address in one of the virtual mappings of the physical memory rather than
  // the physical address itself. We're emulating swapping in the GPU subsystem,
  // which works with GPU memory addresses (physical addresses directly) from
  // proper fetch constants like ones used to bind textures to shaders, not CPU
  // MMU addresses, so translation from virtual to physical is needed.
  uint32_t frontbuffer_virtual_address = gpu_fetch.base_address << 12;
  assert_true(*frontbuffer_ptr == frontbuffer_virtual_address);
  uint32_t frontbuffer_physical_address =
      REX_KERNEL_MEMORY()->GetPhysicalAddress(frontbuffer_virtual_address);
  uint32_t buffer_physical_address = REX_KERNEL_MEMORY()->GetPhysicalAddress(buffer_ptr.guest_address());
  if (vd_swap_log_count < 256) {
    const uint32_t* frontbuffer_sample =
        REX_KERNEL_MEMORY()->TranslatePhysical<const uint32_t*>(frontbuffer_physical_address);
    const uint32_t sample_dwords = uint32_t(*width) * uint32_t(*height);
    uint32_t nonzero = 0;
    uint32_t first_nonzero_offset = UINT32_MAX;
    uint32_t first_nonzero_value = 0;
    uint32_t xor_checksum = 0;
    if (frontbuffer_sample != nullptr) {
      for (uint32_t i = 0; i < sample_dwords; ++i) {
        uint32_t value = frontbuffer_sample[i];
        xor_checksum ^= value;
        if (value != 0 && first_nonzero_offset == UINT32_MAX) {
          first_nonzero_offset = i * sizeof(uint32_t);
          first_nonzero_value = value;
        }
        nonzero += uint32_t(value != 0);
      }
    }
    REXKRNL_INFO(
        "VdSwap #{} buffer={:08X} buffer_pa={:08X} fetch={:08X} fb_va={:08X} fb_arg={:08X} "
        "fb_pa={:08X} format={} color_space={} size={}x{} fetch_size={}x{} fb_nonzero={} "
        "fb_first_off={} fb_first={:08X} fb_xor={:08X}",
        vd_swap_log_count, buffer_ptr.guest_address(), buffer_physical_address,
        fetch_ptr.guest_address(), frontbuffer_virtual_address, frontbuffer_ptr.value(),
        frontbuffer_physical_address, texture_format_ptr.value(), uint32_t(*color_space_ptr),
        uint32_t(*width), uint32_t(*height),
        1 + gpu_fetch.size_2d.width, 1 + gpu_fetch.size_2d.height, nonzero,
        first_nonzero_offset == UINT32_MAX ? -1 : int32_t(first_nonzero_offset),
        first_nonzero_value, xor_checksum);
    ++vd_swap_log_count;
  }
  assert_true(frontbuffer_physical_address != UINT32_MAX);
  if (frontbuffer_physical_address == UINT32_MAX) {
    // Xenia-specific safety check.
    REXKRNL_ERROR("VdSwap: Invalid front buffer virtual address 0x{:08X}",
                  frontbuffer_virtual_address);
    return;
  }
  gpu_fetch.base_address = frontbuffer_physical_address >> 12;

  auto texture_format = rex::graphics::xenos::TextureFormat(texture_format_ptr.value());
  auto color_space = *color_space_ptr;
  assert_true(texture_format == rex::graphics::xenos::TextureFormat::k_8_8_8_8 ||
              texture_format == rex::graphics::xenos::TextureFormat::k_2_10_10_10_AS_16_16_16_16);
  assert_true(color_space == 0);  // RGB(0)
  assert_true(*width == 1 + gpu_fetch.size_2d.width);
  assert_true(*height == 1 + gpu_fetch.size_2d.height);

  // The caller seems to reserve 64 words (256b) in the primary ringbuffer
  // for this method to do what it needs. We just zero them out and send a
  // token value. It'd be nice to figure out what this is really doing so
  // that we could simulate it, though due to TCR I bet all games need to
  // use this method.
  buffer_ptr.Zero(64 * 4);

  uint32_t offset = 0;
  auto dwords = buffer_ptr.as_array<uint32_t>();

  // PM4_XE_SWAP carries the frontbuffer address directly. Do not write the
  // synthetic texture fetch into fetch slot 0, as later draws may use that
  // overlapping register as vertex fetch 0.
  dwords[offset++] = xenos::MakePacketType3(xenos::PM4_XE_SWAP, 4);
  dwords[offset++] = rex::graphics::xenos::kSwapSignature;
  dwords[offset++] = frontbuffer_physical_address;

  dwords[offset++] = *width;
  dwords[offset++] = *height;

  // Fill the rest of the buffer with NOP packets.
  for (uint32_t i = offset; i < 64; i++) {
    dwords[i] = xenos::MakePacketType2();
  }

  // Some generated titles call VdSwap with a standalone system command buffer
  // but never kick a primary ring write pointer for that buffer. PGR3 appends
  // real PM4 work before each VdSwap packet, so submit the newly-written span
  // instead of only the synthesized swap packet.
  auto* graphics_system =
      static_cast<graphics::GraphicsSystem*>(REX_KERNEL_STATE()->emulator()->graphics_system());
  graphics::CommandProcessor* command_processor =
      graphics_system ? graphics_system->command_processor() : nullptr;
  if (command_processor && frontbuffer_physical_address != UINT32_MAX) {
    command_processor->SetPendingSwapFrontbuffer(frontbuffer_physical_address, uint32_t(*width),
                                                 uint32_t(*height));
  }
  static uint32_t vd_swap_bridge_state_log_count = 0;
  if (vd_swap_bridge_state_log_count < 16) {
    const bool in_primary_ring =
        command_processor &&
        command_processor->IsPhysicalRangeInPrimaryRingBuffer(buffer_physical_address, 64 * 4);
    REXKRNL_WARN(
        "VdSwap bridge state #{} gs={} cp={} primary_ring={} seen_wptr={} in_ring={} "
        "ring={:08X}+{:08X}",
        vd_swap_bridge_state_log_count, graphics_system != nullptr,
        command_processor != nullptr,
        command_processor ? command_processor->has_primary_ring_buffer() : false,
        command_processor ? command_processor->has_seen_write_pointer() : false, in_primary_ring,
        command_processor ? command_processor->primary_ring_buffer_ptr() : 0,
        command_processor ? command_processor->primary_ring_buffer_size() : 0);
    ++vd_swap_bridge_state_log_count;
  }
  if (command_processor && buffer_physical_address != UINT32_MAX) {
    const bool packet_in_primary_ring =
        command_processor->IsPhysicalRangeInPrimaryRingBuffer(buffer_physical_address, 64 * 4);
    static uint32_t last_system_command_buffer_end_physical = UINT32_MAX;
    static uint32_t vd_swap_immediate_log_count = 0;
    auto count_valid_pm4_prefix_dwords = [](uint32_t start_physical,
                                            uint32_t max_dwords) -> uint32_t {
      if (!max_dwords) {
        return 0;
      }
      const uint32_t* raw_dwords =
          REX_KERNEL_MEMORY()->TranslatePhysical<const uint32_t*>(start_physical);
      if (!raw_dwords) {
        return 0;
      }
      uint32_t offset = 0;
      while (offset < max_dwords) {
        const uint32_t packet = rex::byte_swap(raw_dwords[offset]);
        uint32_t packet_dwords = 1;
        if (packet != 0 && packet != 0x0BADF00D) {
          switch (packet >> 30) {
            case 0:
              packet_dwords = 1 + ((packet >> 16) & 0x3FFF) + 1;
              break;
            case 1:
              packet_dwords = 3;
              break;
            case 2:
              packet_dwords = 1;
              break;
            case 3:
              packet_dwords = 1 + ((packet >> 16) & 0x3FFF) + 1;
              break;
            default:
              packet_dwords = max_dwords + 1;
              break;
          }
        }
        if (packet_dwords > max_dwords - offset) {
          static uint32_t trim_log_count = 0;
          if (trim_log_count < 32) {
            REXKRNL_WARN(
                "VdSwap standalone PM4 trim #{} span_pa={:08X} valid_dwords={} "
                "max_dwords={} packet_pa={:08X} packet={:08X} packet_dwords={}",
                trim_log_count, start_physical, offset, max_dwords,
                start_physical + offset * sizeof(uint32_t), packet, packet_dwords);
            ++trim_log_count;
          }
          break;
        }
        offset += packet_dwords;
      }
      return offset;
    };

    constexpr uint32_t kVdSwapPacketDwords = 64;
    constexpr uint32_t kMaxStandaloneSubmitBytes = 4 * 1024 * 1024;
    const uint32_t current_packet_end_physical =
        buffer_physical_address + kVdSwapPacketDwords * sizeof(uint32_t);

    if (packet_in_primary_ring) {
      last_system_command_buffer_end_physical = UINT32_MAX;
    } else if (current_packet_end_physical > buffer_physical_address) {
      uint32_t submit_start_physical = buffer_physical_address;
      bool reset_span = last_system_command_buffer_end_physical == UINT32_MAX;
      if (!reset_span) {
        const uint32_t span_bytes =
            buffer_physical_address - last_system_command_buffer_end_physical;
        if (buffer_physical_address >= last_system_command_buffer_end_physical &&
            span_bytes <= kMaxStandaloneSubmitBytes) {
          submit_start_physical = last_system_command_buffer_end_physical;
        } else {
          reset_span = true;
        }
      }

      const uint32_t prefix_bytes = buffer_physical_address - submit_start_physical;
      const uint32_t prefix_dword_limit = prefix_bytes / sizeof(uint32_t);
      const uint32_t prefix_dword_count =
          count_valid_pm4_prefix_dwords(submit_start_physical, prefix_dword_limit);
      const uint32_t submit_bytes = prefix_dword_count * sizeof(uint32_t);
      if (prefix_dword_count != 0 && submit_bytes <= kMaxStandaloneSubmitBytes) {
        if (vd_swap_immediate_log_count < 32) {
          REXKRNL_WARN(
              "VdSwap queued submit #{} span_pa={:08X} dwords={} limit={} current_packet={:08X} "
              "fb_pa={:08X} size={}x{} reset={}",
              vd_swap_immediate_log_count, submit_start_physical, prefix_dword_count,
              prefix_dword_limit,
              buffer_physical_address, frontbuffer_physical_address, uint32_t(*width),
              uint32_t(*height), reset_span);
          ++vd_swap_immediate_log_count;
        }
        rex::thread::Fence submit_fence;
        command_processor->CallInThread(
            [command_processor, submit_start_physical, prefix_dword_count, &submit_fence]() {
              command_processor->ExecutePacket(submit_start_physical, prefix_dword_count);
              submit_fence.Signal();
            });
        submit_fence.Wait();
      } else if (prefix_dword_limit != 0 && vd_swap_immediate_log_count < 32) {
        REXKRNL_WARN(
            "VdSwap skipped standalone PM4 prefix span_pa={:08X} limit={} current_packet={:08X}",
            submit_start_physical, prefix_dword_limit, buffer_physical_address);
        ++vd_swap_immediate_log_count;
      }

      rex::thread::Fence swap_fence;
      command_processor->CallInThread(
          [command_processor, buffer_physical_address, &swap_fence]() {
            command_processor->ExecutePacket(buffer_physical_address, kVdSwapPacketDwords);
            swap_fence.Signal();
          });
      swap_fence.Wait();
      last_system_command_buffer_end_physical = current_packet_end_physical;
    }
  }
}

void RegisterVideoExports(rex::runtime::ExportResolver* export_resolver,
                          KernelState* kernel_state) {
  auto memory = kernel_state->memory();

  // VdGlobalDevice (4b)
  // Pointer to a global D3D device. Games only seem to set this, so we don't
  // have to do anything. We may want to read it back later, though.
  uint32_t pVdGlobalDevice = memory->SystemHeapAlloc(4, 32, memory::kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", 0x01BE, pVdGlobalDevice);
  memory::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGlobalDevice), 0);

  // VdGlobalXamDevice (4b)
  // Pointer to the XAM D3D device, which we don't have.
  uint32_t pVdGlobalXamDevice = memory->SystemHeapAlloc(4, 32, memory::kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", 0x01BF, pVdGlobalXamDevice);
  memory::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGlobalXamDevice), 0);

  // VdGpuClockInMHz (4b)
  // GPU clock. Xenos is 500MHz. Hope nothing is relying on this timing...
  uint32_t pVdGpuClockInMHz = memory->SystemHeapAlloc(4, 32, memory::kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", 0x01C0, pVdGpuClockInMHz);
  memory::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGpuClockInMHz), 500);

  // VdHSIOCalibrationLock (28b)
  // CriticalSection.
  uint32_t pVdHSIOCalibrationLock = memory->SystemHeapAlloc(28, 32, memory::kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", 0x01C1, pVdHSIOCalibrationLock);
  auto hsio_lock = memory->TranslateVirtual<X_RTL_CRITICAL_SECTION*>(pVdHSIOCalibrationLock);
  xeRtlInitializeCriticalSectionAndSpinCount(hsio_lock, pVdHSIOCalibrationLock, 10000);
}

}  // namespace rex::kernel::xboxkrnl

REX_EXPORT(__imp__VdGetCurrentDisplayGamma, rex::kernel::xboxkrnl::VdGetCurrentDisplayGamma_entry)
REX_EXPORT(__imp__VdGetCurrentDisplayInformation,
           rex::kernel::xboxkrnl::VdGetCurrentDisplayInformation_entry)
REX_EXPORT(__imp__VdQueryVideoMode, rex::kernel::xboxkrnl::VdQueryVideoMode_entry)
REX_EXPORT(__imp__VdQueryVideoFlags, rex::kernel::xboxkrnl::VdQueryVideoFlags_entry)
REX_EXPORT(__imp__VdSetDisplayMode, rex::kernel::xboxkrnl::VdSetDisplayMode_entry)
REX_EXPORT(__imp__VdSetDisplayModeOverride, rex::kernel::xboxkrnl::VdSetDisplayModeOverride_entry)
REX_EXPORT(__imp__VdInitializeEngines, rex::kernel::xboxkrnl::VdInitializeEngines_entry)
REX_EXPORT(__imp__VdShutdownEngines, rex::kernel::xboxkrnl::VdShutdownEngines_entry)
REX_EXPORT(__imp__VdGetGraphicsAsicID, rex::kernel::xboxkrnl::VdGetGraphicsAsicID_entry)
REX_EXPORT(__imp__VdEnableDisableClockGating,
           rex::kernel::xboxkrnl::VdEnableDisableClockGating_entry)
REX_EXPORT(__imp__VdSetGraphicsInterruptCallback,
           rex::kernel::xboxkrnl::VdSetGraphicsInterruptCallback_entry)
REX_EXPORT(__imp__VdInitializeRingBuffer, rex::kernel::xboxkrnl::VdInitializeRingBuffer_entry)
REX_EXPORT(__imp__VdEnableRingBufferRPtrWriteBack,
           rex::kernel::xboxkrnl::VdEnableRingBufferRPtrWriteBack_entry)
REX_EXPORT(__imp__VdGetSystemCommandBuffer, rex::kernel::xboxkrnl::VdGetSystemCommandBuffer_entry)
REX_EXPORT(__imp__VdSetSystemCommandBufferGpuIdentifierAddress,
           rex::kernel::xboxkrnl::VdSetSystemCommandBufferGpuIdentifierAddress_entry)
REX_EXPORT(__imp__VdInitializeScalerCommandBuffer,
           rex::kernel::xboxkrnl::VdInitializeScalerCommandBuffer_entry)
REX_EXPORT(__imp__VdCallGraphicsNotificationRoutines,
           rex::kernel::xboxkrnl::VdCallGraphicsNotificationRoutines_entry)
REX_EXPORT(__imp__VdIsHSIOTrainingSucceeded, rex::kernel::xboxkrnl::VdIsHSIOTrainingSucceeded_entry)
REX_EXPORT(__imp__VdPersistDisplay, rex::kernel::xboxkrnl::VdPersistDisplay_entry)
REX_EXPORT(__imp__VdRetrainEDRAMWorker, rex::kernel::xboxkrnl::VdRetrainEDRAMWorker_entry)
REX_EXPORT(__imp__VdRetrainEDRAM, rex::kernel::xboxkrnl::VdRetrainEDRAM_entry)
REX_EXPORT(__imp__VdSwap, rex::kernel::xboxkrnl::VdSwap_entry)
