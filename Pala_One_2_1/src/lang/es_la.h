#ifndef PALA_LANG_ES_LA_H
#define PALA_LANG_ES_LA_H

// ============================================================================
//  Spanish (Latin America) string table — es_LA.
//  Mirror of en.h; the key set MUST stay identical. Adding new keys: edit en.h
//  first, then add the same key here. See src/lang/lang.h for the rule.
//
//  Glyph coverage: all accents used here (á é í ó ú ñ ¿ ¡ ü) are in u8g2's
//  Latin Extended (_te) font set already linked by src/ui/font.cpp, so no
//  font change is required. Web responses already declare charset=utf-8.
// ============================================================================

// ----------------------------------------------------------------------------
//  Boot / fatal screens
// ----------------------------------------------------------------------------
#define D_BOOT_STORAGE_ERROR        "Error de almacenamiento"
#define D_BOOT_TRY_FACTORY_RESET    "Pruebe reinicio de fábrica"

// ----------------------------------------------------------------------------
//  About screen
// ----------------------------------------------------------------------------
#define D_ABOUT_HEADER              "Dispositivo"
#define D_ABOUT_FIRMWARE_PREFIX     "Firmware "
#define D_ABOUT_GESTURE_NEXT        "1x siguiente / abajo"
#define D_ABOUT_GESTURE_OPEN        "2x abrir / elegir"
#define D_ABOUT_GESTURE_HOME        "3x inicio"
#define D_ABOUT_GESTURE_BOOKMARK    "Mantener: marcapáginas"

// ----------------------------------------------------------------------------
//  Library menu entries
// ----------------------------------------------------------------------------
#define D_MENU_BOOKMARKS            "Marcapáginas"
#define D_MENU_LIST                 "Lista"
#define D_MENU_APPS                 "Apps"
#define D_MENU_STATISTICS           "Estadísticas"
#define D_MENU_DEVICE               "Dispositivo"
#define D_MENU_UPLOAD               "Conectar"
#define D_LIBRARY_OPEN_FAILED       "Error al abrir"
#define D_LIBRARY_TRY_UPLOAD        "Intente subir de nuevo"

// ----------------------------------------------------------------------------
//  Statistics screen
// ----------------------------------------------------------------------------
#define D_STATS_HEADING                "Estadísticas"
#define D_STATS_STREAK_CURRENT_FMT     "Racha actual: %u días"
#define D_STATS_STREAK_LONGEST_FMT     "Mejor: %u  Sesiones: %u"
#define D_STATS_LIFETIME_PAGES_FMT     "Páginas leídas: %llu"
#define D_STATS_LIFETIME_PRESSES_FMT   "Pulsaciones: %llu"

// ----------------------------------------------------------------------------
//  List screen
// ----------------------------------------------------------------------------
#define D_LIST_HEADER               "Lista"
#define D_LIST_NONE                 "Sin elementos"

// ----------------------------------------------------------------------------
//  Upload screen
// ----------------------------------------------------------------------------
#define D_UPLOAD_HEADER             "Subir"
#define D_UPLOAD_WIFI               "Wi-Fi"
#define D_UPLOAD_PASSWORD           "Contraseña"
#define D_UPLOAD_OPEN               "Abrir"
#define D_UPLOAD_CONNECTING         "Conectando"
#define D_UPLOAD_CONNECTED          "Conectado"
#define D_UPLOAD_HOTSPOT_HINT_L1    "Pulsa el botón para"
#define D_UPLOAD_HOTSPOT_HINT_L2    "usar punto de acceso"

// ----------------------------------------------------------------------------
//  Apps screen
// ----------------------------------------------------------------------------
#define D_APPS_HEADER               "Apps"
#define D_APPS_NONE                 "Sin apps"

// ----------------------------------------------------------------------------
//  Update screen
// ----------------------------------------------------------------------------
#define D_MENU_UPDATE               "Actualización FW"
#define D_UPDATE_HEADER             "Actualización"
#define D_UPDATE_VERSION_PREFIX     "Versión actual: "
#define D_UPDATE_CHANNEL_LABEL      "Seleccionar canal:"
#define D_UPDATE_CHAN_STABLE        "Estable"
#define D_UPDATE_CHAN_DEV           "Dev"
#define D_UPDATE_BTN_CHECK          "[ Buscar actualización ]"
#define D_UPDATE_NO_CREDS_L1        "Sin credenciales Wi-Fi."
#define D_UPDATE_NO_CREDS_L2        "Configura via instalador."
#define D_UPDATE_CONNECTING         "Conectando..."
#define D_UPDATE_CONN_FAILED        "Conexión Wi-Fi fallida"
#define D_UPDATE_CHECKING           "Verificando..."
#define D_PAGINATE_HEADER          "Indexando"
#define D_UPDATE_SERVER_FAIL        "No se puede alcanzar el servidor"
#define D_UPDATE_UP_TO_DATE         "Ya está actualizado"
#define D_UPDATE_AVAILABLE_PREFIX   "Disponible: "
#define D_UPDATE_BTN_INSTALL        "[ Instalar actualización ]"
#define D_UPDATE_INSTALLING         "Instalando..."
#define D_UPDATE_DOWNLOAD_FAILED    "Descarga fallida"
#define D_UPDATE_REBOOT_MSG         "Actualización instalada"
#define D_UPDATE_REBOOT_HINT        "2x para reiniciar"

// ----------------------------------------------------------------------------
//  Bookmarks screens
// ----------------------------------------------------------------------------
#define D_BOOKMARKS_HEADER          "Marcapáginas"
#define D_BOOKMARKS_NO_BOOKS        "Sin libros"
#define D_BOOKMARKS_NONE            "Sin marcapáginas"
#define D_BOOKMARKS_OPEN_FAILED     "Error al abrir"

// ----------------------------------------------------------------------------
//  Reader
// ----------------------------------------------------------------------------
#define D_READER_BOOK_EMPTY         "Libro vacío"
#define D_READER_BACK_LIBRARY       "Volver a biblioteca"

