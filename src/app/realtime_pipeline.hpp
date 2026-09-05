#pragma once

#include "device_router.hpp"
#include "fallen_doll_input.hpp"
#include "motion_bridge/motion_engine.hpp"
#include "motion_bridge/output_signal_processor.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

class RealtimePipeline final : public QObject {
    Q_OBJECT

public:
    explicit RealtimePipeline(QObject* parent = nullptr);

public slots:
    void start();
    void stop();
    void set_armed(bool armed);
    void emergency_stop();
    void set_output_mode(const QString& mode);
    void set_usb_port(const QString& port);
    void set_wifi_endpoint(const QString& host, int port);
    void set_intiface_url(const QString& url);
    void set_auto_reconnect(bool enabled);
    void set_intiface_target_time_automatic(bool automatic);
    void set_intiface_target_time_ms(int duration_ms);
    void set_axis_gain(int axis, double value);
    void set_axis_inverted(int axis, bool inverted);
    void set_axis_range(int axis, double minimum, double maximum);
    void set_axis_preferred_travel_enabled(int axis, bool enabled);
    void set_axis_preferred_travel_range(int axis, double minimum_percent, double maximum_percent);
    void reset_axis_travel_learning(int axis);
    void set_safety_distance_enabled(bool enabled);
    void set_safety_distance_cm(double centimeters);
    void set_output_rate_hz(int value);
    void set_soft_start_enabled(bool enabled);
    void set_soft_start_duration_ms(int value);
    void set_axis_output_enabled(int axis, bool enabled);
    void set_axis_return_position(int axis, double value);
    void set_axis_speed_limit_enabled(int axis, bool enabled);
    void set_axis_speed_limit(int axis, double value);
    void set_axis_smart_limit_enabled(int axis, bool enabled);
    void set_axis_smart_limit_input(int axis, int input_axis);
    void set_axis_smart_limit_mode(int axis, const QString& mode);
    void set_axis_smart_limit_target(int axis, double value);
    void set_axis_smart_limit_curve(int axis, double lower_input, double lower_factor,
                                    double upper_input, double upper_factor);
    void set_stream_path(const QString& path);
    void set_theme(const QString& theme);
    void set_reference_participant(const QString& reference);

signals:
    void snapshot_ready(const QString& state, const QString& action, const QString& reference_plane,
                        const QVariantList& raw, const QVariantList& smart_limit_inputs,
                        const QVariantList& device, const QVariantList& preferred_travel_statuses);
    void stream_status_changed(bool connected, const QString& text);
    void output_status_changed(const QString& text, bool armed, bool connecting, const QString& mode);
    void spool_path_changed(const QString& path);
    void connection_settings_changed(const QString& usb_port, const QString& wifi_host, int wifi_port,
                                     const QString& intiface_url, bool auto_reconnect);
    void axis_gains_changed(const QVariantList& gains);
    void axis_ranges_changed(const QVariantList& minimums, const QVariantList& maximums);
    void axis_travel_preferences_changed(const QVariantList& preferences);
    void contact_settings_changed(bool safety_distance_enabled, double safety_distance_cm);
    void output_processing_settings_changed(int rate_hz,
                                            bool intiface_target_time_automatic,
                                            int intiface_target_time_ms,
                                            bool soft_start_enabled, int soft_start_duration_ms,
                                            const QVariantList& axis_output_settings);
    void theme_changed(const QString& theme);
    void reference_participants_changed(const QVariantList& references);
    void reference_participant_changed(const QString& reference);

private slots:
    void on_frame(motion_bridge::MotionFrame frame);
    void on_output_tick();

private:
    void load_settings();
    void save_settings();
    void publish_snapshot();
    void publish_connection_settings();
    void publish_axis_gains();
    void publish_axis_ranges();
    void publish_axis_travel_preferences();
    void publish_contact_settings();
    void publish_output_processing_settings();
    void publish_participant_choices(const motion_bridge::MotionFrame& frame);
    void publish_reference_participant();
    void reset_participant_cache();
    [[nodiscard]] bool cache_reference_participant(const QString& key, const QString& label);
    [[nodiscard]] std::chrono::microseconds now() const;

    motion_bridge::MotionEngine engine_;
    motion_bridge::OutputSignalProcessor output_processor_;
    FallenDollInput* input_{};
    DeviceRouter* device_{};
    QElapsedTimer clock_;
    QTimer* output_timer_{};
    motion_bridge::EngineSnapshot target_snapshot_;
    motion_bridge::EngineSnapshot snapshot_;
    std::optional<motion_bridge::EngineSnapshot> last_ui_snapshot_;
    std::chrono::microseconds last_input_time_{};
    std::chrono::microseconds last_output_time_{};
    bool device_output_active_{};
    int output_rate_hz_{50};
    QString spool_path_;
    QString usb_port_;
    QString wifi_host_{"tcode.local"};
    int wifi_port_{8000};
    QString intiface_url_{"ws://127.0.0.1:12345"};
    bool auto_reconnect_{true};
    bool intiface_target_time_automatic_{true};
    int intiface_target_time_ms_{50};
    QString theme_{"dark"};
    QString reference_plane_label_;
    QVariantList reference_participants_;
    QString participant_cache_action_id_;
};
