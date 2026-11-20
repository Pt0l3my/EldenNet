/* SPDX-License-Identifier: MIT */
/*
 * graceui_test.h: graceui.dll's test-only exports, for tools/grace-harness. They exist only in a build configured with
 * GRACEUI_TEST_EXPORTS=ON, never in a release build (tools/check_safety.py --exports checks a DLL's export table), and
 * they are not part of the public ABI: no mod may call them.
 */
#ifndef GRACEUI_TEST_H
#define GRACEUI_TEST_H

#include <stdint.h>

#include "graceui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The test panel: a filled rectangle of one known colour with a line of white text in it, drawn on the foreground of
 * every frame while it is on, in the swap chain's pixels. */
#define GRACEUI_TEST_PANEL_X0 24
#define GRACEUI_TEST_PANEL_Y0 24
#define GRACEUI_TEST_PANEL_X1 184
#define GRACEUI_TEST_PANEL_Y1 88
#define GRACEUI_TEST_PANEL_R 224
#define GRACEUI_TEST_PANEL_G 64
#define GRACEUI_TEST_PANEL_B 160
#define GRACEUI_TEST_TEXT_X 32
#define GRACEUI_TEST_TEXT_Y 40
#define GRACEUI_TEST_TEXT "GraceUI test panel"

/* The host's state, in the copy that answers. */
enum {
  GRACEUI_TEST_HOST_NOT_ASKED = 0,  /* GraceUI_RequestApi never called */
  GRACEUI_TEST_HOST_STARTED = 1,    /* this copy holds the mutex; its init thread runs */
  GRACEUI_TEST_HOST_HOOKED = 2,     /* the hooks are installed */
  GRACEUI_TEST_HOST_FAILED = 3,     /* the init thread gave up (no window, no vtables, a hook refused) */
  GRACEUI_TEST_HOST_IDLE_COPY = 4   /* another copy holds the mutex: this one idles (D72) */
};

/* How the present queue was found (D116). */
enum {
  GRACEUI_TEST_QUEUE_NONE = 0,
  GRACEUI_TEST_QUEUE_SWAP_CHAIN = 1,  /* the swap chain answered GetDevice for ID3D12CommandQueue */
  GRACEUI_TEST_QUEUE_RESIZE1 = 2,     /* the game named it in ResizeBuffers1 */
  GRACEUI_TEST_QUEUE_PRESENT = 3,     /* a direct queue executed inside the original Present */
  GRACEUI_TEST_QUEUE_ONLY_DIRECT = 4  /* the swap chain's device's only direct queue seen executing */
};

typedef struct GraceUiTestState {
  uint32_t size;                  /* set by the caller to sizeof(GraceUiTestState) */
  uint32_t host;                  /* GRACEUI_TEST_HOST_* */
  uint32_t init_threads_started;  /* init threads this copy started: at most 1 */
  uint32_t hook_install_runs;     /* times the install routine ran: at most 1 */
  uint32_t hooks_installed;       /* every hook this copy installed: the table's and the window procedure's filter */
  uint32_t bound;                 /* 1 while a Direct3D 12 swap chain is bound */
  uint32_t binds;                 /* swap chains bound so far */
  uint32_t queue_method;          /* GRACEUI_TEST_QUEUE_* */
  uint32_t direct_queues_seen;    /* direct queues seen executing, on any device */
  uint32_t renderer_ready;        /* 1 once the context, both backends and the atlas's upload exist */
  uint32_t atlas_ready;           /* 1 once the atlas's upload has completed */
  uint32_t atlas_width;
  uint32_t atlas_height;
  uint32_t render_off;            /* 1 if the render path turned itself off for the session */
  uint32_t resizes;               /* ResizeBuffers and ResizeBuffers1 calls on the bound swap chain */
  uint32_t passed_through;        /* swap chains passed through untouched (not Direct3D 12, or not this process's) */
  uint64_t presents;              /* outermost Present and Present1 calls */
  uint64_t frames_drawn;
  uint64_t frames_skipped_busy;   /* frames not drawn because their slot was still in use by the GPU (never waited) */
  uint64_t allocations;           /* on the present thread, inside the detours, since the last reset */
  uint64_t frees;
  uint32_t notice_only;           /* 1 if the anti-cheat check put the session in the notice-only mode (D88) */
  uint32_t anti_cheat;            /* GRACEUI_TEST_ANTI_CHEAT_*: which clause found it */
  uint32_t consumers;             /* mods connected */
  uint32_t notices;               /* notices showing, GraceUI's own among them */
  uint32_t copies_named;          /* other copies of GraceUI this one has named in a notice (D72) */
  uint32_t reserved;
  /* S10.4.c: panels, draw lists, textures, faces and providers. */
  uint32_t panels;                /* panels made, in all */
  uint32_t panels_shown;
  uint64_t lists_submitted;       /* draw lists that passed the checker and were published */
  uint64_t lists_refused;
  uint64_t panel_frames;          /* frames that drew at least one panel's list */
  uint32_t textures;              /* mods' textures registered and not yet gone */
  uint32_t textures_ready;        /* ... on the GPU */
  uint32_t texture_uploads;       /* upload batches queued on the GPU: warm-up's (with the atlas) first */
  uint32_t textures_uploaded;     /* mods' textures whose copy has completed */
  uint32_t faces;                 /* mods' faces registered before warm-up */
  uint32_t faces_baked;           /* faces in the atlas, GraceUI's own among them */
  uint32_t atlas_generation;      /* 0 before warm-up */
  uint32_t notice_face_grace;     /* 1 if notices are drawn in GraceUI's own face (fonts\NotoSans-Regular.ttf) */
  uint32_t world_flags;           /* every provider's claims, combined (GRACEUI_WORLD_*) */
  uint32_t providers;             /* world-state providers */
  /* Appended for the in-game spikes (D175): */
  uint32_t present_prologue;      /* GRACEUI_TEST_PROLOGUE_*: Present's first bytes as GraceUI found them at install */
  uint32_t reserved2;
} GraceUiTestState;

