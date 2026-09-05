#include "motion_bridge/functional_target_selector.hpp"
#include "motion_bridge/motion_engine.hpp"
#include "motion_bridge/output_signal_processor.hpp"
#include "motion_bridge/tcode.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <numbers>

using namespace motion_bridge;

namespace {

BonePose pose(std::string name, Vec3 position, Quaternion rotation = {}) { return {std::move(name), position, rotation}; }

MotionFrame orthogonal_frame(std::chrono::microseconds time = std::chrono::microseconds{0}) {
    Participant reference{"male", "male", "fallen-doll"};
    reference.bones.emplace("Penis01", pose("Penis01", {0, 0, 0}));
    reference.bones.emplace("Penis02", pose("Penis02", {0, 1, 0}));
    reference.bones.emplace("Penis09", pose("Penis09", {0, 1, 0}));
    reference.bones.emplace("M_Hips", pose("M_Hips", {0, 0, 0}));
    Participant target{"female", "female", "fallen-doll"};
    // Maps the Fallen Doll default target basis (-local_y/+local_z) onto the
    // synthetic reference axis (+Y) and right axis (-X).
    target.bones.emplace("M_Gen", pose("M_Gen", {0, 0.5, 0}, {0, 0.7071067811865476, 0, -0.7071067811865476}));
    return {"motion-frame/v1", "fallen-doll", 1, time, true, "Test", "vaginal", {reference, target}};
}

void require_close(const double actual, const double expected) {
    if (std::abs(actual - expected) >= 1e-6) {
        std::cerr << "require_close failed: actual=" << actual << " expected=" << expected << '\n';
        assert(false);
    }
}

void test_contact_and_tcode() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    auto snapshot = engine.process(frame);
    assert(snapshot.state == MotionState::Active);
    require_close(snapshot.raw_axes[0], 1.0);
    require_close(snapshot.raw_axes[1], 0.5);
    require_close(snapshot.raw_axes[2], 0.5);
    assert(encode_tcode(snapshot.device_axes, std::chrono::milliseconds{20}) == "L09999I020 L15000I020 L25000I020 R05000I020 R15000I020 R25000I020\n");
}

void test_gain_scales_output_travel() {
    MotionEngine engine;
    auto tuning = engine.axis_tuning();
    tuning[0] = {.gain = 2.0};
    engine.set_axis_tuning(tuning);
    auto frame = orthogonal_frame();
    frame.l0_reference_length = true;
    // An action may occupy only one side of the global 0..1 L0 coordinate.
    // Gain must expand its observed travel about that action's neutral point,
    // not clamp both samples against a fixed 0.5 midpoint.
    frame.participants[1].bones["M_Gen"].position.y = 0.2;
    require_close(engine.process(frame).device_axes[0], 0.2);
    frame.participants[1].bones["M_Gen"].position.y = 0.4;
    const auto snapshot = engine.process(frame);
    require_close(snapshot.raw_axes[0], 0.4);
    require_close(snapshot.device_axes[0], 0.5);
}

void test_hold_and_return() {
    MotionEngine engine;
    const auto initial = engine.process(orthogonal_frame(std::chrono::milliseconds{1000}));
    assert(initial.state == MotionState::Active);
    assert(engine.process_missing(std::chrono::milliseconds{1200}).state == MotionState::Holding);
    const auto returning = engine.process_missing(std::chrono::milliseconds{1550});
    assert(returning.state == MotionState::Returning);
    require_close(returning.device_axes[0], 0.75);
    const auto idle = engine.process_missing(std::chrono::milliseconds{1900});
    assert(idle.state == MotionState::Idle);
    require_close(idle.device_axes[0], 0.5);
}

void test_custom_axis_return_position() {
    MotionEngine engine;
    engine.set_axis_return_position(0, 0.2);
    const auto initial = engine.process(orthogonal_frame(std::chrono::milliseconds{1000}));
    require_close(initial.device_axes[0], 1.0);

    const auto returning = engine.process_missing(std::chrono::milliseconds{1550});
    assert(returning.state == MotionState::Returning);
    require_close(returning.device_axes[0], 0.6);
    require_close(returning.raw_axes[0], 0.75);

    const auto idle = engine.process_missing(std::chrono::milliseconds{1900});
    assert(idle.state == MotionState::Idle);
    require_close(idle.device_axes[0], 0.2);
    require_close(idle.raw_axes[0], 0.5);
}

void test_bilateral_contact_uses_reference_depth() {
    MotionEngine engine;
    auto config = engine.contact_config();
    config.target_bone = "R_Foot";
    config.target_secondary_bone = "L_Foot";
    engine.set_contact_config(config);
    auto frame = orthogonal_frame();
    frame.participants[1].bones.erase("M_Gen");
    frame.participants[1].bones.emplace("R_Foot", pose("R_Foot", {0.08, 0.4, 0}));
    frame.participants[1].bones.emplace("L_Foot", pose("L_Foot", {-0.08, 0.4, 0}));
    const auto snapshot = engine.process(frame);
    assert(snapshot.contact.target_mode == "bilateral_reference_axis");
    require_close(snapshot.raw_axes[0], 0.4);
    require_close(snapshot.raw_axes[4], 0.5);
    require_close(snapshot.raw_axes[5], 0.5);
}

