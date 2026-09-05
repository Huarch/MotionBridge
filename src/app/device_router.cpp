#include "device_router.hpp"

#include "motion_bridge/tcode.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

using namespace motion_bridge;

namespace {
constexpr int kInitialReconnectDelayMs = 500;
constexpr int kMaximumReconnectDelayMs = 5000;
}

DeviceRouter::DeviceRouter(QObject* parent) : QObject(parent) {
    serial_ = new QSerialPort(this);
    udp_ = new QUdpSocket(this);
    intiface_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    intiface_output_timer_ = new QTimer(this);
    reconnect_timer_ = new QTimer(this);
    intiface_output_timer_->setTimerType(Qt::PreciseTimer);
    intiface_output_timer_->setInterval(intiface_protocol::kTimedPositionIntervalMs);
    reconnect_timer_->setSingleShot(true);
    serial_->setBaudRate(115200);
    serial_->setDataBits(QSerialPort::Data8);
    serial_->setParity(QSerialPort::NoParity);
    serial_->setStopBits(QSerialPort::OneStop);
    serial_->setFlowControl(QSerialPort::NoFlowControl);
    connect(serial_, &QSerialPort::errorOccurred, this, &DeviceRouter::on_serial_error);
    connect(udp_, &QUdpSocket::connected, this, &DeviceRouter::on_wifi_connected);
    connect(udp_, &QUdpSocket::errorOccurred, this, &DeviceRouter::on_wifi_error);
    connect(intiface_, &QWebSocket::textMessageReceived, this, &DeviceRouter::on_intiface_message);
    connect(intiface_, &QWebSocket::errorOccurred, this, &DeviceRouter::on_intiface_error);
    connect(intiface_, &QWebSocket::disconnected, this, &DeviceRouter::on_intiface_disconnected);
    connect(intiface_, &QWebSocket::connected, this, &DeviceRouter::on_intiface_connected);
    connect(intiface_output_timer_, &QTimer::timeout,
            this, &DeviceRouter::on_intiface_output_tick);
    connect(reconnect_timer_, &QTimer::timeout, this, &DeviceRouter::retry_connection);
}

void DeviceRouter::set_mode(const Mode mode) {
    if (mode_ == mode) return;
    emergency_stop();
    mode_ = mode;
    reset_output_tracking();
    ensure_transport();
}

void DeviceRouter::set_usb_port(const QString& port) {
    if (usb_port_ == port) return;
    emergency_stop();
    usb_port_ = port;
    reset_output_tracking();
    ensure_transport();
}

void DeviceRouter::set_wifi_endpoint(const QString& host, const quint16 port) {
    wifi_host_ = host;
    wifi_port_ = port;
}

void DeviceRouter::set_intiface_url(const QUrl& url) {
    if (intiface_url_ == url) return;
    intiface_->close();
    intiface_url_ = url;
    reset_output_tracking();
    ensure_transport();
}

void DeviceRouter::set_auto_reconnect(const bool enabled) {
    if (auto_reconnect_ == enabled) return;
    auto_reconnect_ = enabled;
    if (enabled || !reconnecting_) return;

    cancel_reconnect();
    armed_ = false;
    arming_ = false;
    intiface_device_index_ = -1;
    intiface_position_feature_.reset();
    reset_output_tracking();
    if (serial_->isOpen()) serial_->close();
    udp_->abort();
    if (intiface_->state() != QAbstractSocket::UnconnectedState) intiface_->abort();
    emit status_changed(tr("Automatic reconnect disabled; output disarmed"), false);
}

void DeviceRouter::set_intiface_target_time(const bool automatic,
                                            const int duration_ms) {
    intiface_target_time_automatic_ = automatic;
    intiface_target_time_ms_ = std::clamp(duration_ms, 50, 100);
}

void DeviceRouter::set_axis_return_position(const std::size_t axis, const double position) {
    if (axis >= return_positions_.values.size()) return;
    return_positions_[axis] = std::clamp(position, 0.0, 1.0);
}

