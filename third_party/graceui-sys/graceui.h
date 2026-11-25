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
#define GRACEUI_ABI1_MINOR 8u /* 1: a holder's events and set_caret (GraceUI 0.2.1, D176); 2: the display and scaled
                                 faces (GraceUI 0.2.2, D180); 3: pad chords (GraceUI 0.2.14, D204); 4: a hotkey's
                                 binding as the player set it (GraceUI 0.2.16, D210); 5: a wait handle for the mod's
                                 events (GraceUI 0.2.17, D212); 6: an uploaded file with its command (GraceUI 0.2.26,
                                 D226); 7: what pressed a hotkey, its key or its pad chord (GraceUI 0.2.38, D243); 8: a
                                 panel's drawing each frame, on the present thread (GraceUI 0.2.52, D259) */

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
#define GRACEUI_FACES_PER_MOD 2u       /* a mod's faces in the shared atlas (EST), but EldenNet's: */
#define GRACEUI_FACES_ELDENNET 8u      /* the mod whose id is "eldennet": its surfaces' faces (EST; D80, D139) */
#define GRACEUI_FACES_ALL_MODS 24u     /* every mod's faces together, after which register_face answers full (EST) */
#define GRACEUI_FACE_BYTES_MAX 4194304u /* a face's font file (EST: 4 MiB) */
#define GRACEUI_FACE_RANGES_MAX 64u    /* a face's code-point ranges */
#define GRACEUI_DRAW_VERTEX_BYTES_MAX 262144u /* a submission's vertices (D76, EST: 256 KiB, 13,107 vertices) */
#define GRACEUI_DRAW_INDEX_BYTES_MAX 262144u  /* a submission's indices (EST: 256 KiB) */
#define GRACEUI_DRAW_COMMANDS_MAX 64u  /* a submission's commands (D76, EST) */
#define GRACEUI_DRAW_COMMAND_SIZE_MAX 256u /* a submission's command stride, in bytes (EST): a larger one is refused */
#define GRACEUI_HOTKEYS_PER_MOD 8u     /* a mod's hotkeys (EST) */
#define GRACEUI_HOTKEYS_MAX 64u        /* hotkeys in the session (EST) */
#define GRACEUI_PRESENT_HOOKS_PER_MOD 4u /* a mod's panels drawn each frame by set_on_present (ABI 1.8, D259; EST) */
#define GRACEUI_PRESENT_HOOKS_MAX 8u     /* ... and in the session (EST) */
#define GRACEUI_PRESENT_BUDGET_US 500u   /* a mod's on_present callbacks together, each frame (D71, EST: 0.5 ms) */
#define GRACEUI_PRESENT_OVER_FRAMES 120u /* frames running over that budget before they are suspended (D71) */

/* ---- Capabilities: the features has_cap names. */

#define GRACEUI_CAP_NOTICES "notices" /* notice: since GraceUI 0.1 */
#define GRACEUI_CAP_PANELS "panels"   /* create_panel, submit, show_panel, hide_panel: since GraceUI 0.1 */
#define GRACEUI_CAP_TEXTURES "textures" /* register_texture: since GraceUI 0.1 */
#define GRACEUI_CAP_FACES "faces"     /* register_face, glyph_table: since GraceUI 0.1 */
#define GRACEUI_CAP_INPUT "input"     /* request_input, release_input: since GraceUI 0.1 */
#define GRACEUI_CAP_WORLD_STATE "world-state" /* set_world_state: since GraceUI 0.1 */
#define GRACEUI_CAP_HOTKEYS "hotkeys" /* register_hotkey, and poll's hotkey events: since GraceUI 0.1 (D155) */
#define GRACEUI_CAP_PAGES "pages"     /* register_pages, publish, answer, and poll's protocol events: since 0.2 (D169) */
#define GRACEUI_CAP_HOLDER_EVENTS "holder-events" /* GRACEUI_INPUT_EVENTS, poll's holder events, set_caret: since
                                                    0.2.1, ABI 1.1 (D176) */
#define GRACEUI_CAP_DISPLAY "display" /* connect's display fields, GRACEUI_EVENT_DISPLAY, GRACEUI_FACE_SCALED: since 0.2.2,
                                        ABI 1.2 (D180) */
#define GRACEUI_CAP_PAD_CHORDS "pad-chords" /* register_hotkey's pad_chord: since 0.2.14, ABI 1.3 (D204) */
#define GRACEUI_CAP_HOTKEY_BINDINGS "hotkey-bindings" /* describe_hotkey, GRACEUI_EVENT_HOTKEY_CHANGED: since 0.2.16, ABI
                                                        1.4 (D210) */
