#ifndef LIMINAL_RENDERER_SDL_FRONTEND_H
#define LIMINAL_RENDERER_SDL_FRONTEND_H

#include <stddef.h>
#include <string>

#include "animated_view.h"
#include "game_state.h"
#include "turn_runner.h"

namespace liminal {

struct SdlFrontendConfig {
    RenderConfig render_config;
    HeadlessTurnConfig turn_config;
    AnimatedViewConfig animated_view_config;
    int logical_width;
    int logical_height;
    int window_width;
    int window_height;
    int automated_smoke_test_duration_ms;
    std::string automated_smoke_test_command;

    SdlFrontendConfig()
        : logical_width(1000)
        , logical_height(800)
        , window_width(1400)
        , window_height(1120)
        , automated_smoke_test_duration_ms(0)
    {
    }
};

bool RunSdlFrontend(
    const SdlFrontendConfig& config,
    const SessionState& initial_session_state,
    SessionState* final_session_state,
    char* error_buffer,
    size_t error_buffer_size);

}  // namespace liminal

#endif