// ----------------------------------------------------------------------------
//  App loader error overlay
// ----------------------------------------------------------------------------
#define D_APP_ERR_TITLE             "Error de app"
#define D_APP_ERR_NULL_PATH         "ruta nula"
#define D_APP_ERR_NOT_FOUND         "App no encontrada"
#define D_APP_ERR_TOO_SMALL         "App muy pequeña"
#define D_APP_ERR_INVALID_FILE      "Archivo inválido"
#define D_APP_ERR_TOO_LARGE         "App muy grande"
#define D_APP_ERR_SIZE_LIMIT        "> 48 KB"
#define D_APP_ERR_READ              "Error de lectura"
#define D_APP_ERR_PARTIAL_READ      "Lectura parcial"
#define D_APP_ERR_NO_EXEC_MEM       "Sin memoria ejec."
#define D_APP_ERR_BAD_FILE          "App inválida"
#define D_APP_ERR_WRONG_MAGIC       "Firma incorrecta"
#define D_APP_ERR_API_MISMATCH      "API incompatible"
#define D_APP_ERR_API_FMT           "API v%u, requiere v%u"
#define D_APP_ERR_BAD_ENTRY         "Entrada inválida"
#define D_APP_ERR_BAD_RELOC         "Tabla reloc inválida"
#define D_APP_ERR_RELOC_RANGE       "Reloc fuera de rango"

// ----------------------------------------------------------------------------
//  Bookmark add toasts
// ----------------------------------------------------------------------------
#define D_TOAST_BOOKMARK_EXISTS     "Marcapáginas ya existe"
#define D_TOAST_BOOKMARK_SAVED      "Marcapáginas guardado"

// ----------------------------------------------------------------------------
//  Lock / screensaver
// ----------------------------------------------------------------------------
#define D_SCREENSAVER_LOCKED        "Bloqueado"
#define D_TOAST_UNLOCKED            "Desbloqueado"

// ============================================================================
//  Web UI
// ============================================================================

// ----------------------------------------------------------------------------
//  Shared chrome / storage card
// ----------------------------------------------------------------------------
#define D_WEB_STORAGE_HEADING       "Almacenamiento"
#define D_WEB_STORAGE_BOOKS         "Libros"
#define D_WEB_STORAGE_USED          "Usado"
#define D_WEB_STORAGE_FREE          "Libre"
#define D_WEB_STORAGE_TOTAL         "Total"
#define D_WEB_STORAGE_PCT_SUFFIX    "% del almacenamiento interno en uso."

// ----------------------------------------------------------------------------
//  Navigation links
// ----------------------------------------------------------------------------
#define D_WEB_NAV_HOME              "Inicio"
#define D_WEB_NAV_FILES             "Archivos"
#define D_WEB_NAV_BOOKMARKS         "Marcapáginas"
#define D_WEB_NAV_LIST              "Lista"
#define D_WEB_NAV_SCREENSAVER       "Salvapantallas"
#define D_WEB_NAV_SETTINGS          "Ajustes"
#define D_WEB_NAV_SYNC              "Sincronizar"
#define D_WEB_NAV_WIFI              "Wi-Fi"
#define D_WEB_NAV_FACTORY_RESET     "Reinicio de fábrica"
#define D_WEB_NAV_BACK              "Atrás"

// ----------------------------------------------------------------------------
//  Home page
// ----------------------------------------------------------------------------
#define D_WEB_HOME_TITLE            "Pala One"
#define D_WEB_HOME_FW_PREFIX        "Firmware "
#define D_WEB_HOME_MIDDOT_SEP       " &middot; "
#define D_WEB_HOME_BOOKS_SUFFIX     " libros"
#define D_WEB_HOME_FREE_LABEL       "Libre: "
#define D_WEB_HOME_STORAGE_WARN     "&#9888; Almacenamiento no disponible o casi lleno. Si las subidas fallan, elimine libros o use Reinicio de fábrica desde esta interfaz web."
#define D_WEB_UPLOAD_BOOK_HEADING   "Subir libro"
#define D_WEB_UPLOAD_BOOK_DESC      "Envíe archivos UTF-8 de texto plano (<b>.txt</b>) o <b>.epub</b> a <b>/books</b> en el dispositivo, luego organícelos en carpetas desde la página Archivos. Los EPUB se convierten a texto en su navegador antes de subirlos."
#define D_WEB_UPLOAD_BOOK_BUTTON    "Subir"
// Subida EPUB — cadenas usadas por el conversor del navegador en
// src/web/epub_js.h. Se insertan en un objeto JS con comillas dobles, por lo
// que NO deben contener comillas dobles ni barra invertida.
#define D_WEB_EPUB_UNSUPPORTED      "Este navegador no puede abrir archivos EPUB. Las subidas de texto plano (.txt) siguen funcionando."
#define D_WEB_EPUB_HASHING          "Leyendo archivo..."
#define D_WEB_EPUB_READING          "Abriendo EPUB..."
#define D_WEB_EPUB_CONVERTING       "Convirtiendo seccion"
#define D_WEB_EPUB_UPLOADING        "Subiendo al dispositivo..."
#define D_WEB_EPUB_ERR_NOT_EPUB     "Ese archivo no es un EPUB legible."
#define D_WEB_EPUB_ERR_NO_ROOT      "Al EPUB le falta su documento de paquete."
#define D_WEB_EPUB_ERR_NO_TEXT      "No se encontro texto legible en este EPUB."
#define D_WEB_EPUB_ERR_ZIP64        "Los archivos EPUB ZIP64 no son compatibles."
#define D_WEB_EPUB_ERR_METHOD       "Metodo de compresion ZIP no compatible"
#define D_WEB_EPUB_ERR_BAD_XML      "Este EPUB contiene XML mal formado."
#define D_WEB_EPUB_ERR_UPLOAD       "La subida fallo"
#define D_WEB_MANAGE_FILES_BUTTON   "Administrar archivos"
#define D_WEB_INSTALL_APP_HEADING   "Instalar app"
#define D_WEB_INSTALL_APP_DESC      "Suba un binario de app Pala (<b>.bin</b>) a <b>/apps</b>. El encabezado se valida antes de confirmar; solo se aceptan archivos con la firma y versión de API correctas. Abra <b>Apps</b> desde la biblioteca para ejecutarla."
#define D_WEB_INSTALL_APP_BUTTON    "Instalar app"
#define D_WEB_NOTES_HEADING         "Notas"
#define D_WEB_NOTES_DESC            "Los libros subidos se normalizan y compactan antes de guardarse, por lo que un TXT de origen puede ser más grande que el archivo final almacenado. El lector está optimizado para texto plano UTF-8 e idiomas con alfabeto latino."

