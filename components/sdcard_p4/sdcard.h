#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <ctime>
#include <string>
#include <vector>
#include <functional>

namespace esphome {
namespace sdcard_p4 {

/**
 * @brief File information structure
 */
struct FileInfo {
  std::string name;
  size_t size;
  bool is_directory;
  time_t modified_time;
};

/**
 * @brief SD Card component for ESP32-P4
 * 
 * Provides SDMMC interface for SD card access with:
 * - 1-bit and 4-bit bus modes
 * - Configurable frequency
 * - File operations
 * - Card information
 */
class SDCardComponent : public Component {
 public:
  // ============ Lifecycle ============
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  // ============ Pin Configuration ============
  void set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }
  void set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }
  void set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }
  void set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }
  void set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }

  // ============ Bus Configuration ============
  void set_bus_width(uint8_t width) { this->bus_width_ = width; }
  void set_mount_point(const std::string &mount_point) { this->mount_point_ = mount_point; }
  void set_max_freq_khz(uint32_t freq) { this->max_freq_khz_ = freq; }

  // ============ Status ============
  bool is_mounted() const { return this->is_mounted_; }
  uint64_t get_card_size() const;
  uint64_t get_free_space() const;
  std::string get_card_type();

  // ============ File Operations ============
  bool file_exists(const std::string &path);
  bool write_file(const std::string &path, const std::string &content);
  bool append_file(const std::string &path, const std::string &content);
  std::string read_file(const std::string &path);
  bool delete_file(const std::string &path);
  bool rename_file(const std::string &old_path, const std::string &new_path);

  // ============ Directory Operations ============
  bool create_dir(const std::string &path);
  bool remove_dir(const std::string &path);
  std::vector<std::string> list_dir(const std::string &path);
  std::vector<FileInfo> list_dir_detailed(const std::string &path);

  // ============ Callbacks ============
  void add_on_mount_callback(std::function<void()> &&callback) {
    this->on_mount_callbacks_.add(std::move(callback));
  }
  void add_on_unmount_callback(std::function<void()> &&callback) {
    this->on_unmount_callbacks_.add(std::move(callback));
  }

 protected:
  // ============ Internal Methods ============
  bool mount_card_();
  void unmount_card_();
  std::string get_full_path_(const std::string &path);

  // ============ Pin Configuration ============
  uint8_t clk_pin_{0};
  uint8_t cmd_pin_{0};
  uint8_t data0_pin_{0};
  uint8_t data1_pin_{UINT8_MAX};
  uint8_t data2_pin_{UINT8_MAX};
  uint8_t data3_pin_{UINT8_MAX};

  // ============ Bus Configuration ============
  uint8_t bus_width_{4};
  uint32_t max_freq_khz_{20000};
  std::string mount_point_{"/sdcard"};

  // ============ State ============
  bool is_mounted_{false};
  void *card_{nullptr};  // sdmmc_card_t pointer

  // ============ Callbacks ============
  CallbackManager<void()> on_mount_callbacks_;
  CallbackManager<void()> on_unmount_callbacks_;
};

}  // namespace sdcard_p4
}  // namespace esphome

