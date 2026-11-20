/* SPDX-License-Identifier: MIT */
/*
 * graceui.h: GraceUI's public C ABI (D73, D109), the one way any mod reaches GraceUI (invariant 15). This header is the
 * ABI's source: graceui.dll is built from it, and its first form, ABI 1.0, is kept as src/grace-abi/baseline/
 * graceui-abi1-0.h, against which every later header may only grow (D132).
 *
 * ABI 1 is a preview until GraceUI 1.1 opens the framework to other mods (G18): until then a mod pins GraceUI by its
 * release.
 *
 * The rules (D73). Plain C, x64 only, default packing. Every struct a mod passes starts with its size: set it to sizeof.
 * GraceUI reads the smaller of the two sizes, takes the rest as zero, and refuses a struct smaller than this header's
 * first form of it; of a struct it fills, it writes the smaller of the two sizes. Strings are UTF-8 with a length (no
 * NUL needed) and are copied before the call returns. Nothing is freed across the boundary, and no call throws: every
 * slot answers a status.
 *
 * Finding GraceUI (D70): from a thread of the mod's own, never in DllMain; graceui_sys.h does it. GraceUI_RequestApi(1)
 * then answers ABI 1's table, GraceUiApi1, or null.
 *
 * The compatibility rule (D74), the only one: a mod works with every GraceUI of the ABI major it was built for whose
 * version is at least the floor it names in connect. It calls a slot only if the table's size covers it
 * (GRACEUI_API1_COVERS) and uses a feature only if has_cap names it; a later minor only appends. A new major is a new
 * table from the same export.
 */
#ifndef GRACEUI_H
#define GRACEUI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(GRACEUI_BUILDING_DLL)
#define GRACEUI_API __declspec(dllexport)
#elif defined(_WIN32)
#define GRACEUI_API __declspec(dllimport)
#else
#define GRACEUI_API
#endif

/* GraceUI's version as three numbers packed in one: major (10 bits), minor (10 bits) and patch (12 bits). The macros
 * work in unsigned arithmetic without a cast, so C++ code built with -Wold-style-cast may use them too. */
#define GRACEUI_VERSION_PACK(major, minor, patch) \
  (((0u + (major)) << 22) | ((0u + (minor)) << 12) | (0u + (patch)))
#define GRACEUI_VERSION_MAJOR(v) ((0u + (v)) >> 22)
#define GRACEUI_VERSION_MINOR(v) (((0u + (v)) >> 12) & 0x3FFu)
#define GRACEUI_VERSION_PATCH(v) ((0u + (v)) & 0xFFFu)

/* The ABI major this header describes, and its minor: the number of the newest appends (0: the first table). */
#define GRACEUI_ABI_MAJOR 1u
#define GRACEUI_ABI1_MINOR 0u

/*
 * GraceUI's own version (GRACEUI_VERSION_PACK). Needs no start: callable from any thread, at any time after the DLL is
 * loaded, and it starts nothing.
 */
GRACEUI_API uint32_t GraceUI_Version(void);

/*
 * The table of ABI major `major` (a const GraceUiApi1* for 1), or null. The first call in the process takes GraceUI's
 * single-instance mutex and, in the copy that takes it, runs the anti-cheat check and starts the host: a short-lived
 * thread that waits for the game's window and installs the overlay's hooks (D71, D72, D133). A second copy of GraceUI
 * loaded from another folder starts nothing and answers null to every call, its first included (invariant 18). Never
 * call it from DllMain.
 */
GRACEUI_API const void* GraceUI_RequestApi(uint32_t major);

/* The two exports' types, for GetProcAddress. */
typedef uint32_t (*GraceUiVersionFn)(void);
typedef const void* (*GraceUiRequestApiFn)(uint32_t major);

/* ---- Statuses: what every slot answers. */

typedef uint32_t GraceUiStatus;