void DeviceRouter::set_armed(const bool armed) {
    if (!armed) {
        emergency_stop();
        return;
    }
    reset_output_tracking();
    cancel_reconnect();
    arming_ = false;
    armed_ = false;
    arming_ = true;
    if (mode_ == Mode::Intiface) {
        intiface_device_index_ = -1;
        intiface_position_feature_.reset();
    }
    ensure_transport();
}

DeviceRouter::Mode DeviceRouter::mode() const noexcept { return mode_; }
bool DeviceRouter::armed() const noexcept { return armed_; }
bool DeviceRouter::arming() const noexcept { return arming_; }

void DeviceRouter::ensure_transport() {
    if (mode_ != Mode::Usb && serial_->isOpen()) serial_->close();
    if (mode_ != Mode::Intiface) {
        if (intiface_->state() != QAbstractSocket::UnconnectedState) intiface_->close();
    }
    if (!armed_ && !arming_) {
        emit status_changed(tr("Output disarmed"), false);
        return;
    }
    if (mode_ == Mode::Usb) {
        if (usb_port_.isEmpty()) {
            arming_ = false;
            emit status_changed(tr("Select a USB port"), false);
            return;
        }
        if (!serial_->isOpen()) {
            serial_->setPortName(usb_port_);
            if (!serial_->open(QIODevice::WriteOnly)) {
                if (auto_reconnect_) {
                    begin_reconnect(serial_->errorString());
                } else {
                    arming_ = false;
                    emit status_changed(serial_->errorString(), false);
                }
                return;
            }
            serial_->setDataTerminalReady(true);
            serial_->setRequestToSend(true);
        }
        armed_ = true;
        arming_ = false;
        cancel_reconnect();
        emit status_changed(tr("USB armed: %1").arg(usb_port_), true);
    } else if (mode_ == Mode::Wifi) {
        if (udp_->peerName() != wifi_host_ || udp_->peerPort() != wifi_port_) {
            udp_->abort();
            udp_->connectToHost(wifi_host_, wifi_port_);
        }
        armed_ = true;
        arming_ = false;
        cancel_reconnect();
        emit status_changed(tr("Wi-Fi UDP armed: %1:%2 (device connection cannot be verified)").arg(wifi_host_).arg(wifi_port_), true);
    } else if (mode_ == Mode::Intiface) {
        if (intiface_->state() == QAbstractSocket::UnconnectedState) {
            intiface_->open(intiface_url_);
        } else if (intiface_->state() == QAbstractSocket::ConnectedState) {
            request_intiface_devices();
        }
        emit status_changed(tr("Connecting to Intiface Desktop"), false);
    } else {
        armed_ = false;
        arming_ = false;
        emit status_changed(tr("Select an output method"), false);
    }
}

void DeviceRouter::send(const Axes& axes, const std::chrono::milliseconds interval) {
    if (!armed_) return;
    send_output(axes, interval);
}

