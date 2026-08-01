#include "sdl_frontend.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "renderer.h"
#include "scene_compiler.h"

namespace liminal {

namespace {

enum WorkerActivity {
    kWorkerActivityIdle = 0,
    kWorkerActivityLlm,
    kWorkerActivityRenderer,
    kWorkerActivityComplete,
    kWorkerActivityFailed,
};

struct WorkerSharedState {
    std::mutex mutex;
    std::atomic<bool> stop_requested;
    bool busy;
    bool result_ready;
    bool error_ready;
    WorkerActivity activity;
    std::string status_text;
    std::string error_text;
    bool terminal_stream_open;
    std::string terminal_stream_phase_label;
    SessionState session_state;
    HeadlessTurnResult turn_result;
    std::vector<unsigned char> pixels;

    WorkerSharedState()
        : stop_requested(false)
        , busy(false)
        , result_ready(false)
        , error_ready(false)
        , activity(kWorkerActivityIdle)
        , terminal_stream_open(false)
    {
    }
};

struct InputWindow {
    std::string text;
    int cursor_x;

    InputWindow()
        : cursor_x(0)
    {
    }
};

struct UiTextSpan {
    std::string text;
    bool highlighted;

    UiTextSpan()
        : highlighted(false)
    {
    }
};

struct UiTextLine {
    std::vector<UiTextSpan> spans;
};

struct UiTextToken {
    std::string text;
    bool highlighted;
    bool whitespace;

    UiTextToken()
        : highlighted(false)
        , whitespace(false)
    {
    }
};

struct UiFonts {
    TTF_Font* regular;
    TTF_Font* highlight;
    int line_skip;
    int line_height;

