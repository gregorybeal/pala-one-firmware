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
//  Served by web/chrome.cpp as a cacheable resource, like /style.css, so the
//  ~11 KB doesn't ride along on every page render.
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
var SKIP = { script:1, style:1, head:1, svg:1, img:1, image:1, audio:1,
             video:1, object:1, iframe:1, link:1, meta:1, title:1 };
var BLOCK = { p:1, div:1, li:1, h1:1, h2:1, h3:1, h4:1, h5:1, h6:1,
              blockquote:1, td:1, th:1, tr:1, section:1, article:1, aside:1,
              header:1, footer:1, pre:1, figcaption:1, figure:1, dt:1, dd:1,
              ul:1, ol:1, dl:1, table:1, hr:1, nav:1, body:1 };

function isToc(el) {
  var t = el.getAttribute('epub:type')
       || (el.getAttributeNS ? el.getAttributeNS('http://www.idpf.org/2007/ops', 'type') : null)
       || '';
  return /\b(toc|landmarks|page-list)\b/.test(t);
}

function walk(node, out) {
  for (var n = node.firstChild; n; n = n.nextSibling) {
    if (n.nodeType === 3) {                       /* text */
      out.push(n.nodeValue.replace(/\s+/g, ' '));
      continue;
    }
    if (n.nodeType !== 1) continue;               /* skip comments, PIs */
    var name = (n.localName || '').toLowerCase();
    if (SKIP[name]) continue;
    if (name === 'nav' && isToc(n)) continue;
    if (name === 'br') { out.push('\n'); continue; }
    var heading = /^h[1-6]$/.test(name);
    var block = heading || !!BLOCK[name];
    if (block) out.push(heading ? '\n\n' : '\n');
    walk(n, out);
    if (block) out.push(heading ? '\n\n' : '\n');
  }
}

function docToText(src) {
  var dp = new DOMParser();
  var d = dp.parseFromString(src, 'application/xhtml+xml');
  /* Plenty of shipped EPUBs are not actually well-formed XML. */
  if (d.querySelector('parsererror')) d = dp.parseFromString(src, 'text/html');
  var body = d.body || allByLocal(d, 'body')[0] || d.documentElement;
  if (!body) return '';
  var out = [];
  walk(body, out);
  return out.join('')
    .replace(/\u00a0/g, ' ')
    .replace(/[ \t]+/g, ' ')
    .replace(/[ \t]*\n[ \t]*/g, '\n')
    .replace(/\n{3,}/g, '\n\n')
    .trim();
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

  var spine = [];
  var refs = allByLocal(opf, 'itemref');
  for (var r = 0; r < refs.length; r++) {
    if ((refs[r].getAttribute('linear') || '').toLowerCase() === 'no') continue;
    var it = items[refs[r].getAttribute('idref')];
    if (!it) continue;
    if (it.type && it.type.indexOf('html') < 0 && it.type.indexOf('xml') < 0) continue;
    spine.push(resolvePath(opfDir, it.href));
  }
  if (!spine.length) throw new Error(M.noText);

  var chunks = [];
  for (var s = 0; s < spine.length; s++) {
    say(M.converting + ' ' + (s + 1) + '/' + spine.length);
    await breathe();
    var src = await zipText(zip, spine[s]);
    if (src === null) continue;
    var text = docToText(src);
    if (text) chunks.push(text);
  }
  if (!chunks.length) throw new Error(M.noText);

  return {
    text:   chunks.join('\n\n') + '\n',
    title:  textByLocal(opf, 'title'),
    author: textByLocal(opf, 'creator')
  };
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

  var payload, name;
  if (/\.epub$/i.test(file.name)) {
    say(M.reading);
    await breathe();
    var book = await epubToText(buf);
    name = buildName(book.title, book.author, file.name);
    payload = new Blob([book.text], { type: 'text/plain; charset=utf-8' });
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
