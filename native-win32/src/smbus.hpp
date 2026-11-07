#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "pawnio.hpp"

struct ISmBus {
    virtual ~ISmBus() = default;
    virtual bool probe(uint8_t addr7) = 0;
    virtual uint8_t read_byte_data(uint8_t addr7, uint8_t command) = 0;
    virtual void write_byte_data(uint8_t addr7, uint8_t command, uint8_t value) = 0;
    virtual void write_word_data(uint8_t addr7, uint8_t command, uint16_t value) = 0;
};

class AmdSmBus : public ISmBus {
public:
    static AmdSmBus open(PawnioApi& api, const std::wstring& module_path, int port);
    ~AmdSmBus() override;
    AmdSmBus(const AmdSmBus&) = delete;
    AmdSmBus& operator=(const AmdSmBus&) = delete;
    AmdSmBus(AmdSmBus&& o) noexcept;
    bool probe(uint8_t addr7) override;
    uint8_t read_byte_data(uint8_t addr7, uint8_t command) override;
    void write_byte_data(uint8_t addr7, uint8_t command, uint8_t value) override;
    void write_word_data(uint8_t addr7, uint8_t command, uint16_t value) override;
    int port() const { return port_; }
private:
    AmdSmBus() = default;
    void* handle_ = nullptr;
    HANDLE mutex_ = nullptr;
    PawnioApi* api_ = nullptr;
    int port_ = 0;
    unsigned long long xfer(uint8_t addr7, int rw, uint8_t command, int proto, unsigned long long data);
};

inline uint16_t swap_reg(uint16_t reg) {
    return (uint16_t)(((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));
}
void register_write(ISmBus& bus, uint8_t addr7, uint16_t reg, uint8_t value);
uint8_t register_read(ISmBus& bus, uint8_t addr7, uint16_t reg);
