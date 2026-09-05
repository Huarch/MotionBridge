#include "motion_bridge/motion_engine.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace motion_bridge {
namespace {

constexpr double kEpsilon = 1e-8;
constexpr double kPlaneIntersectionBlendStartAlignment = 0.25;
constexpr double kPlaneIntersectionFullAlignment = 0.75;
constexpr double kPlaneIntersectionMaximumCorrectionFraction = 0.25;
constexpr double kTravelReversalHysteresis = 0.008;
constexpr double kTravelMinimumHalfStroke = 0.02;
constexpr double kTravelAbsoluteTolerance = 0.015;
constexpr std::size_t kTravelCandidateLimit = 8;
constexpr std::size_t kTravelRequiredStableHalfStrokes = 6;
constexpr auto kTravelTransitionDuration = std::chrono::microseconds{500000};

[[nodiscard]] Vec3 add(const Vec3 left, const Vec3 right) { return {left.x + right.x, left.y + right.y, left.z + right.z}; }
[[nodiscard]] Vec3 subtract(const Vec3 left, const Vec3 right) { return {left.x - right.x, left.y - right.y, left.z - right.z}; }
[[nodiscard]] Vec3 scale(const Vec3 value, const double scalar) { return {value.x * scalar, value.y * scalar, value.z * scalar}; }
[[nodiscard]] double dot(const Vec3 left, const Vec3 right) { return left.x * right.x + left.y * right.y + left.z * right.z; }
[[nodiscard]] Vec3 cross(const Vec3 left, const Vec3 right) {
    return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z, left.x * right.y - left.y * right.x};
}
[[nodiscard]] double magnitude(const Vec3 value) { return std::sqrt(dot(value, value)); }
[[nodiscard]] std::optional<Vec3> normalize(const Vec3 value) {
    const auto length = magnitude(value);
    return length <= kEpsilon ? std::nullopt : std::optional<Vec3>{scale(value, 1.0 / length)};
}
[[nodiscard]] Vec3 project_on_plane(const Vec3 value, const Vec3 normal) {
    const auto normalized = normalize(normal);
    return normalized ? subtract(value, scale(*normalized, dot(value, *normalized))) : value;
}
[[nodiscard]] Quaternion normalized(const Quaternion value) {
    const auto length = std::sqrt(value.w * value.w + value.x * value.x + value.y * value.y + value.z * value.z);
    return length <= kEpsilon ? Quaternion{} : Quaternion{value.w / length, value.x / length, value.y / length, value.z / length};
}
[[nodiscard]] Quaternion multiply(const Quaternion left, const Quaternion right) {
    return {
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
    };
}
[[nodiscard]] Vec3 rotate(const Quaternion rotation, const Vec3 vector) {
    const auto q = normalized(rotation);
    const auto rotated = multiply(multiply(q, Quaternion{0.0, vector.x, vector.y, vector.z}), Quaternion{q.w, -q.x, -q.y, -q.z});
    return {rotated.x, rotated.y, rotated.z};
}
[[nodiscard]] Vec3 local_axis(const Quaternion rotation, const std::string& name) {
    const auto sign = !name.empty() && name.front() == '-' ? -1.0 : 1.0;
    if (name.ends_with("local_y")) return rotate(rotation, {0.0, sign, 0.0});
    if (name.ends_with("local_z")) return rotate(rotation, {0.0, 0.0, sign});
    return rotate(rotation, {sign, 0.0, 0.0});
}
[[nodiscard]] double signed_angle_degrees(const Vec3 start, const Vec3 end, const Vec3 axis) {
    const auto a = normalize(start); const auto b = normalize(end); const auto n = normalize(axis);
    if (!a || !b || !n) return 0.0;
    return std::atan2(dot(*n, cross(*a, *b)), std::clamp(dot(*a, *b), -1.0, 1.0)) * 180.0 / std::numbers::pi;
}
[[nodiscard]] double range01(const double value, const double minimum, const double maximum) {
    return maximum <= minimum + kEpsilon ? 0.5 : clamp01((value - minimum) / (maximum - minimum));
}
[[nodiscard]] double symmetric01(const double value, const double maximum) {
    return maximum <= kEpsilon ? 0.5 : clamp01(0.5 + value / (2.0 * maximum));
}
[[nodiscard]] double smoothstep01(const double value) {
    const auto t = clamp01(value);
    return t * t * (3.0 - 2.0 * t);
}
[[nodiscard]] double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 == 0 ? (values[middle - 1] + values[middle]) * 0.5 : values[middle];
}
[[nodiscard]] const Participant* participant(const MotionFrame& frame, const std::string& key, const std::string& required_bone) {
    if (!key.empty()) {
        const auto found = std::find_if(frame.participants.begin(), frame.participants.end(), [&key](const Participant& item) { return item.stable_key == key; });
        return found == frame.participants.end() ? nullptr : &*found;
    }
    const auto found = std::find_if(frame.participants.begin(), frame.participants.end(), [&required_bone](const Participant& item) {
        return item.bones.contains(required_bone);
    });
    return found == frame.participants.end() ? nullptr : &*found;
}
[[nodiscard]] const BonePose* bone(const Participant* item, const std::string& name) {
    if (item == nullptr) return nullptr;
    const auto found = item->bones.find(name);
    return found == item->bones.end() ? nullptr : &found->second;
}
struct PelvisPlane {
    Vec3 normal;
    Vec3 tangent;
};
struct TargetBasis {
    Vec3 origin;
    Vec3 up;
    Vec3 right;
};

