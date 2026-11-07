#include "hw_worker.hpp"
#include "diag.hpp"
#include <cstring>

HwWorker::HwWorker(std::wstring exe_dir) : dir_(std::move(exe_dir)) {
    th_ = std::thread([this] { loop(); });
}
HwWorker::~HwWorker() {
    request_stop();
    if (th_.joinable()) th_.join();
}
void HwWorker::request_stop() {
    std::lock_guard<std::mutex> lock(mu_);
    stop_ = true;
    preview_.reset();
    queued_.reset();
    cv_.notify_all();
}
void HwWorker::post(HwJob job) {
    std::lock_guard<std::mutex> lock(mu_);
    if (job.kind == HwKind::PreviewColor) {
        preview_ = std::move(job);
    } else {
        preview_.reset();
        queued_ = std::move(job);
    }
    cv_.notify_all();
}
void HwWorker::loop() {
    for (;;) {
        HwJob job;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [&] { return stop_ || queued_ || preview_; });
            if (stop_) return;
            if (queued_) { job = std::move(*queued_); queued_.reset(); }
            else { job = std::move(*preview_); preview_.reset(); }
        }
        if (job.kind == HwKind::Shutdown) return;
        run_job(std::move(job));
    }
}
void HwWorker::run_job(HwJob job) {
    try {
        if (!hw_) hw_ = std::make_unique<XtreemHardware>(dir_);
        if (job.kind == HwKind::Probe) {
            hw_->refresh_probe();
            if (on_done) on_done(job.kind, hw_->last_probe.can_use_hardware(), hw_->last_probe.message, {}, hw_->last_probe);
            return;
        }
        if (!hw_->last_probe.can_use_hardware()) {
            hw_->refresh_probe();
            if (!hw_->last_probe.can_use_hardware()) {
                if (on_done) on_done(job.kind, false, hw_->last_probe.message, {}, hw_->last_probe);
                return;
            }
        }
        std::vector<XtreemDevice> found;
        std::wstring msg = L"OK";
        switch (job.kind) {
        case HwKind::Detect:
            found = hw_->detect();
            msg = found.empty() ? L"No modules (driver OK). Check AMD platform / Aura off / addresses."
                                : L"Ready";
            break;
        case HwKind::Off:
            hw_->apply_off(job.devices);
            break;
        case HwKind::On:
            hw_->apply_on(job.devices);
            break;
        case HwKind::Effect:
            hw_->apply_effect_all(job.devices, job.mode, job.speed, job.dir);
            break;
        case HwKind::Color:
        case HwKind::PreviewColor:
            hw_->apply_color(job.devices, job.r, job.g, job.b);
            break;
        case HwKind::Restore:
            found = hw_->detect();
            hw_->restore_profile(found, job.profile);
            break;
        case HwKind::Dump:
            msg = hw_->dump(job.devices);
            break;
        default:
            break;
        }
        if (on_done) on_done(job.kind, true, msg, found, hw_->last_probe);
    } catch (const std::exception& ex) {
        std::wstring w(ex.what(), ex.what() + strlen(ex.what()));
        if (on_done) on_done(job.kind, false, w, {}, hw_ ? hw_->last_probe : PawnioProbe{});
    }
}
