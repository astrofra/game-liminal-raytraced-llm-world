#include "resource_paths.h"
#include "ambience_audio.h"
#include "nlohmann/json.hpp"
#include "narrative_language.h"
#include "sdl_frontend.h"
#include "investigation.h"
#include "known_barrier.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <math.h>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "animated_view.h"
#include "renderer.h"
#include "scene_compiler.h"
#include "research_archive.h"

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
    GameLanguage language = kGameLanguageEnglish;
    std::string status_text;
    std::string error_text;
    bool terminal_stream_open;
    std::string terminal_stream_phase_label;
    SessionState session_state;
    HeadlessTurnResult turn_result;
    std::vector<unsigned char> pixels;
    double first_image_render_duration_ms;

    WorkerSharedState()
        : stop_requested(false)
        , busy(false)
        , result_ready(false)
        , error_ready(false)
        , activity(kWorkerActivityIdle)
        , terminal_stream_open(false)
        , first_image_render_duration_ms(0.0)
    {
    }
};

struct AnimationFramePublication {
    uint64_t generation_id;
    int image_index;
    std::vector<unsigned char> pixels;
    double render_duration_ms;

    AnimationFramePublication()
        : generation_id(0)
        , image_index(-1)
        , render_duration_ms(0.0)
    {
    }
};

struct AnimationWorkerSharedState {
    std::mutex mutex;
    std::atomic<bool> stop_requested;
    std::atomic<uint64_t> active_generation_id;
    bool busy;
    uint64_t worker_generation_id;
    int image_being_rendered;
    std::vector<AnimationFramePublication> completed_frames;
    bool completion_ready;
    uint64_t completion_generation_id;
    double total_generation_duration_ms;
    bool failure_ready;
    uint64_t failure_generation_id;
    std::string failure_text;

    AnimationWorkerSharedState()
        : stop_requested(false)
        , active_generation_id(0)
        , busy(false)
        , worker_generation_id(0)
        , image_being_rendered(-1)
        , completion_ready(false)
        , completion_generation_id(0)
        , total_generation_duration_ms(0.0)
        , failure_ready(false)
        , failure_generation_id(0)
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
    TTF_Font* title_large;
    TTF_Font* title_small;
    int line_skip;
    int line_height;

    UiFonts()
        : regular(0)
        , highlight(0)
        , title_large(0)
        , title_small(0)
        , line_skip(20)
        , line_height(18)
    {
    }
};

static const char* UiText(
    GameLanguage language,
    const char* english,
    const char* french,
    const char* norwegian,
    const char* danish,
    const char* german,
    const char* italian)
{
    switch (language) {
    case kGameLanguageFrench: return french;
    case kGameLanguageNorwegian: return norwegian;
    case kGameLanguageDanish: return danish;
    case kGameLanguageGerman: return german;
    case kGameLanguageItalian: return italian;
    case kGameLanguageEnglish:
    default: return english;
    }
}

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
        case kHeadlessTurnStreamSceneDesign: return "scene-design";
        case kHeadlessTurnStreamSceneAudit: return "scene-audit";
        case kHeadlessTurnStreamScenePreview: return "scene-preview";
        default:
            return "stream";
    }
}

static const char* StreamPhaseStatus(HeadlessTurnStreamPhase phase, GameLanguage language)
{
    switch (phase) {
        case kHeadlessTurnStreamPrimaryResponse:
            return UiText(language,"Thinking...","Réflexion...","Tenker...","Tænker...","Überlegt...","Riflessione...");
        case kHeadlessTurnStreamRepairResponse:
            return UiText(language,"Adjusting the place...","Ajustement du lieu...","Justerer stedet...","Justerer stedet...","Ort wird angepasst...","Adattamento del luogo...");
        case kHeadlessTurnStreamSceneProgram:
            return UiText(language,"Building the place...","Construction du lieu...","Bygger stedet...","Bygger stedet...","Ort wird aufgebaut...","Costruzione del luogo...");
        case kHeadlessTurnStreamSceneDesign:
            return UiText(language,"Imagining the place...","Composition du lieu...","Forestiller stedet...","Forestiller stedet...","Ort wird entworfen...","Ideazione del luogo...");
        case kHeadlessTurnStreamSceneAudit:
            return UiText(language,"Checking the passages...","Vérification des passages...","Sjekker passasjene...","Kontrollerer passagerne...","Durchgänge werden geprüft...","Verifica dei passaggi...");
        case kHeadlessTurnStreamScenePreview:
            return UiText(language,"Preparing the view...","Préparation de la vue...","Forbereder utsikten...","Forbereder udsigten...","Ansicht wird vorbereitet...","Preparazione della vista...");
        default:
            return UiText(language,"Thinking...","Réflexion...","Tenker...","Tænker...","Überlegt...","Riflessione...");
    }
}

