#include "hw_worker.hpp"
#include "diag.hpp"
#include "persist.hpp"
#include <cstring>
#include <exception>
#include <windows.h>

static void persist_job(const HwJob& job) {
    if (!job.persist || !job.persist_kind[0]) return;
    for (auto& d : job.devices) {
        persist_applied_one(d.address7, d.bus_port, job.persist_kind,
                            job.mode, job.speed, job.dir, job.r, job.g, job.b);
    }
}

HwWorker::HwWorker(std::wstring exe_dir, FakeHw* fake)
    : dir_(std::move(exe_dir)), fake_(fake) {
    th_ = std::thread([this] { loop(); });
}
HwWorker::~HwWorker() {
    request_stop();
    if (th_.joinable()) th_.join();
}
void HwWorker::set_on_done(HwDone cb) {
    std::lock_guard<std::mutex> lock(mu_);
    on_done_ = std::move(cb);
}
void HwWorker::request_stop() {
    std::lock_guard<std::mutex> lock(mu_);
    stop_ = true;
    on_done_ = nullptr;
    preview_.reset();
    queued_.reset();
    cv_.notify_all();
}
void HwWorker::post(HwJob job) {
    std::lock_guard<std::mutex> lock(mu_);
    if (stop_) return;
    if (job.kind == HwKind::PreviewColor) {
        preview_ = std::move(job);
    } else {
        preview_.reset();
        queued_ = std::move(job);
    }
    cv_.notify_all();
}
void HwWorker::emit(HwKind kind, bool ok, std::wstring msg, std::vector<XtreemDevice> devices, PawnioProbe probe) {
    HwDone cb;
    {
        std::lock_guard<std::mutex> lock(mu_);
        cb = on_done_;
    }
    if (cb) cb(kind, ok, std::move(msg), std::move(devices), std::move(probe));
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
        if (fake_) {
            if (fake_->stall_ms > 0) Sleep((DWORD)fake_->stall_ms);
            std::lock_guard<std::mutex> lock(fake_->mu);
            char line[128];
            auto rec_dev = [&] {
                fake_->last_dev_n = job.devices.size();
                fake_->last_addr = job.devices.empty() ? 0 : job.devices[0].address7;
            };
            switch (job.kind) {
            case HwKind::Probe:
                fake_->probe_n++;
                fake_->log.push_back("probe");
                emit(job.kind, fake_->probe.can_use_hardware(), fake_->probe.message, {}, fake_->probe);
                return;
            case HwKind::Detect:
                fake_->detect_n++;
                fake_->log.push_back("detect");
                emit(job.kind, true,
                     fake_->devices.empty() ? L"No modules (fake)" : L"Ready",
                     fake_->devices, fake_->probe);
                return;
            case HwKind::Off:
                fake_->off_n++; rec_dev();
                fake_->log.push_back("off");
                break;
            case HwKind::On:
                fake_->on_n++; rec_dev();
                fake_->log.push_back("on");
                break;
            case HwKind::Effect:
                fake_->effect_n++; rec_dev(); fake_->last_mode = job.mode;
                snprintf(line, sizeof(line), "effect:%u", (unsigned)job.mode);
                fake_->log.push_back(line);
                break;
            case HwKind::PreviewColor:
                fake_->preview_n++; rec_dev();
                fake_->last_r = job.r; fake_->last_g = job.g; fake_->last_b = job.b;
                snprintf(line, sizeof(line), "preview:%u,%u,%u", job.r, job.g, job.b);
                fake_->log.push_back(line);
                break;
            case HwKind::Color:
                fake_->color_n++; rec_dev();
                fake_->last_r = job.r; fake_->last_g = job.g; fake_->last_b = job.b;
                snprintf(line, sizeof(line), "color:%u,%u,%u", job.r, job.g, job.b);
                fake_->log.push_back(line);
                break;
            case HwKind::Restore:
                fake_->restore_n++;
                fake_->log.push_back("restore");
                emit(job.kind, true, L"Restored saved profile", {}, fake_->probe);
                return;
            default:
                fake_->log.push_back("other");
                break;
            }
            persist_job(job);
            emit(job.kind, true, L"OK", {}, fake_->probe);
            return;
        }

        if (!hw_) hw_ = std::make_unique<XtreemHardware>(dir_);
        if (job.kind == HwKind::Probe) {
            hw_->refresh_probe();
            emit(job.kind, hw_->last_probe.can_use_hardware(), hw_->last_probe.message, {}, hw_->last_probe);
            return;
        }
        if (!hw_->last_probe.can_use_hardware()) {
            hw_->refresh_probe();
            if (!hw_->last_probe.can_use_hardware()) {
                emit(job.kind, false, hw_->last_probe.message, {}, hw_->last_probe);
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
        persist_job(job);
        if (job.kind == HwKind::Restore)
            msg = L"Restored saved profile";
        emit(job.kind, true, msg, found, hw_->last_probe);
    } catch (const std::exception& ex) {
        std::wstring w(ex.what(), ex.what() + strlen(ex.what()));
        emit(job.kind, false, w, {}, hw_ ? hw_->last_probe : (fake_ ? fake_->probe : PawnioProbe{}));
    }
}