#define GRACEUI_OK 0u
#define GRACEUI_TOO_OLD 1u       /* connect: this GraceUI is older than the mod's floor; the answer names both versions */
#define GRACEUI_DUPLICATE_ID 2u  /* connect: a connected mod has this id already */
#define GRACEUI_INACTIVE 3u      /* GraceUI is inactive for this session (connect's answer says why): calls do nothing,
                                    but notice, in the anti-cheat's notice-only mode (D88) */
#define GRACEUI_REFUSED 4u       /* an argument failed its check (connect's answer says which); nothing was done */
#define GRACEUI_NOT_CONNECTED 5u /* the handle names no connected mod */
#define GRACEUI_NOT_YET 6u       /* the slot is declared, but this GraceUI does not provide it yet (has_cap) */
#define GRACEUI_FULL 7u          /* a bounded store has no room: this mod's notices, panels, textures or faces, or the
                                    session's */
#define GRACEUI_FAULT 8u         /* a fault inside GraceUI: nothing was done, and GraceUI's log has a line (D110) */
#define GRACEUI_NOT_READY 9u     /* not yet possible in this session: ask again later (glyph_table before the shared
                                    atlas is baked at the overlay's warm-up) */
#define GRACEUI_TOO_LATE 10u     /* past its time: a texture after the mod's registration window (its first submit),
                                    a face after warm-up */
#define GRACEUI_UNBOUND 11u      /* register_hotkey: the hotkey is registered, with its id, but its default is taken (by
                                    another hotkey or the menu key) or it has none: it fires nothing until the player
                                    binds it (D164) */

/* Why connect answered GRACEUI_INACTIVE. */
#define GRACEUI_INACTIVE_ANTI_CHEAT 1u  /* an anti-cheat was found before GraceUI hooked: notices only (D88) */
#define GRACEUI_INACTIVE_HOST_FAILED 2u /* the overlay could not start (no game window, a hook refused) */
#define GRACEUI_INACTIVE_RENDER_DEAD 3u /* the overlay turned itself off after a fault: nothing more is drawn */

/* Why connect answered GRACEUI_REFUSED. */
#define GRACEUI_REFUSED_ARGUMENT 1u /* a null pointer, or a struct smaller than its first size */
#define GRACEUI_REFUSED_ID 2u       /* the id is not 3 to 32 of a-z, 0-9 and '-' */
#define GRACEUI_REFUSED_NAME 3u     /* the display name is empty, longer than 64 bytes, or not UTF-8 text */
#define GRACEUI_REFUSED_CAPS 4u     /* the caps are longer than 256 bytes, or not cap names separated by spaces */
#define GRACEUI_REFUSED_TOO_MANY 5u /* GRACEUI_MODS_MAX mods are connected */

/* ---- Limits. */

#define GRACEUI_MODS_MAX 32u           /* mods connected at once */
#define GRACEUI_ID_MAX 32u             /* bytes of a mod's id */
#define GRACEUI_NAME_MAX 64u           /* bytes of a display name */
#define GRACEUI_CAPS_MAX 256u          /* bytes of a mod's caps */
#define GRACEUI_NOTICE_TEXT_MAX 512u   /* bytes of a notice's text */
#define GRACEUI_NOTICES_PER_MOD 4u     /* a mod's notices showing at once */
#define GRACEUI_PANELS_MAX 64u         /* panels in the session (EST) */
#define GRACEUI_PANELS_PER_MOD 16u     /* a mod's panels (EST: EldenNet's surfaces are eight or more) */
#define GRACEUI_TEXTURES_MAX 64u       /* textures in the session (EST) */
#define GRACEUI_TEXTURES_PER_MOD 8u    /* a mod's textures (EST) */
#define GRACEUI_TEXTURE_BYTES_PER_MOD 8388608u /* a mod's textures' pixels, 4 bytes each (EST: 8 MiB) */
#define GRACEUI_TEXTURE_SIDE_MAX 4096u /* a texture's width and height */
#define GRACEUI_FACES_PER_MOD 2u       /* a mod's faces in the shared atlas (EST; EldenNet's surfaces more, D80) */
#define GRACEUI_FACE_BYTES_MAX 4194304u /* a face's font file (EST: 4 MiB) */
#define GRACEUI_FACE_RANGES_MAX 64u    /* a face's code-point ranges */
#define GRACEUI_DRAW_VERTEX_BYTES_MAX 262144u /* a submission's vertices (D76, EST: 256 KiB, 13,107 vertices) */
#define GRACEUI_DRAW_INDEX_BYTES_MAX 262144u  /* a submission's indices (EST: 256 KiB) */
#define GRACEUI_DRAW_COMMANDS_MAX 64u  /* a submission's commands (D76, EST) */
#define GRACEUI_DRAW_COMMAND_SIZE_MAX 256u /* a submission's command stride, in bytes (EST): a larger one is refused */
#define GRACEUI_HOTKEYS_PER_MOD 8u     /* a mod's hotkeys (EST) */
#define GRACEUI_HOTKEYS_MAX 64u        /* hotkeys in the session (EST) */

