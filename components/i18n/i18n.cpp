#include "i18n.h"

#ifdef USE_I18N

#include "esphome/core/log.h"
#include "esphome/core/application.h"

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
    
    // Update internal state
    this->current_locale_ = locale;
    esphome::i18n::set_locale(locale.c_str());
    
    ESP_LOGD(TAG, "Locale changed successfully");
    
    // TODO: Trigger UI update event for LVGL labels
    // This would allow automatic label updates without manual refresh
  }
}

std::string I18nComponent::get_current_locale() {
  // Sync with internal state in case it was changed directly
  const char *internal_locale = esphome::i18n::get_locale();
  if (internal_locale && this->current_locale_ != std::string(internal_locale)) {
    this->current_locale_ = std::string(internal_locale);
  }
  return this->current_locale_;
}

std::string I18nComponent::translate(const std::string &key) {
  // Translate key using current locale
  const char *result = esphome::i18n::tr(key.c_str());
  
  // Return translated string or original key if not found
  return std::string(result ? result : key);
}

}  // namespace i18n
}  // namespace esphome

#endif  // USE_I18N
