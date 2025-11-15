#include "sdcard.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <cstring>
#include <ctime>

// ✅ Используем ESP-IDF VFS API вместо POSIX
#include <esp_vfs.h>

namespace esphome {
namespace sdcard_p4 {

static const char *const TAG = "sdcard_p4";

void SDCardComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD Card...");
  
  if (this->clk_pin_ == 0 || this->cmd_pin_ == 0 || this->data0_pin_ == 0) {
    ESP_LOGE(TAG, "Required pins not configured!");
    this->mark_failed();
    return;
  }

  if (this->bus_width_ == 4) {
    if (this->data1_pin_ == UINT8_MAX || this->data2_pin_ == UINT8_MAX || this->data3_pin_ == UINT8_MAX) {
      ESP_LOGE(TAG, "4-bit mode requires DATA1, DATA2, DATA3 pins!");
      this->mark_failed();
      return;
    }
  }

  this->mount_card_();
}

void SDCardComponent::loop() {
  // Можно добавить периодическую проверку статуса карты
}

bool SDCardComponent::mount_card_() {
  // Конфигурация хоста
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = this->max_freq_khz_;

  // Конфигурация слота
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  
  // Установка ширины шины
  if (this->bus_width_ == 1) {
    slot_config.width = 1;
    ESP_LOGCONFIG(TAG, "Using 1-bit bus width");
  } else {
    slot_config.width = 4;
    ESP_LOGCONFIG(TAG, "Using 4-bit bus width");
  }

  // Установка пинов
  slot_config.clk = (gpio_num_t)this->clk_pin_;
  slot_config.cmd = (gpio_num_t)this->cmd_pin_;
  slot_config.d0 = (gpio_num_t)this->data0_pin_;
  
  if (this->bus_width_ == 4) {
    slot_config.d1 = (gpio_num_t)this->data1_pin_;
    slot_config.d2 = (gpio_num_t)this->data2_pin_;
    slot_config.d3 = (gpio_num_t)this->data3_pin_;
  }

  // Дополнительные настройки слота
  slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  // Конфигурация монтирования
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 10,
      .allocation_unit_size = 16 * 1024,
      .disk_status_check_enable = false
  };

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(
      this->mount_point_.c_str(), 
      &host, 
      &slot_config, 
      &mount_config, 
      &card
  );

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem. Is SD card formatted?");
    } else if (ret == ESP_ERR_TIMEOUT) {
      ESP_LOGE(TAG, "SD card initialization timeout");
    } else if (ret == ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "SD card in invalid state");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SD card: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    this->mount_failed_ = true;
    this->mark_failed();
    return false;
  }

  this->card_ = card;
  this->is_mounted_ = true;
  this->mount_failed_ = false;

  ESP_LOGI(TAG, "✓ SD Card mounted successfully!");
  this->print_card_info();
  
  this->on_mount_callbacks_.call();
  
  return true;
}

void SDCardComponent::unmount_card_() {
  if (this->is_mounted_) {
    esp_vfs_fat_sdcard_unmount(this->mount_point_.c_str(), static_cast<sdmmc_card_t*>(this->card_));
    this->is_mounted_ = false;
    this->card_ = nullptr;
    
    ESP_LOGI(TAG, "SD Card unmounted");
    this->on_unmount_callbacks_.call();
  }
}

void SDCardComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card P4:");
  ESP_LOGCONFIG(TAG, "  Mount Point: %s", this->mount_point_.c_str());
  ESP_LOGCONFIG(TAG, "  Bus Width: %d-bit", this->bus_width_);
  ESP_LOGCONFIG(TAG, "  Max Frequency: %d kHz", this->max_freq_khz_);
  
  ESP_LOGCONFIG(TAG, "  CLK Pin: GPIO%d", this->clk_pin_);
  ESP_LOGCONFIG(TAG, "  CMD Pin: GPIO%d", this->cmd_pin_);
  ESP_LOGCONFIG(TAG, "  DATA0 Pin: GPIO%d", this->data0_pin_);
  
  if (this->bus_width_ == 4) {
    ESP_LOGCONFIG(TAG, "  DATA1 Pin: GPIO%d", this->data1_pin_);
    ESP_LOGCONFIG(TAG, "  DATA2 Pin: GPIO%d", this->data2_pin_);
    ESP_LOGCONFIG(TAG, "  DATA3 Pin: GPIO%d", this->data3_pin_);
  }
  
  if (this->mount_failed_) {
    ESP_LOGCONFIG(TAG, "  Status: Mount Failed ✗");
  } else if (this->is_mounted_) {
    ESP_LOGCONFIG(TAG, "  Status: Mounted ✓");
    ESP_LOGCONFIG(TAG, "  Card Name: %s", get_card_name().c_str());
    ESP_LOGCONFIG(TAG, "  Card Type: %s", get_card_type().c_str());
    ESP_LOGCONFIG(TAG, "  Card Speed: %d kHz", get_card_speed());
    ESP_LOGCONFIG(TAG, "  Card Size: %.2f GB", get_card_size() / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGCONFIG(TAG, "  Free Space: %.2f GB", get_free_space() / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGCONFIG(TAG, "  Used Space: %.2f GB", get_used_space() / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGCONFIG(TAG, "  Usage: %.1f%%", get_usage_percent());
  } else {
    ESP_LOGCONFIG(TAG, "  Status: Not Mounted");
  }
}

std::string SDCardComponent::get_full_path_(const std::string &path) {
  if (path.empty() || path[0] != '/') {
    return this->mount_point_ + "/" + path;
  }
  return this->mount_point_ + path;
}

// ==================== Работа с директориями (ESP-IDF VFS) ====================

std::vector<std::string> SDCardComponent::list_dir(const std::string &path) {
  std::vector<std::string> files;
  
  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "SD Card not mounted");
    return files;
  }

  std::string full_path = get_full_path_(path);
  
  // ✅ Используем ESP-IDF VFS API
  struct stat st;
  if (stat(full_path.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Failed to stat directory: %s", full_path.c_str());
    return files;
  }

  // Простой способ - читаем через FILE API
  DIR *dir = nullptr;
  struct dirent *entry = nullptr;
  
  // Пробуем открыть через стандартный API (работает через VFS)
  dir = opendir(full_path.c_str());
  if (dir) {
    while ((entry = readdir(dir)) != nullptr) {
      if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
        files.push_back(entry->d_name);
      }
    }
    closedir(dir);
  } else {
    ESP_LOGE(TAG, "Failed to open directory: %s", full_path.c_str());
  }
  
  return files;
}

std::vector<FileInfo> SDCardComponent::list_dir_detailed(const std::string &path) {
  std::vector<FileInfo> files;
  
  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "SD Card not mounted");
    return files;
  }

  std::string full_path = get_full_path_(path);
  
  DIR *dir = opendir(full_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", full_path.c_str());
    return files;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    
    FileInfo info;
    info.name = entry->d_name;
    
    std::string item_path = full_path + "/" + entry->d_name;
    struct stat st;
    if (stat(item_path.c_str(), &st) == 0) {
      info.size = st.st_size;
      info.is_directory = S_ISDIR(st.st_mode);
      info.modified_time = st.st_mtime;
    } else {
      info.size = 0;
      info.is_directory = false;
      info.modified_time = 0;
    }
    
    files.push_back(info);
  }
  
  closedir(dir);
  return files;
}

bool SDCardComponent::create_dir(const std::string &path) {
  if (!this->is_mounted_) return false;
  
  std::string full_path = get_full_path_(path);
  
  // ✅ Используем mkdir через VFS
  int ret = mkdir(full_path.c_str(), 0755);
  
  if (ret != 0 && errno != EEXIST) {
    ESP_LOGE(TAG, "Failed to create directory: %s (errno: %d)", full_path.c_str(), errno);
    return false;
  }
  
  ESP_LOGI(TAG, "Created directory: %s", path.c_str());
  return true;
}

bool SDCardComponent::remove_dir(const std::string &path) {
  if (!this->is_mounted_) return false;
  
  std::string full_path = get_full_path_(path);
  
  // ✅ Используем rmdir через VFS
  int ret = rmdir(full_path.c_str());
  
  if (ret != 0) {
    ESP_LOGE(TAG, "Failed to remove directory: %s (errno: %d)", full_path.c_str(), errno);
    return false;
  }
  
  ESP_LOGI(TAG, "Removed directory: %s", path.c_str());
  return true;
}

// ==================== Работа с файлами ====================