[[nodiscard]] std::optional<TargetBasis> target_contact_basis(
    const MotionFrame& frame,
    const Participant* item) {
    if (!frame.target_frame || item == nullptr) return std::nullopt;
    const auto& spec = *frame.target_frame;
    const auto* origin = bone(item, spec.origin_bone);
    const auto* forward = bone(item, spec.forward_bone);
    const auto* left = bone(item, spec.left_bone);
    const auto* right = bone(item, spec.right_bone);
    if (!origin || !forward || !left || !right) return std::nullopt;

    const auto lateral = normalize(subtract(right->position, left->position));
    if (!lateral) return std::nullopt;
    if (spec.mode == "plane_normal" || spec.mode == "plane_intersection") {
        // Direction landmarks describe the surface independently from the
        // physical entrance anchor. For a body plane this is the line from the
        // thigh midpoint to the spine; for a local contact ring it is the line
        // from the lateral midpoint to the forward landmark.
        const auto lateral_midpoint = scale(add(left->position, right->position), 0.5);
        const auto longitudinal = normalize(subtract(forward->position, lateral_midpoint));
        if (!longitudinal) return std::nullopt;
        const auto up = normalize(cross(*lateral, *longitudinal));
        if (!up) return std::nullopt;
        const auto tangent = normalize(project_on_plane(*lateral, *up));
        // The declared origin is the physical entrance and therefore anchors
        // the plane. The other landmarks determine its orientation only. An
        // averaged landmark position would move the entrance toward the labia,
        // anal ring, or lips by an arbitrary rig-dependent offset.
        return tangent ? std::optional<TargetBasis>{TargetBasis{origin->position, *up, *tangent}} : std::nullopt;
    }
    if (spec.mode == "axis_tangent") {
        const auto up = normalize(subtract(origin->position, forward->position));
        if (!up) return std::nullopt;
        const auto tangent = normalize(project_on_plane(*lateral, *up));
        return tangent ? std::optional<TargetBasis>{TargetBasis{origin->position, *up, *tangent}} : std::nullopt;
    }
    return std::nullopt;
}
[[nodiscard]] std::optional<PelvisPlane> plane_from_landmarks(
    const Participant* item,
    const std::string& center_name,
    const std::string& forward_name,
    const std::string& left_name,
    const std::string& right_name) {
    const auto* center = bone(item, center_name);
    const auto* forward = bone(item, forward_name);
    const auto* left = bone(item, left_name);
    const auto* right = bone(item, right_name);
    if (!center || !forward || !left || !right) return std::nullopt;
    const auto lateral = normalize(subtract(right->position, left->position));
    const auto vertical = normalize(subtract(forward->position, center->position));
    if (!lateral || !vertical) return std::nullopt;
    const auto normal = normalize(cross(*lateral, *vertical));
    if (!normal) return std::nullopt;
    const auto tangent = normalize(project_on_plane(*lateral, *normal));
    if (!tangent) return std::nullopt;
    return PelvisPlane{*normal, *tangent};
}
[[nodiscard]] std::optional<PelvisPlane> reference_plane(const MotionFrame& frame, const Participant* item) {
    if (frame.reference_plane) {
        const auto& plane = *frame.reference_plane;
        if (const auto custom = plane_from_landmarks(item, plane.center_bone, plane.forward_bone, plane.left_bone, plane.right_bone)) {
            return custom;
        }
    }
    return plane_from_landmarks(item, "M_Hips", "M_Spine1", "L_Thigh", "R_Thigh");
}
[[nodiscard]] double shape(const double value, const MotionCurve curve) {
    switch (curve) {
    case MotionCurve::Smoothstep: return value * value * (3.0 - 2.0 * value);
    case MotionCurve::Smootherstep: return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
    default: return value;
    }
}
[[nodiscard]] double tune_value(double value, const AxisTuning& tuning, const double gain_center) {
    value = clamp01(value);
    const auto center = std::clamp(tuning.center, 0.0, 1.0);
    const auto gain = std::clamp(tuning.gain, 0.25, 4.0);
    const auto dead_zone = std::clamp(tuning.dead_zone, 0.0, 0.4);
    const auto positive = value >= center;
    const auto span = positive ? std::max(1.0 - center, kEpsilon) : std::max(center, kEpsilon);
    auto progress = positive ? (value - center) / span : (center - value) / span;
    progress = std::max(0.0, (progress - dead_zone) / std::max(1.0 - dead_zone, kEpsilon));
    const auto shaped = shape(progress, tuning.curve);
    const auto normalized_value = positive ? center + span * shaped : center - span * shaped;
    const auto lower = std::clamp(std::min(tuning.output_min, tuning.output_max), 0.0, 1.0);
    const auto upper = std::clamp(std::max(tuning.output_min, tuning.output_max), 0.0, 1.0);
    // The geometric signal is already normalised by the game adapter.  Gain
    // must therefore enlarge the *observed motion* around its own neutral
    // value, not around a fixed 0.5.  A fixed midpoint clipped short L0
    // strokes that happened to live entirely in one half of the range.
    const auto center_output = lower + std::clamp(gain_center, 0.0, 1.0) * (upper - lower);
    const auto unscaled_output = lower + normalized_value * (upper - lower);
    // Gain changes the device's excursion around its selected center.  Apply
    // it after dead-zone and curve shaping so it controls travel, not input
    // sensitivity or the shape of the response curve.
    auto output = center_output + (unscaled_output - center_output) * gain;
    output = std::clamp(output, lower, upper);
    if (tuning.inverted) output = lower + upper - output;
    return tuning.enabled ? clamp01(output) : 0.5;
}