void test_axis_inversion_is_independent() {
    MotionEngine engine;
    auto tuning = engine.axis_tuning();
    tuning[1].inverted = true;
    engine.set_axis_tuning(tuning);
    auto frame = orthogonal_frame();
    frame.participants[1].bones["M_Gen"].position.z = 0.075;
    const auto snapshot = engine.process(frame);
    require_close(snapshot.raw_axes[1], 0.25);
    require_close(snapshot.device_axes[1], 0.75);
    require_close(snapshot.device_axes[2], 0.5);
}

void test_tcode_can_send_only_changed_axes_with_real_interval() {
    Axes axes;
    axes[0] = 1.0;
    axes[3] = 0.25;
    AxisMask included{};
    included[0] = true;
    included[3] = true;
    assert(encode_tcode(axes, std::chrono::milliseconds{17}, included) == "L09999I017 R02500I017\n");
    included.fill(false);
    assert(encode_tcode(axes, std::chrono::milliseconds{17}, included).empty());
}

void test_output_soft_start_tracks_actual_command() {
    OutputSignalConfig config;
    config.soft_start_enabled = true;
    config.soft_start_for = std::chrono::milliseconds{600};
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    processor.arm(std::chrono::milliseconds{0});

    require_close(processor.process(target, std::chrono::milliseconds{0}, true)[0], 0.5);
    require_close(processor.process(target, std::chrono::milliseconds{300}, true)[0], 0.75);
    require_close(processor.process(target, std::chrono::milliseconds{600}, true)[0], 1.0);
}

void test_output_speed_limit_is_per_axis_and_time_based() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    config.speed_limit_enabled[0] = true;
    config.speed_limit_enabled[1] = true;
    config.max_speed_per_second[0] = 1.0;
    config.max_speed_per_second[1] = 2.0;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    target[1] = 1.0;
    processor.arm(std::chrono::milliseconds{0});

    const auto first = processor.process(target, std::chrono::milliseconds{0}, true);
    require_close(first[0], 0.5);
    require_close(first[1], 0.5);
    const auto second = processor.process(target, std::chrono::milliseconds{100}, true);
    require_close(second[0], 0.6);
    require_close(second[1], 0.7);
}

void test_output_waits_for_live_motion_and_does_not_delay_safety_return() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    config.speed_limit_enabled.fill(true);
    config.max_speed_per_second.fill(0.25);
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    processor.arm(std::chrono::milliseconds{0});

    require_close(processor.process(target, std::chrono::milliseconds{200}, false)[0], 0.5);
    (void)processor.process(target, std::chrono::milliseconds{200}, true);
    Axes safety_target;
    safety_target[0] = 0.25;
    require_close(processor.process(safety_target, std::chrono::milliseconds{220}, false)[0], 0.25);
}

void test_output_speed_limit_only_affects_enabled_axes() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    config.speed_limit_enabled[0] = true;
    config.max_speed_per_second[0] = 1.0;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    target[1] = 1.0;
    processor.arm(std::chrono::milliseconds{0});

    const auto output = processor.process(target, std::chrono::milliseconds{0}, true);
    require_close(output[0], 0.5);
    require_close(output[1], 1.0);
}

void test_output_axis_disable_moves_only_selected_axis_to_return_position() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    config.axis_output_enabled[0] = false;
    config.axis_return_position[0] = 0.2;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    target[1] = 0.2;
    processor.arm(std::chrono::milliseconds{0});

    const auto output = processor.process(target, std::chrono::milliseconds{0}, true);
    require_close(output[0], 0.2);
    require_close(output[1], 0.2);
}

void test_output_waits_at_custom_return_positions_before_live_motion() {
    OutputSignalConfig config;
    config.axis_return_position[0] = 0.3;
    config.axis_return_position[1] = 0.7;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    target[1] = 0.0;
    processor.arm(std::chrono::milliseconds{0});

    const auto output = processor.process(target, std::chrono::milliseconds{100}, false);
    require_close(output[0], 0.3);
    require_close(output[1], 0.7);
    const auto first_live = processor.process(target, std::chrono::milliseconds{100}, true);
    require_close(first_live[0], 0.3);
    require_close(first_live[1], 0.7);
}

void test_output_axis_disable_obeys_its_speed_limit() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    config.speed_limit_enabled[0] = true;
    config.max_speed_per_second[0] = 1.0;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    processor.arm(std::chrono::milliseconds{0});
    for (int tick = 0; tick <= 5; ++tick) {
        (void)processor.process(target, std::chrono::milliseconds{tick * 100}, true);
    }
    require_close(processor.current()[0], 1.0);

    config.axis_output_enabled[0] = false;
    processor.set_config(config);
    require_close(processor.process(target, std::chrono::milliseconds{600}, true)[0], 0.9);
}

void test_output_disabled_axis_stays_at_return_position_during_stream_return() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    config.axis_output_enabled[0] = false;
    config.axis_return_position[0] = 0.2;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    processor.arm(std::chrono::milliseconds{0});
    require_close(processor.process(target, std::chrono::milliseconds{0}, true)[0], 0.2);

    Axes returning;
    returning[0] = 0.8;
    require_close(processor.process(returning, std::chrono::milliseconds{100}, false)[0], 0.2);
}

void test_smart_limit_value_mode_supports_zero_and_one_factors() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    auto& smart_limit = config.smart_limit[0];
    smart_limit.enabled = true;
    smart_limit.target_value = 0.25;
    smart_limit.lower_factor = 0.0;
    smart_limit.upper_factor = 0.0;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 0.9;
    processor.arm(std::chrono::milliseconds{0});
    require_close(processor.process(target, std::chrono::milliseconds{0}, true)[0], 0.25);

    smart_limit.lower_factor = 1.0;
    smart_limit.upper_factor = 1.0;
    processor.set_config(config);
    require_close(processor.process(target, std::chrono::milliseconds{20}, true)[0], 0.9);
}

