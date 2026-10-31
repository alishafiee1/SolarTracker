#pragma once
// Simulator (cycle 013): the real control stack (angle provider, wind monitor, state machine, motion
// controller, settings store) on top of fakes and a small physical model of the actuator, the
// accelerometer and the anemometer. One step is one 100 ms control tick, the same as on the device.

#include <math.h>

#include <functional>
#include <memory>

#include "angle_provider.h"
#include "control_state_machine.h"
#include "fakes.h"
#include "motion_controller.h"
#include "settings.h"
#include "sun_position.h"
#include "tracking.h"
#include "wind_monitor.h"

namespace sim {

// Small repeatable noise source (xorshift, sum of uniforms ~ Gaussian).
class Rng {
public:
    explicit Rng(uint32_t seed) : s_(seed ? seed : 1) {}
    uint32_t next() {
        s_ ^= s_ << 13;
        s_ ^= s_ >> 17;
        s_ ^= s_ << 5;
        return s_;
    }
    double uniform() { return next() / 4294967296.0; }
    double gauss() { return (uniform() + uniform() + uniform() + uniform() - 2.0) * 1.7320508; }  // sigma 1

private:
    uint32_t s_;
};

struct ActuatorModel {
    double speedDegPerS = 0.6;
    double coastS = 0.3;       // time constant of the run-out after power is cut
    double limitDeg = 50.0;    // internal limit switches
    bool jammed = false;       // the relays switch but the arm does not move
    bool wireBroken = false;   // the actuator cable is cut: no motion, no current
    bool relayStuck = false;   // CH1 contacts welded: the supply stays connected after the stop command
    double runCurrentA = 3.0;  // two actuators in parallel, about 1.5 A each
    double jamCurrentA = 6.0;  // blocked arm
    double inrushA = 4.0;      // extra current in the first 300 ms
    double angleDeg = 0.0;     // true panel angle, positive towards west
    double velocity = 0.0;
};

class World {
public:
    static constexpr uint32_t kStepMs = 100;

    World(uint32_t seed, int64_t startUtc, settings::Mode mode = settings::Mode::Auto)
        : rng_(seed), utc_(startUtc), relays_(&time_), store_(blobs_), angle_(accel_) {
        clock_.setUtc(startUtc);
        boot();  // loads the stored settings, which are the defaults on a new device
        cfg_.mode = mode;
        ctl_->command(mode == settings::Mode::Manual ? control::Command::ModeManual : control::Command::ModeAuto, 0);
    }

    // ---- environment ----
    ActuatorModel act;
    double accelNoiseDeg = 0.3;                                   // R1: sigma of the angle noise
    std::function<double(uint32_t ms)> windKmh = [](uint32_t) { return 0.0; };
    bool sensorUnplugged = false;
    double currentNoiseA = 0.05;
    bool clockValid = true;

    // ---- access ----
    FakeTime& time() { return time_; }
    FakeRelays& relays() { return relays_; }
    FakeEventLog& log() { return log_; }
    FakeSettingsStore& blobs() { return blobs_; }
    FakeClock& clock() { return clock_; }
    FakeAccel& accel() { return accel_; }
    settings::Settings& cfg() { return cfg_; }
    settings::SettingsStore& store() { return store_; }
    angle::AngleProvider& angle() { return angle_; }
    wind::WindMonitor& wind() { return *wind_; }
    control::ControlStateMachine& ctl() { return *ctl_; }
    motion::MotionController& motion() { return *motion_; }
    int64_t utc() const { return utc_; }
    double actuatorCurrentA() const { return currentA_; }
    // Fit the current transformer: from now on the controller uses the current (module 009).
    void enableCurrentModule() {
        cfg_.currentPresent = true;
        motion_->setTiming(timingFrom(cfg_));
    }
    void setUtc(int64_t t) { utc_ = t; }
    uint32_t elapsedMs() const { return time_.nowMs(); }
    int powerOns() const { return powerOns_; }
    float lastTargetDeg() const { return lastTarget_; }
    int bootCount() const { return bootCount_; }

