#include "sdl_frontend.h"

#include <algorithm>
#include <atomic>
#include <math.h>
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
    bool command_line;

    UiTextLine()
        : command_line(false)
    {
    }
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

static std::string CollapseAsciiWhitespace(const std::string& text)
{
    std::string output;
    output.reserve(text.size());

    bool previous_was_space = false;
    for (size_t index = 0; index < text.size(); ++index) {
        const char value = text[index];
        const bool is_space = value == ' ' || value == '\t' || value == '\r' || value == '\n';
        if (is_space) {
            if (!previous_was_space && !output.empty()) {
                output.push_back(' ');
            }
            previous_was_space = true;
            continue;
        }

        output.push_back(value);
        previous_was_space = false;
    }

    return TrimAsciiSpaces(output);
}

static std::string ToLowerAsciiCopy(const std::string& text)
{
    std::string lower = text;
    for (size_t index = 0; index < lower.size(); ++index) {
        if (lower[index] >= 'A' && lower[index] <= 'Z') {
            lower[index] = static_cast<char>(lower[index] - 'A' + 'a');
        }
    }
    return lower;
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

static void MarkWrappedLinesAsCommand(
    std::vector<UiTextLine>* lines,
    size_t start_index)
{
    if (!lines) {
        return;
    }

    for (size_t index = start_index; index < lines->size(); ++index) {
        (*lines)[index].command_line = true;
    }
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

static std::string ExpandInfocomShortcutCommand(const std::string& command, GameLanguage language)
{
    const std::string normalized = ToLowerAsciiCopy(CollapseAsciiWhitespace(command));
    if (normalized == "n" || normalized == "north" || normalized == "nord") {
        return language == kGameLanguageFrench ? "NORD" : "NORTH";
    }
    if (normalized == "s" || normalized == "south" || normalized == "sud") {
        return language == kGameLanguageFrench ? "SUD" : "SOUTH";
    }
    if (normalized == "e" || normalized == "east" || normalized == "est") {
        return language == kGameLanguageFrench ? "EST" : "EAST";
    }
    if (normalized == "w" || normalized == "west" || normalized == "o" || normalized == "ouest") {
        return language == kGameLanguageFrench ? "OUEST" : "WEST";
    }
    if (normalized == "z" || normalized == "wait" || normalized == "attendre") {
        return language == kGameLanguageFrench ? "ATTENDRE" : "WAIT";
    }
    if (normalized == "i" || normalized == "inventory" || normalized == "inventaire") {
        return language == kGameLanguageFrench ? "INVENTAIRE" : "INVENTORY";
    }
    if (normalized == "q" || normalized == "quit" || normalized == "quitter") {
        return language == kGameLanguageFrench ? "QUITTER" : "QUIT";
    }
    return command;
}

static bool LooksPredominantlyEnglishForUi(const std::string& text)
{
    const std::string lower = " " + ToLowerAsciiCopy(CollapseAsciiWhitespace(text)) + " ";
    const char* markers[] = {
        " the ", " you ", " your ", " and ", " with ", " into ", " from ", " this ", " that ",
        " its ", " is ", " are ", " across ", " beyond ", " through ", " remains ", " stands ",
        " fractured ", " quarry ", " survey ", " north ", " east ", " south ", " west ",
        " shelter ", " room ", " field ", " cut ", " trench ",
    };
    int score = 0;
    for (size_t index = 0; index < sizeof(markers) / sizeof(markers[0]); ++index) {
        if (lower.find(markers[index]) != std::string::npos) {
            ++score;
        }
    }
    return score >= 2 || lower.compare(0, 5, " the ") == 0 || lower.compare(0, 5, " you ") == 0;
}

static std::string DescribeCurrentPlaceLabelForUi(const SessionState& session_state)
{
    const std::string label = DescribeCurrentPlaceLabel(session_state);
    if (session_state.language == kGameLanguageFrench && LooksPredominantlyEnglishForUi(label)) {
        return "Secteur de prospection";
    }
    return label;
}

static bool IsQuitCommand(const std::string& command)
{
    const std::string normalized = ToLowerAsciiCopy(CollapseAsciiWhitespace(command));
    return normalized == "q" || normalized == "quit" || normalized == "quitter";
}

static void DrawTextSpan(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::string& text,
    float x,
    float y,
    int line_height,
    SDL_Color color,
    float* advance_x);

static void DrawPanel(SDL_Renderer* renderer, const SDL_FRect& rect, Uint8 fill, Uint8 border)
{
    SDL_SetRenderDrawColor(renderer, fill, fill, fill, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border, border, border, 255);
    SDL_RenderRect(renderer, &rect);
}

static void DrawLanguageSelectionCell(
    SDL_Renderer* renderer,
    const UiFonts& fonts,
    const SDL_FRect& rect,
    const std::string& label)
{
    const SDL_FRect outer_rect = {rect.x - 3.0f, rect.y - 3.0f, rect.w + 6.0f, rect.h + 6.0f};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &outer_rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &rect);

    const int text_width = MeasureTextWidth(fonts.regular, label);
    const int text_height = std::max(TTF_GetFontHeight(fonts.regular), 1);
    DrawTextSpan(
        renderer,
        fonts.regular,
        label,
        rect.x + (rect.w - static_cast<float>(text_width)) * 0.5f,
        rect.y + (rect.h - static_cast<float>(text_height)) * 0.5f - 1.0f,
        text_height,
        SDL_Color{255, 255, 255, 255},
        0);
}

static void DrawLanguageSelection(SDL_Renderer* renderer, const UiFonts& fonts, int logical_width, int logical_height)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const std::string heading = "SELECT LANGUAGE / CHOISIR LA LANGUE";
    const int heading_width = MeasureTextWidth(fonts.regular, heading);
    DrawTextSpan(
        renderer,
        fonts.regular,
        heading,
        (static_cast<float>(logical_width) - static_cast<float>(heading_width)) * 0.5f,
        static_cast<float>(logical_height) * 0.30f,
        fonts.line_height,
        SDL_Color{255, 255, 255, 255},
        0);

    const float panel_width = 360.0f;
    const float panel_height = 58.0f;
    const float panel_x = (static_cast<float>(logical_width) - panel_width) * 0.5f;
    const float first_y = static_cast<float>(logical_height) * 0.42f;
    DrawLanguageSelectionCell(renderer, fonts, SDL_FRect{panel_x, first_y, panel_width, panel_height}, "English (E)");
    DrawLanguageSelectionCell(renderer, fonts, SDL_FRect{panel_x, first_y + 90.0f, panel_width, panel_height}, "Français (F)");
}

