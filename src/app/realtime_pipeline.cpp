#include "realtime_pipeline.hpp"
#include "motion_bridge_settings.hpp"

#include <QDir>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

using namespace motion_bridge;

namespace {

QVariantList axes_to_variant(const Axes& axes) {
    QVariantList result;
    for (const auto value : axes.values) result.push_back(value);
    return result;
}

DeviceRouter::Mode parse_mode(const QString& value) {
    if (value == u"usb") return DeviceRouter::Mode::Usb;
    if (value == u"wifi") return DeviceRouter::Mode::Wifi;
    if (value == u"intiface") return DeviceRouter::Mode::Intiface;
    return DeviceRouter::Mode::None;
}

QString mode_name(const DeviceRouter::Mode mode) {
    switch (mode) {
    case DeviceRouter::Mode::Usb: return QStringLiteral("usb");
    case DeviceRouter::Mode::Wifi: return QStringLiteral("wifi");
    case DeviceRouter::Mode::Intiface: return QStringLiteral("intiface");
    default: return QStringLiteral("none");
    }
}

} // namespace

RealtimePipeline::RealtimePipeline(QObject* parent) : QObject(parent) {
    // These objects perform all of their work on the realtime thread.  Make
    // them children of the pipeline before it is moved so that Qt migrates
    // their timers, sockets and file watcher with the pipeline as one unit.
    input_ = new FallenDollInput(this);
    device_ = new DeviceRouter(this);
    output_timer_ = new QTimer(this);
    output_timer_->setTimerType(Qt::PreciseTimer);
    output_timer_->setInterval(20);
}

void RealtimePipeline::start() {
    clock_.start();
    load_settings();
    // Signal processing also drives the axis cards and 3D preview. Keep it
    // live independently of whether hardware output is currently armed.
    output_processor_.arm(now());
    connect(input_, &FallenDollInput::frame_ready, this, &RealtimePipeline::on_frame);
    connect(input_, &FallenDollInput::connection_changed, this, [this](const bool connected, const QString& detail) { emit stream_status_changed(connected, detail); });
    connect(device_, &DeviceRouter::status_changed, this, [this](const QString& status, const bool) {
        if (device_->armed() && !device_output_active_) {
            const auto current = now();
            output_processor_.arm(current);
            last_output_time_ = current;
            device_output_active_ = true;
        } else if (!device_->armed() && !device_->arming()) {
            device_output_active_ = false;
        }
        emit output_status_changed(status, device_->armed(), device_->arming(), mode_name(device_->mode()));
    });
    connect(output_timer_, &QTimer::timeout, this, &RealtimePipeline::on_output_tick);
    output_timer_->start();
    input_->start();
    emit output_status_changed(device_->armed() ? tr("Output armed") : tr("Output disarmed"), device_->armed(), device_->arming(), mode_name(device_->mode()));
}

void RealtimePipeline::stop() { output_timer_->stop(); output_processor_.disarm(); device_->emergency_stop(); }
void RealtimePipeline::set_armed(const bool armed) {
    if (armed) {
        const auto current = now();
        output_processor_.arm(current);
        last_output_time_ = current;
    }
    device_->set_armed(armed);
    save_settings();
}
void RealtimePipeline::emergency_stop() { device_->emergency_stop(); }
void RealtimePipeline::set_output_mode(const QString& mode) {
    device_->set_mode(parse_mode(mode));
    save_settings();
}
void RealtimePipeline::set_usb_port(const QString& port) { usb_port_ = port.trimmed(); device_->set_usb_port(usb_port_); auto settings = motion_bridge_settings(); settings.setValue("device/usbPort", usb_port_); publish_connection_settings(); }
void RealtimePipeline::set_wifi_endpoint(const QString& host, const int port) { wifi_host_ = host.trimmed(); wifi_port_ = std::clamp(port, 1, 65535); device_->set_wifi_endpoint(wifi_host_, static_cast<quint16>(wifi_port_)); auto settings = motion_bridge_settings(); settings.setValue("device/wifiHost", wifi_host_); settings.setValue("device/wifiPort", wifi_port_); publish_connection_settings(); }
void RealtimePipeline::set_intiface_url(const QString& url) { intiface_url_ = url.trimmed(); device_->set_intiface_url(QUrl(intiface_url_)); auto settings = motion_bridge_settings(); settings.setValue("device/intifaceUrl", intiface_url_); publish_connection_settings(); }
void RealtimePipeline::set_auto_reconnect(const bool enabled) {
    if (auto_reconnect_ == enabled) return;
    auto_reconnect_ = enabled;
    device_->set_auto_reconnect(enabled);
    auto settings = motion_bridge_settings();
    settings.setValue("device/autoReconnect", enabled);
    publish_connection_settings();
}

void RealtimePipeline::set_intiface_target_time_automatic(const bool automatic) {
    if (intiface_target_time_automatic_ == automatic) return;
    intiface_target_time_automatic_ = automatic;
    device_->set_intiface_target_time(automatic, intiface_target_time_ms_);
    auto settings = motion_bridge_settings();
    settings.setValue("output/intifaceTargetTimeAutomatic", automatic);
    publish_output_processing_settings();
}

