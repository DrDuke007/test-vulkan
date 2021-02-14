#pragma once
#include "base/types.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"

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
};
