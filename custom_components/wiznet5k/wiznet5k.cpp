#include "wiznet5k.h"
#include "esphome/core/log.h"

static const char *TAG = "wiznet5k";

namespace wiznet5k {

void WIZNET5KComponent::set_static_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  static_ip_[0] = a; static_ip_[1] = b; static_ip_[2] = c; static_ip_[3] = d;
  have_static_ip_ = true;
}
void WIZNET5KComponent::set_subnet_mask(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  subnet_mask_[0] = a; subnet_mask_[1] = b; subnet_mask_[2] = c; subnet_mask_[3] = d;
  have_subnet_mask_ = true;
}
void WIZNET5KComponent::set_gateway(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  gateway_[0] = a; gateway_[1] = b; gateway_[2] = c; gateway_[3] = d;
  have_gateway_ = true;
}
void WIZNET5KComponent::set_mac(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
  mac_[0]=a; mac_[1]=b; mac_[2]=c; mac_[3]=d; mac_[4]=e; mac_[5]=f;
  have_mac_ = true;
}

void WIZNET5KComponent::setup() {
  ESP_LOGI(TAG, "WIZNET5K setup()");
  if (reset_pin_) {
    ESP_LOGI(TAG, "Toggling reset pin (simple sequence)");
    // Mirrors the MicroPython reset sequence (active-high then low)
    reset_pin_->digital_write(true);
    delay(100);
    reset_pin_->digital_write(false);
    delay(100);
  }

  // Apply provided network config (placeholder logging)
  apply_network_config_();

  // perform detection (soft reset + version check)
  if (detect_w5500()) {
    ESP_LOGI(TAG, "W5500 detected successfully.");
  } else {
    ESP_LOGE(TAG, "W5500 detection failed (version/reg checks).");
  }
}

void WIZNET5KComponent::update() {
  if (debug_) {
    ESP_LOGD(TAG, "Periodic update (debug)");
  }
}

/*
 * Low-level SPI helpers
 * Format (as in MicroPython driver):
 * - send addr_hi, addr_lo, control byte
 * - then data bytes (for write) or read bytes (for read)
 *
 * We create a single transfer: header (3 bytes) + payload (data or zeros)
 * Use spi::SPIDevice::transfer(...) to perform the transfer while CS is asserted.
 */

bool WIZNET5KComponent::read_bytes(uint16_t addr, uint8_t control, size_t len, std::vector<uint8_t> &out) {
  out.clear();
  size_t total = 3 + len;
  std::vector<uint8_t> tx(total, 0x00), rx(total, 0x00);
  tx[0] = (uint8_t)(addr >> 8);
  tx[1] = (uint8_t)(addr & 0xFF);
  tx[2] = control;
  // rest remain 0x00 to clock in data
  // Perform SPI transfer (SPIDevice provides transfer)
  bool ok = this->transfer(tx.data(), rx.data(), total);
  if (!ok) {
    ESP_LOGW(TAG, "SPI transfer failed for read addr=0x%04X len=%u", addr, (unsigned)len);
    return false;
  }
  // copy payload (rx[3] .. rx[3+len-1])
  out.resize(len);
  for (size_t i = 0; i < len; ++i) {
    out[i] = rx[3 + i];
  }
  if (debug_) {
    std::string s = "";
    for (size_t i = 0; i < len; ++i) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%02X", out[i]);
      s += buf;
      s += " ";
    }
    ESP_LOGD(TAG, "READ 0x%04X ctrl=0x%02X -> %s", addr, control, s.c_str());
  }
  return true;
}

bool WIZNET5KComponent::write_bytes(uint16_t addr, uint8_t control, const std::vector<uint8_t> &data) {
  size_t total = 3 + data.size();
  std::vector<uint8_t> tx(total, 0x00), rx(total, 0x00);
  tx[0] = (uint8_t)(addr >> 8);
  tx[1] = (uint8_t)(addr & 0xFF);
  tx[2] = control;
  for (size_t i = 0; i < data.size(); ++i) tx[3 + i] = data[i];

  bool ok = this->transfer(tx.data(), rx.data(), total);
  if (!ok) {
    ESP_LOGW(TAG, "SPI transfer failed for write addr=0x%04X len=%u", addr, (unsigned)data.size());
    return false;
  }
  if (debug_) {
    std::string s = "";
    for (size_t i = 0; i < data.size(); ++i) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%02X", data[i]);
      s += buf;
      s += " ";
    }
    ESP_LOGD(TAG, "WRITE 0x%04X ctrl=0x%02X <- %s", addr, control, s.c_str());
  }
  return true;
}

