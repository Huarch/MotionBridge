#pragma once

#include "intiface_protocol.hpp"
#include "motion_bridge/types.hpp"

#include <QObject>
#include <QElapsedTimer>
#include <QSerialPort>
#include <QTimer>
#include <QUdpSocket>
#include <QWebSocket>

#include <array>
#include <chrono>
#include <cstddef>

class DeviceRouter final : public QObject {
    Q_OBJECT

public:
    enum class Mode { None, Usb, Wifi, Intiface };
    Q_ENUM(Mode)

    explicit DeviceRouter(QObject* parent = nullptr);
    void set_mode(Mode mode);
    void set_usb_port(const QString& port);
    void set_wifi_endpoint(const QString& host, quint16 port);
    void set_intiface_url(const QUrl& url);
    void set_auto_reconnect(bool enabled);
    void set_intiface_target_time(bool automatic, int duration_ms);
    void set_axis_return_position(std::size_t axis, double position);
    void set_armed(bool armed);
    [[nodiscard]] Mode mode() const noexcept;
    [[nodiscard]] bool armed() const noexcept;
    [[nodiscard]] bool arming() const noexcept;
    void send(const motion_bridge::Axes& axes, std::chrono::milliseconds interval);
    void emergency_stop();

signals:
    void status_changed(const QString& text, bool connected);

private slots:
    void on_serial_error(QSerialPort::SerialPortError error);
    void on_wifi_connected();
    void on_wifi_error(QAbstractSocket::SocketError error);
    void on_intiface_connected();
    void on_intiface_disconnected();
    void on_intiface_message(const QString& message);
    void on_intiface_error(QAbstractSocket::SocketError error);
    void on_intiface_output_tick();
    void retry_connection();

private:
    void ensure_transport();
    void send_output(const motion_bridge::Axes& axes, std::chrono::milliseconds interval, bool force_full = false);
    void request_intiface_devices();
    void send_intiface_message(const QJsonObject& message);
    void stop_intiface_output();
    void begin_reconnect(const QString& reason);
    void cancel_reconnect();
    void reset_output_tracking();
    bool try_select_intiface_device(const QJsonObject& device);

    Mode mode_{Mode::None};
    bool armed_{};
    bool arming_{};
    QString usb_port_;
    QString wifi_host_{"tcode.local"};
    quint16 wifi_port_{8000};
    QUrl intiface_url_{"ws://127.0.0.1:12345"};
    QSerialPort* serial_{};
    QUdpSocket* udp_{};
    QWebSocket* intiface_{};
    QTimer* intiface_output_timer_{};
    QTimer* reconnect_timer_{};
    int intiface_request_id_{1};
    int intiface_device_index_{-1};
    std::optional<intiface_protocol::PositionFeature> intiface_position_feature_;
    bool auto_reconnect_{true};
    bool reconnecting_{};
    int reconnect_attempt_{};
    bool intiface_target_time_automatic_{true};
    int intiface_target_time_ms_{50};
    QElapsedTimer intiface_output_clock_;
    motion_bridge::Axes pending_intiface_axes_;
    bool pending_intiface_axes_valid_{};
    motion_bridge::Axes return_positions_;
    motion_bridge::Axes last_sent_axes_;
    std::array<bool, 6> last_sent_valid_{};
};