// ----------------------------------------------------------------------------
//  Files page
// ----------------------------------------------------------------------------
#define D_WEB_FILES_HEADING         "Archivos"
#define D_WEB_FILES_SUBTITLE        "Administre libros, carpetas y estructura de la biblioteca de Pala One."
#define D_WEB_CREATE_FOLDER_HEADING "Crear carpeta"
#define D_WEB_CREATE_FOLDER_PLACEHOLDER "libros o clasicos/espanol"
#define D_WEB_CREATE_FOLDER_BUTTON  "Crear carpeta"
#define D_WEB_CREATE_FOLDER_HINT    "Las carpetas viven dentro de /books."
#define D_WEB_FOLDERS_HEADING       "Carpetas"
#define D_WEB_NO_FOLDERS            "Aún no hay carpetas. Los libros viven en la raíz de /books."
#define D_WEB_CONFIRM_DELETE_FOLDER "¿Eliminar carpeta? Solo se pueden eliminar carpetas vacías."
#define D_WEB_DELETE_BUTTON         "Eliminar"
#define D_WEB_LIBRARY_FILES_HEADING "Archivos de biblioteca"
#define D_WEB_LIBRARY_FULL_WARN     "&#9888; Biblioteca llena (máx. 80 libros). Elimine libros para hacer espacio."
#define D_WEB_FOLDER_LIMIT_WARN     "&#9888; Límite de carpetas alcanzado (máx. 32)."
#define D_WEB_NO_BOOKS_UPLOADED     "Aún no se han subido libros."
#define D_WEB_BOOK_ROOT             "Raíz"
#define D_WEB_BOOK_BYTES_LABEL      " bytes"
#define D_WEB_BOOK_FOLDER_LABEL     " &middot; carpeta: "
#define D_WEB_BOOK_CURRENT_PAGE     " &middot; página actual: "
#define D_WEB_JUMP_BUTTON           "Ir"
#define D_WEB_JUMP_HINT             "Establezca la página que se abrirá la próxima vez en el dispositivo."
#define D_WEB_JUMP_HINT2            "La primera apertura puede tardar un momento."
#define D_WEB_PAGE_PLACEHOLDER      "Página"
#define D_WEB_MOVE_BUTTON           "Mover"
#define D_WEB_MOVE_HINT             "Use la ruta exacta de la carpeta."
#define D_WEB_MOVE_PLACEHOLDER      "vacío para raíz"
#define D_WEB_CONFIRM_DELETE_FILE   "¿Eliminar archivo?"
#define D_WEB_DOWNLOAD_BUTTON       "Descargar"
#define D_WEB_APPS_PAGE_HEADING     "Apps"
#define D_WEB_NO_APPS_INSTALLED     "Sin apps instaladas."
#define D_WEB_CONFIRM_DELETE_APP    "¿Eliminar app?"

// ----------------------------------------------------------------------------
//  Plain-text 4xx/5xx error bodies
// ----------------------------------------------------------------------------
#define D_WEB_ERR_MISSING_ID            "id faltante"
#define D_WEB_ERR_BAD_ID                "id inválido"
#define D_WEB_ERR_MISSING_FOLDER        "carpeta faltante"
#define D_WEB_ERR_BAD_FOLDER            "carpeta inválida"
#define D_WEB_ERR_FOLDER_LIMIT          "límite de carpetas alcanzado"
#define D_WEB_ERR_MKDIR_FAILED          "fallo al crear carpeta"
#define D_WEB_ERR_FOLDER_NOT_FOUND      "carpeta no encontrada"
#define D_WEB_ERR_FOLDER_NOT_EMPTY      "carpeta no vacía"
#define D_WEB_ERR_DELETE_FAILED         "fallo al eliminar"
#define D_WEB_ERR_FOLDER_CREATE_FAILED  "fallo al crear carpeta"
#define D_WEB_ERR_DEST_EXISTS           "destino ya existe"
#define D_WEB_ERR_MOVE_FAILED           "fallo al mover"
#define D_WEB_ERR_MISSING_ID_PAGE       "id/página faltante"
#define D_WEB_ERR_MISSING_BOOK_IDX      "libro/idx faltante"
#define D_WEB_ERR_BAD_BOOK              "libro inválido"
#define D_WEB_ERR_BAD_IDX               "idx inválido"
#define D_WEB_ERR_MISSING_BOOK          "libro faltante"
#define D_WEB_ERR_MISSING_NAME          "nombre faltante"
#define D_WEB_ERR_INVALID_NAME          "nombre inválido"

// ----------------------------------------------------------------------------
//  List page
// ----------------------------------------------------------------------------
#define D_WEB_LIST_HEADING          "Lista"
#define D_WEB_LIST_SUBTITLE         "Cree una lista simple de compras o tareas para Pala One."
#define D_WEB_LIST_EDIT_HEADING     "Editar lista"
#define D_WEB_LIST_EDIT_DESC        "Los elementos aparecen en el dispositivo solo cuando al menos una línea contiene texto. Mantenga el botón en el dispositivo para marcar un elemento como hecho."
#define D_WEB_LIST_ITEM_PLACEHOLDER "Elemento de lista"
#define D_WEB_LIST_SAVE_BUTTON      "Guardar lista"
#define D_WEB_LIST_DELETE_DONE      "Eliminar elementos marcados"
#define D_WEB_LIST_HINT             "Las filas vacías se ignoran. Las filas marcadas se pueden eliminar directamente."

