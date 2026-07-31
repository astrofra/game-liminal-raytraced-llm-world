#include "sdl_frontend.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include "renderer.h"
#include "scene_compiler.h"

namespace liminal {

namespace {

struct WorkerSharedState {
    std::mutex mutex;
    std::atomic<bool> stop_requested;
    bool busy;
    bool result_ready;
    bool error_ready;
    std::string status_text;
    std::string live_phase_label;
    std::string live_text;
    std::string error_text;
    SessionState session_state;
    HeadlessTurnResult turn_result;
    std::vector<unsigned char> pixels;

    WorkerSharedState()
        : stop_requested(false)
        , busy(false)
        , result_ready(false)
        , error_ready(false)
    {
    }
};

struct InputWindow {
    std::string text;
    size_t cursor_column;

    InputWindow()
        : cursor_column(0)
    {
    }
};

static void SetError(char* error_buffer, size_t error_buffer_size, const char* message)
{
    if (!error_buffer || error_buffer_size == 0) {
        return;
    }

    snprintf(error_buffer, error_buffer_size, "%s", message ? message : "Unknown error.");
}

static void SetSdlError(char* error_buffer, size_t error_buffer_size, const char* prefix)
{
    const char* sdl_error = SDL_GetError();
    if (!sdl_error || !sdl_error[0]) {
        sdl_error = "Unknown SDL error.";
    }

    if (!error_buffer || error_buffer_size == 0) {
        return;
    }

    snprintf(error_buffer, error_buffer_size, "%s: %s", prefix ? prefix : "SDL error", sdl_error);
}

static const char* StreamPhaseLabel(HeadlessTurnStreamPhase phase)
{
    switch (phase) {
        case kHeadlessTurnStreamPrimaryResponse:
            return "turn-json";
        case kHeadlessTurnStreamRepairResponse:
            return "repair-json";
        case kHeadlessTurnStreamSceneProgram:
            return "scene-program";
        default:
            return "stream";
    }
}

static const char* StreamPhaseStatus(HeadlessTurnStreamPhase phase)
{
    switch (phase) {
        case kHeadlessTurnStreamPrimaryResponse:
            return "Streaming turn response...";
        case kHeadlessTurnStreamRepairResponse:
            return "Repairing malformed turn JSON...";
        case kHeadlessTurnStreamSceneProgram:
            return "Streaming candidate scene program...";
        default:
            return "Streaming...";
    }
}

static bool IsUtf8Continuation(unsigned char value)
{
    return (value & 0xc0u) == 0x80u;
}

static size_t AdvanceUtf8(const std::string& text, size_t index)
{
    if (index >= text.size()) {
        return text.size();
    }

    const unsigned char lead = static_cast<unsigned char>(text[index]);
    size_t advance = 1;
    if ((lead & 0x80u) == 0x00u) {
        advance = 1;
    } else if ((lead & 0xe0u) == 0xc0u) {
        advance = 2;
    } else if ((lead & 0xf0u) == 0xe0u) {
        advance = 3;
    } else if ((lead & 0xf8u) == 0xf0u) {
        advance = 4;
    }

    return std::min(text.size(), index + advance);
}

static size_t RetreatUtf8(const std::string& text, size_t index)
{
    if (index == 0 || text.empty()) {
        return 0;
    }

    size_t cursor = std::min(index, text.size()) - 1;
    while (cursor > 0 && IsUtf8Continuation(static_cast<unsigned char>(text[cursor]))) {
        --cursor;
    }
    return cursor;
}

static char ToDebugGlyph(const std::string& codepoint)
{
    if (codepoint.empty()) {
        return ' ';
    }

    const unsigned char value = static_cast<unsigned char>(codepoint[0]);
    if (codepoint.size() == 1 && value >= 32u && value <= 126u) {
        return static_cast<char>(value);
    }
    if (codepoint.size() == 1 && (value == '\n' || value == '\t')) {
        return static_cast<char>(value);
    }
    return '?';
}

static std::vector<std::string> SplitUtf8(const std::string& text)
{
    std::vector<std::string> parts;
    for (size_t index = 0; index < text.size();) {
        const size_t next = AdvanceUtf8(text, index);
        parts.push_back(text.substr(index, next - index));
        index = next;
    }
    return parts;
}

static std::string SanitizeDebugText(const std::string& text)
{
    const std::vector<std::string> codepoints = SplitUtf8(text);
    std::string output;
    output.reserve(codepoints.size());
    for (size_t index = 0; index < codepoints.size(); ++index) {
        const char glyph = ToDebugGlyph(codepoints[index]);
        if (glyph == '\t') {
            output.append("    ");
        } else {
            output.push_back(glyph);
        }
    }
    return output;
}

static std::string TrimAsciiSpaces(const std::string& text)
{
    size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t' || text[start] == '\r')) {
        ++start;
    }