#define GRACEUI_CAP_WAKE "wake" /* wake_event: since 0.2.17, ABI 1.5 (D212) */
#define GRACEUI_CAP_HOTKEY_BY "hotkey-by" /* GraceUiEvent.hotkey_by: since 0.2.38, ABI 1.7 (D243) */
#define GRACEUI_CAP_ON_PRESENT "on-present" /* set_on_present: since 0.2.52, ABI 1.8 (D259) */

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
  /* From ABI 1.2 (D180): the display as it stands at connect (a mod sizes its faces for the registration window). */
  float ui_scale;            /* GraceUI's ui.scale (Menu size): the player's, kept from the last session, or its default */
  float ui_scale_default;    /* its default: a mod's content scale is (H / 1080) x ui_scale / ui_scale_default */
  uint32_t display_width;    /* the back buffer's size; 0 until the game's swap chain is bound (often so at connect) */
  uint32_t display_height;
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
  /* From ABI 1.2 (D180). */
  uint32_t flags;          /* GRACEUI_FACE_* */
  uint32_t reserved;       /* 0 */
} GraceUiFaceDesc;

/* A scaled face: baked at warm-up at pixel_height x the content scale, (H / 1080) x ui.scale / its default, rounded to a
 * whole pixel (6 to 128 px),
 * so a mod designs at 1080 lines and the default Menu size and need not know the display when it registers. Its glyph
 * table gives the size it was baked at; GRACEUI_EVENT_DISPLAY's faces_scale says the scale. The atlas is baked once
 * (invariant 11): after a later change of the display or of ui.scale, the mod draws its glyphs scaled by
 * content_scale / faces_scale, a little soft, until the next session. Without the flag a face is baked at pixel_height. */
#define GRACEUI_FACE_SCALED 1u

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

/* ---- A panel's drawing each frame (ABI 1.8, D259; D71): an opt-in callback on the present thread.
 *
 * A panel is drawn from the newest list its mod submitted, one frame or more after the mod built it. A mod that must
 * change a panel in every frame (a fade, a label that follows the camera) asks set_on_present for a callback on the
 * game's present thread instead. In each frame the panel is shown, GraceUI calls it with the frame's time and the
 * panel's place, and it answers:
 *  - GRACEUI_PRESENT_KEEP: draw the panel's newest submitted list, as without a callback;
 *  - GRACEUI_PRESENT_DRAW: draw the list it wrote into `list`, from arrays of its own that hold until it returns.
 *    GraceUI copies that list into the room it reserved when the callback was set (the capacities below), checks the
 *    copy as submit checks a list, and draws it in this frame alone, in place of the submitted one; a list that does
 *    not fit or fails the check is not drawn (the submitted one is), and is logged once.
 * Any other answer is KEEP. A callback binds invariant 10 as GraceUI's own drawing does: it never waits, takes no lock,
 * allocates nothing per frame after its first frames, and never lets an exception leave it. It calls no slot of this
 * table: from inside it each answers GRACEUI_REFUSED, has_cap alone excepted. A mod's callbacks together have
 * GRACEUI_PRESENT_BUDGET_US in each frame (EST, measured on GraceUI's clock around each call): over it in
 * GRACEUI_PRESENT_OVER_FRAMES frames running, or an exception out of one, and every callback of the mod is suspended
 * for the session, with one notice naming the mod and a log line; its panels then draw their submitted lists, and
 * set_on_present answers GRACEUI_REFUSED for a callback (clearing one is taken). set_on_present that replaces or clears
 * a callback, and disconnect, return only once the old callback is not running, and it is never called again: a mod may
 * then free what `user` points to, or unload. Each waits for the frame being drawn to end, for as long as that takes;
 * never call either from DllMain, nor from a thread the present thread may be waiting for. */

#define GRACEUI_PRESENT_KEEP 0u /* the callback's answers */
#define GRACEUI_PRESENT_DRAW 1u

#define GRACEUI_PRESENT_MENU_OPEN 1u /* GraceUiPresentFrame.flags: GraceUI's menu is open in this frame */