    UiFonts()
        : regular(0)
        , highlight(0)
        , line_skip(20)
        , line_height(18)
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

static void SetTtfError(char* error_buffer, size_t error_buffer_size, const char* prefix)
{
    SetSdlError(error_buffer, error_buffer_size, prefix ? prefix : "SDL_ttf error");
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

static const char* ActivityLabel(WorkerActivity activity)
{
    switch (activity) {
        case kWorkerActivityLlm:
            return "llm";
        case kWorkerActivityRenderer:
            return "cpu";
        case kWorkerActivityComplete:
            return "done";
        case kWorkerActivityFailed:
            return "error";
        default:
            return "idle";
    }
}

static char SpinnerGlyph(Uint64 ticks_ms)
{
    static const char kSpinner[] = {'/', '-', '\\', '|'};
    return kSpinner[(ticks_ms / 120u) % 4u];
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

static TTF_Font* FontForSpan(const UiFonts& fonts, bool highlighted)
{
    return highlighted ? fonts.highlight : fonts.regular;
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

static bool IsWhitespaceOnly(const std::string& text)
{
    for (size_t index = 0; index < text.size(); ++index) {
        const char value = text[index];
        if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
            return false;
        }
    }
    return true;
}

static int MeasureTextWidth(TTF_Font* font, const std::string& text)
{
    if (!font || text.empty()) {
        return 0;
    }

    int width = 0;
    int height = 0;
    if (!TTF_GetStringSize(font, text.c_str(), text.size(), &width, &height)) {
        return 0;
    }
    return width;
}

static size_t MeasureFitLength(TTF_Font* font, const std::string& text, int max_width)
{
    if (!font || text.empty() || max_width <= 0) {
        return 0;
    }

    int measured_width = 0;
    size_t measured_length = 0;
    if (!TTF_MeasureString(font, text.c_str(), text.size(), max_width, &measured_width, &measured_length)) {
        return 0;
    }
    return measured_length;
}

static bool TryOpenFontCandidate(const char* path, float point_size, TTF_Font** font)
{
    if (!path || !path[0] || !font) {
        return false;
    }

    *font = TTF_OpenFont(path, point_size);
    return *font != 0;
}

static bool OpenFontWithFallbacks(const char* asset_path, float point_size, TTF_Font** font)
{
    if (!asset_path || !font) {
        return false;
    }

    if (TryOpenFontCandidate(asset_path, point_size, font)) {
        return true;
    }

    const char* base_path = SDL_GetBasePath();
    if (base_path) {
        const std::string base(base_path);

        const std::string candidates[] = {
            base + asset_path,
            base + "../" + asset_path,
            base + "../../" + asset_path,
        };
        for (size_t index = 0; index < sizeof(candidates) / sizeof(candidates[0]); ++index) {
            if (TryOpenFontCandidate(candidates[index].c_str(), point_size, font)) {
                return true;
            }
        }
    }

    return false;
}

static bool LoadUiFonts(UiFonts* fonts, char* error_buffer, size_t error_buffer_size)
{
    if (!fonts) {
        SetError(error_buffer, error_buffer_size, "Invalid UI font container.");
        return false;
    }

    if (!TTF_Init()) {
        SetTtfError(error_buffer, error_buffer_size, "TTF_Init failed");
        return false;
    }

    if (!OpenFontWithFallbacks("assets/fonts/Zilla_Slab/ZillaSlab-Regular.ttf", 18.0f, &fonts->regular)) {
        SetTtfError(error_buffer, error_buffer_size, "Failed to open Zilla Slab regular font");
        TTF_Quit();
        return false;
    }

    if (!OpenFontWithFallbacks(
            "assets/fonts/Zilla_Slab_Highlight/ZillaSlabHighlight-Regular.ttf",
            18.0f,
            &fonts->highlight)) {
        SetTtfError(error_buffer, error_buffer_size, "Failed to open Zilla Slab highlight font");
        TTF_CloseFont(fonts->regular);
        fonts->regular = 0;
        TTF_Quit();
        return false;
    }

    fonts->line_skip = std::max(TTF_GetFontLineSkip(fonts->regular), TTF_GetFontLineSkip(fonts->highlight));
    fonts->line_height = std::max(TTF_GetFontHeight(fonts->regular), TTF_GetFontHeight(fonts->highlight));
    if (fonts->line_skip <= 0) {
        fonts->line_skip = 24;
    }
    if (fonts->line_height <= 0) {
        fonts->line_height = fonts->line_skip;
    }
    return true;
}

static void DestroyUiFonts(UiFonts* fonts)
{
    if (!fonts) {
        return;
    }

    if (fonts->highlight) {
        TTF_CloseFont(fonts->highlight);
        fonts->highlight = 0;
    }
    if (fonts->regular) {
        TTF_CloseFont(fonts->regular);
        fonts->regular = 0;
    }
    if (TTF_WasInit()) {
        TTF_Quit();
    }
}

static void AppendSpanText(UiTextLine* line, const std::string& text, bool highlighted)
{
    if (!line || text.empty()) {
        return;
    }

    if (!line->spans.empty() && line->spans.back().highlighted == highlighted) {
        line->spans.back().text += text;
        return;
    }

    UiTextSpan span;
    span.text = text;
    span.highlighted = highlighted;
    line->spans.push_back(span);
}

static void ParseHighlightMarkup(const std::string& text, std::vector<UiTextSpan>* spans)
{
    if (!spans) {
        return;
    }

    spans->clear();

    size_t cursor = 0;
    while (cursor < text.size()) {
        const size_t open = text.find('*', cursor);
        if (open == std::string::npos) {
            if (cursor < text.size()) {
                UiTextSpan span;
                span.text = text.substr(cursor);
                span.highlighted = false;
                spans->push_back(span);
            }
            break;
        }

        if (open > cursor) {
            UiTextSpan span;
            span.text = text.substr(cursor, open - cursor);
            span.highlighted = false;
            spans->push_back(span);
        }

        const size_t close = text.find('*', open + 1);
        if (close == std::string::npos) {
            UiTextSpan span;
            span.text = text.substr(open);
            span.highlighted = false;
            spans->push_back(span);
            break;
        }

        if (close > open + 1) {
            UiTextSpan span;
            span.text = text.substr(open + 1, close - open - 1);
            span.highlighted = true;
            spans->push_back(span);
        }
        cursor = close + 1;
    }
}

static void TokenizeUiTextSpans(const std::vector<UiTextSpan>& spans, std::vector<UiTextToken>* tokens)
{
    if (!tokens) {
        return;
    }

    tokens->clear();
    for (size_t span_index = 0; span_index < spans.size(); ++span_index) {
        const UiTextSpan& span = spans[span_index];
        size_t cursor = 0;
        while (cursor < span.text.size()) {
            const char value = span.text[cursor];
            const bool whitespace = value == ' ' || value == '\t' || value == '\r';
            const size_t start = cursor;
            if (whitespace) {
                while (cursor < span.text.size()) {
                    const char next = span.text[cursor];
                    if (next != ' ' && next != '\t' && next != '\r') {
                        break;
                    }
                    ++cursor;
                }
            } else {
                while (cursor < span.text.size()) {
                    const char next = span.text[cursor];
                    if (next == ' ' || next == '\t' || next == '\r') {
                        break;
                    }
                    cursor = AdvanceUtf8(span.text, cursor);
                }
            }

            UiTextToken token;
            token.highlighted = span.highlighted;
            token.whitespace = whitespace;
            token.text = whitespace ? " " : span.text.substr(start, cursor - start);
            tokens->push_back(token);
        }
    }
}

static void FlushWrappedLine(UiTextLine* current_line, std::vector<UiTextLine>* lines, int* current_width)
{
    if (!current_line || !lines || !current_width) {
        return;
    }

    lines->push_back(*current_line);
    current_line->spans.clear();
    *current_width = 0;
}

static void AppendWrappedParagraph(
    const std::string& paragraph,
    const UiFonts& fonts,
    int max_width,
    std::vector<UiTextLine>* lines)
{
    if (!lines) {
        return;
    }

    if (max_width <= 0) {
        lines->push_back(UiTextLine());
        return;
    }

    std::vector<UiTextSpan> parsed_spans;
    ParseHighlightMarkup(paragraph, &parsed_spans);

    std::vector<UiTextToken> tokens;
    TokenizeUiTextSpans(parsed_spans, &tokens);
    if (tokens.empty()) {
        lines->push_back(UiTextLine());
        return;
    }

    UiTextLine current_line;
    int current_width = 0;
    bool pending_space = false;
    bool pending_space_highlight = false;

    for (size_t token_index = 0; token_index < tokens.size(); ++token_index) {
        UiTextToken token = tokens[token_index];
        if (token.whitespace) {
            pending_space = !current_line.spans.empty();
            pending_space_highlight = token.highlighted;
            continue;
        }

        std::string remaining = token.text;
        while (!remaining.empty()) {
            const TTF_Font* pending_font = FontForSpan(fonts, pending_space_highlight);
            TTF_Font* word_font = FontForSpan(fonts, token.highlighted);
            const int pending_width =
                pending_space && !current_line.spans.empty() ? MeasureTextWidth(const_cast<TTF_Font*>(pending_font), " ") : 0;
            const int word_width = MeasureTextWidth(word_font, remaining);

            if (!current_line.spans.empty() && current_width + pending_width + word_width > max_width) {
                FlushWrappedLine(&current_line, lines, &current_width);
                pending_space = false;
                continue;
            }

            if (current_width + pending_width + word_width <= max_width) {
                if (pending_width > 0) {
                    AppendSpanText(&current_line, " ", pending_space_highlight);
                    current_width += pending_width;
                }
                AppendSpanText(&current_line, remaining, token.highlighted);
                current_width += word_width;
                remaining.clear();
                pending_space = false;
                continue;
            }

            const int available_width = std::max(1, max_width - current_width - pending_width);
            size_t fit_length = MeasureFitLength(word_font, remaining, available_width);
            if (fit_length == 0) {
                fit_length = AdvanceUtf8(remaining, 0);
            }

            const std::string fitted = remaining.substr(0, fit_length);
            if (pending_width > 0) {
                AppendSpanText(&current_line, " ", pending_space_highlight);
                current_width += pending_width;
            }
            AppendSpanText(&current_line, fitted, token.highlighted);
            current_width += MeasureTextWidth(word_font, fitted);
            remaining.erase(0, fit_length);
            pending_space = false;

            if (!remaining.empty()) {
                FlushWrappedLine(&current_line, lines, &current_width);
            }
        }
    }

    if (current_line.spans.empty()) {
        lines->push_back(UiTextLine());
    } else {
        lines->push_back(current_line);
    }
}

static void AppendWrappedText(const std::string& text, const UiFonts& fonts, int max_width, std::vector<UiTextLine>* lines)
{
    if (!lines) {
        return;
    }

    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            AppendWrappedParagraph(text.substr(start), fonts, max_width, lines);
            break;
        }

        AppendWrappedParagraph(text.substr(start, end - start), fonts, max_width, lines);
        start = end + 1;
        if (start == text.size()) {
            lines->push_back(UiTextLine());
            break;
        }
    }
}

static void BuildInputWindow(
    const std::string& input_text,
    size_t cursor_byte,
    TTF_Font* font,
    int max_width,
    InputWindow* input_window)
{
    if (!input_window) {
        return;
    }

    input_window->text.clear();
    input_window->cursor_x = 0;

    if (!font || max_width <= 0) {
        return;
    }

    size_t start = 0;
    const size_t clamped_cursor = std::min(cursor_byte, input_text.size());
    while (start < clamped_cursor) {
        const std::string visible_before_cursor = input_text.substr(start, clamped_cursor - start);
        if (MeasureTextWidth(font, visible_before_cursor) <= max_width) {
            break;
        }
        start = AdvanceUtf8(input_text, start);
    }

    size_t end = clamped_cursor;
    while (end < input_text.size()) {
        const size_t next = AdvanceUtf8(input_text, end);
        const std::string candidate = input_text.substr(start, next - start);
        if (MeasureTextWidth(font, candidate) > max_width) {
            break;
        }
        end = next;
    }

    input_window->text = input_text.substr(start, end - start);
    input_window->cursor_x = MeasureTextWidth(font, input_text.substr(start, clamped_cursor - start));
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

static bool SpatialStateBlocksDirectionForUi(const SpatialState& spatial_state, CardinalDirection direction)
{
    for (size_t index = 0; index < spatial_state.blocked_exits.size(); ++index) {
        CardinalDirection blocked_direction = kDirectionUnknown;
        if (ParseCardinalDirection(spatial_state.blocked_exits[index].c_str(), &blocked_direction) &&
            blocked_direction == direction) {
            return true;
        }
    }
    return false;
}

static void DrawCompassDirectionCell(SDL_Renderer* renderer, const SDL_FRect& rect, bool open)
{
    if (!renderer) {
        return;
    }

    if (open) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderRect(renderer, &rect);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 236, 236, 236, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderRect(renderer, &rect);
    SDL_RenderLine(renderer, rect.x + 4.0f, rect.y + 4.0f, rect.x + rect.w - 4.0f, rect.y + rect.h - 4.0f);
    SDL_RenderLine(renderer, rect.x + rect.w - 4.0f, rect.y + 4.0f, rect.x + 4.0f, rect.y + rect.h - 4.0f);
}

static void DrawExitCompass(SDL_Renderer* renderer, const SessionState& session_state, const SDL_FRect& scene_rect)
{
    if (!renderer) {
        return;
    }

    const SDL_FRect panel_rect = {
        scene_rect.x + scene_rect.w - 124.0f,
        scene_rect.y + 12.0f,
        108.0f,
        108.0f,
    };
    DrawPanel(renderer, panel_rect, 244, 0);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDebugText(renderer, panel_rect.x + 24.0f, panel_rect.y + 6.0f, "EXITS");

    const SDL_FRect hub_rect = {panel_rect.x + 44.0f, panel_rect.y + 44.0f, 20.0f, 20.0f};
    DrawPanel(renderer, hub_rect, 250, 0);

    const SDL_FRect north_rect = {panel_rect.x + 43.0f, panel_rect.y + 20.0f, 22.0f, 22.0f};
    const SDL_FRect south_rect = {panel_rect.x + 43.0f, panel_rect.y + 66.0f, 22.0f, 22.0f};
    const SDL_FRect west_rect = {panel_rect.x + 20.0f, panel_rect.y + 43.0f, 22.0f, 22.0f};
    const SDL_FRect east_rect = {panel_rect.x + 66.0f, panel_rect.y + 43.0f, 22.0f, 22.0f};

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderLine(renderer, panel_rect.x + 54.0f, panel_rect.y + 42.0f, panel_rect.x + 54.0f, panel_rect.y + 44.0f);
    SDL_RenderLine(renderer, panel_rect.x + 54.0f, panel_rect.y + 64.0f, panel_rect.x + 54.0f, panel_rect.y + 66.0f);
    SDL_RenderLine(renderer, panel_rect.x + 42.0f, panel_rect.y + 54.0f, panel_rect.x + 44.0f, panel_rect.y + 54.0f);
    SDL_RenderLine(renderer, panel_rect.x + 64.0f, panel_rect.y + 54.0f, panel_rect.x + 66.0f, panel_rect.y + 54.0f);

    DrawCompassDirectionCell(
        renderer,
        north_rect,
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionNorth));
    DrawCompassDirectionCell(
        renderer,
        south_rect,
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionSouth));
    DrawCompassDirectionCell(
        renderer,
        west_rect,
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionWest));
    DrawCompassDirectionCell(
        renderer,
        east_rect,
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionEast));

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDebugText(renderer, panel_rect.x + 51.0f, panel_rect.y + 12.0f, "N");
    SDL_RenderDebugText(renderer, panel_rect.x + 51.0f, panel_rect.y + 92.0f, "S");
    SDL_RenderDebugText(renderer, panel_rect.x + 8.0f, panel_rect.y + 51.0f, "W");
    SDL_RenderDebugText(renderer, panel_rect.x + 92.0f, panel_rect.y + 51.0f, "E");
}

