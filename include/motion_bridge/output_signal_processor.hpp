#pragma once

#include "motion_bridge/types.hpp"

#include <array>
#include <chrono>

namespace motion_bridge {

enum class SmartLimitMode { Value, Speed };

struct AxisSmartLimit {
    bool enabled{};
    std::size_t input_axis{};
    SmartLimitMode mode{SmartLimitMode::Value};
    double target_value{0.5};
    // X is the selected driver axis and Y is the motion retained by this axis.
    // Outside the two control points the nearest endpoint factor is held.
    double lower_input{0.25};
    double lower_factor{1.0};
    double upper_input{0.9};
    double upper_factor{};
};

struct OutputSignalConfig {
    bool soft_start_enabled{true};
    std::chrono::milliseconds soft_start_for{600};
    std::array<bool, 6> axis_output_enabled{true, true, true, true, true, true};
    std::array<double, 6> axis_return_position{0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    std::array<AxisSmartLimit, 6> smart_limit;
    std::array<bool, 6> speed_limit_enabled{};
    std::array<double, 6> max_speed_per_second{4.0, 4.0, 4.0, 4.0, 4.0, 4.0};
};

// Converts the tuned device target into the value that is actually sent.
// It deliberately lives after MotionEngine so output protection never changes
// game-space contact calculations, participant routing, gain or range tuning.
class OutputSignalProcessor {
public:
    explicit OutputSignalProcessor(OutputSignalConfig config = {});

    void set_config(OutputSignalConfig config);
    [[nodiscard]] const OutputSignalConfig& config() const noexcept;

    // Every arm begins at each axis' return position. Soft start begins only
    // after the first live target arrives.
    void arm(std::chrono::microseconds now);
    void disarm();

    [[nodiscard]] Axes process(const Axes& target, std::chrono::microseconds now, bool live_motion);
    [[nodiscard]] const Axes& current() const noexcept;
    [[nodiscard]] const Axes& smart_limit_inputs() const noexcept;

private:
    OutputSignalConfig config_;
    Axes current_;
    Axes smart_limit_inputs_;
    bool armed_{};
    bool live_started_{};
    std::chrono::microseconds live_started_at_{};
    std::chrono::microseconds last_process_at_{};
};

} // namespace motion_bridge
