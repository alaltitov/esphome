#include "i18n.h"

#ifdef USE_I18N

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include <cstring>

namespace esphome {
namespace i18n {

static const char *const TAG = "i18n";

I18nComponent *global_i18n_component = nullptr;

void I18nComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I18N component...");

  ESP_LOGI(TAG, "Free heap BEFORE buffer allocation: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free PSRAM BEFORE buffer allocation: %d bytes", 
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  // Set global instance for automation actions
  global_i18n_component = this;
  esphome::i18n::i18n_init_buffer();

  ESP_LOGI(TAG, "Free heap AFTER buffer allocation: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free PSRAM AFTER buffer allocation: %d bytes", 
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  
  UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(NULL);
  ESP_LOGI(TAG, "Stack high water mark: %d bytes", stack_high_water * sizeof(StackType_t));


  // Initialize translation buffer (allocates memory)
  esphome::i18n::i18n_init_buffer();

  // Verify buffer allocation
  if (esphome::i18n::I18N_USE_PSRAM) {
    ESP_LOGCONFIG(TAG, "  Translation buffer allocated in PSRAM");
#ifdef USE_ESP32
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    ESP_LOGD(TAG, "  PSRAM free after allocation: %d bytes", info.total_free_bytes);
#endif
  } else {
    ESP_LOGCONFIG(TAG, "  Translation buffer allocated in standard RAM");
#ifdef USE_ESP32
    ESP_LOGD(TAG, "  Free heap after allocation: %d bytes", esp_get_free_heap_size());
#elif defined(USE_ESP8266)
    ESP_LOGD(TAG, "  Free heap after allocation: %d bytes", system_get_free_heap_size());
#elif defined(USE_RP2040)
    ESP_LOGD(TAG, "  Heap monitoring not available on RP2040");
#endif
  }

  // Set default locale
  const char *default_loc = esphome::i18n::TRANSLATIONS_DEFAULT_LOCALE;
  esphome::i18n::set_locale(default_loc);
  this->current_locale_ = std::string(default_loc);

  ESP_LOGCONFIG(TAG, "I18N setup complete");
  ESP_LOGCONFIG(TAG, "  Default locale: %s", default_loc);
  ESP_LOGCONFIG(TAG, "  Buffer size: %zu bytes", esphome::i18n::I18N_BUFFER_SIZE);
  ESP_LOGCONFIG(TAG, "  Total translation keys: %zu", esphome::i18n::I18N_KEY_COUNT);
}

void I18nComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "I18N Component:");
  ESP_LOGCONFIG(TAG, "  Current locale: %s", this->get_current_locale().c_str());
  ESP_LOGCONFIG(TAG, "  Total translation keys: %zu", esphome::i18n::I18N_KEY_COUNT);
  ESP_LOGCONFIG(TAG, "  Buffer configuration:");
  ESP_LOGCONFIG(TAG, "    Size: %zu bytes", esphome::i18n::I18N_BUFFER_SIZE);
  ESP_LOGCONFIG(TAG, "    Location: %s", esphome::i18n::I18N_USE_PSRAM ? "PSRAM" : "RAM");

  // Verify internal state matches
  const char *internal_locale = esphome::i18n::get_locale();
  if (internal_locale != nullptr) {
    ESP_LOGCONFIG(TAG, "  Internal locale: %s", internal_locale);
    
    if (strcmp(internal_locale, this->current_locale_.c_str()) != 0) {
      ESP_LOGW(TAG, "  WARNING: Locale mismatch detected!");
      ESP_LOGW(TAG, "    Component locale: %s", this->current_locale_.c_str());
      ESP_LOGW(TAG, "    Internal locale: %s", internal_locale);
    }
  } else {
    ESP_LOGW(TAG, "  WARNING: Internal locale is NULL!");
  }

