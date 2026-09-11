#ifndef PALA_WEB_KOSYNC_H
#define PALA_WEB_KOSYNC_H

// ============================================================================
//  KOReader sync settings + per-book document ids.
//
//    GET  /kosync       — account form + a table of books and their doc ids
//    POST /kosync       — save / test / register / clear, by `do` field
//    POST /kosync-doc   — from the upload page's script: {name, md5}
//    POST /kosync-book  — manual per-book doc-id edit, the escape hatch for
//                         books uploaded before this feature existed
//
//  Mounted from registerWebRoutes() in web/web.cpp.
// ============================================================================
void registerKosyncRoutes();

#endif  // PALA_WEB_KOSYNC_H
