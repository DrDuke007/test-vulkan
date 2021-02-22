#pragma once
#include "base/types.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "inputs.hpp"
#include "ui.hpp"

class App
{
  public:
    App();
    ~App();

    void run();

  private:
    void update();

    platform::Window window;
    Renderer renderer;
    bool is_minimized;
    UI::Context ui;
    Inputs inputs;
};
