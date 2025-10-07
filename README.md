
# 🌍 **Internationalization (i18n) component for ESPHome**

<div align="center">

![ESPHome](https://img.shields.io/badge/ESPHome-2025.9.0-00BFFF?style=for-the-badge&logo=esphome&logoColor=white)
![LVGL](https://img.shields.io/badge/LVGL-v8.4-red?style=for-the-badge)
![Version](https://img.shields.io/badge/Version-1.0.0-green?style=for-the-badge)

</div>

---

### **Why This Component?**

- 🚀 **Zero Runtime Overhead** - Translations stored in PROGMEM (Flash)
- 🔄 **Dynamic Locale Switching** - Change language on-the-fly
- 📦 **Compact** - Efficient memory usage for embedded devices
- 🎨 **LVGL Integration** - Perfect for touchscreen displays
- 🛠️ **Easy Setup** - Simple YAML configuration


## ✨ **Features**

| Feature | Description |
|---------|-------------|
| **🌍 Multi-Language Support** | Add locales (en, ru, de, fr, etc.) |
| **💾 PROGMEM Storage** | Translations stored in Flash, not RAM |
| **🔄 Runtime Switching** | Change locale without reboot |
| **📝 YAML-Based** | Simple, readable translation files |
| **🎯 Nested Keys** | Organize translations hierarchically |
| **🔌 LVGL Integration** | Update labels automatically |
| **⚡ Fast Lookups** | Optimized translation retrieval |
| **🛡️ Fallback Support** | Returns key if translation missing |


---

## 📋 **Requirements**

- **ESPHome** >= 2025.9.0
- **LVGL Component** (for UI integration)
- **Python 3.8+** (for build process)
- **PyYAML** (automatically installed)

---

## 📥 **Installation**

1. **Add to your ESPHome YAML configuration:**

```yaml
external_components:
  - source: github://alaltitov/esphome
    components: [i18n]
```

2. **Create Translation Files (for example):**

translations/en.yaml:

```yaml
hello: "Hello"
goodbye: "Goodbye"

weather:
  sunny: "Sunny"
  cloudy: "Cloudy"
  rainy: "Rainy"
```

translations/ru.yaml:

```yaml
hello: "Привет"
goodbye: "Пока"

weather:
  sunny: "Солнечно"
  cloudy: "Облачно"
  rainy: "Дождливо"
```

3. **Add to your ESPHome YAML:**

```yaml
i18n:
  id: i18n_translations
  sources:
    - translations/en.yaml
    - translations/ru.yaml
    # - translations/de.yaml
    # - translations/fr.yaml
    # - translations/es.yaml
  default_locale: en

```

4. **Use in LVGL:**

```yaml
lvgl:
  pages:
    - id: climate_page
      bg_color: color_slate_blue_gray
      widgets:
        - label:
            id: hello_lbl
            text: "TEST"

        - obj:
            align: center
            bg_color: color_blue
            width: 150
            height: 80
            widgets:
              - label:
                  id: lang_btn_label
                  align: center
                  text_color: color_white
                  text: "EN"
            on_press:
              then:
                - lambda: |-
                    std::string current = id(i18n_translations).get_current_locale();
                    if (current == "ru") {
                      id(i18n_translations).set_current_locale("en");
                    } else {
                      id(i18n_translations).set_current_locale("ru");
                    }
                    
                    ESP_LOGI("main", "Locale switched to: %s", id(i18n_translations).get_current_locale().c_str());
                    
                - lvgl.label.update:
                    id: hello_lbl
                    text: !lambda |-
                      return id(i18n_translations).translate("weather.cloudy");
                      
                - lvgl.label.update:
                    id: lang_btn_label
                    text: !lambda |-
                      std::string current = id(i18n_translations).get_current_locale();
                      return (current == "ru") ? "EN" : "RU";
```

## ⚙️ Configuration

### Component Configuration

```yaml
i18n:
  # Component ID (required for automations)
  id: i18n_translations
  
  # Translation source files (required)
  sources:
    - translations/en.yaml
    - translations/ru.yaml
    - translations/de.yaml
  
  # Default locale (optional, default: "en")
  default_locale: en
```
### Configuration Options

| Option | Type | Required | Description |
|:-------|:-----|:--------:|:------------|
| `id`| ID | Yes | Component identifier |
| `sources` | List | Yes | List of YAML translation files |
| `default_locale` | String | No | Default locale on boot|


## 📚 API

### Change locale:

1. **Method 1: Via lambda**

```yaml
button:
  - platform: template
    name: "Switch to Russian"
    on_press:
      - lambda: |-
          id(i18n_translations).set_current_locale("ru");

```

2. **Method 2: Via YAML action**

```yaml
button:
  - platform: template
    name: "Switch to English"
    on_press:
      - i18n.set_locale:
          id: i18n_translations
          locale: "en"

```

### Get Current Locale

```yaml
lambda: |-
  ESP_LOGI("main", "Current locale: %s", id(i18n_translations).get_current_locale().c_str());
```

### Get Translation

1. **Simple translation:**

```yaml
- lvgl.label.update:
    id: some_id
    text: !lambda |-
      return id(i18n_translations).translate("weather.cloudy");
```

2. **With dynamic key:**

```yaml
- lvgl.label.update:
    id: some_id
    text: !lambda |-
      return id(i18n_translations).translate("weather." + id(weather_state_sensor_some_id).state));
```

3. **With value:**

```cpp
ota:
  - platform: esphome
    password: !secret display_ota
    on_progress: 
      - lvgl.label.update:
          id: ota_label
          text: !lambda |-
            static char buffer[64];
            snprintf(buffer, sizeof(buffer), id(i18n_translations).translate("ota.progress").c_str(), x); 
            // where "x" - value progress from OTA
            return buffer;
```

4. **List translation:**

LVGL ESPHome `roller` object example

```cpp
      - lambda: |-
          lv_obj_t *roller_obj = id(backlight_settings_sleep_time_roller).obj;
          if (roller_obj == nullptr) {
            return;
          }

          uint16_t old_selection = lv_roller_get_selected(roller_obj);

          std::string options = 
            id(i18n_translations).translate("sleep_time.never") + "\n" +
            id(i18n_translations).translate("sleep_time.minute1") + "\n" +
            id(i18n_translations).translate("sleep_time.minute5") + "\n" +
            id(i18n_translations).translate("sleep_time.minute10") + "\n" +
            id(i18n_translations).translate("sleep_time.minute30") + "\n" +
            id(i18n_translations).translate("sleep_time.hour1") + "\n" +
            id(i18n_translations).translate("sleep_time.hour6") + "\n" +
            id(i18n_translations).translate("sleep_time.hour12");

          lv_roller_set_options(roller_obj, options.c_str(), LV_ROLLER_MODE_NORMAL);
          lv_roller_set_selected(roller_obj, old_selection, LV_ANIM_OFF);
```

## 📋 API Methods Summary

| Method | Description | Returns |
|--------|-------------|---------|
| `translate(key)` | Translate key using **current** locale | `std::string` |
| `set_current_locale(locale)` | Change current language | `void` |
| `get_current_locale()` | Get current language code | `std::string` |


## 💝 Support the Project
Made with ❤️ for the ESPHome community

This project was made in my free time and if it was useful to you, you can support me if you find it necessary 😊:

**ETH/USDT (ERC-20):** `0x9fF0E16a58229bEcdFDf47d9759f20bE77356994`

Or just put ⭐ Thank you