void RealtimePipeline::set_intiface_target_time_ms(const int duration_ms) {
    const auto next = std::clamp(duration_ms, 50, 100);
    if (intiface_target_time_ms_ == next) return;
    intiface_target_time_ms_ = next;
    device_->set_intiface_target_time(intiface_target_time_automatic_, next);
    auto settings = motion_bridge_settings();
    settings.setValue("output/intifaceTargetTimeMs", next);
    publish_output_processing_settings();
}
void RealtimePipeline::set_axis_gain(const int axis, const double value) {
    if (axis < 0 || axis >= 6) return;
    auto tuning = engine_.axis_tuning();
    auto& item = tuning[static_cast<std::size_t>(axis)];
    const auto next = std::clamp(value, 0.25, 4.0);
    if (std::abs(item.gain - next) < 0.0001) return;
    item.gain = next;
    engine_.set_axis_tuning(tuning);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("motion/%1/gain").arg(QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)])), item.gain);
    publish_axis_gains();
}

void RealtimePipeline::set_axis_inverted(const int axis, const bool inverted) {
    if (axis < 0 || axis >= 6) return;
    auto tuning = engine_.axis_tuning();
    auto& item = tuning[static_cast<std::size_t>(axis)];
    if (item.inverted == inverted) return;
    item.inverted = inverted;
    engine_.set_axis_tuning(tuning);
    const auto axis_name = QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)]);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("motion/%1/inverted").arg(axis_name), inverted);
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_range(const int axis, const double minimum, const double maximum) {
    if (axis < 0 || axis >= 6) return;
    auto tuning = engine_.axis_tuning();
    auto& item = tuning[static_cast<std::size_t>(axis)];
    const auto next_minimum = std::clamp(std::min(minimum, maximum), 0.0, 1.0);
    const auto next_maximum = std::clamp(std::max(minimum, maximum), 0.0, 1.0);
    if (std::abs(item.output_min - next_minimum) < 0.0001 &&
        std::abs(item.output_max - next_maximum) < 0.0001) return;
    item.output_min = next_minimum;
    item.output_max = next_maximum;
    engine_.set_axis_tuning(tuning);
    auto settings = motion_bridge_settings();
    const auto axis_name = QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)]);
    settings.setValue(QString("motion/%1/min").arg(axis_name), item.output_min);
    settings.setValue(QString("motion/%1/max").arg(axis_name), item.output_max);
    publish_axis_ranges();
}

void RealtimePipeline::set_axis_preferred_travel_enabled(const int axis, const bool enabled) {
    if (axis < 0 || axis >= 6) return;
    auto config = engine_.axis_travel_preferences()[static_cast<std::size_t>(axis)];
    if (config.enabled == enabled) return;
    config.enabled = enabled;
    engine_.set_axis_travel_preference(static_cast<std::size_t>(axis), config);
    auto settings = motion_bridge_settings();
    const auto axis_name = QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)]);
    settings.setValue(QString("motion/%1/preferredTravelEnabled").arg(axis_name), enabled);
    last_ui_snapshot_.reset();
    publish_axis_travel_preferences();
}

void RealtimePipeline::set_axis_preferred_travel_range(const int axis, const double minimum_percent, const double maximum_percent) {
    if (axis < 0 || axis >= 6) return;
    auto normalized_minimum = std::clamp(std::min(minimum_percent, maximum_percent), 0.0, 90.0);
    auto normalized_maximum = std::clamp(std::max(minimum_percent, maximum_percent), 10.0, 100.0);
    if (normalized_maximum - normalized_minimum < 10.0) {
        normalized_maximum = std::min(100.0, normalized_minimum + 10.0);
        normalized_minimum = std::max(0.0, normalized_maximum - 10.0);
    }
    auto config = engine_.axis_travel_preferences()[static_cast<std::size_t>(axis)];
    const auto preferred_minimum = normalized_minimum / 100.0;
    const auto preferred_maximum = normalized_maximum / 100.0;
    if (std::abs(config.preferred_minimum - preferred_minimum) < 0.0001 &&
        std::abs(config.preferred_maximum - preferred_maximum) < 0.0001) return;
    config.preferred_minimum = preferred_minimum;
    config.preferred_maximum = preferred_maximum;
    engine_.set_axis_travel_preference(static_cast<std::size_t>(axis), config);
    auto settings = motion_bridge_settings();
    const auto axis_name = QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)]);
    settings.setValue(QString("motion/%1/preferredTravelMinimum").arg(axis_name), static_cast<int>(std::lround(normalized_minimum * 100.0)));
    settings.setValue(QString("motion/%1/preferredTravelMaximum").arg(axis_name), static_cast<int>(std::lround(normalized_maximum * 100.0)));
    last_ui_snapshot_.reset();
    publish_axis_travel_preferences();
}

void RealtimePipeline::reset_axis_travel_learning(const int axis) {
    if (axis < 0 || axis >= 6) return;
    engine_.reset_axis_travel_learning(static_cast<std::size_t>(axis));
    last_ui_snapshot_.reset();
}

void RealtimePipeline::set_safety_distance_enabled(const bool enabled) {
    auto contact = engine_.contact_config();
    if (contact.safety_distance_enabled == enabled) return;
    contact.safety_distance_enabled = enabled;
    engine_.set_contact_config(contact);
    auto settings = motion_bridge_settings();
    settings.setValue("contact/safetyDistanceEnabled", enabled);
    last_ui_snapshot_.reset();
    publish_contact_settings();
}