typedef struct GraceUiPresentFrame {
  uint32_t size;            /* sizeof(GraceUiPresentFrame) in this GraceUI: a field exists only if it ends within it */
  uint32_t reserved;        /* 0 */
  uint64_t panel;           /* the panel (create_panel's id) */
  uint64_t frame;           /* GraceUI's frames this session in which it drew shown panels, counting this one: it rises by
                               one in each frame a callback can be called in, not in frames GraceUI passed by */
  int64_t time_us;          /* this frame's time: microseconds of a steady clock (on Windows, QueryPerformanceCounter's) */
  float delta_seconds;      /* since the frame before it in that count (1/60 for the first, and at most 0.25 after a
                               pause: time_us gives the clock itself) */
  float x;                  /* the panel's place on the display now (its anchor applied), in the display's pixels: the */
  float y;                  /*   list stays in the panel's own pixels, as submit's do, and GraceUI moves it there */
  float width;
  float height;
  uint32_t display_width;   /* the back buffer's size */
  uint32_t display_height;
  uint32_t flags;           /* GRACEUI_PRESENT_MENU_OPEN, or 0 */
  float spent_us;           /* the mod's callbacks together in the frame before it in that count, in microseconds on
                               GraceUI's clock: what its budget weighs (0 if none of them ran then) */
  uint32_t reserved2;       /* 0 */
} GraceUiPresentFrame;

/* `list` comes zeroed but for its size and command_size (sizeof(GraceUiDrawCommand)), which the callback may set to its
 * own stride; it writes the counts, the generation and the array pointers, as for submit. */
typedef uint32_t (*GraceUiPresentCallback)(void* user, const GraceUiPresentFrame* frame, GraceUiDrawList* list);

typedef struct GraceUiPresentHook {
  uint32_t size;
  uint32_t vertex_capacity;   /* the most vertices a list it draws holds: 1 to GRACEUI_DRAW_VERTEX_BYTES_MAX's 13,107 */
  uint32_t index_capacity;    /* ... indices: 1 to GRACEUI_DRAW_INDEX_BYTES_MAX's 131,072 */
  uint32_t command_capacity;  /* ... commands: 1 to GRACEUI_DRAW_COMMANDS_MAX */
  GraceUiPresentCallback callback; /* null: the panel's callback cleared (the capacities are then not read) */
  void* user;                 /* given back to it as is */
} GraceUiPresentHook;

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

/* A request's flags (ABI 1.1, D176): with GRACEUI_INPUT_EVENTS the holder's input comes to its mod through poll (its
 * keys, characters, pointer and pad buttons, and why its holder ended), never to another mod. Without it, as for every
 * ABI 1.0 mod, the input is taken from the game and the mod hears nothing of it. */
#define GRACEUI_INPUT_EVENTS 1u

typedef struct GraceUiInputRequest {
  uint32_t size;
  uint32_t kind;      /* GRACEUI_HOLDER_PANEL or GRACEUI_HOLDER_MODAL */
  uint32_t wants;     /* GRACEUI_WANT_* */
  uint32_t reserved;  /* 0 */
  uint64_t panel;     /* the panel whose view takes the input (create_panel's id): the mod's own, shown, made with
                         GRACEUI_PANEL_TAKES_INPUT, on the modal layer for GRACEUI_HOLDER_MODAL and on another for
                         GRACEUI_HOLDER_PANEL */
  /* ABI 1.1 (D176): */
  uint32_t flags;     /* GRACEUI_INPUT_EVENTS, or 0; an unknown flag is refused */
  uint32_t reserved2; /* 0 */
} GraceUiInputRequest;

typedef struct GraceUiInputAnswer {
  uint32_t size;         /* set by the caller to sizeof(GraceUiInputAnswer) */
  uint32_t answer;       /* GRACEUI_INPUT_* */
  uint32_t why;          /* with GRACEUI_INPUT_NOT_NOW: GRACEUI_NOT_NOW_* */
  uint32_t holder_kind;  /* with GRACEUI_INPUT_REFUSED: the holder's GRACEUI_HOLDER_* */
  char holder[40];       /* with GRACEUI_INPUT_REFUSED: the holder's mod id, NUL-terminated ("graceui": the menu) */
} GraceUiInputAnswer;

/* A text caret (ABI 1.1, D176): its rectangle in the panel's own pixels, from its top-left corner. */
typedef struct GraceUiCaret {
  uint32_t size;
  uint32_t reserved;  /* 0 */
  uint64_t panel;     /* the mod's panel that holds the field (create_panel's id) */
  float x;            /* the caret's left edge */
  float y;            /* its top */
  float height;       /* its height; 0: no caret */
  float reserved2;    /* 0 */
} GraceUiCaret;