/* What Present's first bytes held when GraceUI installed its hook (D86's cue that another hook was there first). */
enum { GRACEUI_TEST_PROLOGUE_UNKNOWN = 0, GRACEUI_TEST_PROLOGUE_PLAIN = 1, GRACEUI_TEST_PROLOGUE_JUMP = 2 };

/* What the anti-cheat check found (D133). */
enum { GRACEUI_TEST_ANTI_CHEAT_NONE = 0, GRACEUI_TEST_ANTI_CHEAT_MODULE = 1, GRACEUI_TEST_ANTI_CHEAT_PARENT = 2 };

/* Turns the test panel on (1) or off (0). */
GRACEUI_API void GraceUI_Test_SetPanel(uint32_t on);
/* Fills *out (out->size set by the caller); answers 1, or 0 if the size is too small. */
GRACEUI_API uint32_t GraceUI_Test_GetState(GraceUiTestState* out);
/* Resets the allocation counters, the frame-cost and input-cost samples, and the characters Dear ImGui received. */
GRACEUI_API void GraceUI_Test_ResetCounters(void);
/* One frame's cost on the bound swap chain, in nanoseconds: the Present detour's whole time less the original Present's,
 * and the part of it spent inside the runtime's ExecuteCommandLists and Signal on GraceUI's own submission. */
typedef struct GraceUiTestFrameCost {
  uint32_t total_ns;
  uint32_t submit_ns;
} GraceUiTestFrameCost;

/* Copies up to `capacity` of the newest frames' costs, oldest first; answers how many. */
GRACEUI_API uint32_t GraceUI_Test_GetFrameCosts(GraceUiTestFrameCost* out, uint32_t capacity);
/* Copies log line `index` (0 the oldest kept) into `out`, NUL-terminated; answers its length, or 0 past the last. */
GRACEUI_API uint32_t GraceUI_Test_GetLogLine(uint32_t index, char* out, uint32_t capacity);
/* Copies installed hook `index` (install order) into `out` as D102's list names it, with " in <DLL>" for one of
 * several XInput DLLs; answers its length, or 0 past the last. */
GRACEUI_API uint32_t GraceUI_Test_GetHookName(uint32_t index, char* out, uint32_t capacity);

/* ---- The input half (D77, D125): the holder, reached through these until S10.4.b's ABI. */