void test_smart_limit_speed_mode_blends_from_current_output() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    auto& smart_limit = config.smart_limit[0];
    smart_limit.enabled = true;
    smart_limit.mode = SmartLimitMode::Speed;
    smart_limit.lower_factor = 0.5;
    smart_limit.upper_factor = 0.5;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    processor.arm(std::chrono::milliseconds{0});

    require_close(processor.process(target, std::chrono::milliseconds{0}, true)[0], 0.53125);
}

void test_smart_limit_holds_endpoints_and_interpolates_between_them() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    for (std::size_t index = 0; index < 3; ++index) {
        auto& smart_limit = config.smart_limit[index];
        smart_limit.enabled = true;
        smart_limit.input_axis = index;
        smart_limit.target_value = 0.0;
        smart_limit.lower_input = 0.25;
        smart_limit.lower_factor = 0.8;
        smart_limit.upper_input = 0.75;
        smart_limit.upper_factor = 0.2;
    }
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 0.1;
    target[1] = 0.5;
    target[2] = 0.9;
    processor.arm(std::chrono::milliseconds{0});

    const auto output = processor.process(target, std::chrono::milliseconds{0}, true);
    require_close(output[0], 0.08);
    require_close(output[1], 0.25);
    require_close(output[2], 0.18);
}

void test_smart_limit_x_coordinates_change_transition_location() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    auto& smart_limit = config.smart_limit[0];
    smart_limit.enabled = true;
    smart_limit.target_value = 0.0;
    smart_limit.lower_input = 0.2;
    smart_limit.lower_factor = 1.0;
    smart_limit.upper_input = 0.4;
    smart_limit.upper_factor = 0.0;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 0.25;
    processor.arm(std::chrono::milliseconds{0});
    require_close(processor.process(target, std::chrono::milliseconds{0}, true)[0], 0.1875);

    smart_limit.lower_input = 0.0;
    smart_limit.upper_input = 0.1;
    processor.set_config(config);
    require_close(processor.process(target, std::chrono::milliseconds{20}, true)[0], 0.0);
}

void test_smart_limit_uses_selected_driver_axes_from_one_snapshot() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    auto& first = config.smart_limit[1];
    first.enabled = true;
    first.input_axis = 0;
    first.target_value = 0.0;
    first.lower_input = 0.0;
    first.lower_factor = 0.0;
    first.upper_input = 1.0;
    first.upper_factor = 1.0;
    auto& second = config.smart_limit[2];
    second.enabled = true;
    second.input_axis = 1;
    second.target_value = 0.0;
    second.lower_input = 0.0;
    second.lower_factor = 0.0;
    second.upper_input = 1.0;
    second.upper_factor = 1.0;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 0.25;
    target[1] = 0.8;
    target[2] = 0.6;
    processor.arm(std::chrono::milliseconds{0});

    const auto output = processor.process(target, std::chrono::milliseconds{0}, true);
    require_close(output[0], 0.25);
    require_close(output[1], 0.2);
    require_close(output[2], 0.48);
}

void test_smart_limit_reads_disabled_driver_return_position() {
    OutputSignalConfig config;
    config.soft_start_enabled = false;
    config.axis_output_enabled[0] = false;
    config.axis_return_position[0] = 0.2;
    auto& smart_limit = config.smart_limit[1];
    smart_limit.enabled = true;
    smart_limit.input_axis = 0;
    smart_limit.target_value = 0.0;
    smart_limit.lower_input = 0.0;
    smart_limit.lower_factor = 0.0;
    smart_limit.upper_input = 1.0;
    smart_limit.upper_factor = 1.0;
    OutputSignalProcessor processor(config);
    Axes target;
    target[0] = 1.0;
    target[1] = 0.8;
    processor.arm(std::chrono::milliseconds{0});

    const auto output = processor.process(target, std::chrono::milliseconds{0}, true);
    require_close(output[0], 0.2);
    require_close(output[1], 0.16);
    require_close(processor.smart_limit_inputs()[0], 0.2);
}

void test_smart_limit_normalizes_control_points_and_driver_axis() {
    OutputSignalConfig config;
    auto& reversed = config.smart_limit[0];
    reversed.input_axis = 99;
    reversed.lower_input = 1.2;
    reversed.lower_factor = -1.0;
    reversed.upper_input = -0.2;
    reversed.upper_factor = 2.0;
    config.smart_limit[1].lower_input = 0.5;
    config.smart_limit[1].upper_input = 0.5;
    OutputSignalProcessor processor(config);

    const auto& normalized = processor.config();
    assert(normalized.smart_limit[0].input_axis == 5);
    require_close(normalized.smart_limit[0].lower_input, 0.0);
    require_close(normalized.smart_limit[0].lower_factor, 1.0);
    require_close(normalized.smart_limit[0].upper_input, 1.0);
    require_close(normalized.smart_limit[0].upper_factor, 0.0);
    assert(normalized.smart_limit[1].upper_input - normalized.smart_limit[1].lower_input >= 0.01 - 1e-9);
}