[[nodiscard]] double shortest_angle_delta(const double current, const double baseline) {
    // R0 is a relative actuator, not an accumulated turn counter.  A moving
    // reference axis can cross the signed-angle seam during ordinary motion;
    // retain the equivalent turn closest to the baseline so that seam
    // crossings cannot make a toy spin through multiple revolutions.
    return std::remainder(current - baseline, 360.0);
}

} // namespace

double clamp01(const double value) noexcept { return std::clamp(value, 0.0, 1.0); }

MotionEngine::MotionEngine(ContactConfig contact, SafetyConfig safety) : contact_(std::move(contact)), safety_(safety) {
    for (std::size_t axis = 1; axis < travel_configs_.size(); ++axis) {
        travel_configs_[axis].preferred_minimum = 0.2;
        travel_configs_[axis].preferred_maximum = 0.8;
    }
    for (std::size_t axis = 3; axis < travel_configs_.size(); ++axis) {
        travel_configs_[axis].maximum_gain = 2.0;
    }
}
void MotionEngine::set_contact_config(ContactConfig contact) {
    contact_ = std::move(contact);
    last_valid_.reset();
    angle_binding_key_.clear();
    twist_baseline_.reset();
    roll_baseline_.reset();
    pitch_baseline_.reset();
    gain_binding_key_.clear();
    gain_envelope_valid_.fill(false);
    for (std::size_t axis = 0; axis < travel_runtime_.size(); ++axis) {
        travel_runtime_[axis].cache.clear();
        reset_axis_travel_live_state(axis, {});
    }
}
void MotionEngine::set_axis_tuning(std::array<AxisTuning, 6> tuning) { tuning_ = std::move(tuning); }
void MotionEngine::set_axis_return_position(const std::size_t axis, const double position) {
    if (axis >= safety_.return_positions.size()) return;
    safety_.return_positions[axis] = clamp01(position);
}
void MotionEngine::set_axis_travel_preference(const std::size_t axis, PreferredTravelConfig config) {
    if (axis >= travel_configs_.size()) return;
    config.preferred_minimum = std::clamp(config.preferred_minimum, 0.0, 0.9);
    config.preferred_maximum = std::clamp(config.preferred_maximum, 0.1, 1.0);
    if (config.preferred_maximum - config.preferred_minimum < 0.1) {
        config.preferred_maximum = std::min(1.0, config.preferred_minimum + 0.1);
        config.preferred_minimum = std::max(0.0, config.preferred_maximum - 0.1);
    }
    const auto maximum_allowed_gain = axis < 3 ? 4.0 : 2.0;
    config.maximum_gain = std::clamp(config.maximum_gain, 1.0, maximum_allowed_gain);
    travel_configs_[axis] = config;
    if (!config.enabled) {
        travel_runtime_[axis].status = {};
        travel_runtime_[axis].optimized_center.reset();
    }
}
void MotionEngine::reset_axis_travel_learning(const std::size_t axis) {
    if (axis >= travel_runtime_.size()) return;
    auto& runtime = travel_runtime_[axis];
    if (!runtime.profile_key.empty()) runtime.cache.erase(runtime.profile_key);
    const auto active_key = runtime.profile_key;
    reset_axis_travel_live_state(axis, active_key);
}
const ContactConfig& MotionEngine::contact_config() const noexcept { return contact_; }
const std::array<AxisTuning, 6>& MotionEngine::axis_tuning() const noexcept { return tuning_; }
const std::array<PreferredTravelConfig, 6>& MotionEngine::axis_travel_preferences() const noexcept { return travel_configs_; }

