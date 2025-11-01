#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "generated/translations.h"

#include <string>

namespace esphome {
namespace i18n {

/**
 * @brief I18N Component for ESPHome with LVGL integration
 * 
 * Provides internationalization support with:
 * - Runtime locale switching
 * - PROGMEM storage for translations (Flash memory)
 * - Optional PSRAM buffer support
 * - Zero-copy translation access via tr()
 * - Safe copy translation access via translate()
 * 
 * @warning This component is NOT thread-safe!
 * @warning tr() returns pointer to static buffer - reused on each call!
 */
class I18nComponent : public Component {
 public:
  /**
   * @brief Initialize the I18N component
   * 
   * Sets up translation buffer and default locale
   */
  void setup() override;

  /**
   * @brief Dump component configuration to logs
   */
  void dump_config() override;

  /**
   * @brief Get current active locale
   * 
   * @return Current locale code (e.g., "en", "ru", "de")
   */
  std::string get_current_locale() const { return this->current_locale_; }

  /**
   * @brief Set current active locale
   * 
   * Changes the language for all subsequent translations.
   * Updates internal state and triggers locale change in translation engine.
   * 
   * @param locale Locale code to activate (e.g., "en", "ru", "de")
   * 
   * @note This does NOT automatically update existing UI elements!
   *       You must manually refresh labels/widgets after locale change.
   * 
   * Example:
   * @code
   * id(i18n).set_current_locale("ru");
   * id(my_label).update();  // Refresh label with new locale
   * @endcode
   */
  void set_current_locale(const std::string &locale);

  /**
   * @brief Translate key using current locale (SAFE - returns copy)
   * 
   * This method creates a std::string copy of the translation.
   * Safe to store in variables or use multiple times.
   * 
   * @param key Translation key (e.g., "weather.sunny")
   * @return std::string Copy of translated text
   * 
   * @note Slower than tr() due to memory allocation
   * @note Safe for storing in variables
   * 
   * Example:
   * @code
   * std::string text1 = id(i18n).translate("hello");  // "Hello"
   * std::string text2 = id(i18n).translate("world");  // "World"
   * // Both strings are independent and valid
   * @endcode
   */
  std::string translate(const std::string &key);

  /**
   * @brief Translate key using explicit locale (SAFE - returns copy)
   * 
   * Translates using specified locale without changing current locale.
   * Creates a std::string copy of the translation.
   * 
   * @param key Translation key (e.g., "weather.sunny")
   * @param locale Explicit locale to use (e.g., "en", "ru")
   * @return std::string Copy of translated text
   * 
   * @note Does NOT change current locale
   * @note Uses temporary buffer internally
   * 
   * Example:
   * @code
   * // Current locale is "en"
   * std::string en_text = id(i18n).translate("hello", "en");  // "Hello"
   * std::string ru_text = id(i18n).translate("hello", "ru");  // "Привет"
   * // Current locale is still "en"
   * @endcode
   */
  std::string translate(const std::string &key, const std::string &locale);

  /**
   * @brief Fast translation using current locale (UNSAFE - returns pointer to static buffer!)
   * 
   * ⚠️ WARNING: Returns pointer to STATIC BUFFER that is REUSED on each call!
   * 
   * This is the fastest translation method (zero allocations), but the returned
   * pointer becomes INVALID after the next tr() call.
   * 
   * @param key Translation key (e.g., "weather.sunny")
   * @return const char* Pointer to static buffer (VALID UNTIL NEXT tr() CALL!)
   * 
   * @warning DO NOT store the returned pointer!
   * @warning DO NOT call tr() again before using the result!
   * @warning NOT thread-safe!
   * 
   * ✅ SAFE usage:
   * @code
   * // Use immediately
   * lv_label_set_text(label, id(i18n).tr("hello"));
   * 
   * // Build string incrementally
   * std::string result;
   * result += id(i18n).tr("part1");
   * result += "\n";
   * result += id(i18n).tr("part2");  // Previous tr() result is now invalid
   * @endcode
   * 
   * ❌ UNSAFE usage:
   * @code
   * // DO NOT DO THIS!
   * const char* text1 = id(i18n).tr("hello");  // "Hello"
   * const char* text2 = id(i18n).tr("world");  // "World" (overwrites buffer!)
   * // text1 now also points to "World"!
   * @endcode
   * 
   * @note For safe usage, use translate() instead
   * @note Buffer size configured via buffer_size in YAML (default: 256 bytes)
   * @note Buffer location depends on use_psram setting (RAM or PSRAM)
   */
  const char *tr(const char *key) { return esphome::i18n::tr(key); }

  /**
   * @brief Get translation buffer size in bytes
   * 
   * @return Size of internal translation buffer
   */
  size_t get_buffer_size() const { return esphome::i18n::I18N_BUFFER_SIZE; }

  /**
   * @brief Check if PSRAM mode is enabled
   * 
   * @return true if translation buffer is allocated in PSRAM
   * @return false if translation buffer is in standard RAM
   */
  bool is_psram_enabled() const { return esphome::i18n::I18N_USE_PSRAM; }

  /**
   * @brief Get total number of translation keys
   * 
   * @return Number of unique translation keys across all locales
   */
  size_t get_key_count() const { return esphome::i18n::I18N_KEY_COUNT; }

 protected:
  std::string current_locale_;  ///< Currently active locale code
};

/**
 * @brief Global I18N component instance
 * 
 * Used by automation actions to access the component.
 * Set during setup() in i18n.cpp.
 */
extern I18nComponent *global_i18n_component;

/**
 * @brief Automation action to set locale
 * 
 * Usage in YAML:
 * @code
 * button:
 *   - platform: template
 *     name: "Switch to Russian"
 *     on_press:
 *       - i18n.set_locale:
 *           id: i18n_translations
 *           locale: "ru"
 * @endcode
 * 
 * @tparam Ts Template arguments from automation
 */
template<typename... Ts> class SetLocaleAction : public Action<Ts...> {
 public:
  /**
   * @brief Construct SetLocaleAction
   * 
   * @param parent Pointer to I18nComponent instance
   */
  explicit SetLocaleAction(I18nComponent *parent) : parent_(parent) {}

  /**
   * @brief Set locale template (can be static string or lambda)
   * 
   * @param locale Template for locale code
   */
  TEMPLATABLE_VALUE(std::string, locale)

  /**
   * @brief Execute the action
   * 
   * @param x Automation trigger arguments
   */
  void play(Ts... x) override {
    auto locale = this->locale_.value(x...);
    if (this->parent_ != nullptr) {
      this->parent_->set_current_locale(locale);
    }
  }

 protected:
  I18nComponent *parent_;  ///< Parent component instance
};

}  // namespace i18n
}  // namespace esphome