// ----------------------------------------------------------------------------
//  Reset page
// ----------------------------------------------------------------------------
#define D_WEB_RESET_HEADING         "Reinicio de fábrica"
#define D_WEB_RESET_SUBTITLE        "Borre todos los libros, marcapáginas, progreso y recursos personalizados."
#define D_WEB_RESET_CONFIRM_HEADING "Confirmar reinicio"
#define D_WEB_RESET_WARNING         "Esto eliminará TODOS los libros, marcapáginas y progreso de lectura."
#define D_WEB_RESET_DETAIL          "El sistema de archivos del dispositivo se formateará y los ajustes volverán a los valores predeterminados."
#define D_WEB_RESET_YES_BUTTON      "Sí, reiniciar"
#define D_WEB_RESET_COMPLETE_HEADING "Reinicio de fábrica completo"
#define D_WEB_RESET_COMPLETE_DESC   "Todos los libros, marcapáginas, progreso y recursos personalizados fueron eliminados. El dispositivo está ahora en un estado limpio."
#define D_WEB_GO_HOME_BUTTON        "Ir al inicio"
#define D_WEB_OPEN_FILES_BUTTON     "Abrir archivos"
#define D_WEB_RESET_SUCCESS_TITLE   "Reinicio completo"
#define D_WEB_RESET_SUCCESS_SUBTITLE "Pala One se reinició con éxito."
#define D_WEB_RESET_BANNER          "&#10003; Reinicio de fábrica completo."

// ----------------------------------------------------------------------------
//  Settings page
// ----------------------------------------------------------------------------
#define D_WEB_SETTINGS_TITLE        "Ajustes de Pala One"
#define D_WEB_SETTINGS_SUBTITLE_PREFIX "Firmware "
#define D_WEB_SETTINGS_SUBTITLE_SUFFIX " — página de configuración almacenada directamente en el dispositivo."
#define D_WEB_SETTINGS_BACK_NAV     "&#8592; Inicio"
#define D_WEB_READING_HEADING       "Lectura"
#define D_WEB_FONT_SIZE_LABEL       "Tamaño de fuente"
#define D_WEB_FONT_SIZE_8           "8px &mdash; diminuto"
#define D_WEB_FONT_SIZE_10          "10px &mdash; pequeño"
#define D_WEB_FONT_SIZE_12          "12px &mdash; mediano"
#define D_WEB_FONT_SIZE_14          "14px &mdash; grande"
#define D_WEB_FONT_SIZE_HINT        "Controla cuántas líneas caben en cada página."
#define D_WEB_SLEEP_AFTER_LABEL     "Suspender después de"
#define D_WEB_SLEEP_30S             "30 segundos"
#define D_WEB_SLEEP_1M              "1 minuto"
#define D_WEB_SLEEP_2M              "2 minutos"
#define D_WEB_SLEEP_5M              "5 minutos"
#define D_WEB_SLEEP_10M             "10 minutos"
#define D_WEB_SLEEP_30M             "30 minutos"
#define D_WEB_SLEEP_HINT            "La suspensión automática mantiene bajo el consumo de batería en reposo."
#define D_WEB_LINE_SPACING_LABEL    "Espaciado de línea"
#define D_WEB_LINE_SPACING_0        "0 px &mdash; compacto"
#define D_WEB_LINE_SPACING_1        "1 px &mdash; normal"
#define D_WEB_LINE_SPACING_2        "2 px &mdash; relajado"
#define D_WEB_LINE_SPACING_3        "3 px &mdash; suelto"
#define D_WEB_LINE_SPACING_HINT     "Un pequeño cambio aquí puede facilitar mucho la lectura."
#define D_WEB_NO_SCREENSAVER_LABEL  "Modo sin salvapantallas"
#define D_WEB_NO_SCREENSAVER_HINT   "El dispositivo sigue durmiéndose según el temporizador habitual y actualiza la pantalla antes de dormirse. Luego muestra la última página del libro y omite la actualización completa al despertar, para que puedas continuar leyendo con un solo clic sin la interrupción de un refresco de pantalla."
#define D_WEB_LOCK_ON_SLEEP_LABEL   "Bloquear al dormir"
#define D_WEB_LOCK_ON_SLEEP_HINT    "Bloquea el dispositivo automáticamente cada vez que se duerme. Necesitarás una pulsación larga para desbloquearlo al despertar."
#define D_WEB_SAVE_SETTINGS_BUTTON  "Guardar ajustes"
#define D_WEB_SETTINGS_NO_EXTRAS    "Sin archivos extra, scripts ni fuentes."
#define D_WEB_SCREENSAVER_HEADING   "Salvapantallas"
#define D_WEB_SCREENSAVER_SPECS     "Suba bytes XBM en bruto: <b>3904 bytes</b>, 250&times;122 px, 1 bit, LSB primero, 32 bytes por fila."
#define D_WEB_SCREENSAVER_TIP       "Consejo: use <a class='link' href='https://javl.github.io/image2cpp/' target='_blank'>image2cpp</a> con <b>Plain bytes</b>. Invierta los colores si es necesario."
#define D_WEB_SCREENSAVER_ACTIVE    "&#10003; Salvapantallas personalizado activo."
#define D_WEB_CONFIRM_DEL_SCREENSAVER "¿Eliminar salvapantallas personalizado?"
#define D_WEB_SCREENSAVER_DEFAULT   "Usando salvapantallas predeterminado."
#define D_WEB_SLEEP_IMAGE_LABEL     "Archivo de imagen de suspensión"
#define D_WEB_SCREENSAVER_UPLOAD_BUTTON "Subir imagen"

// Buttons / remappable hold-gestures section.
#define D_WEB_BUTTONS_HEADING       "Botones"
#define D_WEB_BUTTONS_HINT          "1 clic = siguiente, 2 = anterior, 3 = inicio. Las tres pulsaciones largas abajo son reasignables."
#define D_WEB_BUTTONS_LONG          "Pulsación larga"
#define D_WEB_BUTTONS_EXTRA_LONG    "Pulsación muy larga"
#define D_WEB_BUTTONS_CLICK_HOLD    "Clic y mantener"
#define D_WEB_BUTTONS_SAVE          "Guardar botones"
#define D_WEB_BUTTONS_LOCK_HINT     "Si está bloqueado, repita cualquier pulsación larga para desbloquear."
#define D_WEB_BUTTONS_ACTION_NONE     "Ninguna"
#define D_WEB_BUTTONS_ACTION_BOOKMARK "Marcar página"
#define D_WEB_BUTTONS_ACTION_LOCK     "Bloquear dispositivo"
#define D_WEB_BUTTONS_ACTION_MENU     "Abrir menú"

