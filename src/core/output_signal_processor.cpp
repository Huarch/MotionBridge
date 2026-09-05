#include "motion_bridge/output_signal_processor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace motion_bridge {
namespace {

constexpr double kMinimumSmartLimitInputSeparation = 0.01;

[[nodiscard]] double smoothstep(const double value) {
    const auto clamped = std::clamp(value, 0.0, 1.0);
    return clamped * clamped * (3.0 - 2.0 * clamped);
}

[[nodiscard]] double seconds_between(
    const std::chrono::microseconds later,
    const std::chrono::microseconds earlier) {
    return std::max(0.0, static_cast<double>((later - earlier).count()) / 1'000'000.0);
}

void normalize_smart_limit_curve(AxisSmartLimit& smart_limit) {
    smart_limit.input_axis = std::min(smart_limit.input_axis, std::size_t{5});
    smart_limit.lower_input = std::clamp(smart_limit.lower_input, 0.0, 1.0);
    smart_limit.lower_factor = std::clamp(smart_limit.lower_factor, 0.0, 1.0);
    smart_limit.upper_input = std::clamp(smart_limit.upper_input, 0.0, 1.0);
    smart_limit.upper_factor = std::clamp(smart_limit.upper_factor, 0.0, 1.0);

    if (smart_limit.lower_input > smart_limit.upper_input) {
        std::swap(smart_limit.lower_input, smart_limit.upper_input);
        std::swap(smart_limit.lower_factor, smart_limit.upper_factor);
    }

    if (smart_limit.upper_input - smart_limit.lower_input < kMinimumSmartLimitInputSeparation) {
        const auto midpoint = (smart_limit.lower_input + smart_limit.upper_input) * 0.5;
        smart_limit.lower_input = std::clamp(midpoint - kMinimumSmartLimitInputSeparation * 0.5,
                                             0.0,
                                             1.0 - kMinimumSmartLimitInputSeparation);
        smart_limit.upper_input = smart_limit.lower_input + kMinimumSmartLimitInputSeparation;
    }
}

[[nodiscard]] double smart_limit_factor(const AxisSmartLimit& smart_limit, const double input) {
    if (input <= smart_limit.lower_input) return smart_limit.lower_factor;
    if (input >= smart_limit.upper_input) return smart_limit.upper_factor;
    const auto position = (input - smart_limit.lower_input) /
                          (smart_limit.upper_input - smart_limit.lower_input);
    return smart_limit.lower_factor + (smart_limit.upper_factor - smart_limit.lower_factor) * position;
}

} // namespace

OutputSignalProcessor::OutputSignalProcessor(OutputSignalConfig config) { set_config(std::move(config)); }

void OutputSignalProcessor::set_config(OutputSignalConfig config) {
    config.soft_start_for = std::clamp(config.soft_start_for, std::chrono::milliseconds{0}, std::chrono::milliseconds{3000});
    for (std::size_t index = 0; index < config.max_speed_per_second.size(); ++index) {
        config.axis_return_position[index] = std::clamp(config.axis_return_position[index], 0.0, 1.0);
        config.max_speed_per_second[index] = std::clamp(config.max_speed_per_second[index], 0.25, 10.0);
        auto& smart_limit = config.smart_limit[index];
        smart_limit.target_value = std::clamp(smart_limit.target_value, 0.0, 1.0);
        normalize_smart_limit_curve(smart_limit);
    }
    config_ = std::move(config);
}

const OutputSignalConfig& OutputSignalProcessor::config() const noexcept { return config_; }

void OutputSignalProcessor::arm(const std::chrono::microseconds now) {
    for (std::size_t index = 0; index < current_.values.size(); ++index) {
        current_[index] = config_.axis_return_position[index];
        smart_limit_inputs_[index] = config_.axis_return_position[index];
    }
    armed_ = true;
    live_started_ = false;
    live_started_at_ = {};
    last_process_at_ = now;
}

