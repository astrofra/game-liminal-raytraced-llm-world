#include "place_proposal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <string>
#include <utility>

#include "nlohmann/json.hpp"

namespace {

bool SetFailure(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
    return false;
}

bool IsKnownDirection(const std::string& direction) {
    return direction == "north" || direction == "east" || direction == "south" ||
           direction == "west";
}

bool RequireExactKeys(const nlohmann::json& object,
                      std::initializer_list<const char*> keys,
                      std::string* error,
                      const char* description) {
    if (!object.is_object()) {
        return SetFailure(error, std::string(description) + " must be an object");
    }
    if (object.size() != keys.size()) {
        return SetFailure(error,
                          std::string(description) + " must contain exactly the required fields");
    }
    for (const char* key : keys) {
        if (object.find(key) == object.end()) {
            return SetFailure(error, std::string(description) + " is missing field: " + key);
        }
    }
    return true;
}

bool ReadRequiredString(const nlohmann::json& object, const char* key,
                        std::size_t max_bytes, std::string* result,
                        std::string* error) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return SetFailure(error, std::string("missing field: ") + key);
    }
    if (!it->is_string()) {
        return SetFailure(error, std::string(key) + " must be a string");
    }
    const std::string value = it->get<std::string>();
    if (value.empty()) {
        return SetFailure(error, std::string(key) + " must not be empty");
    }
    if (value.size() > max_bytes) {
        return SetFailure(error, std::string(key) + " exceeds its byte limit");
    }
    *result = value;
    return true;
}

bool ReadExternalTemperature(const nlohmann::json& value, int* result,
                             std::string* error) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return SetFailure(error, "thermal.external_temperature_c must be an integer");
    }

    long long number = 0;
    if (value.is_number_unsigned()) {
        const unsigned long long unsigned_number = value.get<unsigned long long>();
        if (unsigned_number > 520ULL) {
            return SetFailure(error,
                              "thermal.external_temperature_c is outside 300..520");
        }
        number = static_cast<long long>(unsigned_number);
    } else {
        number = value.get<long long>();
    }

    if (number < 300 || number > 520) {
        return SetFailure(error, "thermal.external_temperature_c is outside 300..520");
    }
    *result = static_cast<int>(number);
    return true;
}

bool ReadBodyTemperature(const nlohmann::json& value, double* result,
                         std::string* error) {
    if (!value.is_number()) {
        return SetFailure(error, "thermal.body_temperature_c must be a number");
    }
    const double number = value.get<double>();
    if (!std::isfinite(number)) {
        return SetFailure(error, "thermal.body_temperature_c must be finite");
    }
    if (number < 34.0 || number > 43.0) {
        return SetFailure(error, "thermal.body_temperature_c is outside 34..43");
    }
    *result = number;
    return true;
}

class ProposalSax final : public nlohmann::json_sax<nlohmann::json> {
public:
    bool null() override {
        return AddValue(nlohmann::json(nullptr));
    }

    bool boolean(bool value) override {
        return AddValue(nlohmann::json(value));
    }

    bool number_integer(nlohmann::json::number_integer_t value) override {
        return AddValue(nlohmann::json(value));
    }

    bool number_unsigned(nlohmann::json::number_unsigned_t value) override {
        return AddValue(nlohmann::json(value));
    }

    bool number_float(nlohmann::json::number_float_t value,
                      const nlohmann::json::string_t&) override {
        return AddValue(nlohmann::json(value));
    }

    bool string(nlohmann::json::string_t& value) override {
        return AddValue(nlohmann::json(value));
    }

    bool binary(nlohmann::json::binary_t&) override {
        return Fail("binary values are not valid JSON text");
    }

    bool start_object(std::size_t) override {
        if (frames_.size() >= 4) return Fail("JSON nesting exceeds proposal contract");
        if (frames_.empty() && has_root_) {
            return Fail("multiple JSON root values");
        }
        Frame frame;
        frame.value = nlohmann::json::object();
        frame.is_object = true;
        frames_.push_back(std::move(frame));
        return true;
    }