void DeviceRouter::send_output(const Axes& axes, const std::chrono::milliseconds interval, const bool force_full) {
    if (mode_ == Mode::Usb) {
        if (!serial_->isOpen()) return;
        AxisMask dirty_axes{};
        for (std::size_t index = 0; index < dirty_axes.size(); ++index) {
            const auto current = static_cast<int>(std::floor(std::clamp(axes[index], 0.0, 1.0) * 9999.0 + 0.5));
            const auto previous = static_cast<int>(std::floor(std::clamp(last_sent_axes_[index], 0.0, 1.0) * 9999.0 + 0.5));
            dirty_axes[index] = force_full || !last_sent_valid_[index] || current != previous;
        }
        const auto payload = QByteArray::fromStdString(encode_tcode(axes, interval, dirty_axes));
        if (!payload.isEmpty() && serial_->write(payload) >= 0) {
            for (std::size_t index = 0; index < dirty_axes.size(); ++index) {
                if (!dirty_axes[index]) continue;
                last_sent_axes_[index] = axes[index];
                last_sent_valid_[index] = true;
            }
        }
        return;
    }
    if (mode_ == Mode::Wifi) {
        udp_->write(QByteArray::fromStdString(encode_tcode(axes, interval)));
        return;
    }
    if (mode_ == Mode::Intiface && intiface_->state() == QAbstractSocket::ConnectedState &&
        intiface_device_index_ >= 0 && intiface_position_feature_) {
        if (intiface_position_feature_->command_type ==
            intiface_protocol::PositionCommandType::HwPositionWithDuration) {
            if (!force_full) {
                pending_intiface_axes_ = axes;
                pending_intiface_axes_valid_ = true;
                return;
            }
            const auto manual_duration = intiface_target_time_automatic_
                ? std::optional<int>{}
                : std::optional<int>{intiface_target_time_ms_};
            const auto message = intiface_protocol::output_command(
                intiface_request_id_++, intiface_device_index_, *intiface_position_feature_,
                axes[0], intiface_protocol::timed_position_duration_ms(
                    std::chrono::milliseconds{intiface_protocol::kTimedPositionIntervalMs},
                    manual_duration));
            send_intiface_message(message);
            last_sent_axes_[0] = axes[0];
            last_sent_valid_[0] = true;
            return;
        }
        if (!force_full && last_sent_valid_[0] && std::abs(last_sent_axes_[0] - axes[0]) < 0.005) return;
        const auto message = intiface_protocol::output_command(
            intiface_request_id_++, intiface_device_index_, *intiface_position_feature_,
            axes[0], 0);
        send_intiface_message(message);
        last_sent_axes_[0] = axes[0];
        last_sent_valid_[0] = true;
        return;
    }
}

void DeviceRouter::emergency_stop() {
    cancel_reconnect();
    if (armed_) send_output(return_positions_, std::chrono::milliseconds{20}, true);
    stop_intiface_output();
    armed_ = false;
    arming_ = false;
    reset_output_tracking();
    if (serial_->isOpen()) serial_->close();
    emit status_changed(tr("Output disarmed safely"), false);
}

void DeviceRouter::request_intiface_devices() {
    const auto start_scanning = QJsonObject{{"StartScanning", QJsonObject{{"Id", intiface_request_id_++}}}};
    const auto request_device_list = QJsonObject{{"RequestDeviceList", QJsonObject{{"Id", intiface_request_id_++}}}};
    intiface_->sendTextMessage(QString::fromUtf8(
        QJsonDocument(QJsonArray{start_scanning, request_device_list}).toJson(QJsonDocument::Compact)));
}

void DeviceRouter::send_intiface_message(const QJsonObject& message) {
    intiface_->sendTextMessage(QString::fromUtf8(
        QJsonDocument(QJsonArray{message}).toJson(QJsonDocument::Compact)));
}

void DeviceRouter::stop_intiface_output() {
    if (intiface_->state() != QAbstractSocket::ConnectedState || intiface_device_index_ < 0) return;
    const auto message = QJsonObject{{"StopCmd", QJsonObject{
        {"Id", intiface_request_id_++}, {"DeviceIndex", intiface_device_index_}, {"Inputs", false}, {"Outputs", true}
    }}};
    send_intiface_message(message);
}

void DeviceRouter::on_serial_error(const QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError || mode_ != Mode::Usb ||
        (!armed_ && !arming_ && !reconnecting_)) return;
    const auto reason = serial_->errorString();
    if (auto_reconnect_) {
        begin_reconnect(reason);
        return;
    }

    armed_ = false;
    arming_ = false;
    reconnecting_ = false;
    reset_output_tracking();
    if (serial_->isOpen()) serial_->close();
    emit status_changed(reason, false);
}

void DeviceRouter::on_wifi_error(const QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    if (mode_ != Mode::Wifi ||
        (!armed_ && !arming_ && !reconnecting_)) return;
    const auto reason = udp_->errorString();
    if (auto_reconnect_) {
        begin_reconnect(reason);
        return;
    }

    armed_ = false;
    arming_ = false;
    reconnecting_ = false;
    reset_output_tracking();
    udp_->abort();
    emit status_changed(reason, false);
}