/* ---- Capabilities: the features has_cap names. */

#define GRACEUI_CAP_NOTICES "notices" /* notice: since GraceUI 0.1 */
#define GRACEUI_CAP_PANELS "panels"   /* create_panel, submit, show_panel, hide_panel: since GraceUI 0.1 */
#define GRACEUI_CAP_TEXTURES "textures" /* register_texture: since GraceUI 0.1 */
#define GRACEUI_CAP_FACES "faces"     /* register_face, glyph_table: since GraceUI 0.1 */
#define GRACEUI_CAP_INPUT "input"     /* request_input, release_input: since GraceUI 0.1 */
#define GRACEUI_CAP_WORLD_STATE "world-state" /* set_world_state: since GraceUI 0.1 */
#define GRACEUI_CAP_HOTKEYS "hotkeys" /* register_hotkey, and poll's hotkey events: since GraceUI 0.1 (D155) */

/* A connected mod's handle, which every slot after connect takes; 0 is none. */
typedef uint64_t GraceUiConsumer;

/* ---- connect (D73). */

typedef struct GraceUiConnectRequest {
  uint32_t size;          /* sizeof(GraceUiConnectRequest) */
  uint32_t abi_minor;     /* GRACEUI_ABI1_MINOR as the mod was built */
  uint32_t version;       /* the mod's own version, GRACEUI_VERSION_PACK */
  uint32_t floor;         /* the oldest GraceUI it works with, GRACEUI_VERSION_PACK; 0 for any */
  const char* id;         /* 3 to 32 of a-z, 0-9 and '-', unique among the mods connected */
  uint64_t id_length;
  const char* name;       /* its display name, which players see: 1 to 64 bytes of UTF-8 */
  uint64_t name_length;
  const char* caps;       /* the GraceUI features it uses: cap names separated by spaces, or empty */
  uint64_t caps_length;
} GraceUiConnectRequest;

typedef struct GraceUiConnectAnswer {
  uint32_t size;             /* set by the caller to sizeof(GraceUiConnectAnswer) */
  GraceUiStatus status;      /* what connect answered: GRACEUI_OK, _TOO_OLD, _DUPLICATE_ID, _INACTIVE or _REFUSED */
  uint32_t reason;           /* with GRACEUI_INACTIVE a GRACEUI_INACTIVE_*, with GRACEUI_REFUSED a GRACEUI_REFUSED_* */
  uint32_t graceui_version;  /* this GraceUI's version */
  uint32_t floor;            /* with GRACEUI_TOO_OLD: the version the mod needs */
  uint32_t abi_minor;        /* the ABI minor this GraceUI serves */
  GraceUiConsumer consumer;  /* with GRACEUI_OK or GRACEUI_INACTIVE: the mod's handle; else 0 */
} GraceUiConnectAnswer;

/* ---- notice: a short text GraceUI shows above everything else (the notice layer, D76), in game. */

#define GRACEUI_TONE_NORMAL 0u  /* a gold accent */
#define GRACEUI_TONE_WARNING 1u /* ember */
#define GRACEUI_TONE_DANGER 2u  /* rot */