void test_direct_profile_uses_reference_length_and_axis_mask() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    frame.l0_reference_length = true;
    frame.active_axes = {true, false, false, false, false, false};
    frame.participants[0].bones["Penis09"].position.y = 2.0;
    frame.participants[1].bones["M_Gen"].position.y = 0.5;
    const auto snapshot = engine.process(frame);
    require_close(snapshot.raw_axes[0], 0.25);
    for (std::size_t index = 1; index < 6; ++index) require_close(snapshot.raw_axes[index], 0.5);
}

void test_direct_profile_can_calibrate_signed_l0_range() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    frame.l0_reference_length = true;
    frame.direct_l0_min_meters = -0.20;
    frame.direct_l0_max_meters = 0.80;
    frame.participants[1].bones["M_Gen"].position.y = 0.30;
    const auto snapshot = engine.process(frame);
    require_close(snapshot.raw_axes[0], 0.5);
}

void test_direct_profile_can_invert_l0_without_flipping_global_setting() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    frame.l0_reference_length = true;
    frame.direct_l0_min_meters = 0.0;
    frame.direct_l0_max_meters = 1.0;
    frame.direct_l0_inverted = true;
    frame.participants[1].bones["M_Gen"].position.y = 0.25;
    require_close(engine.process(frame).raw_axes[0], 0.75);
}

EngineSnapshot process_l0(MotionEngine& engine, MotionFrame& frame, const double value, const int milliseconds) {
    frame.participants[1].bones["M_Gen"].position.y = value;
    frame.monotonic_time = std::chrono::milliseconds{milliseconds};
    ++frame.sequence;
    return engine.process(frame);
}

void learn_l0_loop(MotionEngine& engine, MotionFrame& frame, const double low, const double high) {
    (void)process_l0(engine, frame, low, 0);
    for (int step = 1; step <= 8; ++step) {
        (void)process_l0(engine, frame, step % 2 == 0 ? low : high, step * 100);
    }
}

EngineSnapshot process_l1(MotionEngine& engine, MotionFrame& frame, const double value, const int milliseconds) {
    // The synthetic frame's reference-forward direction is -Z and the
    // default lateral range is +/-0.15 m.
    frame.participants[1].bones["M_Gen"].position.z = -(value - 0.5) * 0.30;
    frame.monotonic_time = std::chrono::milliseconds{milliseconds};
    ++frame.sequence;
    return engine.process(frame);
}

void learn_l1_loop(MotionEngine& engine, MotionFrame& frame, const double low, const double high) {
    (void)process_l1(engine, frame, low, 0);
    for (int step = 1; step <= 8; ++step) {
        (void)process_l1(engine, frame, step % 2 == 0 ? low : high, step * 100);
    }
}

void test_l0_preferred_travel_is_disabled_by_default() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    frame.l0_reference_length = true;
    learn_l0_loop(engine, frame, 0.0, 0.2);
    const auto snapshot = process_l0(engine, frame, 0.2, 1400);
    require_close(snapshot.raw_axes[0], 0.2);
    assert(snapshot.preferred_travel[0].state == PreferredTravelState::Disabled);
}

void test_l0_preferred_travel_expands_only_after_stable_loop() {
    MotionEngine engine;
    engine.set_axis_travel_preference(0, {true, 0.0, 0.6, 4.0});
    auto frame = orthogonal_frame();
    frame.action_id = "ShortLoop";
    frame.l0_reference_length = true;

    // Before six stable half-strokes the original geometric L0 is preserved.
    for (int step = 0; step < 6; ++step) {
        const auto snapshot = process_l0(engine, frame, step % 2 == 0 ? 0.0 : 0.2, step * 100);
        assert(snapshot.preferred_travel[0].state == PreferredTravelState::Learning);
        require_close(snapshot.raw_axes[0], step % 2 == 0 ? 0.0 : 0.2);
    }
    learn_l0_loop(engine, frame, 0.0, 0.2);
    const auto high = process_l0(engine, frame, 0.2, 1400);
    assert(high.preferred_travel[0].state == PreferredTravelState::Locked);
    require_close(high.preferred_travel[0].observed_travel, 0.2);
    require_close(high.preferred_travel[0].applied_gain, 3.0);
    require_close(high.raw_axes[0], 0.6);
    require_close(process_l0(engine, frame, 0.0, 1500).raw_axes[0], 0.0);
}

void test_l0_preferred_travel_never_shrinks_large_motion() {
    MotionEngine engine;
    engine.set_axis_travel_preference(0, {true, 0.0, 0.6, 4.0});
    auto frame = orthogonal_frame();
    frame.action_id = "LargeLoop";
    frame.l0_reference_length = true;
    learn_l0_loop(engine, frame, 0.1, 0.8);
    const auto high = process_l0(engine, frame, 0.8, 1400);
    require_close(high.preferred_travel[0].applied_gain, 1.0);
    require_close(high.raw_axes[0], 0.8);
}

void test_l0_preferred_travel_ignores_trigger_after_lock() {
    MotionEngine engine;
    engine.set_axis_travel_preference(0, {true, 0.0, 0.6, 4.0});
    auto frame = orthogonal_frame();
    frame.action_id = "TriggerLoop";
    frame.l0_reference_length = true;
    learn_l0_loop(engine, frame, 0.0, 0.2);
    (void)process_l0(engine, frame, 0.2, 1400);
    const auto trigger = process_l0(engine, frame, 0.3, 1500);
    require_close(trigger.raw_axes[0], 0.9);
    require_close(trigger.preferred_travel[0].observed_travel, 0.2);
    require_close(trigger.preferred_travel[0].applied_gain, 3.0);
}