static const char* LocalizedActivityLabel(WorkerActivity activity, GameLanguage language)
{
    switch (activity) {
    case kWorkerActivityLlm:
        return UiText(language, "llm", "IMAGINATION", "FORESTILLING", "FORESTILLING", "IMAGINATION", "IMMAGINAZIONE");
    case kWorkerActivityRenderer:
        return UiText(language, "cpu", "RAYTRACING", "STRÅLESPORING", "STRÅLESPORING", "RAYTRACING", "RAYTRACING");
    case kWorkerActivityComplete:
        return UiText(language, "done", "TERMINÉ", "FERDIG", "FÆRDIG", "FERTIG", "COMPLETATO");
    case kWorkerActivityFailed:
        return UiText(language, "error", "ERREUR", "FEIL", "FEJL", "FEHLER", "ERRORE");
    default:
        return UiText(language, "idle", "PRÊT", "KLAR", "KLAR", "BEREIT", "PRONTO");
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

    return TryOpenFontCandidate(ResourcePath(asset_path).c_str(), point_size, font);
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
    if (!OpenFontWithFallbacks("assets/fonts/Zilla_Slab/ZillaSlab-Regular.ttf", 52.0f, &fonts->title_large)) {
        SetTtfError(error_buffer, error_buffer_size, "Failed to open large title font");
        TTF_CloseFont(fonts->highlight);
        TTF_CloseFont(fonts->regular);
        fonts->highlight = 0;
        fonts->regular = 0;
        TTF_Quit();
        return false;
    }
    if (!OpenFontWithFallbacks("assets/fonts/Zilla_Slab/ZillaSlab-Regular.ttf", 24.0f, &fonts->title_small)) {
        SetTtfError(error_buffer, error_buffer_size, "Failed to open small title font");
        TTF_CloseFont(fonts->title_large);
        TTF_CloseFont(fonts->highlight);
        TTF_CloseFont(fonts->regular);
        fonts->title_large = 0;
        fonts->highlight = 0;
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

    if (fonts->title_small) {
        TTF_CloseFont(fonts->title_small);
        fonts->title_small = 0;
    }
    if (fonts->title_large) {
        TTF_CloseFont(fonts->title_large);
        fonts->title_large = 0;
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
        return UiText(language, "NORTH", "NORD", "NORD", "NORD", "NORD", "NORD");
    }
    if (normalized == "s" || normalized == "south" || normalized == "sud" || normalized == "syd" ||
        normalized == "sor" || normalized == "sør" || normalized == "süd") {
        return UiText(language, "SOUTH", "SUD", "SØR", "SYD", "SÜD", "SUD");
    }
    if (normalized == "o") {
        if (language == kGameLanguageGerman) {
            return "OST";
        }
        if (language == kGameLanguageNorwegian || language == kGameLanguageDanish) {
            return "ØST";
        }
        return UiText(language, "WEST", "OUEST", "VEST", "VEST", "WEST", "OVEST");
    }
    if (normalized == "e" || normalized == "east" || normalized == "est" || normalized == "ost" ||
        normalized == "øst") {
        return UiText(language, "EAST", "EST", "ØST", "ØST", "OST", "EST");
    }
    if (normalized == "w" || normalized == "v" || normalized == "west" || normalized == "ouest" ||
        normalized == "ovest" || normalized == "vest") {
        return UiText(language, "WEST", "OUEST", "VEST", "VEST", "WEST", "OVEST");
    }
    if (normalized == "z" || normalized == "wait" || normalized == "attendre" || normalized == "vent" ||
        normalized == "warten" || normalized == "aspetta") {
        return UiText(language, "WAIT", "ATTENDRE", "VENT", "VENT", "WARTEN", "ASPETTA");
    }
    if (normalized == "i" || normalized == "inventory" || normalized == "inventaire" ||
        normalized == "inventar" || normalized == "inventario") {
        return UiText(language, "INVENTORY", "INVENTAIRE", "INVENTAR", "INVENTAR", "INVENTAR", "INVENTARIO");
    }
    if (normalized == "q" || normalized == "quit" || normalized == "quitter" || normalized == "avslutt" ||
        normalized == "afslut" || normalized == "beenden" || normalized == "esci") {
        return UiText(language, "QUIT", "QUITTER", "AVSLUTT", "AFSLUT", "BEENDEN", "ESCI");
    }
    return command;
}

static std::string DescribeCurrentPlaceLabelForUi(const SessionState& session_state)
{
    const std::string label = DescribeCurrentPlaceLabel(session_state);
    if (IsClearlyWrongNarrativeLanguage(label,session_state.language)) {
        return UiText(
            session_state.language, "Survey Sector", "Secteur de prospection", "Prospekteringssektor",
            "Prospekteringssektor", "Prospektionssektor", "Settore di prospezione");
    }
    return label;
}

static bool IsQuitCommand(const std::string& command)
{
    const std::string normalized = ToLowerAsciiCopy(CollapseAsciiWhitespace(command));
    return normalized == "q" || normalized == "quit" || normalized == "quitter" ||
        normalized == "avslutt" || normalized == "afslut" || normalized == "beenden" || normalized == "esci";
}

static bool LanguageForSelectionKey(SDL_Keycode key, GameLanguage* language)
{
    if (!language) {
        return false;
    }
    switch (key) {
    case SDLK_E: *language = kGameLanguageEnglish; return true;
    case SDLK_F: *language = kGameLanguageFrench; return true;
    case SDLK_N: *language = kGameLanguageNorwegian; return true;
    case SDLK_D: *language = kGameLanguageDanish; return true;
    case SDLK_G: *language = kGameLanguageGerman; return true;
    case SDLK_I: *language = kGameLanguageItalian; return true;
    default: return false;
    }
}

static const char* ReadyHint(GameLanguage language)
{
    return UiText(
        language,
        "Enter: send a command. Wheel / Page Up / Page Down: read the transcript.",
        "Entrée : envoyer. Molette / Page précédente / Page suivante : relire le carnet.",
        "Enter: send kommando. Musehjul / Page Up / Page Down: les teksten.",
        "Enter: send kommando. Musehjul / Page Up / Page Down: læs teksten.",
        "Eingabe: Befehl senden. Mausrad / Bild auf / Bild ab: Text lesen.",
        "Invio: invia comando. Rotella / Pag su / Pag giù: rileggi il testo.");
}

static void DrawTextSpan(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::string& text,
    float x,
    float y,
    int line_height,
    SDL_Color color,
    float* advance_x,
    SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND);

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

    const std::string french_title = "Entre les Murs Latents";
    const std::string english_title = "Within the Latent Walls";
    const int french_title_width = MeasureTextWidth(fonts.title_large, french_title);
    const int english_title_width = MeasureTextWidth(fonts.title_small, english_title);
    const int french_title_height = std::max(TTF_GetFontHeight(fonts.title_large), 1);
    const int english_title_height = std::max(TTF_GetFontHeight(fonts.title_small), 1);
    const float french_title_y = static_cast<float>(logical_height) * 0.13f;
    const float english_title_y = french_title_y + static_cast<float>(french_title_height) + 8.0f;
    DrawTextSpan(
        renderer,
        fonts.title_large,
        french_title,
        (static_cast<float>(logical_width) - static_cast<float>(french_title_width)) * 0.5f,
        french_title_y,
        french_title_height,
        SDL_Color{255, 255, 255, 255},
        0);
    DrawTextSpan(
        renderer,
        fonts.title_small,
        english_title,
        (static_cast<float>(logical_width) - static_cast<float>(english_title_width)) * 0.5f,
        english_title_y,
        english_title_height,
        SDL_Color{190, 190, 190, 255},
        0);

    const std::string heading = "CHOISIR LA LANGUE / SELECT LANGUAGE";
    const int heading_width = MeasureTextWidth(fonts.regular, heading);
    const float heading_y = english_title_y + static_cast<float>(english_title_height) + 40.0f;
    DrawTextSpan(
        renderer,
        fonts.regular,
        heading,
        (static_cast<float>(logical_width) - static_cast<float>(heading_width)) * 0.5f,
        heading_y,
        fonts.line_height,
        SDL_Color{255, 255, 255, 255},
        0);

    const float panel_width = 300.0f;
    const float panel_height = 54.0f;
    const float column_gap = 34.0f;
    const float row_gap = 28.0f;
    const float panel_x = (static_cast<float>(logical_width) - panel_width * 2.0f - column_gap) * 0.5f;
    const float first_y = heading_y + static_cast<float>(fonts.line_height) + 52.0f;
    DrawLanguageSelectionCell(renderer, fonts, SDL_FRect{panel_x, first_y, panel_width, panel_height}, "English (E)");
    DrawLanguageSelectionCell(
        renderer,
        fonts,
        SDL_FRect{panel_x + panel_width + column_gap, first_y, panel_width, panel_height},
        "Français (F)");
    DrawLanguageSelectionCell(
        renderer,
        fonts,
        SDL_FRect{panel_x, first_y + panel_height + row_gap, panel_width, panel_height},
        "Norsk (N)");
    DrawLanguageSelectionCell(
        renderer,
        fonts,
        SDL_FRect{panel_x + panel_width + column_gap, first_y + panel_height + row_gap, panel_width, panel_height},
        "Dansk (D)");
    DrawLanguageSelectionCell(
        renderer,
        fonts,
        SDL_FRect{panel_x, first_y + (panel_height + row_gap) * 2.0f, panel_width, panel_height},
        "Deutsch (G)");
    DrawLanguageSelectionCell(
        renderer,
        fonts,
        SDL_FRect{panel_x + panel_width + column_gap, first_y + (panel_height + row_gap) * 2.0f, panel_width, panel_height},
        "Italiano (I)");

    const std::string copyright = "(c) Resistance 2026";
    const int copyright_width = MeasureTextWidth(fonts.regular, copyright);
    const int copyright_height = std::max(TTF_GetFontHeight(fonts.regular), 1);
    DrawTextSpan(
        renderer,
        fonts.regular,
        copyright,
        (static_cast<float>(logical_width) - static_cast<float>(copyright_width)) * 0.5f,
        static_cast<float>(logical_height - copyright_height - 18),
        copyright_height,
        SDL_Color{150, 150, 150, 255},
        0);
}

static SDL_Texture* LoadVisorTexture(SDL_Renderer* renderer)
{
    const std::string path = ResourcePath("assets/hud-mask.png");
    SDL_Surface* surface = SDL_LoadPNG(path.c_str());
    if (!surface) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) return nullptr;
    // Straight alpha preserves the clear aperture and the dotted glass edge.
    if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
        !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    return texture;
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

static SDL_Color HudPhosphorColor(bool full_intensity)
{
    return full_intensity
        ? SDL_Color{184, 254, 80, 255}
        : SDL_Color{61, 84, 26, 255};
}

static void DrawCompassDirectionCell(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const SDL_FRect& rect,
    const char* label,
    bool open,
    bool known_barrier=false)
{
    if (!renderer) {
        return;
    }

    const SDL_Color phosphor = known_barrier ? SDL_Color{245, 191, 91, 255} : HudPhosphorColor(open);
    const SDL_BlendMode blend_mode = known_barrier ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_ADD;
    SDL_SetRenderDrawBlendMode(renderer, blend_mode);
    SDL_SetRenderDrawColor(renderer, phosphor.r, phosphor.g, phosphor.b, phosphor.a);
    SDL_RenderRect(renderer, &rect);
    if(known_barrier) {
        // A crossed amber cell distinguishes a discovered obstruction from a
        // merely closed architectural exit. Keep the direction legible.
        SDL_SetRenderDrawColor(renderer,245,191,91,255);
        SDL_RenderRect(renderer,&rect);
        // Split the slash around the label so N/E/S/W remains readable.
        SDL_RenderLine(renderer,rect.x+2,rect.y+rect.h-2,rect.x+7,rect.y+rect.h-7);
        SDL_RenderLine(renderer,rect.x+rect.w-7,rect.y+7,rect.x+rect.w-2,rect.y+2);
    }
    if (open) {
        const SDL_FRect one_pixel_rect = {rect.x - 1.0f, rect.y - 1.0f, rect.w + 2.0f, rect.h + 2.0f};
        const SDL_FRect two_pixel_rect = {rect.x - 2.0f, rect.y - 2.0f, rect.w + 4.0f, rect.h + 4.0f};
        SDL_RenderRect(renderer, &one_pixel_rect);
        SDL_RenderRect(renderer, &two_pixel_rect);
    }

    if (!font || !label || !label[0]) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        return;
    }

    const int label_width = MeasureTextWidth(font, label);
    const int label_height = std::max(TTF_GetFontHeight(font), 1);
    DrawTextSpan(
        renderer,
        font,
        label,
        rect.x + (rect.w - static_cast<float>(label_width)) * 0.5f,
        rect.y + (rect.h - static_cast<float>(label_height)) * 0.5f - 1.0f,
        label_height,
        phosphor,
        0,
        blend_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

// Shared vertical center of the HUD pair: the thermal block centers its two
// panels on it, and the compass centers its own block on the same line.
static float ThermalHudCenterY(const UiFonts& fonts, const SDL_FRect& scene_rect)
{
    const int label_height = std::max(TTF_GetFontHeight(fonts.regular), 1);
    const float panel_height = static_cast<float>(label_height) + 12.0f;
    // The 4 pixels are half the 8-pixel gap between the two thermal panels.
    return scene_rect.y + scene_rect.h - panel_height - 72.0f - 4.0f;
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
    const float compass_size = 3.0f * cell_size + 2.0f * gap;
    // Center the compass in the right clear area, away from the right gasket joint.
    const float compass_center_x = scene_rect.x + scene_rect.w * 0.75f;
    const float anchor_x = compass_center_x - compass_size * 0.5f;
    // Keep every cell above the bitmap gasket and its dotted transition while
    // sharing the thermal block's vertical center.
    const float anchor_y = ThermalHudCenterY(fonts, scene_rect) - compass_size * 0.5f;

    const SDL_FRect north_rect = {anchor_x + cell_size + gap, anchor_y, cell_size, cell_size};
    const SDL_FRect west_rect = {anchor_x, anchor_y + cell_size + gap, cell_size, cell_size};
    const SDL_FRect east_rect = {anchor_x + (cell_size + gap) * 2.0f, anchor_y + cell_size + gap, cell_size, cell_size};
    const SDL_FRect south_rect = {anchor_x + cell_size + gap, anchor_y + (cell_size + gap) * 2.0f, cell_size, cell_size};

    DrawCompassDirectionCell(
        renderer,
        fonts.regular,
        north_rect,
        LocalizedDirectionShortcut(kDirectionNorth,session_state.language),
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionNorth)&&!HasKnownBarrier(session_state,kDirectionNorth),
        HasKnownBarrier(session_state,kDirectionNorth));
    DrawCompassDirectionCell(
        renderer,
        fonts.regular,
        east_rect,
        LocalizedDirectionShortcut(kDirectionEast,session_state.language),
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionEast)&&!HasKnownBarrier(session_state,kDirectionEast),
        HasKnownBarrier(session_state,kDirectionEast));
    DrawCompassDirectionCell(
        renderer,
        fonts.regular,
        south_rect,
        LocalizedDirectionShortcut(kDirectionSouth,session_state.language),
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionSouth)&&!HasKnownBarrier(session_state,kDirectionSouth),
        HasKnownBarrier(session_state,kDirectionSouth));
    DrawCompassDirectionCell(
        renderer,
        fonts.regular,
        west_rect,
        LocalizedDirectionShortcut(kDirectionWest,session_state.language),
        !SpatialStateBlocksDirectionForUi(session_state.spatial_state, kDirectionWest)&&!HasKnownBarrier(session_state,kDirectionWest),
        HasKnownBarrier(session_state,kDirectionWest));
}

