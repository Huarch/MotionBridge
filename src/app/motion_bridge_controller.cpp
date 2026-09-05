#include "motion_bridge_controller.hpp"
#include "motion_bridge_settings.hpp"

#include <QGuiApplication>
#include <QMetaObject>
#include <QScreen>
#include <QSerialPortInfo>

#include <algorithm>

MotionBridgeController::MotionBridgeController(QObject* parent) : QObject(parent), pipeline_(new RealtimePipeline) {
    auto ui_settings = motion_bridge_settings();
    display_scale_percent_ = normalize_ui_scale_percent(ui_settings.value("ui/displayScalePercent", 0).toInt());
    startup_display_scale_percent_ = display_scale_percent_;
    pipeline_->moveToThread(&realtime_thread_);
    connect(&realtime_thread_, &QThread::finished, pipeline_, &QObject::deleteLater);
    connect(pipeline_, &RealtimePipeline::snapshot_ready, this, [this](const QString& state, const QString& action, const QString& reference_plane, const QVariantList& raw, const QVariantList& smart_limit_inputs, const QVariantList& device, const QVariantList& preferred_travel_statuses) {
        motion_state_ = state; action_name_ = action; reference_plane_ = reference_plane; raw_axes_ = raw; smart_limit_input_axes_ = smart_limit_inputs; device_axes_ = device;
        axis_travel_statuses_ = preferred_travel_statuses;
        emit snapshotChanged();
    });
    connect(pipeline_, &RealtimePipeline::stream_status_changed, this, [this](const bool connected, const QString& status) {
        stream_connected_ = connected; stream_status_ = status; emit statusChanged();
    });
    connect(pipeline_, &RealtimePipeline::output_status_changed, this, [this](const QString& status, const bool armed, const bool connecting, const QString& mode) {
        output_status_ = status; armed_ = armed; output_connecting_ = connecting; output_mode_ = mode; emit statusChanged();
    });
    connect(pipeline_, &RealtimePipeline::spool_path_changed, this, [this](const QString& path) { spool_path_ = path; emit statusChanged(); });
    connect(pipeline_, &RealtimePipeline::connection_settings_changed, this, [this](const QString& usb_port, const QString& wifi_host, const int wifi_port, const QString& intiface_url, const bool auto_reconnect) {
        usb_port_ = usb_port; wifi_host_ = wifi_host; wifi_port_ = wifi_port; intiface_url_ = intiface_url; auto_reconnect_ = auto_reconnect; emit settingsChanged();
    });
    connect(pipeline_, &RealtimePipeline::axis_gains_changed, this, [this](const QVariantList& gains) { axis_gains_ = gains; emit settingsChanged(); });
    connect(pipeline_, &RealtimePipeline::axis_ranges_changed, this, [this](const QVariantList& minimums, const QVariantList& maximums) {
        axis_minimums_ = minimums;
        axis_maximums_ = maximums;
        emit settingsChanged();
    });
    connect(pipeline_, &RealtimePipeline::axis_travel_preferences_changed, this, [this](const QVariantList& preferences) {
        axis_travel_preferences_ = preferences;
        emit settingsChanged();
    });
    connect(pipeline_, &RealtimePipeline::contact_settings_changed, this, [this](const bool safety_distance_enabled, const double safety_distance_cm) {
        safety_distance_enabled_ = safety_distance_enabled;
        safety_distance_cm_ = safety_distance_cm;
        emit settingsChanged();
    });
    connect(pipeline_, &RealtimePipeline::output_processing_settings_changed, this,
        [this](const int rate_hz, const bool intiface_target_time_automatic,
               const int intiface_target_time_ms, const bool soft_start_enabled,
               const int soft_start_duration_ms,
               const QVariantList& axis_output_settings) {
            output_rate_hz_ = rate_hz;
            intiface_target_time_automatic_ = intiface_target_time_automatic;
            intiface_target_time_ms_ = intiface_target_time_ms;
            soft_start_enabled_ = soft_start_enabled;
            soft_start_duration_ms_ = soft_start_duration_ms;
            axis_output_settings_ = axis_output_settings;
            emit settingsChanged();
        });
    connect(pipeline_, &RealtimePipeline::reference_participants_changed, this, [this](const QVariantList& references) {
        reference_participants_ = references;
        emit participantChoicesChanged();
    });
    connect(pipeline_, &RealtimePipeline::reference_participant_changed, this, [this](const QString& reference) {
        reference_participant_ = reference;
        emit settingsChanged();
    });
    connect(pipeline_, &RealtimePipeline::theme_changed, this, [this](const QString& theme) { theme_ = theme; emit themeChanged(); });
    usb_scan_timer_.setInterval(1500);
    connect(&usb_scan_timer_, &QTimer::timeout, this, &MotionBridgeController::refresh_usb_ports);
    refresh_usb_ports();
    usb_scan_timer_.start();
    realtime_thread_.start(QThread::HighPriority);
    QMetaObject::invokeMethod(pipeline_, "start", Qt::QueuedConnection);
}