void RealtimePipeline::set_safety_distance_cm(const double centimeters) {
    const auto normalized = std::clamp(centimeters, 2.0, 50.0);
    auto contact = engine_.contact_config();
    const auto meters = normalized / 100.0;
    if (std::abs(contact.safety_distance_meters - meters) < 0.0001) return;
    contact.safety_distance_meters = meters;
    engine_.set_contact_config(contact);
    auto settings = motion_bridge_settings();
    settings.setValue("contact/safetyDistanceCm", normalized);
    last_ui_snapshot_.reset();
    publish_contact_settings();
}

void RealtimePipeline::set_output_rate_hz(const int value) {
    const auto next = std::clamp(value, 20, 100);
    if (output_rate_hz_ == next) return;
    output_rate_hz_ = next;
    output_timer_->setInterval(std::max(1, static_cast<int>(std::lround(1000.0 / output_rate_hz_))));
    auto settings = motion_bridge_settings();
    settings.setValue("output/rateHz", output_rate_hz_);
    publish_output_processing_settings();
}

void RealtimePipeline::set_soft_start_enabled(const bool enabled) {
    auto config = output_processor_.config();
    if (config.soft_start_enabled == enabled) return;
    config.soft_start_enabled = enabled;
    output_processor_.set_config(config);
    auto settings = motion_bridge_settings();
    settings.setValue("output/softStartEnabled", enabled);
    publish_output_processing_settings();
}

void RealtimePipeline::set_soft_start_duration_ms(const int value) {
    auto config = output_processor_.config();
    const auto next = std::chrono::milliseconds{std::clamp(value, 0, 3000)};
    if (config.soft_start_for == next) return;
    config.soft_start_for = next;
    output_processor_.set_config(config);
    auto settings = motion_bridge_settings();
    settings.setValue("output/softStartDurationMs", static_cast<int>(next.count()));
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_output_enabled(const int axis, const bool enabled) {
    if (axis < 0 || axis >= 6) return;
    auto config = output_processor_.config();
    auto& item = config.axis_output_enabled[static_cast<std::size_t>(axis)];
    if (item == enabled) return;
    item = enabled;
    output_processor_.set_config(config);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("output/%1/axisOutputEnabled").arg(QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)])), enabled);
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_return_position(const int axis, const double value) {
    if (axis < 0 || axis >= 6) return;
    const auto next = std::clamp(value, 0.0, 1.0);
    auto config = output_processor_.config();
    auto& item = config.axis_return_position[static_cast<std::size_t>(axis)];
    if (std::abs(item - next) < 0.0001) return;
    item = next;
    output_processor_.set_config(config);
    engine_.set_axis_return_position(static_cast<std::size_t>(axis), next);
    device_->set_axis_return_position(static_cast<std::size_t>(axis), next);
    auto settings = motion_bridge_settings();
    const auto prefix = QString("output/%1/").arg(
        QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)]));
    settings.setValue(prefix + "returnPosition", next);
    settings.remove(prefix + "axisSafeValue");
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_speed_limit_enabled(const int axis, const bool enabled) {
    if (axis < 0 || axis >= 6) return;
    auto config = output_processor_.config();
    auto& item = config.speed_limit_enabled[static_cast<std::size_t>(axis)];
    if (item == enabled) return;
    item = enabled;
    output_processor_.set_config(config);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("output/%1/speedLimitEnabled").arg(QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)])), enabled);
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_speed_limit(const int axis, const double value) {
    if (axis < 0 || axis >= 6) return;
    auto config = output_processor_.config();
    const auto next = std::clamp(value, 0.25, 10.0);
    auto& speed = config.max_speed_per_second[static_cast<std::size_t>(axis)];
    if (std::abs(speed - next) < 0.0001) return;
    speed = next;
    output_processor_.set_config(config);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("output/%1/maxSpeed").arg(QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)])), next);
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_smart_limit_enabled(const int axis, const bool enabled) {
    if (axis < 0 || axis >= 6) return;
    auto config = output_processor_.config();
    auto& smart_limit = config.smart_limit[static_cast<std::size_t>(axis)];
    if (smart_limit.enabled == enabled) return;
    smart_limit.enabled = enabled;
    output_processor_.set_config(config);
    const auto actual = output_processor_.config().smart_limit[static_cast<std::size_t>(axis)].enabled;
    auto settings = motion_bridge_settings();
    settings.setValue(QString("output/%1/smartLimitEnabled").arg(QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)])), actual);
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_smart_limit_input(const int axis, const int input_axis) {
    if (axis < 0 || axis >= 6 || input_axis < 0 || input_axis >= 6) return;
    auto config = output_processor_.config();
    auto& smart_limit = config.smart_limit[static_cast<std::size_t>(axis)];
    const auto next = static_cast<std::size_t>(input_axis);
    if (smart_limit.input_axis == next) return;
    smart_limit.input_axis = next;
    output_processor_.set_config(config);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("output/%1/smartLimitInputAxis").arg(QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)])), input_axis);
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_smart_limit_mode(const int axis, const QString& mode) {
    if (axis < 0 || axis >= 6) return;
    const auto next = mode.compare("speed", Qt::CaseInsensitive) == 0 ? SmartLimitMode::Speed : SmartLimitMode::Value;
    auto config = output_processor_.config();
    auto& smart_limit = config.smart_limit[static_cast<std::size_t>(axis)];
    if (smart_limit.mode == next) return;
    smart_limit.mode = next;
    output_processor_.set_config(config);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("output/%1/smartLimitMode").arg(QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)])),
                      next == SmartLimitMode::Speed ? "speed" : "value");
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_smart_limit_target(const int axis, const double value) {
    if (axis < 0 || axis >= 6) return;
    const auto next = std::clamp(value, 0.0, 1.0);
    auto config = output_processor_.config();
    auto& target = config.smart_limit[static_cast<std::size_t>(axis)].target_value;
    if (std::abs(target - next) < 0.0001) return;
    target = next;
    output_processor_.set_config(config);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("output/%1/smartLimitTargetValue").arg(QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)])), next);
    publish_output_processing_settings();
}

