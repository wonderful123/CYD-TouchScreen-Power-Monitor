#pragma once
// theme.h - colour and geometry constants used by the C++ views (plain integers, no LVGL types).
#include <cstdint>

namespace pwr::ui {

// ---- palette
constexpr uint32_t COL_TEXT = 0xD3E6EB;
constexpr uint32_t COL_DIM = 0x566E7A;
constexpr uint32_t COL_RULE = 0x15202A;
constexpr uint32_t COL_ACCENT = 0x46D3B5;
constexpr uint32_t COL_WARN = 0xE8B04A;
constexpr uint32_t COL_CRIT = 0xFF5C5C;
constexpr uint32_t COL_SCROLLBAR = 0x2A3C47;

// ---- scope plot area (px)
constexpr int SCOPE_X = 44;
constexpr int SCOPE_W = 424;
constexpr int SCOPE_H = 176;

// ---- overview sparkline area (px)
constexpr int SPARK_W = 138;
constexpr int SPARK_H = 44;

}  // namespace pwr::ui