MotionBridgeController::~MotionBridgeController() {
    if (pipeline_ != nullptr && realtime_thread_.isRunning()) {
        QMetaObject::invokeMethod(pipeline_, "stop", Qt::BlockingQueuedConnection);
        realtime_thread_.quit();
        realtime_thread_.wait();
    }
}

QString MotionBridgeController::stream_status() const { return stream_status_; }
bool MotionBridgeController::stream_connected() const { return stream_connected_; }
QString MotionBridgeController::output_status() const { return output_status_; }
QString MotionBridgeController::motion_state() const { return motion_state_; }
QString MotionBridgeController::action_name() const { return action_name_; }
QString MotionBridgeController::reference_plane() const { return reference_plane_; }
QVariantList MotionBridgeController::raw_axes() const { return raw_axes_; }
QVariantList MotionBridgeController::smart_limit_input_axes() const { return smart_limit_input_axes_; }
QVariantList MotionBridgeController::device_axes() const { return device_axes_; }
bool MotionBridgeController::armed() const { return armed_; }
bool MotionBridgeController::output_connecting() const { return output_connecting_; }
QString MotionBridgeController::output_mode() const { return output_mode_; }
QString MotionBridgeController::spool_path() const { return spool_path_; }
QString MotionBridgeController::usb_port() const { return usb_port_; }
QString MotionBridgeController::wifi_host() const { return wifi_host_; }
int MotionBridgeController::wifi_port() const { return wifi_port_; }
QString MotionBridgeController::intiface_url() const { return intiface_url_; }
bool MotionBridgeController::auto_reconnect() const { return auto_reconnect_; }
QVariantList MotionBridgeController::axis_gains() const { return axis_gains_; }
QVariantList MotionBridgeController::axis_minimums() const { return axis_minimums_; }
QVariantList MotionBridgeController::axis_maximums() const { return axis_maximums_; }
QVariantList MotionBridgeController::axis_travel_preferences() const { return axis_travel_preferences_; }
QVariantList MotionBridgeController::axis_travel_statuses() const { return axis_travel_statuses_; }
bool MotionBridgeController::safety_distance_enabled() const { return safety_distance_enabled_; }
double MotionBridgeController::safety_distance_cm() const { return safety_distance_cm_; }
int MotionBridgeController::output_rate_hz() const { return output_rate_hz_; }
bool MotionBridgeController::intiface_target_time_automatic() const { return intiface_target_time_automatic_; }
int MotionBridgeController::intiface_target_time_ms() const { return intiface_target_time_ms_; }
bool MotionBridgeController::soft_start_enabled() const { return soft_start_enabled_; }
int MotionBridgeController::soft_start_duration_ms() const { return soft_start_duration_ms_; }
QVariantList MotionBridgeController::axis_output_settings() const { return axis_output_settings_; }
QVariantList MotionBridgeController::reference_participants() const { return reference_participants_; }
QString MotionBridgeController::reference_participant() const { return reference_participant_; }
QStringList MotionBridgeController::usb_ports() const { return usb_ports_; }
QString MotionBridgeController::theme() const { return theme_; }
int MotionBridgeController::display_scale_percent() const { return display_scale_percent_; }
bool MotionBridgeController::display_scale_restart_required() const { return display_scale_percent_ != startup_display_scale_percent_; }