/* ---- Hotkeys (D77, D155): an action with a default key and modifiers or a pad chord; GraceUI's Hotkeys page rebinds it.
 *
 * GraceUI watches for the hotkey in the game's window procedure: a fresh press of its key with exactly its modifiers,
 * while no holder has input, is one press, which the mod takes through poll (GRACEUI_EVENT_HOTKEY) on its own thread.
 * The press, its repeats, characters and release never reach the game; a key bound with no modifier is never the
 * game's while it is bound, through DirectInput as well. A default already taken (by another hotkey, or the menu key)
 * registers unbound: the hotkey exists and fires nothing until the player binds it (GraceUI 0.6's Hotkeys page), and
 * GraceUI's log names it.
 *
 * A pad chord (ABI 1.3, GRACEUI_CAP_PAD_CHORDS, D204; proposal 0005): two of XInput's buttons in order, an anchor and a
 * key, given as GRACEUI_PAD_CHORD(anchor, key). The anchor always reaches the game. A key pressed while the anchor has
 * been held for 150 ms is held back from the game; both held for 0.4 s from the key's press fire the hotkey, the key
 * never reaches the game, and both are hidden from it until each is released. Either released sooner replays the key
 * to the game as one tap. Chords stand aside while any holder has input and whenever request_input would answer
 * not_now (danger, a game menu, loading, not ready): then the key reaches the game at once. The times are estimates
 * (EST) that S4 confirms on a pad. A chord another hotkey has bound registers unbound, as a taken key does; one action
 * may have a key and a chord, each bound on its own, and either fires it. A mod built before ABI 1.3, and any GraceUI
 * before 0.2.14, answers a chord GRACEUI_NOT_YET. */

/* A hotkey as the player has bound it (ABI 1.4, D210; G37): GraceUI's Hotkeys page lets the player change any mod's
 * key and pad chord, kept between sessions, and the mod learns of a change by GRACEUI_EVENT_HOTKEY_CHANGED. A mod that
 * names its key in its own words ("Press T to talk") reads it here. */
#define GRACEUI_HOTKEY_KEY_BOUND 1u    /* its key fires it now */
#define GRACEUI_HOTKEY_CHORD_BOUND 2u  /* its pad chord fires it now */

typedef struct GraceUiHotkeyBinding {
  uint32_t size;        /* set by the caller to sizeof(GraceUiHotkeyBinding) */
  uint32_t bound;       /* GRACEUI_HOTKEY_*_BOUND */
  uint32_t key;         /* its key, a Windows virtual key; 0 for none */
  uint32_t modifiers;   /* GRACEUI_MOD_* */
  uint32_t pad_chord;   /* GRACEUI_PAD_CHORD(anchor, key); 0 for none */
  uint32_t reserved;    /* 0 */
  char key_text[32];    /* the key as the Hotkeys page writes it ("Ctrl+K"), NUL-terminated; empty for none */
  char chord_text[48];  /* the chord likewise ("D-pad Up, then View"); empty for none */
} GraceUiHotkeyBinding;

/* A pad chord for GraceUiHotkeyDesc.pad_chord: `anchor` then `key`, each exactly one XINPUT_GAMEPAD_* button (not a
 * trigger or a stick), the two different. */
#define GRACEUI_PAD_CHORD(anchor, key) ((((0u + (anchor)) & 0xFFFFu) << 16) | ((0u + (key)) & 0xFFFFu))

#define GRACEUI_MOD_CTRL 1u
#define GRACEUI_MOD_SHIFT 2u
#define GRACEUI_MOD_ALT 4u

typedef struct GraceUiHotkeyDesc {
  uint32_t size;
  uint32_t key;          /* the default key, a Windows virtual key (not Escape, a modifier or a mouse button); 0 for
                            none */
  uint32_t modifiers;    /* GRACEUI_MOD_*; Alt with F4 is refused */
  uint32_t pad_chord;    /* the default pad chord, GRACEUI_PAD_CHORD(anchor, key); 0 for none (ABI 1.3) */
  const char* action;    /* the action's id within the mod: 1 to 32 of a-z, 0-9 and '-', not the mod's already */
  uint64_t action_length;
  const char* name;      /* what the Hotkeys page calls it: 1 to 64 bytes of UTF-8 */
  uint64_t name_length;
} GraceUiHotkeyDesc;

/* ---- poll's events (D155): what GraceUI has for a mod, one at a time, on the mod's own thread. GraceUI 0.1 gives
 * hotkey presses; the bridge protocol (§6, GraceUI 0.2) adds its own kinds, and their fields after these. */

