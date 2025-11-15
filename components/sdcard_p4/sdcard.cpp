#include "sd_card.h"

#ifdef ESP_PLATFORM
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_err.h"
#include "driver/gpio.h"
#endif

namespace esphome {
namespace sd_card_esp32p4 {

static const char *const TAG = "sd_card_esp32p4";

void SDCardComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SD Card (SDMMC mode)...");
  
  if (this->mount_card_()) {
    ESP_LOGI(TAG, "SD Card mounted successfully");
    this->mounted_ = true;
  } else {
    ESP_LOGE(TAG, "Failed to mount SD Card");
    this->mark_failed();
  }
}

void SDCardComponent::loop() {
  // Можно добавить периодическую проверку состояния
}

void SDCardComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card ESP32-P4 (SDMMC):");
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  CMD Pin: ", this->cmd_pin_);
  LOG_PIN("  DATA0 Pin: ", this->data0_pin_);
  if (this->bus_width_ == 4) {
    LOG_PIN("  DATA1 Pin: ", this->data1_pin_);
    LOG_PIN("  DATA2 Pin: ", this->data2_pin_);
    LOG_PIN("  DATA3 Pin: ", this->data3_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Mount Point: %s", this->mount_point_.c_str());
  ESP_LOGCONFIG(TAG, "  Bus Width: %d-bit", this->bus_width_);
  ESP_LOGCONFIG(TAG, "  Max Frequency: %d kHz", this->max_freq_khz_);
  ESP_LOGCONFIG(TAG, "  Status: %s", this->mounted_ ? "Mounted" : "Not Mounted");
  
  if (this->mounted_) {
    ESP_LOGCONFIG(TAG, "  Card Type: %s", this->get_card_type());
    ESP_LOGCONFIG(TAG, "  Card Size: %.2f GB", this->get_card_size() / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGCONFIG(TAG, "  Used Space: %.2f MB", this->get_used_space() / (1024.0 * 1024.0));
    ESP_LOGCONFIG(TAG, "  Free Space: %.2f MB", this->get_free_space() / (1024.0 * 1024.0));
  }
}

bool SDCardComponent::mount_card_() {
#ifdef ESP_PLATFORM
  esp_err_t ret;
  
  // Настройка SDMMC хоста
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = this->max_freq_khz_;
  
  // Настройка пинов для SDMMC
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  
  // Устанавливаем ширину шины
  if (this->bus_width_ == 1) {
    slot_config.width = 1;
  } else {
    slot_config.width = 4;
  }
  
  // Настройка пинов
  slot_config.clk = (gpio_num_t)this->clk_pin_->get_pin();
  slot_config.cmd = (gpio_num_t)this->cmd_pin_->get_pin();
  slot_config.d0 = (gpio_num_t)this->data0_pin_->get_pin();
  
  if (this->bus_width_ == 4) {
    slot_config.d1 = (gpio_num_t)this->data1_pin_->get_pin();
    slot_config.d2 = (gpio_num_t)this->data2_pin_->get_pin();
    slot_config.d3 = (gpio_num_t)this->data3_pin_->get_pin();
  }
  
  // Отключаем внутренние подтяжки (если у вас есть внешние)
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
  
  // Настройка монтирования
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 10,
    .allocation_unit_size = 16 * 1024,
    .disk_status_check_enable = false
  };
  
  ESP_LOGI(TAG, "Mounting SD card...");
  ESP_LOGI(TAG, "  CLK: GPIO%d", slot_config.clk);
  ESP_LOGI(TAG, "  CMD: GPIO%d", slot_config.cmd);
  ESP_LOGI(TAG, "  D0:  GPIO%d", slot_config.d0);
  if (this->bus_width_ == 4) {
    ESP_LOGI(TAG, "  D1:  GPIO%d", slot_config.d1);
    ESP_LOGI(TAG, "  D2:  GPIO%d", slot_config.d2);
    ESP_LOGI(TAG, "  D3:  GPIO%d", slot_config.d3);
  }
  
  // Монтирование карты через SDMMC
  ret = esp_vfs_fat_sdmmc_mount(
    this->mount_point_.c_str(),
    &host,
    &slot_config,
    &mount_config,
    &this->card_
  );
  
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
               "If you want the card to be formatted, set format_if_mount_failed = true.");
    } else if (ret == ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "SD card already mounted");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SD card (%s). "
               "Make sure SD card lines have pull-up resistors in place.", 
               esp_err_to_name(ret));
    }
    return false;
  }
  
  // Вывод информации о карте
  ESP_LOGI(TAG, "SD card mounted successfully");
  sdmmc_card_print_info(stdout, this->card_);
  
  return true;