static void UploadSceneTexture(
    const std::vector<unsigned char>& rgb_pixels,
    int width,
    int height,
    std::vector<unsigned char>* rgba_pixels,
    SDL_Texture* texture)
{
    if (!rgba_pixels || !texture || width <= 0 || height <= 0) {
        return;
    }

    rgba_pixels->resize(static_cast<size_t>(width * height * 4));
    const size_t pixel_count = static_cast<size_t>(width * height);
    for (size_t index = 0; index < pixel_count; ++index) {
        const size_t src = index * 3;
        const size_t dst = index * 4;
        (*rgba_pixels)[dst + 0] = src + 0 < rgb_pixels.size() ? rgb_pixels[src + 0] : 0u;
        (*rgba_pixels)[dst + 1] = src + 1 < rgb_pixels.size() ? rgb_pixels[src + 1] : 0u;
        (*rgba_pixels)[dst + 2] = src + 2 < rgb_pixels.size() ? rgb_pixels[src + 2] : 0u;
        (*rgba_pixels)[dst + 3] = 255u;
    }

    SDL_UpdateTexture(texture, 0, &(*rgba_pixels)[0], width * 4);
}

static void CloseTerminalStreamLocked(WorkerSharedState* shared_state)
{
    if (!shared_state || !shared_state->terminal_stream_open) {
        return;
    }

    fputc('\n', stdout);
    fflush(stdout);
    shared_state->terminal_stream_open = false;
    shared_state->terminal_stream_phase_label.clear();
}

