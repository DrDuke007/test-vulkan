#pragma once
#include "base/types.hpp"
#include "platform/window.hpp"

class App
{
  public:
    App();
    ~App();

    void run();

  private:
    void update();

    platform::Window window;

    bool is_minimized;
};