typedef struct GraceUiNotice {
  uint32_t size;         /* sizeof(GraceUiNotice) */
  uint32_t tone;         /* GRACEUI_TONE_* */
  uint32_t key;          /* 0: a new notice. Otherwise the mod's own key: a notice with the key of one showing replaces
                            it in place, and one with an empty text withdraws it */
  uint32_t duration_ms;  /* how long it shows, from the call; 0 until it is replaced or withdrawn (a banner) */
  const char* text;      /* 1 to GRACEUI_NOTICE_TEXT_MAX bytes of UTF-8; empty, with a key, to withdraw */
  uint64_t text_length;
} GraceUiNotice;

/* ---- Textures, faces and glyphs (D76, D80): registered once, in the mod's registration window.
 *
 * A texture is registered from the mod's connect until its first submit (its registration window), within its budget
 * (GRACEUI_TEXTURES_PER_MOD, GRACEUI_TEXTURE_BYTES_PER_MOD): its pixels are copied before the call returns, and GraceUI
 * uploads it once, at the overlay's warm-up or, for a mod that comes later, at its next frame; a command that names it
 * is drawn once it is on the GPU. A face joins the one shared atlas, which GraceUI bakes once at warm-up and never
 * grows: register_face is taken only before warm-up (GRACEUI_TOO_LATE after it). glyph_table then gives any face's
 * glyphs, GraceUI's own among them (face 1: the notices' face), for a mod to lay out its own text in its draw lists. */

#define GRACEUI_TEXTURE_RGBA8 1u /* four bytes a pixel, R first, straight alpha */

typedef struct GraceUiTextureDesc {
  uint32_t size;
  uint32_t format;     /* GRACEUI_TEXTURE_RGBA8 */
  uint32_t width;
  uint32_t height;
  uint32_t row_bytes;  /* from one row's start to the next's: at least width * 4 */
  uint32_t reserved;   /* 0 */
  const void* pixels;  /* copied before the call returns */
} GraceUiTextureDesc;

typedef struct GraceUiFaceDesc {
  uint32_t size;
  uint32_t pixel_height;   /* the face's size at a scale of 1 */
  const void* data;        /* a TrueType or OpenType font file, copied before the call returns */
  uint64_t data_length;
  const uint32_t* ranges;  /* the code points to bake, as pairs of first and last; null for GraceUI's default */
  uint64_t range_count;    /* pairs */
} GraceUiFaceDesc;

typedef struct GraceUiGlyph {
  uint32_t code_point;
  float advance;
  float x0, y0, x1, y1;  /* the glyph's quad from the pen's position, in pixels */
  float u0, v0, u1, v1;  /* its place in the atlas */
} GraceUiGlyph;

typedef struct GraceUiGlyphTable {
  uint32_t size;               /* set by the caller to sizeof(GraceUiGlyphTable) */
  uint32_t generation;         /* the shared atlas's generation: a draw list built from this table names it */
  uint32_t face_id;
  uint32_t glyph_count;
  float pixel_height;
  float ascent;
  float descent;
  float line_gap;
  uint64_t atlas;              /* the atlas's texture id, for draw commands */
  const GraceUiGlyph* glyphs;  /* GraceUI's, read-only, sorted by code point: valid while `generation` is current */
} GraceUiGlyphTable;
/* A glyph's quad is measured from the pen at the top of the line, as Dear ImGui's are: a line's baseline is at the pen's
 * y plus `ascent`; `descent` is below the baseline, negative as fonts give it; the next line is `pixel_height` below.
 * In GraceUI 0.1 the atlas is never rebuilt, so a table stays valid for the session. */

/* ---- Panels and draw lists (D76).
 *
 * A panel is a rectangle of the display a mod draws itself, on a layer, hidden until show_panel. Its draw lists are in
 * the panel's own pixels, its top-left corner at 0,0, whatever its place on the display: GraceUI moves them where the
 * anchor puts the panel, and every clip rectangle must lie within the panel's width and height, so nothing is drawn
 * outside it. submit copies the list before it returns and checks it (counts, each command's ranges, every index
 * within its command's own vertices, clip rectangles, texture ids, the atlas's generation, the size bounds); a list
 * that fails is refused (GRACEUI_REFUSED), logged once, and the panel keeps its last list. */