void MotionEngine::reset_axis_travel_live_state(const std::size_t axis, const std::string& profile_key) {
    auto& runtime = travel_runtime_[axis];
    runtime.profile_key = profile_key;
    runtime.profile.reset();
    runtime.transition_at.reset();
    runtime.has_sample = false;
    runtime.anchor = 0.0;
    runtime.extremum = 0.0;
    runtime.direction = 0;
    runtime.last_turning_point.reset();
    runtime.half_strokes.clear();
    runtime.optimized_center.reset();
    runtime.status = travel_configs_[axis].enabled
        ? PreferredTravelStatus{PreferredTravelState::Learning, 0.0, 1.0, 0}
        : PreferredTravelStatus{};
    if (const auto cached = runtime.cache.find(profile_key); cached != runtime.cache.end()) {
        runtime.profile = cached->second;
    }
}

void MotionEngine::record_axis_turning_point(const std::size_t axis, const double value, const std::chrono::microseconds now) {
    auto& runtime = travel_runtime_[axis];
    if (runtime.last_turning_point) {
        const auto low = std::min(*runtime.last_turning_point, value);
        const auto high = std::max(*runtime.last_turning_point, value);
        if (high - low >= kTravelMinimumHalfStroke) {
            runtime.half_strokes.emplace_back(low, high);
            if (runtime.half_strokes.size() > kTravelCandidateLimit) {
                runtime.half_strokes.erase(runtime.half_strokes.begin());
            }
        }
    }
    runtime.last_turning_point = value;

    std::vector<double> travels;
    travels.reserve(runtime.half_strokes.size());
    for (const auto& [low, high] : runtime.half_strokes) travels.push_back(high - low);
    const auto candidate_travel = median(travels);
    runtime.status.observed_travel = candidate_travel;
    runtime.status.stable_half_strokes = static_cast<unsigned int>(runtime.half_strokes.size());
    if (runtime.half_strokes.size() < kTravelRequiredStableHalfStrokes) return;
    const auto tolerance = std::max(kTravelAbsoluteTolerance, candidate_travel * 0.2);
    std::vector<double> stable_travels;
    std::vector<double> stable_centers;
    for (const auto& [low, high] : runtime.half_strokes) {
        const auto travel = high - low;
        if (std::abs(travel - candidate_travel) > tolerance) continue;
        stable_travels.push_back(travel);
        stable_centers.push_back((low + high) * 0.5);
    }
    runtime.status.stable_half_strokes = static_cast<unsigned int>(stable_travels.size());
    if (stable_travels.size() < kTravelRequiredStableHalfStrokes) return;

    runtime.profile = PreferredTravelProfile{median(stable_centers), median(stable_travels)};
    runtime.cache[runtime.profile_key] = *runtime.profile;
    runtime.transition_at = now;
    // Manual Gain must begin observing the newly expanded primary motion,
    // rather than retaining the smaller pre-learning envelope.
    gain_envelope_valid_[axis] = false;
}

