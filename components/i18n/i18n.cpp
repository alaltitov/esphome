#include "i18n.h"

#ifdef USE_I18N

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cstring>

namespace esphome {
namespace i18n {

static const char *const TAG = "i18n";

// Global component pointer for lambda access
I18nComponent *global_i18n_component = nullptr;

void I18nComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I18N component...");

  // Set global pointer for easy access
  global_i18n_component = this;

  // Initialize with default locale
  const char *default_loc = esphome::i18n::TRANSLATIONS_DEFAULT_LOCALE;
  esphome::i18n::set_locale(default_loc);
  this->current_locale_ = std::string(default_loc);

  ESP_LOGCONFIG(TAG, "I18N setup complete. Default locale: %s", default_loc);
}

void I18nComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "I18N Component");
  ESP_LOGCONFIG(TAG, "  Current locale: %s", this->get_current_locale().c_str());
  ESP_LOGCONFIG(TAG, "  Available translations: %zu keys", esphome::i18n::I18N_KEY_COUNT);

  // Verify internal state matches
  const char *internal_locale = esphome::i18n::get_locale();
  ESP_LOGCONFIG(TAG, "  Internal locale: %s", internal_locale ? internal_locale : "NULL");
}

void I18nComponent::set_current_locale(const std::string &locale) {
  // Only change if different to avoid unnecessary updates
  if (this->current_locale_ != locale) {
    ESP_LOGI(TAG, "Changing locale from '%s' to '%s'", this->current_locale_.c_str(), locale.c_str());
    
    // Update internal state FIRST
    this->current_locale_ = locale;
    esphome::i18n::set_locale(this->current_locale_.c_str());
    
    // Verify it was set correctly
    const char *new_locale = esphome::i18n::get_locale();
    ESP_LOGD(TAG, "Locale changed successfully. Internal locale is now: %s", new_locale);
    
    // TODO: Trigger UI update event for LVGL labels
    // This would allow automatic label updates without manual refresh
  } else {
    ESP_LOGV(TAG, "Locale already set to '%s', skipping", locale.c_str());
  }
}

std::string I18nComponent::get_current_locale() {
  // Always sync with internal state
  const char *internal_locale = esphome::i18n::get_locale();
  if (internal_locale) {
    std::string internal_str(internal_locale);
    if (this->current_locale_ != internal_str) {
      ESP_LOGW(TAG, "Locale mismatch! Component: '%s', Internal: '%s'. Syncing...", 
               this->current_locale_.c_str(), internal_locale);
      this->current_locale_ = internal_str;
    }
  }
  return this->current_locale_;
}

std::string I18nComponent::translate(const std::string &key) {
  // Get current locale
  const char *current_loc = this->current_locale_.c_str();
  
  ESP_LOGVV(TAG, "Translating key='%s' with locale='%s'", key.c_str(), current_loc);
  
  // Use a local buffer to avoid static caching
  char buf[256];
  esphome::i18n::i18n_get_buf_internal(current_loc, key.c_str(), buf, sizeof(buf));
  
  ESP_LOGVV(TAG, "Translation result: '%s'", buf);
  
  return std::string(buf);
}

std::string I18nComponent::translate(const std::string &key, const std::string &locale) {
  ESP_LOGVV(TAG, "Translating key='%s' with explicit locale='%s'", key.c_str(), locale.c_str());
  
  // Use a local buffer
  char buf[256];
  esphome::i18n::i18n_get_buf_internal(locale.c_str(), key.c_str(), buf, sizeof(buf));
  
  ESP_LOGVV(TAG, "Translation result: '%s'", buf);
  
  return std::string(buf);
}

}  // namespace i18n
}  // namespace esphome

#endif  // USE_I18N