#define GRACEUI_LAYER_HUD 1u    /* the layers a mod's panel takes: hud < panel < (GraceUI's menu) < modal < (notices) */
#define GRACEUI_LAYER_PANEL 2u
#define GRACEUI_LAYER_MODAL 4u

#define GRACEUI_ANCHOR_TOP_LEFT 0u /* the point of the display a panel's bounds are measured from */
#define GRACEUI_ANCHOR_TOP 1u
#define GRACEUI_ANCHOR_TOP_RIGHT 2u
#define GRACEUI_ANCHOR_LEFT 3u
#define GRACEUI_ANCHOR_CENTRE 4u
#define GRACEUI_ANCHOR_RIGHT 5u
#define GRACEUI_ANCHOR_BOTTOM_LEFT 6u
#define GRACEUI_ANCHOR_BOTTOM 7u
#define GRACEUI_ANCHOR_BOTTOM_RIGHT 8u

#define GRACEUI_PANEL_TAKES_INPUT 1u /* the panel may take input (request_input) */

typedef struct GraceUiPanelDesc {
  uint32_t size;
  uint32_t layer;   /* GRACEUI_LAYER_* */
  uint32_t anchor;  /* GRACEUI_ANCHOR_* */
  uint32_t flags;   /* GRACEUI_PANEL_* */
  float x;          /* the panel's bounds from its anchor, in the display's pixels: x and y move it inwards from the
                       anchor's edges (from the centre, rightwards and downwards, for a centred anchor); width and
                       height from 1 to 16384 */
  float y;
  float width;
  float height;
} GraceUiPanelDesc;

/* Dear ImGui's default ImDrawVert, as D76 fixes it: position, texture coordinate, and colour as bytes R, G, B, A. */
typedef struct GraceUiDrawVert {
  float x;
  float y;
  float u;
  float v;
  uint32_t colour;
} GraceUiDrawVert;

typedef struct GraceUiDrawCommand {
  float clip_x0;           /* the clip rectangle, in the panel's pixels: within its width and height */
  float clip_y0;
  float clip_x1;
  float clip_y1;
  uint64_t texture;        /* register_texture's id, or the shared atlas's (glyph_table) */
  uint32_t vertex_offset;  /* the command's vertices: at most 65,536 from here */
  uint32_t vertex_count;
  uint32_t index_offset;   /* its indices, each relative to vertex_offset */
  uint32_t index_count;
} GraceUiDrawCommand;

typedef struct GraceUiDrawList {
  uint32_t size;
  uint32_t atlas_generation;  /* the shared atlas's generation the list was built against, if it draws glyphs */
  uint32_t vertex_count;
  uint32_t index_count;
  uint32_t command_count;
  uint32_t command_size;      /* sizeof(GraceUiDrawCommand) as the mod was built: the commands' stride */
  const GraceUiDrawVert* vertices;
  const uint16_t* indices;
  const GraceUiDrawCommand* commands;
} GraceUiDrawList;

/* ---- Input (D77): one holder in the process at a time, answered at once. */

#define GRACEUI_HOLDER_MENU 1u  /* GraceUI's menu (never a mod's request) */
#define GRACEUI_HOLDER_PANEL 2u
#define GRACEUI_HOLDER_MODAL 3u

#define GRACEUI_WANT_KEYBOARD 1u
#define GRACEUI_WANT_MOUSE 2u
#define GRACEUI_WANT_PAD 4u

#define GRACEUI_INPUT_GRANTED 0u
#define GRACEUI_INPUT_REFUSED 1u  /* another holds input: the answer names it */
#define GRACEUI_INPUT_NOT_NOW 2u  /* the answer says why */