static void DrawThermalIndicator(
    SDL_Renderer* renderer,
    const UiFonts& fonts,
    const SDL_FRect& rect,
    const std::string& label_text)
{
    const int label_height = std::max(TTF_GetFontHeight(fonts.regular), 1);
    const float padding_x = 10.0f;
    const SDL_Color phosphor = HudPhosphorColor(true);
    const SDL_FRect outer_rect = {rect.x - 1.0f, rect.y - 1.0f, rect.w + 2.0f, rect.h + 2.0f};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(renderer, phosphor.r, phosphor.g, phosphor.b, phosphor.a);
    SDL_RenderRect(renderer, &outer_rect);
    SDL_RenderRect(renderer, &rect);

    DrawTextSpan(
        renderer,
        fonts.regular,
        label_text,
        rect.x + padding_x,
        rect.y + (rect.h - static_cast<float>(label_height)) * 0.5f - 1.0f,
        label_height,
        phosphor,
        0,
        SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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
    snprintf(
        external_buffer,
        sizeof(external_buffer),
        UiText(
            session_state.language,
            "EXT. TEMPERATURE  %d °C",
            "TEMP. EXTÉRIEURE  %d °C",
            "UTETEMPERATUR  %d °C",
            "UDETEMPERATUR  %d °C",
            "AUSSENTEMPERATUR  %d °C",
            "TEMP. ESTERNA  %d °C"),
        session_state.hard_state.external_temperature_c);
    snprintf(
        body_buffer,
        sizeof(body_buffer),
        UiText(
            session_state.language,
            "BODY TEMPERATURE  %.1f °C",
            "TEMP. CORPORELLE  %.1f °C",
            "KROPPSTEMPERATUR  %.1f °C",
            "KROPSTEMPERATUR  %.1f °C",
            "KÖRPERTEMPERATUR  %.1f °C",
            "TEMP. CORPOREA  %.1f °C"),
        session_state.hard_state.body_temperature_c);

    const int label_height = std::max(TTF_GetFontHeight(fonts.regular), 1);
    const float panel_width = static_cast<float>(std::max(
        MeasureTextWidth(fonts.regular, external_buffer),
        MeasureTextWidth(fonts.regular, body_buffer))) + 20.0f;
    const float panel_height = static_cast<float>(label_height) + 12.0f;
    // Center the aligned pair in the left clear area, away from rim and nose notch.
    const float panel_x = scene_rect.x + scene_rect.w * 0.25f - panel_width * 0.5f;
    // Preserve the previous panel height: shared center plus half the 8-pixel gap.
    const float body_y = ThermalHudCenterY(fonts, scene_rect) + 4.0f;
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
    shared_state->status_text = StreamPhaseStatus(phase,shared_state->language);
    WriteTerminalStreamChunkLocked(shared_state, phase, delta_text);
    return !shared_state->stop_requested.load();
}

static const char* TurnCancelledText(GameLanguage language)
{
    return UiText(language,"Cancelled. You remain here.","Annulé. Vous restez ici.",
        "Avbrutt. Du blir her.","Annulleret. Du bliver her.","Abgebrochen. Du bleibst hier.","Annullato. Rimani qui.");
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
    shared_state->status_text = UiText(shared_state->language,"Turn failed.","Échec du tour.","Turen mislyktes.","Turen mislykkedes.","Zug fehlgeschlagen.","Turno non riuscito.");
    if (message) fprintf(stderr, "Turn failed: %s\n", message);
    const std::string detail = message ? message : "";
    const bool model_error = detail.find("Model file not found") != std::string::npos ||
        detail.find("Model SHA-256") != std::string::npos || detail.find("Failed to load") != std::string::npos;
    shared_state->error_text = shared_state->stop_requested.load() ? TurnCancelledText(shared_state->language) :
        model_error ? UiText(shared_state->language,
            "The local model is missing or unreadable. Check the complete game package.",
            "Le modèle local manque ou est illisible. Vérifiez le paquet complet du jeu.",
            "Den lokale modellen mangler eller kan ikke leses. Kontroller hele spillpakken.",
            "Den lokale model mangler eller kan ikke læses. Kontrollér hele spilpakken.",
            "Das lokale Modell fehlt oder ist nicht lesbar. Prüfe das vollständige Spielpaket.",
            "Il modello locale manca o non è leggibile. Verifica il pacchetto completo del gioco.") :
        UiText(shared_state->language,
            "This turn could not be completed. You remain here.",
            "Ce tour n’a pas pu aboutir. Vous restez ici.",
            "Denne turen kunne ikke fullføres. Du blir her.",
            "Denne tur kunne ikke gennemføres. Du bliver her.",
            "Dieser Zug konnte nicht abgeschlossen werden. Du bleibst hier.",
            "Questo turno non è stato completato. Rimani qui.");
}

static PeripheralPostProcessConfig PostProcessConfigForAnimationImage(
    const PeripheralPostProcessConfig& base_config,
    uint64_t generation_id,
    int image_index)
{
    PeripheralPostProcessConfig image_config = base_config;
    image_config.grain_seed ^= static_cast<unsigned int>(generation_id);
    image_config.grain_seed ^= static_cast<unsigned int>(generation_id >> 32) * 0x85ebca6bu;
    image_config.grain_seed ^= static_cast<unsigned int>(image_index + 1) * 0x9e3779b9u;
    return image_config;
}

static void RunTurnWorker(
    SdlFrontendConfig config,
    SessionState initial_session_state,
    std::string player_command,
    uint64_t target_generation_id,
    WorkerSharedState* shared_state)
{
    if (!shared_state) {
        return;
    }

    HeadlessTurnConfig turn_config = config.turn_config;
    turn_config.reuse_unchanged_view = true;
    turn_config.stream_callback = OnHeadlessTurnStream;
    turn_config.stream_user_data = shared_state;

    {
        std::lock_guard<std::mutex> lock(shared_state->mutex);
        shared_state->language=initial_session_state.language;
        shared_state->activity = kWorkerActivityLlm;
        shared_state->status_text = StreamPhaseStatus(kHeadlessTurnStreamPrimaryResponse,shared_state->language);
        shared_state->error_text.clear();
    }

    char error_buffer[512];
    memset(error_buffer, 0, sizeof(error_buffer));

    HeadlessTurnResult turn_result;
    auto fail_turn = [&](const char* message) {
        {
            std::lock_guard<std::mutex> lock(shared_state->mutex);
            shared_state->turn_result = turn_result;
        }
        FinalizeWorkerFailure(shared_state, message);
    };
    if (!RunHeadlessTurnFromState(
            initial_session_state,
            player_command.c_str(),
            turn_config,
            &turn_result,
            error_buffer,
            sizeof(error_buffer))) {
        if (shared_state->stop_requested.load()) {
            fail_turn("Turn cancelled.");
        } else {
            fail_turn(error_buffer[0] ? error_buffer : "Headless turn failed.");
        }
        return;
    }

    if (shared_state->stop_requested.load()) {
        fail_turn("Turn cancelled.");
        return;
    }

    SessionState updated_session_state = initial_session_state;
    UpdateSessionStateFromTurn(player_command.c_str(), turn_result, &updated_session_state);

    std::vector<unsigned char> pixels;
    double render_duration_ms = 0.0;
    if (!turn_result.view_unchanged) {
        {
            std::lock_guard<std::mutex> lock(shared_state->mutex);
            CloseTerminalStreamLocked(shared_state);
            shared_state->activity = kWorkerActivityRenderer;
            shared_state->status_text = UiText(shared_state->language,"Calculating the view...","Calcul de la vue...",
                "Beregner utsikten...","Beregner udsigten...","Ansicht wird berechnet...","Calcolo della vista...");
        }

        const std::chrono::steady_clock::time_point render_start = std::chrono::steady_clock::now();
        RenderControl control{[](void* state) { return !static_cast<WorkerSharedState*>(state)->stop_requested.load(); },shared_state};
        if (!RenderSceneToPixels(turn_result.rendered_scene, config.render_config, &pixels,&control)) {
            fail_turn("Rendering failed.");
            return;
        }
        if (!ApplyPeripheralViewPostProcess(
                &pixels,
                config.render_config.width,
                config.render_config.height,
                PostProcessConfigForAnimationImage(config.post_process_config, target_generation_id, 0),
                0)) {
            fail_turn("View post-processing failed.");
            return;
        }
        render_duration_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - render_start).count();
    }

    if (shared_state->stop_requested.load()) {
        fail_turn("Turn cancelled.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shared_state->mutex);
        CloseTerminalStreamLocked(shared_state);
        PrintTurnProvenance(updated_session_state, turn_result);
        PrintHeadlessTurnDebugTrace(turn_result, stdout);
        shared_state->busy = false;
        shared_state->result_ready = true;
        shared_state->error_ready = false;
        shared_state->activity = kWorkerActivityComplete;
        shared_state->status_text = UiText(shared_state->language,"Turn complete.","Tour terminé.","Turen er ferdig.","Turen er færdig.","Zug abgeschlossen.","Turno completato.");
        std::swap(shared_state->session_state, updated_session_state);
        std::swap(shared_state->turn_result, turn_result);
        shared_state->pixels.swap(pixels);
        shared_state->first_image_render_duration_ms = render_duration_ms;
    }
}

static void RunAnimationWorker(
    RenderConfig render_config,
    AnimatedViewConfig animation_config,
    PeripheralPostProcessConfig post_process_config,
    Scene scene_snapshot,
    Camera pose_a,
    Camera pose_b,
    uint64_t generation_id,
    double first_image_render_duration_ms,
    AnimationWorkerSharedState* shared_state)
{
    if (!shared_state) {
        return;
    }

    const std::chrono::steady_clock::time_point generation_start = std::chrono::steady_clock::now();
    fprintf(
        stdout,
        "[animated-view] generation=%llu started available=1 image_bytes=%zu texture_bytes=%zu\n",
        static_cast<unsigned long long>(generation_id),
        static_cast<size_t>(render_config.width) * static_cast<size_t>(render_config.height) * 3u,
        static_cast<size_t>(render_config.width) * static_cast<size_t>(render_config.height) * 4u);
    fflush(stdout);

    bool failed = false;
    bool cancelled = false;
    std::string failure_text;

    for (int image_index = 1; image_index < kAnimatedViewImageCapacity; ++image_index) {
        if (shared_state->stop_requested.load() ||
            shared_state->active_generation_id.load() != generation_id) {
            cancelled = true;
            break;
        }

        {
            std::lock_guard<std::mutex> lock(shared_state->mutex);
            shared_state->image_being_rendered = image_index;
        }
        if (animation_config.debug_logging) {
            fprintf(
                stdout,
                "[animated-view] generation=%llu rendering=%d\n",
                static_cast<unsigned long long>(generation_id),
                image_index);
            fflush(stdout);
        }

        if (animation_config.forced_failure_image_index == image_index) {
            failed = true;
            failure_text = "Forced animated-view render failure.";
            break;
        }

        scene_snapshot.camera = CalculateAnimatedCameraPose(pose_a, pose_b, image_index);
        const std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();
        std::vector<unsigned char> pixels;
        struct AnimationGate { AnimationWorkerSharedState* state; uint64_t generation; } gate{shared_state,generation_id};
        RenderControl control{[](void* p) {
            const auto& g=*static_cast<AnimationGate*>(p);
            return !g.state->stop_requested.load() && g.state->active_generation_id.load()==g.generation;
        },&gate};
        if (!RenderSceneToPixels(scene_snapshot, render_config, &pixels,&control)) {
            if(shared_state->stop_requested.load() || shared_state->active_generation_id.load()!=generation_id) {cancelled=true;break;}
            failed = true;
            failure_text = "Animated-view raytracing failed.";
            break;
        }
        if (!ApplyPeripheralViewPostProcess(
                &pixels,
                render_config.width,
                render_config.height,
                PostProcessConfigForAnimationImage(post_process_config, generation_id, image_index),
                0)) {
            failed = true;
            failure_text = "Animated-view post-processing failed.";
            break;
        }
        const double render_duration_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - frame_start).count();

        if (shared_state->stop_requested.load() ||
            shared_state->active_generation_id.load() != generation_id) {
            fprintf(
                stdout,
                "[animated-view] generation=%llu stale image=%d discarded duration_ms=%.2f\n",
                static_cast<unsigned long long>(generation_id),
                image_index,
                render_duration_ms);
            fflush(stdout);
            cancelled = true;
            break;
        }

        {
            std::lock_guard<std::mutex> lock(shared_state->mutex);
            shared_state->completed_frames.push_back(AnimationFramePublication());
            AnimationFramePublication& publication = shared_state->completed_frames.back();
            publication.generation_id = generation_id;
            publication.image_index = image_index;
            publication.pixels.swap(pixels);
            publication.render_duration_ms = render_duration_ms;
        }

        fprintf(
            stdout,
            "[animated-view] generation=%llu image=%d rendered duration_ms=%.2f\n",
            static_cast<unsigned long long>(generation_id),
            image_index,
            render_duration_ms);
        fflush(stdout);
    }

    const double worker_duration_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - generation_start).count();
    {
        std::lock_guard<std::mutex> lock(shared_state->mutex);
        shared_state->busy = false;
        shared_state->image_being_rendered = -1;
        if (shared_state->active_generation_id.load() == generation_id && !cancelled) {
            if (failed) {
                shared_state->failure_ready = true;
                shared_state->failure_generation_id = generation_id;
                shared_state->failure_text = failure_text;
            } else {
                shared_state->completion_ready = true;
                shared_state->completion_generation_id = generation_id;
                shared_state->total_generation_duration_ms =
                    first_image_render_duration_ms + worker_duration_ms;
            }
        }
    }

    if (cancelled) {
        fprintf(
            stdout,
            "[animated-view] generation=%llu cancelled or superseded\n",
            static_cast<unsigned long long>(generation_id));
    } else if (failed) {
        fprintf(
            stderr,
            "[animated-view] generation=%llu failed: %s\n",
            static_cast<unsigned long long>(generation_id),
            failure_text.c_str());
    } else {
        fprintf(
            stdout,
            "[animated-view] generation=%llu complete total_ms=%.2f\n",
            static_cast<unsigned long long>(generation_id),
            first_image_render_duration_ms + worker_duration_ms);
    }
    fflush(stdout);
}