    bool key(nlohmann::json::string_t& value) override {
        if (frames_.empty() || !frames_.back().is_object) {
            return Fail("JSON object key outside an object");
        }
        Frame& frame = frames_.back();
        if (frame.has_pending_key) {
            return Fail("JSON object key without a value");
        }
        if (frame.value.find(value) != frame.value.end()) {
            return Fail("duplicate JSON key: " + value);
        }
        frame.pending_key = value;
        frame.has_pending_key = true;
        return true;
    }

    bool end_object() override {
        if (frames_.empty() || !frames_.back().is_object) {
            return Fail("unexpected end of JSON object");
        }
        if (frames_.back().has_pending_key) {
            return Fail("JSON object key without a value");
        }
        nlohmann::json completed = std::move(frames_.back().value);
        frames_.pop_back();
        return AddValue(std::move(completed));
    }

    bool start_array(std::size_t) override {
        if (frames_.size() >= 4) return Fail("JSON nesting exceeds proposal contract");
        if (frames_.empty() && has_root_) {
            return Fail("multiple JSON root values");
        }
        Frame frame;
        frame.value = nlohmann::json::array();
        frame.is_object = false;
        frames_.push_back(std::move(frame));
        return true;
    }

    bool end_array() override {
        if (frames_.empty() || frames_.back().is_object) {
            return Fail("unexpected end of JSON array");
        }
        nlohmann::json completed = std::move(frames_.back().value);
        frames_.pop_back();
        return AddValue(std::move(completed));
    }

    bool parse_error(std::size_t, const std::string&,
                     const nlohmann::detail::exception& exception) override {
        return Fail(std::string("invalid JSON: ") + exception.what());
    }

    bool has_root() const {
        return has_root_;
    }

    const nlohmann::json& root() const {
        return root_;
    }

    const std::string& error() const {
        return error_;
    }

private:
    struct Frame {
        nlohmann::json value;
        bool is_object = false;
        bool has_pending_key = false;
        std::string pending_key;
    };

    bool AddValue(nlohmann::json value) {
        if (frames_.empty()) {
            if (has_root_) {
                return Fail("multiple JSON root values");
            }
            root_ = std::move(value);
            has_root_ = true;
            return true;
        }

        Frame& frame = frames_.back();
        if (frame.is_object) {
            if (!frame.has_pending_key) {
                return Fail("JSON object value without a key");
            }
            frame.value[frame.pending_key] = std::move(value);
            frame.has_pending_key = false;
            frame.pending_key.clear();
        } else {
            frame.value.push_back(std::move(value));
        }
        return true;
    }

    bool Fail(std::string message) {
        if (error_.empty()) {
            error_ = std::move(message);
        }
        return false;
    }

    std::vector<Frame> frames_;
    nlohmann::json root_;
    bool has_root_ = false;
    std::string error_;
};

}  // namespace