#define GRACEUI_NOT_NOW_NOT_READY 1u /* the overlay cannot show the holder's view yet */
#define GRACEUI_NOT_NOW_DANGER 2u
#define GRACEUI_NOT_NOW_LOADING 3u
#define GRACEUI_NOT_NOW_GAME_MENU 4u

typedef struct GraceUiInputRequest {
  uint32_t size;
  uint32_t kind;      /* GRACEUI_HOLDER_PANEL or GRACEUI_HOLDER_MODAL */
  uint32_t wants;     /* GRACEUI_WANT_* */
  uint32_t reserved;  /* 0 */
  uint64_t panel;     /* the panel whose view takes the input (create_panel's id): the mod's own, shown, made with
                         GRACEUI_PANEL_TAKES_INPUT, on the modal layer for GRACEUI_HOLDER_MODAL and on another for
                         GRACEUI_HOLDER_PANEL */
} GraceUiInputRequest;

typedef struct GraceUiInputAnswer {
  uint32_t size;         /* set by the caller to sizeof(GraceUiInputAnswer) */
  uint32_t answer;       /* GRACEUI_INPUT_* */
  uint32_t why;          /* with GRACEUI_INPUT_NOT_NOW: GRACEUI_NOT_NOW_* */
  uint32_t holder_kind;  /* with GRACEUI_INPUT_REFUSED: the holder's GRACEUI_HOLDER_* */
  char holder[40];       /* with GRACEUI_INPUT_REFUSED: the holder's mod id, NUL-terminated ("graceui": the menu) */
} GraceUiInputAnswer;

/* ---- Hotkeys (D77, D155): an action with a default key and modifiers or a pad chord; GraceUI's Hotkeys page rebinds it.
 *
 * GraceUI watches for the hotkey in the game's window procedure: a fresh press of its key with exactly its modifiers,
 * while no holder has input, is one press, which the mod takes through poll (GRACEUI_EVENT_HOTKEY) on its own thread.
 * The press, its repeats, characters and release never reach the game; a key bound with no modifier is never the
 * game's while it is bound, through DirectInput as well. A default already taken (by another hotkey, or the menu key)
 * registers unbound: the hotkey exists and fires nothing until the player binds it (GraceUI 0.6's Hotkeys page), and
 * GraceUI's log names it. GraceUI 0.1 takes keys alone: a pad chord answers GRACEUI_NOT_YET. */

#define GRACEUI_MOD_CTRL 1u
#define GRACEUI_MOD_SHIFT 2u
#define GRACEUI_MOD_ALT 4u

typedef struct GraceUiHotkeyDesc {
  uint32_t size;
  uint32_t key;          /* the default key, a Windows virtual key (not Escape, a modifier or a mouse button); 0 for
                            none */
  uint32_t modifiers;    /* GRACEUI_MOD_*; Alt with F4 is refused */
  uint32_t pad_chord;    /* the default pad chord, XInput's button bits; 0 for none (GraceUI 0.1: 0 only) */
  const char* action;    /* the action's id within the mod: 1 to 32 of a-z, 0-9 and '-', not the mod's already */
  uint64_t action_length;
  const char* name;      /* what the Hotkeys page calls it: 1 to 64 bytes of UTF-8 */
  uint64_t name_length;
} GraceUiHotkeyDesc;

/* ---- poll's events (D155): what GraceUI has for a mod, one at a time, on the mod's own thread. GraceUI 0.1 gives
 * hotkey presses; the bridge protocol (§6, GraceUI 0.2) adds its own kinds, and their fields after these. */

#define GRACEUI_EVENT_NONE 0u   /* nothing is waiting */
#define GRACEUI_EVENT_HOTKEY 1u /* a hotkey of the mod's was pressed */

typedef struct GraceUiEvent {
  uint32_t size;       /* set by the caller to sizeof(GraceUiEvent) */
  uint32_t kind;       /* GRACEUI_EVENT_* */
  uint32_t hotkey_id;  /* with GRACEUI_EVENT_HOTKEY: register_hotkey's id */
  uint32_t count;      /* with GRACEUI_EVENT_HOTKEY: its presses since its last event, 1 or more */
} GraceUiEvent;