void MotionBridgeController::set_armed(const bool armed) { QMetaObject::invokeMethod(pipeline_, "set_armed", Qt::QueuedConnection, Q_ARG(bool, armed)); }
void MotionBridgeController::emergency_stop() { QMetaObject::invokeMethod(pipeline_, "emergency_stop", Qt::QueuedConnection); }
void MotionBridgeController::set_output_mode(const QString& mode) { QMetaObject::invokeMethod(pipeline_, "set_output_mode", Qt::QueuedConnection, Q_ARG(QString, mode)); }
void MotionBridgeController::set_usb_port(const QString& port) { QMetaObject::invokeMethod(pipeline_, "set_usb_port", Qt::QueuedConnection, Q_ARG(QString, port)); }
void MotionBridgeController::set_wifi_endpoint(const QString& host, const int port) { QMetaObject::invokeMethod(pipeline_, "set_wifi_endpoint", Qt::QueuedConnection, Q_ARG(QString, host), Q_ARG(int, port)); }
void MotionBridgeController::set_intiface_url(const QString& url) { QMetaObject::invokeMethod(pipeline_, "set_intiface_url", Qt::QueuedConnection, Q_ARG(QString, url)); }
void MotionBridgeController::set_auto_reconnect(const bool enabled) { QMetaObject::invokeMethod(pipeline_, "set_auto_reconnect", Qt::QueuedConnection, Q_ARG(bool, enabled)); }
void MotionBridgeController::set_intiface_target_time_automatic(const bool automatic) { QMetaObject::invokeMethod(pipeline_, "set_intiface_target_time_automatic", Qt::QueuedConnection, Q_ARG(bool, automatic)); }
void MotionBridgeController::set_intiface_target_time_ms(const int duration_ms) { QMetaObject::invokeMethod(pipeline_, "set_intiface_target_time_ms", Qt::QueuedConnection, Q_ARG(int, duration_ms)); }
void MotionBridgeController::set_axis_gain(const int axis, const double value) { QMetaObject::invokeMethod(pipeline_, "set_axis_gain", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(double, value)); }
void MotionBridgeController::set_axis_inverted(const int axis, const bool inverted) { QMetaObject::invokeMethod(pipeline_, "set_axis_inverted", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(bool, inverted)); }
void MotionBridgeController::set_axis_range(const int axis, const double minimum, const double maximum) { QMetaObject::invokeMethod(pipeline_, "set_axis_range", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(double, minimum), Q_ARG(double, maximum)); }
void MotionBridgeController::set_axis_preferred_travel_enabled(const int axis, const bool enabled) { QMetaObject::invokeMethod(pipeline_, "set_axis_preferred_travel_enabled", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(bool, enabled)); }
void MotionBridgeController::set_axis_preferred_travel_range(const int axis, const double minimum_percent, const double maximum_percent) { QMetaObject::invokeMethod(pipeline_, "set_axis_preferred_travel_range", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(double, minimum_percent), Q_ARG(double, maximum_percent)); }
void MotionBridgeController::reset_axis_travel_learning(const int axis) { QMetaObject::invokeMethod(pipeline_, "reset_axis_travel_learning", Qt::QueuedConnection, Q_ARG(int, axis)); }
void MotionBridgeController::set_safety_distance_enabled(const bool enabled) { QMetaObject::invokeMethod(pipeline_, "set_safety_distance_enabled", Qt::QueuedConnection, Q_ARG(bool, enabled)); }
void MotionBridgeController::set_safety_distance_cm(const double centimeters) { QMetaObject::invokeMethod(pipeline_, "set_safety_distance_cm", Qt::QueuedConnection, Q_ARG(double, centimeters)); }
void MotionBridgeController::set_output_rate_hz(const int value) { QMetaObject::invokeMethod(pipeline_, "set_output_rate_hz", Qt::QueuedConnection, Q_ARG(int, value)); }
void MotionBridgeController::set_soft_start_enabled(const bool enabled) { QMetaObject::invokeMethod(pipeline_, "set_soft_start_enabled", Qt::QueuedConnection, Q_ARG(bool, enabled)); }
void MotionBridgeController::set_soft_start_duration_ms(const int value) { QMetaObject::invokeMethod(pipeline_, "set_soft_start_duration_ms", Qt::QueuedConnection, Q_ARG(int, value)); }
void MotionBridgeController::set_axis_output_enabled(const int axis, const bool enabled) { QMetaObject::invokeMethod(pipeline_, "set_axis_output_enabled", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(bool, enabled)); }
void MotionBridgeController::set_axis_return_position(const int axis, const double value) { QMetaObject::invokeMethod(pipeline_, "set_axis_return_position", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(double, value)); }
void MotionBridgeController::set_axis_speed_limit_enabled(const int axis, const bool enabled) { QMetaObject::invokeMethod(pipeline_, "set_axis_speed_limit_enabled", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(bool, enabled)); }
void MotionBridgeController::set_axis_speed_limit(const int axis, const double value) { QMetaObject::invokeMethod(pipeline_, "set_axis_speed_limit", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(double, value)); }
void MotionBridgeController::set_axis_smart_limit_enabled(const int axis, const bool enabled) { QMetaObject::invokeMethod(pipeline_, "set_axis_smart_limit_enabled", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(bool, enabled)); }
void MotionBridgeController::set_axis_smart_limit_input(const int axis, const int input_axis) { QMetaObject::invokeMethod(pipeline_, "set_axis_smart_limit_input", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(int, input_axis)); }
void MotionBridgeController::set_axis_smart_limit_mode(const int axis, const QString& mode) { QMetaObject::invokeMethod(pipeline_, "set_axis_smart_limit_mode", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(QString, mode)); }
void MotionBridgeController::set_axis_smart_limit_target(const int axis, const double value) { QMetaObject::invokeMethod(pipeline_, "set_axis_smart_limit_target", Qt::QueuedConnection, Q_ARG(int, axis), Q_ARG(double, value)); }
void MotionBridgeController::set_axis_smart_limit_curve(const int axis, const double lower_input, const double lower_factor,
                                                         const double upper_input, const double upper_factor) {
    QMetaObject::invokeMethod(pipeline_, "set_axis_smart_limit_curve", Qt::QueuedConnection,
                              Q_ARG(int, axis), Q_ARG(double, lower_input), Q_ARG(double, lower_factor),
                              Q_ARG(double, upper_input), Q_ARG(double, upper_factor));
}
void MotionBridgeController::set_stream_path(const QString& path) { QMetaObject::invokeMethod(pipeline_, "set_stream_path", Qt::QueuedConnection, Q_ARG(QString, path)); }
void MotionBridgeController::set_reference_participant(const QString& reference) { QMetaObject::invokeMethod(pipeline_, "set_reference_participant", Qt::QueuedConnection, Q_ARG(QString, reference)); }

