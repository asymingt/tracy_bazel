// Checks that the imgui module carries Tracy's configuration and patches, and
// that the GLFW + OpenGL3 backends link. No window is opened.

#include <cstdio>

// The include spellings Tracy's sources use (relative to the imgui source root).
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_opengl3_loader.h>
#include <misc/freetype/imgui_freetype.h>

// imgui-no-default-font.patch (via imconfig.h)
#ifndef IMGUI_DISABLE_DEFAULT_FONT
#error "imconfig.h should define IMGUI_DISABLE_DEFAULT_FONT"
#endif
// imgui-loader.patch
#ifndef GL_LINEAR_MIPMAP_LINEAR
#error "patched loader should define GL_LINEAR_MIPMAP_LINEAR"
#endif
#ifndef glCompressedTexImage2D
#error "patched loader should expose glCompressedTexImage2D"
#endif
#ifndef IMGUI_USE_WCHAR32
#error "IMGUI_USE_WCHAR32 should propagate"
#endif

int main() {
  IMGUI_CHECKVERSION();
  static_assert(sizeof(ImWchar) == 4, "IMGUI_USE_WCHAR32");
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(640, 480);
  const ImFontLoader* loader = ImGuiFreeType::GetFontLoader();
  std::printf("imgui %s, freetype loader %s\n", ImGui::GetVersion(), loader ? "present" : "missing");
  // Referencing the backends forces them to link (without initialising them).
  volatile auto glfw_init = &ImGui_ImplGlfw_InitForOpenGL;
  volatile auto gl_init = &ImGui_ImplOpenGL3_Init;
  ImGui::DestroyContext();
  return (loader != nullptr && glfw_init != nullptr && gl_init != nullptr) ? 0 : 1;
}