double MotionEngine::optimize_axis(const std::size_t axis, const double value, const std::string& profile_key, const std::chrono::microseconds now) {
    auto& runtime = travel_runtime_[axis];
    const auto& config = travel_configs_[axis];
    runtime.optimized_center.reset();
    if (!config.enabled) {
        runtime.status = {};
        return value;
    }
    if (profile_key != runtime.profile_key) reset_axis_travel_live_state(axis, profile_key);

    if (!runtime.profile) {
        if (!runtime.has_sample) {
            runtime.has_sample = true;
            runtime.anchor = value;
            runtime.extremum = value;
        } else if (runtime.direction == 0) {
            if (value >= runtime.anchor + kTravelReversalHysteresis) {
                runtime.direction = 1;
                runtime.extremum = value;
            } else if (value <= runtime.anchor - kTravelReversalHysteresis) {
                runtime.direction = -1;
                runtime.extremum = value;
            }
        } else if (runtime.direction > 0) {
            if (value > runtime.extremum) {
                runtime.extremum = value;
            } else if (value <= runtime.extremum - kTravelReversalHysteresis) {
                record_axis_turning_point(axis, runtime.extremum, now);
                runtime.direction = -1;
                runtime.extremum = value;
            }
        } else {
            if (value < runtime.extremum) {
                runtime.extremum = value;
            } else if (value >= runtime.extremum + kTravelReversalHysteresis) {
                record_axis_turning_point(axis, runtime.extremum, now);
                runtime.direction = 1;
                runtime.extremum = value;
            }
        }
    }

    if (!runtime.profile) {
        runtime.status.state = PreferredTravelState::Learning;
        runtime.status.applied_gain = 1.0;
        return value;
    }

    const auto source_travel = std::max(runtime.profile->travel, kEpsilon);
    const auto preferred_travel = config.preferred_maximum - config.preferred_minimum;
    const auto desired_gain = preferred_travel / source_travel;
    const auto gain = std::clamp(desired_gain, 1.0, config.maximum_gain);
    const auto enlarging = gain > 1.0 + kEpsilon;
    const auto expanded_travel = source_travel * gain;
    const auto target_center = enlarging
        ? config.preferred_minimum + expanded_travel * 0.5
        : runtime.profile->center;
    runtime.optimized_center = target_center;
    const auto optimized = clamp01(target_center + (value - runtime.profile->center) * gain);
    runtime.status.observed_travel = source_travel;
    runtime.status.applied_gain = gain;
    runtime.status.stable_half_strokes = static_cast<unsigned int>(kTravelRequiredStableHalfStrokes);
    runtime.status.state = desired_gain > config.maximum_gain + kEpsilon
        ? PreferredTravelState::Limited : PreferredTravelState::Locked;
    if (!runtime.transition_at) return optimized;
    const auto elapsed = std::max(std::chrono::microseconds::zero(), now - *runtime.transition_at);
    const auto blend = smoothstep01(static_cast<double>(elapsed.count()) /
                                    static_cast<double>(kTravelTransitionDuration.count()));
    if (elapsed >= kTravelTransitionDuration) runtime.transition_at.reset();
    return value + (optimized - value) * blend;
}