// Device personalization card (src/web/settings.cpp).
#define D_WEB_DEVICE_HEADING        "Dispositivo"
#define D_WEB_DEVICE_INTRO          "Personaliza el nombre que aparece en el encabezado de la biblioteca."
#define D_WEB_HEADER_TITLE_LABEL    "Título del encabezado"
#define D_WEB_HEADER_TITLE_HINT     "Se muestra arriba de la pantalla de biblioteca. Deja vacío para ocultarlo."
#define D_WEB_HEADER_TITLE_RESET    "Restaurar predeterminado"

// ----------------------------------------------------------------------------
//  Upload routes
// ----------------------------------------------------------------------------
#define D_WEB_UPLOAD_COMPLETE_HEADING "Subida completa"
#define D_WEB_UPLOAD_COMPLETE_DESC  "Su libro está ahora almacenado en el dispositivo y disponible en la biblioteca."
#define D_WEB_UPLOAD_BOOK_LABEL     "Libro"
#define D_WEB_UPLOAD_STORED_SIZE    "Tamaño almacenado"
#define D_WEB_UPLOAD_BOOKS_NOW      "Libros ahora"
#define D_WEB_UPLOAD_FREE_SPACE     "Espacio libre"
#define D_WEB_UPLOAD_ANOTHER        "Subir otro"
#define D_WEB_UPLOAD_BOOK_SAVED     "Libro guardado con éxito."
#define D_WEB_UPLOAD_FINISHED       "&#10003; Subida finalizada."
#define D_WEB_UPLOAD_ERR_FALLBACK   "Subida fallida"
#define D_WEB_ERR_LIBRARY_FULL      "Biblioteca llena"
#define D_WEB_ERR_NOT_ENOUGH_SPACE  "Espacio insuficiente"
#define D_WEB_ERR_CANT_CREATE_TEMP_BOOK "No se pudo crear archivo temporal de subida"
#define D_WEB_ERR_WRITE_FAILED      "Fallo de escritura (¿sin espacio?)"
#define D_WEB_ERR_FINALIZE_UPLOAD   "Fallo al finalizar la subida"
#define D_WEB_ERR_EMPTY_UPLOAD      "Subida vacía"
#define D_WEB_ERR_UPLOAD_ABORTED    "Subida abortada"
#define D_WEB_SLEEP_UPLOAD_ERR_FALLBACK "Fallo al subir imagen de suspensión"
#define D_WEB_SLEEP_UPLOAD_HEADING  "Salvapantallas actualizado"
#define D_WEB_SLEEP_UPLOAD_DESC     "Su imagen de suspensión personalizada se guardó con éxito y se mostrará la próxima vez que el dispositivo se suspenda."
#define D_WEB_BACK_TO_SETTINGS      "Volver a ajustes"
#define D_WEB_SLEEP_UPLOAD_SUBTITLE "Salvapantallas guardado con éxito."
#define D_WEB_SLEEP_UPLOAD_BANNER   "&#10003; Imagen de suspensión personalizada subida."
#define D_WEB_SLEEP_ERR_TEMP        "No se pudo crear archivo temporal de suspensión"
#define D_WEB_SLEEP_ERR_SIZE        "La imagen de suspensión debe ser exactamente 3904 bytes"
#define D_WEB_SLEEP_ERR_SAVE        "Fallo al guardar imagen de suspensión"
#define D_WEB_SLEEP_ERR_ABORTED     "Subida de imagen de suspensión abortada"

// ----------------------------------------------------------------------------
//  App upload route
// ----------------------------------------------------------------------------
#define D_WEB_APP_UPLOAD_ERR_FALLBACK "Subida de app fallida"
#define D_WEB_APP_INSTALLED_HEADING "App instalada"
#define D_WEB_APP_INSTALLED_DESC    "Abra el dispositivo, vaya a <b>Apps</b> en la biblioteca y haga doble clic para ejecutarla."
#define D_WEB_APP_LABEL             "App"
#define D_WEB_APPS_NOW              "Apps ahora"
#define D_WEB_APP_INSTALLED_SUBTITLE "App guardada en /apps/."
#define D_WEB_APP_INSTALLED_BANNER  "&#10003; App lista para ejecutarse."
#define D_WEB_APP_VALID_OK          "OK"
#define D_WEB_APP_VALID_TOO_SMALL   "App inválida (archivo muy pequeño)"
#define D_WEB_APP_VALID_BAD_MAGIC   "App inválida (firma incorrecta)"
#define D_WEB_APP_VALID_BAD_ENTRY   "App inválida (entrada inválida)"
#define D_WEB_APP_VALID_BAD_RELOC   "App inválida (tabla reloc inválida)"
#define D_WEB_APP_VALID_API_FMT     "App inválida (API v%u, requiere v%u)"
#define D_WEB_APP_VALID_INVALID     "App inválida"
#define D_WEB_APPS_DIR_FULL         "Directorio de apps lleno"
#define D_WEB_ERR_CANT_CREATE_TEMP_APP "No se pudo crear archivo temporal de app"
#define D_WEB_APP_TOO_LARGE         "App muy grande (> 48 KB)"
#define D_WEB_APP_BINARY_TOO_SMALL  "Binario de app muy pequeño"
#define D_WEB_APP_CANT_READ_HEADER  "No se pudo leer el encabezado de la app"
#define D_WEB_APP_FINALIZE_FAILED   "Fallo al finalizar la subida de la app"
#define D_WEB_APP_UPLOAD_ABORTED    "Subida de app abortada"