static void DrawProceduralVisorMask(SDL_Renderer* renderer, const SDL_FRect& rect)
{
    if (!renderer || rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }

    const int height = static_cast<int>(rect.h);
    const float center_x = rect.x + rect.w * 0.5f;
    const float full_half_width = rect.w * 0.495f;
    const float corner_radius = std::min(rect.h * 0.16f, 68.0f);
    const float notch_start = rect.h * 0.68f;
    const float notch_half_width = rect.w * 0.09f;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    for (int row = 0; row < height; ++row) {
        const float y = static_cast<float>(row) + 0.5f;
        float corner_inset = 0.0f;
        if (y < corner_radius) {
            const float dy = corner_radius - y;
            corner_inset = corner_radius - sqrtf(std::max(0.0f, corner_radius * corner_radius - dy * dy));
        } else if (y > rect.h - corner_radius) {
            const float dy = y - (rect.h - corner_radius);
            corner_inset = corner_radius - sqrtf(std::max(0.0f, corner_radius * corner_radius - dy * dy));
        }

        const float left_inside = center_x - full_half_width + corner_inset;
        const float right_inside = center_x + full_half_width - corner_inset;
        if (left_inside > rect.x) {
            const SDL_FRect left_mask = {rect.x, rect.y + static_cast<float>(row), left_inside - rect.x, 1.0f};
            SDL_RenderFillRect(renderer, &left_mask);
        }
        if (right_inside < rect.x + rect.w) {
            const SDL_FRect right_mask = {
                right_inside,
                rect.y + static_cast<float>(row),
                rect.x + rect.w - right_inside,
                1.0f};
            SDL_RenderFillRect(renderer, &right_mask);
        }

        if (y >= notch_start) {
            const float t = (y - notch_start) / std::max(1.0f, rect.h - notch_start);
            const float ellipse_term = std::max(0.0f, 1.0f - (1.0f - t) * (1.0f - t));
            const float half_notch = notch_half_width * sqrtf(ellipse_term);
            const SDL_FRect center_mask = {
                center_x - half_notch,
                rect.y + static_cast<float>(row),
                half_notch * 2.0f,
                1.0f};
            SDL_RenderFillRect(renderer, &center_mask);
        }
    }
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

static void DrawCompassDirectionCell(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const SDL_FRect& rect,
    const char* label,
    bool open)
{
    if (!renderer) {
        return;
    }

    const SDL_FRect outer_rect = {rect.x - 2.0f, rect.y - 2.0f, rect.w + 4.0f, rect.h + 4.0f};
    if (open) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &outer_rect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderRect(renderer, &rect);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &outer_rect);
        SDL_SetRenderDrawColor(renderer, 242, 242, 242, 255);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderRect(renderer, &rect);
    }

    if (!font || !label || !label[0]) {
        return;
    }

    const SDL_Color label_color = open ? SDL_Color{255, 255, 255, 255} : SDL_Color{0, 0, 0, 255};
    const int label_width = MeasureTextWidth(font, label);
    const int label_height = std::max(TTF_GetFontHeight(font), 1);
    DrawTextSpan(
        renderer,
        font,
        label,
        rect.x + (rect.w - static_cast<float>(label_width)) * 0.5f,
        rect.y + (rect.h - static_cast<float>(label_height)) * 0.5f - 1.0f,
        label_height,
        label_color,
        0);
}