void RealtimePipeline::set_axis_smart_limit_curve(const int axis, const double lower_input, const double lower_factor,
                                                   const double upper_input, const double upper_factor) {
    if (axis < 0 || axis >= 6) return;
    auto config = output_processor_.config();
    auto& smart_limit = config.smart_limit[static_cast<std::size_t>(axis)];
    if (std::abs(smart_limit.lower_input - lower_input) < 0.0001 &&
        std::abs(smart_limit.lower_factor - lower_factor) < 0.0001 &&
        std::abs(smart_limit.upper_input - upper_input) < 0.0001 &&
        std::abs(smart_limit.upper_factor - upper_factor) < 0.0001) return;
    smart_limit.lower_input = lower_input;
    smart_limit.lower_factor = lower_factor;
    smart_limit.upper_input = upper_input;
    smart_limit.upper_factor = upper_factor;
    output_processor_.set_config(config);
    const auto& actual = output_processor_.config().smart_limit[static_cast<std::size_t>(axis)];
    const auto axis_name = QString::fromLatin1(kAxisNames[static_cast<std::size_t>(axis)]);
    auto settings = motion_bridge_settings();
    settings.setValue(QString("output/%1/smartLimitLowerInput").arg(axis_name), actual.lower_input);
    settings.setValue(QString("output/%1/smartLimitLowerFactor").arg(axis_name), actual.lower_factor);
    settings.setValue(QString("output/%1/smartLimitUpperInput").arg(axis_name), actual.upper_input);
    settings.setValue(QString("output/%1/smartLimitUpperFactor").arg(axis_name), actual.upper_factor);
    publish_output_processing_settings();
}

void RealtimePipeline::set_stream_path(const QString& path) {
    spool_path_ = path;
    input_->set_spool_path(path);
    input_->start();
    reset_participant_cache();
    emit reference_participants_changed(reference_participants_);
    auto settings = motion_bridge_settings();
    settings.setValue("input/spoolPath", path);
    emit spool_path_changed(path);
}

void RealtimePipeline::set_theme(const QString& theme) {
    const auto normalized = theme.compare("light", Qt::CaseInsensitive) == 0 ? QStringLiteral("light") : QStringLiteral("dark");
    if (theme_ == normalized) return;
    theme_ = normalized;
    auto settings = motion_bridge_settings();
    settings.setValue("ui/theme", theme_);
    emit theme_changed(theme_);
}

void RealtimePipeline::set_reference_participant(const QString& reference) {
    auto contact = engine_.contact_config();
    const auto next_reference = reference.trimmed().toStdString();
    if (contact.reference_participant == next_reference) return;
    // Changing the source actor switches all six axes. Stop output first, then
    // require an explicit arm after a fresh frame confirms the new route.
    output_processor_.arm(now());
    device_->emergency_stop();
    contact.reference_participant = next_reference;
    engine_.set_contact_config(contact);
    input_->set_reference_participant(reference);
    auto settings = motion_bridge_settings();
    settings.setValue("contact/referenceParticipant", reference.trimmed());
    publish_reference_participant();
}

void RealtimePipeline::on_frame(MotionFrame frame) {
    reference_plane_label_.clear();
    if (frame.reference_plane) {
        const auto& plane = *frame.reference_plane;
        reference_plane_label_ = QString::fromStdString(plane.mode);
        if (!plane.center_bone.empty()) {
            reference_plane_label_ += " · " + QString::fromStdString(plane.center_bone) + " / "
                + QString::fromStdString(plane.left_bone) + " / " + QString::fromStdString(plane.right_bone);
        }
    }
    publish_participant_choices(frame);
    frame.monotonic_time = now();
    last_input_time_ = frame.monotonic_time;
    // Consume every game frame so calibration/envelopes stay exact, but keep
    // only the latest target for the fixed-rate device clock. File-system
    // notifications may deliver several 20 ms frames in one batch and must
    // never turn that batch into a burst of serial or socket writes.
    target_snapshot_ = engine_.process(frame);
}

void RealtimePipeline::on_output_tick() {
    const auto current = now();
    if (current - last_input_time_ >= std::chrono::milliseconds{25}) {
        target_snapshot_ = engine_.process_missing(current);
    }
    snapshot_ = target_snapshot_;
    const auto live_motion = snapshot_.state == MotionState::Active || snapshot_.state == MotionState::Holding;
    snapshot_.device_axes = output_processor_.process(target_snapshot_.device_axes, current, live_motion);
    if (device_->armed() || device_->arming()) {
        const auto nominal_ms = std::max(1, static_cast<int>(std::lround(1000.0 / output_rate_hz_)));
        const auto elapsed_us = last_output_time_.count() > 0 ? (current - last_output_time_).count() : nominal_ms * 1000LL;
        const auto interval_ms = std::clamp(static_cast<long long>(std::llround(static_cast<double>(elapsed_us) / 1000.0)), 1LL, 999LL);
        device_->send(snapshot_.device_axes, std::chrono::milliseconds{interval_ms});
    }
    last_output_time_ = current;
    publish_snapshot();
}

