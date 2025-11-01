"""
I18N (Internationalization) component for ESPHome with LVGL support.

FEATURES:
- PSRAM support for translation buffers (optional)
- Optimized memory usage
- Runtime locale switching
"""

import logging
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.core import CORE
from esphome.helpers import write_file_if_changed
from esphome.const import CONF_ID

_LOGGER = logging.getLogger(__name__)

# Component metadata
DOMAIN = "i18n"
CODEOWNERS = ["@alaltitov"]
DEPENDENCIES = ["lvgl"]

# C++ namespace and classes
i18n_ns = cg.esphome_ns.namespace("i18n")
I18nComponent = i18n_ns.class_("I18nComponent", cg.Component)
SetLocaleAction = i18n_ns.class_("SetLocaleAction", automation.Action)

# Configuration keys
CONF_USE_PSRAM = "use_psram"
CONF_BUFFER_SIZE = "buffer_size"

# ------------------ Component Schema ------------------

I18N_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(I18nComponent),
        cv.Required("sources"): cv.ensure_list(cv.file_),
        cv.Optional("default_locale", default="en"): cv.string_strict,
        cv.Optional(CONF_USE_PSRAM, default=False): cv.boolean,
        cv.Optional(CONF_BUFFER_SIZE, default=256): cv.int_range(min=64, max=2048),
    }
)

def i18n_config_schema(config):
    """Validate i18n configuration schema."""
    if not config or isinstance(config, dict):
        return I18N_SCHEMA(config)
    raise cv.Invalid("i18n expects a single configuration, not a list")

CONFIG_SCHEMA = i18n_config_schema

# ------------------ Action Schema ------------------

SET_LOCALE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(I18nComponent),
        cv.Required("locale"): cv.templatable(cv.string_strict),
    }
)

@automation.register_action("i18n.set_locale", SetLocaleAction, SET_LOCALE_ACTION_SCHEMA)
async def set_locale_action_to_code(config, action_id, template_args, args):
    """Register set_locale action for automations."""
    parent = await cg.get_variable(config[CONF_ID])
    locale_template = await cg.templatable(config["locale"], args, cg.std_string)
    var = cg.new_Pvariable(action_id, template_args, parent)
    cg.add(var.set_locale(locale_template))
    return var

# ------------------ YAML Locale Utilities ------------------

def _flatten_dict(prefix, obj, out):
    """Recursively flatten nested dictionary into dot-notation keys."""
    if isinstance(obj, dict):
        for k, v in obj.items():
            key = f"{prefix}.{k}" if prefix else str(k)
            _flatten_dict(key, v, out)
    else:
        out[prefix] = str(obj)

def _load_yaml_file(path: Path) -> dict:
    """Load and parse YAML file."""
    import yaml
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f) or {}

def _cpp_escape_literal(s: str) -> str:
    """Escape string for C++ string literal."""
    return s.replace("\\", "\\\\").replace('"', '\\"')

def _sym_from_key(key: str) -> str:
    """Convert translation key to valid C++ symbol name."""
    import re
    s = re.sub(r"[^A-Za-z0-9]+", "_", key).strip("_")
    if not s:
        s = "KEY"
    s = s.upper()
    if s[0].isdigit():
        s = "_" + s
    return s

# ------------------ Generate translations.h ------------------

