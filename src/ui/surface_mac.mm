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

#include <cmath>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <rex/ui/surface_mac.h>

namespace rex {
namespace ui {

bool MacNSViewSurface::GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const {
  if (!view_) {
    width_out = 0;
    height_out = 0;
    return false;
  }

  const NSRect bounds = [view_ bounds];
  if (bounds.size.width <= 0.0 || bounds.size.height <= 0.0) {
    width_out = 0;
    height_out = 0;
    return false;
  }

  const NSRect backing_bounds = [view_ convertRectToBacking:bounds];
  if (backing_bounds.size.width <= 0.0 || backing_bounds.size.height <= 0.0) {
    width_out = 0;
    height_out = 0;
    return false;
  }

  width_out = static_cast<uint32_t>(std::lround(backing_bounds.size.width));
  height_out = static_cast<uint32_t>(std::lround(backing_bounds.size.height));
  return width_out && height_out;
}

double MacNSViewSurface::GetBackingScale() const {
  if (!view_) {
    return 1.0;
  }

  const NSRect bounds = [view_ bounds];
  if (bounds.size.width > 0.0) {
    const NSRect backing_bounds = [view_ convertRectToBacking:bounds];
    if (backing_bounds.size.width > 0.0) {
      return backing_bounds.size.width / bounds.size.width;
    }
  }

  NSScreen* screen = [view_ window].screen;
  if (!screen) {
    screen = NSScreen.mainScreen;
  }
  return screen ? screen.backingScaleFactor : 1.0;
}

void MacNSViewSurface::ConfigureMetalLayer(uint32_t drawable_width,
                                           uint32_t drawable_height) const {
  CAMetalLayer* metal_layer = GetOrCreateMetalLayer();
  if (!metal_layer) {
    return;
  }

  CALayer* main_layer = [view_ layer];
  metal_layer.framebufferOnly = YES;
  metal_layer.opaque = YES;
  metal_layer.frame = main_layer.bounds;
  metal_layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
  metal_layer.contentsScale = GetBackingScale();
  metal_layer.drawableSize = CGSizeMake(drawable_width, drawable_height);
}

CAMetalLayer* MacNSViewSurface::GetOrCreateMetalLayer() const {
  if (!view_) {
    return nullptr;
  }

  CALayer* main_layer = [view_ layer];
  if ([main_layer isKindOfClass:[CAMetalLayer class]]) {
    return static_cast<CAMetalLayer*>(main_layer);
  }
  if (main_layer) {
    for (CALayer* sublayer in main_layer.sublayers) {
      if ([sublayer isKindOfClass:[CAMetalLayer class]]) {
        return static_cast<CAMetalLayer*>(sublayer);
      }
    }
  }

  if (!main_layer) {
    [view_ setWantsLayer:YES];
    main_layer = [view_ layer];
  }
  if (!main_layer) {
    return nullptr;
  }

  CAMetalLayer* metal_layer = [CAMetalLayer layer];
  metal_layer.framebufferOnly = YES;
  metal_layer.opaque = YES;
  metal_layer.frame = main_layer.bounds;
  metal_layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
  [main_layer addSublayer:metal_layer];
  return metal_layer;
}

std::unique_ptr<Surface> CreateMacNSViewSurface(SDL_Window* window) {
  if (!window) {
    return nullptr;
  }

  SDL_PropertiesID properties = SDL_GetWindowProperties(window);
  NSWindow* ns_window = static_cast<NSWindow*>(
      SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr));
  if (!ns_window) {
    return nullptr;
  }

  NSView* view = ns_window.contentView;
  if (!view) {
    return nullptr;
  }
  return std::make_unique<MacNSViewSurface>(view);
}

}  // namespace ui
}  // namespace rex
