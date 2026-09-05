#pragma once

#include "realtime_pipeline.hpp"

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class MotionBridgeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString streamStatus READ stream_status NOTIFY statusChanged)
    Q_PROPERTY(bool streamConnected READ stream_connected NOTIFY statusChanged)
    Q_PROPERTY(QString outputStatus READ output_status NOTIFY statusChanged)
    Q_PROPERTY(QString motionState READ motion_state NOTIFY snapshotChanged)
    Q_PROPERTY(QString actionName READ action_name NOTIFY snapshotChanged)
    Q_PROPERTY(QString referencePlane READ reference_plane NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList rawAxes READ raw_axes NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList smartLimitInputAxes READ smart_limit_input_axes NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList deviceAxes READ device_axes NOTIFY snapshotChanged)
    Q_PROPERTY(bool armed READ armed NOTIFY statusChanged)
    Q_PROPERTY(bool outputConnecting READ output_connecting NOTIFY statusChanged)
    Q_PROPERTY(QString outputMode READ output_mode NOTIFY statusChanged)
    Q_PROPERTY(QString spoolPath READ spool_path NOTIFY statusChanged)
    Q_PROPERTY(QString usbPort READ usb_port NOTIFY settingsChanged)
    Q_PROPERTY(QString wifiHost READ wifi_host NOTIFY settingsChanged)
    Q_PROPERTY(int wifiPort READ wifi_port NOTIFY settingsChanged)
    Q_PROPERTY(QString intifaceUrl READ intiface_url NOTIFY settingsChanged)
    Q_PROPERTY(bool autoReconnect READ auto_reconnect NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList axisGains READ axis_gains NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList axisMinimums READ axis_minimums NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList axisMaximums READ axis_maximums NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList axisTravelPreferences READ axis_travel_preferences NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList axisTravelStatuses READ axis_travel_statuses NOTIFY snapshotChanged)
    Q_PROPERTY(bool safetyDistanceEnabled READ safety_distance_enabled NOTIFY settingsChanged)
    Q_PROPERTY(double safetyDistanceCm READ safety_distance_cm NOTIFY settingsChanged)
    Q_PROPERTY(int outputRateHz READ output_rate_hz NOTIFY settingsChanged)
    Q_PROPERTY(bool intifaceTargetTimeAutomatic READ intiface_target_time_automatic NOTIFY settingsChanged)
    Q_PROPERTY(int intifaceTargetTimeMs READ intiface_target_time_ms NOTIFY settingsChanged)
    Q_PROPERTY(bool softStartEnabled READ soft_start_enabled NOTIFY settingsChanged)
    Q_PROPERTY(int softStartDurationMs READ soft_start_duration_ms NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList axisOutputSettings READ axis_output_settings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList referenceParticipants READ reference_participants NOTIFY participantChoicesChanged)
    Q_PROPERTY(QString referenceParticipant READ reference_participant NOTIFY settingsChanged)
    Q_PROPERTY(QStringList usbPorts READ usb_ports NOTIFY usbPortsChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(int displayScalePercent READ display_scale_percent NOTIFY settingsChanged)
    Q_PROPERTY(bool displayScaleRestartRequired READ display_scale_restart_required NOTIFY settingsChanged)

public:
    explicit MotionBridgeController(QObject* parent = nullptr);
    ~MotionBridgeController() override;

    [[nodiscard]] QString stream_status() const;
    [[nodiscard]] bool stream_connected() const;
    [[nodiscard]] QString output_status() const;
    [[nodiscard]] QString motion_state() const;
    [[nodiscard]] QString action_name() const;
    [[nodiscard]] QString reference_plane() const;
    [[nodiscard]] QVariantList raw_axes() const;
    [[nodiscard]] QVariantList smart_limit_input_axes() const;
    [[nodiscard]] QVariantList device_axes() const;
    [[nodiscard]] bool armed() const;
    [[nodiscard]] bool output_connecting() const;
    [[nodiscard]] QString output_mode() const;
    [[nodiscard]] QString spool_path() const;
    [[nodiscard]] QString usb_port() const;
    [[nodiscard]] QString wifi_host() const;
    [[nodiscard]] int wifi_port() const;
    [[nodiscard]] QString intiface_url() const;
    [[nodiscard]] bool auto_reconnect() const;
    [[nodiscard]] QVariantList axis_gains() const;
    [[nodiscard]] QVariantList axis_minimums() const;
    [[nodiscard]] QVariantList axis_maximums() const;
    [[nodiscard]] QVariantList axis_travel_preferences() const;
    [[nodiscard]] QVariantList axis_travel_statuses() const;
    [[nodiscard]] bool safety_distance_enabled() const;
    [[nodiscard]] double safety_distance_cm() const;
    [[nodiscard]] int output_rate_hz() const;
    [[nodiscard]] bool intiface_target_time_automatic() const;
    [[nodiscard]] int intiface_target_time_ms() const;
    [[nodiscard]] bool soft_start_enabled() const;
    [[nodiscard]] int soft_start_duration_ms() const;
    [[nodiscard]] QVariantList axis_output_settings() const;
    [[nodiscard]] QVariantList reference_participants() const;
    [[nodiscard]] QString reference_participant() const;
    [[nodiscard]] QStringList usb_ports() const;
    [[nodiscard]] QString theme() const;
    [[nodiscard]] int display_scale_percent() const;
    [[nodiscard]] bool display_scale_restart_required() const;

    Q_INVOKABLE void set_armed(bool armed);
    Q_INVOKABLE void emergency_stop();
    Q_INVOKABLE void set_output_mode(const QString& mode);
    Q_INVOKABLE void set_usb_port(const QString& port);
    Q_INVOKABLE void set_wifi_endpoint(const QString& host, int port);
    Q_INVOKABLE void set_intiface_url(const QString& url);
    Q_INVOKABLE void set_auto_reconnect(bool enabled);
    Q_INVOKABLE void set_intiface_target_time_automatic(bool automatic);
    Q_INVOKABLE void set_intiface_target_time_ms(int duration_ms);
    Q_INVOKABLE void set_axis_gain(int axis, double value);
    Q_INVOKABLE void set_axis_inverted(int axis, bool inverted);
    Q_INVOKABLE void set_axis_range(int axis, double minimum, double maximum);
    Q_INVOKABLE void set_axis_preferred_travel_enabled(int axis, bool enabled);
    Q_INVOKABLE void set_axis_preferred_travel_range(int axis, double minimum_percent, double maximum_percent);
    Q_INVOKABLE void reset_axis_travel_learning(int axis);
    Q_INVOKABLE void set_safety_distance_enabled(bool enabled);
    Q_INVOKABLE void set_safety_distance_cm(double centimeters);
    Q_INVOKABLE void set_output_rate_hz(int value);
    Q_INVOKABLE void set_soft_start_enabled(bool enabled);
    Q_INVOKABLE void set_soft_start_duration_ms(int value);
    Q_INVOKABLE void set_axis_output_enabled(int axis, bool enabled);
    Q_INVOKABLE void set_axis_return_position(int axis, double value);
    Q_INVOKABLE void set_axis_speed_limit_enabled(int axis, bool enabled);
    Q_INVOKABLE void set_axis_speed_limit(int axis, double value);
    Q_INVOKABLE void set_axis_smart_limit_enabled(int axis, bool enabled);
    Q_INVOKABLE void set_axis_smart_limit_input(int axis, int input_axis);
    Q_INVOKABLE void set_axis_smart_limit_mode(int axis, const QString& mode);
    Q_INVOKABLE void set_axis_smart_limit_target(int axis, double value);
    Q_INVOKABLE void set_axis_smart_limit_curve(int axis, double lower_input, double lower_factor,
                                                double upper_input, double upper_factor);
    Q_INVOKABLE void set_stream_path(const QString& path);
    Q_INVOKABLE void refresh_usb_ports();
    Q_INVOKABLE void set_theme(const QString& theme);
    Q_INVOKABLE void set_reference_participant(const QString& reference);
    Q_INVOKABLE void set_display_scale_percent(int percent);
    Q_INVOKABLE QVariantMap primary_screen_available_geometry() const;

signals:
    void snapshotChanged();
    void statusChanged();
    void settingsChanged();
    void usbPortsChanged();
    void themeChanged();
    void participantChoicesChanged();

private:
    QThread realtime_thread_;
    RealtimePipeline* pipeline_{};
    QString stream_status_{tr("Starting real-time pipeline")};
    bool stream_connected_{};
    QString output_status_{tr("Output disarmed")};
    QString motion_state_{"idle"};
    QString action_name_;
    QString reference_plane_;
    QVariantList raw_axes_{0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    QVariantList smart_limit_input_axes_{0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    QVariantList device_axes_{0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    QString output_mode_{"none"};
    QString spool_path_;
    bool armed_{};
    bool output_connecting_{};
    QString usb_port_;
    QString wifi_host_{"tcode.local"};
    int wifi_port_{8000};
    QString intiface_url_{"ws://127.0.0.1:12345"};
    bool auto_reconnect_{true};
    QVariantList axis_gains_{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    QVariantList axis_minimums_{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    QVariantList axis_maximums_{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    QVariantList axis_travel_preferences_;
    QVariantList axis_travel_statuses_;
    bool safety_distance_enabled_{};
    double safety_distance_cm_{10.0};
    int output_rate_hz_{50};
    bool intiface_target_time_automatic_{true};
    int intiface_target_time_ms_{50};
    bool soft_start_enabled_{true};
    int soft_start_duration_ms_{600};
    QVariantList axis_output_settings_;
    QVariantList reference_participants_;
    QString reference_participant_;
    QStringList usb_ports_;
    QTimer usb_scan_timer_;
    QString theme_{"dark"};
    int display_scale_percent_{};
    int startup_display_scale_percent_{};
};