enum { GRACEUI_TEST_KIND_MENU = 1, GRACEUI_TEST_KIND_PANEL = 2, GRACEUI_TEST_KIND_MODAL = 3 };
enum { GRACEUI_TEST_WANT_KEYBOARD = 1, GRACEUI_TEST_WANT_MOUSE = 2, GRACEUI_TEST_WANT_PAD = 4, GRACEUI_TEST_WANT_ALL = 7 };
enum { GRACEUI_TEST_GRANTED = 0, GRACEUI_TEST_REFUSED = 1, GRACEUI_TEST_NOT_NOW = 2 };
enum {
  GRACEUI_TEST_NOT_NOW_NONE = 0,
  GRACEUI_TEST_NOT_NOW_NOT_READY = 1,
  GRACEUI_TEST_NOT_NOW_DANGER = 2,
  GRACEUI_TEST_NOT_NOW_LOADING = 3,
  GRACEUI_TEST_NOT_NOW_GAME_MENU = 4,
  GRACEUI_TEST_NOT_NOW_INVALID = 5
};
enum {
  GRACEUI_TEST_CLOSED_NONE = 0,
  GRACEUI_TEST_CLOSED_RELEASED = 1,
  GRACEUI_TEST_CLOSED_MENU_KEY = 2,
  GRACEUI_TEST_CLOSED_DANGER = 3,
  GRACEUI_TEST_CLOSED_LOADING = 4,
  GRACEUI_TEST_CLOSED_FOCUS_LOST = 5,
  GRACEUI_TEST_CLOSED_NO_RENDER = 6,
  GRACEUI_TEST_CLOSED_FAULT = 7,
  GRACEUI_TEST_CLOSED_PLAYER = 8
};
enum { GRACEUI_TEST_WORLD_DANGER = 1, GRACEUI_TEST_WORLD_LOADING = 2, GRACEUI_TEST_WORLD_GAME_MENU = 4 };
enum { GRACEUI_TEST_COST_WINDOW = 0, GRACEUI_TEST_COST_DINPUT = 1, GRACEUI_TEST_COST_XINPUT = 2 };

typedef struct GraceUiTestAnswer {
  uint32_t answer;      /* GRACEUI_TEST_GRANTED, _REFUSED or _NOT_NOW */
  uint32_t why;         /* GRACEUI_TEST_NOT_NOW_* */
  uint32_t owner;       /* granted: the new holder; refused: the holder */
  uint32_t kind;
  uint32_t generation;
} GraceUiTestAnswer;

typedef struct GraceUiTestInput {
  uint32_t size;               /* set by the caller to sizeof(GraceUiTestInput) */
  uint32_t holder_owner;       /* 0 with no holder */
  uint32_t holder_kind;        /* GRACEUI_TEST_KIND_*, 0 with no holder */
  uint32_t holder_wants;
  uint32_t generation;         /* grants so far */
  uint32_t last_closed_owner;
  uint32_t last_closed_kind;
  uint32_t last_close_reason;  /* GRACEUI_TEST_CLOSED_* */
  uint32_t closes;
  uint32_t notices;            /* "‹Mod› has the controls" */
  uint32_t window_filter;      /* 1 once the game's window procedure is filtered */
  uint32_t window_unicode;     /* 1 if the filtered window is a Unicode window, 0 an ANSI one */
  uint32_t render_hooks;
  uint32_t dinput_hooks;
  uint32_t xinput_hooks;
  uint32_t input_off;          /* 1 if a fault turned input off for the session */
  uint32_t events_dropped;     /* the holder's events the ring had no room for */
  uint32_t chars_seen;         /* code points Dear ImGui received since the last reset (GraceUI_Test_GetChars) */
  uint32_t imgui_keys_down;    /* Dear ImGui's keys down after the last frame, mouse buttons not counted */
  uint32_t imgui_draws_cursor; /* 1 if the last frame drew Dear ImGui's own cursor */
  int32_t imgui_mouse_x;       /* Dear ImGui's mouse position after the last frame, in back-buffer pixels */
  int32_t imgui_mouse_y;
  uint64_t frames_fed;         /* frames whose input came from a holder */
  uint64_t window;             /* the window whose filter is active, as a number (0 for none) */
  uint32_t windows_filtered;   /* windows given a filter so far: the first found, then the bound swap chain's */
  uint32_t reserved;
  /* Appended for the in-game spikes (D175). Input of a device the holder takes that still reached the game, by kind
   * (Alt+F4's press, which always does, left out): */
  uint32_t passed_presses;     /* key presses */
  uint32_t passed_releases;    /* key releases (one the game saw begin reaches it, by design) */
  uint32_t passed_characters;
  uint32_t passed_mouse;       /* mouse moves, wheels and button presses */
  uint32_t passed_raw;         /* raw input */
  /* DirectInput and XInput reads GraceUI changed: while a holder held, and with none (a closing press until it is
   * released, and the menu key, which never reaches the game): */
  uint32_t masked_keyboard_held;
  uint32_t masked_mouse_held;
  uint32_t masked_pad_held;
  uint32_t masked_keyboard_free;
  uint32_t masked_mouse_free;
  uint32_t masked_pad_free;
  uint32_t reserved2;
} GraceUiTestInput;

