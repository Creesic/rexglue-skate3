
// fm2 - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <array>
#include <filesystem>

#include <rex/rex_app.h>

class Fm2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Fm2App>(new Fm2App(ctx, "fm2",
        PPCImageConfig));
  }

  // Override virtual hooks for customization:
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // void OnShutdown() override {}
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (!paths.game_data_root.empty()) {
      return;
    }

    const auto exe_dir = paths.config_path.parent_path();
    const auto cwd = std::filesystem::current_path();
    const std::array<std::filesystem::path, 7> candidates = {
        cwd / "assets",
        cwd / "FM2" / "assets",
        exe_dir / "assets",
        exe_dir / ".." / ".." / ".." / ".." / "assets",
        exe_dir / ".." / ".." / ".." / ".." / ".." / "FM2" / "assets",
        exe_dir / ".." / ".." / ".." / "assets",
        exe_dir / ".." / ".." / ".." / ".." / ".." / "assets",
    };

    for (const auto& candidate : candidates) {
      if (std::filesystem::is_regular_file(candidate / "default.xex")) {
        paths.game_data_root = candidate;
        return;
      }
    }
  }
};