#define GRACEUI_EVENT_NONE 0u      /* nothing is waiting */
#define GRACEUI_EVENT_HOTKEY 1u    /* a hotkey of the mod's was pressed */
#define GRACEUI_EVENT_WATCHED 2u   /* a view now watches a topic of the mod's space: `frame` holds its sub frame (D169) */
#define GRACEUI_EVENT_UNWATCHED 3u /* no view watches the topic now: `frame` holds its unsub frame; stop building it */
#define GRACEUI_EVENT_COMMAND 4u   /* a view's command: `frame` holds its cmd frame, `reply` the handle answer takes;
                                      * since protocol 1.8 the frame's "view" names who sent it, from the channel it
                                      * came on: "game", "browser" (this PC) or "device" (one paired at home, D224) */
/* A holder's events (ABI 1.1, D176), for the mod that holds input and asked with GRACEUI_INPUT_EVENTS, only while it
 * holds it, in the order they happened; an event smaller than ABI 1.1's never gets them. */
#define GRACEUI_EVENT_KEY 5u         /* a key's press, auto-repeat or release: key, modifiers, pressed, repeat */
#define GRACEUI_EVENT_CHARACTER 6u   /* one character typed: code_point (a surrogate pair joined, an IME's result) */
#define GRACEUI_EVENT_POINTER 7u     /* with GRACEUI_WANT_MOUSE: x and y in the panel's own pixels (moves coalesced to
                                        the newest); a button's press or release (button, pressed); the wheel */
#define GRACEUI_EVENT_INPUT_ENDED 8u /* the mod's holder ended: why; once, after its other events */
#define GRACEUI_EVENT_PAD 9u         /* with GRACEUI_WANT_PAD: a pad button's press or release (button, pressed, pad) */
/* From ABI 1.2 (D180), to every mod that polls with an event of 1.2's size: */
#define GRACEUI_EVENT_DISPLAY 10u    /* the display: once after warm-up, then after each change of its size or ui.scale */
/* From ABI 1.4 (D210), to a mod connected with ABI minor 4 or more: */
#define GRACEUI_EVENT_HOTKEY_CHANGED 11u /* the player changed a hotkey's key or pad chord (hotkey_id): describe_hotkey
                                            gives it as it is now */

/* Why a holder ended (GRACEUI_EVENT_INPUT_ENDED's `why`). */
#define GRACEUI_CLOSED_RELEASED 1u   /* release_input, the holding panel hidden, or its mod gone */
#define GRACEUI_CLOSED_MENU_KEY 2u
#define GRACEUI_CLOSED_DANGER 3u
#define GRACEUI_CLOSED_LOADING 4u
#define GRACEUI_CLOSED_FOCUS_LOST 5u
#define GRACEUI_CLOSED_NO_RENDER 6u  /* no overlay frame for 2 s */
#define GRACEUI_CLOSED_FAULT 7u
#define GRACEUI_CLOSED_PLAYER 8u     /* the player's exit: Escape held 1 s, or the menu key twice */

/* A pointer event's button (GRACEUI_EVENT_POINTER with button_event 1). */
#define GRACEUI_BUTTON_LEFT 0u
#define GRACEUI_BUTTON_RIGHT 1u
#define GRACEUI_BUTTON_MIDDLE 2u
#define GRACEUI_BUTTON_X1 3u
#define GRACEUI_BUTTON_X2 4u

/* What pressed a hotkey (GraceUiEvent.hotkey_by, ABI 1.7, D243). */
#define GRACEUI_HOTKEY_BY_KEY 1u   /* its key */
#define GRACEUI_HOTKEY_BY_CHORD 2u /* its pad chord (D204) */