/* ---- World-state providers (D78): every claim of every provider is combined. */

#define GRACEUI_WORLD_DANGER 1u
#define GRACEUI_WORLD_LOADING 2u
#define GRACEUI_WORLD_GAME_MENU 4u   /* a game menu, or the game's own text entry, is open */
#define GRACEUI_WORLD_TITLE 8u       /* the title screen */
#define GRACEUI_WORLD_MULTIPLAYER 16u
#define GRACEUI_WORLD_ANTI_CHEAT 32u /* the provider found an anti-cheat: the browser view closes for the session (D81) */

typedef struct GraceUiWorldState {
  uint32_t size;
  uint32_t flags;  /* GRACEUI_WORLD_* */
} GraceUiWorldState;

/* ---- log. */

#define GRACEUI_LOG_INFO 0u
#define GRACEUI_LOG_WARNING 1u
#define GRACEUI_LOG_ERROR 2u

/* A slot whose place is kept for a later increment, which gives it its type: until then it answers GRACEUI_NOT_YET, and
 * a mod never calls it. */
typedef GraceUiStatus (*GraceUiReservedSlot)(GraceUiConsumer consumer, const void* reserved);

/*
 * ABI 1's table (D73, D131), in D73's order. Each slot answers GRACEUI_NOT_CONNECTED for a handle that names no
 * connected mod, then GRACEUI_INACTIVE while GraceUI is inactive (notice excepted in notice-only mode), then
 * GRACEUI_NOT_YET where this GraceUI does not provide it. GraceUI 0.1 provides every slot but three of the bridge
 * protocol's four (register_pages, publish, answer) and log, which are declared for the increments that build them;
 * its poll gives hotkey events alone (D155).
 */