/* request_input (D77): fills *out; answers 1, or 0 if `out` is null. */
GRACEUI_API uint32_t GraceUI_Test_RequestInput(uint32_t owner, uint32_t kind, uint32_t wants, GraceUiTestAnswer* out);
/* The holder's own release: answers 1 if `owner` held input. */
GRACEUI_API uint32_t GraceUI_Test_ReleaseInput(uint32_t owner);
/* A world-state provider's claims (D78), GRACEUI_TEST_WORLD_* flags: danger and loading end any holder. */
GRACEUI_API void GraceUI_Test_SetWorld(uint32_t flags);
/* The holder's text caret, in back-buffer pixels, for the IME's windows; height 0 for none. */
GRACEUI_API void GraceUI_Test_SetCaret(int32_t x, int32_t y, int32_t height);
/* The code page an ANSI window's characters are read in: 0 for its keyboard layout's. */
GRACEUI_API void GraceUI_Test_SetCodePage(uint32_t code_page);
/* Pad `user`'s buttons (XINPUT_GAMEPAD_*) as an XInput read gives them, through the detour's own filter (D176). */
GRACEUI_API void GraceUI_Test_Pad(uint32_t user, uint32_t buttons);
/* Fills *out (out->size set by the caller); answers 1, or 0 if the size is too small. */
GRACEUI_API uint32_t GraceUI_Test_GetInput(GraceUiTestInput* out);
/* Copies up to `capacity` of the code points Dear ImGui received since the last reset, oldest first; answers how many. */
GRACEUI_API uint32_t GraceUI_Test_GetChars(uint32_t* out, uint32_t capacity);
/* Copies up to `capacity` of the newest input costs of one kind (GRACEUI_TEST_COST_*), in nanoseconds: the filter's
 * own work on one message, or a detour's on one read, the original's time left out. Answers how many. */
GRACEUI_API uint32_t GraceUI_Test_GetInputCosts(uint32_t kind, uint32_t* out, uint32_t capacity);

/* ---- S10.4.b: the anti-cheat check, the mods connected and the notices (D131 to D135). */

/* The anti-cheat check compares these names instead of the real ones (an anti-cheat's module prefix, its launcher's
 * image name), so that a test simulates a hit with no anti-cheat software present. Before the first
 * GraceUI_RequestApi only; null keeps the real name. */
GRACEUI_API void GraceUI_Test_SetAntiCheatNames(const wchar_t* module_prefix, const wchar_t* parent_name);

typedef struct GraceUiTestNotice {
  uint32_t size;          /* set by the caller to sizeof(GraceUiTestNotice) */
  uint32_t owner;         /* 0: GraceUI's own; else the mod's number (GraceUiTestConsumer.owner) */
  uint32_t key;
  uint32_t tone;          /* GRACEUI_TONE_* */
  uint32_t timed;         /* 1 if it shows for a time, 0 until replaced or withdrawn */
  uint32_t text_length;
  char text[GRACEUI_NOTICE_TEXT_MAX + 1];  /* NUL-terminated */
} GraceUiTestNotice;

/* Fills *out with notice `index` of those showing, oldest first (out->size set by the caller); answers 1, or 0 past
 * the last or if the size is too small. */
GRACEUI_API uint32_t GraceUI_Test_GetNotice(uint32_t index, GraceUiTestNotice* out);
/* Withdraws every notice, GraceUI's own included. */
GRACEUI_API void GraceUI_Test_ClearNotices(void);

typedef struct GraceUiTestConsumer {
  uint32_t size;          /* set by the caller to sizeof(GraceUiTestConsumer) */
  uint32_t owner;         /* the mod's number, 1 to GRACEUI_MODS_MAX */
  uint32_t version;
  uint32_t floor;
  uint32_t abi_minor;
  char id[GRACEUI_ID_MAX + 1];
  char name[GRACEUI_NAME_MAX + 1];
  char caps[GRACEUI_CAPS_MAX + 1];
} GraceUiTestConsumer;

/* Fills *out with connected mod `index`, in the order of their slots (out->size set by the caller); answers 1, or 0
 * past the last or if the size is too small. */
GRACEUI_API uint32_t GraceUI_Test_GetConsumer(uint32_t index, GraceUiTestConsumer* out);

/* ---- The host review's fixes (2026-11-16). */

/* The first GraceUI_RequestApi of this copy takes `ms` longer, so that a request made meanwhile shows its bounded wait
 * (2 s, then null). Set before the first request. */