static void DrawExitCompass(
    SDL_Renderer* renderer,
    const UiFonts& fonts,
    const SessionState& session_state,
    const SDL_FRect& scene_rect)
{
    if (!renderer) {
        return;
    }

    const float cell_size = 30.0f;
    const float gap = 8.0f;
    const float anchor_x = scene_rect.x + scene_rect.w - 112.0f;
    const float anchor_y = scene_rect.y + scene_rect.h - 112.0f;

    const SDL_FRect north_rect = {anchor_x + cell_size + gap, anchor_y, cell_size, cell_size};
    const SDL_FRect west_rect = {anchor_x, anchor_y + cell_size + gap, cell_size, cell_size};
    const SDL_FRect east_rect = {anchor_x + (cell_size + gap) * 2.0f, anchor_y + cell_size + gap, cell_size, cell_size};
    const SDL_FRect south_rect = {anchor_x + cell_size + gap, anchor_y + (cell_size + gap) * 2.0f, cell_size, cell_size};

    DrawCompassDirectionCell(
        renderer,
        fonts.regular,
        north_rect,
        "N",
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionNorth));
    DrawCompassDirectionCell(
        renderer,
        fonts.regular,
        east_rect,
        "E",
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionEast));
    DrawCompassDirectionCell(
        renderer,
        fonts.regular,
        south_rect,
        "S",
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionSouth));
    DrawCompassDirectionCell(
        renderer,
        fonts.regular,
        west_rect,
        session_state.language == kGameLanguageFrench ? "O" : "W",
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionWest));
}

