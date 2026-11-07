#pragma once
#include "xtreem.hpp"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>

enum class HwKind { Probe, Detect, Off, On, Effect, Color, PreviewColor, Restore, Dump, Shutdown };

struct HwJob {
    HwKind kind = HwKind::Detect;
    std::vector<XtreemDevice> devices;
    uint8_t mode = 0, speed = 2, dir = 0, r = 0, g = 0, b = 0;
    Profile profile;
};

using HwDone = std::function<void(HwKind kind, bool ok, std::wstring msg, std::vector<XtreemDevice> devices, PawnioProbe probe)>;

class HwWorker {
public:
    explicit HwWorker(std::wstring exe_dir);
    ~HwWorker();
    void post(HwJob job);          // serial; preview is latest-wins
    void request_stop();
private:
    std::wstring dir_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::optional<HwJob> queued_;
    std::optional<HwJob> preview_;
    bool stop_ = false;
    std::thread th_;
    std::unique_ptr<XtreemHardware> hw_;
    void loop();
    void run_job(HwJob job);
public:
    HwDone on_done;
};