static void WriteTerminalStreamChunkLocked(
    WorkerSharedState* shared_state,
    HeadlessTurnStreamPhase phase,
    const char* delta_text)
{
    if (!shared_state) {
        return;
    }

    const std::string phase_label = StreamPhaseLabel(phase);
    if (!shared_state->terminal_stream_open || shared_state->terminal_stream_phase_label != phase_label) {
        CloseTerminalStreamLocked(shared_state);
        fprintf(stdout, "[%s] ", phase_label.c_str());
        fflush(stdout);
        shared_state->terminal_stream_open = true;
        shared_state->terminal_stream_phase_label = phase_label;
    }

    if (delta_text && delta_text[0]) {
        fputs(delta_text, stdout);
        fflush(stdout);
    }
}

static bool LoadSceneForSessionPlace(
    const SessionState& session_state,
    Scene* scene,
    char* error_buffer,
    size_t error_buffer_size)
{
    if (!scene) {
        SetError(error_buffer, error_buffer_size, "Invalid scene target.");
        return false;
    }

    if (IsGeneratedPlaceId(session_state.current_place_id)) {
        for (size_t index = 0; index < session_state.generated_rooms.size(); ++index) {
            if (session_state.generated_rooms[index].room_id == session_state.current_place_id) {
                return AuditSceneCandidateText(
                    session_state.current_place_id.c_str(),
                    session_state.generated_rooms[index].scene_text.c_str(),
                    scene,
                    error_buffer,
                    error_buffer_size);
            }
        }

        SetError(error_buffer, error_buffer_size, "Unknown generated place in session state.");
        return false;
    }

    return CompileSpatialStateToScene(session_state.spatial_state, scene, error_buffer, error_buffer_size);
}