void RealtimePipeline::publish_snapshot() {
    if (last_ui_snapshot_) {
        bool changed = last_ui_snapshot_->state != snapshot_.state || last_ui_snapshot_->action_id != snapshot_.action_id;
        for (std::size_t index = 0; index < snapshot_.device_axes.values.size() && !changed; ++index) {
            changed = std::abs(last_ui_snapshot_->device_axes[index] - snapshot_.device_axes[index]) > 0.0001 ||
                std::abs(last_ui_snapshot_->raw_axes[index] - snapshot_.raw_axes[index]) > 0.0001 ||
                last_ui_snapshot_->preferred_travel[index].state != snapshot_.preferred_travel[index].state ||
                last_ui_snapshot_->preferred_travel[index].stable_half_strokes != snapshot_.preferred_travel[index].stable_half_strokes ||
                std::abs(last_ui_snapshot_->preferred_travel[index].applied_gain - snapshot_.preferred_travel[index].applied_gain) > 0.0001 ||
                std::abs(last_ui_snapshot_->preferred_travel[index].observed_travel - snapshot_.preferred_travel[index].observed_travel) > 0.0001;
        }
        if (!changed) return;
    }
    last_ui_snapshot_ = snapshot_;
    QVariantList preferred_travel_statuses;
    for (const auto& status : snapshot_.preferred_travel) {
        preferred_travel_statuses.push_back(QVariantMap{
            {"state", QString::fromLatin1(to_string(status.state))},
            {"observedTravel", status.observed_travel * 100.0},
            {"automaticGain", status.applied_gain},
            {"stableHalfStrokes", static_cast<int>(status.stable_half_strokes)},
        });
    }
    emit snapshot_ready(QString::fromLatin1(to_string(snapshot_.state)),
                        QString::fromStdString(snapshot_.action_id),
                        reference_plane_label_,
                        axes_to_variant(snapshot_.raw_axes),
                        axes_to_variant(output_processor_.smart_limit_inputs()),
                        axes_to_variant(snapshot_.device_axes),
                        preferred_travel_statuses);
}

void RealtimePipeline::save_settings() {
    auto settings = motion_bridge_settings();
    settings.setValue("device/mode", mode_name(device_->mode()));
    settings.setValue("device/armed", false);
}