typedef struct GraceUiApi1 {
  uint32_t size;             /* the table's size in this GraceUI: a slot exists only if it ends within it */
  uint32_t abi_minor;        /* ABI 1's minor in this GraceUI */
  uint32_t graceui_version;  /* GraceUI_Version() */
  uint32_t reserved;         /* 0 */

  /* 1 if this GraceUI provides the feature named (GRACEUI_CAP_*), else 0, whatever the session's state. */
  uint32_t (*has_cap)(const char* name, uint64_t name_length);

  /* Connects a mod: GRACEUI_OK, _TOO_OLD (with a notice naming the mod and both versions, and one log line),
   * _DUPLICATE_ID, _INACTIVE (with the reason) or _REFUSED (with the reason), never silence. With GRACEUI_OK and with
   * GRACEUI_INACTIVE the mod is connected and has its handle; nothing else connects it. */
  GraceUiStatus (*connect)(const GraceUiConnectRequest* request, GraceUiConnectAnswer* answer);

  /* The handle's mod is no longer connected: its notices are withdrawn, and its id is free again. */
  GraceUiStatus (*disconnect)(GraceUiConsumer consumer);

  GraceUiReservedSlot register_pages; /* the bridge protocol's (§6), from GraceUI 0.2 */
  GraceUiReservedSlot publish;
  /* The mod's next event, at once: GRACEUI_OK with one (GRACEUI_EVENT_NONE if nothing is waiting), or _REFUSED for an
   * event that is null or below its first size. Call it until it gives GRACEUI_EVENT_NONE; it never waits (D155). */
  GraceUiStatus (*poll)(GraceUiConsumer consumer, GraceUiEvent* event);
  GraceUiReservedSlot answer;

  /* A texture, in the mod's registration window: GRACEUI_OK with its id, _REFUSED (a format, size or row length
   * GraceUI does not take), _FULL (over the mod's budget or the session's) or _TOO_LATE (after its first submit). */
  GraceUiStatus (*register_texture)(GraceUiConsumer consumer, const GraceUiTextureDesc* texture, uint64_t* texture_id);
  /* A face for the shared atlas, before warm-up: GRACEUI_OK with its id, _REFUSED (not a TrueType or OpenType file,
   * a size outside 6 to 128 pixels, bad ranges), _FULL or _TOO_LATE. */
  GraceUiStatus (*register_face)(GraceUiConsumer consumer, const GraceUiFaceDesc* face, uint32_t* face_id);
  /* A face's glyphs in the shared atlas: GRACEUI_OK, _NOT_READY before warm-up, or _REFUSED for an unknown face. */
  GraceUiStatus (*glyph_table)(GraceUiConsumer consumer, uint32_t face_id, GraceUiGlyphTable* table);
  /* A panel, hidden: GRACEUI_OK with its id, _REFUSED or _FULL (GRACEUI_PANELS_PER_MOD). */
  GraceUiStatus (*create_panel)(GraceUiConsumer consumer, const GraceUiPanelDesc* panel, uint64_t* panel_id);
  /* The panel's next draw list (above): GRACEUI_OK or _REFUSED. The mod's first submit closes its registration
   * window. */
  GraceUiStatus (*submit)(GraceUiConsumer consumer, uint64_t panel_id, const GraceUiDrawList* list);
  GraceUiStatus (*show_panel)(GraceUiConsumer consumer, uint64_t panel_id);
  /* Hiding a panel that holds input ends its holder (released). */
  GraceUiStatus (*hide_panel)(GraceUiConsumer consumer, uint64_t panel_id);
  /* GRACEUI_OK with the answer, given at once (D77): granted, refused naming the holder, or not now with the reason;
   * nothing queues and nothing deposes the holder. The player's exit (a second press of the menu key within 2 s of
   * "<mod> has the controls", or Escape held 1 s) always ends a holder. _REFUSED for a request that names no panel of
   * the mod's that may take input, or a kind that does not suit its layer. */
  GraceUiStatus (*request_input)(GraceUiConsumer consumer, const GraceUiInputRequest* request,
                                 GraceUiInputAnswer* answer);
  /* Gives input back if the mod holds it; GRACEUI_OK either way. */
  GraceUiStatus (*release_input)(GraceUiConsumer consumer);
  /* A hotkey (above): GRACEUI_OK with its id, bound; GRACEUI_UNBOUND with its id, registered but unbound (D164); _REFUSED (a reserved key, an
   * unknown modifier, Alt+F4, an action that is not an id or already the mod's, a name that is not 1 to 64 bytes of
   * UTF-8, a key with no place on the keyboard); _FULL (GRACEUI_HOTKEYS_PER_MOD, or the session's); _NOT_YET for a pad
   * chord. */
  GraceUiStatus (*register_hotkey)(GraceUiConsumer consumer, const GraceUiHotkeyDesc* hotkey, uint32_t* hotkey_id);

  /* Shows, replaces or withdraws one of the mod's notices: GRACEUI_OK, _REFUSED (a text that is not UTF-8, too long,
   * or empty without a key) or _FULL (GRACEUI_NOTICES_PER_MOD showing). Callable in the anti-cheat's notice-only mode,
   * after connect answered GRACEUI_INACTIVE with GRACEUI_INACTIVE_ANTI_CHEAT (D88). */
  GraceUiStatus (*notice)(GraceUiConsumer consumer, const GraceUiNotice* notice);

  /* The mod's claims as a world-state provider (D78), replacing its earlier ones; every provider's are combined, and
   * any provider's danger or loading ends the input holder (invariant 9). GRACEUI_OK or _REFUSED (an unknown flag). */
  GraceUiStatus (*set_world_state)(GraceUiConsumer consumer, const GraceUiWorldState* state);
  GraceUiStatus (*log)(GraceUiConsumer consumer, uint32_t level, const char* text, uint64_t text_length);
} GraceUiApi1;

/* Whether `api`'s size covers `slot` (a member of GraceUiApi1): call a slot only then. */
#define GRACEUI_API1_COVERS(api, slot) \
  ((api) != NULL && (api)->size >= offsetof(GraceUiApi1, slot) + sizeof((api)->slot))

#ifdef __cplusplus
}
#endif

#endif /* GRACEUI_H */