// ----------------------------------------------------------------------------
//  Bookmarks web page
// ----------------------------------------------------------------------------
#define D_WEB_BOOKMARKS_HEADING     "Marcapáginas"
#define D_WEB_BOOKMARKS_SUBTITLE    "Posiciones de lectura guardadas para Pala One, agrupadas por libro."
#define D_WEB_NO_BOOKS_YET          "Aún no hay libros disponibles."
#define D_WEB_NO_BOOKMARKS_CARD     "Sin marcapáginas"
#define D_WEB_BOOKMARKS_OPEN_FAILED_CARD "Error al abrir"
#define D_WEB_BOOKMARK_PILL_PREFIX  "Marcapáginas "
#define D_WEB_BOOKMARK_VIEW         "Ver"
#define D_WEB_CONFIRM_DELETE_BOOKMARK "¿Eliminar marcapáginas?"
#define D_WEB_BOOKMARK_DOWNLOAD_ALL "Descargar todos los marcapáginas"
#define D_WEB_BOOKMARK_VIEW_HEADING "Vista de marcapáginas"
#define D_WEB_BOOKMARK_VIEW_SUBTITLE "Previsualice el texto de página guardado para este marcapáginas."
#define D_WEB_BOOKMARK_VIEW_BACK_NAV "&#8592; Atrás"
#define D_WEB_BOOKMARK_PAGE_EMPTY   "(vacío)"
#define D_WEB_BOOKMARK_OPEN_FAILED_DOT "Error al abrir."

// Bookmark export plaintext labels
#define D_WEB_BMEXPORT_BOOK         "Libro: "
#define D_WEB_BMEXPORT_BOOKMARKS    "Marcapáginas: "
#define D_WEB_BMEXPORT_BOOKMARK_LBL "Marcapáginas "
#define D_WEB_NO_BOOKMARKS_THIS_BOOK "Sin marcapáginas para este libro"

// ----------------------------------------------------------------------------
//  Lector en el navegador + buscar/saltar (src/web/find.cpp).
// ----------------------------------------------------------------------------
#define D_WEB_READ_TITLE            "Leer"
#define D_WEB_READ_SUBTITLE         "Explora y busca el libro en tu navegador. Usa Saltar para fijar el punto de retoma del dispositivo."
#define D_WEB_READ_BYTES_LABEL      "bytes"
#define D_WEB_READ_CURRENT_PAGE_LABEL "página actual:"
#define D_WEB_READ_FIND_PLACEHOLDER "Buscar en el libro"
#define D_WEB_READ_FIND_ALL         "Buscar todo"
#define D_WEB_READ_FIND_PREV        "Anterior"
#define D_WEB_READ_FIND_NEXT        "Siguiente"
#define D_WEB_READ_JUMP_HERE        "Saltar aquí"
#define D_WEB_READ_LOADING          "Cargando texto del libro..."
#define D_WEB_READ_PAGE_PLACEHOLDER "Número de página"
#define D_WEB_READ_JUMP_PAGE        "Saltar a página"
#define D_WEB_READ_JUMP_HINT        "Guarda directamente la próxima página de apertura."
#define D_WEB_READ_AND_FIND_LINK    "Leer y buscar"

// ----------------------------------------------------------------------------
//  Familia de fuente + lectura biónica + retención de posición
//  (src/web/settings.cpp).
// ----------------------------------------------------------------------------
#define D_WEB_READING_INTRO         "Cambiar la fuente, familia, espaciado de línea o modo biónico mantiene tu lugar en el libro actual &mdash; el dispositivo reorganiza las páginas alrededor del byte que estás leyendo y aterriza en la página que lo contiene."
#define D_WEB_FONT_FAMILY_LABEL     "Familia de fuente"
#define D_WEB_FONT_FAMILY_HELVETICA "Helvetica"
#define D_WEB_FONT_FAMILY_DYSLEXIC  "OpenDyslexic"
#define D_WEB_FONT_FAMILY_HINT      "OpenDyslexic usa formas de letra más gruesas diseñadas para una lectura más fácil."
#define D_WEB_BIONIC_LABEL          "Lectura biónica"
#define D_WEB_BIONIC_HINT           "Resalta en negrita las primeras letras de cada palabra para anclar la mirada."
#define D_WEB_SETTINGS_APPLY_HINT   "Los cambios se aplican en la próxima página renderizada."

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
//  Editor y administrador multi-ranura de salvapantallas (src/web/screensavers.cpp).
//  Las cadenas internas del editor en JS (estado / errores) aún NO están i18n'd.
// ----------------------------------------------------------------------------
#define D_WEB_SS_TITLE              "Salvapantallas"
#define D_WEB_SS_SUBTITLE           "Imágenes de suspensión personalizadas, rotación multi-ranura y editor de bitmap en el firmware."
#define D_WEB_SS_ROTATION_HEADING   "Rotación"
#define D_WEB_SS_ROTATION_INTRO     "Elige qué se muestra en la pantalla cuando el dispositivo se suspende. Cycle recorre las ranuras pobladas en orden; Shuffle elige al azar sin repeticiones inmediatas."
#define D_WEB_SS_MODE_LABEL         "Modo"
#define D_WEB_SS_MODE_SINGLE        "Solo una imagen"
#define D_WEB_SS_MODE_CYCLE         "Ciclar entre ranuras"
#define D_WEB_SS_MODE_SHUFFLE       "Mezclar ranuras"
#define D_WEB_SS_SLOTS_POPULATED    "Ranuras pobladas: "
#define D_WEB_SS_SAVE_MODE          "Guardar modo"
#define D_WEB_SS_SLOTS_HEADING      "Ranuras de rotación"
#define D_WEB_SS_SLOT_LABEL         "Ranura"
#define D_WEB_SS_SLOT_EMPTY         "vacía"
#define D_WEB_SS_CONFIRM_DEL_SLOT   "¿Eliminar esta ranura?"
#define D_WEB_SS_DOWNLOAD_ARIA      "Descargar salvapantallas"
#define D_WEB_SS_UPLOAD_ARIA        "Subir salvapantallas .bin"
#define D_WEB_SS_DELETE_ARIA        "Eliminar salvapantallas"
#define D_WEB_SS_ROTATE             "Girar 90\u00b0"
#define D_WEB_SS_SINGLE_HEADING     "Salvapantallas único"
#define D_WEB_SS_SINGLE_ALT         "Salvapantallas único"
#define D_WEB_SS_CONFIRM_DEL_SINGLE "¿Eliminar el salvapantallas único?"
#define D_WEB_SS_NO_SINGLE          "Sin salvapantallas único cargado."
#define D_WEB_SS_EDITOR_HEADING     "Editor"
#define D_WEB_SS_EDITOR_INTRO       "Arrastra una imagen al editor y súbela a una ranura de rotación o como el salvapantallas único heredado. Todas las imágenes se renderizan en 250&times;122 1 bit (3904 bytes)."
#define D_WEB_SS_SOURCE_IMAGE       "Imagen fuente"
#define D_WEB_SS_TOLERANCE          "Tolerancia de negro"
#define D_WEB_SS_INVERT             "Invertir blanco/negro"
#define D_WEB_SS_PRECISE_CONTROL    "Control preciso"
#define D_WEB_SS_ZOOM               "Zoom"
#define D_WEB_SS_MOVE_X             "Mover X"
#define D_WEB_SS_MOVE_Y             "Mover Y"
#define D_WEB_SS_PREVIEW_LABEL      "Vista previa (arrastra para mover, pellizca o usa la rueda para zoom)"
#define D_WEB_SS_RESET_FIT          "Reiniciar ajuste"
#define D_WEB_SS_NO_IMAGE           "Sin imagen cargada"
#define D_WEB_SS_SAVE_TO            "Guardar en"
#define D_WEB_SS_DST_SINGLE         "Salvapantallas único (/sleep.bin)"
#define D_WEB_SS_DST_AUTO_PREFIX    "Próxima ranura libre (ranura "
#define D_WEB_SS_DST_AUTO_SUFFIX    ")"
#define D_WEB_SS_DST_FULL           "(Todas las ranuras llenas)"
#define D_WEB_SS_DST_SLOT_PREFIX    "Ranura de rotación "
#define D_WEB_SS_DST_OVERWRITE      " (sobrescribir)"
#define D_WEB_SS_UPLOAD_EDITED      "Subir imagen editada"