  // Memory diagnostics
  if (esphome::i18n::I18N_USE_PSRAM) {
#ifdef USE_ESP32
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    uint32_t total_psram = info.total_free_bytes + info.total_allocated_bytes;
    uint32_t used_psram = info.total_allocated_bytes;
    
    ESP_LOGCONFIG(TAG, "  PSRAM statistics:");
    ESP_LOGCONFIG(TAG, "    Total: %d bytes", total_psram);
    ESP_LOGCONFIG(TAG, "    Used: %d bytes (%.1f%%)", used_psram, 
                  (float)used_psram / total_psram * 100.0f);
    ESP_LOGCONFIG(TAG, "    Free: %d bytes (%.1f%%)", info.total_free_bytes,
                  (float)info.total_free_bytes / total_psram * 100.0f);
    ESP_LOGCONFIG(TAG, "    Largest free block: %d bytes", info.largest_free_block);
#else
    ESP_LOGCONFIG(TAG, "  PSRAM mode enabled but not on ESP32 platform");
#endif
  } else {
    ESP_LOGCONFIG(TAG, "  Heap statistics:");
#ifdef USE_ESP32
    ESP_LOGCONFIG(TAG, "    Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGCONFIG(TAG, "    Free internal heap: %d bytes", esp_get_free_internal_heap_size());
    ESP_LOGCONFIG(TAG, "    Min free heap: %d bytes", esp_get_minimum_free_heap_size());
    
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    uint32_t total_sram = info.total_free_bytes + info.total_allocated_bytes;
    ESP_LOGCONFIG(TAG, "    SRAM total: %d, free: %d, largest_free: %d", 
                 total_sram, info.total_free_bytes, info.largest_free_block);
#elif defined(USE_ESP8266)
    ESP_LOGCONFIG(TAG, "    Free heap: %d bytes", system_get_free_heap_size());
#elif defined(USE_RP2040)
    ESP_LOGCONFIG(TAG, "    Heap monitoring not available on RP2040");
#else
    ESP_LOGCONFIG(TAG, "    Heap monitoring not available on this platform");
#endif
  }

  // Test translation to verify setup
  const char *test_key = "test.key";
  const char *test_result = esphome::i18n::tr(test_key);
  ESP_LOGV(TAG, "  Translation test: '%s' -> '%s'", test_key, test_result);
}

void I18nComponent::set_current_locale(const std::string &locale) {
  if (locale.empty()) {
    ESP_LOGW(TAG, "Attempted to set empty locale, ignoring");
    return;
  }

  if (this->current_locale_ != locale) {
    ESP_LOGI(TAG, "Changing locale: '%s' -> '%s'", 
             this->current_locale_.c_str(), locale.c_str());
    
    // Update component state
    this->current_locale_ = locale;
    
    // Update internal translation engine state
    esphome::i18n::set_locale(this->current_locale_.c_str());
    
    // Verify the change
    const char *new_locale = esphome::i18n::get_locale();
    if (new_locale != nullptr) {
      ESP_LOGD(TAG, "Locale changed successfully to: %s", new_locale);
      
      if (strcmp(new_locale, locale.c_str()) != 0) {
        ESP_LOGW(TAG, "Locale mismatch after change!");
        ESP_LOGW(TAG, "  Requested: %s", locale.c_str());
        ESP_LOGW(TAG, "  Actual: %s", new_locale);
      }
    } else {
      ESP_LOGE(TAG, "Failed to verify locale change - internal locale is NULL!");
    }
  } else {
    ESP_LOGV(TAG, "Locale already set to '%s', skipping", locale.c_str());
  }
}

std::string I18nComponent::translate(const std::string &key) {
  if (key.empty()) {
    ESP_LOGV(TAG, "translate() called with empty key");
    return std::string();
  }

  // Use tr() and immediately copy to std::string
  const char *result = esphome::i18n::tr(key.c_str());
  
  if (result == nullptr) {
    ESP_LOGW(TAG, "Translation returned NULL for key: %s", key.c_str());
    return key;  // Fallback to key
  }

  ESP_LOGVV(TAG, "translate('%s') -> '%s'", key.c_str(), result);
  return std::string(result);
}

std::string I18nComponent::translate(const std::string &key, const std::string &locale) {
  if (key.empty()) {
    ESP_LOGV(TAG, "translate() called with empty key");
    return std::string();
  }

  if (locale.empty()) {
    ESP_LOGW(TAG, "translate() called with empty locale, using current locale");
    return this->translate(key);
  }

  // Allocate temporary buffer for explicit locale translation
  // We can't use the global buffer as it would interfere with tr()
  char *temp_buffer = nullptr;
  size_t buffer_size = esphome::i18n::I18N_BUFFER_SIZE;

  // Allocate in same memory type as main buffer
  if (esphome::i18n::I18N_USE_PSRAM) {
#ifdef USE_ESP32
    temp_buffer = (char *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (temp_buffer == nullptr) {
      // Fallback to regular RAM
      ESP_LOGD(TAG, "PSRAM allocation failed for temp buffer, using RAM");
      temp_buffer = (char *)malloc(buffer_size);
    }
#else
    temp_buffer = (char *)malloc(buffer_size);
#endif
  } else {
    temp_buffer = (char *)malloc(buffer_size);
  }

  if (temp_buffer == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate temporary buffer for translation");
    return key;  // Fallback to key
  }

  // Get translation using explicit locale
  esphome::i18n::i18n_get_buf_internal(locale.c_str(), key.c_str(), temp_buffer, buffer_size);
  
  std::string result(temp_buffer);
  
  // Free temporary buffer
  free(temp_buffer);

  ESP_LOGVV(TAG, "translate('%s', '%s') -> '%s'", key.c_str(), locale.c_str(), result.c_str());
  return result;
}

}  // namespace i18n
}  // namespace esphome

#endif  // USE_I18N