typedef struct GraceUiEvent {
  uint32_t size;       /* set by the caller to sizeof(GraceUiEvent) */
  uint32_t kind;       /* GRACEUI_EVENT_* */
  uint32_t hotkey_id;  /* with GRACEUI_EVENT_HOTKEY: register_hotkey's id */
  uint32_t count;      /* with GRACEUI_EVENT_HOTKEY: its presses since its last event, 1 or more */
  /* From GraceUI 0.2 (D169): an event smaller than this never gets the protocol's kinds, so a 0.1 mod sees hotkeys. */
  uint64_t reply;          /* with GRACEUI_EVENT_COMMAND: the handle answer takes */
  char* frame;             /* the mod's buffer, which GraceUI writes the event's frame into (UTF-8 JSON, no NUL) */
  uint64_t frame_capacity; /* its size in bytes: GRACEUI_FRAME_FROM_PAGE_MAX always suffices */
  uint64_t frame_length;   /* the frame's length; with GRACEUI_FULL, the size needed */
  /* From ABI 1.1 (D176): a holder's events. An event smaller than this never gets them. */
  uint32_t key;          /* GRACEUI_EVENT_KEY: its virtual key (VK_*) */
  uint32_t modifiers;    /* GRACEUI_EVENT_KEY: GRACEUI_MOD_* held with it */
  uint32_t pressed;      /* KEY, PAD, and a POINTER button: 1 pressed, 0 released */
  uint32_t repeat;       /* GRACEUI_EVENT_KEY: 1 for an auto-repeat of a held key */
  uint32_t code_point;   /* GRACEUI_EVENT_CHARACTER: one Unicode scalar value */
  uint32_t button;       /* POINTER: GRACEUI_BUTTON_*; PAD: one XInput button's mask (XINPUT_GAMEPAD_*) */
  uint32_t button_event; /* POINTER: 1 for a button's press or release, 0 for a move or the wheel */
  uint32_t pad;          /* GRACEUI_EVENT_PAD: the pad's index, 0 to 3 */
  uint32_t why;          /* GRACEUI_EVENT_INPUT_ENDED: GRACEUI_CLOSED_* */
  uint32_t dropped;      /* this holder's events lost before this one (the mod polled too slowly; 256 wait at most) */
  float x;               /* GRACEUI_EVENT_POINTER: in the panel's own pixels, from its top-left corner (outside it */
  float y;               /*   when the pointer is) */
  float wheel_x;         /* GRACEUI_EVENT_POINTER: wheel notches, rightwards */
  float wheel_y;         /*   and away from the player */
  /* From ABI 1.2 (D180): GRACEUI_EVENT_DISPLAY's fields. An event smaller than this never gets that kind. */
  uint32_t display_width;  /* the back buffer's size */
  uint32_t display_height;
  float ui_scale;          /* GraceUI's ui.scale (Menu size) */
  float ui_scale_default;  /* its default */
  float scale;             /* the effective scale, (display_height / 1080) x ui_scale, GraceUI's own text's (§3.13) */
  float content_scale;     /* scale / ui_scale_default: a mod's design size at 1080 lines and the default Menu size */
  float faces_scale;       /* the content scale the mod's GRACEUI_FACE_SCALED faces were baked at (0 if it has none) */
  /* From ABI 1.7 (D243), in a word that was reserved, so an event of 1.2's size holds it: with GRACEUI_EVENT_HOTKEY,
   * what made the presses `count` counts, GRACEUI_HOTKEY_BY_KEY and GRACEUI_HOTKEY_BY_CHORD (both, when both did).
   * 0 from a GraceUI without has_cap("hotkey-by"), which wrote 0 here. */
  uint32_t hotkey_by;
  /* From ABI 1.6 (D226): a file command's file, uploaded in the browser view on this PC (§7.2 item 18). With
   * GRACEUI_EVENT_COMMAND, GraceUI writes the file's bytes into the mod's buffer beside the frame; a buffer too small
   * answers GRACEUI_FULL with both lengths set, and the event waits for the next poll. GraceUI keeps no copy once it is
   * written. A mod whose event is smaller than this never gets a file command: GraceUI answers it not_now itself. */
  uint8_t* file;           /* the mod's buffer for the file, or null */
  uint64_t file_capacity;  /* its size in bytes: GRACEUI_UPLOAD_MAX always suffices */
  uint64_t file_length;    /* the file's length, 0 when the command has none; with GRACEUI_FULL, the size needed */
} GraceUiEvent;

/* ---- A space (D75, D81, D169): the mod's pages, topics and commands, as the bridge protocol's frames (§6). The same
 * JSON frames as the browser's WebSocket cross the ABI, so a space has one code path for both views. GraceUI routes and
 * checks; the space validates its own commands (invariant 3). Each of register_pages, publish, poll and answer may be
 * called from any thread of the mod's, during GraceUI's warm-up too, but never from DllMain; none waits for a view. */