/*
 * Detection & soft-reset (portiert von MicroPython driver)
 */

int WIZNET5KComponent::sw_reset() {
  // write 0x80 to MR (0x0000) and read back expecting 0x00
  std::vector<uint8_t> data = { 0x80 };
  if (!write_bytes(0x0000, 0x04, data)) {
    ESP_LOGW(TAG, "sw_reset: write failed");
    return -1;
  }
  delay(10);
  std::vector<uint8_t> r;
  if (!read_bytes(0x0000, 0x00, 1, r)) {
    ESP_LOGW(TAG, "sw_reset: read failed");
    return -1;
  }
  if (r.size() < 1) return -1;
  if (r[0] != 0x00) return -1;
  return 0;
}

bool WIZNET5KComponent::detect_w5500() {
  // try soft reset and then a version check like MicroPython
  if (sw_reset() != 0) {
    ESP_LOGE(TAG, "soft reset failed");
    return false;
  }

  // MR tests similar to MicroPython driver
  write_bytes(0x0000, 0x04, std::vector<uint8_t>{0x08});
  std::vector<uint8_t> r;
  if (!read_bytes(0x0000, 0x00, 1, r)) return false;
  if (r.size() < 1 || r[0] != 0x08) {
    ESP_LOGW(TAG, "MR test 0x08 mismatch: 0x%02X", r.size()? r[0] : 0xFF);
  }

  write_bytes(0x0000, 0x04, std::vector<uint8_t>{0x10});
  if (!read_bytes(0x0000, 0x00, 1, r)) return false;
  if (r.size() < 1 || r[0] != 0x10) {
    ESP_LOGW(TAG, "MR test 0x10 mismatch: 0x%02X", r.size()? r[0] : 0xFF);
  }

  write_bytes(0x0000, 0x04, std::vector<uint8_t>{0x00});
  if (!read_bytes(0x0000, 0x00, 1, r)) return false;
  if (r.size() < 1 || r[0] != 0x00) {
    ESP_LOGW(TAG, "MR test 0x00 mismatch: 0x%02X", r.size()? r[0] : 0xFF);
  }

  // version register 0x0039 should return 0x04 for W5500
  if (!read_bytes(0x0039, 0x00, 1, r)) return false;
  if (r.size() < 1) return false;
  if (r[0] != 0x04) {
    ESP_LOGE(TAG, "Version register unexpected: 0x%02X", r[0]);
    return false;
  }

  return true;
}

void WIZNET5KComponent::apply_network_config_() {
  if (have_mac_) {
    ESP_LOGI(TAG, "Configured MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
    // placeholder: write to REG_SHAR (0x0009) using control byte 0x04 (write)
    std::vector<uint8_t> macdata = { mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5] };
    write_bytes(0x0009, 0x04, macdata);
  }

  if (have_static_ip_) {
    ESP_LOGI(TAG, "Configured Static IP: %u.%u.%u.%u",
             static_ip_[0], static_ip_[1], static_ip_[2], static_ip_[3]);
    std::vector<uint8_t> ipdata = { static_ip_[0], static_ip_[1], static_ip_[2], static_ip_[3] };
    write_bytes(0x000F, 0x04, ipdata);  // REG_SIPR
  }

  if (have_subnet_mask_) {
    ESP_LOGI(TAG, "Configured Subnet Mask: %u.%u.%u.%u",
             subnet_mask_[0], subnet_mask_[1], subnet_mask_[2], subnet_mask_[3]);
    std::vector<uint8_t> smdata = { subnet_mask_[0], subnet_mask_[1], subnet_mask_[2], subnet_mask_[3] };
    write_bytes(0x0005, 0x04, smdata); // REG_SUBR
  }

  if (have_gateway_) {
    ESP_LOGI(TAG, "Configured Gateway: %u.%u.%u.%u",
             gateway_[0], gateway_[1], gateway_[2], gateway_[3]);
    std::vector<uint8_t> gwdata = { gateway_[0], gateway_[1], gateway_[2], gateway_[3] };
    write_bytes(0x0001, 0x04, gwdata); // REG_GAR
  }
}

}  // namespace wiznet5k