def _gen_translations_h(all_keys: list[str], use_psram: bool, buffer_size: int) -> str:
    """Generate C++ header file with translation function declarations."""
    psram_note = " (PSRAM enabled)" if use_psram else ""
    return (
        "#pragma once\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        "namespace esphome {\n"
        "namespace i18n {\n\n"
        f"// Translation buffer configuration{psram_note}\n"
        f"static constexpr size_t I18N_BUFFER_SIZE = {buffer_size};\n"
        f"static constexpr bool I18N_USE_PSRAM = {'true' if use_psram else 'false'};\n\n"
        "// Main translation function - returns translated string for given key\n"
        "// NOTE: Returns pointer to static buffer - valid until next tr() call\n"
        "// WARNING: Not thread-safe! Do not store returned pointer.\n"
        "const char* tr(const char* key);\n\n"
        "// Set current locale (e.g., \"en\", \"ru\", \"de\")\n"
        "void set_locale(const char* loc);\n\n"
        "// Get current locale\n"
        "const char* get_locale();\n\n"
        "// Internal functions (do not call directly)\n"
        "void i18n_set_locale_internal(const char* loc);\n"
        "void i18n_get_buf_internal(const char* loc, const char* key, char* buf, size_t n);\n"
        "void i18n_init_buffer();  // Initialize translation buffer\n"
        "void i18n_cleanup_buffer();  // Cleanup translation buffer\n\n"
        "// Default locale constant\n"
        "extern const char TRANSLATIONS_DEFAULT_LOCALE[];\n\n"
        "// Total number of translation keys\n"
        f"static constexpr size_t I18N_KEY_COUNT = {len(all_keys)};\n\n"
        "} // namespace i18n\n"
        "} // namespace esphome\n"
    )

# ------------------ Generate translations.cpp ------------------

def _gen_translations_cpp(locales_map: dict[str, dict[str, str]], default_locale: str, 
                         all_keys: list[str], use_psram: bool, buffer_size: int) -> str:
    """Generate C++ implementation file with translation tables."""
    
    # Validate translations
    missing_report = []
    for loc, kv in locales_map.items():
        missing = [k for k in all_keys if k not in kv]
        if missing:
            missing_report.append(f"{loc}: {', '.join(missing)}")
    if missing_report:
        raise cv.Invalid("Missing translations for keys:\n" + "\n".join(missing_report))

    # Generate string tables for each locale
    block_strings = []
    for loc in sorted(locales_map.keys()):
        upper = loc.upper()
        strings = []
        
        for k in all_keys:
            lit = _cpp_escape_literal(locales_map[loc][k])
            sym = f"STR_{upper}_{_sym_from_key(k)}"
            strings.append(f'static const char {sym}[] PROGMEM = "{lit}";')
        
        table_elems = ",\n  ".join([f'STR_{upper}_{_sym_from_key(k)}' for k in all_keys])
        block = (
            f"// Locale: {loc}\n"
            + "\n".join(strings)
            + f"\nstatic const char* const TABLE_{upper}[] PROGMEM = {{\n  {table_elems}\n}};\n"
        )
        block_strings.append(block)

    blocks_joined = "\n".join(block_strings)
    keys_literals = ",\n  ".join([f'"{_cpp_escape_literal(k)}"' for k in all_keys])

    # Generate locale selection
    case_lines = []
    for loc in sorted(locales_map.keys()):
        case_lines.append(f'  if (strcmp(loc, "{loc}") == 0) return TABLE_{loc.upper()};')
    select_table_cases = "\n".join(case_lines)

    # Build C++ source
    parts = []
    parts.append('#include "generated/translations.h"')
    parts.append("#include <string.h>")
    
    # PSRAM support
    if use_psram:
        parts.append("#ifdef ESP32")
        parts.append("#include <esp_heap_caps.h>")
        parts.append("#define I18N_MALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)")
        parts.append("#define I18N_FREE(ptr) heap_caps_free(ptr)")
        parts.append("#else")
        parts.append("#define I18N_MALLOC(size) malloc(size)")
        parts.append("#define I18N_FREE(ptr) free(ptr)")
        parts.append("#endif")
    else:
        parts.append("#define I18N_MALLOC(size) malloc(size)")
        parts.append("#define I18N_FREE(ptr) free(ptr)")
    
    # PROGMEM compatibility
    parts.append("#ifdef ARDUINO")
    parts.append("#include <pgmspace.h>")
    parts.append("#else")
    parts.append("#define PROGMEM")
    parts.append("#define pgm_read_ptr(addr) (*(addr))")
    parts.append("#define strncpy_P strncpy")
    parts.append("#endif\n")

    parts.append("namespace esphome {")
    parts.append("namespace i18n {\n")

    # State variables
    parts.append(f'const char TRANSLATIONS_DEFAULT_LOCALE[] = "{default_locale}";')
    parts.append("static const char* current_loc = TRANSLATIONS_DEFAULT_LOCALE;")
    parts.append("static char* translation_buffer = nullptr;  // Dynamic buffer\n")

    # Master key list
    parts.append("static const char* const I18N_KEYS[] = {")
    parts.append(f"  {keys_literals}")
    parts.append("};")
    parts.append("static constexpr size_t I18N_KEYS_COUNT = sizeof(I18N_KEYS)/sizeof(I18N_KEYS[0]);\n")

    # Translation tables
    parts.append(blocks_joined)

    # Buffer management
    parts.append("// Initialize translation buffer")
    parts.append("void i18n_init_buffer() {")
    parts.append("  if (translation_buffer == nullptr) {")
    if use_psram:
        parts.append("    translation_buffer = (char*)I18N_MALLOC(I18N_BUFFER_SIZE);")
        parts.append("    if (translation_buffer == nullptr) {")
        parts.append("      // Fallback to regular RAM if PSRAM allocation fails")
        parts.append("      translation_buffer = (char*)malloc(I18N_BUFFER_SIZE);")
        parts.append("    }")
    else:
        parts.append("    translation_buffer = (char*)I18N_MALLOC(I18N_BUFFER_SIZE);")
    parts.append("  }")
    parts.append("}\n")

    parts.append("// Cleanup translation buffer")
    parts.append("void i18n_cleanup_buffer() {")
    parts.append("  if (translation_buffer != nullptr) {")
    parts.append("    I18N_FREE(translation_buffer);")
    parts.append("    translation_buffer = nullptr;")
    parts.append("  }")
    parts.append("}\n")

    # Locale setter
    parts.append("void i18n_set_locale_internal(const char* loc) {")
    parts.append("  if (loc) current_loc = loc;")
    parts.append("}\n")

    # Table selector
    parts.append("static const char* const* select_table(const char* loc) {")
    parts.append(select_table_cases)
    parts.append(f"  return TABLE_{default_locale.upper()};")
    parts.append("}\n")

    # Key finder
    parts.append("static int key_index_of(const char* key) {")
    parts.append("  for (size_t i = 0; i < I18N_KEYS_COUNT; ++i) {")
    parts.append("    if (strcmp(I18N_KEYS[i], key) == 0) return (int)i;")
    parts.append("  }")
    parts.append("  return -1;")
    parts.append("}\n")

    # PROGMEM reader
    parts.append("static const char* get_ptr_from_progmem(const char* const* table, size_t idx) {")
    parts.append("  return (const char*)pgm_read_ptr(&(table[idx]));")
    parts.append("}\n")

    # Internal getter
    parts.append("void i18n_get_buf_internal(const char* loc, const char* key, char* buf, size_t n) {")
    parts.append("  if (!buf || n == 0) return;")
    parts.append("  if (!key) { buf[0] = '\\0'; return; }")
    parts.append("  ")
    parts.append("  int idx = key_index_of(key);")
    parts.append("  if (idx < 0) {")
    parts.append("    strncpy(buf, key, n - 1);")
    parts.append("    buf[n - 1] = '\\0';")
    parts.append("    return;")
    parts.append("  }")
    parts.append("  ")
    parts.append("  auto table = select_table(loc && loc[0] ? loc : TRANSLATIONS_DEFAULT_LOCALE);")
    parts.append("  const char* p = get_ptr_from_progmem(table, (size_t)idx);")
    parts.append("  strncpy_P(buf, p, n - 1);")
    parts.append("  buf[n - 1] = '\\0';")
    parts.append("}\n")

    # Main tr() function
    parts.append("// Main translation function")
    if use_psram:
        parts.append("// OPTIMIZATION: Uses PSRAM buffer to save internal RAM")
    else:
        parts.append("// OPTIMIZATION: Uses heap buffer to avoid stack allocation")
    parts.append("const char* tr(const char* key) {")
    parts.append("  // Lazy initialization of buffer")
    parts.append("  if (translation_buffer == nullptr) {")
    parts.append("    i18n_init_buffer();")
    parts.append("  }")
    parts.append("  ")
    parts.append("  if (translation_buffer == nullptr) {")
    parts.append("    // Allocation failed - return key as fallback")
    parts.append("    return key ? key : \"\";")
    parts.append("  }")
    parts.append("  ")
    parts.append("  i18n_get_buf_internal(current_loc, key, translation_buffer, I18N_BUFFER_SIZE);")
    parts.append("  return translation_buffer;")
    parts.append("}\n")

    # Public API
    parts.append("void set_locale(const char* loc) {")
    parts.append("  i18n_set_locale_internal(loc);")
    parts.append("}\n")

    parts.append("const char* get_locale() {")
    parts.append("  return current_loc;")
    parts.append("}\n")

    parts.append("} // namespace i18n")
    parts.append("} // namespace esphome\n")

    return "\n".join(parts)

# ------------------ Main Code Generator ------------------

async def to_code(config):
    """Main code generation function."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_global(i18n_ns.using)
    cg.add_define("USE_I18N")

    default_locale: str = config["default_locale"]
    use_psram: bool = config[CONF_USE_PSRAM]
    buffer_size: int = config[CONF_BUFFER_SIZE]
    sources = [Path(CORE.relative_config_path(p)) for p in config["sources"]]

    # Log configuration
    if use_psram:
        _LOGGER.info(f"I18N: PSRAM mode enabled, buffer size: {buffer_size} bytes")
    else:
        _LOGGER.info(f"I18N: Standard RAM mode, buffer size: {buffer_size} bytes")

    # Load locale files
    locales_map: dict[str, dict[str, str]] = {}
    all_keys_set = set()

    for src in sources:
        if not src.exists():
            raise cv.Invalid(f"Locale file not found: {src}")
        
        loc = src.stem
        data = _load_yaml_file(src)
        flat = {}
        _flatten_dict("", data, flat)
        
        locales_map[loc] = flat
        all_keys_set.update(flat.keys())

    all_keys = sorted(all_keys_set)

    if default_locale not in locales_map:
        raise cv.Invalid(f"default_locale='{default_locale}' not found in sources")

    # Generate C++ files
    gen_dir = CORE.relative_src_path("generated")
    hdr_path = Path(gen_dir) / "translations.h"
    cpp_path = Path(gen_dir) / "translations.cpp"

    hdr = _gen_translations_h(all_keys, use_psram, buffer_size)
    cpp = _gen_translations_cpp(locales_map, default_locale, all_keys, use_psram, buffer_size)

    write_file_if_changed(hdr_path, hdr)
    write_file_if_changed(cpp_path, cpp)

    # Build flags
    cg.add_build_flag("-Isrc")
    cg.add_build_flag("-std=gnu++17")