    size_t end = text.size();
    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
        --end;
    }

    return text.substr(start, end - start);
}

static void AppendWrappedParagraph(const std::string& paragraph, size_t max_chars, std::vector<std::string>* lines)
{
    if (!lines) {
        return;
    }

    if (max_chars == 0) {
        lines->push_back(std::string());
        return;
    }

    std::string remaining = paragraph;
    if (remaining.empty()) {
        lines->push_back(std::string());
        return;
    }

    while (!remaining.empty()) {
        if (remaining.size() <= max_chars) {
            lines->push_back(remaining);
            break;
        }

        size_t split = remaining.rfind(' ', max_chars);
        if (split == std::string::npos || split == 0) {
            split = max_chars;
        }

        lines->push_back(remaining.substr(0, split));
        remaining = TrimAsciiSpaces(remaining.substr(split));
    }
}

static void AppendWrappedText(const std::string& text, size_t max_chars, std::vector<std::string>* lines)
{
    if (!lines) {
        return;
    }

    const std::string sanitized = SanitizeDebugText(text);
    size_t start = 0;
    while (start <= sanitized.size()) {
        const size_t end = sanitized.find('\n', start);
        const size_t length = end == std::string::npos ? sanitized.size() - start : end - start;
        AppendWrappedParagraph(sanitized.substr(start, length), max_chars, lines);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

static void BuildInputWindow(
    const std::string& input_text,
    size_t cursor_byte,
    size_t max_chars,
    InputWindow* input_window)
{
    if (!input_window) {
        return;
    }

    input_window->text.clear();
    input_window->cursor_column = 0;

    const std::vector<std::string> glyphs = SplitUtf8(input_text);
    size_t cursor_codepoint = 0;
    for (size_t index = 0, byte_offset = 0; index < glyphs.size() && byte_offset < cursor_byte; ++index) {
        byte_offset += glyphs[index].size();
        ++cursor_codepoint;
    }

    size_t start = 0;
    if (glyphs.size() > max_chars && cursor_codepoint >= max_chars) {
        start = cursor_codepoint - max_chars + 1;
    }

    const size_t end = std::min(glyphs.size(), start + max_chars);
    for (size_t index = start; index < end; ++index) {
        const char glyph = ToDebugGlyph(glyphs[index]);
        input_window->text.push_back(glyph == '\n' ? ' ' : glyph);
    }

    if (cursor_codepoint < start) {
        input_window->cursor_column = 0;
    } else if (cursor_codepoint > end) {
        input_window->cursor_column = end - start;
    } else {
        input_window->cursor_column = cursor_codepoint - start;
    }
}

static std::string TrimCommandText(const std::string& text)
{
    return TrimAsciiSpaces(text);
}

static void DrawPanel(SDL_Renderer* renderer, const SDL_FRect& rect, Uint8 fill, Uint8 border)
{
    SDL_SetRenderDrawColor(renderer, fill, fill, fill, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border, border, border, 255);
    SDL_RenderRect(renderer, &rect);
}

static void UploadSceneTexture(
    const std::vector<unsigned char>& grayscale_pixels,
    int width,
    int height,
    std::vector<unsigned char>* rgba_pixels,
    SDL_Texture* texture)
{
    if (!rgba_pixels || !texture || width <= 0 || height <= 0) {
        return;
    }

    rgba_pixels->resize(static_cast<size_t>(width * height * 4));
    for (size_t index = 0; index < grayscale_pixels.size(); ++index) {
        const unsigned char value = grayscale_pixels[index];
        const size_t dst = index * 4;
        (*rgba_pixels)[dst + 0] = value;
        (*rgba_pixels)[dst + 1] = value;
        (*rgba_pixels)[dst + 2] = value;
        (*rgba_pixels)[dst + 3] = 255u;
    }

    SDL_UpdateTexture(texture, 0, &(*rgba_pixels)[0], width * 4);
}

static bool OnHeadlessTurnStream(
    HeadlessTurnStreamPhase phase,
    const char* accumulated_text,
    const char*,
    void* user_data)
{
    WorkerSharedState* shared_state = static_cast<WorkerSharedState*>(user_data);
    if (!shared_state) {
        return true;
    }

    if (shared_state->stop_requested.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(shared_state->mutex);
    shared_state->status_text = StreamPhaseStatus(phase);
    shared_state->live_phase_label = StreamPhaseLabel(phase);
    shared_state->live_text = accumulated_text ? accumulated_text : "";
    return !shared_state->stop_requested.load();
}

static void FinalizeWorkerFailure(WorkerSharedState* shared_state, const char* message)
{
    if (!shared_state) {
        return;
    }

    std::lock_guard<std::mutex> lock(shared_state->mutex);
    shared_state->busy = false;
    shared_state->result_ready = false;
    shared_state->error_ready = true;
    shared_state->status_text = "Turn failed.";
    shared_state->error_text = message ? message : "Unknown error.";
}

static void RunTurnWorker(
    SdlFrontendConfig config,
    SessionState initial_session_state,
    std::string player_command,
    WorkerSharedState* shared_state)
{
    if (!shared_state) {
        return;
    }

    shared_state->stop_requested.store(false);

    HeadlessTurnConfig turn_config = config.turn_config;
    turn_config.stream_callback = OnHeadlessTurnStream;
    turn_config.stream_user_data = shared_state;

    {
        std::lock_guard<std::mutex> lock(shared_state->mutex);
        shared_state->status_text = "Preparing turn...";
        shared_state->live_phase_label.clear();
        shared_state->live_text.clear();
        shared_state->error_text.clear();
    }

    char error_buffer[512];
    memset(error_buffer, 0, sizeof(error_buffer));

    HeadlessTurnResult turn_result;
    if (!RunHeadlessTurnFromState(
            initial_session_state,
            player_command.c_str(),
            turn_config,
            &turn_result,
            error_buffer,
            sizeof(error_buffer))) {
        if (shared_state->stop_requested.load()) {
            FinalizeWorkerFailure(shared_state, "Turn cancelled.");
        } else {
            FinalizeWorkerFailure(shared_state, error_buffer[0] ? error_buffer : "Headless turn failed.");
        }
        return;
    }

    if (shared_state->stop_requested.load()) {
        FinalizeWorkerFailure(shared_state, "Turn cancelled.");
        return;
    }

    SessionState updated_session_state = initial_session_state;
    UpdateSessionStateFromTurn(player_command.c_str(), turn_result, &updated_session_state);

    {
        std::lock_guard<std::mutex> lock(shared_state->mutex);
        shared_state->status_text = "Raytracing scene...";
        shared_state->live_phase_label.clear();
        shared_state->live_text.clear();
    }

    std::vector<unsigned char> pixels;
    if (!RenderSceneToPixels(turn_result.rendered_scene, config.render_config, &pixels)) {
        FinalizeWorkerFailure(shared_state, "Rendering failed.");
        return;
    }

    if (shared_state->stop_requested.load()) {
        FinalizeWorkerFailure(shared_state, "Turn cancelled.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shared_state->mutex);
        shared_state->busy = false;
        shared_state->result_ready = true;
        shared_state->error_ready = false;
        shared_state->status_text = "Turn complete.";
        shared_state->live_phase_label.clear();
        shared_state->live_text.clear();
        shared_state->session_state = updated_session_state;
        shared_state->turn_result = turn_result;
        shared_state->pixels.swap(pixels);
    }
}

static std::string BuildStatusLine(
    const SessionState& session_state,
    const std::string& worker_status,
    const HeadlessTurnResult& last_turn_result,
    bool have_last_turn,
    bool busy)
{
    char buffer[512];
    if (have_last_turn) {
        snprintf(
            buffer,
            sizeof(buffer),
            "loc=%s turn=%d alert=%d status=%s prompt=%d gen=%d time=%.0f ms%s",
            LocationIdToString(session_state.spatial_state.location_id),
            session_state.hard_state.turn_number,
            session_state.hard_state.alert_level,
            worker_status.c_str(),
            last_turn_result.prompt_tokens,
            last_turn_result.generated_tokens,
            last_turn_result.inference_time_ms,
            busy ? " [busy]" : "");
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "loc=%s turn=%d alert=%d status=%s%s",
            LocationIdToString(session_state.spatial_state.location_id),
            session_state.hard_state.turn_number,
            session_state.hard_state.alert_level,
            worker_status.c_str(),
            busy ? " [busy]" : "");
    }
    return buffer;
}

static void BuildTranscriptLines(
    const SessionState& session_state,
    const std::string& live_phase_label,
    const std::string& live_text,
    const std::string& ui_message,
    size_t max_chars,
    std::vector<std::string>* lines)
{
    if (!lines) {
        return;
    }

    lines->clear();

    for (size_t index = 0; index < session_state.history.size(); ++index) {
        const SessionTurnRecord& record = session_state.history[index];
        AppendWrappedText(std::string("> ") + record.player_command, max_chars, lines);
        if (!record.narration.empty()) {
            AppendWrappedText(record.narration, max_chars, lines);
        }
        if (!record.clarification.empty()) {
            AppendWrappedText(std::string("[") + record.clarification + "]", max_chars, lines);
        }
    }

    if (!ui_message.empty()) {
        AppendWrappedText(std::string("[") + ui_message + "]", max_chars, lines);
    }

    if (!live_text.empty()) {
        AppendWrappedText(std::string("[live ") + live_phase_label + "]", max_chars, lines);
        AppendWrappedText(live_text, max_chars, lines);
    }
}

static void DrawConsoleText(
    SDL_Renderer* renderer,
    const SDL_FRect& text_rect,
    const std::vector<std::string>& lines,
    size_t line_capacity)
{
    if (!renderer || line_capacity == 0) {
        return;
    }

    const size_t start = lines.size() > line_capacity ? lines.size() - line_capacity : 0;
    float y = text_rect.y + 8.0f;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (size_t index = start; index < lines.size(); ++index) {
        SDL_RenderDebugText(renderer, text_rect.x + 8.0f, y, lines[index].c_str());
        y += static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
    }
}

}  // namespace

bool RunSdlFrontend(
    const SdlFrontendConfig& config,
    const SessionState& initial_session_state,
    SessionState* final_session_state,
    char* error_buffer,
    size_t error_buffer_size)
{
    SessionState current_session_state = initial_session_state;
    std::vector<std::string> command_history;
    for (size_t index = 0; index < current_session_state.history.size(); ++index) {
        if (!current_session_state.history[index].player_command.empty()) {
            command_history.push_back(current_session_state.history[index].player_command);
        }
    }

    char local_error[512];
    memset(local_error, 0, sizeof(local_error));

    Scene initial_scene;
    if (!CompileSpatialStateToScene(current_session_state.spatial_state, &initial_scene, local_error, sizeof(local_error))) {
        SetError(error_buffer, error_buffer_size, local_error[0] ? local_error : "Cannot compile initial scene.");
        return false;
    }

    std::vector<unsigned char> grayscale_pixels;
    if (!RenderSceneToPixels(initial_scene, config.render_config, &grayscale_pixels)) {
        SetError(error_buffer, error_buffer_size, "Cannot render initial scene.");
        return false;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SetSdlError(error_buffer, error_buffer_size, "SDL_Init failed");
        return false;
    }

    SDL_Window* window = 0;
    SDL_Renderer* renderer = 0;
    SDL_Texture* scene_texture = 0;
    bool success = false;

    if (!SDL_CreateWindowAndRenderer(
            "Le desert des tokens",
            config.window_width,
            config.window_height,
            SDL_WINDOW_RESIZABLE,
            &window,
            &renderer)) {
        SetSdlError(error_buffer, error_buffer_size, "SDL_CreateWindowAndRenderer failed");
        SDL_Quit();
        return false;
    }

    if (!SDL_SetRenderLogicalPresentation(
            renderer,
            config.logical_width,
            config.logical_height,
            SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        SetSdlError(error_buffer, error_buffer_size, "SDL_SetRenderLogicalPresentation failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    if (!SDL_StartTextInput(window)) {
        SetSdlError(error_buffer, error_buffer_size, "SDL_StartTextInput failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    scene_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        config.render_config.width,
        config.render_config.height);
    if (!scene_texture) {
        SetSdlError(error_buffer, error_buffer_size, "SDL_CreateTexture failed");
        SDL_StopTextInput(window);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    std::vector<unsigned char> texture_rgba_pixels;
    UploadSceneTexture(
        grayscale_pixels,
        config.render_config.width,
        config.render_config.height,
        &texture_rgba_pixels,
        scene_texture);

    const SDL_FRect scene_frame = {40.0f, 28.0f, 920.0f, 460.0f};
    const SDL_FRect scene_rect = {60.0f, 48.0f, 880.0f, 440.0f};
    const SDL_FRect console_rect = {40.0f, 516.0f, 920.0f, 208.0f};
    const SDL_FRect input_rect = {40.0f, 740.0f, 920.0f, 32.0f};
    const size_t console_max_chars = static_cast<size_t>(std::max(1, static_cast<int>(console_rect.w / 8.0f) - 2));
    const size_t input_max_chars = static_cast<size_t>(std::max(1, static_cast<int>((input_rect.w - 24.0f) / 8.0f)));
    const size_t console_line_capacity = static_cast<size_t>(std::max(1, static_cast<int>((console_rect.h - 16.0f) / 8.0f)));

    WorkerSharedState worker_shared_state;
    std::thread worker_thread;
    bool worker_joined = true;
    bool running = true;
    bool have_last_turn = false;
    HeadlessTurnResult last_turn_result;
    std::string input_text;
    size_t input_cursor = 0;
    int history_index = -1;
    std::string history_draft;
    std::string ui_message = "Ready. Press Enter to send a command.";
    std::string worker_status = "idle";
    std::string live_phase_label;
    std::string live_text;
    std::vector<std::string> transcript_lines;
    bool worker_busy = false;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                worker_shared_state.stop_requested.store(true);
                running = false;
                break;
            }

            if (event.type == SDL_EVENT_TEXT_INPUT) {
                const char* inserted = event.text.text ? event.text.text : "";
                if (inserted[0]) {
                    input_text.insert(input_cursor, inserted);
                    input_cursor += strlen(inserted);
                    history_index = -1;
                }
                continue;
            }

            if (event.type != SDL_EVENT_KEY_DOWN || !event.key.down || event.key.repeat) {
                continue;
            }

            const SDL_Keycode key = event.key.key;
            if (key == SDLK_LEFT) {
                input_cursor = RetreatUtf8(input_text, input_cursor);
                continue;
            }
            if (key == SDLK_RIGHT) {
                input_cursor = AdvanceUtf8(input_text, input_cursor);
                continue;
            }
            if (key == SDLK_HOME) {
                input_cursor = 0;
                continue;
            }
            if (key == SDLK_END) {
                input_cursor = input_text.size();
                continue;
            }
            if (key == SDLK_BACKSPACE) {
                if (input_cursor > 0) {
                    const size_t previous = RetreatUtf8(input_text, input_cursor);
                    input_text.erase(previous, input_cursor - previous);
                    input_cursor = previous;
                }
                history_index = -1;
                continue;
            }
            if (key == SDLK_DELETE) {
                if (input_cursor < input_text.size()) {
                    const size_t next = AdvanceUtf8(input_text, input_cursor);
                    input_text.erase(input_cursor, next - input_cursor);
                }
                history_index = -1;
                continue;
            }
            if (key == SDLK_UP) {
                if (!command_history.empty()) {
                    if (history_index < 0) {
                        history_draft = input_text;
                        history_index = static_cast<int>(command_history.size()) - 1;
                    } else if (history_index > 0) {
                        --history_index;
                    }
                    input_text = command_history[static_cast<size_t>(history_index)];
                    input_cursor = input_text.size();
                }
                continue;
            }
            if (key == SDLK_DOWN) {
                if (history_index >= 0) {
                    if (history_index + 1 < static_cast<int>(command_history.size())) {
                        ++history_index;
                        input_text = command_history[static_cast<size_t>(history_index)];
                    } else {
                        history_index = -1;
                        input_text = history_draft;
                    }
                    input_cursor = input_text.size();
                }
                continue;
            }
            if (key == SDLK_ESCAPE) {
                bool busy = false;
                {
                    std::lock_guard<std::mutex> lock(worker_shared_state.mutex);
                    busy = worker_shared_state.busy;
                }
                if (busy) {
                    worker_shared_state.stop_requested.store(true);
                    ui_message = "Cancellation requested...";
                } else {
                    input_text.clear();
                    input_cursor = 0;
                    history_index = -1;
                }
                continue;
            }
            if (key == SDLK_RETURN) {
                const std::string command = TrimCommandText(input_text);
                if (command.empty()) {
                    ui_message = "Empty command ignored.";
                    continue;
                }

                bool busy = false;
                {
                    std::lock_guard<std::mutex> lock(worker_shared_state.mutex);
                    busy = worker_shared_state.busy;
                }
                if (busy) {
                    ui_message = "A turn is already running.";
                    continue;
                }

                if (worker_thread.joinable()) {
                    worker_thread.join();
                    worker_joined = true;
                }

                if (command_history.empty() || command_history.back() != command) {
                    command_history.push_back(command);
                }

                input_text.clear();
                input_cursor = 0;
                history_index = -1;
                history_draft.clear();

                {
                    std::lock_guard<std::mutex> lock(worker_shared_state.mutex);
                    worker_shared_state.busy = true;
                    worker_shared_state.result_ready = false;
                    worker_shared_state.error_ready = false;
                    worker_shared_state.status_text = "Preparing turn...";
                    worker_shared_state.live_phase_label.clear();
                    worker_shared_state.live_text.clear();
                    worker_shared_state.error_text.clear();
                }
                worker_shared_state.stop_requested.store(false);
                worker_thread = std::thread(
                    RunTurnWorker,
                    config,
                    current_session_state,
                    command,
                    &worker_shared_state);
                worker_joined = false;
                ui_message = std::string("Command queued: ") + command;
                continue;
            }
        }

        bool turn_completed = false;
        bool turn_failed = false;
        std::string failure_text;

        {
            std::lock_guard<std::mutex> lock(worker_shared_state.mutex);
            worker_busy = worker_shared_state.busy;
            worker_status = worker_shared_state.status_text.empty() ? "idle" : worker_shared_state.status_text;
            live_phase_label = worker_shared_state.live_phase_label;
            live_text = worker_shared_state.live_text;

            if (worker_shared_state.result_ready) {
                current_session_state = worker_shared_state.session_state;
                last_turn_result = worker_shared_state.turn_result;
                have_last_turn = true;
                grayscale_pixels = worker_shared_state.pixels;
                UploadSceneTexture(
                    grayscale_pixels,
                    config.render_config.width,
                    config.render_config.height,
                    &texture_rgba_pixels,
                    scene_texture);
                worker_shared_state.result_ready = false;
                turn_completed = true;
                ui_message = last_turn_result.turn_result.narration.empty()
                    ? "Turn complete."
                    : last_turn_result.turn_result.narration;
            } else if (worker_shared_state.error_ready) {
                failure_text = worker_shared_state.error_text;
                worker_shared_state.error_ready = false;
                turn_failed = true;
                ui_message = failure_text;
            }
        }

        if (!worker_joined && !worker_busy && worker_thread.joinable()) {
            worker_thread.join();
            worker_joined = true;
        }

        if (turn_completed) {
            live_phase_label.clear();
            live_text.clear();
        } else if (turn_failed) {
            live_phase_label.clear();
            live_text.clear();
        }

        const std::string status_line = BuildStatusLine(
            current_session_state,
            worker_status.empty() ? "idle" : worker_status,
            last_turn_result,
            have_last_turn,
            worker_busy);

        BuildTranscriptLines(current_session_state, live_phase_label, live_text, ui_message, console_max_chars, &transcript_lines);

        InputWindow input_window;
        BuildInputWindow(input_text, input_cursor, input_max_chars, &input_window);

        SDL_SetRenderDrawColor(renderer, 198, 198, 198, 255);
        SDL_RenderClear(renderer);

        DrawPanel(renderer, scene_frame, 236, 0);
        DrawPanel(renderer, console_rect, 236, 0);
        DrawPanel(renderer, input_rect, 250, 0);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDebugText(renderer, 48.0f, 8.0f, "LE DESERT DES TOKENS");
        SDL_RenderDebugText(renderer, 48.0f, 500.0f, status_line.c_str());

        SDL_RenderTexture(renderer, scene_texture, 0, &scene_rect);
        DrawConsoleText(renderer, console_rect, transcript_lines, console_line_capacity);

        SDL_RenderDebugText(renderer, input_rect.x + 8.0f, input_rect.y + 12.0f, ">");
        SDL_RenderDebugText(renderer, input_rect.x + 24.0f, input_rect.y + 12.0f, input_window.text.c_str());
        const float cursor_x =
            input_rect.x + 24.0f + static_cast<float>(input_window.cursor_column * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
        SDL_RenderLine(
            renderer,
            cursor_x,
            input_rect.y + 10.0f,
            cursor_x,
            input_rect.y + 22.0f);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    worker_shared_state.stop_requested.store(true);
    if (worker_thread.joinable()) {
        worker_thread.join();
    }

    if (final_session_state) {
        *final_session_state = current_session_state;
    }

    SDL_DestroyTexture(scene_texture);
    SDL_StopTextInput(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    success = true;
    return success;
}

}  // namespace liminal