void test_l0_preferred_travel_caps_gain_and_caches_by_action() {
    MotionEngine engine;
    engine.set_axis_travel_preference(0, {true, 0.0, 0.6, 4.0});
    auto frame = orthogonal_frame();
    frame.action_id = "TinyLoop";
    frame.l0_reference_length = true;
    learn_l0_loop(engine, frame, 0.0, 0.1);
    const auto limited = process_l0(engine, frame, 0.1, 1400);
    assert(limited.preferred_travel[0].state == PreferredTravelState::Limited);
    require_close(limited.preferred_travel[0].applied_gain, 4.0);
    require_close(limited.raw_axes[0], 0.4);

    frame.action_id = "OtherLoop";
    assert(process_l0(engine, frame, 0.1, 1500).preferred_travel[0].state == PreferredTravelState::Learning);
    frame.action_id = "TinyLoop";
    const auto cached = process_l0(engine, frame, 0.1, 1600);
    assert(cached.preferred_travel[0].state == PreferredTravelState::Limited);
    require_close(cached.raw_axes[0], 0.4);

    engine.reset_axis_travel_learning(0);
    const auto reset = process_l0(engine, frame, 0.1, 1700);
    assert(reset.preferred_travel[0].state == PreferredTravelState::Learning);
    require_close(reset.raw_axes[0], 0.1);
}

void test_l0_preferred_travel_does_not_learn_noise_or_one_way_motion() {
    MotionEngine engine;
    engine.set_axis_travel_preference(0, {true, 0.0, 0.6, 4.0});
    auto frame = orthogonal_frame();
    frame.action_id = "OneWay";
    frame.l0_reference_length = true;
    for (int step = 0; step <= 10; ++step) {
        const auto value = static_cast<double>(step) * 0.05;
        const auto snapshot = process_l0(engine, frame, value, step * 100);
        assert(snapshot.preferred_travel[0].state == PreferredTravelState::Learning);
        require_close(snapshot.raw_axes[0], value);
    }
    for (int step = 11; step <= 30; ++step) {
        const auto value = 0.5 + (step % 2 == 0 ? 0.003 : -0.003);
        const auto snapshot = process_l0(engine, frame, value, step * 100);
        assert(snapshot.preferred_travel[0].state == PreferredTravelState::Learning);
        require_close(snapshot.raw_axes[0], value);
    }
}

void test_l0_preferred_travel_runs_before_manual_gain() {
    MotionEngine engine;
    auto tuning = engine.axis_tuning();
    tuning[0].gain = 1.5;
    engine.set_axis_tuning(tuning);
    engine.set_axis_travel_preference(0, {true, 0.0, 0.6, 4.0});
    auto frame = orthogonal_frame();
    frame.action_id = "GainLoop";
    frame.l0_reference_length = true;
    learn_l0_loop(engine, frame, 0.0, 0.2);
    const auto high = process_l0(engine, frame, 0.2, 1400);
    require_close(high.raw_axes[0], 0.6);
    require_close(high.device_axes[0], 0.75);
}

void test_preferred_travel_is_independent_for_other_axes() {
    MotionEngine engine;
    engine.set_axis_travel_preference(1, {true, 0.2, 0.8, 4.0});
    auto frame = orthogonal_frame();
    frame.action_id = "L1ShortLoop";
    frame.l0_reference_length = true;
    learn_l1_loop(engine, frame, 0.4, 0.6);

    const auto high = process_l1(engine, frame, 0.6, 1400);
    assert(high.preferred_travel[0].state == PreferredTravelState::Disabled);
    assert(high.preferred_travel[1].state == PreferredTravelState::Locked);
    require_close(high.preferred_travel[1].observed_travel, 0.2);
    require_close(high.preferred_travel[1].applied_gain, 3.0);
    require_close(high.raw_axes[1], 0.8);
    require_close(high.raw_axes[0], 0.5);
}

void test_preferred_travel_maps_stable_endpoints_to_configured_interval() {
    MotionEngine engine;
    engine.set_axis_travel_preference(0, {true, 0.1, 0.7, 4.0});
    auto frame = orthogonal_frame();
    frame.action_id = "OffsetPreferredRange";
    frame.l0_reference_length = true;
    learn_l0_loop(engine, frame, 0.2, 0.4);

    require_close(process_l0(engine, frame, 0.2, 1400).raw_axes[0], 0.1);
    require_close(process_l0(engine, frame, 0.4, 1500).raw_axes[0], 0.7);
    // A short extra movement keeps the same fixed mapping and can extend
    // beyond the learned preferred interval without changing it.
    require_close(process_l0(engine, frame, 0.5, 1600).raw_axes[0], 1.0);
}

void test_rotational_preferred_travel_uses_conservative_gain_cap() {
    MotionEngine engine;
    engine.set_axis_travel_preference(2, {true, 0.2, 0.8, 4.0});
    engine.set_axis_travel_preference(3, {true, 0.2, 0.8, 4.0});
    require_close(engine.axis_travel_preferences()[2].maximum_gain, 4.0);
    require_close(engine.axis_travel_preferences()[3].maximum_gain, 2.0);
}