static bool OnHeadlessTurnStream(
    HeadlessTurnStreamPhase phase,
    const char*,
    const char* delta_text,
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
    shared_state->activity = kWorkerActivityLlm;
    shared_state->status_text = StreamPhaseStatus(phase);
    WriteTerminalStreamChunkLocked(shared_state, phase, delta_text);
    return !shared_state->stop_requested.load();
}

static void FinalizeWorkerFailure(WorkerSharedState* shared_state, const char* message)
{
    if (!shared_state) {
        return;
    }

    std::lock_guard<std::mutex> lock(shared_state->mutex);
    CloseTerminalStreamLocked(shared_state);
    shared_state->busy = false;
    shared_state->result_ready = false;
    shared_state->error_ready = true;
    shared_state->activity = kWorkerActivityFailed;
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
        shared_state->activity = kWorkerActivityLlm;
        shared_state->status_text = "Preparing turn...";
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
        CloseTerminalStreamLocked(shared_state);
        shared_state->activity = kWorkerActivityRenderer;
        shared_state->status_text = "Raytracing scene...";
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
        CloseTerminalStreamLocked(shared_state);
        shared_state->busy = false;
        shared_state->result_ready = true;
        shared_state->error_ready = false;
        shared_state->activity = kWorkerActivityComplete;
        shared_state->status_text = "Turn complete.";
        shared_state->session_state = updated_session_state;
        shared_state->turn_result = turn_result;
        shared_state->pixels.swap(pixels);
    }
}