    // Panel angle error against the sun target (valid while the sun is up).
    double trackingError() const {
        const sun::SunPosition p = sun::position(utc_, cfg_.latDeg, cfg_.lonDeg);
        const double target = tracking::target(tracking::rotation(p), cfg_.minDeg, cfg_.maxDeg);
        return act.angleDeg - target;
    }
    bool sunUp() const { return !sun::isNight(sun::position(utc_, cfg_.latDeg, cfg_.lonDeg)); }

    // Put the panel somewhere by hand (the installer, or a test set-up), without the actuator.
    void placePanel(double deg) {
        act.angleDeg = deg;
        act.velocity = 0.0;
    }

    // Run the two-step calibration the way an installer does and save it.
    void calibrate() {
        const double keep = act.angleDeg;
        const double keepNoise = accelNoiseDeg;
        accelNoiseDeg = 0.0;  // calibration is done on a still panel in calm air; the 0.3 degree noise models wind and vibration
        placePanel(0.0);
        angle_.beginCapture(angle::CalStep::Horizontal);
        for (size_t i = 0; i < angle::kCaptureSamples; ++i) sampleSensor();
        placePanel(-8.0);  // turned east by hand
        angle_.beginCapture(angle::CalStep::East);
        for (size_t i = 0; i < angle::kCaptureSamples; ++i) sampleSensor();
        angle_.saveTo(blobs_);
        placePanel(keep);
        accelNoiseDeg = keepNoise;
        for (int i = 0; i < 10; ++i) sampleSensor();  // fill the moving average
    }

    // One control tick.
    void step() {
        const double dt = kStepMs / 1000.0;
        time_.advance(kStepMs);
        utc_ += (stepCount_++ % 10 == 9) ? 1 : 0;  // 10 steps of 100 ms = 1 s

        // actuator: the supply reaches it through CH1, or through welded contacts
        const bool commanded = relays_.power();
        const bool supplied = (commanded || act.relayStuck) && !act.wireBroken;
        const bool west = relays_.direction() == hal::Dir::West;
        const bool atLimit = (west && act.angleDeg >= act.limitDeg) || (!west && act.angleDeg <= -act.limitDeg);  // the jack's own limit switch cuts its power
        const bool driven = supplied && !atLimit;
        const double want = (driven && !act.jammed) ? (west ? act.speedDegPerS : -act.speedDegPerS) : 0.0;
        act.velocity = driven ? want : act.velocity * exp(-dt / act.coastS);
        act.angleDeg += act.velocity * dt;
        if (act.angleDeg > act.limitDeg) act.angleDeg = act.limitDeg;
        if (act.angleDeg < -act.limitDeg) act.angleDeg = -act.limitDeg;
        if (driven && !wasDriven_) drivenSinceMs_ = time_.nowMs();
        wasDriven_ = driven;
        currentA_ = driven ? (act.jammed ? act.jamCurrentA : act.runCurrentA) + (time_.nowMs() - drivenSinceMs_ < 300 ? act.inrushA : 0.0) : 0.0;
        if (commanded && !wasPowered_) ++powerOns_;
        wasPowered_ = commanded;

        sampleSensor();

        // anemometer: one pulse per revolution
        const double kmh = windKmh(time_.nowMs());
        clockUs_ += kStepMs * 1000.0;
        if (kmh > 0.5) {
            const double periodUs = 1.0e6 / (kmh / cfg_.anemometerFactor);
            if (nextPulseUs_ < clockUs_ - 4.0 * periodUs) nextPulseUs_ = clockUs_;
            while (nextPulseUs_ <= clockUs_) {
                wind_->feed(static_cast<uint32_t>(nextPulseUs_));
                nextPulseUs_ += periodUs;
            }
        }
        const wind::Config wc{cfg_.anemometerFactor, cfg_.windStowKmh, cfg_.windStowDelayS, cfg_.windReturnKmh, cfg_.windReturnMin};
        const wind::Result wr = wind_->update(time_.nowMs(), static_cast<uint32_t>(clockUs_), wc);
        if (wr.stowStarted) log_.append(hal::EventType::StateChange, "wind stow start");
        if (wr.stowEnded) log_.append(hal::EventType::StateChange, "wind stow end");

        control::Inputs in;
        in.angleDeg = angle_.angleDeg();
        in.angleOk = angle_.ok();
        in.calibrated = angle_.calibrated();
        in.utcEpoch = utc_;
        in.clockValid = clockValid;
        in.windStow = wind_->stow();
        in.currentA = static_cast<float>(currentA_ > 0.0 ? currentA_ + rng_.gauss() * currentNoiseA : fabs(rng_.gauss() * currentNoiseA));
        ctl_->tick(time_.nowMs(), in);
    }

    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t += kStepMs) step();
    }

    // Power cut: everything volatile is lost, the settings in flash and the battery-backed clock stay.
    // The panel keeps its position; the actuator stops with the relays.
    void powerCut(uint32_t outageMs) {
        act.velocity = 0.0;
        relays_.setPower(false);
        utc_ += outageMs / 1000;  // the DS3231 keeps time on its battery
        time_.advance(outageMs);
        clockUs_ += outageMs * 1000.0;
        boot();
    }