bool SDCardComponent::file_exists(const std::string &path) {
  if (!this->is_mounted_) return false;
  
  std::string full_path = get_full_path_(path);
  struct stat st;
  return stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

size_t SDCardComponent::get_file_size(const std::string &path) {
  if (!this->is_mounted_) return 0;
  
  std::string full_path = get_full_path_(path);
  struct stat st;
  if (stat(full_path.c_str(), &st) == 0) {
    return st.st_size;
  }
  return 0;
}

bool SDCardComponent::delete_file(const std::string &path) {
  if (!this->is_mounted_) return false;
  
  std::string full_path = get_full_path_(path);
  int ret = unlink(full_path.c_str());
  
  if (ret != 0) {
    ESP_LOGE(TAG, "Failed to delete file: %s (errno: %d)", full_path.c_str(), errno);
    return false;
  }
  
  ESP_LOGI(TAG, "Deleted file: %s", path.c_str());
  return true;
}

bool SDCardComponent::rename_file(const std::string &old_path, const std::string &new_path) {
  if (!this->is_mounted_) return false;
  
  std::string full_old_path = get_full_path_(old_path);
  std::string full_new_path = get_full_path_(new_path);
  
  int ret = rename(full_old_path.c_str(), full_new_path.c_str());
  
  if (ret != 0) {
    ESP_LOGE(TAG, "Failed to rename file from %s to %s (errno: %d)", old_path.c_str(), new_path.c_str(), errno);
    return false;
  }
  
  ESP_LOGI(TAG, "Renamed file: %s -> %s", old_path.c_str(), new_path.c_str());
  return true;
}

// ==================== Чтение/запись файлов ====================

std::string SDCardComponent::read_file(const std::string &path) {
  if (!this->is_mounted_) return "";
  
  std::string full_path = get_full_path_(path);
  FILE *file = fopen(full_path.c_str(), "r");
  
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", full_path.c_str());
    return "";
  }
  
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  std::string content;
  content.resize(file_size);
  
  size_t read_size = fread(&content[0], 1, file_size, file);
  fclose(file);
  
  if (read_size != (size_t)file_size) {
    ESP_LOGW(TAG, "Read size mismatch for file: %s", path.c_str());
  }
  
  return content;
}

bool SDCardComponent::write_file(const std::string &path, const std::string &content) {
  if (!this->is_mounted_) return false;
  
  std::string full_path = get_full_path_(path);
  FILE *file = fopen(full_path.c_str(), "w");
  
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path.c_str());
    return false;
  }
  
  size_t written = fwrite(content.c_str(), 1, content.length(), file);
  fclose(file);
  
  if (written != content.length()) {
    ESP_LOGE(TAG, "Failed to write complete content to file: %s", path.c_str());
    return false;
  }
  
  ESP_LOGI(TAG, "Written %zu bytes to file: %s", written, path.c_str());
  return true;
}

bool SDCardComponent::append_file(const std::string &path, const std::string &content) {
  if (!this->is_mounted_) return false;
  
  std::string full_path = get_full_path_(path);
  FILE *file = fopen(full_path.c_str(), "a");
  
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for appending: %s", full_path.c_str());
    return false;
  }
  
  size_t written = fwrite(content.c_str(), 1, content.length(), file);
  fclose(file);
  
  if (written != content.length()) {
    ESP_LOGE(TAG, "Failed to append complete content to file: %s", path.c_str());
    return false;
  }
  
  ESP_LOGI(TAG, "Appended %zu bytes to file: %s", written, path.c_str());
  return true;
}

std::vector<uint8_t> SDCardComponent::read_binary_file(const std::string &path) {
  std::vector<uint8_t> data;
  
  if (!this->is_mounted_) return data;
  
  std::string full_path = get_full_path_(path);
  FILE *file = fopen(full_path.c_str(), "rb");
  
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for reading: %s", full_path.c_str());
    return data;
  }
  
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  data.resize(file_size);
  size_t read_size = fread(data.data(), 1, file_size, file);
  fclose(file);
  
  if (read_size != (size_t)file_size) {
    ESP_LOGW(TAG, "Read size mismatch for file: %s", path.c_str());
    data.resize(read_size);
  }
  
  return data;
}