void DeviceRouter::on_wifi_connected() {
    if (mode_ != Mode::Wifi || !reconnecting_) return;
    armed_ = true;
    arming_ = false;
    cancel_reconnect();
    emit status_changed(
        tr("Wi-Fi UDP armed: %1:%2 (device connection cannot be verified)")
            .arg(wifi_host_).arg(wifi_port_), true);
}

void DeviceRouter::on_intiface_connected() {
    if (mode_ != Mode::Intiface || (!arming_ && !armed_ && !reconnecting_)) {
        intiface_->close();
        return;
    }
    reconnect_timer_->stop();
    const auto request = QJsonObject{{"RequestServerInfo", QJsonObject{
        {"Id", intiface_request_id_++}, {"ClientName", "Motion Bridge"},
        {"ProtocolVersionMajor", 4}, {"ProtocolVersionMinor", 0}
    }}};
    send_intiface_message(request);
    emit status_changed(tr("Connected to Intiface Desktop; checking devices"), false);
}

void DeviceRouter::on_intiface_disconnected() {
    if (mode_ != Mode::Intiface || (!armed_ && !arming_ && !reconnecting_)) return;
    if (auto_reconnect_) {
        begin_reconnect(tr("Intiface disconnected"));
        return;
    }

    armed_ = false;
    arming_ = false;
    intiface_device_index_ = -1;
    intiface_position_feature_.reset();
    reset_output_tracking();
    emit status_changed(tr("Intiface disconnected"), false);
}

void DeviceRouter::on_intiface_message(const QString& message) {
    const auto document = QJsonDocument::fromJson(message.toUtf8());
    const auto messages = document.isArray() ? document.array() : QJsonArray{document.object()};
    for (const auto& value : messages) {
        const auto object = value.toObject();
        if (object.contains("ServerInfo") && arming_) {
            request_intiface_devices();
        }
        if (object.contains("DeviceList") && arming_) {
            intiface_device_index_ = -1;
            intiface_position_feature_.reset();
            const auto devices = object.value("DeviceList").toObject().value("Devices").toObject();
            for (auto it = devices.begin(); it != devices.end(); ++it) {
                if (try_select_intiface_device(it.value().toObject())) break;
            }
            if (!intiface_position_feature_) {
                emit status_changed(tr("Waiting for an Intiface device with position output"), false);
            }
        }
        if (object.contains("DeviceAdded") && arming_ && !intiface_position_feature_) {
            try_select_intiface_device(object.value("DeviceAdded").toObject());
        }
        if (object.contains("DeviceRemoved") && armed_) {
            const auto removed_index = object.value("DeviceRemoved").toObject().value("DeviceIndex").toInt(-1);
            if (removed_index == intiface_device_index_) {
                armed_ = false;
                arming_ = auto_reconnect_;
                intiface_device_index_ = -1;
                intiface_position_feature_.reset();
                reset_output_tracking();
                if (auto_reconnect_) {
                    reconnecting_ = true;
                    emit status_changed(
                        tr("Intiface device disconnected; waiting for automatic reconnect"), false);
                } else {
                    emit status_changed(tr("Intiface device disconnected"), false);
                }
            }
        }
    }
}

bool DeviceRouter::try_select_intiface_device(const QJsonObject& device) {
    const auto position_feature = intiface_protocol::find_position_feature(device);
    const auto device_index = device.value("DeviceIndex").toInt(-1);
    if (!position_feature || device_index < 0) return false;

    intiface_device_index_ = device_index;
    intiface_position_feature_ = *position_feature;
    const auto device_name = device.value("DeviceName").toString();
    armed_ = true;
    arming_ = false;
    cancel_reconnect();
    reset_output_tracking();
    if (position_feature->command_type ==
        intiface_protocol::PositionCommandType::HwPositionWithDuration) {
        intiface_output_clock_.start();
        intiface_output_timer_->start();
    }
    const auto feature_name = position_feature->command_type ==
            intiface_protocol::PositionCommandType::HwPositionWithDuration
        ? tr("Position-with-duration") : tr("Position");
    emit status_changed(
        tr("Intiface armed: %1 (L0 → %2)").arg(device_name, feature_name), true);
    return true;
}