void test_safety_distance_blocks_far_initial_signal_and_learning() {
    MotionEngine engine;
    auto contact = engine.contact_config();
    contact.safety_distance_enabled = true;
    contact.safety_distance_meters = 0.10;
    engine.set_contact_config(contact);
    engine.set_axis_travel_preference(0, {true, 0.0, 0.6, 4.0});

    auto frame = orthogonal_frame();
    frame.action_id = "FarInitialization";
    frame.l0_reference_length = true;
    frame.participants[1].bones["M_Gen"].position.x = 0.20;
    for (int step = 0; step <= 8; ++step) {
        const auto snapshot = process_l0(engine, frame, step % 2 == 0 ? 0.1 : 0.3, step * 100);
        assert(snapshot.state == MotionState::Idle);
        require_close(snapshot.device_axes[0], 0.5);
    }

    frame.participants[1].bones["M_Gen"].position.x = 0.0;
    const auto entered = process_l0(engine, frame, 0.1, 1000);
    assert(entered.state == MotionState::Active);
    assert(entered.preferred_travel[0].state == PreferredTravelState::Learning);
    assert(entered.preferred_travel[0].stable_half_strokes == 0);

    frame.participants[1].bones["M_Gen"].position.x = 0.20;
    assert(process_l0(engine, frame, 0.2, 1100).state == MotionState::Holding);
}

void test_safety_distance_is_backward_compatible_when_disabled() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    frame.participants[1].bones["M_Gen"].position.x = 0.30;
    assert(engine.process(frame).state == MotionState::Active);
}

void test_humanoid_pelvis_plane_overrides_single_support_rotation() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    auto& reference = frame.participants[0];
    reference.bones.emplace("M_Spine1", pose("M_Spine1", {0, 1, 0}));
    reference.bones.emplace("L_Thigh", pose("L_Thigh", {-1, 0, 0}));
    reference.bones.emplace("R_Thigh", pose("R_Thigh", {1, 0, 0}));
    frame.participants[1].bones["M_Gen"].position = {0.10, 0.5, 0};
    const auto snapshot = engine.process(frame);
    // The default support mapping uses -local X. The validated pelvis plane
    // uses the named left/right landmarks, so positive world X is L2-positive.
    assert(snapshot.raw_axes[2] > 0.5);
}

void test_profile_plane_uses_native_nonhuman_landmarks() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    auto& reference = frame.participants[0];
    reference.bones.emplace("Root_M", pose("Root_M", {0, 0, 0}));
    reference.bones.emplace("Chest_M", pose("Chest_M", {0, 1, 0}));
    reference.bones.emplace("Hip_L", pose("Hip_L", {-1, 0, 0}));
    reference.bones.emplace("Hip_R", pose("Hip_R", {1, 0, 0}));
    frame.reference_plane = BodyReferencePlane{"quadruped_trunk", "Root_M", "Chest_M", "Hip_L", "Hip_R"};
    frame.participants[1].bones["M_Gen"].position = {0.10, 0.5, 0};
    const auto snapshot = engine.process(frame);
    assert(snapshot.raw_axes[2] > 0.5);
}

void test_twist_remains_relative_when_reference_crosses_a_turn() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    // The target's local Z axis travels through more than one signed-angle
    // revolution. R0 must stay the shortest displacement from its activation
    // baseline, rather than becoming a continuously increasing turn counter.
    frame.participants[1].bones["M_Gen"].rotation = {1, 0, 0, 0};
    (void)engine.process(frame);
    frame.participants[1].bones["M_Gen"].rotation = {-0.1736481776669303, 0, 0.984807753012208, 0};
    (void)engine.process(frame);
    frame.participants[1].bones["M_Gen"].rotation = {-0.9396926207859084, 0, -0.3420201433256687, 0};
    const auto snapshot = engine.process(frame);
    assert(std::abs(snapshot.contact.twist_degrees) <= 180.0 + 1e-6);
}

void test_pitch_crosses_signed_angle_seam_without_output_jump() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    const auto one_degree = std::numbers::pi / 180.0;
    // Rotating the target basis by +/-1 degree around local X places its up
    // vector on opposite numeric sides of the signed-angle 180-degree seam.
    // The physical change is only two degrees, so R2 must remain continuous.
    frame.participants[1].bones["M_Gen"].rotation = {
        std::cos(one_degree * 0.5), std::sin(one_degree * 0.5), 0, 0};
    (void)engine.process(frame);
    frame.participants[1].bones["M_Gen"].rotation = {
        std::cos(one_degree * 0.5), -std::sin(one_degree * 0.5), 0, 0};
    const auto snapshot = engine.process(frame);
    assert(std::abs(snapshot.contact.pitch_degrees) < 3.0);
    assert(std::abs(snapshot.raw_axes[5] - 0.5) < 0.06);
}

void test_target_contact_frame_replaces_only_rotation_basis() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    auto& target = frame.participants[1];
    target.bones.emplace("M_Clit", pose("M_Clit", {0, 0.5, 1.0}));
    target.bones.emplace("L_Labia", pose("L_Labia", {-1.0, 0.5, 0}));
    target.bones.emplace("R_Labia", pose("R_Labia", {1.0, 0.5, 0}));
    frame.target_frame = TargetContactFrame{
        "plane_normal", "M_Gen", "M_Gen", "M_Clit", "L_Labia", "R_Labia"};

    const auto snapshot = engine.process(frame);
    assert(snapshot.contact.target_mode == "target_contact_frame");
    // The frame changes only R0/R1/R2. Contact position and translations are
    // still sourced from the original selected M_Gen point.
    require_close(snapshot.raw_axes[0], 1.0);
    require_close(snapshot.raw_axes[1], 0.5);
    require_close(snapshot.raw_axes[2], 0.5);
}

