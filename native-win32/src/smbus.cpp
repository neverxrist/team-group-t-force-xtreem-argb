#include "smbus.hpp"
#include "diag.hpp"
#include <windows.h>
#include <stdexcept>
#include <fstream>
#include <vector>

static constexpr int I2C_SMBUS_READ = 1;
static constexpr int I2C_SMBUS_WRITE = 0;
static constexpr int I2C_SMBUS_QUICK = 0;
static constexpr int I2C_SMBUS_BYTE_DATA = 2;
static constexpr int I2C_SMBUS_WORD_DATA = 3;

void register_write(ISmBus& bus, uint8_t addr7, uint16_t reg, uint8_t value) {
    bus.write_word_data(addr7, 0x00, swap_reg(reg));
    bus.write_byte_data(addr7, 0x01, value);
}
uint8_t register_read(ISmBus& bus, uint8_t addr7, uint16_t reg) {
    bus.write_word_data(addr7, 0x00, swap_reg(reg));
    return bus.read_byte_data(addr7, 0x81);
}

AmdSmBus AmdSmBus::open(PawnioApi& api, const std::wstring& module_path, int port) {
    diag::Timer t("AmdSmBus.open");
    std::ifstream f(module_path, std::ios::binary);
    if (!f) throw std::runtime_error("SmbusPIIX4.bin not found next to the exe");
    std::vector<unsigned char> blob((std::istreambuf_iterator<char>(f)), {});
    unsigned int ver = 0;
    int vhr = api.version(&ver);
    if (!pawnio_ok(vhr))
        throw std::runtime_error("pawnio_version failed after a Ready probe (not treated as a missing driver)");
    void* handle = nullptr;
    int ohr = api.open(&handle);
    if (!pawnio_ok(ohr) || !handle) {
        if ((unsigned)ohr == 0x80070005u)
            throw std::runtime_error("pawnio_open access denied (run as Administrator; not a missing-driver diagnosis)");
        throw std::runtime_error("pawnio_open failed (not classified as a missing driver here)");
    }
    int lhr = api.load(handle, blob.data(), blob.size());
    if (!pawnio_ok(lhr)) {
        api.close(handle);
        throw std::runtime_error("pawnio_load(SmbusPIIX4.bin) failed (unsupported platform or bad module)");
    }
    HANDLE mtx = CreateMutexA(nullptr, FALSE, pawnio_mutex_name());
    if (!mtx) {
        api.close(handle);
        throw std::runtime_error("Failed to create SMBus mutex");
    }
    AmdSmBus bus;
    bus.api_ = &api;
    bus.handle_ = handle;
    bus.mutex_ = mtx;
    bus.port_ = port;
    unsigned long long in[1] = { (unsigned long long)port };
    unsigned long long out[1] = {};
    size_t nret = 0;
    DWORD w = WaitForSingleObject(mtx, INFINITE);
    if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED) {
        CloseHandle(mtx);
        api.close(handle);
        throw std::runtime_error("SMBus mutex wait failed");
    }
    int phr = api.execute(handle, "ioctl_piix4_port_sel", in, 1, out, 1, &nret);
    ReleaseMutex(mtx);
    if (!pawnio_ok(phr)) {
        CloseHandle(mtx);
        api.close(handle);
        throw std::runtime_error("ioctl_piix4_port_sel failed");
    }
    unsigned long long sin[1] = { 2 };
    api.execute(handle, "ioctl_set_sleep_mode", sin, 1, nullptr, 0, &nret);
    return bus;
}

AmdSmBus::AmdSmBus(AmdSmBus&& o) noexcept {
    handle_ = o.handle_; mutex_ = o.mutex_; api_ = o.api_; port_ = o.port_;
    o.handle_ = nullptr; o.mutex_ = nullptr; o.api_ = nullptr;
}
AmdSmBus::~AmdSmBus() {
    if (mutex_) CloseHandle(mutex_);
    if (handle_ && api_ && api_->close) api_->close(handle_);
}

unsigned long long AmdSmBus::xfer(uint8_t addr7, int rw, uint8_t command, int proto, unsigned long long data) {
    unsigned long long in[9]{};
    in[0] = addr7; in[1] = (unsigned long long)rw; in[2] = command; in[3] = (unsigned long long)proto; in[4] = data;
    unsigned long long out[5]{};
    size_t nret = 0;
    DWORD w = WaitForSingleObject(mutex_, INFINITE);
    if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED)
        throw std::runtime_error("SMBus mutex wait failed");
    int hr = api_->execute(handle_, "ioctl_smbus_xfer", in, 9, out, 5, &nret);
    ReleaseMutex(mutex_);
    if (!pawnio_ok(hr)) throw std::runtime_error("SMBus xfer failed");
    return out[0];
}

bool AmdSmBus::probe(uint8_t addr7) {
    try { xfer(addr7, I2C_SMBUS_WRITE, 0, I2C_SMBUS_QUICK, 0); return true; }
    catch (...) { return false; }
}
uint8_t AmdSmBus::read_byte_data(uint8_t addr7, uint8_t command) {
    return (uint8_t)(xfer(addr7, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, 0) & 0xFF);
}
void AmdSmBus::write_byte_data(uint8_t addr7, uint8_t command, uint8_t value) {
    xfer(addr7, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, value);
}
void AmdSmBus::write_word_data(uint8_t addr7, uint8_t command, uint16_t value) {
    xfer(addr7, I2C_SMBUS_WRITE, command, I2C_SMBUS_WORD_DATA, value);
}
