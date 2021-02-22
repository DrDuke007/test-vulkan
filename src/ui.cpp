#include "ui.hpp"

#include "base/types.hpp"
#include "base/algorithms.hpp"
#include "inputs.hpp"
#include "platform/window.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace UI
{

void Context::create(Context & /*ctx*/)
{
    // Init context
    ImGui::CreateContext();

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
    io.BackendPlatformName = "custom";
}

static platform::Cursor cursor_from_imgui()
{
    ImGuiIO &io                   = ImGui::GetIO();
    ImGuiMouseCursor imgui_cursor = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    platform::Cursor cursor       = platform::Cursor::None;
    switch (imgui_cursor)
    {
        case ImGuiMouseCursor_Arrow:
            cursor = platform::Cursor::Arrow;
            break;
        case ImGuiMouseCursor_TextInput:
            cursor = platform::Cursor::TextInput;
            break;
        case ImGuiMouseCursor_ResizeAll:
            cursor = platform::Cursor::ResizeAll;
            break;
        case ImGuiMouseCursor_ResizeEW:
            cursor = platform::Cursor::ResizeEW;
            break;
        case ImGuiMouseCursor_ResizeNS:
            cursor = platform::Cursor::ResizeNS;
            break;
        case ImGuiMouseCursor_ResizeNESW:
            cursor = platform::Cursor::ResizeNESW;
            break;
        case ImGuiMouseCursor_ResizeNWSE:
            cursor = platform::Cursor::ResizeNWSE;
            break;
        case ImGuiMouseCursor_Hand:
            cursor = platform::Cursor::Hand;
            break;
        case ImGuiMouseCursor_NotAllowed:
            cursor = platform::Cursor::NotAllowed;
            break;
    }
    return cursor;
}

void Context::on_mouse_movement(platform::Window &window, double /*xpos*/, double /*ypos*/)
{
    auto cursor = cursor_from_imgui();
    window.set_cursor(cursor);
}

void Context::start_frame(platform::Window &window, Inputs &inputs)
{
    ImGuiIO &io                  = ImGui::GetIO();
    io.DisplaySize.x             = window.width;
    io.DisplaySize.y             = window.height;
    io.DisplayFramebufferScale.x = window.get_dpi_scale().x;
    io.DisplayFramebufferScale.y = window.get_dpi_scale().y;

    io.MousePos = window.mouse_position;

    static_assert(to_underlying(MouseButton::Count) == 5);
    for (uint i = 0; i < 5; i++)
    {
        io.MouseDown[i] = inputs.is_pressed(static_cast<MouseButton>(i));
    }

    auto cursor        = cursor_from_imgui();
    static auto s_last = cursor;
    if (s_last != cursor)
    {
        window.set_cursor(cursor);
        s_last = cursor;
    }

    // NewFrame() has to be called before giving the inputs to imgui
    ImGui::NewFrame();
}

void Context::destroy() { ImGui::DestroyContext(); }

void Context::display_ui()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Windows"))
        {
            for (auto &[_, window] : windows)
            {
                ImGui::MenuItem(window.name.c_str(), nullptr, &window.is_visible);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

bool Context::begin_window(std::string_view name, bool is_visible, ImGuiWindowFlags /*flags*/)
{
    if (windows.count(name))
    {
        auto &window = windows.at(name);
        if (window.is_visible)
        {
            ImGui::Begin(name.data(), &window.is_visible, 0);
            return true;
        }
    }
    else
    {
        windows[name] = {.name = std::string(name), .is_visible = is_visible};
    }

    return false;
}

void Context::end_window() { ImGui::End(); }
} // namespace my_app