static void DrawThermalIndicator(
    SDL_Renderer* renderer,
    const UiFonts& fonts,
    const SDL_FRect& rect,
    const std::string& label_text)
{
    const int label_height = std::max(TTF_GetFontHeight(fonts.regular), 1);
    const float padding_x = 10.0f;
    const SDL_FRect outer_rect = {rect.x - 2.0f, rect.y - 2.0f, rect.w + 4.0f, rect.h + 4.0f};

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &outer_rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderRect(renderer, &rect);

    DrawTextSpan(
        renderer,
        fonts.regular,
        label_text,
        rect.x + padding_x,
        rect.y + (rect.h - static_cast<float>(label_height)) * 0.5f - 1.0f,
        label_height,
        SDL_Color{255, 255, 255, 255},
        0);
}

static void DrawThermalHud(
    SDL_Renderer* renderer,
    const UiFonts& fonts,
    const SessionState& session_state,
    const SDL_FRect& scene_rect)
{
    if (!renderer) {
        return;
    }

    char external_buffer[96];
    char body_buffer[96];
    if (session_state.language == kGameLanguageFrench) {
        snprintf(
            external_buffer,
            sizeof(external_buffer),
            "TEMP. EXTÉRIEURE  %d °C",
            session_state.hard_state.external_temperature_c);
        snprintf(
            body_buffer,
            sizeof(body_buffer),
            "TEMP. CORPORELLE  %.1f °C",
            session_state.hard_state.body_temperature_c);
    } else {
        snprintf(
            external_buffer,
            sizeof(external_buffer),
            "EXT. TEMPERATURE  %d °C",
            session_state.hard_state.external_temperature_c);
        snprintf(
            body_buffer,
            sizeof(body_buffer),
            "BODY TEMPERATURE  %.1f °C",
            session_state.hard_state.body_temperature_c);
    }

    const int label_height = std::max(TTF_GetFontHeight(fonts.regular), 1);
    const float panel_width = static_cast<float>(std::max(
        MeasureTextWidth(fonts.regular, external_buffer),
        MeasureTextWidth(fonts.regular, body_buffer))) + 20.0f;
    const float panel_height = static_cast<float>(label_height) + 12.0f;
    const float panel_x = scene_rect.x + 14.0f;
    const float body_y = scene_rect.y + scene_rect.h - panel_height - 14.0f;
    const SDL_FRect external_rect = {panel_x, body_y - panel_height - 8.0f, panel_width, panel_height};
    const SDL_FRect body_rect = {panel_x, body_y, panel_width, panel_height};
    DrawThermalIndicator(renderer, fonts, external_rect, external_buffer);
    DrawThermalIndicator(renderer, fonts, body_rect, body_buffer);
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

static const GeneratedRoom* FindGeneratedRoomForPlace(
    const SessionState& session_state,
    const std::string& place_id)
{
    if (!IsGeneratedPlaceId(place_id)) {
        return 0;
    }

    for (size_t index = 0; index < session_state.generated_rooms.size(); ++index) {
        if (session_state.generated_rooms[index].room_id == place_id) {
            return &session_state.generated_rooms[index];
        }
    }

    return 0;
}

static bool TurnCreatesCurrentGeneratedRoom(
    const HeadlessTurnResult& turn_result,
    const std::string& place_id)
{
    if (!turn_result.generated_room_created) {
        return false;
    }

    for (size_t index = 0; index < turn_result.generated_rooms_to_add.size(); ++index) {
        if (turn_result.generated_rooms_to_add[index].room_id == place_id) {
            return true;
        }
    }

    return false;
}

static const char* BoolSourceLabel(bool fallback_used)
{
    return fallback_used ? "fallback" : "llm";
}

static const char* GeneratedRoomSceneSourceLabel(const GeneratedRoom* room)
{
    if (!room) {
        return "unknown";
    }
    if (!room->scene_source.empty()) {
        return room->scene_source.c_str();
    }
    return room->scene_fallback_used ? "fallback" : "llm";
}

static const char* RenderSourceLabel(
    const SessionState& session_state,
    const HeadlessTurnResult& turn_result)
{
    if (turn_result.used_candidate_scene_for_render) {
        return "candidate-scene";
    }
    if (IsGeneratedPlaceId(session_state.current_place_id)) {
        return "generated-room-cache";
    }
    return "compiled-spatial-state";
}

static void PrintTurnProvenance(
    const SessionState& session_state,
    const HeadlessTurnResult& turn_result)
{
    const std::string place_label = DescribeCurrentPlaceLabel(session_state);
    if (!IsGeneratedPlaceId(session_state.current_place_id)) {
        fprintf(
            stdout,
            "[turn-info] place=%s kind=canonical render=%s turn_fallback=%s\n",
            place_label.c_str(),
            RenderSourceLabel(session_state, turn_result),
            turn_result.used_turn_fallback ? "yes" : "no");
        fflush(stdout);
        return;
    }

    const GeneratedRoom* room = FindGeneratedRoomForPlace(session_state, session_state.current_place_id);
    fprintf(
        stdout,
        "[turn-info] place=%s kind=generated room=%s render=%s metadata=%s scene=%s turn_fallback=%s\n",
        place_label.c_str(),
        TurnCreatesCurrentGeneratedRoom(turn_result, session_state.current_place_id) ? "new" : "cached",
        RenderSourceLabel(session_state, turn_result),
        room ? BoolSourceLabel(room->metadata_fallback_used) : "unknown",
        GeneratedRoomSceneSourceLabel(room),
        turn_result.used_turn_fallback ? "yes" : "no");
    fflush(stdout);
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
        PrintTurnProvenance(updated_session_state, turn_result);
        PrintHeadlessTurnDebugTrace(turn_result, stdout);
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
    (void)worker_status;
    (void)last_turn_result;
    (void)have_last_turn;
    char buffer[512];
    const char spinner = busy ? SpinnerGlyph(ticks_ms) : ' ';
    if (session_state.language == kGameLanguageFrench) {
        const char* activity_label = "PRÊT";
        switch (activity) {
        case kWorkerActivityLlm: activity_label = "IMAGINATION"; break;
        case kWorkerActivityRenderer: activity_label = "RAYTRACING"; break;
        case kWorkerActivityComplete: activity_label = "TERMINÉ"; break;
        case kWorkerActivityFailed: activity_label = "ERREUR"; break;
        default: break;
        }
        snprintf(
            buffer,
            sizeof(buffer),
            "[%c] %s | lieu=%s tour=%d alerte=%d",
            spinner,
            activity_label,
            DescribeCurrentPlaceLabelForUi(session_state).c_str(),
            session_state.hard_state.turn_number,
            session_state.hard_state.alert_level);
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "[%c] %s | location=%s turn=%d alert=%d",
            spinner,
            ActivityLabel(activity),
            DescribeCurrentPlaceLabelForUi(session_state).c_str(),
            session_state.hard_state.turn_number,
            session_state.hard_state.alert_level);
    }
    return buffer;
}

