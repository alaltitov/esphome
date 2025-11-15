#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <vector>
#include <string>

#ifdef ESP_PLATFORM
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "ff.h"
#endif

namespace esphome {
namespace sd_card_esp32p4 {

enum BusWidth : uint8_t {
  BUS_WIDTH_1 = 1,
  BUS_WIDTH_4 = 4,
};

struct FileInfo {
  std::string name;
  size_t size;
  bool is_directory;
};

class SDCardComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Настройка пинов для SDMMC режима
  void set_clk_pin(GPIOPin *clk_pin) { clk_pin_ = clk_pin; }
  void set_cmd_pin(GPIOPin *cmd_pin) { cmd_pin_ = cmd_pin; }
  void set_data0_pin(GPIOPin *data0_pin) { data0_pin_ = data0_pin; }
  void set_data1_pin(GPIOPin *data1_pin) { data1_pin_ = data1_pin; }
  void set_data2_pin(GPIOPin *data2_pin) { data2_pin_ = data2_pin; }
  void set_data3_pin(GPIOPin *data3_pin) { data3_pin_ = data3_pin; }
  
  void set_mount_point(const std::string &mount_point) { mount_point_ = mount_point; }
  void set_bus_width(uint8_t width) { bus_width_ = width; }
  void set_max_freq_khz(uint32_t freq) { max_freq_khz_ = freq; }

  // Основные функции работы с файлами
  bool file_exists(const char *path);
  bool read_file(const char *path, std::vector<uint8_t> &data);
  bool read_file_chunked(const char *path, std::function<bool(const uint8_t*, size_t)> callback, size_t chunk_size = 4096);
  bool write_file(const char *path, const uint8_t *data, size_t length);
  bool append_file(const char *path, const uint8_t *data, size_t length);
  bool delete_file(const char *path);
  bool rename_file(const char *old_path, const char *new_path);
  bool create_directory(const char *path);
  bool list_directory(const char *path, std::vector<FileInfo> &files);
  size_t get_file_size(const char *path);
  
  // Функции для работы с медиа
  bool read_image(const char *path, std::vector<uint8_t> &image_data);
  bool read_audio(const char *path, std::vector<uint8_t> &audio_data);
  
  // Получение информации о карте
  uint64_t get_card_size();
  uint64_t get_used_space();
  uint64_t get_free_space();
  const char* get_card_type();
  bool is_mounted() { return mounted_; }

 protected:
  GPIOPin *clk_pin_{nullptr};
  GPIOPin *cmd_pin_{nullptr};
  GPIOPin *data0_pin_{nullptr};
  GPIOPin *data1_pin_{nullptr};
  GPIOPin *data2_pin_{nullptr};
  GPIOPin *data3_pin_{nullptr};
  
  std::string mount_point_{"/sdcard"};
  uint8_t bus_width_{4};
  uint32_t max_freq_khz_{20000};
  bool mounted_{false};
  
#ifdef ESP_PLATFORM
  sdmmc_card_t *card_{nullptr};
#endif

  bool mount_card_();
  void unmount_card_();
  std::string get_full_path_(const char *path);
};

}  // namespace sd_card_esp32p4
}  // namespace esphome