// ----------------------------------------------------------------------------
//  Sincronizacion KOReader (web/kosync.cpp)
//
//  Varias de estas cadenas se insertan en atributos HTML con comillas simples
//  o en un confirm() de JS, por lo que, igual que D_WEB_CONFIRM_*, ninguna
//  puede contener comilla simple ni barra invertida.
// ----------------------------------------------------------------------------
#define D_WEB_KS_TITLE              "Sincronizacion KOReader"
#define D_WEB_KS_SUBTITLE           "Mantenga su posicion de lectura al dia con sus otros dispositivos KOReader."
#define D_WEB_KS_NO_WIFI            "&#9888; No hay redes Wi-Fi guardadas. La sincronizacion necesita una."
#define D_WEB_KS_NO_WIFI_LINK       "Agregar una red"
#define D_WEB_KS_ACCOUNT_HEADING    "Cuenta de sincronizacion"
#define D_WEB_KS_ACCOUNT_INTRO      "Use la misma cuenta que en KOReader. El servidor publico sync.koreader.rocks funciona sin configuracion, o apunte a uno propio."
#define D_WEB_KS_SERVER_LABEL       "Servidor"
#define D_WEB_KS_SERVER_HINT        "Dejelo vacio para usar el predeterminado. Un nombre de host sin esquema se asume https."
#define D_WEB_KS_USER_LABEL         "Usuario"
#define D_WEB_KS_PASS_LABEL         "Contrasena"
#define D_WEB_KS_PASS_KEEP          "sin cambios"
#define D_WEB_KS_PASS_HINT          "En el dispositivo solo se guarda el MD5 de su contrasena, nunca la contrasena. Dejelo vacio para conservar la guardada."
#define D_WEB_KS_ENABLE_LABEL       "Activar sincronizacion"
#define D_WEB_KS_ENABLE_HINT        "Agrega una entrada Sincronizar al menu del lector. La sincronizacion siempre ocurre a peticion, nunca en segundo plano."
#define D_WEB_KS_SAVE_BUTTON        "Guardar"
#define D_WEB_KS_TEST_BUTTON        "Probar conexion"
#define D_WEB_KS_REGISTER_BUTTON    "Registrar"
#define D_WEB_KS_REGISTER_HINT      "Registrar crea una cuenta nueva en el servidor con el usuario y la contrasena de arriba."
#define D_WEB_KS_CONFIRM_CLEAR      "Olvidar la cuenta de sincronizacion guardada?"
#define D_WEB_KS_CLEAR_BUTTON       "Olvidar cuenta"
#define D_WEB_KS_DOCS_HEADING       "Identificadores de libros"
#define D_WEB_KS_DOCS_INTRO         "Cada libro se sincroniza con el mismo identificador que usa KOReader &mdash; un MD5 parcial del archivo original. Se registra automaticamente al subirlo desde esta pagina. Los libros anteriores a esta funcion aparecen como no definidos; vuelva a subirlos o pegue aqui el valor de KOReader."
#define D_WEB_KS_DOCS_EMPTY         "Todavia no hay libros en el dispositivo."
#define D_WEB_KS_DOC_SET            "Identificador registrado"
#define D_WEB_KS_DOC_UNSET          "Sin identificador &mdash; este libro no se sincronizara"
#define D_WEB_KS_DOC_PLACEHOLDER    "32 caracteres hexadecimales"
#define D_WEB_KS_DOC_SAVE_BUTTON    "Guardar"
#define D_WEB_KS_DOC_HINT           "Vacie el campo y guarde para quitar el identificador."
#define D_WEB_KS_MSG_OK             "Conectado. La cuenta funciona."
#define D_WEB_KS_MSG_NOT_CONFIGURED "Complete primero el servidor, el usuario y la contrasena."
#define D_WEB_KS_MSG_NO_NETWORK     "No se pudo contactar al servidor de sincronizacion."
#define D_WEB_KS_MSG_UNAUTHORIZED   "El servidor rechazo el usuario o la contrasena."
#define D_WEB_KS_MSG_NOT_FOUND      "El servidor aun no tiene progreso guardado para este libro."
#define D_WEB_KS_MSG_TAKEN          "Ese nombre de usuario ya esta registrado."
#define D_WEB_KS_MSG_SERVER_ERROR   "El servidor de sincronizacion devolvio un error"
#define D_WEB_KS_MSG_CLEARED        "Cuenta de sincronizacion olvidada."
#define D_WEB_KS_MSG_NEED_BOTH      "Ingrese usuario y contrasena."
#define D_WEB_KS_MSG_REGISTERED     "Cuenta creada y guardada."
#define D_WEB_KS_MSG_SAVED          "Ajustes de sincronizacion guardados."
#define D_WEB_KS_MSG_DOC_CLEARED    "Identificador eliminado."
#define D_WEB_KS_MSG_DOC_SAVED      "Identificador guardado."
#define D_WEB_KS_ERR_BAD_HASH       "El identificador debe tener 32 caracteres hexadecimales."
#define D_WEB_KS_ERR_BAD_MAP        "Mapa de estructura rechazado."

