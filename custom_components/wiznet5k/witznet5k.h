#pragma once

#include "esphome.h"
#include "esphome/components/spi/spi.h"
#include <vector>
#include <array>

namespace wiznet5k {

class WIZNET5KComponent : public PollingComponent, public spi::SPIDevice {
 public:
  WIZNET5KComponent(uint32_t update_interval = 60000) : PollingComponent(update_interval), spi::SPIDevice() {}

  void setup() override;
  void update() override;

  // config setters from codegen
  void set_reset_pin(GPIOPin *pin) { reset_pin_ = pin; }
  void set_debug(bool debug) { debug_ = debug; }

  void set_static_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
  void set_subnet_mask(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
  void set_gateway(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
  void set_mac(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f);

  // low-level W5500 SPI access (mirror of the MicroPython read/write semantics)
  // control = control byte as used in the MicroPython driver (callback param)
  bool read_bytes(uint16_t addr, uint8_t control, size_t len, std::vector<uint8_t> &out);
  bool write_bytes(uint16_t addr, uint8_t control, const std::vector<uint8_t> &data);

  // detection / reset helpers
  int sw_reset();
  bool detect_w5500();

 protected:
  GPIOPin *reset_pin_{nullptr};
  bool debug_{false};

  bool have_static_ip_{false};
  bool have_subnet_mask_{false};
  bool have_gateway_{false};
  bool have_mac_{false};

  uint8_t static_ip_[4]{0,0,0,0};
  uint8_t subnet_mask_[4]{0,0,0,0};
  uint8_t gateway_[4]{0,0,0,0};
  uint8_t mac_[6]{0,0,0,0,0,0};

  // placeholders for more features
  void apply_network_config_();

};

}  // namespace wiznet5k