void test_target_plane_intersection_drives_translation_axes() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    frame.l0_reference_length = true;
    auto& target = frame.participants[1];
    // The entrance point is offset from the Reference axis while the nearby
    // landmarks tilt its plane. The resulting line-plane intersection is
    // deeper than the legacy axial projection and has a lateral residual.
    target.bones["M_Gen"].position = {0.03, 0.5, 0};
    target.bones.emplace("M_Clit", pose("M_Clit", {0.03, 0.6, 0.1}));
    target.bones.emplace("L_Labia", pose("L_Labia", {-0.02, 0.5, -0.02}));
    target.bones.emplace("R_Labia", pose("R_Labia", {0.08, 0.5, 0.02}));
    frame.target_frame = TargetContactFrame{
        "plane_intersection", "M_Gen", "M_Gen", "M_Clit", "L_Labia", "R_Labia"};

    const auto snapshot = engine.process(frame);
    assert(snapshot.contact.target_mode == "target_plane_blended");
    const auto alignment = 0.01 / std::sqrt(0.004 * 0.004 + 0.01 * 0.01 + 0.01 * 0.01);
    const auto blend_position = (alignment - 0.25) / (0.75 - 0.25);
    const auto blend = blend_position * blend_position * (3.0 - 2.0 * blend_position);
    const auto expected_axial = 0.5 + 0.012 * blend;
    require_close(snapshot.contact.axial_meters, expected_axial);
    require_close(snapshot.contact.radial_meters, std::sqrt(0.03 * 0.03 + std::pow(0.5 - expected_axial, 2)));
    require_close(snapshot.raw_axes[0], expected_axial);
    require_close(snapshot.raw_axes[1], 0.5);
    require_close(snapshot.raw_axes[2], 0.4);
}

void test_target_plane_intersection_falls_back_when_parallel() {
    MotionEngine engine;
    auto frame = orthogonal_frame();
    frame.l0_reference_length = true;
    auto& target = frame.participants[1];
    // This target plane has a +Z normal while the Reference travels along +Y.
    // Their intersection is undefined, so the legacy single-point translation
    // must remain available without invalidating the motion frame.
    target.bones.emplace("M_Clit", pose("M_Clit", {0, 1.5, 0}));
    target.bones.emplace("L_Labia", pose("L_Labia", {-1.0, 0.5, 0}));
    target.bones.emplace("R_Labia", pose("R_Labia", {1.0, 0.5, 0}));
    frame.target_frame = TargetContactFrame{
        "plane_intersection", "M_Gen", "M_Gen", "M_Clit", "L_Labia", "R_Labia"};

    const auto snapshot = engine.process(frame);
    assert(snapshot.contact.target_mode == "target_plane_fallback");
    require_close(snapshot.raw_axes[0], 0.5);
    require_close(snapshot.raw_axes[1], 0.5);
    require_close(snapshot.raw_axes[2], 0.5);
}

void test_selector_resolves_frame_for_locked_functional_bone() {
    FunctionalTargetSelector selector;
    auto frame = orthogonal_frame();
    frame.action_id = "HandAction";
    auto& target = frame.participants[1];
    target.bones.erase("M_Gen");
    target.bones.emplace("R_Hand", pose("R_Hand", {0.2, 0.6, 0}));
    target.target_frames.push_back(TargetContactFrame{
        "plane_normal", "R_Hand", "R_Hand", "R_Middle_F01", "R_Pinky_F01", "R_Index_F01"});

    assert(selector.alias_target(frame, {"R_Hand"}));
    assert(frame.target_frame.has_value());
    assert(frame.target_frame->source_bone == "R_Hand");
}

void test_functional_target_priority_stays_locked_during_an_action() {
    FunctionalTargetSelector selector;
    auto frame = orthogonal_frame();
    frame.action_id = "HandAction";
    auto& target = frame.participants[1];
    target.bones.erase("M_Gen");
    target.bones.emplace("R_Hand", pose("R_Hand", {0.2, 0.6, 0}));
    target.bones.emplace("L_Hand", pose("L_Hand", {0.0, 0.4, 0}));

    assert(selector.alias_target(frame, {"R_Hand", "L_Hand"}));
    require_close(frame.participants[1].bones.at("M_Gen").position.x, 0.2);
    assert(selector.selected_bone() == "R_Hand");

    // Even when the other hand becomes much closer, the selected functional
    // target must remain stable until the action or explicit priority changes.
    frame.participants[1].bones["R_Hand"].position = {1.0, 1.0, 0};
    frame.participants[1].bones["L_Hand"].position = {0.0, 0.1, 0};
    assert(selector.alias_target(frame, {"R_Hand", "L_Hand"}));
    require_close(frame.participants[1].bones.at("M_Gen").position.x, 1.0);
    assert(selector.selected_bone() == "R_Hand");
}

void test_functional_target_releases_only_after_missing_grace() {
    FunctionalTargetSelector selector{2};
    auto frame = orthogonal_frame();
    frame.action_id = "HandAction";
    auto& target = frame.participants[1];
    target.bones.erase("M_Gen");
    target.bones.emplace("R_Hand", pose("R_Hand", {0.2, 0.6, 0}));
    target.bones.emplace("L_Hand", pose("L_Hand", {0.0, 0.4, 0}));
    assert(selector.alias_target(frame, {"R_Hand", "L_Hand"}));

    target.bones.erase("R_Hand");
    target.bones.erase("M_Gen");
    assert(!selector.alias_target(frame, {"R_Hand", "L_Hand"}));
    assert(!selector.alias_target(frame, {"R_Hand", "L_Hand"}));
    assert(selector.alias_target(frame, {"R_Hand", "L_Hand"}));
    assert(selector.selected_bone() == "L_Hand");
}