// ----------------------------------------------------------------------------
//  Pantalla de sincronizacion (ui/screens/sync_screen.cpp)
// ----------------------------------------------------------------------------
#define D_SYNC_HEADER               "Sincronizar"
#define D_SYNC_NOT_CONFIGURED_L1    "Sincronizacion sin configurar."
#define D_SYNC_NOT_CONFIGURED_L2    "Configurela en la web."
#define D_SYNC_NO_DOC_L1            "Libro sin id de sincronizacion."
#define D_SYNC_NO_DOC_L2            "Vuelva a subirlo desde la web."
#define D_SYNC_NO_CREDS             "Sin credenciales Wi-Fi."
#define D_SYNC_CONNECTING           "Conectando a Wi-Fi..."
#define D_SYNC_CONN_FAILED          "Fallo la conexion Wi-Fi."
#define D_SYNC_WORKING              "Sincronizando..."
#define D_SYNC_UP_TO_DATE           "Sincronizado"
#define D_SYNC_HERE_FMT             "Aqui: %d%%"
#define D_SYNC_OTHER_FMT            "Otro: %d%%"
#define D_SYNC_ACTION_JUMP          "Ir al otro dispositivo"
#define D_SYNC_ACTION_KEEP          "Mantener esta posicion"
#define D_SYNC_JUMPED               "Posicion adoptada"
#define D_SYNC_JUMPED_SHORT_L1     "No se alcanzó esa página."
#define D_SYNC_JUMPED_SHORT_L2     "El otro dispositivo no cambió."
#define D_SYNC_FAILED               "Fallo la sincronizacion"
#define D_SYNC_ERR_AUTH             "Revise usuario / clave"
#define D_SYNC_ERR_SERVER           "Error del servidor"
#define D_SYNC_HINT_CHOOSE          "1x mover  2x elegir  3x atras"
#define D_SYNC_HINT_EXIT            "cualquier pulsacion: atras"
#define D_MENU_READER_SYNC          "Sincronizar"

// ----------------------------------------------------------------------------
//  Redes Wi-Fi guardadas (web/wifi.cpp)
//
//  D_WEB_WIFI_CONFIRM_FORGET se inserta en un confirm() de JS y el resto en
//  atributos HTML con comillas simples, por lo que ninguna puede contener
//  comilla simple ni barra invertida.
// ----------------------------------------------------------------------------
#define D_WEB_WIFI_TITLE            "Redes Wi-Fi"
#define D_WEB_WIFI_SUBTITLE         "Redes a las que el dispositivo se conecta para subir libros, actualizar y sincronizar."
#define D_WEB_WIFI_SAVED_HEADING    "Redes guardadas"
#define D_WEB_WIFI_SAVED_INTRO      "El dispositivo prueba la ultima que uso, luego busca y se conecta a la red guardada mas fuerte que encuentre. El orden de abajo no es un orden de prioridad."
#define D_WEB_WIFI_NONE             "Todavia no hay redes guardadas."
#define D_WEB_WIFI_SECURED          "Contrasena guardada"
#define D_WEB_WIFI_OPEN             "Red abierta"
#define D_WEB_WIFI_LAST_USED        "usada por ultima vez"
#define D_WEB_WIFI_FORGET_BUTTON    "Olvidar"
#define D_WEB_WIFI_CONFIRM_FORGET   "Olvidar esta red?"
#define D_WEB_WIFI_ADD_HEADING      "Agregar una red"
#define D_WEB_WIFI_ADD_INTRO        "Escriba el nombre de la red tal como aparece, incluidas las mayusculas. Agregar un nombre ya guardado reemplaza su contrasena."
#define D_WEB_WIFI_SSID_LABEL       "Nombre de la red"
#define D_WEB_WIFI_PASS_LABEL       "Contrasena"
#define D_WEB_WIFI_PASS_HINT        "Dejelo vacio para una red abierta. Las contrasenas guardadas nunca se muestran en esta pagina."
#define D_WEB_WIFI_ADD_BUTTON       "Agregar red"
#define D_WEB_WIFI_CAPACITY_HINT    "Hasta 5 redes. Agregar una sexta reemplaza la mas antigua."
#define D_WEB_WIFI_MSG_ADDED        "Red agregada."
#define D_WEB_WIFI_MSG_ADDED_EVICTED "Red agregada. Se elimino la red guardada mas antigua para hacer espacio."
#define D_WEB_WIFI_MSG_UPDATED      "Contrasena actualizada para esa red."
#define D_WEB_WIFI_MSG_FORGOTTEN    "Red olvidada."
#define D_WEB_WIFI_ERR_NO_SSID      "Ingrese un nombre de red."
#define D_WEB_WIFI_ERR_SSID_LONG    "Ese nombre de red es demasiado largo."
#define D_WEB_WIFI_ERR_PASS_LONG    "Esa contrasena es demasiado larga."

#endif  // PALA_LANG_ES_LA_H