#else
  ESP_LOGE(TAG, "SD Card support only available on ESP32 platform");
  return false;
#endif
}

void SDCardComponent::unmount_card_() {
#ifdef ESP_PLATFORM
  if (this->mounted_) {
    esp_vfs_fat_sdcard_unmount(this->mount_point_.c_str(), this->card_);
    this->mounted_ = false;
    ESP_LOGI(TAG, "SD Card unmounted");
  }
#endif
}

std::string SDCardComponent::get_full_path_(const char *path) {
  if (path[0] == '/') {
    return this->mount_point_ + std::string(path);
  }
  return this->mount_point_ + "/" + std::string(path);
}

bool SDCardComponent::file_exists(const char *path) {
  if (!this->mounted_) return false;
  
  std::string full_path = this->get_full_path_(path);
  struct stat st;
  return (stat(full_path.c_str(), &st) == 0);
}

size_t SDCardComponent::get_file_size(const char *path) {
  if (!this->mounted_) return 0;
  
  std::string full_path = this->get_full_path_(path);
  struct stat st;
  if (stat(full_path.c_str(), &st) == 0) {
    return st.st_size;
  }
  return 0;
}

bool SDCardComponent::read_file(const char *path, std::vector<uint8_t> &data) {
  if (!this->mounted_) {
    ESP_LOGE(TAG, "SD Card not mounted");
    return false;
  }
  
  std::string full_path = this->get_full_path_(path);
  
  FILE *file = fopen(full_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file: %s", full_path.c_str());
    return false;
  }
  
  // Получаем размер файла
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  if (file_size <= 0) {
    fclose(file);
    ESP_LOGE(TAG, "Invalid file size: %ld", file_size);
    return false;
  }
  
  // Читаем файл
  data.resize(file_size);
  size_t bytes_read = fread(data.data(), 1, file_size, file);
  fclose(file);
  
  if (bytes_read != (size_t)file_size) {
    ESP_LOGE(TAG, "Failed to read complete file. Expected: %ld, Read: %d", file_size, bytes_read);
    return false;
  }
  
  ESP_LOGD(TAG, "Successfully read %d bytes from %s", bytes_read, path);
  return true;
}

bool SDCardComponent::read_file_chunked(const char *path, std::function<bool(const uint8_t*, size_t)> callback, size_t chunk_size) {
  if (!this->mounted_) {
    ESP_LOGE(TAG, "SD Card not mounted");
    return false;
  }
  
  std::string full_path = this->get_full_path_(path);
  
  FILE *file = fopen(full_path.c_str(), "rb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file: %s", full_path.c_str());
    return false;
  }
  
  std::vector<uint8_t> buffer(chunk_size);
  size_t bytes_read;
  bool success = true;
  
  while ((bytes_read = fread(buffer.data(), 1, chunk_size, file)) > 0) {
    if (!callback(buffer.data(), bytes_read)) {
      success = false;
      break;
    }
  }
  
  fclose(file);
  return success;
}

bool SDCardComponent::write_file(const char *path, const uint8_t *data, size_t length) {
  if (!this->mounted_) {
    ESP_LOGE(TAG, "SD Card not mounted");
    return false;
  }
  
  std::string full_path = this->get_full_path_(path);
  
  FILE *file = fopen(full_path.c_str(), "wb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to create file: %s", full_path.c_str());
    return false;
  }
  
  size_t bytes_written = fwrite(data, 1, length, file);
  fclose(file);
  
  if (bytes_written != length) {
    ESP_LOGE(TAG, "Failed to write complete file. Expected: %d, Written: %d", length, bytes_written);
    return false;
  }
  
  ESP_LOGD(TAG, "Successfully wrote %d bytes to %s", bytes_written, path);
  return true;
}

bool SDCardComponent::append_file(const char *path, const uint8_t *data, size_t length) {
  if (!this->mounted_) {
    ESP_LOGE(TAG, "SD Card not mounted");
    return false;
  }
  
  std::string full_path = this->get_full_path_(path);
  
  FILE *file = fopen(full_path.c_str(), "ab");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for append: %s", full_path.c_str());
    return false;
  }
  
  size_t bytes_written = fwrite(data, 1, length, file);
  fclose(file);
  
  return (bytes_written == length);
}