void test_functional_target_honours_explicit_reference_selection() {
    FunctionalTargetSelector selector;
    auto frame = orthogonal_frame();
    frame.action_id = "HandAction";
    frame.participants[1].bones.erase("M_Gen");
    frame.participants[1].bones.emplace("R_Hand", pose("R_Hand", {0.2, 0.6, 0}));
    Participant reference{"male:1", "male", "fallen-doll"};
    reference.bones.emplace("Penis01", pose("Penis01", {10, 0, 0}));
    Participant alternate{"female:1", "female", "fallen-doll"};
    alternate.bones.emplace("R_Hand", pose("R_Hand", {9.8, 0.4, 0}));
    frame.participants.push_back(std::move(reference));
    frame.participants.push_back(std::move(alternate));

    assert(selector.alias_target(frame, {"R_Hand"}, "Penis01", "M_Gen", "male:1"));
    assert(selector.selected_participant() == "female:1");
    require_close(frame.participants[3].bones.at("M_Gen").position.x, 9.8);
}

void test_functional_target_uses_contact_pair_for_selected_reference() {
    FunctionalTargetSelector selector;
    auto frame = orthogonal_frame();
    frame.action_id = "VaginalMouth";
    frame.participants[0].stable_key = "male:0";
    frame.participants[1].bones.emplace("M_Jaw", pose("M_Jaw", {0, 0.05, 0}));

    Participant second_reference{"male:1", "male", "fallen-doll"};
    second_reference.bones.emplace("Penis01", pose("Penis01", {10, 0, 0}));
    frame.participants.push_back(std::move(second_reference));
    frame.participants[1].bones["M_Gen"].position = {10, 0.05, 0};

    const std::unordered_map<std::string, std::vector<std::string>> pairs{
        {"male:0", {"M_Jaw"}},
        {"male:1", {"M_Gen"}},
    };
    assert(selector.alias_target(frame, {"M_Jaw", "M_Gen"}, "Penis01", "M_Gen", "male:1", pairs));
    assert(selector.selected_bone() == "M_Gen");
    require_close(frame.participants[1].bones.at("M_Gen").position.x, 10.0);
}

} // namespace

int main() {
    test_contact_and_tcode();
    test_tcode_can_send_only_changed_axes_with_real_interval();
    test_output_soft_start_tracks_actual_command();
    test_output_speed_limit_is_per_axis_and_time_based();
    test_output_waits_for_live_motion_and_does_not_delay_safety_return();
    test_output_speed_limit_only_affects_enabled_axes();
    test_output_axis_disable_moves_only_selected_axis_to_return_position();
    test_output_waits_at_custom_return_positions_before_live_motion();
    test_output_axis_disable_obeys_its_speed_limit();
    test_output_disabled_axis_stays_at_return_position_during_stream_return();
    test_smart_limit_value_mode_supports_zero_and_one_factors();
    test_smart_limit_speed_mode_blends_from_current_output();
    test_smart_limit_holds_endpoints_and_interpolates_between_them();
    test_smart_limit_x_coordinates_change_transition_location();
    test_smart_limit_uses_selected_driver_axes_from_one_snapshot();
    test_smart_limit_reads_disabled_driver_return_position();
    test_smart_limit_normalizes_control_points_and_driver_axis();
    test_gain_scales_output_travel();
    test_axis_inversion_is_independent();
    test_hold_and_return();
    test_custom_axis_return_position();
    test_bilateral_contact_uses_reference_depth();
    test_direct_profile_uses_reference_length_and_axis_mask();
    test_direct_profile_can_calibrate_signed_l0_range();
    test_direct_profile_can_invert_l0_without_flipping_global_setting();
    test_l0_preferred_travel_is_disabled_by_default();
    test_l0_preferred_travel_expands_only_after_stable_loop();
    test_l0_preferred_travel_never_shrinks_large_motion();
    test_l0_preferred_travel_ignores_trigger_after_lock();
    test_l0_preferred_travel_caps_gain_and_caches_by_action();
    test_l0_preferred_travel_does_not_learn_noise_or_one_way_motion();
    test_l0_preferred_travel_runs_before_manual_gain();
    test_preferred_travel_is_independent_for_other_axes();
    test_preferred_travel_maps_stable_endpoints_to_configured_interval();
    test_rotational_preferred_travel_uses_conservative_gain_cap();
    test_safety_distance_blocks_far_initial_signal_and_learning();
    test_safety_distance_is_backward_compatible_when_disabled();
    test_humanoid_pelvis_plane_overrides_single_support_rotation();
    test_profile_plane_uses_native_nonhuman_landmarks();
    test_twist_remains_relative_when_reference_crosses_a_turn();
    test_pitch_crosses_signed_angle_seam_without_output_jump();
    test_target_contact_frame_replaces_only_rotation_basis();
    test_target_plane_intersection_drives_translation_axes();
    test_target_plane_intersection_falls_back_when_parallel();
    test_selector_resolves_frame_for_locked_functional_bone();
    test_functional_target_priority_stays_locked_during_an_action();
    test_functional_target_releases_only_after_missing_grace();
    test_functional_target_honours_explicit_reference_selection();
    test_functional_target_uses_contact_pair_for_selected_reference();
    std::cout << "motion_bridge_core_tests: OK\n";
}
