#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace motion_bridge {

inline constexpr std::array<const char*, 6> kAxisNames{"L0", "L1", "L2", "R0", "R1", "R2"};

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct Quaternion {
    double w{1.0};
    double x{};
    double y{};
    double z{};
};

struct BonePose {
    std::string name;
    Vec3 position;
    Quaternion rotation;
};

// Optional adapter-declared contact surface. Nearby landmarks always supply a
// stable orientation for R0/R1/R2. Adapters may additionally opt translations
// into plane_intersection; an empty mode keeps the legacy single-point path.
struct TargetContactFrame {
    std::string mode;
    std::string source_bone;
    std::string origin_bone;
    std::string forward_bone;
    std::string left_bone;
    std::string right_bone;
    std::string translation_mode;
};

struct Participant {
    std::string stable_key;
    std::string role;
    std::string skeleton_id;
    // Human-readable identity supplied by the game adapter. `stable_key`
    // remains the internal selection key because it is unique and durable.
    std::string participant_tag;
    std::unordered_map<std::string, BonePose> bones;
    std::vector<TargetContactFrame> target_frames;
};

// A game adapter may provide a verified body plane with native bone names.
// It is independent of any one game or species naming convention.
struct BodyReferencePlane {
    std::string mode;
    std::string center_bone;
    std::string forward_bone;
    std::string left_bone;
    std::string right_bone;
};

struct MotionFrame {
    std::string schema{"motion-frame/v1"};
    std::string game_id;
    std::uint64_t sequence{};
    std::chrono::microseconds monotonic_time{};
    bool action_active{};
    std::string action_id;
    std::string action_category;
    std::vector<Participant> participants;
    // Game adapters may declare that L0 is normalized by the live reference
    // axis length instead of the generic human 8-27 cm range.
    bool l0_reference_length{};
    // A calibrated game profile can supply a local signed stroke range. This
    // takes precedence over the generic or full-reference L0 mapping.
    std::optional<double> direct_l0_min_meters;
    std::optional<double> direct_l0_max_meters;
    bool direct_l0_inverted{};
    std::array<bool, 6> active_axes{true, true, true, true, true, true};
    // Optional low-cost body landmarks supplied by the game profile. When
    // absent, Fallen Doll's original humanoid pelvis names remain supported.
    std::optional<BodyReferencePlane> reference_plane;
    // Resolved after participant and functional-bone selection for this action.
    std::optional<TargetContactFrame> target_frame;
};

struct Axes {
    std::array<double, 6> values{0.5, 0.5, 0.5, 0.5, 0.5, 0.5};

    [[nodiscard]] double& operator[](std::size_t index) { return values[index]; }
    [[nodiscard]] double operator[](std::size_t index) const { return values[index]; }
};

struct ContactConfig {
    std::string reference_participant;
    std::string origin_bone{"Penis01"};
    std::string direction_bone{"Penis02"};
    std::string tip_bone{"Penis09"};
    std::string support_bone{"M_Hips"};
    std::string target_bone{"M_Gen"};
    // Optional second contact bone for bilateral Hand/Foot mapping. When set,
    // its line to target_bone supplies twist and prevents false ankle/hand tilt.
    std::string target_secondary_bone;
    std::string support_right_axis{"-local_x"};
    std::string support_up_axis{"+local_y"};
    std::string target_up_axis{"-local_y"};
    std::string target_right_axis{"+local_z"};
    double l0_min_meters{0.08};
    double l0_max_meters{0.27};
    double lateral_range_meters{0.15};
    double twist_range_degrees{90.0};
    double tilt_range_degrees{30.0};
    double radius_scale{0.22};
    double safety_distance_meters{0.10};
    bool invert_l0{};
    bool require_contact{};
    bool safety_distance_enabled{};
};

enum class MotionCurve { Linear, Smoothstep, Smootherstep };

struct AxisTuning {
    double gain{1.0};
    double center{0.5};
    double dead_zone{};
    MotionCurve curve{MotionCurve::Linear};
    double output_min{};
    double output_max{1.0};
    bool enabled{true};
    bool inverted{};
};

enum class PreferredTravelState { Disabled, Learning, Locked, Limited };

struct PreferredTravelConfig {
    bool enabled{};
    // Desired output interval for the learned stable motion. The preferred
    // span is maximum - minimum; extra motion may still use remaining output
    // headroom outside this interval.
    double preferred_minimum{};
    double preferred_maximum{0.6};
    double maximum_gain{4.0};
};

struct PreferredTravelStatus {
    PreferredTravelState state{PreferredTravelState::Disabled};
    double observed_travel{};
    double applied_gain{1.0};
    unsigned int stable_half_strokes{};
};

struct SafetyConfig {
    std::chrono::milliseconds hold_for{250};
    std::chrono::milliseconds return_for{600};
    std::array<double, 6> return_positions{0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
};

enum class MotionState { Idle, Active, Holding, Returning, Fault };

struct ContactStatus {
    bool valid{};
    bool contact_valid{};
    std::string reason{"missing_input"};
    double reference_length{};
    double reference_radius{};
    double axial_meters{};
    double radial_meters{};
    double twist_degrees{};
    double roll_degrees{};
    double pitch_degrees{};
    std::string target_mode{"single_bone"};
};

struct EngineSnapshot {
    std::uint64_t sequence{};
    std::chrono::microseconds monotonic_time{};
    MotionState state{MotionState::Idle};
    Axes raw_axes;
    Axes device_axes;
    std::array<PreferredTravelStatus, 6> preferred_travel;
    ContactStatus contact;
    std::string action_id;
    std::string action_category;
};

} // namespace motion_bridge