static void BuildTranscriptLines(
    const SessionState& session_state,
    const std::string& pending_command,
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
        const size_t start_line = lines->size();
        AppendWrappedText(
            std::string("> ") + ExpandInfocomShortcutCommand(record.player_command, session_state.language),
            fonts,
            max_width,
            lines);
        MarkWrappedLinesAsCommand(lines, start_line);
        if (!record.narration.empty()) {
            const std::string narration =
                session_state.language == kGameLanguageFrench && LooksPredominantlyEnglishForUi(record.narration)
                ? "Le compte rendu de ce tour demeure archivé dans sa langue d'origine."
                : record.narration;
            AppendWrappedText(narration, fonts, max_width, lines);
        }
    }

    if (!pending_command.empty()) {
        const size_t start_line = lines->size();
        AppendWrappedText(std::string("> ") + pending_command, fonts, max_width, lines);
        MarkWrappedLinesAsCommand(lines, start_line);
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
    for (size_t index = start; index < lines.size(); ++index) {
        float x = text_rect.x + 8.0f;
        const SDL_Color color = lines[index].command_line ? SDL_Color{76, 76, 76, 255} : SDL_Color{0, 0, 0, 255};
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

static std::string FitTextWithEllipsis(TTF_Font* font, const std::string& text, int max_width)
{
    if (!font || text.empty() || max_width <= 0) {
        return std::string();
    }

    if (MeasureTextWidth(font, text) <= max_width) {
        return text;
    }

    const std::string ellipsis = "...";
    const int ellipsis_width = MeasureTextWidth(font, ellipsis);
    if (ellipsis_width >= max_width) {
        return ellipsis;
    }

    const size_t fit_length = MeasureFitLength(font, text, max_width - ellipsis_width);
    const std::string trimmed = TrimAsciiSpaces(text.substr(0, fit_length));
    if (trimmed.empty()) {
        return ellipsis;
    }

    return trimmed + ellipsis;
}

static void DrawTitleBar(
    SDL_Renderer* renderer,
    const UiFonts& fonts,
    const SDL_FRect& rect,
    const SessionState& session_state)
{
    if (!renderer) {
        return;
    }

    DrawPanel(renderer, rect, 18, 0);

    const std::string room_title = DescribeCurrentPlaceLabelForUi(session_state);
    char stats_buffer[128];
    snprintf(
        stats_buffer,
        sizeof(stats_buffer),
        session_state.language == kGameLanguageFrench ? "Déplacements %d   Score %d" : "Moves %d   Score %d",
        session_state.hard_state.move_count,
        session_state.hard_state.score);
    const std::string stats_text = stats_buffer;

    const int stats_width = MeasureTextWidth(fonts.regular, stats_text);
    const int title_max_width = std::max(1, static_cast<int>(rect.w) - 24 - stats_width - 24);
    const std::string fitted_title = FitTextWithEllipsis(fonts.regular, room_title, title_max_width);
    const float text_y = rect.y + (rect.h - static_cast<float>(fonts.line_height)) * 0.5f - 1.0f;
    const SDL_Color title_color = {255, 255, 255, 255};

    DrawTextSpan(
        renderer,
        fonts.regular,
        fitted_title,
        rect.x + 10.0f,
        text_y,
        fonts.line_height,
        title_color,
        0);

    DrawTextSpan(
        renderer,
        fonts.regular,
        stats_text,
        rect.x + rect.w - 10.0f - static_cast<float>(stats_width),
        text_y,
        fonts.line_height,
        title_color,
        0);
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
            "Eryx - Quarry Survey Prototype",
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

    const SDL_FRect title_rect = {40.0f, 4.0f, 920.0f, 22.0f};
    const SDL_FRect scene_frame = {40.0f, 28.0f, 920.0f, 460.0f};
    const SDL_FRect scene_rect = {
        scene_frame.x + 1.0f,
        scene_frame.y + 1.0f,
        scene_frame.w - 2.0f,
        scene_frame.h - 2.0f,
    };
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
    bool language_selected = false;
    bool have_last_turn = false;
    HeadlessTurnResult last_turn_result;
    std::string input_text;
    size_t input_cursor = 0;
    int history_index = -1;
    std::string history_draft;
    std::string ui_message;
    std::string persistent_hint;
    WorkerActivity worker_activity = kWorkerActivityIdle;
    std::string worker_status = "idle";
    std::string pending_command;
    std::vector<UiTextLine> transcript_lines;
    bool worker_busy = false;
    Uint64 text_input_suppressed_until = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                worker_shared_state.stop_requested.store(true);
                running = false;
                break;
            }

            if (!language_selected) {
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.down && !event.key.repeat) {
                    if (event.key.key == SDLK_E) {
                        current_session_state.language = kGameLanguageEnglish;
                        for (size_t index = 0; index < command_history.size(); ++index) {
                            command_history[index] = ExpandInfocomShortcutCommand(command_history[index], current_session_state.language);
                        }
                        language_selected = true;
                        text_input_suppressed_until = SDL_GetTicks() + 250;
                        persistent_hint = "Ready. Press Enter to send a command.";
                    } else if (event.key.key == SDLK_F) {
                        current_session_state.language = kGameLanguageFrench;
                        for (size_t index = 0; index < command_history.size(); ++index) {
                            command_history[index] = ExpandInfocomShortcutCommand(command_history[index], current_session_state.language);
                        }
                        language_selected = true;
                        text_input_suppressed_until = SDL_GetTicks() + 250;
                        persistent_hint = "Prêt. Appuyez sur Entrée pour envoyer une commande.";
                    }
                }
                continue;
            }

            if (event.type == SDL_EVENT_TEXT_INPUT) {
                if (SDL_GetTicks() < text_input_suppressed_until) {
                    continue;
                }
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
                    ui_message = current_session_state.language == kGameLanguageFrench
                        ? "Annulation demandée..."
                        : "Cancellation requested...";
                } else {
                    input_text.clear();
                    input_cursor = 0;
                    history_index = -1;
                }
                continue;
            }
            if (key == SDLK_RETURN) {
                const std::string raw_command = TrimCommandText(input_text);
                if (raw_command.empty()) {
                    ui_message = current_session_state.language == kGameLanguageFrench
                        ? "Commande vide ignorée."
                        : "Empty command ignored.";
                    continue;
                }

                const std::string command = ExpandInfocomShortcutCommand(raw_command, current_session_state.language);

                bool busy = false;
                {
                    std::lock_guard<std::mutex> lock(worker_shared_state.mutex);
                    busy = worker_shared_state.busy;
                }
                if (busy) {
                    ui_message = current_session_state.language == kGameLanguageFrench
                        ? "Un tour est déjà en cours."
                        : "A turn is already running.";
                    continue;
                }

                if (worker_thread.joinable()) {
                    worker_thread.join();
                    worker_joined = true;
                }

                if (IsQuitCommand(command)) {
                    worker_shared_state.stop_requested.store(true);
                    running = false;
                    break;
                }

                if (command_history.empty() || command_history.back() != command) {
                    command_history.push_back(command);
                }

                pending_command = command;
                persistent_hint.clear();
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

        if (!language_selected && running) {
            DrawLanguageSelection(renderer, ui_fonts, config.logical_width, config.logical_height);
            SDL_RenderPresent(renderer);
            SDL_Delay(16);
            continue;
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
                pending_command.clear();
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
                ui_message = current_session_state.language == kGameLanguageFrench
                    ? "Le tour a échoué. Consultez la sortie de diagnostic."
                    : failure_text;
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
        BuildTranscriptLines(
            current_session_state,
            pending_command,
            display_message,
            ui_fonts,
            console_wrap_width,
            &transcript_lines);

        InputWindow input_window;
        BuildInputWindow(input_text, input_cursor, ui_fonts.regular, input_text_max_width, &input_window);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        DrawPanel(renderer, scene_frame, 236, 0);
        DrawPanel(renderer, console_rect, 236, 0);
        DrawPanel(renderer, input_rect, 250, 0);

        DrawTitleBar(renderer, ui_fonts, title_rect, current_session_state);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, 48.0f, 500.0f, status_line.c_str());

        SDL_RenderTexture(renderer, scene_texture, 0, &scene_rect);
        DrawProceduralVisorMask(renderer, scene_rect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderRect(renderer, &scene_frame);
        DrawThermalHud(renderer, ui_fonts, current_session_state, scene_rect);
        DrawExitCompass(renderer, ui_fonts, current_session_state, scene_rect);
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
