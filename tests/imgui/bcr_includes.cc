// BCR imgui module include spellings (<imgui/...>).

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/misc/freetype/imgui_freetype.h>

bool BcrIncludesWork() { return ImGuiFreeType::GetFontLoader() != nullptr && IMGUI_VERSION_NUM >= 19300; }