std::optional<EngineSnapshot> MotionEngine::calculate(const MotionFrame& frame) {
    const auto* reference = participant(frame, contact_.reference_participant, contact_.origin_bone);
    const auto* target_owner = participant(frame, {}, contact_.target_bone);
    const auto* origin = bone(reference, contact_.origin_bone);
    const auto* direction = bone(reference, contact_.direction_bone);
    const auto* tip = bone(reference, contact_.tip_bone);
    const auto* support = bone(reference, contact_.support_bone);
    const auto* target = bone(target_owner, contact_.target_bone);
    if (!frame.action_active || !origin || !direction || !tip || !support || !target) return std::nullopt;

    const auto axis = normalize(subtract(direction->position, origin->position));
    const auto length = magnitude(subtract(tip->position, origin->position));
    if (!axis || length <= kEpsilon) return std::nullopt;
    const auto delta = subtract(target->position, origin->position);
    auto axial = dot(delta, *axis);
    const auto closest = add(origin->position, scale(*axis, std::clamp(axial, 0.0, length)));
    const auto radial = subtract(target->position, closest);
    if (contact_.safety_distance_enabled &&
        magnitude(radial) > std::clamp(contact_.safety_distance_meters, 0.02, 0.50)) return std::nullopt;

    const auto body_plane = reference_plane(frame, reference);
    auto reference_right = body_plane
        ? normalize(project_on_plane(body_plane->tangent, *axis))
        : std::optional<Vec3>{};
    if (!reference_right) {
        reference_right = normalize(project_on_plane(local_axis(support->rotation, contact_.support_right_axis), *axis));
    }
    if (!reference_right) reference_right = normalize(project_on_plane(local_axis(support->rotation, contact_.support_up_axis), *axis));
    if (!reference_right) return std::nullopt;
    const auto reference_forward = normalize(cross(*reference_right, *axis));
    if (!reference_forward) return std::nullopt;

    const auto* secondary_target = contact_.target_secondary_bone.empty()
        ? nullptr
        : bone(target_owner, contact_.target_secondary_bone);
    const auto bilateral = secondary_target != nullptr;
    const auto contact_basis = bilateral ? std::optional<TargetBasis>{} : target_contact_basis(frame, target_owner);
    auto translation_delta = body_plane ? project_on_plane(delta, body_plane->normal) : radial;
    auto radial_distance = magnitude(radial);
    const auto plane_intersection_requested = !bilateral
        && frame.target_frame
        && (frame.target_frame->mode == "plane_intersection"
            || frame.target_frame->translation_mode == "plane_intersection");
    auto plane_intersection_used = false;
    auto plane_intersection_blended = false;
    if (plane_intersection_requested && contact_basis) {
        const auto alignment = dot(*axis, contact_basis->up);
        const auto absolute_alignment = std::abs(alignment);
        if (absolute_alignment > kEpsilon) {
            const auto intersection_distance = dot(
                subtract(contact_basis->origin, origin->position), contact_basis->up) / alignment;
            if (std::isfinite(intersection_distance)) {
                const auto blend = smoothstep01(
                    (absolute_alignment - kPlaneIntersectionBlendStartAlignment)
                    / (kPlaneIntersectionFullAlignment - kPlaneIntersectionBlendStartAlignment));
                const auto maximum_correction = length * kPlaneIntersectionMaximumCorrectionFraction;
                const auto correction = std::clamp(intersection_distance - axial, -maximum_correction, maximum_correction);
                axial += correction * blend;
                const auto intersection = add(origin->position, scale(*axis, axial));
                // Preserve the existing signs: translations describe the
                // target entrance relative to the Reference axis. The origin
                // anchors the entrance while the other landmarks determine
                // the plane normal used by the intersection.
                translation_delta = subtract(contact_basis->origin, intersection);
                radial_distance = magnitude(translation_delta);
                plane_intersection_used = blend > kEpsilon;
                plane_intersection_blended = plane_intersection_used && blend < 1.0 - kEpsilon;
            }
        }
    }
    const auto radius = length * contact_.radius_scale;
    const auto contact_valid = radial_distance <= radius && axial >= -radius && axial <= length + radius;
    if (contact_.require_contact && !contact_valid) return std::nullopt;

    const auto target_right = bilateral
        ? project_on_plane(subtract(target->position, secondary_target->position), *axis)
        : project_on_plane(contact_basis ? contact_basis->right : local_axis(target->rotation, contact_.target_right_axis), *axis);
    const auto target_up = bilateral ? *axis
        : contact_basis ? contact_basis->up
        : local_axis(target->rotation, contact_.target_up_axis);
    if (magnitude(target_right) <= kEpsilon) return std::nullopt;
    const auto wrapped_twist = signed_angle_degrees(*reference_right, target_right, *axis);
    const auto wrapped_roll = -signed_angle_degrees(*axis, project_on_plane(target_up, *reference_forward), *reference_forward);
    const auto wrapped_pitch = signed_angle_degrees(*axis, project_on_plane(target_up, *reference_right), *reference_right);
    const auto binding_key = reference->stable_key + "|" + target_owner->stable_key + "|" + frame.action_id + "|"
        + contact_.target_bone + "|" + contact_.target_secondary_bone + "|" + contact_.target_up_axis + "|" + contact_.target_right_axis
        + "|" + (frame.target_frame ? frame.target_frame->mode + ":" + frame.target_frame->source_bone + ":"
            + frame.target_frame->origin_bone + ":" + frame.target_frame->forward_bone + ":"
            + frame.target_frame->left_bone + ":" + frame.target_frame->right_bone + ":"
            + frame.target_frame->translation_mode : "single_bone");
    const auto travel_profile_key = frame.game_id + "|" + frame.action_id + "|" + reference->skeleton_id + "|"
        + target_owner->skeleton_id + "|" + contact_.origin_bone + "|" + contact_.direction_bone + "|"
        + contact_.tip_bone + "|" + contact_.target_bone + "|" + contact_.target_secondary_bone + "|"
        + (frame.target_frame ? frame.target_frame->mode + ":" + frame.target_frame->source_bone + ":"
            + frame.target_frame->origin_bone + ":" + frame.target_frame->translation_mode : "single_bone");
    const auto binding_changed = binding_key != angle_binding_key_;
    if (binding_changed || !twist_baseline_ || !roll_baseline_ || !pitch_baseline_) {
        twist_baseline_ = wrapped_twist;
        roll_baseline_ = wrapped_roll;
        pitch_baseline_ = wrapped_pitch;
    }
    angle_binding_key_ = binding_key;
    const auto twist = shortest_angle_delta(wrapped_twist, *twist_baseline_);
    // Signed angles are periodic. A physically continuous tilt can cross the
    // -180/+180 representation seam, which previously changed R1/R2 by almost
    // a full revolution in one frame. Like twist, express both tilts as the
    // shortest displacement from this binding's activation orientation.
    const auto roll = shortest_angle_delta(wrapped_roll, *roll_baseline_);
    const auto pitch = shortest_angle_delta(wrapped_pitch, *pitch_baseline_);

    Axes raw;
    const auto absolute_l0 = frame.direct_l0_min_meters && frame.direct_l0_max_meters
        ? range01(axial, *frame.direct_l0_min_meters, *frame.direct_l0_max_meters)
        : (bilateral || frame.l0_reference_length)
            ? range01(axial, 0.0, length)
            : range01(axial, contact_.l0_min_meters, contact_.l0_max_meters);
    raw[0] = absolute_l0;
    // Per-action calibration describes the game skeleton's local direction;
    // the global control remains a user override, so two inversions cancel.
    if (contact_.invert_l0 != frame.direct_l0_inverted) raw[0] = 1.0 - raw[0];
    raw[1] = symmetric01(dot(translation_delta, *reference_forward), contact_.lateral_range_meters);
    raw[2] = symmetric01(dot(translation_delta, *reference_right), contact_.lateral_range_meters);
    raw[3] = symmetric01(twist, contact_.twist_range_degrees);
    raw[4] = symmetric01(roll, contact_.tilt_range_degrees);
    raw[5] = symmetric01(pitch, contact_.tilt_range_degrees);
    for (std::size_t index = 0; index < raw.values.size(); ++index) {
        raw[index] = frame.active_axes[index]
            ? optimize_axis(index, raw[index], travel_profile_key, frame.monotonic_time)
            : 0.5;
    }
    std::array<PreferredTravelStatus, 6> travel_statuses;
    for (std::size_t index = 0; index < travel_statuses.size(); ++index) {
        travel_statuses[index] = travel_runtime_[index].status;
    }
    return EngineSnapshot{frame.sequence, frame.monotonic_time, MotionState::Active, raw, tune(raw, frame.active_axes, binding_key), travel_statuses,
        {true, contact_valid, contact_valid ? "ok" : "outside_contact_radius", length, radius, axial, radial_distance, twist, roll, pitch,
            bilateral ? "bilateral_reference_axis"
                : plane_intersection_blended ? "target_plane_blended"
                : plane_intersection_used ? "target_plane_intersection"
                : plane_intersection_requested ? "target_plane_fallback"
                : contact_basis ? "target_contact_frame" : "single_bone"},
        frame.action_id, frame.action_category};
}

