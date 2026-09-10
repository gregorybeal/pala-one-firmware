#ifndef PALA_WEB_EPUB_JS_H
#define PALA_WEB_EPUB_JS_H

#include "src/config.h"   // D_WEB_EPUB_* strings via lang/lang.h

// ============================================================================
//  /epub.js — browser-side EPUB -> plain text converter.
//
//  The firmware deliberately knows nothing about EPUB. This script runs in
//  the visitor's browser, unzips the .epub, flattens its spine to UTF-8 text
//  and posts THAT to the existing /upload route under a .txt name, so the
//  paginator, page cache, bookmarks and byte-offset progress model are all
//  untouched by the feature.
//
//  Everything it needs is a native browser API, which matters because the
//  SoftAP captive portal has no route to the internet — no CDN is reachable:
//    - DecompressionStream('deflate-raw')  for the ZIP members
//    - DOMParser                           for container.xml / OPF / XHTML
//    - TextDecoder, Blob, fetch, FormData
//
//  It also computes the KOReader "partial MD5" of the ORIGINAL uploaded file
//  and posts it to /kosync-doc. That hash has to be taken here, over the
//  bytes as the user has them, because the device rewrites the text as it
//  stores it (normalizeTypography + compactText in web/upload.cpp) — a hash
//  computed on-device would never match the same book opened in KOReader.
//  See src/hal/kosync.cpp for the protocol side.
//
//  It also builds the per-book spine map that makes KOReader sync land on a
//  paragraph rather than a percentage — see pure/sync_map_codec.h for the
//  format and buildSyncMap() below for the writer — and posts it to
//  /kosync-map. Doing it here is not a choice: the map has to describe the
//  text as the DEVICE will store it, so this script reproduces the firmware's
//  normalizeTypography() + compactText() (see normalizeAndCompact) and
//  measures offsets against that.
//
//  Served by web/chrome.cpp as a cacheable resource, like /style.css, so the
//  ~24 KB doesn't ride along on every page render.
//
//  Authoring note: user-facing strings come in through PALA_EPUB_MSG below so
//  they stay translatable. They are emitted into a JS double-quoted object
//  literal, so a translation MUST NOT contain a double quote (") or a
//  backslash (\) — same class of constraint as the D_WEB_CONFIRM_* rule
//  documented in src/lang/lang.h.
// ============================================================================

static const char kEpubJs[] PROGMEM =
  "var PALA_EPUB_MSG={"
    "unsupported:\"" D_WEB_EPUB_UNSUPPORTED    "\","
    "hashing:\""     D_WEB_EPUB_HASHING        "\","
    "reading:\""     D_WEB_EPUB_READING        "\","
    "converting:\""  D_WEB_EPUB_CONVERTING     "\","
    "uploading:\""   D_WEB_EPUB_UPLOADING      "\","
    "notEpub:\""     D_WEB_EPUB_ERR_NOT_EPUB   "\","
    "noRoot:\""      D_WEB_EPUB_ERR_NO_ROOT    "\","
    "noText:\""      D_WEB_EPUB_ERR_NO_TEXT    "\","
    "zip64:\""       D_WEB_EPUB_ERR_ZIP64      "\","
    "method:\""      D_WEB_EPUB_ERR_METHOD     "\","
    "badXml:\""      D_WEB_EPUB_ERR_BAD_XML    "\","
    "failed:\""      D_WEB_EPUB_ERR_UPLOAD     "\""
  "};\n"
  R"PALAJS(
