#pragma once
#include "xtreem.hpp"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <optional>
#include <vector>
#include <string>

enum class HwKind { Probe, Detect, Off, On, Effect, Color, PreviewColor, Restore, Dump, Shutdown };

struct HwJob {
    HwKind kind = HwKind::Detect;
    std::vector<XtreemDevice> devices;
    uint8_t mode = 0, speed = 2, dir = 0, r = 0, g = 0, b = 0;
    Profile profile;
};

using HwDone = std::function<void(HwKind kind, bool ok, std::wstring msg, std::vector<XtreemDevice> devices, PawnioProbe probe)>;

// In-process mock. Never opens PawnIO or SMBus. Tests must pass a FakeHw*.
struct FakeHw {
    PawnioProbe probe;
    std::vector<XtreemDevice> devices;
    std::mutex mu;
    std::vector<std::string> log;
    int stall_ms = 0;
    int preview_n = 0, color_n = 0, off_n = 0, on_n = 0, effect_n = 0, probe_n = 0, detect_n = 0;
    uint8_t last_r = 0, last_g = 0, last_b = 0, last_mode = 0, last_addr = 0;
    size_t last_dev_n = 0;
    bool used_real_hardware = false;
};

class HwWorker {
public:
    explicit HwWorker(std::wstring exe_dir, FakeHw* fake = nullptr);
    ~HwWorker();
    void post(HwJob job);          // serial; preview is latest-wins
    void request_stop();           // drop queued work; in-flight job finishes; no more HWND posts
    void set_on_done(HwDone cb);
private:
    std::wstring dir_;
    FakeHw* fake_ = nullptr;
    std::mutex mu_;
    std::condition_variable cv_;
    std::optional<HwJob> queued_;
    std::optional<HwJob> preview_;
    bool stop_ = false;
    std::thread th_;
    std::unique_ptr<XtreemHardware> hw_;
    HwDone on_done_;
    void loop();
    void run_job(HwJob job);
    void emit(HwKind kind, bool ok, std::wstring msg, std::vector<XtreemDevice> devices, PawnioProbe probe);
};