void OutputSignalProcessor::disarm() {
    for (std::size_t index = 0; index < current_.values.size(); ++index) {
        current_[index] = config_.axis_return_position[index];
        smart_limit_inputs_[index] = config_.axis_return_position[index];
    }
    armed_ = false;
    live_started_ = false;
    live_started_at_ = {};
    last_process_at_ = {};
}

Axes OutputSignalProcessor::process(
    const Axes& target,
    const std::chrono::microseconds now,
    const bool live_motion) {
    if (!armed_) return target;

    // Before the first usable frame the hardware remains centred. Once live
    // motion has started, Holding is also treated as live by the caller.
    if (!live_started_) {
        if (!live_motion) {
            last_process_at_ = now;
            return current_;
        }
        live_started_ = true;
        live_started_at_ = now;
        last_process_at_ = now;
    }

    // Engine safety owns stream-loss returning. Do not let a user speed limit
    // stretch its established 600 ms return or interfere with emergency stop.
    if (!live_motion) {
        current_ = target;
        for (std::size_t index = 0; index < current_.values.size(); ++index) {
            if (!config_.axis_output_enabled[index]) current_[index] = config_.axis_return_position[index];
        }
        smart_limit_inputs_ = current_;
        last_process_at_ = now;
        return current_;
    }

    auto candidate = target;
    if (config_.soft_start_enabled && config_.soft_start_for.count() > 0) {
        const auto elapsed_ms = static_cast<double>((now - live_started_at_).count()) / 1000.0;
        const auto progress = smoothstep(elapsed_ms / static_cast<double>(config_.soft_start_for.count()));
        for (std::size_t index = 0; index < candidate.values.size(); ++index) {
            const auto return_position = config_.axis_return_position[index];
            candidate[index] = return_position + (target[index] - return_position) * progress;
        }
    }

    // Disabled driver axes expose their return position to dependent smart limits.
    for (std::size_t index = 0; index < candidate.values.size(); ++index) {
        if (!config_.axis_output_enabled[index]) {
            candidate[index] = config_.axis_return_position[index];
        }
    }

    // Every target axis reads its selected driver from one shared snapshot,
    // so processing order cannot make cross-axis limits influence each other.
    const auto smart_limit_inputs = candidate;
    smart_limit_inputs_ = smart_limit_inputs;
    for (std::size_t index = 0; index < candidate.values.size(); ++index) {
        if (!config_.axis_output_enabled[index]) continue;
        const auto& smart_limit = config_.smart_limit[index];
        if (!smart_limit.enabled) continue;
        const auto input = std::clamp(smart_limit_inputs[smart_limit.input_axis], 0.0, 1.0);
        const auto factor = std::clamp(smart_limit_factor(smart_limit, input), 0.0, 1.0);
        if (smart_limit.mode == SmartLimitMode::Value) {
            candidate[index] = smart_limit.target_value +
                               (candidate[index] - smart_limit.target_value) * factor;
        } else {
            candidate[index] = current_[index] + (candidate[index] - current_[index]) * std::pow(factor, 4.0);
        }
    }

    // A delayed event loop must not turn one late tick into an unlimited jump.
    // 100 ms still lets enabled axes catch up without freezing.
    const auto elapsed = std::min(seconds_between(now, last_process_at_), 0.1);
    for (std::size_t index = 0; index < candidate.values.size(); ++index) {
        if (config_.speed_limit_enabled[index]) {
            const auto maximum_delta = config_.max_speed_per_second[index] * elapsed;
            candidate[index] = std::clamp(candidate[index], current_[index] - maximum_delta, current_[index] + maximum_delta);
        }
    }

    for (auto& value : candidate.values) value = std::clamp(value, 0.0, 1.0);
    current_ = candidate;
    last_process_at_ = now;
    return current_;
}

const Axes& OutputSignalProcessor::current() const noexcept { return current_; }
const Axes& OutputSignalProcessor::smart_limit_inputs() const noexcept { return smart_limit_inputs_; }

} // namespace motion_bridge
