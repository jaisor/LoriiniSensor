#include "display/pages/HelpPage.h"

#include "Configuration.h"
#ifdef SCREEN

#include "Device.h"
#include "display/UI.h"

#define HELP_MAX_ROWS 4
#ifndef WIFI_FALLBACK_PASS
  #define WIFI_FALLBACK_PASS "-"  // Built without WIFI
#endif

struct help_row_t {
  const char *key;    // Button or gesture
  const char *what;   // What it does
};

struct help_section_t {
  const char *title;
  help_row_t rows[HELP_MAX_ROWS];
};

// 7-bit ASCII only; each line fits ~180px in its font (key: UI_FONT_LABEL, what: UI_FONT_SMALL)
static const help_section_t SECTIONS[] = {
  { "Pages", {
    { "PWR",        "next page" },
    { "2x PWR",     "previous page" },
    { "Swipe",      "next / previous page" },
    { "BOOT, 2x tap", "open the page" },
  }},
  { "Inside a page", {
    { "PWR",        "next item, or more" },
    { "2x PWR",     "previous item, or less" },
    { "BOOT",       "select, confirm" },
    { "Hold BOOT",  "back (or hold screen)" },
  }},
  { "Sensors", {
    { "Plug in",    "own page in a few s" },
    { "Power page", "PWR next channel" },
    { "Climate page", "PWR next graph" },
    { "Unplugged",  "page goes after 1 min" },
  }},
  { "Settings", {
    { "WiFi",       "BOOT forgets network" },
    { "Sensors",    "what was found" },
    { "Brightness, Sound", "PWR +   2x PWR -" },
    { "Web page",   "shunt resistors" },
  }},
  { "Screen saver", {
    { "Idle half the time", "the screen dims" },
    { "Idle all of it",  "the screen goes dark" },
    { "Button or touch", "only wakes it up" },
    { "Set the time",    "Settings, web page" },
  }},
  { "Power", {
    { "Hold PWR 3s", "switch off (on battery)" },
    { "RST",         "restart" },
    { "BOOT + RST",  "USB flashing mode" },
    { "3 quick boots", "factory reset" },
  }},
  { "Web setup", {
    { "Setup mode",  "join LoriiniSensor_..." },
    { "Password",    WIFI_FALLBACK_PASS },
    { "Then open",   "http://192.168.4.1" },
    { "Connected",   "IP in Settings > WiFi" },
  }},
};
static const uint8_t SECTION_COUNT = sizeof(SECTIONS) / sizeof(SECTIONS[0]);

void CHelpPage::drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  const int16_t cx = x + w / 2, cy = y + 66;
  gfx.fillCircle(cx, cy, 32, UI_COLOR_YELLOW);
  UI_drawCentered(gfx, "?", cy + 13, UI_FONT_LARGE, UI_COLOR_PANEL, x, w);
  UI_drawCentered(gfx, getTitle(), y + 134, UI_FONT_TITLE, UI_COLOR_TEXT, x, w);
  UI_drawCentered(gfx, "Buttons and gestures", y + 168, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
}

void CHelpPage::drawContent(Arduino_GFX &gfx) {
  const help_section_t &s = SECTIONS[section];
  UI_drawCentered(gfx, s.title, UI_BODY_TOP + 30, UI_FONT_TITLE, UI_COLOR_YELLOW);

  const int16_t left = 30;
  for (uint8_t i = 0; i < HELP_MAX_ROWS && s.rows[i].key; i++) {
    int16_t y = UI_BODY_TOP + 58 + i * 40;
    gfx.setFont(UI_FONT_LABEL);
    gfx.setTextColor(UI_COLOR_TEXT);
    gfx.setCursor(left, y);
    gfx.print(s.rows[i].key);
    gfx.setFont(UI_FONT_SMALL);
    gfx.setTextColor(UI_COLOR_MUTED);
    gfx.setCursor(left, y + 18);
    gfx.print(s.rows[i].what);
  }

  UI_drawPageDots(gfx, UI_BODY_BOTTOM - 8, SECTION_COUNT, section);
}

PageAction CHelpPage::onButton(ButtonId id, ButtonEvent event) {
  if (id == ButtonId::POWER && event == ButtonEvent::CLICK) {
    section = (section + 1) % SECTION_COUNT;
    return PageAction::REDRAW;
  }
  if (id == ButtonId::POWER && event == ButtonEvent::DOUBLE_CLICK) {
    section = (section + SECTION_COUNT - 1) % SECTION_COUNT;
    return PageAction::REDRAW;
  }
  if (id == ButtonId::BOOT && (event == ButtonEvent::CLICK || event == ButtonEvent::LONG_PRESS)) {
    return PageAction::EXIT;
  }
  return PageAction::NONE;
}

#endif
