
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
  id: my_i18n
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
                  align: center
                  text_color: color_white
                  text: "Change lang"
            on_press:
              then:
                - lambda: |-
                    const char* cur = esphome::lvgl_i18n::get_locale();
                    const char* next = (cur && std::string(cur) == "ru") ? "en" : "ru";
                    esphome::lvgl_i18n::set_locale(next);
                    lv_label_set_text(id(hello_lbl), esphome::lvgl_i18n::tr("weather.cloudy"));
```

## ⚙️ Configuration

### Component Configuration

```yaml
i18n:
  # Component ID (required for automations)
  id: my_i18n
  
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