bool ParsePlaceProposal(const std::string& text, int current_external, float current_body,
    const std::map<std::string, bool>& fixed_exits,
    const std::vector<std::string>& possible_exits,
    PlaceProposal* output, std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    if (output == nullptr) {
        return SetFailure(error, "output must not be null");
    }
    if (!std::isfinite(current_body)) {
        return SetFailure(error, "current_body must be finite");
    }

    for (const auto& entry : fixed_exits) {
        if (!IsKnownDirection(entry.first)) {
            return SetFailure(error, "unknown fixed exit direction: " + entry.first);
        }
    }
    for (const auto& direction : possible_exits) {
        if (!IsKnownDirection(direction)) {
            return SetFailure(error, "unknown possible exit direction: " + direction);
        }
    }

    if (text.size() > 16384) return SetFailure(error, "proposal exceeds 16384 bytes");
    ProposalSax sax;
    bool parsed = false;
    try {
        parsed = nlohmann::json::sax_parse(text, &sax);
    } catch (const std::exception& exception) {
        return SetFailure(error, std::string("invalid JSON: ") + exception.what());
    } catch (...) {
        return SetFailure(error, "invalid JSON");
    }

    if (!parsed) {
        return SetFailure(error, sax.error().empty() ? std::string("invalid JSON") : sax.error());
    }
    if (!sax.has_root()) {
        return SetFailure(error, "missing JSON root value");
    }

    nlohmann::json root = sax.root();
    // Audio is independently validated and cannot reject a mechanical proposal.
    if(root.is_object())root.erase("audio_mix");
    if (!RequireExactKeys(root, {"title", "summary", "narration", "thermal", "exits"},
                          error, "root object")) {
        return false;
    }

    PlaceProposal result;
    if (!ReadRequiredString(root, "title", 300, &result.title, error) ||
        !ReadRequiredString(root, "summary", 2400, &result.summary, error) ||
        !ReadRequiredString(root, "narration", 6000, &result.narration, error)) {
        return false;
    }

    const auto thermal_it = root.find("thermal");
    if (thermal_it == root.end() ||
        !RequireExactKeys(*thermal_it,
                          {"external_temperature_c", "body_temperature_c", "reason"},
                          error, "thermal")) {
        return false;
    }
    const nlohmann::json& thermal = *thermal_it;

    int external_temperature = 0;
    double body_temperature = 0.0;
    const auto external_it = thermal.find("external_temperature_c");
    const auto body_it = thermal.find("body_temperature_c");
    if (external_it == thermal.end() || body_it == thermal.end()) {
        return SetFailure(error, "thermal is missing a temperature field");
    }
    if (!ReadExternalTemperature(*external_it, &external_temperature, error) ||
        !ReadBodyTemperature(*body_it, &body_temperature, error) ||
        !ReadRequiredString(thermal, "reason", 1200, &result.thermal_reason, error)) {
        return false;
    }

    const long long external_delta =
        static_cast<long long>(external_temperature) -
        static_cast<long long>(current_external);
    if (external_delta < -20 || external_delta > 20) {
        return SetFailure(error,
                          "thermal.external_temperature_c varies by more than 20 degrees");
    }

    constexpr double kBodyVariationLimit = 0.4001;
    constexpr double kBodyVariationEpsilon = 1e-9;
    if (std::fabs(body_temperature - static_cast<double>(current_body)) >
        kBodyVariationLimit + kBodyVariationEpsilon) {
        return SetFailure(error,
                          "thermal.body_temperature_c varies by more than 0.4001 degrees");
    }

    result.external_temperature_c = external_temperature;
    result.body_temperature_c = static_cast<float>(body_temperature);

    const auto exits_it = root.find("exits");
    if (exits_it == root.end() ||
        !RequireExactKeys(*exits_it, {"north", "east", "south", "west"},
                          error, "exits")) {
        return false;
    }
    const nlohmann::json& exits = *exits_it;

    const char* const directions[] = {"north", "east", "south", "west"};
    for (const char* direction : directions) {
        const auto exit_it = exits.find(direction);
        if (exit_it == exits.end()) {
            return SetFailure(error, std::string("exits is missing field: ") + direction);
        }
        if (!exit_it->is_boolean()) {
            return SetFailure(error, std::string("exits.") + direction + " must be a boolean");
        }
        result.exits[direction] = exit_it->get<bool>();
    }

    for (const char* direction : directions) {
        const bool proposed = result.exits[direction];
        if (proposed &&
            std::find(possible_exits.begin(), possible_exits.end(), direction) ==
                possible_exits.end()) {
            return SetFailure(error,
                              std::string("exit is not geometrically possible: ") + direction);
        }

        const auto fixed_it = fixed_exits.find(direction);
        if (fixed_it != fixed_exits.end() && fixed_it->second != proposed) {
            return SetFailure(error,
                              std::string("exit conflicts with a fixed exit: ") + direction);
        }
    }

    *output = std::move(result);
    return true;
}
