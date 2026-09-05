#pragma once

#include "motion_bridge/types.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace motion_bridge {

class MotionEngine {
public:
    explicit MotionEngine(ContactConfig contact = {}, SafetyConfig safety = {});

    void set_contact_config(ContactConfig contact);
    void set_axis_tuning(std::array<AxisTuning, 6> tuning);
    void set_axis_return_position(std::size_t axis, double position);
    void set_axis_travel_preference(std::size_t axis, PreferredTravelConfig config);
    void reset_axis_travel_learning(std::size_t axis);
    [[nodiscard]] const ContactConfig& contact_config() const noexcept;
    [[nodiscard]] const std::array<AxisTuning, 6>& axis_tuning() const noexcept;
    [[nodiscard]] const std::array<PreferredTravelConfig, 6>& axis_travel_preferences() const noexcept;

    [[nodiscard]] EngineSnapshot process(const MotionFrame& frame);
    [[nodiscard]] EngineSnapshot process_missing(std::chrono::microseconds now);

private:
    [[nodiscard]] std::optional<EngineSnapshot> calculate(const MotionFrame& frame);
    [[nodiscard]] Axes tune(const Axes& raw, const std::array<bool, 6>& active_axes, const std::string& binding_key);
    [[nodiscard]] EngineSnapshot apply_safety(EngineSnapshot next, std::chrono::microseconds now);
    [[nodiscard]] double optimize_axis(std::size_t axis, double value, const std::string& profile_key, std::chrono::microseconds now);
    void reset_axis_travel_live_state(std::size_t axis, const std::string& profile_key);
    void record_axis_turning_point(std::size_t axis, double value, std::chrono::microseconds now);

    struct PreferredTravelProfile {
        double center{0.5};
        double travel{};
    };

    struct PreferredTravelRuntime {
        PreferredTravelStatus status;
        std::unordered_map<std::string, PreferredTravelProfile> cache;
        std::string profile_key;
        std::optional<PreferredTravelProfile> profile;
        std::optional<std::chrono::microseconds> transition_at;
        bool has_sample{};
        double anchor{};
        double extremum{};
        int direction{};
        std::optional<double> last_turning_point;
        std::vector<std::pair<double, double>> half_strokes;
        std::optional<double> optimized_center;
    };

    ContactConfig contact_;
    SafetyConfig safety_;
    std::array<AxisTuning, 6> tuning_{};
    std::optional<EngineSnapshot> last_valid_;
    std::chrono::microseconds last_valid_time_{};
    std::string angle_binding_key_;
    std::optional<double> twist_baseline_;
    std::optional<double> roll_baseline_;
    std::optional<double> pitch_baseline_;
    // Gain expands an animation about its observed neutral position, rather
    // than the arbitrary global 0.5.  The envelope is reset only when the
    // reference/target/action binding changes, so it cannot drift while a
    // loop is playing.
    std::string gain_binding_key_;
    std::array<double, 6> gain_min_{};
    std::array<double, 6> gain_max_{};
    std::array<bool, 6> gain_envelope_valid_{};
    std::array<PreferredTravelConfig, 6> travel_configs_{};
    std::array<PreferredTravelRuntime, 6> travel_runtime_{};
};

[[nodiscard]] const char* to_string(MotionState state) noexcept;
[[nodiscard]] const char* to_string(PreferredTravelState state) noexcept;
[[nodiscard]] double clamp01(double value) noexcept;

} // namespace motion_bridge