void RealtimePipeline::load_settings() {
    auto settings = motion_bridge_settings();
    theme_ = settings.value("ui/theme", "dark").toString().toLower() == u"light" ? QStringLiteral("light") : QStringLiteral("dark");
    const auto default_path = QDir::homePath() + "/.f8/studio/games/fallen-doll/runtime/fd-skeleton.ndjson";
    spool_path_ = settings.value("input/spoolPath", default_path).toString();
    input_->set_spool_path(spool_path_);
    const auto saved_mode = settings.value("device/mode", "none").toString().toLower();
    const auto migrated_mode = saved_mode == u"handy" ? QStringLiteral("intiface") : saved_mode;
    if (migrated_mode != saved_mode) settings.setValue("device/mode", migrated_mode);
    device_->set_mode(parse_mode(migrated_mode));
    usb_port_ = settings.value("device/usbPort").toString();
    wifi_host_ = settings.value("device/wifiHost", "tcode.local").toString();
    wifi_port_ = settings.value("device/wifiPort", 8000).toInt();
    intiface_url_ = settings.value("device/intifaceUrl", "ws://127.0.0.1:12345").toString();
    const auto legacy_auto_reconnect = settings.value("device/intifaceAutoReconnect", true).toBool();
    auto_reconnect_ = settings.value("device/autoReconnect", legacy_auto_reconnect).toBool();
    if (!settings.contains("device/autoReconnect")) {
        settings.setValue("device/autoReconnect", auto_reconnect_);
    }
    settings.remove("device/intifaceAutoReconnect");
    device_->set_usb_port(usb_port_);
    device_->set_wifi_endpoint(wifi_host_, static_cast<quint16>(std::clamp(wifi_port_, 1, 65535)));
    device_->set_intiface_url(QUrl(intiface_url_));
    device_->set_auto_reconnect(auto_reconnect_);
    const auto legacy_target_time = settings.value("output/intifaceDurationMs");
    const auto has_target_time_mode = settings.contains("output/intifaceTargetTimeAutomatic");
    intiface_target_time_automatic_ = has_target_time_mode
        ? settings.value("output/intifaceTargetTimeAutomatic", true).toBool()
        : !legacy_target_time.isValid();
    intiface_target_time_ms_ = std::clamp(
        settings.value("output/intifaceTargetTimeMs",
                       legacy_target_time.isValid() ? legacy_target_time : 50).toInt(),
        50, 100);
    settings.setValue("output/intifaceTargetTimeAutomatic", intiface_target_time_automatic_);
    settings.setValue("output/intifaceTargetTimeMs", intiface_target_time_ms_);
    settings.remove("output/intifaceDurationMs");
    device_->set_intiface_target_time(
        intiface_target_time_automatic_, intiface_target_time_ms_);
    output_rate_hz_ = std::clamp(settings.value("output/rateHz", 50).toInt(), 20, 100);
    output_timer_->setInterval(std::max(1, static_cast<int>(std::lround(1000.0 / output_rate_hz_))));
    auto output_config = output_processor_.config();
    output_config.soft_start_enabled = settings.value("output/softStartEnabled", true).toBool();
    output_config.soft_start_for = std::chrono::milliseconds{
        std::clamp(settings.value("output/softStartDurationMs", 600).toInt(), 0, 3000)};
    const auto legacy_speed_enabled = settings.value("output/speedLimitEnabled", false).toBool();
    for (int index = 0; index < 6; ++index) {
        const auto axis_name = QString::fromLatin1(kAxisNames[static_cast<std::size_t>(index)]);
        const auto prefix = QString("output/%1/").arg(axis_name);
        output_config.axis_output_enabled[static_cast<std::size_t>(index)] =
            settings.value(prefix + "axisOutputEnabled", true).toBool();
        const auto return_position_key = prefix + "returnPosition";
        const auto legacy_safe_value_key = prefix + "axisSafeValue";
        const auto return_position = std::clamp(
            settings.value(return_position_key,
                           settings.value(legacy_safe_value_key, 0.5)).toDouble(),
            0.0, 1.0);
        output_config.axis_return_position[static_cast<std::size_t>(index)] = return_position;
        engine_.set_axis_return_position(static_cast<std::size_t>(index), return_position);
        device_->set_axis_return_position(static_cast<std::size_t>(index), return_position);
        if (!settings.contains(return_position_key) && settings.contains(legacy_safe_value_key)) {
            settings.setValue(return_position_key, return_position);
        }
        settings.remove(legacy_safe_value_key);
        output_config.speed_limit_enabled[static_cast<std::size_t>(index)] =
            settings.value(prefix + "speedLimitEnabled", legacy_speed_enabled).toBool();
        output_config.max_speed_per_second[static_cast<std::size_t>(index)] =
            std::clamp(settings.value(prefix + "maxSpeed", 4.0).toDouble(), 0.25, 10.0);
        auto& smart_limit = output_config.smart_limit[static_cast<std::size_t>(index)];
        const auto migrate_remap = !settings.contains(prefix + "smartLimitEnabled") &&
                                   settings.contains(prefix + "remapEnabled");
        smart_limit.enabled = settings.value(
            prefix + "smartLimitEnabled",
            settings.value(prefix + "remapEnabled", false)).toBool();
        smart_limit.input_axis = static_cast<std::size_t>(std::clamp(
            settings.value(prefix + "smartLimitInputAxis", migrate_remap ? index : 0).toInt(), 0, 5));
        const auto legacy_mode = settings.value(prefix + "remapMode", "value").toString();
        smart_limit.mode = settings.value(prefix + "smartLimitMode", legacy_mode).toString().compare("speed", Qt::CaseInsensitive) == 0
            ? SmartLimitMode::Speed : SmartLimitMode::Value;
        smart_limit.target_value = settings.value(
            prefix + "smartLimitTargetValue",
            settings.value(prefix + "remapTargetValue", 0.5)).toDouble();
        smart_limit.lower_input = settings.value(
            prefix + "smartLimitLowerInput",
            migrate_remap ? settings.value(prefix + "remapLowerInput", 0.0) : 0.25).toDouble();
        smart_limit.lower_factor = settings.value(
            prefix + "smartLimitLowerFactor",
            migrate_remap ? settings.value(prefix + "remapLowerFactor", settings.value(prefix + "remapFactorAtZero", 1.0)) : 1.0).toDouble();
        smart_limit.upper_input = settings.value(
            prefix + "smartLimitUpperInput",
            migrate_remap ? settings.value(prefix + "remapUpperInput", 1.0) : 0.9).toDouble();
        smart_limit.upper_factor = settings.value(
            prefix + "smartLimitUpperFactor",
            migrate_remap ? settings.value(prefix + "remapUpperFactor", settings.value(prefix + "remapFactorAtOne", 0.0)) : 0.0).toDouble();

        if (migrate_remap) {
            settings.setValue(prefix + "smartLimitEnabled", smart_limit.enabled);
            settings.setValue(prefix + "smartLimitInputAxis", static_cast<int>(smart_limit.input_axis));
            settings.setValue(prefix + "smartLimitMode", smart_limit.mode == SmartLimitMode::Speed ? "speed" : "value");
            settings.setValue(prefix + "smartLimitTargetValue", smart_limit.target_value);
            settings.setValue(prefix + "smartLimitLowerInput", smart_limit.lower_input);
            settings.setValue(prefix + "smartLimitLowerFactor", smart_limit.lower_factor);
            settings.setValue(prefix + "smartLimitUpperInput", smart_limit.upper_input);
            settings.setValue(prefix + "smartLimitUpperFactor", smart_limit.upper_factor);
        }
        for (const auto* obsolete_key : {
                 "remapEnabled", "remapMode", "remapTargetValue", "remapLowerInput",
                 "remapLowerFactor", "remapUpperInput", "remapUpperFactor",
                 "remapFactorAtZero", "remapFactorAtOne"}) {
            settings.remove(prefix + obsolete_key);
        }
    }
    output_processor_.set_config(output_config);
    auto contact = engine_.contact_config();
    const auto contact_text = [&settings](const char* key, const std::string& fallback) { return settings.value(QString("contact/%1").arg(key), QString::fromStdString(fallback)).toString().toStdString(); };
    const auto contact_number = [&settings](const char* key, const double fallback) { return settings.value(QString("contact/%1").arg(key), fallback).toDouble(); };
    contact.origin_bone = contact_text("originBone", contact.origin_bone); contact.direction_bone = contact_text("directionBone", contact.direction_bone);
    contact.tip_bone = contact_text("tipBone", contact.tip_bone); contact.support_bone = contact_text("supportBone", contact.support_bone);
    contact.support_right_axis = contact_text("supportRightAxis", contact.support_right_axis); contact.support_up_axis = contact_text("supportUpAxis", contact.support_up_axis);
    contact.target_up_axis = contact_text("targetUpAxis", contact.target_up_axis); contact.target_right_axis = contact_text("targetRightAxis", contact.target_right_axis);
    contact.l0_min_meters = contact_number("l0MinMeters", contact.l0_min_meters); contact.l0_max_meters = contact_number("l0MaxMeters", contact.l0_max_meters);
    contact.lateral_range_meters = contact_number("lateralRangeMeters", contact.lateral_range_meters); contact.twist_range_degrees = contact_number("twistRangeDegrees", contact.twist_range_degrees);
    contact.tilt_range_degrees = contact_number("tiltRangeDegrees", contact.tilt_range_degrees); contact.radius_scale = contact_number("radiusScale", contact.radius_scale);
    contact.invert_l0 = settings.value("contact/invertL0", contact.invert_l0).toBool(); contact.require_contact = settings.value("contact/requireContact", contact.require_contact).toBool();
    contact.safety_distance_enabled = settings.value(
        "contact/safetyDistanceEnabled", settings.value("contact/workingDistanceEnabled", false)).toBool();
    contact.safety_distance_meters = std::clamp(settings.value(
        "contact/safetyDistanceCm", settings.value("contact/workingDistanceCm", 10.0)).toDouble(), 2.0, 50.0) / 100.0;
    contact.reference_participant = contact_text("referenceParticipant", contact.reference_participant);
    engine_.set_contact_config(contact);
    input_->set_reference_participant(QString::fromStdString(contact.reference_participant));
    reset_participant_cache();
    for (std::size_t axis = 0; axis < kAxisNames.size(); ++axis) {
        auto travel = engine_.axis_travel_preferences()[axis];
        const auto axis_name = QString::fromLatin1(kAxisNames[axis]);
        travel.enabled = settings.value(QString("motion/%1/preferredTravelEnabled").arg(axis_name), false).toBool();
        const auto minimum_key = QString("motion/%1/preferredTravelMinimum").arg(axis_name);
        const auto maximum_key = QString("motion/%1/preferredTravelMaximum").arg(axis_name);
        if (settings.contains(minimum_key) || settings.contains(maximum_key)) {
            travel.preferred_minimum = static_cast<double>(std::clamp(
                settings.value(minimum_key, static_cast<int>(std::lround(travel.preferred_minimum * 10000.0))).toInt(), 0, 8999)) / 10000.0;
            travel.preferred_maximum = static_cast<double>(std::clamp(
                settings.value(maximum_key, static_cast<int>(std::lround(travel.preferred_maximum * 10000.0))).toInt(), 1000, 9999)) / 10000.0;
        } else {
            // Migrate the v0.1.5 preview setting, where one value represented
            // only the desired span. L0 keeps its 0000-based convention;
            // other axes remain centered around 5000.
            const auto legacy_span = std::clamp(
                settings.value(QString("motion/%1/preferredTravel").arg(axis_name), 6000).toInt(), 1000, 9000);
            const auto legacy_minimum = axis == 0 ? 0 : (10000 - legacy_span) / 2;
            travel.preferred_minimum = static_cast<double>(legacy_minimum) / 10000.0;
            travel.preferred_maximum = static_cast<double>(legacy_minimum + legacy_span) / 10000.0;
        }
        travel.maximum_gain = axis < 3 ? 4.0 : 2.0;
        engine_.set_axis_travel_preference(axis, travel);
    }
    auto tuning = engine_.axis_tuning();
    for (int index = 0; index < 6; ++index) {
        auto& item = tuning[static_cast<std::size_t>(index)]; const auto axis_name = QString::fromLatin1(kAxisNames[static_cast<std::size_t>(index)]);
        item.gain = settings.value(QString("motion/%1/gain").arg(axis_name), 1.0).toDouble();
        item.center = settings.value(QString("motion/%1/center").arg(axis_name), item.center).toDouble();
        item.dead_zone = settings.value(QString("motion/%1/deadZone").arg(axis_name), item.dead_zone).toDouble();
        item.output_min = settings.value(QString("motion/%1/min").arg(axis_name), item.output_min).toDouble();
        item.output_max = settings.value(QString("motion/%1/max").arg(axis_name), item.output_max).toDouble();
        item.inverted = settings.value(QString("motion/%1/inverted").arg(axis_name), item.inverted).toBool();
        const auto curve = settings.value(QString("motion/%1/curve").arg(axis_name), "LINEAR").toString().toUpper();
        item.curve = curve == u"SMOOTHERSTEP" ? MotionCurve::Smootherstep : curve == u"SMOOTHSTEP" ? MotionCurve::Smoothstep : MotionCurve::Linear;
    }
    engine_.set_axis_tuning(tuning);
    publish_connection_settings();
    publish_axis_gains();
    publish_axis_ranges();
    publish_axis_travel_preferences();
    publish_contact_settings();
    publish_output_processing_settings();
    publish_reference_participant();
    emit reference_participants_changed(reference_participants_);
    emit theme_changed(theme_);
    emit spool_path_changed(spool_path_);
}