static std::string BuildStatusLine(
    const SessionState& session_state,
    WorkerActivity activity,
    const std::string& worker_status,
    const HeadlessTurnResult& last_turn_result,
    bool have_last_turn,
    bool busy,
    Uint64 ticks_ms)
{
    char buffer[512];
    const char spinner = busy ? SpinnerGlyph(ticks_ms) : ' ';
    if (have_last_turn) {
        snprintf(
            buffer,
            sizeof(buffer),
            "[%c] %s | loc=%s turn=%d alert=%d | %s | prompt=%d gen=%d time=%.0f ms",
            spinner,
            ActivityLabel(activity),
            DescribeCurrentPlaceLabel(session_state).c_str(),
            session_state.hard_state.turn_number,
            session_state.hard_state.alert_level,
            worker_status.c_str(),
            last_turn_result.prompt_tokens,
            last_turn_result.generated_tokens,
            last_turn_result.inference_time_ms);
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "[%c] %s | loc=%s turn=%d alert=%d | %s",
            spinner,
            ActivityLabel(activity),
            DescribeCurrentPlaceLabel(session_state).c_str(),
            session_state.hard_state.turn_number,
            session_state.hard_state.alert_level,
            worker_status.c_str());
    }
    return buffer;
}

static void BuildTranscriptLines(
    const SessionState& session_state,
    const std::string& ui_message,
    const UiFonts& fonts,
    int max_width,
    std::vector<UiTextLine>* lines)
{
    if (!lines) {
        return;
    }

    lines->clear();

    for (size_t index = 0; index < session_state.history.size(); ++index) {
        const SessionTurnRecord& record = session_state.history[index];
        AppendWrappedText(std::string("> ") + record.player_command, fonts, max_width, lines);
        if (!record.narration.empty()) {
            AppendWrappedText(record.narration, fonts, max_width, lines);
        }
    }

    if (!ui_message.empty()) {
        AppendWrappedText(std::string("[") + ui_message + "]", fonts, max_width, lines);
    }
}