bool SDCardComponent::delete_file(const char *path) {
  if (!this->mounted_) return false;
  
  std::string full_path = this->get_full_path_(path);
  int result = unlink(full_path.c_str());
  
  if (result == 0) {
    ESP_LOGD(TAG, "Deleted file: %s", path);
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to delete file: %s", path);
    return false;
  }
}

bool SDCardComponent::rename_file(const char *old_path, const char *new_path) {
  if (!this->mounted_) return false;
  
  std::string full_old_path = this->get_full_path_(old_path);
  std::string full_new_path = this->get_full_path_(new_path);
  
  int result = rename(full_old_path.c_str(), full_new_path.c_str());
  
  if (result == 0) {
    ESP_LOGD(TAG, "Renamed file: %s -> %s", old_path, new_path);
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to rename file: %s -> %s", old_path, new_path);
    return false;
  }
}

bool SDCardComponent::create_directory(const char *path) {
#ifdef ESP_PLATFORM
  if (!this->mounted_) return false;
  
  std::string full_path = this->get_full_path_(path);
  
  // Используем FatFS API
  FRESULT res = f_mkdir(full_path.c_str());
  
  if (res == FR_OK) {
    ESP_LOGD(TAG, "Created directory: %s", path);
    return true;
  } else if (res == FR_EXIST) {
    ESP_LOGD(TAG, "Directory already exists: %s", path);
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to create directory: %s (error: %d)", path, res);
    return false;
  }
#else
  return false;
#endif
}

bool SDCardComponent::list_directory(const char *path, std::vector<FileInfo> &files) {
#ifdef ESP_PLATFORM
  if (!this->mounted_) return false;
  
  std::string full_path = this->get_full_path_(path);
  files.clear();
  
  DIR dir;
  FILINFO fno;
  FRESULT res;
  
  res = f_opendir(&dir, full_path.c_str());
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to open directory: %s (error: %d)", path, res);
    return false;
  }
  
  while (true) {
    res = f_readdir(&dir, &fno);
    if (res != FR_OK || fno.fname[0] == 0) break;
    
    FileInfo info;
    info.name = fno.fname;
    info.size = fno.fsize;
    info.is_directory = (fno.fattrib & AM_DIR) != 0;
    
    files.push_back(info);
  }
  
  f_closedir(&dir);
  ESP_LOGD(TAG, "Listed %d items in directory: %s", files.size(), path);
  return true;
#else
  return false;
#endif
}

bool SDCardComponent::read_image(const char *path, std::vector<uint8_t> &image_data) {
  return this->read_file(path, image_data);
}

bool SDCardComponent::read_audio(const char *path, std::vector<uint8_t> &audio_data) {
  return this->read_file(path, audio_data);
}

uint64_t SDCardComponent::get_card_size() {
#ifdef ESP_PLATFORM
  if (!this->mounted_ || this->card_ == nullptr) return 0;
  return ((uint64_t)this->card_->csd.capacity) * this->card_->csd.sector_size;
#else
  return 0;
#endif
}

uint64_t SDCardComponent::get_free_space() {
#ifdef ESP_PLATFORM
  if (!this->mounted_) return 0;
  
  FATFS *fs;
  DWORD free_clusters;
  FRESULT res;
  
  res = f_getfree("0:", &free_clusters, &fs);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get free space (error: %d)", res);
    return 0;
  }
  
  uint64_t free_bytes = (uint64_t)free_clusters * fs->csize * fs->ssize;
  return free_bytes;
#else
  return 0;
#endif
}

uint64_t SDCardComponent::get_used_space() {
  uint64_t total = this->get_card_size();
  uint64_t free = this->get_free_space();
  return (total > free) ? (total - free) : 0;
}

const char* SDCardComponent::get_card_type() {
#ifdef ESP_PLATFORM
  if (!this->mounted_ || this->card_ == nullptr) return "Unknown";
  
  if (this->card_->ocr & SD_OCR_SDHC_CAP) {
    return "SDHC/SDXC";
  } else {
    return "SDSC";
  }
#else
  return "Unknown";
#endif
}

}  // namespace sd_card_esp32p4
}  // namespace esphome