QVariantMap MotionBridgeController::primary_screen_available_geometry() const {
    const auto* screen = QGuiApplication::primaryScreen();
    const auto area = screen ? screen->availableGeometry() : QRect{};
    return {{"x", area.x()}, {"y", area.y()}, {"width", area.width()}, {"height", area.height()}};
}

void MotionBridgeController::refresh_usb_ports() {
    QStringList ports;
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        const auto name = info.portName().trimmed();
        if (!name.isEmpty() && !ports.contains(name, Qt::CaseInsensitive)) ports.push_back(name);
    }
    std::sort(ports.begin(), ports.end(), [](const QString& left, const QString& right) {
        return QString::localeAwareCompare(left, right) < 0;
    });
    if (ports == usb_ports_) return;
    usb_ports_ = ports;
    emit usbPortsChanged();
}

void MotionBridgeController::set_theme(const QString& theme) {
    QMetaObject::invokeMethod(pipeline_, "set_theme", Qt::QueuedConnection, Q_ARG(QString, theme));
}

void MotionBridgeController::set_display_scale_percent(const int percent) {
    const auto normalized = normalize_ui_scale_percent(percent);
    if (display_scale_percent_ == normalized) return;
    display_scale_percent_ = normalized;
    auto settings = motion_bridge_settings();
    settings.setValue("ui/displayScalePercent", display_scale_percent_);
    settings.sync();
    emit settingsChanged();
}