static void StartAnimationWorker(
    const RenderConfig& render_config,
    const AnimatedViewConfig& animation_config,
    const PeripheralPostProcessConfig& post_process_config,
    const AnimatedView& view,
    AnimationWorkerSharedState* shared_state,
    std::thread* worker_thread)
{
    if (!shared_state || !worker_thread || worker_thread->joinable() ||
        view.available_image_count < 1 || view.available_image_count >= kAnimatedViewImageCapacity ||
        view.generation_complete || view.generation_failed) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shared_state->mutex);
        shared_state->busy = true;
        shared_state->worker_generation_id = view.generation_id;
        shared_state->image_being_rendered = 1;
        shared_state->completion_ready = false;
        shared_state->failure_ready = false;
        shared_state->failure_text.clear();
    }
    shared_state->stop_requested.store(false);
    shared_state->active_generation_id.store(view.generation_id);
    *worker_thread = std::thread(
        RunAnimationWorker,
        render_config,
        animation_config,
        post_process_config,
        view.scene_snapshot,
        view.pose_a,
        view.pose_b,
        view.generation_id,
        view.total_generation_duration_ms,
        shared_state);
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
    snprintf(
        buffer,
        sizeof(buffer),
        UiText(
            session_state.language,
            "[%c] %s | location=%s turn=%d alert=%d",
            "[%c] %s | lieu=%s tour=%d alerte=%d",
            "[%c] %s | sted=%s tur=%d alarm=%d",
            "[%c] %s | sted=%s tur=%d alarm=%d",
            "[%c] %s | Ort=%s Zug=%d Alarm=%d",
            "[%c] %s | luogo=%s turno=%d allerta=%d"),
        spinner,
        LocalizedActivityLabel(activity, session_state.language),
        DescribeCurrentPlaceLabelForUi(session_state).c_str(),
        session_state.hard_state.turn_number,
        session_state.hard_state.alert_level);
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
    AppendWrappedText(InvestigationMandate(session_state.language),fonts,max_width,lines);
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
                IsClearlyWrongNarrativeLanguage(record.narration,session_state.language)
                ? UiText(
                    session_state.language,
                    "This turn remains archived in its original language.",
                    "Le compte rendu de ce tour demeure archivé dans sa langue d'origine.",
                    "Denne turen er fortsatt arkivert på originalspråket.",
                    "Denne tur er fortsat arkiveret på originalsproget.",
                    "Dieser Zug bleibt in seiner Originalsprache archiviert.",
                    "Questo turno resta archiviato nella lingua originale.")
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
    float* advance_x,
    SDL_BlendMode blend_mode)
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
    SDL_SetTextureBlendMode(texture, blend_mode);

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
    size_t line_capacity, size_t scroll_offset)
{
    if (!renderer || line_capacity == 0) {
        return;
    }

    const size_t end = lines.size() - std::min(scroll_offset,lines.size());
    const size_t start = end > line_capacity ? end - line_capacity : 0;
    float y = text_rect.y + 8.0f;
    for (size_t index = start; index < end; ++index) {
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
        UiText(
            session_state.language,
            "Moves %d   Score %d",
            "Déplacements %d   Score %d",
            "Trekk %d   Poeng %d",
            "Træk %d   Point %d",
            "Züge %d   Punkte %d",
            "Mosse %d   Punti %d"),
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
    RestoreAuthoritativeCanonicalTopology(&current_session_state);
    const bool scripted_audio_start=config.turn_config.ambience_catalog&&current_session_state.history.empty()&&
        current_session_state.current_place_id==BuildCanonicalPlaceId(kLocationQuarryThreshold)&&
        current_session_state.audio_catalog_version.empty();
    if(scripted_audio_start) {
        const auto& catalog=*config.turn_config.ambience_catalog;
        for(size_t i=0;i<kAmbienceTracks;++i)
            current_session_state.audio_targets[i]=std::min(current_session_state.audio_targets[i],catalog.tracks[i].max_gain);
        current_session_state.audio_catalog_version=catalog.version;
    }
    ResearchArchive archive;
    if (!archive.Start(config.research_root,current_session_state,config.turn_config.generation_config,config.render_config)) {
        SetError(error_buffer,error_buffer_size,archive.Error().c_str());return false;
    }
    bool archive_ok = true;
    unsigned int command_sequence = 0;
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

    const std::chrono::steady_clock::time_point initial_render_start = std::chrono::steady_clock::now();
    std::vector<unsigned char> rgb_pixels;
    if (!RenderSceneToPixels(initial_scene, config.render_config, &rgb_pixels)) {
        SetError(error_buffer, error_buffer_size, "Cannot render initial scene.");
        return false;
    }
    if (!ApplyPeripheralViewPostProcess(
            &rgb_pixels,
            config.render_config.width,
            config.render_config.height,
            PostProcessConfigForAnimationImage(config.post_process_config, 1u, 0),
            0)) {
        SetError(error_buffer, error_buffer_size, "Cannot post-process initial scene.");
        return false;
    }
    const double initial_render_duration_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - initial_render_start).count();
    archive_ok = archive.Image(current_session_state,rgb_pixels) && archive_ok;

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
            "Within the Latent Walls",
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

    SDL_Texture* visor_texture = LoadVisorTexture(renderer);
    if (!visor_texture) {
        SetSdlError(error_buffer, error_buffer_size, "Cannot load visor assets/hud-mask.png");
        SDL_DestroyTexture(scene_texture);
        SDL_StopTextInput(window);
        DestroyUiFonts(&ui_fonts);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    AnimatedView active_animated_view;
    uint64_t next_generation_id = 1u;
    if (!InitializeAnimatedView(
            &active_animated_view,
            next_generation_id,
            initial_scene,
            config.animated_view_config,
            &rgb_pixels,
            initial_render_duration_ms)) {
        SetError(error_buffer, error_buffer_size, "Cannot initialize animated view.");
        SDL_DestroyTexture(visor_texture);
        SDL_DestroyTexture(scene_texture);
        SDL_StopTextInput(window);
        DestroyUiFonts(&ui_fonts);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    AmbienceAudio audio;
    if(config.turn_config.ambience_catalog) {
        std::string why;
        if(audio.Open(*config.turn_config.ambience_catalog,&why)) {
            audio.SetTargets(current_session_state.audio_targets,config.audio_ramp_seconds);
            auto audio_start=nlohmann::json::parse(audio.SnapshotJson("started"));
            audio_start["source"]=scripted_audio_start?"scripted_quarry_threshold":"session_targets";
            audio_start["ramp_seconds"]=config.audio_ramp_seconds;
            archive_ok=archive.AudioEvent(audio_start.dump())&&archive_ok;
        } else {
            fprintf(stderr,"[audio] Playback unavailable: %s\n",why.c_str());
            archive_ok=archive.AudioEvent(nlohmann::json{{"event","unavailable"},{"error",why},
                {"catalog_version",config.turn_config.ambience_catalog->version}}.dump())&&archive_ok;
        }
    }
    std::vector<unsigned char> texture_rgba_pixels;
    UploadSceneTexture(
        active_animated_view.images[0].pixels,
        config.render_config.width,
        config.render_config.height,
        &texture_rgba_pixels,
        scene_texture);
    uint64_t uploaded_generation_id = active_animated_view.generation_id;
    int uploaded_image_index = 0;

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
    AnimationWorkerSharedState animation_shared_state;
    std::thread animation_worker_thread;
    animation_shared_state.active_generation_id.store(active_animated_view.generation_id);
    StartAnimationWorker(
        config.render_config,
        config.animated_view_config,
        config.post_process_config,
        active_animated_view,
        &animation_shared_state,
        &animation_worker_thread);
    Uint64 playback_last_ticks = SDL_GetTicks();
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
    int console_scroll = 0;
    bool worker_busy = false;
    Uint64 text_input_suppressed_until = 0;
    const Uint64 smoke_test_start_ticks = SDL_GetTicks();
    bool smoke_test_text_pushed = false;
    bool smoke_test_return_pushed = false;
    if (config.automated_smoke_test_duration_ms > 0) {
        current_session_state.language = config.automated_smoke_test_language;
        language_selected = true;
        persistent_hint = ReadyHint(current_session_state.language);
    }

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                worker_shared_state.stop_requested.store(true);
                animation_shared_state.stop_requested.store(true);
                running = false;
                break;
            }

            if (!language_selected) {
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.down && !event.key.repeat) {
                    GameLanguage selected_language = kGameLanguageEnglish;
                    if (LanguageForSelectionKey(event.key.key, &selected_language)) {
                        current_session_state.language = selected_language;
                        for (size_t index = 0; index < command_history.size(); ++index) {
                            command_history[index] = ExpandInfocomShortcutCommand(command_history[index], current_session_state.language);
                        }
                        language_selected = true;
                        playback_last_ticks = SDL_GetTicks();
                        text_input_suppressed_until = SDL_GetTicks() + 250;
                        persistent_hint = ReadyHint(current_session_state.language);
                    }
                }
                continue;
            }

            if(event.type==SDL_EVENT_MOUSE_WHEEL) {
                const int sign=event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1;
                const int maximum=static_cast<int>(transcript_lines.size()>console_line_capacity?transcript_lines.size()-console_line_capacity:0);
                console_scroll=std::clamp(console_scroll+sign*event.wheel.integer_y*3,0,maximum);
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
            if(key==SDLK_PAGEUP||key==SDLK_PAGEDOWN) {
                const int maximum=static_cast<int>(transcript_lines.size()>console_line_capacity?transcript_lines.size()-console_line_capacity:0);
                console_scroll=std::clamp(console_scroll+(key==SDLK_PAGEUP?1:-1)*static_cast<int>(console_line_capacity),0,maximum);
                continue;
            }
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
                    busy = worker_shared_state.busy || worker_shared_state.result_ready;
                }
                if (busy) {
                    worker_shared_state.stop_requested.store(true);
                    ui_message = UiText(
                        current_session_state.language,
                        "Cancellation requested...", "Annulation demandée...", "Avbrudd forespurt...",
                        "Annullering anmodet...", "Abbruch angefordert...", "Annullamento richiesto...");
                } else {
                    input_text.clear();
                    input_cursor = 0;
                    history_index = -1;
                }
                continue;
            }
            if (key == SDLK_RETURN) {
                console_scroll=0;
                const std::string raw_command = TrimCommandText(input_text);
                if (raw_command.empty()) {
                    ui_message = UiText(
                        current_session_state.language,
                        "Empty command ignored.", "Commande vide ignorée.", "Tom kommando ignorert.",
                        "Tom kommando ignoreret.", "Leerer Befehl ignoriert.", "Comando vuoto ignorato.");
                    continue;
                }

                const std::string command = ExpandInfocomShortcutCommand(raw_command, current_session_state.language);

                bool busy = false;
                {
                    std::lock_guard<std::mutex> lock(worker_shared_state.mutex);
                    busy = worker_shared_state.busy || worker_shared_state.result_ready;
                }
                if (busy) {
                    ui_message = UiText(
                        current_session_state.language,
                        "A turn is already running.", "Un tour est déjà en cours.", "En tur pågår allerede.",
                        "En tur er allerede i gang.", "Ein Zug wird bereits verarbeitet.", "Un turno è già in corso.");
                    continue;
                }

                if (worker_thread.joinable()) {
                    worker_thread.join();
                    worker_joined = true;
                }

                if (IsQuitCommand(command)) {
                    worker_shared_state.stop_requested.store(true);
                    animation_shared_state.stop_requested.store(true);
                    running = false;
                    break;
                }

                if (command_history.empty() || command_history.back() != command) {
                    command_history.push_back(command);
                }

                pending_command = command;
                archive_ok = archive.Command(command) && archive_ok;
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
                    worker_shared_state.language = current_session_state.language;
                    worker_shared_state.status_text = StreamPhaseStatus(kHeadlessTurnStreamPrimaryResponse,current_session_state.language);
                    worker_shared_state.error_text.clear();
                }
                worker_shared_state.stop_requested.store(false);
                SdlFrontendConfig request_config=config;
                request_config.turn_config.audio_current_known=audio.Active();
                request_config.turn_config.audio_current_gains=audio.Current();
                request_config.turn_config.generation_config.seed += 104729u * command_sequence++;
                worker_thread = std::thread(
                    RunTurnWorker,
                    request_config,
                    current_session_state,
                    command,
                    next_generation_id + 1u,
                    &worker_shared_state);
                worker_joined = false;
                ui_message.clear();
                continue;
            }
        }

        const Uint64 smoke_test_elapsed_ticks = SDL_GetTicks() - smoke_test_start_ticks;
        if (!config.automated_smoke_test_command.empty() && !smoke_test_text_pushed &&
            smoke_test_elapsed_ticks >= 250u) {
            SDL_Event text_event;
            SDL_zero(text_event);
            text_event.type = SDL_EVENT_TEXT_INPUT;
            text_event.text.windowID = SDL_GetWindowID(window);
            text_event.text.text = config.automated_smoke_test_command.c_str();
            SDL_PushEvent(&text_event);
            smoke_test_text_pushed = true;
        }
        if (smoke_test_text_pushed && !smoke_test_return_pushed && smoke_test_elapsed_ticks >= 350u) {
            SDL_Event return_event;
            SDL_zero(return_event);
            return_event.type = SDL_EVENT_KEY_DOWN;
            return_event.key.windowID = SDL_GetWindowID(window);
            return_event.key.key = SDLK_RETURN;
            return_event.key.down = true;
            return_event.key.repeat = false;
            SDL_PushEvent(&return_event);
            smoke_test_return_pushed = true;
        }

        if (config.automated_smoke_test_duration_ms > 0 &&
            smoke_test_elapsed_ticks >=
                static_cast<Uint64>(config.automated_smoke_test_duration_ms)) {
            worker_shared_state.stop_requested.store(true);
            animation_shared_state.stop_requested.store(true);
            running = false;
            break;
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
        bool turn_result_ready = false;
        SessionState completed_session_state;
        HeadlessTurnResult completed_turn_result;
        std::vector<unsigned char> completed_first_image;
        double completed_first_image_duration_ms = 0.0;

        {
            std::lock_guard<std::mutex> lock(worker_shared_state.mutex);
            worker_busy = worker_shared_state.busy;
            worker_activity = worker_shared_state.activity;
            worker_status = worker_shared_state.status_text.empty() ? "idle" : worker_shared_state.status_text;

            if(worker_shared_state.result_ready && worker_shared_state.stop_requested.load()) {
                worker_shared_state.result_ready=false;
                worker_shared_state.error_ready=true;
                worker_shared_state.error_text=TurnCancelledText(worker_shared_state.language);
            }
            if (worker_shared_state.result_ready) {
                std::swap(completed_session_state, worker_shared_state.session_state);
                std::swap(completed_turn_result, worker_shared_state.turn_result);
                completed_first_image.swap(worker_shared_state.pixels);
                completed_first_image_duration_ms = worker_shared_state.first_image_render_duration_ms;
                worker_shared_state.result_ready = false;
                turn_result_ready = true;
            } else if (worker_shared_state.error_ready) {
                failure_text = worker_shared_state.error_text;
                completed_turn_result = worker_shared_state.turn_result;
                worker_shared_state.error_ready = false;
                turn_failed = true;
                ui_message = current_session_state.language == kGameLanguageEnglish
                    ? failure_text
                    : UiText(
                        current_session_state.language,
                        "The turn failed. Check the diagnostic output.",
                        "Le tour a échoué. Consultez la sortie de diagnostic.",
                        "Turen mislyktes. Se diagnoseutdataene.",
                        "Turen mislykkedes. Se diagnosticeringsoutputtet.",
                        "Der Zug ist fehlgeschlagen. Sieh in die Diagnoseausgabe.",
                        "Il turno non è riuscito. Consulta l'output diagnostico.");
            }
        }

        if (turn_result_ready && completed_turn_result.view_unchanged) {
            current_session_state = std::move(completed_session_state);
            last_turn_result = std::move(completed_turn_result);
            have_last_turn = true;
            pending_command.clear();
            turn_completed = true;
            archive_ok = archive.Turn(current_session_state,last_turn_result,{}) && archive_ok;
            ui_message.clear();
            fprintf(stdout,"[animated-view] reused generation=%llu; read-only turn, render_ms=0\n",
                static_cast<unsigned long long>(active_animated_view.generation_id));
            fflush(stdout);
        } else if (turn_result_ready) {
            AnimatedView replacement_view;
            ++next_generation_id;
            if (!InitializeAnimatedView(
                    &replacement_view,
                    next_generation_id,
                    completed_turn_result.rendered_scene,
                    config.animated_view_config,
                    &completed_first_image,
                    completed_first_image_duration_ms)) {
                turn_failed = true;
                failure_text = "Cannot initialize the replacement animated view.";
                ui_message = current_session_state.language == kGameLanguageEnglish
                    ? failure_text
                    : UiText(
                        current_session_state.language,
                        "The new view could not be initialized.",
                        "La nouvelle vue n'a pas pu être initialisée.",
                        "Den nye visningen kunne ikke initialiseres.",
                        "Den nye visning kunne ikke initialiseres.",
                        "Die neue Ansicht konnte nicht initialisiert werden.",
                        "Non è stato possibile inizializzare la nuova vista.");
            } else {
                animation_shared_state.active_generation_id.store(replacement_view.generation_id);
                animation_shared_state.stop_requested.store(true);
                active_animated_view = std::move(replacement_view);
                current_session_state = std::move(completed_session_state);
                last_turn_result = std::move(completed_turn_result);
                have_last_turn = true;
                pending_command.clear();
                playback_last_ticks = SDL_GetTicks();
                UploadSceneTexture(
                    active_animated_view.images[0].pixels,
                    config.render_config.width,
                    config.render_config.height,
                    &texture_rgba_pixels,
                    scene_texture);
                uploaded_generation_id = active_animated_view.generation_id;
                uploaded_image_index = 0;
                turn_completed = true;
                archive_ok = archive.Turn(current_session_state,last_turn_result,
                    active_animated_view.images[0].pixels) && archive_ok;
                ui_message.clear();
                fprintf(
                    stdout,
                    "[animated-view] active generation=%llu swapped on image=0 duration_ms=%.2f\n",
                    static_cast<unsigned long long>(active_animated_view.generation_id),
                    completed_first_image_duration_ms);
                fflush(stdout);
            }
        }

        if (turn_failed && archive.Pending())
            archive_ok = archive.Failure(failure_text,completed_turn_result) && archive_ok;
        if (!worker_joined && !worker_busy && worker_thread.joinable()) {
            worker_thread.join();
            worker_joined = true;
        }

        std::vector<AnimationFramePublication> animation_publications;
        bool animation_worker_busy = false;
        uint64_t animation_worker_generation_id = 0;
        int animation_image_being_rendered = -1;
        bool animation_completion_ready = false;
        uint64_t animation_completion_generation_id = 0;
        double animation_total_generation_duration_ms = 0.0;
        bool animation_failure_ready = false;
        uint64_t animation_failure_generation_id = 0;
        std::string animation_failure_text;
        {
            std::lock_guard<std::mutex> lock(animation_shared_state.mutex);
            animation_worker_busy = animation_shared_state.busy;
            animation_worker_generation_id = animation_shared_state.worker_generation_id;
            animation_image_being_rendered = animation_shared_state.image_being_rendered;
            animation_publications.swap(animation_shared_state.completed_frames);
            if (animation_shared_state.completion_ready) {
                animation_completion_ready = true;
                animation_completion_generation_id = animation_shared_state.completion_generation_id;
                animation_total_generation_duration_ms = animation_shared_state.total_generation_duration_ms;
                animation_shared_state.completion_ready = false;
            }
            if (animation_shared_state.failure_ready) {
                animation_failure_ready = true;
                animation_failure_generation_id = animation_shared_state.failure_generation_id;
                animation_failure_text = animation_shared_state.failure_text;
                animation_shared_state.failure_ready = false;
            }
        }

        active_animated_view.image_being_rendered =
            animation_worker_generation_id == active_animated_view.generation_id
            ? animation_image_being_rendered
            : -1;
        for (size_t publication_index = 0; publication_index < animation_publications.size(); ++publication_index) {
            AnimationFramePublication& publication = animation_publications[publication_index];
            if (publication.generation_id != active_animated_view.generation_id) {
                fprintf(
                    stdout,
                    "[animated-view] active=%llu stale publication generation=%llu image=%d discarded\n",
                    static_cast<unsigned long long>(active_animated_view.generation_id),
                    static_cast<unsigned long long>(publication.generation_id),
                    publication.image_index);
                continue;
            }

            if (!AppendAnimatedViewImage(
                    &active_animated_view,
                    publication.generation_id,
                    publication.image_index,
                    &publication.pixels,
                    publication.render_duration_ms)) {
                active_animated_view.generation_failed = true;
                animation_failure_ready = true;
                animation_failure_generation_id = active_animated_view.generation_id;
                animation_failure_text = "Non-contiguous animated-view publication rejected.";
                break;
            }

            fprintf(
                stdout,
                "[animated-view] generation=%llu available=%d memory_bytes=%zu\n",
                static_cast<unsigned long long>(active_animated_view.generation_id),
                active_animated_view.available_image_count,
                AnimatedViewImageMemoryBytes(active_animated_view));
            fflush(stdout);
        }

        if (animation_completion_ready &&
            animation_completion_generation_id == active_animated_view.generation_id) {
            active_animated_view.generation_complete = true;
            active_animated_view.image_being_rendered = -1;
            active_animated_view.total_generation_duration_ms = animation_total_generation_duration_ms;
        }
        if (animation_failure_ready &&
            animation_failure_generation_id == active_animated_view.generation_id) {
            active_animated_view.generation_failed = true;
            active_animated_view.image_being_rendered = -1;
            ui_message = current_session_state.language == kGameLanguageEnglish
                ? (animation_failure_text.empty()
                    ? "Animation generation stopped; available images remain playable."
                    : animation_failure_text)
                : UiText(
                    current_session_state.language,
                    "Animation generation stopped; available images remain playable.",
                    "L'animation s'est arrêtée ; les images disponibles restent utilisables.",
                    "Animasjonen stoppet; de tilgjengelige bildene kan fortsatt brukes.",
                    "Animationen stoppede; de tilgængelige billeder kan stadig bruges.",
                    "Die Animation wurde angehalten; die verfügbaren Bilder bleiben nutzbar.",
                    "L'animazione si è interrotta; le immagini disponibili restano utilizzabili.");
        }

        if (!animation_worker_busy && animation_worker_thread.joinable()) {
            animation_worker_thread.join();
        }
        if (!animation_worker_thread.joinable() &&
            !active_animated_view.generation_complete &&
            !active_animated_view.generation_failed &&
            active_animated_view.available_image_count < kAnimatedViewImageCapacity) {
            StartAnimationWorker(
                config.render_config,
                config.animated_view_config,
                config.post_process_config,
                active_animated_view,
                &animation_shared_state,
                &animation_worker_thread);
        }

        if (turn_completed) {
            if(audio.Active()&&last_turn_result.audio_mix.valid) {
                archive_ok=archive.AudioEvent(audio.SnapshotJson("before_target"))&&archive_ok;
                audio.SetTargets(current_session_state.audio_targets,config.audio_ramp_seconds);
                auto audio_event=nlohmann::json::parse(audio.SnapshotJson("target_committed"));
                audio_event["ramp_seconds"]=config.audio_ramp_seconds;
                archive_ok=archive.AudioEvent(audio_event.dump())&&archive_ok;
            }
            persistent_hint.clear();
        } else if (turn_failed) {
            persistent_hint.clear();
        }

        const Uint64 playback_now_ticks = SDL_GetTicks();
        const double playback_elapsed_seconds =
            static_cast<double>(playback_now_ticks - playback_last_ticks) / 1000.0;
        playback_last_ticks = playback_now_ticks;
        AdvanceAnimatedViewPlayback(
            &active_animated_view,
            playback_elapsed_seconds,
            config.animated_view_config.playback_images_per_second);
        if (active_animated_view.displayed_image_index >= 0 &&
            active_animated_view.displayed_image_index < active_animated_view.available_image_count &&
            (uploaded_generation_id != active_animated_view.generation_id ||
                uploaded_image_index != active_animated_view.displayed_image_index)) {
            UploadSceneTexture(
                active_animated_view.images[static_cast<size_t>(active_animated_view.displayed_image_index)].pixels,
                config.render_config.width,
                config.render_config.height,
                &texture_rgba_pixels,
                scene_texture);
            uploaded_generation_id = active_animated_view.generation_id;
            uploaded_image_index = active_animated_view.displayed_image_index;
            if (config.animated_view_config.debug_logging) {
                fprintf(
                    stdout,
                    "[animated-view] generation=%llu displayed=%d direction=%d available=%d rendering=%d\n",
                    static_cast<unsigned long long>(active_animated_view.generation_id),
                    active_animated_view.displayed_image_index,
                    active_animated_view.playback_direction,
                    active_animated_view.available_image_count,
                    active_animated_view.image_being_rendered);
                fflush(stdout);
            }
        }

        const std::string status_line = BuildStatusLine(
            current_session_state,
            worker_activity,
            worker_status.empty() ? "idle" : worker_status,
            last_turn_result,
            have_last_turn,
            worker_busy,
            SDL_GetTicks());

        const std::string display_message = !ui_message.empty() ? ui_message :
            (worker_busy && config.turn_config.dynamic_scenes ? worker_status : persistent_hint);
        BuildTranscriptLines(
            current_session_state,
            pending_command,
            display_message,
            ui_fonts,
            console_wrap_width,
            &transcript_lines);
        if(turn_completed&&(last_turn_result.turn_result.intent=="review_investigation"||
                            last_turn_result.turn_result.intent=="read_field_notes")) {
            // Start a long dossier at the latest command, not at its final lines.
            size_t first=transcript_lines.size();
            while(first>0&&!transcript_lines[first-1].command_line)--first;
            while(first>0&&transcript_lines[first-1].command_line)--first;
            console_scroll=static_cast<int>(transcript_lines.size()>first+console_line_capacity?
                transcript_lines.size()-first-console_line_capacity:0);
        }
        console_scroll=std::clamp(console_scroll,0,static_cast<int>(transcript_lines.size()>console_line_capacity?
            transcript_lines.size()-console_line_capacity:0));

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
        SDL_RenderTexture(renderer, visor_texture, 0, &scene_rect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderRect(renderer, &scene_frame);
        DrawThermalHud(renderer, ui_fonts, current_session_state, scene_rect);
        DrawExitCompass(renderer, ui_fonts, current_session_state, scene_rect);
        DrawConsoleText(renderer, ui_fonts, console_rect, transcript_lines, console_line_capacity,static_cast<size_t>(console_scroll));

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
    animation_shared_state.stop_requested.store(true);
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    if (animation_worker_thread.joinable()) {
        animation_worker_thread.join();
    }

    if (final_session_state) {
        *final_session_state = current_session_state;
    }
    if (archive.Pending())
        archive_ok = archive.Failure("Closed during turn; not committed.",worker_shared_state.turn_result) && archive_ok;
    if(audio.Active())archive_ok=archive.AudioEvent(audio.SnapshotJson("stopped"))&&archive_ok;
    audio.Close();
    archive_ok = archive.Close(current_session_state) && archive_ok;

    SDL_DestroyTexture(visor_texture);
    SDL_DestroyTexture(scene_texture);
    SDL_StopTextInput(window);
    DestroyUiFonts(&ui_fonts);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    success = archive_ok;
    if (!success) SetError(error_buffer,error_buffer_size,archive.Error().c_str());
    return success;
}

}  // namespace liminal
