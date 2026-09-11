#ifndef PALA_PURE_XPOINTER_H
#define PALA_PURE_XPOINTER_H

#include "arduino_compat.h"

// ============================================================================
//  crengine / KOReader XPointer strings.
//
//  KOReader stores a reading position as an XPointer into crengine's DOM and
//  sends it in the kosync `progress` field, e.g.
//
//      /body/DocFragment[11]/body/div/p[3].0
//      /body/DocFragment[2]/body/p[1]/text().15
//      /body/DocFragment[1]/body/h1/text().0
//
//  Structure: a crengine root `body`, one `DocFragment[N]` per spine
//  document, then the path *inside* that document's own XHTML `<body>`.
//  A trailing `.N` is a character offset into the addressed node and a
//  `text()` step addresses a text node rather than an element.
//
//  This module only parses and composes the string. Turning one into a byte
//  offset needs the per-book spine map (storage/sync_map.h).
//
//  Two things crengine does that we cannot reproduce exactly:
//
//    - It omits `[1]` when a step has no same-named siblings, so `p` and
//      `p[1]` mean the same thing. Parsing normalizes the absent index to 1
//      and `canonicalPath` always writes it back explicitly, so the map's
//      stored paths and a parsed pointer compare on equal terms.
//    - Its DOM is not the browser's DOM. crengine autoboxes stray inline
//      content and drops elements the flattener keeps (and vice versa), so a
//      pointer's deepest steps may not exist in our map. Callers are expected
//      to retry against successively shallower prefixes — see
//      SyncMap::offsetForXPointer, which does exactly that.
// ============================================================================

static const int XPOINTER_MAX_STEPS = 16;

// One `name` or `name[ordinal]` step. `name` is lowercased on parse.
struct XPointerStep {
  char     name[24] = {0};
  uint16_t ordinal  = 1;   // 1-based; an absent [n] parses as 1
};

struct XPointer {
  bool     valid    = false;
  bool     hasFragment = false;  // false for single-document books (plain .txt)
  uint16_t fragment = 0;         // N from DocFragment[N], 1-based
  uint8_t  stepCount = 0;        // steps after DocFragment[N], starting at `body`
  XPointerStep steps[XPOINTER_MAX_STEPS];
  uint32_t textOffset = 0;       // the trailing `.N`, 0 when absent

  const XPointerStep& step(uint8_t i) const { return steps[i]; }
};

// Parse `s`. Returns false (and leaves `out.valid` false) for anything that
// isn't a usable pointer: empty, no steps, a bare number (KOReader sends a
// plain percentage string in `progress` for paged documents, and so does
// this firmware's own older push path).
bool parseXPointer(const String& s, XPointer& out);

// Canonical `/name[ord]/name[ord]` rendering of steps [0, count). Always
// writes ordinals explicitly. `count == 0` yields an empty string.
String canonicalPath(const XPointer& xp, uint8_t count);

// Compose a pointer for the map's stored form. `parentPath` is a canonical
// path as produced by canonicalPath (or stored by the browser-side builder),
// `leafName`/`leafOrdinal` address the block within it.
String buildXPointer(uint16_t fragment, const String& parentPath,
                     const String& leafName, uint16_t leafOrdinal,
                     uint32_t textOffset);

#endif  // PALA_PURE_XPOINTER_H