void DeviceRouter::reset_output_tracking() {
    last_sent_valid_.fill(false);
    pending_intiface_axes_valid_ = false;
    intiface_output_timer_->stop();
    intiface_output_clock_.invalidate();
}

void DeviceRouter::on_intiface_output_tick() {
    const auto elapsed = intiface_output_clock_.isValid()
        ? std::chrono::milliseconds{intiface_output_clock_.restart()}
        : std::chrono::milliseconds{intiface_protocol::kTimedPositionIntervalMs};
    if (!armed_ || mode_ != Mode::Intiface ||
        intiface_->state() != QAbstractSocket::ConnectedState ||
        intiface_device_index_ < 0 || !intiface_position_feature_ ||
        intiface_position_feature_->command_type !=
            intiface_protocol::PositionCommandType::HwPositionWithDuration ||
        !pending_intiface_axes_valid_) {
        return;
    }

    const auto next_position = intiface_protocol::map_position(
        pending_intiface_axes_[0], *intiface_position_feature_);
    if (last_sent_valid_[0] && next_position == intiface_protocol::map_position(
            last_sent_axes_[0], *intiface_position_feature_)) {
        return;
    }

    const auto message = intiface_protocol::output_command(
        intiface_request_id_++, intiface_device_index_, *intiface_position_feature_,
        pending_intiface_axes_[0], intiface_protocol::timed_position_duration_ms(
            elapsed,
            intiface_target_time_automatic_
                ? std::optional<int>{}
                : std::optional<int>{intiface_target_time_ms_}));
    send_intiface_message(message);
    last_sent_axes_[0] = pending_intiface_axes_[0];
    last_sent_valid_[0] = true;
}

void DeviceRouter::on_intiface_error(const QAbstractSocket::SocketError) {
    if (mode_ == Mode::Intiface && auto_reconnect_ &&
        (armed_ || arming_ || reconnecting_)) {
        begin_reconnect(intiface_->errorString());
        return;
    }
    armed_ = false;
    arming_ = false;
    reconnecting_ = false;
    intiface_device_index_ = -1;
    intiface_position_feature_.reset();
    reset_output_tracking();
    emit status_changed(intiface_->errorString(), false);
}

void DeviceRouter::begin_reconnect(const QString& reason) {
    if (mode_ == Mode::None || !auto_reconnect_ ||
        (!armed_ && !arming_ && !reconnecting_)) return;

    armed_ = false;
    arming_ = true;
    reconnecting_ = true;
    if (mode_ == Mode::Intiface) {
        intiface_device_index_ = -1;
        intiface_position_feature_.reset();
    }
    reset_output_tracking();
    if (reconnect_timer_->isActive()) return;

    const auto exponent = std::min(reconnect_attempt_, 4);
    const auto delay_ms = std::min(kInitialReconnectDelayMs * (1 << exponent),
                                   kMaximumReconnectDelayMs);
    ++reconnect_attempt_;
    reconnect_timer_->start(delay_ms);
    emit status_changed(
        tr("%1; reconnecting in %2 ms").arg(reason).arg(delay_ms), false);
}

void DeviceRouter::retry_connection() {
    if (mode_ == Mode::None || !auto_reconnect_ || !reconnecting_) return;
    emit status_changed(tr("Reconnecting output (attempt %1)").arg(reconnect_attempt_), false);
    if (mode_ == Mode::Usb) {
        if (serial_->isOpen()) serial_->close();
        ensure_transport();
    } else if (mode_ == Mode::Wifi) {
        udp_->abort();
        udp_->connectToHost(wifi_host_, wifi_port_);
    } else if (mode_ == Mode::Intiface) {
        if (intiface_->state() != QAbstractSocket::UnconnectedState) intiface_->abort();
        intiface_->open(intiface_url_);
    }
}

void DeviceRouter::cancel_reconnect() {
    reconnect_timer_->stop();
    reconnect_attempt_ = 0;
    reconnecting_ = false;
}
