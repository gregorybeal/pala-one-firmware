#include "src/ui/reader_menu.h"

#include "src/config.h"           // MARGIN_X, SCREEN_W, SCREEN_H
#include "src/hal/display.h"      // u8g2, display
#include "src/pure/paths.h"       // lastPathComponent, stripTxtExt
#include "src/storage/kosync_settings.h"   // Kosync::enabled
#include "src/ui/font.h"
#include "src/ui/reader.h"        // g_bookview
#include "src/ui/screens/reader_screen.h"  // g_readerScreen — where to hang nextScreen
#include "src/ui/screens/sync_screen.h"    // g_syncScreen
#include "src/ui/statusbar.h"
#include "src/ui/widgets.h"       // prepareMenuFrame, drawSectionHeader

namespace ReaderMenu {

static bool s_active = false;
static int  s_focus  = 0;

// Rows, in draw order. Sync only appears once the user has configured it in
// the web UI, so a device that never uses sync keeps the original one-row
// menu (and with it the original single-click-cycles-statusbar feel).
enum Row { ROW_STATUSBAR = 0, ROW_SYNC = 1 };

static int rowCount() {
  return Kosync::enabled() ? 2 : 1;
}

static const char* statusbarLabel() {
  switch (Statusbar::mode()) {
    case Statusbar::Minimal: return "Minimal";
    case Statusbar::Hidden:  return "Hidden";
    case Statusbar::Full:
    default:                 return "Full";
  }
}

bool isActive() { return s_active; }

void open() {
  s_active = true;
  s_focus  = 0;
  // First paint of the menu sits over whatever the reader last drew. Use a
  // full refresh so reader text doesn't ghost through the partial-refresh
  // overlay; subsequent in-menu redraws (statusbar cycles) go through
  // prepareMenuFrame's normal fastmode path for snappy feedback.
  forceNextMenuFrameFull();
  draw();
}

void close() {
  s_active = false;
}

void draw() {
  prepareMenuFrame();
  Font::useBody();
  int ascent = u8g2.getFontAscent();
  int lineH = (ascent - u8g2.getFontDescent()) + Font::currentLineGap() + 1;
  int y = drawSectionHeader("Reading");

  // Book title (truncated by font; the screen clip handles overflow).
  String title = stripTxtExt(lastPathComponent(g_bookview.book.path()));
  Font::useBold();
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(title.c_str());
  y += lineH;
  Font::useBody();

  size_t total = g_bookview.book.size();
  if (total == 0) total = 1;
  uint32_t pos = g_bookview.pages.offsets[g_bookview.cursor.pageIndex];
  int pct = (int)((pos * 100UL) / (uint32_t)total);

  // pages.count grows lazily as the reader walks the book; only show it as
  // a total when pagination has actually reached EOF. Otherwise the "of Y"
  // reads as a hard cap rather than "this is how many pages exist."
  char buf[64];
  if (g_bookview.pages.eofReached && g_bookview.pages.count > 0) {
    snprintf(buf, sizeof(buf), "Page %d of %d  (%d%%)",
             g_bookview.cursor.pageIndex + 1, g_bookview.pages.count, pct);
  } else {
    snprintf(buf, sizeof(buf), "Page %d  (%d%% of book)",
             g_bookview.cursor.pageIndex + 1, pct);
  }
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(buf);
  y += lineH;

  // Blank gap so the settings section reads as separate from progress info.
  y += lineH;

  const int rows = rowCount();
  if (s_focus >= rows) s_focus = 0;

  // Statusbar mode row.
  snprintf(buf, sizeof(buf), "Statusbar: %s", statusbarLabel());
  if (rows > 1 && s_focus == ROW_STATUSBAR) Font::useBold(); else Font::useBody();
  u8g2.setCursor(MARGIN_X, y);
  u8g2.print(buf);
  y += lineH;

  if (rows > 1) {
    if (s_focus == ROW_SYNC) Font::useBold(); else Font::useBody();
    u8g2.setCursor(MARGIN_X, y);
    u8g2.print(D_MENU_READER_SYNC);
  }
  Font::useBody();

  // Footer hint at the very bottom. With one row there is nothing to move
  // between, so the original single-click shortcut still applies.
  u8g2.setCursor(MARGIN_X, SCREEN_H - 2);
  u8g2.print(rows > 1 ? "1x move  2x pick  3x close"
                      : "click: cycle  2x: close");

  display.update();
}

bool onButton(const ButtonEvent& ev) {
  if (!s_active) return false;
  if (!ev.any()) return false;

  const int rows = rowCount();

  // Single-row menu: unchanged from before sync existed — click cycles the
  // statusbar, anything else closes.
  if (rows == 1) {
    if (ev.kind == ButtonEvent::Short) {
      Statusbar::cycleMode();
      draw();
      return true;
    }
    close();
    return true;
  }

  if (ev.kind == ButtonEvent::Short) {
    s_focus = (s_focus + 1) % rows;
    draw();
    return true;
  }

  if (ev.kind == ButtonEvent::Double) {
    if (s_focus == ROW_STATUSBAR) {
      Statusbar::cycleMode();
      draw();
      return true;
    }
    // Sync: hand off to the sync screen, which owns the Wi-Fi session and
    // comes back to the reader when it is done. The book stays open.
    close();
    g_readerScreen.nextScreen = &g_syncScreen;
    return true;
  }

  // Any other gesture closes — caller redraws the underlying page.
  close();
  return true;
}

}  // namespace ReaderMenu
