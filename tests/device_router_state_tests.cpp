#include "device_router.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QTimer>

#include <cstdlib>
#include <iostream>

namespace {

bool deliver_intiface_message(DeviceRouter& router, const QJsonObject& message) {
    const auto payload = QString::fromUtf8(
        QJsonDocument(QJsonArray{message}).toJson(QJsonDocument::Compact));
    return QMetaObject::invokeMethod(
        &router, "on_intiface_message", Qt::DirectConnection, Q_ARG(QString, payload));
}

QJsonObject position_device(const int device_index) {
    return QJsonObject{
        {"DeviceIndex", device_index},
        {"DeviceName", "Test Position Device"},
        {"DeviceFeatures", QJsonObject{{"0", QJsonObject{
            {"FeatureIndex", 0},
            {"Output", QJsonObject{{"HwPositionWithDuration", QJsonObject{
                {"Value", QJsonArray{0, 100}},
                {"Duration", QJsonArray{0, 100000}}
            }}}}
        }}}}
    };
}

QTimer* timed_position_timer(DeviceRouter& router) {
    const auto timers = router.findChildren<QTimer*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* timer : timers) {
        if (!timer->isSingleShot() &&
            timer->interval() == intiface_protocol::kTimedPositionIntervalMs) {
            return timer;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    DeviceRouter router;

    router.set_mode(DeviceRouter::Mode::Usb);
    router.set_armed(true);
    if (router.armed() || router.arming()) {
        std::cerr << "USB without a selected port was not reported as failed\n";
        return EXIT_FAILURE;
    }

    router.set_usb_port(QStringLiteral("MOTIONBRIDGE_TEST_MISSING_PORT"));
    router.set_armed(true);
    if (router.armed() || !router.arming()) {
        std::cerr << "Unavailable USB port did not enter automatic reconnect\n";
        return EXIT_FAILURE;
    }
    router.set_auto_reconnect(false);
    if (router.armed() || router.arming()) {
        std::cerr << "Disabling USB automatic reconnect did not disarm output\n";
        return EXIT_FAILURE;
    }
    router.set_auto_reconnect(true);

    router.set_mode(DeviceRouter::Mode::Wifi);
    router.set_armed(true);
    if (!router.armed() || router.arming()) {
        std::cerr << "Connectionless Wi-Fi UDP output was not reported as locally armed\n";
        return EXIT_FAILURE;
    }
    if (!QMetaObject::invokeMethod(
            &router, "on_wifi_error", Qt::DirectConnection,
            Q_ARG(QAbstractSocket::SocketError, QAbstractSocket::NetworkError)) ||
        router.armed() || !router.arming()) {
        std::cerr << "Wi-Fi socket error did not enter automatic reconnect\n";
        return EXIT_FAILURE;
    }
    router.emergency_stop();

    router.set_mode(DeviceRouter::Mode::Intiface);
    router.set_armed(true);
    if (router.armed() || !router.arming()) {
        std::cerr << "Intiface was reported ARMED before device discovery completed\n";
        return EXIT_FAILURE;
    }

    const QJsonObject unsupported_device{
        {"DeviceAdded", QJsonObject{
            {"DeviceIndex", 3},
            {"DeviceName", "Input Only Device"},
            {"DeviceFeatures", QJsonObject{}}
        }}
    };
    if (!deliver_intiface_message(router, unsupported_device) ||
        router.armed() || !router.arming()) {
        std::cerr << "Unsupported Intiface device incorrectly cancelled discovery\n";
        return EXIT_FAILURE;
    }

    const auto supported_device = position_device(7);
    if (!deliver_intiface_message(router, QJsonObject{{"DeviceAdded", supported_device}}) ||
        !router.armed() || router.arming()) {
        std::cerr << "Compatible Intiface position device did not complete arming\n";
        return EXIT_FAILURE;
    }
    const auto* output_timer = timed_position_timer(router);
    if (output_timer == nullptr || !output_timer->isActive() ||
        output_timer->timerType() != Qt::PreciseTimer) {
        std::cerr << "Timed Intiface device did not start its precise 20 Hz clock\n";
        return EXIT_FAILURE;
    }

    if (!deliver_intiface_message(router, QJsonObject{{"DeviceRemoved", QJsonObject{{"DeviceIndex", 7}}}}) ||
        router.armed() || !router.arming()) {
        std::cerr << "Removing the selected Intiface device did not enter automatic reconnect\n";
        return EXIT_FAILURE;
    }
    if (output_timer->isActive()) {
        std::cerr << "Timed Intiface clock remained active after device removal\n";
        return EXIT_FAILURE;
    }

    if (!deliver_intiface_message(router, QJsonObject{{"DeviceAdded", supported_device}}) ||
        !router.armed() || router.arming()) {
        std::cerr << "Reappearing Intiface device did not resume requested output\n";
        return EXIT_FAILURE;
    }

    if (!QMetaObject::invokeMethod(&router, "on_intiface_disconnected", Qt::DirectConnection) ||
        router.armed() || !router.arming()) {
        std::cerr << "Unexpected Intiface server disconnect did not schedule reconnect\n";
        return EXIT_FAILURE;
    }

    router.emergency_stop();
    if (!deliver_intiface_message(router, QJsonObject{{"DeviceAdded", supported_device}}) ||
        router.armed() || router.arming()) {
        std::cerr << "Intiface DeviceAdded rearmed output after an explicit stop\n";
        return EXIT_FAILURE;
    }

    router.set_armed(true);
    if (!deliver_intiface_message(router, QJsonObject{{"DeviceAdded", supported_device}}) ||
        !router.armed() || router.arming()) {
        std::cerr << "Compatible Intiface device did not rearm for reconnect-disabled test\n";
        return EXIT_FAILURE;
    }
    router.set_auto_reconnect(false);
    if (!QMetaObject::invokeMethod(&router, "on_intiface_disconnected", Qt::DirectConnection) ||
        router.armed() || router.arming()) {
        std::cerr << "Disabled automatic reconnect did not leave output disarmed\n";
        return EXIT_FAILURE;
    }

    router.set_auto_reconnect(true);
    router.set_armed(true);
    router.emergency_stop();
    if (!deliver_intiface_message(router, QJsonObject{{"DeviceAdded", supported_device}}) ||
        router.armed() || router.arming()) {
        std::cerr << "Intiface DeviceAdded rearmed output after an explicit stop\n";
        return EXIT_FAILURE;
    }

    std::cout << "Device router state tests passed\n";
    return EXIT_SUCCESS;
}
