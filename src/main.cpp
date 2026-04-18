#include "app/Application.hpp"

#include <SDL3/SDL.h>

int main() {
    app::Application app;
    if (!app.init()) {
        SDL_Log("VulcanCAD initialization failed: %s", app.last_error().c_str());
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "VulcanCAD - Startup Error",
            app.last_error().c_str(),
            nullptr);
        app.shutdown();
        return 1;
    }

    app.run();
    app.shutdown();
    return 0;
}