(function(){
'use strict';

var M = PALA_EPUB_MSG;
var form   = document.getElementById('bookform');
var input  = document.getElementById('bookfile');
var statusEl = document.getElementById('upstatus');
var button = form ? form.querySelector('button[type=submit]') : null;
if (!form || !input || !statusEl) return;

/* Without DecompressionStream we cannot inflate ZIP members. Leave the form
   as a plain native POST (plain text still uploads fine) and say so. */
if (typeof DecompressionStream !== 'function') {
  input.setAttribute('accept', '.txt,text/plain');
  say(M.unsupported, true);
  return;
}

function say(msg, bad) {
  statusEl.textContent = msg || '';
  statusEl.className = 'muted' + (bad ? ' upload-error' : '');
}
function busy(on) {
  if (button) button.disabled = on;
  input.disabled = on;
}
function breathe() {
  /* Yield so the e-ink-era status line actually repaints between spine docs. */
  return new Promise(function(r){ setTimeout(r, 0); });
}

/* ---------------------------------------------------------------- MD5 ---
   Needed for the KOReader document id; SubtleCrypto deliberately omits MD5.
   One-shot over at most 12 KB of sampled bytes, so no streaming API. */
var MD5_S = [7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
             5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
             4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
             6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21];
var MD5_K = (function(){
  var k = new Uint32Array(64);
  for (var i = 0; i < 64; i++) k[i] = (Math.abs(Math.sin(i + 1)) * 4294967296) >>> 0;
  return k;
})();

function md5hex(bytes) {
  var n = bytes.length;
  var padded = n + 1;
  padded += ((56 - padded % 64) + 64) % 64;
  var total = padded + 8;
  var m = new Uint8Array(total);
  m.set(bytes);
  m[n] = 0x80;
  var lo = (n << 3) >>> 0;
  var hi = Math.floor(n / 536870912) >>> 0;   /* n * 8 / 2^32 */
  m[padded]   =  lo         & 0xff;
  m[padded+1] = (lo >>>  8) & 0xff;
  m[padded+2] = (lo >>> 16) & 0xff;
  m[padded+3] = (lo >>> 24) & 0xff;
  m[padded+4] =  hi         & 0xff;
  m[padded+5] = (hi >>>  8) & 0xff;
  m[padded+6] = (hi >>> 16) & 0xff;
  m[padded+7] = (hi >>> 24) & 0xff;

  var a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  var W = new Uint32Array(16);
  for (var off = 0; off < total; off += 64) {
    for (var j = 0; j < 16; j++) {
      var p = off + j * 4;
      W[j] = (m[p] | (m[p+1] << 8) | (m[p+2] << 16) | (m[p+3] << 24)) >>> 0;
    }
    var A = a0, B = b0, C = c0, D = d0;
    for (var i = 0; i < 64; i++) {
      var F, g;
      if (i < 16)      { F = (B & C) | (~B & D);        g = i; }
      else if (i < 32) { F = (D & B) | (~D & C);        g = (5 * i + 1) & 15; }
      else if (i < 48) { F = B ^ C ^ D;                 g = (3 * i + 5) & 15; }
      else             { F = C ^ (B | ~D);              g = (7 * i) & 15; }
      F = (F + A + MD5_K[i] + W[g]) >>> 0;
      A = D; D = C; C = B;
      B = (B + ((F << MD5_S[i]) | (F >>> (32 - MD5_S[i])))) >>> 0;
    }
    a0 = (a0 + A) >>> 0; b0 = (b0 + B) >>> 0;
    c0 = (c0 + C) >>> 0; d0 = (d0 + D) >>> 0;
  }
  return le(a0) + le(b0) + le(c0) + le(d0);
}
function le(v) {
  var s = '';
  for (var i = 0; i < 4; i++) {
    var b = (v >>> (i * 8)) & 0xff;
    s += (b < 16 ? '0' : '') + b.toString(16);
  }
  return s;
}

/* KOReader's util.partialMD5: twelve 1 KB samples at 1024 << (2*i), i = -1..10.
   The i = -1 term shifts by -2, which both Lua's bit.lshift and JS mask to 30,
   overflowing 1024 << 30 to exactly 0 — so the first sample is the file head.
   Stop at the first offset at/past EOF, matching the Lua loop's read()==nil. */
function partialMd5(buf) {
  var parts = [], total = 0;
  for (var i = -1; i <= 10; i++) {
    var off = 1024 << (2 * i);
    if (off < 0 || off >= buf.length) break;
    var slice = buf.subarray(off, Math.min(off + 1024, buf.length));
    parts.push(slice);
    total += slice.length;
  }
  var all = new Uint8Array(total), o = 0;
  for (var j = 0; j < parts.length; j++) { all.set(parts[j], o); o += parts[j].length; }
  return md5hex(all);
}

/* ---------------------------------------------------------------- ZIP --- */
function readZip(buf) {
  var dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  var len = buf.length, eocd = -1;
  var min = Math.max(0, len - 65557);          /* max comment + EOCD size */
  for (var i = len - 22; i >= min; i--) {
    if (dv.getUint32(i, true) === 0x06054b50) { eocd = i; break; }
  }
  if (eocd < 0) throw new Error(M.notEpub);
  var count  = dv.getUint16(eocd + 10, true);
  var cdOff  = dv.getUint32(eocd + 16, true);
  if (cdOff === 0xFFFFFFFF || count === 0xFFFF) throw new Error(M.zip64);

  var dec = new TextDecoder('utf-8');
  var map = {}, p = cdOff;
  for (var n = 0; n < count && p + 46 <= len; n++) {
    if (dv.getUint32(p, true) !== 0x02014b50) break;
    var nlen = dv.getUint16(p + 28, true);
    var elen = dv.getUint16(p + 30, true);
    var clen = dv.getUint16(p + 32, true);
    var name = dec.decode(buf.subarray(p + 46, p + 46 + nlen));
    /* Sizes come from the central directory, so entries written with a
       data descriptor (zeroed local header sizes) still resolve. */
    map[name] = {
      method: dv.getUint16(p + 10, true),
      csize:  dv.getUint32(p + 20, true),
      lho:    dv.getUint32(p + 42, true)
    };
    p += 46 + nlen + elen + clen;
  }
  return { dv: dv, buf: buf, map: map };
}

async function zipText(zip, path) {
  var e = zip.map[path] || zip.map['./' + path];
  if (!e) return null;
  var dv = zip.dv;
  if (dv.getUint32(e.lho, true) !== 0x04034b50) return null;
  var start = e.lho + 30
            + dv.getUint16(e.lho + 26, true)     /* file name length */
            + dv.getUint16(e.lho + 28, true);    /* extra field length */
  var raw = zip.buf.subarray(start, start + e.csize);
  var out;
  if (e.method === 0) out = raw;
  else if (e.method === 8) out = await inflateRaw(raw);
  else throw new Error(M.method + ' ' + e.method);
  return new TextDecoder('utf-8').decode(out);
}

async function inflateRaw(u8) {
  var stream = new Blob([u8]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

/* ---------------------------------------------------------------- XML --- */
function parseXml(src) {
  var d = new DOMParser().parseFromString(src, 'application/xml');
  if (d.querySelector('parsererror')) throw new Error(M.badXml);
  return d;
}
/* Namespace-agnostic lookup: EPUBs disagree wildly about prefixes, so match
   on localName rather than trusting dc: / opf: to be bound as expected. */
function allByLocal(root, local) {
  var out = [], all = root.getElementsByTagName('*');
  for (var i = 0; i < all.length; i++) {
    if ((all[i].localName || '').toLowerCase() === local) out.push(all[i]);
  }
  return out;
}
function textByLocal(root, local) {
  var els = allByLocal(root, local);
  return els.length ? (els[0].textContent || '').trim() : '';
}

function resolvePath(baseDir, href) {
  var rel = href.split('#')[0];
  try { rel = decodeURIComponent(rel); } catch (e) { /* leave as-is */ }
  if (rel.charAt(0) === '/') return rel.replace(/^\/+/, '');
  var parts = baseDir ? baseDir.split('/') : [];
  var segs = rel.split('/');
  for (var i = 0; i < segs.length; i++) {
    var s = segs[i];
    if (s === '' || s === '.') continue;
    if (s === '..') parts.pop();
    else parts.push(s);
  }
  return parts.join('/');
}

/* ------------------------------------------------------- XHTML -> text --- */
/* Elements crengine drops from its DOM entirely — not counted when numbering
   same-named siblings, because KOReader's XPointers never see them. */
var DROP = { script:1, style:1, head:1, link:1, meta:1, title:1 };
/* Present in the DOM (so they DO count towards sibling ordinals) but
   contribute no text. */
var VOIDISH = { svg:1, img:1, image:1, audio:1, video:1, object:1, iframe:1 };
var BLOCK = { p:1, div:1, li:1, h1:1, h2:1, h3:1, h4:1, h5:1, h6:1,
              blockquote:1, td:1, th:1, tr:1, section:1, article:1, aside:1,
              header:1, footer:1, pre:1, figcaption:1, figure:1, dt:1, dd:1,
              ul:1, ol:1, dl:1, table:1, hr:1, nav:1, body:1 };

/* Ceiling on recorded blocks per book. Past this the map keeps whatever it
   has and the rest of the book resolves at spine-document granularity —
   still far better than a whole-book percentage. Mirrors
   SYNC_MAP_MAX_BLOCKS in pure/sync_map_codec.h. */
var MAX_BLOCKS    = 200000;
var MAX_POOL      = 2048;
var MAX_FRAGMENTS = 4096;

function isToc(el) {
  var t = el.getAttribute('epub:type')
       || (el.getAttributeNS ? el.getAttributeNS('http://www.idpf.org/2007/ops', 'type') : null)
       || '';
  return /\b(toc|landmarks|page-list)\b/.test(t);
}

/* Walks one spine document, appending text to `out` and recording where each
   block element's text starts. `st.len` is the running character length of
   what has been pushed, which is what a block's `c` is measured in until
   normalizeAndCompact() rewrites it to a byte offset.

   `path` is the canonical parent path of this node's children, written the
   way pure/xpointer.h renders one: every step carries an explicit ordinal,
   so `/body[1]/div[2]` compares equal to a crengine pointer's `/body/div[2]`
   once that has been normalized on the device. */
function walk(node, out, st, path, blocks) {
  var counts = {};
  for (var n = node.firstChild; n; n = n.nextSibling) {
    if (n.nodeType === 3) {                       /* text */
      var t = n.nodeValue.replace(/\s+/g, ' ');
      out.push(t); st.len += t.length;
      continue;
    }
    if (n.nodeType !== 1) continue;               /* skip comments, PIs */
    var name = (n.localName || '').toLowerCase();
    if (DROP[name]) continue;

    /* Ordinal is counted over everything crengine keeps, text-bearing or
       not, because that is what its `[n]` indices count. */
    counts[name] = (counts[name] || 0) + 1;
    var ord = counts[name];

    if (VOIDISH[name]) continue;
    if (name === 'nav' && isToc(n)) continue;
    if (name === 'br') { out.push('\n'); st.len += 1; continue; }

    var heading = /^h[1-6]$/.test(name);
    var block = heading || !!BLOCK[name];
    var sep = heading ? '\n\n' : '\n';

    if (block) { out.push(sep); st.len += sep.length; }
    if (block && blocks.length < MAX_BLOCKS) {
      blocks.push({ c: st.len, p: path, n: name, o: ord > 65535 ? 65535 : ord });
    }

    walk(n, out, st, path + '/' + name + '[' + ord + ']', blocks);

    if (block) { out.push(sep); st.len += sep.length; }
  }
}

/* --- normalization -------------------------------------------------------
   Mirrors normalizeTypography() + compactText() from pure/text_util.cpp so
   the text measured here is byte-for-byte the text the device will store,
   and the offsets recorded against it therefore address the stored file.

   Both firmware routines are idempotent, so the device re-running them on
   already-normalized input is the identity. Any drift between the two
   implementations shows up as a textSize mismatch, which makes SyncMap
   reject the map and fall back to percentages — wrong-but-safe rather than
   a confident seek to the wrong paragraph. */
var SMART = {
  0x00a0: ' ',  0x00ab: '"', 0x00bb: '"', 0x0091: "'", 0x0092: "'",
  0x2018: "'",  0x2019: "'", 0x201a: "'", 0x201b: "'",
  0x201c: '"',  0x201d: '"', 0x201e: '"', 0x201f: '"',
  0x2039: '"',  0x203a: '"',
  0x2013: '-',  0x2014: '-', 0x2015: '-',
  0x2026: '...'
};

function normalizeAndCompact(raw, blocks) {
  var out = '', bytes = 0, mi = 0;
  var lastWasSpace = false, newlineCount = 0, pendingSpace = false;

  function emit(str) {
    out += str;
    for (var k = 0; k < str.length; k++) {
      var cc = str.charCodeAt(k);
      if (cc < 0x80) bytes += 1;
      else if (cc < 0x800) bytes += 2;
      else if (cc >= 0xd800 && cc <= 0xdbff) { bytes += 4; k++; }
      else bytes += 3;
    }
  }

  for (var i = 0; i < raw.length; i++) {
    /* Block marks are ascending, so one cursor over them suffices. `<=`
       rather than `===` so a mark that lands on the low half of a surrogate
       pair cannot stall the cursor and shift every mark after it. */
    while (mi < blocks.length && blocks[mi].c <= i) { blocks[mi].c = bytes; mi++; }

    var code = raw.charCodeAt(i);
    if (i === 0 && code === 0xfeff) continue;          /* BOM */

    var ch = SMART[code];
    if (ch === undefined) {
      ch = raw.charAt(i);
      /* An astral character is two code units. Take them together: emit()
         measures a pair as one 4-byte sequence, and handing it the halves
         separately would charge 4 bytes for the first and 3 for the second
         while appending a perfectly good character. */
      if (code >= 0xd800 && code <= 0xdbff && i + 1 < raw.length) {
        ch = raw.substr(i, 2);
        i++;
      }
    }

    if (ch === '\r') continue;
    if (ch === '\t') ch = ' ';

    if (ch === '\n') {
      /* Deferring the space is how the firmware's trailing-space strip
         before a newline is reproduced without rewinding the output. */
      pendingSpace = false;
      newlineCount++;
      if (newlineCount <= 2) emit('\n');
      lastWasSpace = false;
      continue;
    }

    if (ch === ' ') {
      /* A space run between two newlines must not break the newline run:
         the firmware strips those spaces and would then see one run where
         we saw two, collapse it to 2, and store fewer bytes than the map
         was built for. Spaces at the start of a line are dropped outright,
         matching what the flattener's old regex pass did. */
      if (!lastWasSpace) {
        lastWasSpace = true;
        if (newlineCount === 0) pendingSpace = true;
      }
      continue;
    }

    newlineCount = 0;
    lastWasSpace = false;
    if (pendingSpace) { emit(' '); pendingSpace = false; }
    emit(ch);
  }
  while (mi < blocks.length) { blocks[mi].c = bytes; mi++; }

  /* compactText(trimTail) strips trailing whitespace; the flattener's own
     leading trim is preserved here so stored text is unchanged from before
     this feature. Both runs are ASCII, so the byte adjustment is exact. */
  var trimmed = out.replace(/^[ \n]+/, '');
  if (trimmed.length !== out.length) {
    var lead = out.length - trimmed.length;
    bytes -= lead;
    for (var a = 0; a < blocks.length; a++) {
      blocks[a].c = (blocks[a].c > lead) ? (blocks[a].c - lead) : 0;
    }
    out = trimmed;
  }
  trimmed = out.replace(/[ \n]+$/, '');
  if (trimmed.length !== out.length) {
    bytes -= (out.length - trimmed.length);
    out = trimmed;
  }
  for (var b = 0; b < blocks.length; b++) {
    if (blocks[b].c > bytes) blocks[b].c = bytes;
  }

  return { text: out, bytes: bytes, blocks: blocks };
}

function docToDoc(src) {
  var dp = new DOMParser();
  var d = dp.parseFromString(src, 'application/xhtml+xml');
  /* Plenty of shipped EPUBs are not actually well-formed XML. */
  if (d.querySelector('parsererror')) d = dp.parseFromString(src, 'text/html');
  var body = d.body || allByLocal(d, 'body')[0] || d.documentElement;
  if (!body) return { text: '', bytes: 0, blocks: [] };

  var out = [], blocks = [], st = { len: 0 };
  walk(body, out, st, '/body[1]', blocks);
  return normalizeAndCompact(out.join(''), blocks);
}

/* --------------------------------------------------------------- EPUB --- */
async function epubToText(buf) {
  var zip = readZip(buf);

  var containerSrc = await zipText(zip, 'META-INF/container.xml');
  if (containerSrc === null) throw new Error(M.notEpub);
  var rootfile = allByLocal(parseXml(containerSrc), 'rootfile')[0];
  var opfPath = rootfile && rootfile.getAttribute('full-path');
  if (!opfPath) throw new Error(M.noRoot);

  var slash = opfPath.lastIndexOf('/');
  var opfDir = slash >= 0 ? opfPath.slice(0, slash) : '';
  var opfSrc = await zipText(zip, opfPath);
  if (opfSrc === null) throw new Error(M.noRoot);
  var opf = parseXml(opfSrc);

  var items = {};
  var manifest = allByLocal(opf, 'item');
  for (var i = 0; i < manifest.length; i++) {
    var id = manifest[i].getAttribute('id');
    var href = manifest[i].getAttribute('href');
    if (id && href) {
      items[id] = { href: href, type: (manifest[i].getAttribute('media-type') || '').toLowerCase() };
    }
  }

  /* One entry per itemref, INCLUDING the ones we will not read. crengine
     numbers its DocFragments over the spine and we cannot know from here
     whether it counts linear="no" items, so the map carries both numberings
     and the device tries each — see SyncMap::FragmentMode. */
  var spine = [];
  var refs = allByLocal(opf, 'itemref');
  for (var r = 0; r < refs.length; r++) {
    var it = items[refs[r].getAttribute('idref')];
    var linear = (refs[r].getAttribute('linear') || '').toLowerCase() !== 'no';
    var usable = !!it && linear
              && !(it.type && it.type.indexOf('html') < 0 && it.type.indexOf('xml') < 0);
    spine.push({ path: it ? resolvePath(opfDir, it.href) : '', use: usable });
  }
  if (!spine.length) throw new Error(M.noText);

  var chunks = [];      /* text of the fragments that contributed */
  var frags  = [];      /* one record per itemref, for the map */
  var textLen = 0;      /* running byte length of chunks.join('\n\n') */
  var linearNo = 0;

  for (var si = 0; si < spine.length; si++) {
    say(M.converting + ' ' + (si + 1) + '/' + spine.length);
    await breathe();

    var rec = { start: textLen, blocks: [], lin: 0 };
    if (spine[si].use) {
      var src = await zipText(zip, spine[si].path);
      if (src !== null) {
        var doc = docToDoc(src);
        if (doc.text) {
          if (chunks.length > 0) textLen += 2;      /* the '\n\n' join */
          rec.start = textLen;
          rec.blocks = doc.blocks;
          rec.lin = ++linearNo;
          for (var bi = 0; bi < rec.blocks.length; bi++) {
            rec.blocks[bi].c += rec.start;
          }
          textLen += doc.bytes;
          chunks.push(doc.text);
        }
      }
    }
    /* An itemref that contributed nothing still needs a slot, pointing at
       whatever comes next so a pointer into it lands somewhere sane. */
    if (rec.lin === 0) rec.start = textLen;
    frags.push(rec);
  }
  if (!chunks.length) throw new Error(M.noText);

  return {
    /* The trailing newline is stripped again by the device's compactText,
       so the stored file is exactly `textLen` bytes — which is what the map
       is stamped with. */
    text:   chunks.join('\n\n') + '\n',
    bytes:  textLen,
    frags:  frags,
    title:  textByLocal(opf, 'title'),
    author: textByLocal(opf, 'creator')
  };
}

/* ---------------------------------------------------------- spine map ---
   Serializes to the format pure/sync_map_codec.h documents. Little-endian
   throughout, section offsets derived from the counts in the header. */
function buildSyncMap(frags, textBytes) {
  if (!frags.length || frags.length > MAX_FRAGMENTS) return null;

  var pool = [], poolIx = {};
  function intern(str) {
    if (Object.prototype.hasOwnProperty.call(poolIx, str)) return poolIx[str];
    if (pool.length >= MAX_POOL) return -1;
    poolIx[str] = pool.length;
    pool.push(str);
    return poolIx[str];
  }

  var blocks = [], fragRecs = [], overflow = false;
  for (var i = 0; i < frags.length && !overflow; i++) {
    var f = frags[i], base = blocks.length;
    for (var j = 0; j < f.blocks.length; j++) {
      var b = f.blocks[j];
      var pid = intern(b.p), nid = intern(b.n);
      if (pid < 0 || nid < 0) { overflow = true; break; }
      blocks.push([b.c, pid, b.o, nid]);
    }
    var len = blocks.length - base;
    fragRecs.push([f.start, base, len > 65535 ? 65535 : len, f.lin]);
  }

  /* Too many distinct paths to address individually. Keep the fragment
     table, which still puts a sync in the right chapter. */
  if (overflow) {
    blocks = []; pool = []; fragRecs = [];
    for (var k = 0; k < frags.length; k++) {
      fragRecs.push([frags[k].start, 0, 0, frags[k].lin]);
    }
  }

  var enc = new TextEncoder();
  var poolBytes = [], blobLen = 0;
  for (var q = 0; q < pool.length; q++) {
    var e = enc.encode(pool[q]);
    poolBytes.push(e);
    blobLen += e.length;
  }

  var total = 24 + fragRecs.length * 12 + pool.length * 4 + blobLen + blocks.length * 10;
  var ab = new ArrayBuffer(total);
  var dv = new DataView(ab), u8 = new Uint8Array(ab);

  dv.setUint32(0,  0x50534d31, true);    /* 'PSM1' */
  dv.setUint32(4,  textBytes,  true);
  dv.setUint16(8,  fragRecs.length, true);
  dv.setUint16(10, pool.length, true);
  dv.setUint32(12, blocks.length, true);
  dv.setUint32(16, blobLen, true);
  dv.setUint32(20, 0, true);             /* reserved */

  var p = 24;
  for (var fi = 0; fi < fragRecs.length; fi++) {
    dv.setUint32(p,     fragRecs[fi][0], true);
    dv.setUint32(p + 4, fragRecs[fi][1], true);
    dv.setUint16(p + 8, fragRecs[fi][2], true);
    dv.setUint16(p + 10, fragRecs[fi][3], true);
    p += 12;
  }

  var blobAt = p + pool.length * 4, at = 0;
  for (var pi = 0; pi < poolBytes.length; pi++) {
    dv.setUint32(p, at, true);
    p += 4;
    u8.set(poolBytes[pi], blobAt + at);
    at += poolBytes[pi].length;
  }

  p = blobAt + blobLen;
  for (var bj = 0; bj < blocks.length; bj++) {
    dv.setUint32(p,     blocks[bj][0], true);
    dv.setUint16(p + 4, blocks[bj][1], true);
    dv.setUint16(p + 6, blocks[bj][2], true);
    dv.setUint16(p + 8, blocks[bj][3], true);
    p += 10;
  }
  return ab;
}

/* -------------------------------------------------------------- naming ---
   Restricted to the subset sanitizeUploadedFilename() in pure/paths.cpp
   passes through untouched, so the name we announce to /kosync-doc is the
   name the device actually stores. BookInfo::name is 80 bytes. */
function san(s) {
  return (s || '')
    .replace(/[\u2018\u2019]/g, "'")
    .replace(/[\u201c\u201d]/g, '"')
    .replace(/[\u2013\u2014]/g, '-')
    .replace(/[^A-Za-z0-9 _\-]/g, ' ')
    .replace(/\s+/g, ' ')
    .trim();
}
function buildName(title, author, fallback) {
  var name = san(title) || san(fallback.replace(/\.epub$/i, '')) || 'book';
  var who = san(author);
  if (who && name.length + who.length + 3 <= 70) name += ' - ' + who;
  if (name.length > 70) name = name.slice(0, 70).trim();
  return (name || 'book') + '.txt';
}

/* --------------------------------------------------------------- flow --- */
async function run(file) {
  busy(true);
  say(M.hashing);
  await breathe();

  var buf = new Uint8Array(await file.arrayBuffer());
  var docHash = partialMd5(buf);

  var payload, name, syncMap = null;
  if (/\.epub$/i.test(file.name)) {
    say(M.reading);
    await breathe();
    var book = await epubToText(buf);
    name = buildName(book.title, book.author, file.name);
    payload = new Blob([book.text], { type: 'text/plain; charset=utf-8' });
    syncMap = buildSyncMap(book.frags, book.bytes);
  } else {
    name = file.name;
    payload = file;                       /* plain text goes up untouched */
  }

  say(M.uploading);
  var fd = new FormData();
  fd.append('file', payload, name);
  var res = await fetch('/upload', { method: 'POST', body: fd });
  var html = await res.text();
  if (!res.ok) throw new Error(M.failed + ' (' + res.status + ')');

  /* Best-effort: the book is already safely stored, so a failure here must
     not look like an upload failure. Worst case the user pastes the hash in
     by hand on the sync settings page. */
  try {
    await fetch('/kosync-doc', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'name=' + encodeURIComponent(name) + '&md5=' + docHash
    });
  } catch (e) { /* ignore */ }

  /* Likewise best-effort. Without the map a book still syncs, just by
     percentage instead of by paragraph. */
  if (syncMap) {
    try {
      var mf = new FormData();
      mf.append('file', new Blob([syncMap], { type: 'application/octet-stream' }), name);
      await fetch('/kosync-map', { method: 'POST', body: mf });
    } catch (e) { /* ignore */ }
  }

  document.open();
  document.write(html);
  document.close();
}

form.addEventListener('submit', function(ev) {
  var file = input.files && input.files[0];
  if (!file) return;                      /* let native `required` complain */
  ev.preventDefault();
  run(file).catch(function(err) {
    say((err && err.message) ? err.message : String(err), true);
    busy(false);
  });
});

})();
)PALAJS";

#endif  // PALA_WEB_EPUB_JS_H
