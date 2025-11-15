#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <vector>
#include <string>

namespace esphome {
namespace sdcard_p4 {  // ✅ Изменён namespace

enum class BusWidth : uint8_t {
  BUS_WIDTH_1 = 1,
  BUS_WIDTH_4 = 4,
};

struct FileInfo {
  std::string name;
  size_t size;
  bool is_directory;
  time_t modified_time;
};

class SDCardComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  // Сеттеры для пинов
  void set_clk_pin(InternalGPIOPin *pin) { this->clk_pin_ = pin; }
  void set_cmd_pin(InternalGPIOPin *pin) { this->cmd_pin_ = pin; }
  void set_data0_pin(InternalGPIOPin *pin) { this->data0_pin_ = pin; }
  void set_data1_pin(InternalGPIOPin *pin) { this->data1_pin_ = pin; }
  void set_data2_pin(InternalGPIOPin *pin) { this->data2_pin_ = pin; }
  void set_data3_pin(InternalGPIOPin *pin) { this->data3_pin_ = pin; }

  // Сеттеры для настроек
  void set_mount_point(const std::string &mount_point) { this->mount_point_ = mount_point; }
  void set_bus_width(uint8_t bus_width) { this->bus_width_ = static_cast<BusWidth>(bus_width); }
  void set_max_freq_khz(uint32_t max_freq_khz) { this->max_freq_khz_ = max_freq_khz; }

  // Базовые методы
  bool is_mounted() const { return this->is_mounted_; }
  std::string get_mount_point() const { return this->mount_point_; }
  
  // Работа с директориями
  std::vector<std::string> list_dir(const std::string &path);
  std::vector<FileInfo> list_dir_detailed(const std::string &path);
  bool create_dir(const std::string &path);
  bool remove_dir(const std::string &path);
  
  // Работа с файлами
  bool file_exists(const std::string &path);
  size_t get_file_size(const std::string &path);
  bool remove_file(const std::string &path);
  bool rename_file(const std::string &old_path, const std::string &new_path);
  
  // Чтение/запись файлов
  std::string read_file(const std::string &path);
  bool write_file(const std::string &path, const std::string &content, bool append = false);
  std::vector<uint8_t> read_binary_file(const std::string &path);
  bool write_binary_file(const std::string &path, const std::vector<uint8_t> &data, bool append = false);
  
  // Информация о карте
  uint64_t get_card_size();
  uint64_t get_free_space();
  uint64_t get_used_space();
  float get_usage_percent();
  std::string get_card_type();
  std::string get_card_name();
  uint32_t get_card_speed();
  
  // Утилиты
  bool format_card();
  bool test_card_speed(size_t test_size_kb = 1024);
  void print_card_info();
  
  // Callback для событий
  void add_on_mount_callback(std::function<void()> &&callback) { 
    this->on_mount_callbacks_.add(std::move(callback)); 
  }
  void add_on_unmount_callback(std::function<void()> &&callback) { 
    this->on_unmount_callbacks_.add(std::move(callback)); 
  }

 protected:
  InternalGPIOPin *clk_pin_{nullptr};
  InternalGPIOPin *cmd_pin_{nullptr};
  InternalGPIOPin *data0_pin_{nullptr};
  InternalGPIOPin *data1_pin_{nullptr};
  InternalGPIOPin *data2_pin_{nullptr};
  InternalGPIOPin *data3_pin_{nullptr};

  std::string mount_point_{"/sdcard"};
  BusWidth bus_width_{BusWidth::BUS_WIDTH_4};
  uint32_t max_freq_khz_{20000};
  bool is_mounted_{false};
  bool mount_failed_{false};

  void *card_{nullptr};  // Указатель на sdmmc_card_t
  
  CallbackManager<void()> on_mount_callbacks_;
  CallbackManager<void()> on_unmount_callbacks_;
  
  // Внутренние методы
  bool mount_card_();
  void unmount_card_();
  std::string get_full_path_(const std::string &path);
};

}  // namespace sdcard_p4
}  // namespace esphome
