#include "intiface_protocol.hpp"

#include <QJsonArray>
#include <QJsonObject>

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {

QJsonObject feature(const int index, const QJsonObject& output) {
    return QJsonObject{{"FeatureIndex", index}, {"Output", output}};
}

} // namespace

int main() {
    using intiface_protocol::PositionCommandType;

    const QJsonObject simulated{{"DeviceFeatures", QJsonObject{
        {"0", feature(0, QJsonObject{
            {"Position", QJsonObject{{"Value", QJsonArray{0, 100}}}},
            {"HwPositionWithDuration", QJsonObject{
                {"Value", QJsonArray{0, 100}}, {"Duration", QJsonArray{0, 100000}}
            }}
        })}
    }}};
    const auto simulated_feature = intiface_protocol::find_position_feature(simulated);
    if (!simulated_feature || simulated_feature->command_type != PositionCommandType::HwPositionWithDuration) {
        std::cerr << "timed position was not preferred for the simulated stroker\n";
        return EXIT_FAILURE;
    }

    const QJsonObject handy{{"DeviceFeatures", QJsonObject{
        {"0", feature(0, QJsonObject{
            {"HwPositionWithDuration", QJsonObject{
                {"Value", QJsonArray{0, 100}}, {"Duration", QJsonArray{0, 100000}}
            }}
        })}
    }}};
    const auto handy_feature = intiface_protocol::find_position_feature(handy);
    if (!handy_feature || intiface_protocol::map_position(1.0, *handy_feature) != 100 ||
        intiface_protocol::map_position(0.012, *handy_feature) != 1) {
        std::cerr << "Handy 0-100 position mapping is incorrect\n";
        return EXIT_FAILURE;
    }

    const auto message = intiface_protocol::output_command(7, 2, *handy_feature, 0.9, 60);
    const auto output = message.value("OutputCmd").toObject();
    const auto command = output.value("Command").toObject();
    const auto timed = command.value("HwPositionWithDuration").toObject();
    if (output.value("DeviceIndex").toInt() != 2 || output.value("FeatureIndex").toInt() != 0 ||
        timed.value("Value").toInt() != 90 || timed.value("Duration").toInt() != 60 ||
        command.contains("Position")) {
        std::cerr << "Handy timed position command is incorrect\n";
        return EXIT_FAILURE;
    }

    if (intiface_protocol::timed_position_duration_ms(std::chrono::milliseconds{50}) != 50 ||
        intiface_protocol::timed_position_duration_ms(std::chrono::milliseconds{12}) != 20 ||
        intiface_protocol::timed_position_duration_ms(std::chrono::milliseconds{140}) != 100 ||
        intiface_protocol::timed_position_duration_ms(std::chrono::milliseconds{50}, 65) != 65 ||
        intiface_protocol::timed_position_duration_ms(std::chrono::milliseconds{50}, 30) != 50 ||
        intiface_protocol::timed_position_duration_ms(std::chrono::milliseconds{50}, 130) != 100) {
        std::cerr << "automatic timed-position duration did not follow the real output interval\n";
        return EXIT_FAILURE;
    }

    const QJsonObject plain{{"DeviceFeatures", QJsonObject{
        {"3", feature(3, QJsonObject{{"Position", QJsonObject{{"Value", QJsonArray{10, 90}}}}})}
    }}};
    const auto plain_feature = intiface_protocol::find_position_feature(plain);
    if (!plain_feature || plain_feature->command_type != PositionCommandType::Position ||
        intiface_protocol::map_position(0.25, *plain_feature) != 30) {
        std::cerr << "plain Position fallback is incorrect\n";
        return EXIT_FAILURE;
    }

    std::cout << "Intiface protocol tests passed\n";
    return EXIT_SUCCESS;
}