GRACEUI_API void GraceUI_Test_SetFirstRequestDelay(uint32_t ms);
/* The render path turns itself off at its next frame, as a fault turns it off: every input then reaches the game. */
GRACEUI_API void GraceUI_Test_TurnOffRender(void);
/* Lists published to panel `panel` so far (by any submit that passed the checker), or 0 for no such panel: one panel's
 * own count, which another mod's thread submitting meanwhile does not move. */
GRACEUI_API uint64_t GraceUI_Test_GetPanelPublished(uint64_t panel);
/* The shared atlas's largest texture, `side` pixels square, a power of two (0: Dear ImGui's own, 8192), so that a bake
 * runs out of room and the faces' fallback shows. Set before warm-up. */
GRACEUI_API void GraceUI_Test_SetAtlasLimit(uint32_t side);

/* Where GraceUI 0.1 draws notices (D134), in the swap chain's pixels: boxes of this width at the top right, this far
 * from the edges, the newest last, each a ground.2 box with an accent of this width at its left in gold, ember or rot
 * and text in ink.hi, at most three lines. */
#define GRACEUI_TEST_NOTICE_MARGIN 12
#define GRACEUI_TEST_NOTICE_WIDTH 300
#define GRACEUI_TEST_NOTICE_ACCENT 3
#define GRACEUI_TEST_NOTICE_GROUND 0x1D1A15 /* ground.2, as 0xRRGGBB */
#define GRACEUI_TEST_NOTICE_GOLD 0xC9A65A
#define GRACEUI_TEST_NOTICE_EMBER 0xDB9550
#define GRACEUI_TEST_NOTICE_ROT 0xD9806F
/* S10.4.c's look (§3.8): each box this much above and below its text, whose lines are this tall in GraceUI's own face
 * (Noto Sans 15 on 22 px lines), or in Dear ImGui's own font where GraceUI's fonts folder lacks it. */
#define GRACEUI_TEST_NOTICE_PAD_Y 8
#define GRACEUI_TEST_NOTICE_LINE 22
#define GRACEUI_TEST_NOTICE_LINE_FALLBACK 17

/* ---- The in-game menu (D167): GraceUI's own space, drawn by the host as the input holder of kind menu, opened by the
 * menu key (F5). */
typedef struct GraceUiTestMenu {
  uint32_t size;        /* set by the caller to sizeof(GraceUiTestMenu) */
  uint32_t open;        /* 1 while the menu holds input */
  uint32_t generation;  /* its holder's generation, while open */
  uint32_t inputs;      /* keys and buttons the page view took */
  uint64_t lists;       /* lists laid out and published, on GraceUI's own thread */
  uint64_t frames;      /* frames that drew the menu's list */
  uint32_t refusals;    /* commands GraceUI's settings store refused */
  uint32_t layout_us;   /* the last layout and tessellation, in microseconds */
  uint32_t vertices;    /* of the last list */
  uint32_t indices;
  uint32_t commands;
  uint32_t off;         /* 1 once a fault turned the menu off for the session */
  float scale;          /* the effective scale of the last layout: (H / 1080) x ui.scale */
  uint32_t faces;       /* the menu's faces in the atlas: one a type role (11), or 0 if they were left out */
  char page[64];        /* the open page's id */
  char focused[96];     /* the focused element, as grace-view names it ("setting:ui.scale") */
} GraceUiTestMenu;

/* Fills *out (out->size set by the caller); answers 1, or 0 if the size is too small. */
GRACEUI_API uint32_t GraceUI_Test_GetMenu(GraceUiTestMenu* out);
/* A setting of GraceUI's own space as JSON text ("1.05", "\"clear\""), NUL-terminated; answers its length, or 0 before
 * GraceUI's own thread has loaded its space (at its first wake after the first request) or for no such key. One store
 * holds it for the menu and the browser's views alike (D169). Read it while GraceUI is idle. */
GRACEUI_API uint32_t GraceUI_Test_GetSetting(const char* key, char* out, uint32_t capacity);

/* ---- The spaces and the browser view's server (D169). */
typedef struct GraceUiTestServer {
  uint32_t size;     /* set by the caller to sizeof(GraceUiTestServer) */
  uint32_t running;  /* 1 while the server runs */
  uint32_t port;     /* its port, while it runs */
  uint32_t spaces;   /* mods' spaces registered */
  uint32_t pending;  /* mods' commands polled and not yet answered */
  uint32_t failed;   /* 1 if it could not start this session */
} GraceUiTestServer;

/* Fills *out (out->size set by the caller); answers 1, or 0 if the size is too small. */
GRACEUI_API uint32_t GraceUI_Test_GetServer(GraceUiTestServer* out);

#ifdef __cplusplus
}
#endif

#endif /* GRACEUI_TEST_H */
