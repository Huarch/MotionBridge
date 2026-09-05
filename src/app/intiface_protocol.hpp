#pragma once

#include <QJsonObject>

#include <chrono>
#include <optional>

namespace intiface_protocol {

enum class PositionCommandType {
    Position,
    HwPositionWithDuration,
};

struct PositionFeature {
    PositionCommandType command_type{PositionCommandType::Position};
    int feature_index{-1};
    int value_minimum{};
    int value_maximum{100};
    int duration_minimum{};
    int duration_maximum{100000};
};

inline constexpr int kTimedPositionIntervalMs = 50;

std::optional<PositionFeature> find_position_feature(const QJsonObject& device);
int map_position(double normalized_position, const PositionFeature& feature);
int timed_position_duration_ms(std::chrono::milliseconds elapsed,
                               std::optional<int> manual_duration_ms = std::nullopt);
QJsonObject output_command(int request_id, int device_index,
                           const PositionFeature& feature,
                           double normalized_position, int duration_ms);

} // namespace intiface_protocol