void RealtimePipeline::publish_connection_settings() {
    emit connection_settings_changed(
        usb_port_, wifi_host_, wifi_port_, intiface_url_, auto_reconnect_);
}

void RealtimePipeline::publish_axis_gains() {
    QVariantList gains;
    for (const auto& item : engine_.axis_tuning()) gains.push_back(item.gain);
    emit axis_gains_changed(gains);
}

void RealtimePipeline::publish_axis_ranges() {
    QVariantList minimums;
    QVariantList maximums;
    for (const auto& item : engine_.axis_tuning()) {
        minimums.push_back(item.output_min);
        maximums.push_back(item.output_max);
    }
    emit axis_ranges_changed(minimums, maximums);
}

void RealtimePipeline::publish_axis_travel_preferences() {
    QVariantList preferences;
    for (const auto& config : engine_.axis_travel_preferences()) {
        preferences.push_back(QVariantMap{
            {"enabled", config.enabled},
            {"preferredMinimum", config.preferred_minimum * 100.0},
            {"preferredMaximum", config.preferred_maximum * 100.0},
            {"maximumGain", config.maximum_gain},
        });
    }
    emit axis_travel_preferences_changed(preferences);
}

void RealtimePipeline::publish_contact_settings() {
    const auto& contact = engine_.contact_config();
    emit contact_settings_changed(contact.safety_distance_enabled, contact.safety_distance_meters * 100.0);
}

