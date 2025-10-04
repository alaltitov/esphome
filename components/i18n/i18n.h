#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "generated/translations.h"

#ifdef USE_I18N

namespace esphome {
namespace i18n {

/**
 * @brief I18N Component for ESPHome
 * 
 * Provides internationalization support with runtime locale switching.
 * Works with LVGL to update UI text when locale changes.
 */
class I18nComponent : public Component {
 public:
  /**
   * @brief Setup component - called once during initialization
   */
  void setup() override;

  /**
   * @brief Dump configuration to logs
   */
  void dump_config() override;

  /**
   * @brief Set current locale
   * @param locale Locale code (e.g., "en", "ru", "de")
   */
  void set_current_locale(const std::string &locale);

  /**
   * @brief Get current locale
   * @return Current locale code
   */
  std::string get_current_locale();

  /**
   * @brief Translate a key to current locale
   * @param key Translation key (e.g., "weather.cloudy")
   * @return Translated string
   */
  std::string translate(const std::string &key);

 protected:
  std::string current_locale_;  ///< Currently active locale
};

/**
 * @brief Action for setting locale in automations
 * 
 * Usage in YAML:
 * @code
 * on_press:
 *   - i18n.set_locale:
 *       locale: "en"
 * @endcode
 */
template<typename... Ts> class SetLocaleAction : public Action<Ts...> {
 public:
  /**
   * @brief Constructor
   * @param parent Pointer to I18nComponent instance
   */
  explicit SetLocaleAction(I18nComponent *parent) : parent_(parent) {}

  /**
   * @brief Set locale template value
   * @param locale Templatable locale string
   */
  void set_locale(TemplatableValue<std::string, Ts...> locale) { this->locale_ = locale; }

  /**
   * @brief Execute action - change locale
   * @param x Template arguments
   */
  void play(Ts... x) override {
    std::string locale_value = this->locale_.value(x...);
    if (this->parent_ != nullptr) {
      this->parent_->set_current_locale(locale_value);
    }
  }

 protected:
  I18nComponent *parent_{nullptr};                    ///< Parent component
  TemplatableValue<std::string, Ts...> locale_;      ///< Locale template
};

/**
 * @brief Global component pointer for easy access from lambdas
 * 
 * Usage:
 * @code
 * lambda: |-
 *   const char* text = esphome::i18n::tr("hello");
 *   esphome::i18n::set_locale("ru");
 * @endcode
 */
extern I18nComponent *global_i18n_component;

}  // namespace i18n
}  // namespace esphome

#endif  // USE_I18N