static void DrawTextSpan(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::string& text,
    float x,
    float y,
    int line_height,
    SDL_Color color,
    float* advance_x)
{
    if (advance_x) {
        *advance_x = 0.0f;
    }
    if (!renderer || !font || text.empty()) {
        return;
    }

    const int width = MeasureTextWidth(font, text);
    if (advance_x) {
        *advance_x = static_cast<float>(width);
    }
    if (width <= 0 || IsWhitespaceOnly(text)) {
        return;
    }

    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
    if (!surface) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }

    const SDL_FRect dst = {
        x,
        y + (static_cast<float>(line_height) - static_cast<float>(surface->h)) * 0.5f,
        static_cast<float>(surface->w),
        static_cast<float>(surface->h),
    };
    SDL_RenderTexture(renderer, texture, 0, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static void DrawConsoleText(
    SDL_Renderer* renderer,
    const UiFonts& fonts,
    const SDL_FRect& text_rect,
    const std::vector<UiTextLine>& lines,
    size_t line_capacity)
{
    if (!renderer || line_capacity == 0) {
        return;
    }

    const size_t start = lines.size() > line_capacity ? lines.size() - line_capacity : 0;
    float y = text_rect.y + 8.0f;
    const SDL_Color color = {0, 0, 0, 255};
    for (size_t index = start; index < lines.size(); ++index) {
        float x = text_rect.x + 8.0f;
        for (size_t span_index = 0; span_index < lines[index].spans.size(); ++span_index) {
            const UiTextSpan& span = lines[index].spans[span_index];
            float advance_x = 0.0f;
            DrawTextSpan(
                renderer,
                FontForSpan(fonts, span.highlighted),
                span.text,
                x,
                y,
                fonts.line_height,
                color,
                &advance_x);
            x += advance_x;
        }
        y += static_cast<float>(fonts.line_skip);
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
    if (!LoadSceneForSessionPlace(current_session_state, &initial_scene, local_error, sizeof(local_error))) {
        SetError(error_buffer, error_buffer_size, local_error[0] ? local_error : "Cannot compile initial scene.");
        return false;
    }

    std::vector<unsigned char> rgb_pixels;
    if (!RenderSceneToPixels(initial_scene, config.render_config, &rgb_pixels)) {
        SetError(error_buffer, error_buffer_size, "Cannot render initial scene.");
        return false;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SetSdlError(error_buffer, error_buffer_size, "SDL_Init failed");
        return false;
    }

    UiFonts ui_fonts;
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
        DestroyUiFonts(&ui_fonts);
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
        DestroyUiFonts(&ui_fonts);
        SDL_Quit();
        return false;
    }

    if (!LoadUiFonts(&ui_fonts, error_buffer, error_buffer_size)) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    if (!SDL_StartTextInput(window)) {
        SetSdlError(error_buffer, error_buffer_size, "SDL_StartTextInput failed");
        DestroyUiFonts(&ui_fonts);
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
        DestroyUiFonts(&ui_fonts);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    std::vector<unsigned char> texture_rgba_pixels;
    UploadSceneTexture(
        rgb_pixels,
        config.render_config.width,
        config.render_config.height,
        &texture_rgba_pixels,
        scene_texture);

    const SDL_FRect scene_frame = {40.0f, 28.0f, 920.0f, 460.0f};
    const SDL_FRect scene_rect = {60.0f, 48.0f, 880.0f, 440.0f};
    const SDL_FRect console_rect = {40.0f, 516.0f, 920.0f, 196.0f};
    const SDL_FRect input_rect = {40.0f, 728.0f, 920.0f, 44.0f};
    const int console_wrap_width = std::max(1, static_cast<int>(console_rect.w) - 16);
    const int prompt_width = MeasureTextWidth(ui_fonts.regular, "> ");
    const int input_text_max_width = std::max(1, static_cast<int>(input_rect.w) - 24 - prompt_width - 12);
    const size_t console_line_capacity = static_cast<size_t>(
        std::max(1, static_cast<int>((console_rect.h - 16.0f) / static_cast<float>(std::max(ui_fonts.line_skip, 1)))));

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
    std::string ui_message;
    std::string persistent_hint = "Ready. Press Enter to send a command.";
    WorkerActivity worker_activity = kWorkerActivityIdle;
    std::string worker_status = "idle";
    std::vector<UiTextLine> transcript_lines;
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
                    worker_shared_state.activity = kWorkerActivityLlm;
                    worker_shared_state.status_text = "Preparing turn...";
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
                ui_message.clear();
                continue;
            }
        }

        bool turn_completed = false;
        bool turn_failed = false;
        std::string failure_text;

        {
            std::lock_guard<std::mutex> lock(worker_shared_state.mutex);
            worker_busy = worker_shared_state.busy;
            worker_activity = worker_shared_state.activity;
            worker_status = worker_shared_state.status_text.empty() ? "idle" : worker_shared_state.status_text;

            if (worker_shared_state.result_ready) {
                current_session_state = worker_shared_state.session_state;
                last_turn_result = worker_shared_state.turn_result;
                have_last_turn = true;
                rgb_pixels = worker_shared_state.pixels;
                UploadSceneTexture(
                    rgb_pixels,
                    config.render_config.width,
                    config.render_config.height,
                    &texture_rgba_pixels,
                    scene_texture);
                worker_shared_state.result_ready = false;
                turn_completed = true;
                ui_message.clear();
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
            persistent_hint.clear();
        } else if (turn_failed) {
            persistent_hint.clear();
        }

        const std::string status_line = BuildStatusLine(
            current_session_state,
            worker_activity,
            worker_status.empty() ? "idle" : worker_status,
            last_turn_result,
            have_last_turn,
            worker_busy,
            SDL_GetTicks());

        const std::string display_message = !ui_message.empty() ? ui_message : persistent_hint;
        BuildTranscriptLines(current_session_state, display_message, ui_fonts, console_wrap_width, &transcript_lines);

        InputWindow input_window;
        BuildInputWindow(input_text, input_cursor, ui_fonts.regular, input_text_max_width, &input_window);

        SDL_SetRenderDrawColor(renderer, 198, 198, 198, 255);
        SDL_RenderClear(renderer);

        DrawPanel(renderer, scene_frame, 236, 0);
        DrawPanel(renderer, console_rect, 236, 0);
        DrawPanel(renderer, input_rect, 250, 0);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDebugText(renderer, 48.0f, 8.0f, "LE DESERT DES TOKENS");
        SDL_RenderDebugText(renderer, 48.0f, 500.0f, status_line.c_str());

        SDL_RenderTexture(renderer, scene_texture, 0, &scene_rect);
        DrawExitCompass(renderer, current_session_state, scene_rect);
        DrawConsoleText(renderer, ui_fonts, console_rect, transcript_lines, console_line_capacity);

        const SDL_Color player_text_color = {0, 0, 0, 255};
        const float input_text_y =
            input_rect.y + (input_rect.h - static_cast<float>(ui_fonts.line_height)) * 0.5f - 1.0f;
        float prompt_advance = 0.0f;
        DrawTextSpan(
            renderer,
            ui_fonts.regular,
            "> ",
            input_rect.x + 8.0f,
            input_text_y,
            ui_fonts.line_height,
            player_text_color,
            &prompt_advance);
        float input_advance = 0.0f;
        DrawTextSpan(
            renderer,
            ui_fonts.regular,
            input_window.text,
            input_rect.x + 8.0f + prompt_advance,
            input_text_y,
            ui_fonts.line_height,
            player_text_color,
            &input_advance);
        const float cursor_x = input_rect.x + 8.0f + prompt_advance + static_cast<float>(input_window.cursor_x);
        SDL_RenderLine(
            renderer,
            cursor_x,
            input_rect.y + 8.0f,
            cursor_x,
            input_rect.y + input_rect.h - 8.0f);

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
    DestroyUiFonts(&ui_fonts);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    success = true;
    return success;
}

}  // namespace liminal
