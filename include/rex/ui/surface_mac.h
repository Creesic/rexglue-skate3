#pragma once
/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    ReXGlue contributors, 2026 - MoltenVK CAMetalLayer surface
 */

#include <cstdint>
#include <memory>

#include <rex/ui/surface.h>

#ifdef __OBJC__
@class CAMetalLayer;
@class NSView;
#else
typedef void CAMetalLayer;
typedef void NSView;
#endif

struct SDL_Window;

namespace rex {
namespace ui {

class MacNSViewSurface final : public Surface {
 public:
  explicit MacNSViewSurface(NSView* view) : view_(view) {}

  TypeIndex GetType() const override { return kTypeIndex_MacNSView; }
  NSView* view() const { return view_; }
  double GetBackingScale() const;
  void ConfigureMetalLayer(uint32_t drawable_width, uint32_t drawable_height) const;
  CAMetalLayer* GetOrCreateMetalLayer() const;

 protected:
  bool GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const override;

 private:
  NSView* view_ = nullptr;
};

std::unique_ptr<Surface> CreateMacNSViewSurface(SDL_Window* window);

}  // namespace ui
}  // namespace rex