#define GRACEUI_VIEW_GAME 1u    /* the in-game menu shows the space */
#define GRACEUI_VIEW_BROWSER 2u /* the browser view serves it: GraceUI's server runs while a space asks (§7.2 item 1) */
#define GRACEUI_FRAME_FROM_PAGE_MAX 16384u /* a frame from a view: a sub, an unsub or a cmd, at most this many bytes */
#define GRACEUI_UPLOAD_MAX 33554432u       /* an uploaded file, at most 32 MiB (ABI 1.6, D226) */
#define GRACEUI_FRAME_TO_PAGE_MAX 262144u  /* a frame to a view: a state, rows or a result, at most this many bytes */
#define GRACEUI_ANSWER_DEADLINE_MS 10000u  /* a command unanswered this long is answered internal by GraceUI */
#define GRACEUI_SLOW_ANSWER_DEADLINE_MS 90000u  /* one the space's page data names in slow_commands (protocol 1.6,
                                                   D216): a model's draft, answered within this */

typedef struct GraceUiSpaceDesc {
  uint32_t size;      /* set by the caller to sizeof(GraceUiSpaceDesc) */
  uint32_t views;     /* GRACEUI_VIEW_*, one or both */
  uint32_t version;   /* the space's version, GRACEUI_VERSION_PACK, named in welcome */
  uint32_t reserved;  /* 0 */
  const char* pages;  /* its page data, space_pages as UTF-8 JSON (D160): its "space" is the mod's id */
  uint64_t pages_length;
  const char* settings; /* its settings description, settings_description as UTF-8 JSON (§6.9) */
  uint64_t settings_length;
  const char* increment; /* the increment the space is at ("0.2"), named in welcome */
  uint64_t increment_length;
} GraceUiSpaceDesc;

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
 * protocol's four (register_pages, publish, answer) and log, and its poll gives hotkey events alone (D155); GraceUI 0.2
 * provides the protocol's three and poll's protocol events (D169), and log is declared for the increment that builds
 * it; GraceUI 0.2.1 (ABI 1.1) adds set_caret and poll's holder events (D176); GraceUI 0.2.2 (ABI 1.2) connect's display
 * fields, poll's display event and scaled faces (D180), with no new slot.
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

  /* The mod's one space (above), copied before it returns: GRACEUI_OK; _REFUSED (its JSON fails the protocol's
   * decoders, its pages and settings disagree or a default fails, its "space" is not the mod's id, no known view);
   * _DUPLICATE_ID (the mod, or another mod, has registered this space already); _FULL (16 spaces); _INACTIVE in the
   * notice-only mode, where no server runs beside an anti-cheat. Since GraceUI 0.2 (GRACEUI_CAP_PAGES). */
  GraceUiStatus (*register_pages)(GraceUiConsumer consumer, const GraceUiSpaceDesc* space);
  /* A state or rows frame of the mod's own space ({"v":1,"t":"state","space":…,"topic":…,"rev":…,"body":{…}}), at
   * most GRACEUI_FRAME_TO_PAGE_MAX bytes, copied: GRACEUI_OK (dropped if no view watches the topic); _REFUSED (another
   * space's, a topic it does not declare or not of its shape, too long, not a frame); _NOT_YET before register_pages. */
  GraceUiStatus (*publish)(GraceUiConsumer consumer, const char* frame, uint64_t frame_length);
  /* The mod's next event, at once: GRACEUI_OK with one (GRACEUI_EVENT_NONE if nothing is waiting), or _REFUSED for an
   * event that is null or below its first size. Call it until it gives GRACEUI_EVENT_NONE; it never waits (D155).
   * Hotkeys come first, then the space's events (D169), each frame written into the event's buffer; a buffer too small
   * answers GRACEUI_FULL with frame_length the size needed, and the event waits for the next call. */
  GraceUiStatus (*poll)(GraceUiConsumer consumer, GraceUiEvent* event);
  /* The result frame of the command `reply` names ({"v":1,"t":"result","space":…,"id":…,"ok":true}, or with "error"
   * and an optional "body", whose "detail" gives the reason in words, never a secret): GraceUI sets its space and id
   * and routes it to the one view that asked. GRACEUI_OK; _REFUSED (not a result with ok or an error, too long, or a
   * reply that is unknown, answered already or past GRACEUI_ANSWER_DEADLINE_MS, which GraceUI answered internal). */
  GraceUiStatus (*answer)(GraceUiConsumer consumer, uint64_t reply, const char* frame, uint64_t frame_length);

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
   * UTF-8, a key with no place on the keyboard, a pad chord that is not two different buttons); _FULL
   * (GRACEUI_HOTKEYS_PER_MOD, or the session's); _NOT_YET for a pad chord from a mod built before ABI 1.3. With a key
   * and a chord, _UNBOUND if either is taken; the other still fires it. */
  GraceUiStatus (*register_hotkey)(GraceUiConsumer consumer, const GraceUiHotkeyDesc* hotkey, uint32_t* hotkey_id);

  /* Shows, replaces or withdraws one of the mod's notices: GRACEUI_OK, _REFUSED (a text that is not UTF-8, too long,
   * or empty without a key) or _FULL (GRACEUI_NOTICES_PER_MOD showing). Callable in the anti-cheat's notice-only mode,
   * after connect answered GRACEUI_INACTIVE with GRACEUI_INACTIVE_ANTI_CHEAT (D88). */
  GraceUiStatus (*notice)(GraceUiConsumer consumer, const GraceUiNotice* notice);

  /* The mod's claims as a world-state provider (D78), replacing its earlier ones; every provider's are combined, and
   * any provider's danger or loading ends the input holder (invariant 9). GRACEUI_OK or _REFUSED (an unknown flag). */
  GraceUiStatus (*set_world_state)(GraceUiConsumer consumer, const GraceUiWorldState* state);
  GraceUiStatus (*log)(GraceUiConsumer consumer, uint32_t level, const char* text, uint64_t text_length);

  /* ---- ABI 1.1 (GraceUI 0.2.1, D176). */

  /* Where the text caret of the mod's field is, so that GraceUI places the IME's composition and candidate windows at
   * it while the mod holds input (§5.6): GRACEUI_OK; _REFUSED (a panel not the mod's, or a caret not finite). Height 0
   * clears it. Only the holder's caret counts: one set while the mod does not hold input is answered OK and ignored,
   * and the caret ends with its holder, so a mod sets it while it holds (each frame its field has the caret). Since
   * GraceUI 0.2.1 (GRACEUI_CAP_HOLDER_EVENTS). */
  GraceUiStatus (*set_caret)(GraceUiConsumer consumer, const GraceUiCaret* caret);

  /* ---- ABI 1.4 (GraceUI 0.2.16, D210). */

  /* One of the mod's hotkeys as the player has bound it now (above): GRACEUI_OK; _REFUSED (an id not the mod's, or a
   * binding that is null or smaller than 1.4's). Since GraceUI 0.2.16 (GRACEUI_CAP_HOTKEY_BINDINGS). */
  GraceUiStatus (*describe_hotkey)(GraceUiConsumer consumer, uint32_t hotkey_id, GraceUiHotkeyBinding* binding);

  /* ---- ABI 1.5 (GraceUI 0.2.17, D212). */

  /* A wait handle for the mod's events (D71), so that its thread waits for GraceUI instead of polling on a timer:
   * GRACEUI_OK with `*event` a Win32 HANDLE of the mod's own (SYNCHRONIZE access: wait on it, close it when done; ask
   * again for another), or _REFUSED (event null), _FAULT (Windows made no event). GraceUI sets the event, auto-reset,
   * whenever something is kept for the mod's poll: a hotkey's press or change, a holder's event, a view's watch,
   * unwatch or command, the display event; and once when it is asked for. So a mod waits on it, then polls until
   * GRACEUI_EVENT_NONE, then waits again; a wake with nothing to poll can happen and costs one poll. GraceUI never calls
   * the mod's code to wake it. One event for the connection's life; since GraceUI 0.2.17 (GRACEUI_CAP_WAKE). */
  GraceUiStatus (*wake_event)(GraceUiConsumer consumer, void** event);

  /* ---- ABI 1.8 (GraceUI 0.2.52, D259). */

  /* Sets, replaces or clears (a null callback) the callback that draws one of the mod's panels each frame (above):
   * GRACEUI_OK; _REFUSED (a panel not the mod's, a hook null or below its size, capacities outside their bounds, a
   * callback set while the mod's are suspended (clearing one is taken), or called from inside a callback); _FULL (GRACEUI_PRESENT_HOOKS_PER_MOD, or the
   * session's). Setting one closes the mod's registration window, as its first submit does. It may wait for the frame
   * being drawn to end; GraceUI's present thread never waits for it. Since GraceUI 0.2.52 (GRACEUI_CAP_ON_PRESENT). */
  GraceUiStatus (*set_on_present)(GraceUiConsumer consumer, uint64_t panel_id, const GraceUiPresentHook* hook);
} GraceUiApi1;

/* Whether `api`'s size covers `slot` (a member of GraceUiApi1): call a slot only then. */
#define GRACEUI_API1_COVERS(api, slot) \
  ((api) != NULL && (api)->size >= offsetof(GraceUiApi1, slot) + sizeof((api)->slot))

#ifdef __cplusplus
}
#endif

#endif /* GRACEUI_H */
