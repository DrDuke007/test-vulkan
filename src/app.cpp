#include "app.hpp"
#include <variant>

App::App()
{
    platform::Window::create(window, 1280, 720, "Multi viewport");
    renderer = Renderer::create(&window);
}

App::~App()
{
    renderer.destroy();
}


void App::run()
{
    while (!window.should_close())
    {
        window.poll_events();

        Option<platform::event::Resize> last_resize;
        for (auto &event : window.events)
        {
            if (std::holds_alternative<platform::event::Resize>(event))
            {
                auto resize = std::get<platform::event::Resize>(event);
                last_resize = resize;
            }
            else if (std::holds_alternative<platform::event::MouseMove>(event))
            {
                auto move = std::get<platform::event::MouseMove>(event);
                (void)(move);
                this->is_minimized = false;
            }
        }

        if (last_resize)
        {
            auto resize = *last_resize;
            if (resize.width > 0 && resize.height > 0)
            {
                // handle resize
            }
            if (window.minimized)
            {
                this->is_minimized = true;
            }
        }

        window.events.clear();

        if (is_minimized)
        {
            continue;
        }

        renderer.update();
    }
}