Axes MotionEngine::tune(const Axes& raw, const std::array<bool, 6>& active_axes, const std::string& binding_key) {
    if (gain_binding_key_ != binding_key) {
        gain_binding_key_ = binding_key;
        gain_envelope_valid_.fill(false);
    }
    Axes result;
    for (std::size_t index = 0; index < result.values.size(); ++index) {
        if (active_axes[index]) {
            if (!gain_envelope_valid_[index]) {
                gain_min_[index] = raw[index];
                gain_max_[index] = raw[index];
                gain_envelope_valid_[index] = true;
            } else {
                gain_min_[index] = std::min(gain_min_[index], raw[index]);
                gain_max_[index] = std::max(gain_max_[index], raw[index]);
            }
        }
        const auto gain_center = travel_runtime_[index].optimized_center
            ? *travel_runtime_[index].optimized_center
            : gain_envelope_valid_[index]
            ? (gain_min_[index] + gain_max_[index]) * 0.5
            : tuning_[index].center;
        result[index] = tune_value(raw[index], tuning_[index], gain_center);
    }
    return result;
}

EngineSnapshot MotionEngine::apply_safety(EngineSnapshot next, const std::chrono::microseconds now) {
    if (next.contact.valid) {
        last_valid_ = next;
        last_valid_time_ = now;
        return next;
    }
    return process_missing(now);
}