void RealtimePipeline::publish_output_processing_settings() {
    QVariantList axis_settings;
    const auto& config = output_processor_.config();
    const auto& tuning = engine_.axis_tuning();
    for (std::size_t index = 0; index < config.max_speed_per_second.size(); ++index) {
        const auto& smart_limit = config.smart_limit[index];
        axis_settings.push_back(QVariantMap{
            {"axisEnabled", config.axis_output_enabled[index]},
            {"returnPosition", config.axis_return_position[index]},
            {"speedEnabled", config.speed_limit_enabled[index]},
            {"maxSpeed", config.max_speed_per_second[index]},
            {"inverted", tuning[index].inverted},
            {"smartLimitEnabled", smart_limit.enabled},
            {"smartLimitInputAxis", static_cast<int>(smart_limit.input_axis)},
            {"smartLimitMode", smart_limit.mode == SmartLimitMode::Speed ? "speed" : "value"},
            {"smartLimitTargetValue", smart_limit.target_value},
            {"smartLimitLowerInput", smart_limit.lower_input},
            {"smartLimitLowerFactor", smart_limit.lower_factor},
            {"smartLimitUpperInput", smart_limit.upper_input},
            {"smartLimitUpperFactor", smart_limit.upper_factor}
        });
    }
    emit output_processing_settings_changed(
        output_rate_hz_,
        intiface_target_time_automatic_,
        intiface_target_time_ms_,
        config.soft_start_enabled,
        static_cast<int>(config.soft_start_for.count()),
        axis_settings);
}

void RealtimePipeline::publish_participant_choices(const MotionFrame& frame) {
    const auto contact = engine_.contact_config();
    auto changed = false;
    const auto action_id = QString::fromStdString(frame.action_id);
    if (action_id != participant_cache_action_id_) {
        reset_participant_cache();
        participant_cache_action_id_ = action_id;
        changed = true;
    }
    for (const auto& participant : frame.participants) {
        const auto key = QString::fromStdString(participant.stable_key);
        if (key.isEmpty()) continue;
        const auto label = QString::fromStdString(participant.participant_tag);
        if (participant.bones.contains(contact.origin_bone)) changed = cache_reference_participant(key, label) || changed;
    }
    if (changed) emit reference_participants_changed(reference_participants_);
}

void RealtimePipeline::publish_reference_participant() {
    const auto contact = engine_.contact_config();
    emit reference_participant_changed(QString::fromStdString(contact.reference_participant));
}

void RealtimePipeline::reset_participant_cache() {
    participant_cache_action_id_.clear();
    reference_participants_ = {QVariantMap{{"key", ""}, {"label", tr("Automatic")}}};
}

bool RealtimePipeline::cache_reference_participant(const QString& key, const QString& label) {
    const auto normalized_key = key.trimmed();
    if (normalized_key.isEmpty()) return false;
    const auto normalized_label = label.isEmpty() ? normalized_key : label;
    for (auto& value : reference_participants_) {
        auto option = value.toMap();
        if (option.value("key").toString() != normalized_key) continue;
        if (option.value("label").toString() == normalized_label) return false;
        option.insert("label", normalized_label);
        value = option;
        return true;
    }
    reference_participants_.push_back(QVariantMap{{"key", normalized_key}, {"label", normalized_label}});
    return true;
}

std::chrono::microseconds RealtimePipeline::now() const { return std::chrono::microseconds{clock_.nsecsElapsed() / 1000}; }