private:
    void sampleSensor() {
        accel_.setOk(!sensorUnplugged);
        const double deg = act.angleDeg + rng_.gauss() * accelNoiseDeg;
        const double r = deg * 3.14159265358979 / 180.0;
        // Gravity as seen by a box mounted flat under the panel; rotation about the x axis.
        accel_.setGravity({0.0f, static_cast<float>(-sin(r)), static_cast<float>(cos(r))});
        angle_.sample();
    }

    void boot() {
        ++bootCount_;
        store_.load(cfg_, &log_);              // defaults + an event if nothing valid is stored
        angle_.loadFrom(blobs_);               // the saved calibration
        log_.append(hal::EventType::PowerUp, bootCount_ == 1 ? "power-on" : "power-on after cut");
        motion_ = std::make_unique<motion::MotionController>(relays_, timingFrom(cfg_), time_.nowMs());
        motion_->setEventLog(&log_);
        ctl_ = std::make_unique<control::ControlStateMachine>(*motion_, cfg_, &log_);
        wind_ = std::make_unique<wind::WindMonitor>();
        wasPowered_ = false;
        nextPulseUs_ = clockUs_;
    }

    static motion::Timing timingFrom(const settings::Settings& s) {
        motion::Timing t;
        t.stopSettleMs = s.stopSettleMs;
        t.minMoveGapMs = s.minMoveGapMs;
        t.maxMoveMs = s.maxMoveMs;
        t.currentPresent = s.currentPresent;
        t.zeroCurrentA = s.zeroCurrentA;
        return t;
    }

    Rng rng_;
    int64_t utc_;
    FakeTime time_;
    FakeRelays relays_;
    FakeEventLog log_{&utc_};
    FakeSettingsStore blobs_;
    FakeAccel accel_;
    FakeClock clock_;
    settings::Settings cfg_;
    settings::SettingsStore store_;
    angle::AngleProvider angle_;
    std::unique_ptr<motion::MotionController> motion_;
    std::unique_ptr<control::ControlStateMachine> ctl_;
    std::unique_ptr<wind::WindMonitor> wind_;
    double clockUs_ = 1000000.0;
    double nextPulseUs_ = 0.0;
    uint32_t stepCount_ = 0;
    bool wasPowered_ = false;
    bool wasDriven_ = false;
    uint32_t drivenSinceMs_ = 0;
    double currentA_ = 0.0;
    int powerOns_ = 0;
    int bootCount_ = 0;
    float lastTarget_ = 0.0f;
};

}  // namespace sim
