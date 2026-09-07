#include "device_router.hpp"
#include "motion_bridge_settings.hpp"
#include "realtime_pipeline.hpp"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

namespace {

class ReferenceParticipantSettingRestore final {
public:
    ReferenceParticipantSettingRestore() {
        const auto settings = motion_bridge_settings();
        existed_ = settings.contains(key_);
        if (existed_) value_ = settings.value(key_);
    }

    ~ReferenceParticipantSettingRestore() {
        auto settings = motion_bridge_settings();
        if (existed_) settings.setValue(key_, value_);
        else settings.remove(key_);
        settings.sync();
    }

private:
    const QString key_{QStringLiteral("contact/referenceParticipant")};
    bool existed_{};
    QVariant value_;
};

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    ReferenceParticipantSettingRestore restore_setting;
    RealtimePipeline pipeline;
    auto* router = pipeline.findChild<DeviceRouter*>();
    if (router == nullptr) {
        std::cerr << "Realtime pipeline did not create a device router\n";
        return EXIT_FAILURE;
    }

    router->set_mode(DeviceRouter::Mode::Wifi);
    router->set_armed(true);
    if (!router->armed()) {
        std::cerr << "Wi-Fi router did not arm for participant-routing test\n";
        return EXIT_FAILURE;
    }

    bool emergency_stop_seen = false;
    QObject::connect(router, &DeviceRouter::status_changed, [&emergency_stop_seen](const QString& status, const bool) {
        if (status == QStringLiteral("Output disarmed safely")) emergency_stop_seen = true;
    });

    pipeline.set_reference_participant(QStringLiteral("male:1"));
    if (!router->armed() || emergency_stop_seen) {
        std::cerr << "Selecting a reference participant disarmed the output transport\n";
        return EXIT_FAILURE;
    }

    std::cout << "Realtime participant-routing tests passed\n";
    return EXIT_SUCCESS;
}