bool SDCardComponent::write_binary_file(const std::string &path, const std::vector<uint8_t> &data, bool append) {
  if (!this->is_mounted_) return false;
  
  std::string full_path = get_full_path_(path);
  const char *mode = append ? "ab" : "wb";
  FILE *file = fopen(full_path.c_str(), mode);
  
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path.c_str());
    return false;
  }
  
  size_t written = fwrite(data.data(), 1, data.size(), file);
  fclose(file);
  
  if (written != data.size()) {
    ESP_LOGE(TAG, "Failed to write complete data to file: %s", path.c_str());
    return false;
  }
  
  ESP_LOGI(TAG, "Written %zu bytes to file: %s", written, path.c_str());
  return true;
}

// ==================== Информация о карте ====================

uint64_t SDCardComponent::get_card_size() const {
  if (!this->is_mounted_ || !this->card_) return 0;
  
  sdmmc_card_t *card = static_cast<sdmmc_card_t *>(this->card_);
  return ((uint64_t)card->csd.capacity) * card->csd.sector_size;
}

uint64_t SDCardComponent::get_free_space() const {
  if (!this->is_mounted_) return 0;
  
  FATFS *fs;
  DWORD free_clusters;
  
  if (f_getfree("0:", &free_clusters, &fs) == FR_OK) {
    return (uint64_t)free_clusters * fs->csize * 512;
  }
  return 0;
}

uint64_t SDCardComponent::get_used_space() const {
  return get_card_size() - get_free_space();
}

float SDCardComponent::get_usage_percent() const {
  uint64_t total = get_card_size();
  if (total == 0) return 0.0f;
  
  uint64_t used = get_used_space();
  return (used * 100.0f) / total;
}

std::string SDCardComponent::get_card_type() const {
  if (!this->is_mounted_ || !this->card_) {
    return "Unknown";
  }

  sdmmc_card_t *card = static_cast<sdmmc_card_t *>(this->card_);
  
  if (card->is_sdio) {
    return "SDIO";
  } else if (card->is_mmc) {
    return "MMC";
  } else {
    if (card->ocr & (1 << 30)) {
      if (card->csd.capacity > 32 * 1024 * 1024 / 512) {
        return "SDXC";
      } else {
        return "SDHC";
      }
    } else {
      return "SDSC";
    }
  }
}

std::string SDCardComponent::get_card_name() const {
  if (!this->is_mounted_ || !this->card_) return "Unknown";
  
  sdmmc_card_t *card = static_cast<sdmmc_card_t *>(this->card_);
  return std::string(card->cid.name);
}

uint32_t SDCardComponent::get_card_speed() const {
  if (!this->is_mounted_ || !this->card_) return 0;
  
  sdmmc_card_t *card = static_cast<sdmmc_card_t *>(this->card_);
  return card->max_freq_khz;
}

void SDCardComponent::print_card_info() {
  if (!this->is_mounted_ || !this->card_) return;
  
  sdmmc_card_t *card = static_cast<sdmmc_card_t *>(this->card_);
  sdmmc_card_print_info(stdout, card);
}

bool SDCardComponent::format_card() {
  ESP_LOGW(TAG, "Format not implemented yet");
  return false;
}

bool SDCardComponent::test_card_speed(size_t test_size_kb) {
  if (!this->is_mounted_) return false;
  
  ESP_LOGI(TAG, "Testing SD card speed with %zu KB...", test_size_kb);
  
  std::string test_file = "/speed_test.tmp";
  std::vector<uint8_t> test_data(test_size_kb * 1024, 0xAA);
  
  uint32_t start_time = millis();
  bool write_ok = write_binary_file(test_file, test_data, false);
  uint32_t write_time = millis() - start_time;
  
  if (!write_ok) {
    ESP_LOGE(TAG, "Write test failed");
    return false;
  }
  
  float write_speed = (test_size_kb * 1000.0f) / write_time;
  ESP_LOGI(TAG, "Write speed: %.2f KB/s", write_speed);
  
  start_time = millis();
  auto read_data = read_binary_file(test_file);
  uint32_t read_time = millis() - start_time;
  
  if (read_data.size() != test_data.size()) {
    ESP_LOGE(TAG, "Read test failed");
    delete_file(test_file);
    return false;
  }
  
  float read_speed = (test_size_kb * 1000.0f) / read_time;
  ESP_LOGI(TAG, "Read speed: %.2f KB/s", read_speed);
  
  delete_file(test_file);
  
  return true;
}

}  // namespace sdcard_p4
}  // namespace esphome

#endif  // USE_ESP32