EngineSnapshot MotionEngine::process(const MotionFrame& frame) {
    const auto now = frame.monotonic_time;
    if (const auto calculated = calculate(frame)) return apply_safety(*calculated, now);
    return process_missing(now);
}

EngineSnapshot MotionEngine::process_missing(const std::chrono::microseconds now) {
    if (!last_valid_) return {0, now, MotionState::Idle, {}, {}, {}, {false, false, "no_active_contact"}, {}, {}};
    const auto elapsed = now - last_valid_time_;
    if (elapsed <= safety_.hold_for) {
        auto snapshot = *last_valid_; snapshot.monotonic_time = now; snapshot.state = MotionState::Holding; snapshot.contact.valid = false; snapshot.contact.reason = "hold"; return snapshot;
    }
    const auto return_elapsed = elapsed - safety_.hold_for;
    const auto ratio = safety_.return_for.count() <= 0 ? 1.0 : std::clamp(static_cast<double>(return_elapsed.count()) / (safety_.return_for.count() * 1000.0), 0.0, 1.0);
    auto snapshot = *last_valid_;
    snapshot.monotonic_time = now;
    snapshot.state = ratio >= 1.0 ? MotionState::Idle : MotionState::Returning;
    snapshot.contact.valid = false;
    snapshot.contact.reason = ratio >= 1.0 ? "idle" : "returning";
    for (std::size_t index = 0; index < 6; ++index) {
        snapshot.raw_axes[index] = snapshot.raw_axes[index] + (0.5 - snapshot.raw_axes[index]) * ratio;
        const auto return_position = safety_.return_positions[index];
        snapshot.device_axes[index] += (return_position - snapshot.device_axes[index]) * ratio;
    }
    return snapshot;
}

const char* to_string(const MotionState state) noexcept {
    switch (state) {
    case MotionState::Active: return "active";
    case MotionState::Holding: return "holding";
    case MotionState::Returning: return "returning";
    case MotionState::Fault: return "fault";
    default: return "idle";
    }
}

const char* to_string(const PreferredTravelState state) noexcept {
    switch (state) {
    case PreferredTravelState::Learning: return "learning";
    case PreferredTravelState::Locked: return "locked";
    case PreferredTravelState::Limited: return "limited";
    default: return "disabled";
    }
}

} // namespace motion_bridge
