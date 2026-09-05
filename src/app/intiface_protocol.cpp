#include "intiface_protocol.hpp"

#include <QJsonArray>

#include <algorithm>
#include <cmath>

namespace intiface_protocol {
namespace {

constexpr int kMinimumCommandDurationMs = 20;
constexpr int kMaximumCommandDurationMs = 100;

bool read_range(const QJsonObject& object, const char* key, int& minimum, int& maximum) {
    const auto range = object.value(QLatin1String(key)).toArray();
    if (range.size() != 2) return false;
    minimum = range.at(0).toInt();
    maximum = range.at(1).toInt();
    return maximum >= minimum;
}

std::optional<PositionFeature> feature_from_output(const QJsonObject& feature,
                                                   const PositionCommandType command_type) {
    const auto output = feature.value(QStringLiteral("Output")).toObject();
    const auto key = command_type == PositionCommandType::HwPositionWithDuration
        ? QStringLiteral("HwPositionWithDuration") : QStringLiteral("Position");
    const auto properties = output.value(key).toObject();
    if (properties.isEmpty()) return std::nullopt;

    PositionFeature result;
    result.command_type = command_type;
    result.feature_index = feature.value(QStringLiteral("FeatureIndex")).toInt(-1);
    if (result.feature_index < 0 ||
        !read_range(properties, "Value", result.value_minimum, result.value_maximum)) {
        return std::nullopt;
    }
    if (command_type == PositionCommandType::HwPositionWithDuration &&
        !read_range(properties, "Duration", result.duration_minimum, result.duration_maximum)) {
        return std::nullopt;
    }
    return result;
}

} // namespace

std::optional<PositionFeature> find_position_feature(const QJsonObject& device) {
    const auto features = device.value(QStringLiteral("DeviceFeatures")).toObject();

    // Prefer timed position for any stroker that advertises both command types.
    for (auto it = features.begin(); it != features.end(); ++it) {
        auto feature = it.value().toObject();
        if (!feature.contains(QStringLiteral("FeatureIndex"))) {
            feature.insert(QStringLiteral("FeatureIndex"), it.key().toInt());
        }
        if (const auto result = feature_from_output(feature, PositionCommandType::HwPositionWithDuration)) {
            return result;
        }
    }
    for (auto it = features.begin(); it != features.end(); ++it) {
        auto feature = it.value().toObject();
        if (!feature.contains(QStringLiteral("FeatureIndex"))) {
            feature.insert(QStringLiteral("FeatureIndex"), it.key().toInt());
        }
        if (const auto result = feature_from_output(feature, PositionCommandType::Position)) return result;
    }
    return std::nullopt;
}

int map_position(const double normalized_position, const PositionFeature& feature) {
    return static_cast<int>(std::lround(feature.value_minimum +
        std::clamp(normalized_position, 0.0, 1.0) *
            (feature.value_maximum - feature.value_minimum)));
}

int timed_position_duration_ms(const std::chrono::milliseconds elapsed,
                               const std::optional<int> manual_duration_ms) {
    if (manual_duration_ms) return std::clamp(*manual_duration_ms, 50, 100);
    return std::clamp(static_cast<int>(elapsed.count()),
                      kMinimumCommandDurationMs, kMaximumCommandDurationMs);
}

QJsonObject output_command(const int request_id, const int device_index,
                           const PositionFeature& feature,
                           const double normalized_position, const int duration_ms) {
    QJsonObject command;
    if (feature.command_type == PositionCommandType::HwPositionWithDuration) {
        command.insert(QStringLiteral("HwPositionWithDuration"), QJsonObject{
            {QStringLiteral("Value"), map_position(normalized_position, feature)},
            {QStringLiteral("Duration"), std::clamp(duration_ms,
                                                     feature.duration_minimum,
                                                     feature.duration_maximum)}
        });
    } else {
        command.insert(QStringLiteral("Position"), QJsonObject{
            {QStringLiteral("Value"), map_position(normalized_position, feature)}
        });
    }
    return QJsonObject{{QStringLiteral("OutputCmd"), QJsonObject{
        {QStringLiteral("Id"), request_id},
        {QStringLiteral("DeviceIndex"), device_index},
        {QStringLiteral("FeatureIndex"), feature.feature_index},
        {QStringLiteral("Command"), command}
    }}};
}

} // namespace intiface_protocol
