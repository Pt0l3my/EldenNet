# Reasoning log

The working-out behind findings, conclusions, rejected paths and workarounds, in brief, so that a new session or a
switch of model starts with it in view. It covers EldenNet, GraceUI and DataPiler. Decisions stay in
[DECISIONS.md](DECISIONS.md) and plans in [ROADMAP.md](ROADMAP.md); an entry here points to them and does not repeat them.

**How to use it.**

- Read the index below at the start of a session and after a model switch, then open an entry by its code:
  `wp show WHY§12`. `wp find` and `wp grep` search it; `wp refs O59` lists the entries citing a decision.
- Append only. Entries are numbered in order and never renumbered or rewritten; a later finding adds a dated
  **Later** line to the entry, or a new entry that supersedes it.
- One entry per finding, a few lines: what was seen, what follows, what was ruled out, and the next step if any.
- **Labels**, as the research notes use them: **FACT** (seen in the owner's data, in source code at a named commit, or
  in a tool's output), **LIKELY** (strong inference, not yet checked), **SPECULATION** (a guess worth recording),
  **METHOD** (how something is done, and why), **DECIDED** (a pointer to the decision that settled it).
- **Tags:** `#roster` (the game's characters and their ids), `#datapiler`, `#smithbox`, `#text` (the game's text and
  its exports), `#maps`, `#voice`, `#lore`, `#graceui`, `#engine` (Min's engine and what EldenNet takes from it), `#release` (GitHub releases and this session's limits), `#process`.
- The privacy rules bind it as every file: nothing personal, no paths from the owner's PC, no game text beyond
  names the design already uses. No model identifiers.

## Index

| # | Date | Entry | Label | Tags |
|---|---|---|---|---|
| 1 | 2026-11-14 | Why this log exists | METHOD | #process |
| 2 | 2026-11-08 | Smithbox 2.2.6 shows one Item container per language, `item_dlc02` | FACT | #smithbox #text |
| 3 | 2026-11-08 | A text export's `Name` is its file name; the Export Output box shows the last export | FACT | #smithbox #text |
| 4 | 2026-11-08 | Smithbox forgets the Text Editor's Export Directory on restart | FACT | #smithbox |
| 5 | 2026-11-08 | The game's unused text entries read `[ERROR]` | FACT | #text #datapiler |
| 6 | 2026-11-08 | Why the map placements failed: probably no Map Editor in the owner's project | LIKELY | #smithbox #maps |
| 7 | 2026-11-08 | Row reads are one call per row; the batch path reads the wrong bank | FACT | #smithbox #datapiler |
| 8 | 2026-11-08 | What this session can and cannot do on GitHub | FACT | #release #process |
| 9 | 2026-11-08 | The names' text ids: 18, 328 and 428 | FACT | #roster #text |
| 10 | 2026-11-08 | "Named" is not "a named character" | FACT | #roster |
| 11 | 2026-11-08 | Name number 0 is the ordinary enemies' default | FACT | #roster |
| 12 | 2026-11-08 | Why the roster took four jobs (DP8) | METHOD | #roster #datapiler |
| 13 | 2026-11-08 | NPC row ids: the middle digits match the name number | LIKELY | #roster |
| 14 | 2026-11-08 | The expansion's characters: 50 names first in DLC 1 | FACT | #roster #text |
| 15 | 2026-11-08 | The 21 unresolved rows are left out | LIKELY | #roster |
| 16 | 2026-11-08 | Some bosses may be missing from NpcParam's names | LIKELY | #roster #lore |
| 17 | 2026-11-09 | Which kinds speak: evidence, not guesswork | DECIDED | #lore #voice |
| 18 | 2026-11-09 | The game has one spoken tongue and no dragon language | LIKELY | #lore |
| 19 | 2026-11-09 | Why voiceless speakers get generated voices, not borrowed ones | DECIDED | #voice |
| 20 | 2026-11-09 | Why Melina was mute, and why that changed | DECIDED | #lore #voice |
| 21 | 2026-11-09 | DataPiler 0.2.1 and 0.2.2 went out as patch zips | METHOD | #datapiler #release |
| 22 | 2026-11-14 | The design's row leads for Melina, Kalé and Boc are in the data | FACT (Later) | #roster |
| 23 | 2026-11-08 | WorldMapPlaceNameParam has only 10 rows | LIKELY | #roster #maps |
| 24 | 2026-11-09 | TalkParam is the route from a character to its recordings | METHOD | #voice #datapiler |
| 25 | 2026-11-08 | Detect works on the owner's PC: Smithbox's project record is read right | FACT | #datapiler #smithbox |
| 26 | 2026-11-08 | Smithbox ships two exes side by side | FACT | #smithbox #datapiler |
| 27 | 2026-11-08 | How to read Smithbox's source here | METHOD | #smithbox #process |
| 28 | 2026-11-08 | DataPiler's Windows-only failures and their causes | FACT | #datapiler |
| 29 | 2026-11-08 | Publishing pages: what the sweep misses and how publishes fail | METHOD | #process |
| 30 | 2026-11-08 | Other sessions as consultants | METHOD | #process #graceui |
| 31 | 2026-11-14 | Usage is the owner's call | METHOD | #process |
| 32 | 2026-11-14 | Smithbox 2.2.6 has no event-script editor | FACT | #smithbox #roster |
| 33 | 2026-11-14 | Most of the base game's bosses are missing from NpcParam's names | LIKELY | #roster |
| 34 | 2026-11-14 | Bosses sit in entity slots ending 800 to 899 | LIKELY | #roster #maps |
| 35 | 2026-11-14 | Smithbox lists every map as if loaded; DataPiler 0.2.x misread it | FACT | #smithbox #maps #datapiler |
| 36 | 2026-11-14 | The two routes to the bosses, and why both | METHOD | #roster #datapiler |
| 37 | 2026-11-14 | Only Smithbox's plain Map Editor makes the map data its server reads | FACT | #smithbox #maps |
| 38 | 2026-11-14 | A whole text container makes ordinary words look like game text | FACT | #datapiler #text |
| 39 | 2026-11-14 | The map UNKNOWN was DataPiler's own request | FACT | #smithbox #maps #datapiler |
| 40 | 2026-11-14 | Smithbox loads maps only while its Map Editor is drawn | FACT | #smithbox #maps |
| 41 | 2026-11-14 | Smithbox loads one map at a time; ten at once kept one | FACT | #smithbox #maps #datapiler |
| 42 | 2026-11-14 | Where a map load's time goes, and why more PC does not help | FACT | #smithbox #maps |
| 43 | 2026-11-14 | One map that never loads held up the rest | LIKELY | #smithbox #maps #datapiler |
| 44 | 2026-11-14 | What DataPiler can and cannot learn from Smithbox about a failed map | FACT | #smithbox #maps #datapiler |
| 45 | 2026-11-14 | en-0007's full load: 46 of 47 maps, one empty, and Smithbox's auto-save | FACT | #smithbox #maps #datapiler |
| 46 | 2026-11-14 | What the legacy dungeons' boss slots hold | FACT | #roster #maps |
| 47 | 2026-11-14 | Bosses' bar names are numbered by their model | LIKELY | #roster #text |
| 48 | 2026-11-14 | The other unused names: 13 in the named range, 16 of one unknown family | LIKELY | #roster #text |
| 49 | 2026-11-14 | The smaller dungeons hold the model pattern: 61 of 101 named models placed | FACT | #roster #maps |
| 50 | 2026-11-14 | Empty tiles looked like failed loads; Smithbox's file keys tell them apart | FACT | #smithbox #maps #datapiler |
| 51 | 2026-11-14 | The graphics driver went down with Smithbox holding about 720 maps; jobs now keep what they read | LIKELY | #smithbox #maps #datapiler |
| 52 | 2026-11-14 | What Smithbox puts on the graphics card for a map, and the OpenGL build as the way round | FACT | #smithbox #maps #datapiler |
| 53 | 2026-11-14 | The OpenGL build's first 300 maps: no texture warnings; one-go jobs, Resume job and an icon | LIKELY | #smithbox #maps #datapiler |
| 54 | 2026-11-14 | The open world's boss slots: 98 of 101 named models placed | FACT | #roster #maps |
| 55 | 2026-11-14 | The named characters' flag blocks, and the jobs for their placements and forms | METHOD | #roster #flags #datapiler |
| 56 | 2026-11-14 | The map: the game's map only as the player's own copy; trails are sightings; masking against one upload | METHOD | #graceui #map #spoilers #licences |
| 57 | 2026-11-14 | The named characters' census: placements, forms and the talk id's two patterns | FACT | #roster #datapiler #talk |
| 58 | 2026-11-14 | Where the map's coordinates can come from: five map tables, but no placement positions through Smithbox | FACT | #map #datapiler #graceui |
| 59 | 2026-11-14 | The world map's frame: about one map unit per metre, north up, three sheets | LIKELY | #map #datapiler #graceui |
| 60 | 2026-11-15 | The areas' colour keys: 16 area names and 49 finer areas on the base map, 11 names in the expansion | LIKELY | #map #datapiler #graceui |
| 61 | 2026-11-15 | The game's map tiles: four sheets, five zoom levels, a variant per revealed piece | LIKELY | #map #datapiler #graceui |
| 62 | 2026-11-15 | Each tile code bit is one map piece, and map units are level-0 pixels | LIKELY | #map #datapiler #graceui |
| 63 | 2026-11-15 | A tile's pieces that never meet have a variant each, laid together | FACT | #map #datapiler #graceui |
| 64 | 2026-11-15 | Level 2's export holds a second drawing of the map; levels 2 to 4 nest by two | LIKELY | #map #datapiler #graceui |
| 65 | 2026-11-15 | With the check, level 2 is one image; its base and pieces 15 to 20 stand apart | LIKELY | #map #datapiler #graceui |
| 66 | 2026-11-15 | Level 2's export is two tile sets under one set of names; Smithbox's export overwrites by name | LIKELY | #map #datapiler #graceui |
| 67 | 2026-11-15 | The game's container holds both level-2 sets; the one that fits levels 0, 3 and 4 is incomplete | FACT | #map #datapiler #graceui |
| 68 | 2026-11-15 | Level-1 codes do not settle levels 0 and 1; set A is not level 1 at a quarter | FACT | #map #datapiler #graceui |
| 69 | 2026-11-15 | Every base-map level checked: level 0 is mixed like level 2, level 1 hardly passes | FACT | #map #datapiler #graceui |
| 70 | 2026-11-15 | Level 1 sits at the top of its grid; levels 0 and 1 hold an old tile set four columns off | FACT | #map #datapiler #graceui |
| 71 | 2026-11-15 | The expansion's levels 1 and 2 sit at the top left; level 2 is 2,089 / 10,496 of level 0; the old set fills 960 places | FACT | #map #datapiler #graceui |
| 72 | 2026-11-15 | The game's tile masks are a KRAK-packed binder of four files, one per sheet; DataPiler opens it with the game's Oodle | FACT | #map #datapiler #smithbox |
| 73 | 2026-11-15 | The game's masks name today's tiles; levels 3 and 4 are an older layout; one geometry on every sheet | FACT | #map #datapiler #graceui |
| 74 | 2026-11-15 | en-0029: today's tiles match at levels 0 and 1 and on the expansion; level 2 met the older layout's level 3 | FACT | #map #datapiler |
| 75 | 2026-11-15 | en-0030: levels 1 and 2 match on every sheet; level 0 against level 1 drifts from the top left | FACT | #map #datapiler |
| 76 | 2026-11-15 | Every level of every map draws whole from the masks; the levels' true sizes are 0.74857 and 0.19875 | FACT | #map #datapiler #graceui |
| 77 | 2026-11-15 | en-0031: at the measured sizes every variant the game draws passes, at every level of every map | FACT | #map #datapiler |
| 78 | 2026-11-15 | The world-to-map fit: one transform per open-world sheet; the underground and the dungeons go through the game's conversion | FACT | #map #datapiler #graceui |
| 79 | 2026-11-15 | The areas' colour image is very likely the map-information binders, one per kilometre square | LIKELY | #map #datapiler #graceui |
| 80 | 2026-11-15 | The areas' image ships converted: each map-information square holds the two keys' row numbers | FACT | #map #datapiler #graceui |
| 81 | 2026-11-15 | The map-information images' other channels: very likely weather (blue) and the Mimic's Veil (second image) | LIKELY | #map #datapiler #graceui |
| 82 | 2026-11-15 | Confirmed: the blue channel holds the weather key's rows, the second image the mimicry key's | FACT | #map #datapiler #graceui |
| 83 | 2026-11-15 | The design fold: proposal 0001's parts adopted by reference, what waits for G18 to G20, and three settlements of the fold's own | DECIDED | #graceui #design |
| 84 | 2026-11-15 | Under G15 EldenNet links no hook library, so any later hook of its own needs the owner | FACT | #graceui #hooks #design |
| 85 | 2026-11-15 | Two of the inventory's suggestions rejected: G1's later HTML spike and its fallbacks stay | METHOD | #graceui #design |
| 86 | 2026-11-15 | One owner per hooked function: what the evidence on hook libraries rules out, and what it does not | FACT | #hooks #graceui #design |
| 87 | 2026-11-15 | SkyrimNet's plugin model: plugins call its published functions, and nothing hooks SkyrimNet | FACT | #plugins #skyrimnet #design |
| 88 | 2026-11-15 | One plugin cluster: what a shared host can tell apart, and why a library clash needs a check | FACT | #plugins #design |
| 89 | 2026-11-15 | The terrain's heights are very likely the ground models, stored at three levels of detail | LIKELY | #map #datapiler #graceui |
| 90 | 2026-11-15 | The terrain's heights are the ground models: one 1 km square read, its tiles placed by their numbers alone | FACT | #map #datapiler #graceui |
| 91 | 2026-11-15 | The expansion, a far view and the underground in the file dictionary, and two jobs to read them | LIKELY | #map #datapiler #graceui |
| 92 | 2026-11-15 | The expansion, the far view and the underground: what en-0037 and en-0038 show | FACT | #map #datapiler #graceui |
| 93 | 2026-11-15 | The underground's floor rule: the topmost floor no steeper than 50°, checked on 250 placed characters | FACT | #map #datapiler #graceui |
| 94 | 2026-11-15 | The underground's floor rule, revised: the topmost walkable floor reachable from where the characters stand | FACT | #map #datapiler #graceui |
| 95 | 2026-11-15 | Min's engine arrived: a closed Windows DLL with an MIT interface, not a source blueprint | FACT | #engine #process |
| 96 | 2026-11-15 | SkyrimNet's own bio files confirm SN03 §5: ten prose parts, and the same parts empty for the player | FACT | #engine #roster |
| 97 | 2026-11-15 | SkyrimNet 1.2.0 read: our SN notes stand, with two corrections and a few new parts | FACT | #engine #roster |
| 98 | 2026-11-15 | What SkyrimNet 1.2.0 shows our design lacks: thirteen points for our own engine | LIKELY | #engine #roster #process |
| 99 | 2026-11-16 | The Skyrizz session's measurements of SkyrimNet: code gathers, models pick; eight points for our engine | LIKELY | #engine #process |
| 100 | 2026-11-16 | The engine's first increment: parts join exactly; two libraries fetched, not vendored; the template root | METHOD | #engine #process |
| 101 | 2026-11-16 | The engine's second increment: WinHTTP by its full path; the handle's and the probes' choices | DECIDED | #engine #process |
| 102 | 2026-11-16 | Wine 9.0 runs the Windows build, and its WinHTTP differs from Windows' in three ways | FACT | #engine #process |
| 103 | 2026-11-16 | The design on C++: one mapping, the behaviour unchanged, and what the re-base found | METHOD | #engine #graceui #process |
| 104 | 2026-11-16 | GraceUI's server is our own: an independent review's reasons, and its correction of the connection cap | DECIDED | #graceui |
| 105 | 2026-11-16 | The gear's names are in WeaponName (11) and ProtectorName (12), by row id; no row needs the expansion's layers | FACT | #gamedata #datapiler |
| 106 | 2026-11-16 | The built engine on the re-based rules: what Clang's static-object errors, R3 and the caps took | METHOD | #engine |
| 107 | 2026-11-16 | EldenNet's pages moved to PAGES: where the fold cut GraceUI's §4, and what G20 left open | METHOD | #graceui #design |
| 108 | 2026-11-16 | The engine on real Windows, run 1: WinHTTP works over the LAN and over TLS; two transport tests fail where Wine passes | FACT | #engine |
| 109 | 2026-11-16 | WinHTTP's connect deadline and TLS wording: what Wine shows, and the rules that do not rest on it | METHOD | #engine |
| 110 | 2026-11-16 | O88 folded: a preset's providers are matched, not defined; the reserved slots; what "same as dialogue" shows | DECIDED | #engine #design |
| 111 | 2026-11-16 | The clang-cl build with xwin, and the SEH copy: what clang does to `__try`, guard pages, and the pinned CRT | FACT | #engine #gal #build |
| 112 | 2026-11-16 | The sanitizers and the first fuzz targets on Linux: two test defects, no finding in our code | METHOD | #engine |
| 113 | 2026-11-16 | Direct3D 12 runs under Wine in the cloud container (vkd3d on Mesa's software Vulkan, on Xvfb); the toolchain saved as a setup script | FACT | #graceui #build #process |
| 114 | 2026-11-16 | A Direct3D 12 swap chain's GetDevice names the device under Wine and vkd3d-proton, not the queue; the present queue found four ways | FACT | #graceui #overlay |
| 115 | 2026-11-16 | Dear ImGui 1.93's DX12 backend waits for the GPU on texture uploads; its static atlas mode avoids every upload during play | FACT | #graceui #overlay |
| 116 | 2026-11-16 | Wine's wined3d on OpenGL hung on Xvfb when a second thread made a DXGI factory; its Vulkan renderer did not | FACT | #graceui #build #wine |
| 117 | 2026-11-16 | The render half's cost under Wine on software Vulkan, and how it was measured | METHOD | #graceui #overlay #perf |
| 118 | 2026-11-16 | The engine's release binaries carry the build machine's absolute PDB path; /pdbaltpath removes it | FACT | #engine #build #privacy |
| 119 | 2026-11-16 | Dear ImGui's and MinHook's licences read for adoption; Dear ImGui's contribution rule on AI prose is not a licence term | FACT | #graceui #licence |
| 120 | 2026-11-16 | The GAL's types in en-proto: what the design left open, and what C++ took | METHOD | #engine |
| 121 | 2026-11-16 | en-gal's offline part: the reader, the rules and the schemas, and the choices the design left | METHOD | #engine #gal |
| 122 | 2026-11-16 | toml++ 3.4.0's traps: a header without a key, non-ASCII where whitespace may stand, `}` for a value, a date, a space and a digit | FACT | #engine #gal |
| 123 | 2026-11-16 | The fake game: our own stand-in for a game, its invented facts and the choices later parts inherit | DECIDED | #engine #gal #process |
| 124 | 2026-11-16 | Wine on Xvfb for tests: the crash dialog that waits, the desktop bound to no display, and what reaches Linux | FACT | #engine #build #process |
| 125 | 2026-11-16 | Assembly whose bytes do not move: clang relaxes every jump at -O0 | FACT | #engine #build |
| 126 | 2026-11-16 | Wine 9.0 on Xvfb hands injected keys to DirectInput, but releases them on X focus and grab events; a scan code alone gives no virtual key | FACT | #graceui #wine #input |
| 127 | 2026-11-16 | Dear ImGui 1.93's Win32 backend asks the calling thread for keys and the cursor, and its io calls are not thread-safe: GraceUI feeds Dear ImGui itself | FACT | #graceui #overlay #input |
| 128 | 2026-11-16 | Dear ImGui's 16-bit characters turn every code point past U+FFFF into U+FFFD; its queue takes one change of a key or button a frame | FACT | #graceui #overlay #input |
| 129 | 2026-11-16 | Wine's DirectInput and XInput as GraceUI hooks them: one GetDeviceState for keyboard and mouse, five XInput DLLs, no pad | FACT | #graceui #wine #input |
| 130 | 2026-11-16 | The game's window procedure is filtered by a subclass in the window's own character set, not a detour on the game's code | DECIDED | #graceui #overlay #input |
| 131 | 2026-11-16 | The closing press is often down before any read sees it: masks are armed at the first read after a close, with a 250 ms window | DECIDED | #graceui #input |
| 132 | 2026-11-16 | The input half's cost under Wine, and how it was measured | METHOD | #graceui #input #perf |
| 133 | 2026-11-16 | What the harness cannot show of the input half, for S4 | METHOD | #graceui #input #spike |
| 134 | 2026-11-16 | ABI 1's first table keeps D73's order: typed where the design names a slot's shape, the bridge protocol's four reserved in their places | DECIDED | #graceui #abi |
| 135 | 2026-11-16 | GraceUI's anti-cheat check runs in the first GraceUI_RequestApi, not on the init thread: connect's first answer must already know it | DECIDED | #graceui #anticheat |
| 136 | 2026-11-16 | An anti-cheat hit simulated with no anti-cheat software: names a test build is told; Wine's Toolhelp names a process's parent | METHOD | #graceui #anticheat #wine |
| 137 | 2026-11-16 | Dear ImGui frees a draw list unused for 60 s of its own time, and its time leaps when frames resume: a notice would allocate during play | FACT | #graceui #overlay |
| 138 | 2026-11-16 | Two copies of GraceUI share only their two exports: the active copy names the others; an idle one asks only after its own first request | DECIDED | #graceui |
| 139 | 2026-11-16 | The harness's ABI, copies and anti-cheat scenarios under Wine: a renderer that warms up after the hooks, and two flaky tests made sound | METHOD | #graceui #wine #process |
| 140 | 2026-11-16 | A panel's draw list is in the panel's own pixels, copied and checked on the mod's thread, and drawn from its triple buffer where it lies: no retire slot | DECIDED | #graceui #overlay #abi |
| 141 | 2026-11-16 | Dear ImGui 1.93's DX12 backend takes one draw data a frame, honours each command's vertex offset, and grows its buffers itself | FACT | #graceui #overlay |
| 142 | 2026-11-16 | Dear ImGui 1.93 for a mod's headless panel: its atlas built with ImFontAtlasBuildMain and given its texture id before the first frame; API renames that bite | FACT | #graceui #overlay #abi |
| 143 | 2026-11-16 | Textures: the registration window ends at a mod's first submit; a later mod's textures are one upload at the next frame; nothing is freed on the present thread | DECIDED | #graceui #overlay |
| 144 | 2026-11-16 | Faces: warm-up seals the table slot by slot, so a face being written then is late, never lost; a mod's broken font is left out, not thrown | DECIDED | #graceui #overlay |
| 145 | 2026-11-16 | ThreadSanitizer's runtime defines the global allocation functions: a counting operator new cannot run under it | FACT | #graceui #build #test |
| 146 | 2026-11-16 | The harness's panels scenario under Wine: what it reads back, its figures, and a frame-cost check that fails under another build's load | METHOD | #graceui #wine #perf |
| 147 | 2026-11-16 | What S10.4.c leaves to S4 and to the bridge protocol: a panel holder's events, the backend's buffer growth in the game, a mod's untrusted font | METHOD | #graceui #spike #input |
| 148 | 2026-11-16 | Our x86-64 length decoder against LLVM: the choices where processors and LLVM differ | FACT | #engine #gal |
| 149 | 2026-11-16 | The derivation round's tool: leads, the numbers-only writer, and how each fact is found offline | METHOD | #engine #gal #datapiler #privacy |
| 150 | 2026-11-16 | probe-exe on fakegame.exe: what R0 recovers, by toolchain | FACT | #engine #gal |
| 151 | 2026-11-16 | toml++ built two ways in one program: two vtables of one name | FACT | #engine #build |
| 152 | 2026-11-16 | World-state claims and an input grant: read where the providers write them, fenced on both sides | DECIDED | #graceui #input #overlay |
| 153 | 2026-11-16 | The window filter follows the bound swap chain's window; a console's window is never the game's | DECIDED | #graceui #input |
| 154 | 2026-11-16 | The shared atlas has a room, and is baked on GraceUI's own thread, which stays for the session | DECIDED | #graceui #overlay #perf |
| 155 | 2026-11-16 | Dear ImGui 1.93's atlas: an 8192-pixel limit that asserts, a context made on one thread and used on another, ClearTexData for a legacy atlas | FACT | #graceui #overlay |
| 156 | 2026-11-16 | GraceUI lets go of a removed or replaced device; the render path stays off until a later version rebuilds | DECIDED | #graceui #overlay |
| 157 | 2026-11-16 | The review's smaller fixes, and what they leave as it is | DECIDED | #graceui #input #abi |
| 158 | 2026-11-16 | Under Wine a Unicode folder needs a UTF-8 locale; the fuzzers' coverage needs the rules instrumented | METHOD | #graceui #wine #test |
| 159 | 2026-11-16 | en-gal in game: the seam, our own labels, and what the design left open | METHOD | #engine #gal |
| 160 | 2026-11-16 | NativeSys: guard pages are never touched, and the stacks are found by them | DECIDED | #engine #gal |
| 161 | 2026-11-16 | Wine's ReadProcessMemory on its own process ignores page protection and costs about 0.1 ms a copy | FACT | #engine #gal #wine |
| 162 | 2026-11-16 | The GAL in the fake game under Wine: what the test DLL finds, and what it costs | FACT | #engine #gal #wine |
| 163 | 2026-11-16 | One TOML pre-check for the engine, and a quote inside a bare word | DECIDED | #engine #gal #llm |
| 164 | 2026-11-16 | The layout's schema grows past ARCH §6.3's sketch: registers as data, our own labels for the classes, the game's names as values | DECIDED | #engine #gal |
| 165 | 2026-11-16 | The SysLayer is narrower than ARCH §6.4's sketch, so that FakeSys tests the code that runs in game | DECIDED | #engine #gal |
| 166 | 2026-11-16 | The real game's layout is a fresh file; some of ARCH §6.1's fields are placed differently for now | DECIDED | #engine #gal |
| 167 | 2026-11-16 | The task goes Dormant also when its own reading says the session is not solo | DECIDED | #engine #gal |
| 168 | 2026-11-16 | The render feed keeps the last walk's addresses between snapshots, checked again at every read | DECIDED | #engine #gal #perf |
| 169 | 2026-11-16 | What part 4 leaves unbuilt, and why | DECIDED | #engine #gal |
| 170 | 2026-11-16 | The probe's slot search faults on purpose on the game thread: R1 without a crash reporter, or with the rpm copy | METHOD | #engine #gal |
| 171 | 2026-11-16 | R0 refused the protected game's executable: the PE reader was stricter than the loader on sections | FACT | #engine #gal |
| 172 | 2026-11-16 | An early-loaded DLL's crash filter is replaced by the game's own C runtime when it starts | FACT | #engine #launcher |
| 173 | 2026-11-16 | The launcher's watch block: one page of shared memory, made by whoever comes first | DECIDED | #engine #launcher |
| 174 | 2026-11-16 | BLAKE3's hash mode is our own, against its published vectors | DECIDED | #engine #launcher |
| 175 | 2026-11-16 | The gate's badge after a loading screen, and the gate when snapshots stop | DECIDED | #engine #core |
| 176 | 2026-11-16 | The core's slice decides a playthrough from PlaythroughReady, and the store's step is a no-op for now | DECIDED | #engine #core |
| 177 | 2026-11-16 | The launcher's late loads, closing the game, and what it checks of a DLL | DECIDED | #engine #launcher |
| 178 | 2026-11-16 | Wine's module list of another process and the watcher's timing | FACT | #engine #launcher #wine |
| 179 | 2026-11-16 | Once, the fake game ended with 0xC0000409 after its last line, with galtest loaded at the window | SPECULATION | #engine #launcher #wine |
| 180 | 2026-11-16 | R0's FD4 finder refused every hit: it took the first read-only load after the idiom for the path | FACT | #engine #gal |
| 181 | 2026-11-16 | The idiom's wildcards also match loads that are no RIP-relative statics: every hit is decoded now | LIKELY | #engine #gal |
| 182 | 2026-11-16 | An idiom has as many hits as the game has inlined accessors: the scan's cap of 4,096 was too low | FACT | #engine #gal |
| 183 | 2026-11-16 | Accepted FD4 sites without a name layout leave the singletons to R1's getter calls | DECIDED | #engine #gal |
| 184 | 2026-11-16 | A table of one is no table: the group's index from a lone pointer was an artefact | FACT | #engine #gal |
| 185 | 2026-11-16 | A static whose class has no RTTI cannot be found offline; R1 finds its slot | LIKELY | #engine #gal |
| 186 | 2026-11-16 | Product names are compared without symbols, in the derivation and at the gate | DECIDED | #engine #gal #datapiler |
| 187 | 2026-11-16 | The bytes R0 could not decode are no gap of the decoder's | METHOD | #engine #gal |
| 188 | 2026-11-16 | derive.json's schema 1 is frozen by DataPiler 0.2.41's copy of it | DECIDED | #engine #gal #datapiler |
| 189 | 2026-11-16 | The executable has a second code section the derivation does not read | SPECULATION | #engine #gal |
| 190 | 2026-11-16 | A stack range outlives its thread: the reader refused the game's own objects where a stack had been | FACT | #engine #gal #wine |
| 191 | 2026-11-16 | eldennet.dll's start as built, where it departs from ARCH §3.2 | DECIDED | #engine #dll |
| 192 | 2026-11-16 | The recorder writes numbers-only JSON Lines, each line a report of the kind "recording" | DECIDED | #engine #dll #datapiler |
| 193 | 2026-11-16 | R1 in the DLL: the derivation over the live image, then one call at the Mark, then the game thread's searches | DECIDED | #engine #gal #dll #r1 |
| 194 | 2026-11-16 | settings.toml for v0.1: strict, ARCH §11.5's later tables accepted, three tables of this build's own | DECIDED | #engine #dll #settings |
| 195 | 2026-11-16 | The DLL's imports, exports and greps | FACT | #engine #dll #ci |
| 196 | 2026-11-16 | Under Wine, a stack overflow passed on by any DLL's crash filter hangs the process | FACT | #engine #dll #wine #launcher |
| 197 | 2026-11-17 | R0c into gal/: what became a lead, where it lives, and a lead [task] that holds only its group | DECIDED | #engine #gal #r1 |
| 198 | 2026-11-17 | R1 calls only on the reviewed leads; its group check had never compared | FACT | #engine #dll #r1 |
| 199 | 2026-11-17 | The launcher's --probe asks through the watch block; a refusal stops the DLL | DECIDED | #engine #launcher #dll #r1 |
| 200 | 2026-11-17 | The console polls every 200 ms: a quick step passes it unseen in a Release build | FACT | #engine #launcher #wine |
| 201 | 2026-11-17 | spike-report reads DESIGN §16.1's pass rules so | DECIDED | #engine #spike |
| 202 | 2026-11-17 | Replay agrees: en-core's slice answers the same changes on a recording's inputs | FACT | #engine #core #spike |
| 203 | 2026-11-17 | The R1 package, 0.1.0-spike.1 | METHOD | #engine #r1 #spike |
| 204 | 2026-11-17 | C1's slot could answer an older snapshot after a newer one | FACT | #engine #dll |
| 205 | 2026-11-17 | Under Wine, GetKeyState from the window procedure's filter costs about 100 µs a call: GraceUI reads the modifiers only for a fresh press of a bound key | FACT | #graceui #input #perf |
| 206 | 2026-11-17 | R1 run 1 on the owner's PC: lld-link's merged manifest stopped the launcher on Windows (spike.2 fixed it); then the live derivation named 9 of 217 FD4 class objects, so no call was made; spike.3 reads namespaced names and reports where the name pointers lead | FACT | #gal #r1 #derive #launcher |
| 207 | 2026-11-17 | R1 run 2: the game's FD4 class objects hold no names in the image (their words point to the heap), so R1 names the singletons by their instances' RTTI (O92, spike.4) | FACT | #gal #r1 #derive |
| 208 | 2026-11-17 | R1 run 3: CSTask named by its instance's RTTI, the checks passed and register_task returned ok at the title screen, but the task never ticked there; run 4 loads a save after the call (owner's choice) | FACT | #gal #r1 #task |
| 209 | 2026-11-17 | R1 run 4: with a world loaded the task still never ran, and the game crashed on quit in a module not ours; spike.5 searches the hub for where our task was stored | FACT | #gal #r1 #task #crash |
| 210 | 2026-11-20 | Wine 9.0 aborts at start-up ("free(): invalid pointer") when TMPDIR is set, as the cloud environment now sets it: every Wine process runs without it | FACT | #wine #ci #graceui |
| 211 | 2026-11-20 | The launcher no longer links en-core: R1's probe report vocabulary moved into en-gal, and a configure-time check refuses any link that breaks ARCH §2.3's layering | DECIDED | #layering #launcher #build |
| 212 | 2026-11-20 | GraceUI DESIGN §3.3's contrast table rounds each ratio twice, so eight cells read 0.1 high; the tokens reproduce it within 0.06 and the thresholds are checked on the unrounded ratio | METHOD | #graceui #tokens #contrast |
| 213 | 2026-11-20 | nlohmann/json 3.12.0 parses a 250 KiB state frame in about 3.3 ms and writes it in 0.7 ms, against GraceUI §8's 0.5 ms each way for the bridge's largest payload: a risk for S4 | FACT | #graceui #protocol #perf |
| 214 | 2026-11-20 | grace-serve's head parser refuses a NUL, a bare CR, a bare LF or an empty first line as it arrives, so a client that never ends its head is refused at once; everything else waits for the whole head, then Host first | METHOD | #graceui #server #security |
| 215 | 2026-11-20 | graceui-sys's headers are vendored into EldenNet at GraceUI f51bcc7, each checked by SHA-256, not fetched; Dear ImGui's core is fetched at GraceUI's own pin | DECIDED | #engine #graceui #licences |
| 216 | 2026-11-20 | en-overlay's look for GraceUI ends at once when none is installed, rather than after graceui-sys's 30 s | DECIDED | #engine #graceui #dll |
| 217 | 2026-11-20 | The development Mark changes hands through the watch block, on Pause: the launcher lets its key go when EldenNet's GraceUI hotkey is registered | DECIDED | #engine #launcher #graceui |
| 218 | 2026-11-20 | The F11 view is behind debug.panel (SP8): without it the view, its panel, atlas and hotkey are not made and F11 reaches the game | DECIDED | #engine #dll #graceui |
| 219 | 2026-11-20 | E12, a failed GraceUI pre-flight, is told and the game starts headless; --probe and the spikes only log it | DECIDED | #launcher #graceui |
| 220 | 2026-11-20 | Dear ImGui's core in eldennet.dll brought thread-safe statics and ReadConsoleW; /Zc:threadSafeInit- on the core removes the first, the rest are reviewed imports | FACT | #engine #dll #ci |
| 221 | 2026-11-20 | The first end-to-end run with GraceUI: the launcher, eldennet.dll and GraceUI's test build in our fake game on Direct3D 12 under Wine, F11's panel read back from the frame | FACT | #engine #graceui #wine #test |
| 222 | 2026-11-20 | Augur runs straight after en-core's lifecycle slice in the same step, not before it, so its lifecycle events and the loading screen are seen in the step they happen | DECIDED | #engine #augur #core |
| 223 | 2026-11-20 | What ARCH §8 left open, settled for Augur v0.1: lifecycle events as rule files, a 1 s Struck window, engagement re-armed only once apart, FNV-1a hashes and others | DECIDED | #engine #augur |
| 224 | 2026-11-20 | Under 50% snapshot loss Augur invents no event and fires none earlier; brief states and per-hit events are what differ | FACT | #engine #augur #test |
| 225 | 2026-11-20 | A recording's reader now rebuilds what Augur reads of a snapshot, so replay and golden drive Augur from the DLL's recordings, schema unchanged | DECIDED | #engine #augur #spike #datapiler |
| 226 | 2026-11-20 | Augur v0.1's step costs about 26 µs on a busy synthetic step (32 characters, 25 rules), p99 51 to 71 µs, against ARCH's 1 ms budgets; and a CMake tree's cached Python can vanish | FACT | #engine #augur #perf #build |
| 227 | 2026-11-20 | R1 run 5: with no other mods, register_task returned ok again and our task's address was nowhere in 4 MB of the hub's memory three pointers deep; spike.6 searches all memory and summarises the callee | FACT | #gal #r1 #task |
| 228 | 2026-11-20 | Cinzel publishes static faces only at Regular, Bold and Black, so GraceUI's 500 and 600 weights take Regular and Bold; of the faces, EB Garamond and Noto Sans Mono carry the arrows U+2190 to U+2193 | FACT | #graceui #fonts |
| 229 | 2026-11-20 | Under Wine on lavapipe a full-screen overlay's frame-cost p99 is the software renderer's: GraceUI's menu holds its 1 ms present-thread budget at 640x360 and is measured at 1920x1080, the game's run settling it | METHOD | #graceui #perf #wine |
| 230 | 2026-11-20 | en-overlay takes GraceUI's own rules: GRACEUI_UNBOUND is a success with an id, an unbound Mark leaves the launcher's Pause in place, and graceui-sys's STOP_IF_ABSENT replaces our own early stop | DECIDED | #engine #graceui #dll |
| 231 | 2026-11-20 | graceui-sys at 3f1352b leaves installed at 0 for a GraceUI found by its name; en-overlay reads a find by name as installed until it is fixed | FACT | #graceui #engine |
| 232 | 2026-11-20 | Under Wine 9, loading graceui.dll before the game's first window can hang in dxgi.dll's load while other threads make USER calls; EldenNet loads it only after the window | FACT | #wine #graceui #ci |
| 233 | 2026-11-20 | The launcher's pre-flight is tested on a manifest GraceUI's own manifest.py wrote, compiled into the tests byte for byte, with the extra keys accepted | METHOD | #engine #launcher #graceui |
| 234 | 2026-11-20 | R1 spike.6: the callee summarised from the live image, and the whole process searched for our task, our own copies told apart by the probe DLL's own heap | DECIDED | #gal #r1 #task |
| 235 | 2026-11-20 | R1 run 6: register_task's callee is a 93-byte wrapper that keeps the group and the task in saved registers and hands them to two inner calls; 8 of 10.6 GiB searched in a second held no copy of our task | FACT | #gal #r1 #task |
| 236 | 2026-11-20 | R1 spike.7: the task followed past the wrapper: arguments mapped into its calls, its callees summarised one level deeper, all memory read, our vtable and slots searched, the hub's memory diffed across the call | DECIDED | #gal #r1 #task |
| 237 | 2026-11-20 | grace-input's record of the last close lost the holder, so a close by the menu key or the player's exit skipped the host's close bookkeeping | FACT | #graceui #input |
| 238 | 2026-11-20 | GraceUI, not the space, enforces a destructive command's confirmation | DECIDED | #graceui #protocol |
| 239 | 2026-11-20 | A stream is cleared by an empty batch past a gap, with no message kind of its own | METHOD | #graceui #protocol |
| 240 | 2026-11-20 | R1 run 7: the wrapper hands the group and the task to a tail-called function that bounds the group by 168 and refuses through a null test; all readable memory held no copy of our task, its vtable or a slot | FACT | #gal #r1 #task |
| 241 | 2026-11-20 | A mod's panel that takes input gets no characters through ABI 1 | FACT | #graceui #abi |
| 242 | 2026-11-20 | A vtable-slot overlay loaded before GraceUI is hooked through; one loaded after calls through GraceUI's hook | METHOD | #graceui #overlay |
| 243 | 2026-11-20 | Under Wine 9 the process's handle count and private bytes read 0 | FACT | #graceui #wine |
| 244 | 2026-11-20 | ABI 1.1's input flag is appended after the reserved word, not renamed from it | METHOD | #graceui #abi |
| 245 | 2026-11-20 | graceui-sys's bufferless poll asked with a 16-byte event, so a mod using it never saw a holder's events | FACT | #graceui #abi |
| 246 | 2026-11-20 | A holder that ended on another thread between its grant and the event mailbox's enable never got INPUT_ENDED | FACT | #graceui #input |
| 247 | 2026-11-20 | R1 spike.8: the call's return value kept, instructions reported as forms, loaded values followed to their sources, the tail function's table read before and after the call, and a control diff | DECIDED | #gal #r1 #task |
| 248 | 2026-11-20 | R1 run 8: the call returns 1, and the group's entry exists, yet nothing keeps our task; the tail function hands the task to a function of the group's own | FACT | #gal #r1 #task |
| 249 | 2026-11-20 | An observer's run (S0-E, S4) stops T1 at Register; what a stopped start leaves running | FACT | #engine #dll #spike #graceui |
| 250 | 2026-11-20 | In the fake game, a rack line is no sign of our call; the hub's live count is | METHOD | #fakegame #harness #spike |
| 251 | 2026-11-20 | The observer's runs' words join schema 1; the game's environment is always the launcher's own block | DECIDED | #engine #launcher #spike #formats |
| 252 | 2026-11-20 | ABI 1 never had the display-changed event that GraceUI's design promised | FACT | #graceui #abi |
| 253 | 2026-11-20 | Dear ImGui bakes a face at a whole-pixel size, so GraceUI rounds a scaled face's size itself | FACT | #graceui #fonts |
| 254 | 2026-11-20 | A rise in a frame-cost median after a change with nothing per-frame was the machine's: measure old and new alternately | METHOD | #graceui #ci |
| 255 | 2026-11-20 | The owner's package for S0-E and S4: one run file each, GraceUI copied into the game's folder, F11's view switched on | DECIDED | #engine #launcher #graceui #spike |
| 256 | 2026-11-20 | EldenNet's vendored graceui_test.h lagged GraceUI 0.2.1; the GraceUI case now reads the version it expects from graceui.toml | FACT | #engine #graceui #harness |
| 257 | 2026-11-20 | Two OFL fonts' glyph bytes read as an address to the sweep; a narrow allow line in Skyrizz | FACT | #privsweep #graceui |
| 258 | 2026-11-20 | R1 spike.9: the group's own function read with our task and the entry followed in, the fields it reads, walks from the singleton and the entry, and tagged pointers | DECIDED | #gal #r1 #task |
| 259 | 2026-11-20 | G32's switch is a command of GraceUI's own space, not a setting | DECIDED | #graceui #notices |
| 260 | 2026-11-20 | A quiet mod's notice is checked, answered ok and kept, never dropped | DECIDED | #graceui #notices |
| 261 | 2026-11-20 | A test export that read GraceUI's thread's state without a lock crashed a full harness run | LIKELY | #graceui #harness |
| 262 | 2026-11-20 | GraceUI's kept settings and a new ui.scale were saved and told only when the menu next opened | FACT | #graceui #settings |
| 263 | 2026-11-20 | A pad chord's order is carried by splitting ABI 1's pad_chord word, key low and anchor high | METHOD | #graceui #input |
| 264 | 2026-11-20 | An independent review given the code and the rules found GraceUI's faults at the joins between modules | FACT | #graceui #security |
| 265 | 2026-11-20 | A space's settings publications are held to its registration, and a secret's value is masked rather than refused | DECIDED | #graceui #security |
| 266 | 2026-11-20 | GraceUI 0.2's in-game menu draws a mod's settings and shortcut rows only, so EldenNet's Home cards show in the browser alone | FACT | #graceui #engine |
| 267 | 2026-11-20 | EldenNet's first pages: settings written on T11, a refused settings file never written over, and the developer settings on Settings › Developer | DECIDED | #engine #pages |
| 268 | 2026-11-20 | A browser of our own joins GraceUI's server from Linux while the fake game runs under Wine | METHOD | #engine #graceui #harness |
| 269 | 2026-11-20 | A range-for over a member of a temporary nlohmann::json dangles in C++20 | FACT | #engine |
| 270 | 2026-11-20 | One body per topic: GraceUI's components read fixed fields, so a page with two lists needs two topics | FACT | #graceui #pages |
| 271 | 2026-11-20 | GraceUI's menu kept its copy of a topic only while it watched it, so a static status watched first by the browser would never have reached it | FACT | #graceui #pages |
| 272 | 2026-11-20 | F11's view lists Augur's events in Augur's own words, en-core's changes kept second, each event's source marked by colour | DECIDED | #engine #augur #overlay #test |
| 273 | 2026-11-20 | Check an "as the other view does" premise against the code before building to it | FACT | #graceui #pages |
| 274 | 2026-11-21 | EldenNet's AI pages: keys as GraceUI secret settings, kept with a one-user access list, and what GraceUI 0.2 cannot yet do for them | DECIDED | #engine #pages #security |
| 275 | 2026-11-21 | Keeping std::filesystem's imports out of eldennet.dll, the handle told the gate when made, and toml++'s output | FACT | #engine |
| 276 | 2026-11-21 | A key-shaped value in a test and the sweep: allowed by a pattern, since an allow line holding the literal is itself a finding | METHOD | #privsweep #engine |
| 277 | 2026-11-21 | The game's default PC layout leaves `T` free: the talk key's default (O94), to be checked in game by S4 | LIKELY | #input #hotkeys |
| 278 | 2026-11-21 | GraceUI's browser rail kept categories with no page, which the game's rail leaves out: compare both views on a real mod's pages | FACT | #graceui #pages |
| 279 | 2026-11-21 | The lamp's four tones measure 6.0 to 8.2:1 on their ground, its word 10.2:1; glint is a pale blue-grey | FACT | #graceui #lamp |
| 280 | 2026-11-21 | System One, the decision protocol SkyrimNet uses for its classifier, is published and offered by OpenRouter and Ollama | LIKELY | #llm #decisions |
| 281 | 2026-11-21 | Row actions and row secrets are declared on a table's block in the page data, not in a topic's body, so registration can check them | DECIDED | #graceui #pages |
| 282 | 2026-11-21 | Folding O96 and O97: the player's addressee is picked before the reply, and pairs make one speaker with several addressees a decision | LIKELY | #decisions #llm #npc |
| 283 | 2026-11-21 | en-llm took any host starting with "127." for this PC, letting a key go in clear text elsewhere; fixed | FACT | #engine #security |
| 284 | 2026-11-21 | One base-URL check for the form, the providers file and the key rows, each refusal in words | DECIDED | #engine #pages #security |
| 285 | 2026-11-21 | The AI pages' pickers: path-shaped model ids left out, a job's model row holds the model now, provider name rules | DECIDED | #engine #pages #privacy |
| 286 | 2026-11-21 | GraceUI's server pings each WebSocket with a protocol message every 15 s; a test browser must answer it | FACT | #graceui #harness |
| 287 | 2026-11-21 | Test literals and the sweep: private addresses built at run time, invented names clear of its endpoint words | METHOD | #privsweep #harness |
| 288 | 2026-11-21 | A row's changed mark is the space's own list, not a value compared with its default | DECIDED | #graceui #settings |
| 289 | 2026-11-21 | R1 run 9: the group's function hands our untouched task to a second singleton's method; nothing in memory keeps it | FACT | #gal #r1 #task |
| 290 | 2026-11-21 | R1 run 9: the launcher's log stopped at the call, so no 30 s text showed and the crash watcher was blocked | FACT | #launcher #r1 #console |
| 291 | 2026-11-21 | R1 spike.10: G's method read through its stub, the entries' words, G's walk, the array's views and the game's own call sites | DECIDED | #gal #r1 #task |
| 292 | 2026-11-21 | The launcher's console off its watch loop: the log first, a bounded queue, QuickEdit off | DECIDED | #launcher #r1 #console |
| 293 | 2026-11-21 | R1 run 10: G's method wraps our task in a 32-byte request and runs it at once or queues it; the task itself is never read | FACT | #gal #r1 #task |
| 294 | 2026-11-21 | A look review made from a real mod's pages finds what the demo space's goldens cannot | METHOD | #graceui #review |
| 295 | 2026-11-21 | R1 spike.11: the request G's method builds, its methods read in parts, G's fields and queue, and the game's own requests | DECIDED | #gal #r1 #task |
| 296 | 2026-11-21 | R1 run 11: the request runs at once and looks the group's word up in the manager's map; our task never reaches the code that uses it | FACT | #gal #r1 #task |
| 297 | 2026-11-21 | The AI pages' fourth slice: a job's provider, a key's other states, the changed mark, and the floor at GraceUI 0.2.9 | DECIDED | #engine #pages #graceui |
| 298 | 2026-11-21 | A schema's field order is held by a round-trip test; run every tree's full suite before pushing a schema change | METHOD | #graceui #pages |
| 299 | 2026-11-21 | R1 spike.12: the manager's map read at the title and in the world, the match's call, and the owner's world step | DECIDED | #gal #r1 #task #launcher |
| 300 | 2026-11-21 | The AI pages' fifth slice: one card per job, a job's fallback and status, and a page's subscriptions against GraceUI's rate limit | DECIDED | #engine #pages #graceui |
| 301 | 2026-11-21 | GraceUI's browser view paces every frame it sends, so a page with many topics loses none against the server's limit | FACT | #graceui #browser |
| 302 | 2026-11-21 | R1 run 12: group 110 is in the manager's map at the title, and the code it leads to removes a task: we have been calling the game's unregistration | FACT | #gal #r1 #task |
| 303 | 2026-11-21 | Spike.13 not built: a safety classifier stopped the work, and the route is put to the owner (O100) | FACT | #r1 #method |
| 304 | 2026-11-21 | Lodestar's shape: a table of numbers per build, each id re-found at start through its witnesses, no bytes in gal/ | METHOD | #gal #lodestar |
| 305 | 2026-11-21 | A function EldenNet might call carries a verdict of our own tool; unknown means no call | DECIDED | #gal #lodestar #task |
| 306 | 2026-11-21 | Lodestar L1: the registration's label is task_add, and a build's table lives in its layout file | DECIDED | #gal #lodestar |
| 307 | 2026-11-21 | A witness says the bytes at a place refer to the id, not that an instruction starts there | FACT | #gal #lodestar |
| 308 | 2026-11-21 | The accessor's shape is stricter than the pattern it replaces, and loses none of its accepted sites | METHOD | #gal #lodestar |
| 309 | 2026-11-21 | GraceUI assumes nothing per frame of a mod's data, so O101's reader fits ABI 1.2 as it is | FACT | #graceui #lodestar |
| 310 | 2026-11-21 | O102 carried out: gal/patterns.toml's idiom withdrawn, gal/recipes.toml's accessor in use | DECIDED | #gal #lodestar |
| 311 | 2026-11-21 | Digests move from L2 to L7, since under O101 (b) nothing in v1 would read one | DECIDED | #gal #lodestar |
| 312 | 2026-11-21 | Lodestar L3: offline, the finder reports the managers' slots unnamed, and counts the shape against the pattern | METHOD | #gal #lodestar #devkit |
| 313 | 2026-11-21 | A package's licence notices and the sweep: toml++'s address, kept as its licence asks, allowed in those files only | FACT | #process #privacy #spike |
| 314 | 2026-11-22 | The accessors' class objects are no objects with a table of methods; O92's way is the only one | FACT | #gal #lodestar #derive |
| 315 | 2026-11-22 | On 2.7.2.0 the accessor's shape stands at 7,077 places, all also the pattern's; the pattern accepts 4 more | FACT | #gal #lodestar |
| 316 | 2026-11-22 | One class object for each static, and the reverse, is an invariant of the accessors worth checking by itself | METHOD | #gal #lodestar |
| 317 | 2026-11-22 | v1's start makes no call into the game; the reader is a thread of EldenNet's own | DECIDED | #gal #lodestar #boot |
| 318 | 2026-11-22 | The table takes over id by id: an id it holds is never searched for | METHOD | #gal #lodestar |
| 319 | 2026-11-22 | Steady reading out of step with the frame: torn, held and unconfirmed, each counted | METHOD | #gal #lodestar |
| 320 | 2026-11-22 | On the fake game the own-thread reader reads the same truth as the registered task, the hub untouched | FACT | #gal #lodestar #fakegame |
| 321 | 2026-11-22 | With no getter and a dead name layout, the real game's managers can be named only by the table | FACT | #gal #lodestar |
| 322 | 2026-11-22 | The in-game round names a manager by its instance's class and writes only the catalogue's number and label | METHOD | #gal #lodestar |
| 323 | 2026-11-22 | The draft table is checked before it is written, by the same checks a start makes | METHOD | #gal #lodestar |
| 324 | 2026-11-22 | A class that holds a GuardedReader must be built where en-gal is built with and without the probe option | FACT | #build #gal |
| 325 | 2026-11-22 | On the fake game the read-only round names all eleven managers and reads them with no fault | FACT | #fakegame #lodestar |
| 326 | 2026-11-22 | On 2.7.2.0 in game, eight of the eleven managers are named by their instances' classes | FACT | #gal #lodestar #owner-run |
| 327 | 2026-11-22 | The reader on EldenNet's own thread read the real game for 19 minutes with no fault: the first evidence for O101 (b) | FACT | #gal #lodestar #owner-run |
| 328 | 2026-11-22 | The image as loaded gives the finder the same statics, class objects, sites and witnesses as the file | FACT | #gal #lodestar |
| 329 | 2026-11-22 | A third of the game's managers hold objects with no type information the walk reads | LIKELY | #gal #lodestar |
| 330 | 2026-11-22 | A round's report must tell "not there" from "there under another name" | METHOD | #gal #lodestar |
| 331 | 2026-11-22 | Ten of the eleven managers' leads have exactly one class of their name in the image; world_clock's has none | FACT | #gal #lodestar #owner-run |
| 332 | 2026-11-22 | A manager with a class of its name that the round did not name was not missed for want of type information | LIKELY | #gal #lodestar |
| 333 | 2026-11-22 | A search that follows pointers it has not validated asks before it reads | DECIDED | #gal #lodestar #safety |
| 334 | 2026-11-22 | The first invariant recipe of our own, the clock: minutes and hours by how they step | METHOD | #gal #lodestar |
| 335 | 2026-11-22 | A manager no accessor's static holds is looked for among all the data's slots, and stays a lead | METHOD | #gal #lodestar |
| 336 | 2026-11-22 | Two sessions of the game give the eight named managers the same statics, tables and witnesses | FACT | #gal #lodestar #owner-run |
| 337 | 2026-11-22 | A manager's slot is empty for seconds in the ordinary course of the game; v1 must treat it so | FACT | #gal #lodestar #owner-run |
| 338 | 2026-11-22 | No clock in whole minutes and hours among the unnamed managers' objects; one candidate for the play time | FACT | #gal #lodestar #owner-run |
| 339 | 2026-11-22 | Searches on the reader's thread must be budgeted by time, not by count | METHOD | #gal #lodestar |
| 340 | 2026-11-22 | The second round's logs: the slot search found the messages manager, and the report lost it | FACT | #lodestar #probe |
| 341 | 2026-11-22 | 97 in 100 of the data's values that could be a heap address are refused when Windows is asked | LIKELY | #lodestar #probe |
| 342 | 2026-11-22 | 56 of the accessor's 217 statics hold a manager only while a save is loaded | FACT | #lodestar |
| 343 | 2026-11-22 | The accessor's other shapes are recipes of the same kind under ids of their own | METHOD | #lodestar #gal |
| 344 | 2026-11-22 | Third read-only round: messages named by the search of the data's slots; twelve leads in the build's table | FACT | #lodestar #probe |
| 345 | 2026-11-22 | The second round's refusals were values that are no addresses | FACT | #lodestar #probe |
| 346 | 2026-11-22 | The accessor's other shapes confirm the 217 statics rather than add to them | FACT | #lodestar #gal |
| 347 | 2026-11-22 | event_flags is behind none of the places looked; world_clock has a class only under a longer name | FACT | #lodestar #probe |
| 348 | 2026-11-22 | The play time: one field behind one slot rose by the clock in two sessions, a lead for L6 | LIKELY | #lodestar #l6 |
| 349 | 2026-11-22 | Pad chords for mods' hotkeys are built: an anchor, a key held 0.4 s, one hotkey event, or a tap replayed | FACT | #graceui #input |
| 350 | 2026-11-22 | Pad behaviour is tested with no pad; what only a real pad shows waits for testers, since the owner has keyboard and mouse only | METHOD | #graceui #testing |
| 351 | 2026-11-22 | The read-only round may search all of the game's readable private memory once for a missing manager's object, counts and places only | DECIDED | #lodestar #probe |
| 352 | 2026-11-22 | Where the image has no class of a lead's name, a class whose name holds the lead's may name the manager, marked by_related | METHOD | #lodestar |
| 353 | 2026-11-22 | A manager no accessor loads takes its witnesses from the code's references to its slot, and of several slots the one referred to most | METHOD | #lodestar #gal |
| 354 | 2026-11-22 | The event flags' store is behind one of the accessor's statics from the title on; no object of the flags' manager's class in 7.7 GiB | FACT | #lodestar #probe |
| 355 | 2026-11-22 | The world's clock is one object inside the object behind one of the accessor's statics: a field of that manager, no static of its own | FACT | #lodestar #l6 |
| 356 | 2026-11-22 | messages' slot is the one of its two that the code refers to 238 times; the build's table has it with eight witnesses | FACT | #lodestar #gal |
| 357 | 2026-11-22 | The object scan read 19,750 MiB in 781 s without finishing and slowed the reader's thread; it stays off | FACT | #lodestar #probe |
| 358 | 2026-11-22 | A round's report keeps what was seen in the world, not the last pass's state, which is made as the game closes | METHOD | #lodestar |
| 359 | 2026-11-22 | event_flags (1008) is the object the event flags are read through, the store's class its lead; world_clock (1012) waits for L6 | DECIDED | #lodestar #gal |
| 360 | 2026-11-22 | A holder's end could be lost when its box was full; since GraceUI 0.2.15 it always comes | FACT | #graceui #input |
| 361 | 2026-11-22 | GraceUI's key names follow the browser's KeyboardEvent.code, and pad buttons the Xbox pad's own names | DECIDED | #graceui #hotkeys |
| 362 | 2026-11-22 | Every mod's hotkeys are rebound on GraceUI's Hotkeys page; EldenNet's three key settings become registered defaults | DECIDED | #graceui #hotkeys #pages |
| 363 | 2026-11-22 | R1 is retired: what goes, what stays, and who removes it | DECIDED | #engine #lodestar #probe |
| 364 | 2026-11-22 | The first conversation, offline: its plan in four parts, and Engine's rulings on the survey's open points | DECIDED | #engine #conversation |
| 365 | 2026-11-22 | L6's first tool, the watch: a way to an object, its first 512 bytes once a second, and counts of how each word behaves | METHOD | #lodestar #l6 |
| 366 | 2026-11-22 | Rebinding is built: one home for every binding, kept by mod and action ids, applied at each registration | DECIDED | #graceui #hotkeys |
| 367 | 2026-11-22 | A mod may register before GraceUI reads its settings file, so kept bindings also go over registered hotkeys; that path is reviewed, not yet seen run | FACT | #graceui #hotkeys |
| 368 | 2026-11-22 | The fifth read-only round: event_flags named through the store, ten of eleven managers placed, 30 minutes with no fault | FACT | #lodestar #probe |
| 369 | 2026-11-22 | The play time is the low half of the word 160 bytes into the object behind one slot of the data, in four sessions | FACT | #lodestar #l6 |
| 370 | 2026-11-22 | An ask-first read costs about half a millisecond on the owner's PC, a hundred times a plain guarded copy | FACT | #lodestar #probe |
| 371 | 2026-11-22 | A long round's report can pass the writer's 256 KiB and is then refused whole, so a round bounds what it writes | METHOD | #lodestar |
| 372 | 2026-11-22 | R1's leftovers: remove what nothing uses, keep retired words in append-only vocabularies, keep what L6 and the task path use | DECIDED | #engine #lodestar #probe |
| 373 | 2026-11-22 | A wait handle for each mod's events: one auto-reset event per mod, set whenever something is kept for its poll | FACT | #graceui #abi |
| 374 | 2026-11-22 | A static with no class to name it by is found by what its object holds: save_data by the play time | METHOD | #lodestar #l6 |
| 375 | 2026-11-22 | What the profile editor needs that GraceUI lacked: a text field in game, master-detail, a topic per row, a longer deadline | FACT | #graceui #pages |
| 376 | 2026-11-22 | The profile editor is the one content editor in v1, by O43; its rules are EldenNet's since G20 | DECIDED | #graceui #editor |
| 377 | 2026-11-22 | EldenNet's answers to GraceUI's proposal 0010: the eight kinds suffice, with keyed and locked list items, unset fields and file-level messages | DECIDED | #engine #editor #pages |
| 378 | 2026-11-22 | GraceUI's own text field in game: one field model for every kind, every character handed over in order, a secret never laid out | DECIDED | #graceui #input |
| 379 | 2026-11-22 | Posted key messages do not order as a keyboard's: the character comes after the key-up, so the harness types through SendInput | FACT | #graceui #harness |
| 380 | 2026-11-22 | A search hit or a shortcut row on a page with tabs now opens the row's own tab, section and disclosure | FACT | #graceui #pages |
| 381 | 2026-11-22 | The sixth read-only round: save_data in the table by the invariant recipe, 14 leads, the reader at 9.6 a second | FACT | #lodestar #gal |
| 382 | 2026-11-22 | No clock packed into bits in any static's object, nor in 16 KiB of the clock class's object; one slowly rising fraction is the only candidate | FACT | #lodestar #l6 |
| 383 | 2026-11-22 | Master-detail is a page's, not a block: the page's own tabs and sections are the chosen row's detail | DECIDED | #graceui #pages |
| 384 | 2026-11-22 | A family's members in the server: 64 slots a space, taken at the first watch, each state carrying its slot's generation | METHOD | #graceui #serve |
| 385 | 2026-11-22 | A test for a clock must not depend on its pace against the reading's: hours alone, and a steady rise with a jump | METHOD | #lodestar #l6 |
| 386 | 2026-11-22 | A slow command is named in the space's page data, not in its answer, so both views know its deadline before it is sent | DECIDED | #graceui #protocol |
| 387 | 2026-11-22 | EldenNet keys a profile's family member by the same slot as its list row, with a dot for the colon | DECIDED | #engine #editor #pages |
| 388 | 2026-11-22 | The prompt's blocks 2 and 11: block 2 stays at 120 tokens, checked whole by the lint; block 11 rises to 70 with one tic note | DECIDED | #engine #conversation |
| 389 | 2026-11-22 | A flag is told by a single bit that flips once in memory that otherwise keeps still | METHOD | #lodestar #l6 |
| 390 | 2026-11-22 | A round's report may carry a clock candidate's reading before and after a jump, and nothing else the memory held | DECIDED | #lodestar #probe |
| 391 | 2026-11-22 | An editor's unsaved edits are kept by the document's topic, not by the page, so leaving a page loses nothing | DECIDED | #graceui #pages |
| 392 | 2026-11-22 | Prose in game moves by line from the last layout's boundaries, so the view needs no font outside the layout | METHOD | #graceui #view |
| 393 | 2026-11-22 | A save names the row the list chooses next, since a list alone cannot tell a renamed row from a removed one | DECIDED | #graceui #pages |
| 394 | 2026-11-22 | The time of day: the hour and minute packed in one word behind an accessor's static, told by the owner's two passings of time | FACT | #lodestar #l6 |
| 395 | 2026-11-22 | The flags' blocks are likely the plain data behind the store's address 40 bytes in, and their tree behind 56 | LIKELY | #lodestar #l6 |
| 396 | 2026-11-22 | A list of what changed must be told when to look: the tester marks a task done | METHOD | #lodestar |
| 397 | 2026-11-22 | An invariant asks for what the thing does, not for a margin over the next candidate | METHOD | #lodestar |
| 398 | 2026-11-22 | The owner asks for a tester's panel: each task of a round in order, with a timer and a mark when done | FACT | #lodestar #owner |
| 399 | 2026-11-22 | The UI model is state published whole; the UI events are five, versioned at the top only | DECIDED | #engine #conversation |
| 400 | 2026-11-22 | The draft is the core's, and every Close hands it back | DECIDED | #engine #conversation |
| 401 | 2026-11-22 | Toasts are a list with ids; their policy stays in the core | DECIDED | #engine #conversation #graceui |
| 402 | 2026-11-22 | Speech is three factories of per-reply objects; only the lint's tally passes between replies | DECIDED | #engine #conversation |
| 403 | 2026-11-22 | The prompt takes the persona's record as JSON, not a struct both parts would edit | DECIDED | #engine #conversation |
| 404 | 2026-11-22 | Stop sequences go to every flavour; a model that refuses them gets the stripped retry | DECIDED | #engine #conversation #llm |
| 405 | 2026-11-22 | The core's new inputs were ticks until the conversation was wired, proved by twin cores | METHOD | #engine #conversation |
| 406 | 2026-11-22 | en-core's build takes folders and links en-prompt, so the parallel parts never edit it | DECIDED | #engine #conversation #build |
| 407 | 2026-11-22 | A nested variant's alternative may not have a default member initializer | FACT | #engine #conversation #build |
| 408 | 2026-11-22 | en-proto's integer reader is shared, never copied | METHOD | #engine #conversation #build |
| 409 | 2026-11-22 | The sanitizer gives a sentence out once it is final, reading the stream again from its start | DECIDED | #engine #conversation |
| 410 | 2026-11-22 | The sanitizer's general "Name:" pattern also strips real openings | FACT | #engine #conversation |
| 411 | 2026-11-22 | The sanitizer's spans, NFC within the whitelist's letters, and apostrophes | DECIDED | #engine #conversation |
| 412 | 2026-11-22 | The safety list's fingerprint and normalisation (O107) | DECIDED | #engine #conversation #safety |
| 413 | 2026-11-22 | banned.toml leaves out words with a sense of their own in the land | DECIDED | #engine #conversation |
| 414 | 2026-11-22 | The renderer builds its template data from the record, every name always present | DECIDED | #engine #conversation |
| 415 | 2026-11-22 | The pager: Over fills the reply too, and a short sentence waits for its partner | DECIDED | #engine #conversation |
| 416 | 2026-11-22 | Part A's additions to the shared interfaces, each with a default that changes nothing | DECIDED | #engine #conversation |
| 417 | 2026-11-22 | MSVC's std::map may throw on a move, so it is no Expected's value | FACT | #engine #conversation #build |
| 418 | 2026-11-22 | A test persona stays test_only; only the tests' realm, which a release DLL refuses, lets it resolve | DECIDED | #engine #conversation |
| 419 | 2026-11-22 | Only a threat ends a conversation after 10 s of danger; a menu or a hidden overlay holds the pages | DECIDED | #engine #conversation |
| 420 | 2026-11-22 | The conversation's targets, the observer and the talk key, where the design is silent | DECIDED | #engine #conversation |
| 421 | 2026-11-22 | The conversation's failures, holds and closes, where the design is silent | DECIDED | #engine #conversation |
| 422 | 2026-11-22 | The conversation's request ids, and the ladder's L3 and L4 | DECIDED | #engine #conversation |
| 423 | 2026-11-22 | The record part A renders, the pack's other files, and a load during a conversation | DECIDED | #engine #conversation |
| 424 | 2026-11-22 | The fixture flag states, written cumulatively, and the truth tables | DECIDED | #engine #conversation #knowledge |
| 425 | 2026-11-22 | mockllm is a scripted model on the caller's clock; its SSE server is not built | DECIDED | #engine #conversation #eval |
| 426 | 2026-11-22 | CTest's CONFIGURATIONS keeps a test out of the default run | FACT | #engine #conversation #build #eval |
| 427 | 2026-11-22 | The core's lint and prompt stand for whichever content is loaded | DECIDED | #engine #conversation |
| 428 | 2026-11-22 | Block 2's content lint: who is held to the cap | DECIDED | #engine #conversation |
| 429 | 2026-11-22 | Block 11's one tic note is the newest by the tally, and its worst case passes 70 | DECIDED | #engine #conversation |
| 430 | 2026-11-22 | The resolved record is a whitelist of the fields the renderer reads | DECIDED | #engine #conversation #spoilers |
| 431 | 2026-11-22 | The real speech changed nothing in part B's scenarios | FACT | #engine #conversation #eval |
| 432 | 2026-11-22 | A one-page reply sealed while it streams is never last, so a queued line waits for its hold | LIKELY | #engine #conversation |
| 433 | 2026-11-22 | The DLL links en-prompt for real: the folder code split off, one runtime import reviewed | FACT | #engine #conversation #build #dll |
| 434 | 2026-11-22 | The surfaces draw GraceUI's glyphs as quads of their own; Dear ImGui keeps only the box's line | DECIDED | #engine #conversation #graceui |
| 435 | 2026-11-22 | EldenNet's faces are GraceUI's own font files, registered as two scaled faces | DECIDED | #engine #conversation #graceui #fonts |
| 436 | 2026-11-22 | Panels are made for the display, lazily, and each is submitted at most every 100 ms | DECIDED | #engine #conversation #graceui |
| 437 | 2026-11-22 | The box's closes, as the surfaces answer GraceUI's holder ends | DECIDED | #engine #conversation #input |
| 438 | 2026-11-22 | Tab is masked from the game whenever it is bound | LIKELY | #engine #conversation #hotkeys |
| 439 | 2026-11-22 | T6 waits on GraceUI's handle, and the talk key's latency is measured from its SendInput | METHOD | #engine #conversation #graceui |
| 440 | 2026-11-22 | No chat text in the DLL's log | METHOD | #engine #conversation #privacy |
| 441 | 2026-11-22 | From the talk key to the box's first frame: 1 ms on the fake game | FACT | #engine #conversation #fakegame |
| 442 | 2026-11-22 | What part C left to the core: the refusal's toast and the pages' speaking level | LIKELY | #engine #conversation |
| 443 | 2026-11-22 | The content's problems reach the player as one notice, never a value | DECIDED | #engine #conversation #notices |
| 444 | 2026-11-22 | The tester's panel is a window of the launcher's own, not GraceUI's overlay, since a read-only round hooks nothing | DECIDED | #lodestar #launcher |
| 445 | 2026-11-22 | The tester's confirmations are Marks in the watch block's ring, with no new field | METHOD | #lodestar |
| 446 | 2026-11-23 | The first conversation verified whole on four toolchains, the talk case with the real speech under Wine | FACT | #engine #conversation |
| 447 | 2026-11-23 | world_clock is in the build's table by the invariant recipe, an eighth session | FACT | #lodestar #l6 |
| 448 | 2026-11-23 | Bits are set in the flags' stretch where the owner does something the game remembers | FACT | #lodestar #l6 |
| 449 | 2026-11-23 | The report may carry the numbers of a flags' tree record joined to a bit the owner set | DECIDED | #lodestar #probe |
| 450 | 2026-11-23 | The tester's panel ran on the owner's PC over the game, with Pause as its key | FACT | #launcher |
| 451 | 2026-11-23 | The flags' layout: the store's holder and tree, a record's key, mode and block index, a flag as group times 1,000 plus its place | FACT | #lodestar #l6 #flags |
| 452 | 2026-11-23 | Our reader's lead was wrong in one thing: a record's value is a block's index, not a byte offset | FACT | #lodestar #flags |
| 453 | 2026-11-23 | A join must follow the layout the counts show: the index, not the start | METHOD | #lodestar |
| 454 | 2026-11-23 | en-gal's flag reader takes a mode-1 record's value as the block's index | DECIDED | #lodestar #flags |
| 455 | 2026-11-23 | The flags' layout holds in a second session, and the reader's way down the tree arrives at every joined flip | FACT | #lodestar #l6 #flags |
| 456 | 2026-11-23 | Step L6's three subjects are placed after ten read-only rounds, with no fault in some 80,000 readings | FACT | #lodestar #l6 |
| 457 | 2026-11-23 | EldenNet's own flag reader runs first inside a read-only round, beside the round's tree reader, before the start uses it | DECIDED | #engine #lodestar #flags |
| 458 | 2026-11-23 | One group of the reader can be read alone before the others' fields are placed: a partial discovery and a tick for that group | METHOD | #lodestar #flags |
| 459 | 2026-11-23 | What a ready start still needs from the layout: one static and 36 fields, by L6's ways | FACT | #lodestar #gal |
| 460 | 2026-11-23 | The order of the start's layout, and how far a start may rest on the notes' leads for constants | DECIDED | #engine #lodestar #gal |
| 461 | 2026-11-23 | No second Esc after the chat box closes: Esc and the pad act on the box only while it is open | DECIDED | #engine #conversation #graceui #input |
| 462 | 2026-11-23 | EldenNet's own discovery and flags reader work on the game, through the build's layout | FACT | #lodestar #gal #flags |
| 463 | 2026-11-23 | game_state is sought by what it holds at the title and in the world, over four looks | METHOD | #lodestar #l6 #gal |
| 464 | 2026-11-23 | A search that must end inside a session gets the searches' whole share until it is through | METHOD | #lodestar |
| 465 | 2026-11-23 | The menu's frame-cost check in GraceUI's harness is noisy on a four-core container | FACT | #graceui #harness |
| 466 | 2026-11-23 | The layout has a field type for a run of bits in a word; the clock's two fields and the play time enter it as leads | DECIDED | #lodestar #gal |
| 467 | 2026-11-23 | Where a stalled GraceUI init stops: a step set before each part, readable from outside | METHOD | #graceui #spike |
| 468 | 2026-11-23 | EldenNet's own flags reader agrees with the game on flags the owner changed | FACT | #lodestar #gal #flags |
| 469 | 2026-11-23 | game_state: one candidate after the twelfth round, by the owner's word for the slot | LIKELY | #lodestar #l6 #gal |
| 470 | 2026-11-23 | A search by what an object holds in each state takes its candidates in each state and lists its near misses | METHOD | #lodestar |
| 471 | 2026-11-23 | The pilot Kalé and known(P) built offline; the core watches the catalogue's verified aliases only | DECIDED | #engine #knowledge #conversation |
| 472 | 2026-11-23 | The pilot's draft voice was past block 4's cap; the load now holds block 4 to it | FACT | #engine #content |
| 473 | 2026-11-23 | Block 5b's lines are joined by a space | DECIDED | #engine #prompt |
| 474 | 2026-11-23 | runes >= n means two things for now: great runes in the knowledge, runes held in v0.1's predicates | LIKELY | #engine #predicates |
| 475 | 2026-11-23 | Kalé's index row keeps the design's NpcParam lead and leaves its entity lead out | DECIDED | #engine #identity |
| 476 | 2026-11-23 | The "you, now" line and the behaviour switches, as far as v0.2 goes | DECIDED | #engine #conversation |
| 477 | 2026-11-23 | The pilot's smaller choices: the catalogue's sources, clears left out, fixtures by pack | DECIDED | #engine #content |
| 478 | 2026-11-23 | windows.h defines near and far as nothing: en-core's public names avoid both | FACT | #engine #build |
| 479 | 2026-11-23 | An S0 run's facts reach the launcher and spike-report through a report in the run's folder, not the watch block | DECIDED | #engine #spikes #launcher |
| 480 | 2026-11-23 | A discovery refused only for a required static with no way to be found is a layout not complete yet | DECIDED | #engine #boot #gal |
| 481 | 2026-11-23 | S0-A's "untouched image": the table's checks made again once the start has stopped | METHOD | #engine #spikes #lodestar |
| 482 | 2026-11-23 | Under Wine ReadProcessMemory on our own process copies a reserved no-access page | FACT | #gal #wine #spikes |
| 483 | 2026-11-23 | A group never on shows Tripped in every snapshot; S0-B's no_trip counts the session's own trips | FACT | #gal #spikes #tools |
| 484 | 2026-11-23 | A start stopped at Discover has no crash filter of ours; S0-D's crashes there rest on the watcher alone | FACT | #engine #spikes |
| 485 | 2026-11-23 | The panel's confirmations go on after the Mark's hand-over | METHOD | #launcher #spikes #graceui |
| 486 | 2026-11-23 | The probe DLL's p99 on the fake game under Wine sits near S0-B's 0.5 ms | FACT | #gal #spikes #wine |
| 487 | 2026-11-23 | The S0 package's README is README-SPIKES; the s0e-s4 kind's template moves aside | DECIDED | #spikes #tools |
| 488 | 2026-11-23 | A field found by what it holds in the game's states is told from its look-alikes by a second save | METHOD | #lodestar #l6 |
| 489 | 2026-11-23 | The pointers and lists are sought by class from the characters manager's own object | METHOD | #lodestar #gal |
| 490 | 2026-11-23 | G9's pairing is built as decided, with three choices of GraceUI's own | FACT | #graceui #security |
| 491 | 2026-11-23 | Limits shared between listeners need a share per listener; a limit shared between peers needs a cap per peer | METHOD | #graceui #security |
| 492 | 2026-11-23 | A modal driven by the host must be open before the host's first look at it | FACT | #graceui |
| 493 | 2026-11-23 | The second Esc removed; an Esc that reaches the core with the box closed changes nothing | DECIDED | #engine #conversation #input |
| 494 | 2026-11-23 | A queued line goes when the stream ends with the last page up (WHY§432 closed) | DECIDED | #engine #conversation |
| 495 | 2026-11-23 | The refusal's toast names the holder by GraceUI's id; not-now by its reason | LIKELY | #engine #conversation #graceui |
| 496 | 2026-11-23 | L3 and L4 on the pages | LIKELY | #engine #pages |
| 497 | 2026-11-23 | The behaviour settings reach the core (WHY§476 closed) | DECIDED | #engine #settings |
| 498 | 2026-11-23 | One meaning of `runes >= n`: the great runes (WHY§474 closed) | DECIDED | #engine #predicates |
| 499 | 2026-11-23 | disable.toml (S11.3.g) | DECIDED | #engine #content |
| 500 | 2026-11-23 | A failed configure-time download leaves an empty file that the next configure keeps | FACT | #engine #build |
| 501 | 2026-11-23 | The polish's tests on the four toolchains; Lodestar's read-clock case passes | FACT | #engine #tests |
| 502 | 2026-11-23 | The characters manager's places, by search and class, the same with two characters | LIKELY | #lodestar #gal |
| 503 | 2026-11-23 | In the player's object: the parts holder at 0x190 and the character's sheet at 0x580 | LIKELY | #lodestar #gal |
| 504 | 2026-11-23 | game_state's candidate is a number of the character: 4 for the list's first, 2 for the third | FACT | #lodestar #l6 |
| 505 | 2026-11-23 | A search of a manager's object is asked for only once the game is in the state it is to see | METHOD | #lodestar |
| 506 | 2026-11-23 | Fields past a class's 0x4000 copy are far fields, each read by its own small copy | DECIDED | #engine #gal #lodestar |
| 507 | 2026-11-23 | game_state's candidate is not the save's slot: the owner's save file gives the slots | FACT | #lodestar #l6 |
| 508 | 2026-11-23 | Ground truth first, then the search | METHOD | #lodestar |
| 509 | 2026-11-23 | S0-E's run A1 crashed on the owner's PC at GraceUI's first Present, before a frame was drawn | FACT | #spike #graceui #engine |
| 510 | 2026-11-23 | A start that stops at a banner once the window has shown installs the crash filter | DECIDED | #engine #boot |
| 511 | 2026-11-23 | S0-E's next package: A5 with GraceUI and its observer alone, state lines every 200 ms, other overlays closed | DECIDED | #spike #graceui #engine |
| 512 | 2026-11-23 | G28's upload route: the ticket in a header, spent on its first match; the size before a byte, the hash on a worker | FACT | #graceui #security |
| 513 | 2026-11-23 | A browser hashes only in a secure context, so uploads stay on this PC | FACT | #graceui |
| 514 | 2026-11-23 | A count cap needs a byte cap when items vary that much, and every path to a command carries the same rule | METHOD | #graceui #security |
| 515 | 2026-11-23 | A spike run's own record lists the game's modules by name | DECIDED | #launcher #spike |
| 516 | 2026-11-23 | A step set as the code passes, read by an observer that writes at each change, places a fall without a debugger | METHOD | #graceui #spike |
| 517 | 2026-11-23 | Run A5 reached the title without the stand-in; the tester's panel moves below GraceUI's notices | FACT | #spike #graceui #launcher |
| 518 | 2026-11-23 | The stand-in for another overlay caused the owner's crashes; no run loads it any more | DECIDED | #spike #graceui |
| 519 | 2026-11-23 | A game that turns its camera by the mouse keeps the cursor at its centre; an overlay's pointer follows the mouse's own motion | FACT | #graceui #input |
| 520 | 2026-11-23 | The profile editor's commands are answered later, through GraceUI's reply handle | FACT | #engine #editor #pages |
| 521 | 2026-11-23 | A layer over a shipped persona keeps its persona's key; `mine.<id>` waits for the player's own characters | DECIDED | #engine #editor |
| 522 | 2026-11-23 | Block 4 passes its cap with any note over the pilot: a warning on every save | FACT | #engine #content |
| 523 | 2026-11-23 | Sensitive marks: EldenNet's own command names, not the brief's | FACT | #engine #graceui #security |
| 524 | 2026-11-23 | grace-dev's dump draws a master page's detail empty | FACT | #graceui #pages |
| 525 | 2026-11-23 | A paired device in the fake game: the listener at home on 127.0.0.2 | METHOD | #engine #tests #graceui |
| 526 | 2026-11-23 | The browser view's port, a case's own: another run of GraceUI's cases holds 8642 | FACT | #engine #tests |
| 527 | 2026-11-23 | Lodestar's read-state case is timing-tight; read-chars fails on the base too | FACT | #tests #lodestar |
| 528 | 2026-11-23 | A test package's runs check the game's GraceUI against the package's own before the start | DECIDED | #engine #spike #launcher |
| 529 | 2026-11-23 | spikes.4: A5 and B ran to their end; A1 still falls at GraceUI's first Present with EldenNet connected and no stand-in | FACT | #spike #graceui #engine |
| 530 | 2026-11-23 | A start stopped by its reader's files says "not supported yet" of a build Home calls supported | FACT | #engine #pages #words |
| 531 | 2026-11-23 | The moving numbers are sought by invariant on the player's parts, counted by what the tester is doing | METHOD | #lodestar #gal |
| 532 | 2026-11-23 | A waiting task of the tester's panel may carry a mark: the end of its time is stamped as one | METHOD | #lodestar #launcher |
| 533 | 2026-11-23 | A holder that returns after an Alt+Tab waits for GraceUI's next frames before it asks for input again | FACT | #graceui #input |
| 534 | 2026-11-23 | Far fields are built as the ruling says: a copy of their own, into the object's buffer past the class's copy | METHOD | #lodestar #gal |
| 535 | 2026-11-23 | A test's port belongs in the test's environment, never the player's settings | METHOD | #graceui #tests |
| 536 | 2026-11-23 | A read round ran with GraceUI loaded: EldenNet took the Marks, and the owner's Pause presses became the round's marks | FACT | #lodestar #graceui |
| 537 | 2026-11-23 | A read round is free of GraceUI by construction, and the panel's confirmations reach the round whoever holds the Pause key | METHOD | #lodestar #engine |
| 538 | 2026-11-23 | Objects of the player's class are several in a world: the main player is the object at characters.main_player | FACT | #lodestar #gal |
| 539 | 2026-11-23 | The quit's crash codes look like a damaged heap found at the quit, not damage done there | LIKELY | #graceui #crash |
| 540 | 2026-11-23 | ABI 1.6 grows GraceUiEvent: EldenNet's poll is asked at the larger size, and nothing else changes | FACT | #engine #graceui |
| 541 | 2026-11-23 | A free port for every GraceUI case, from one environment of its own | METHOD | #engine #tests #graceui |
| 542 | 2026-11-23 | grace-dev 0.2.31's dump: only the two Named NPCs goldens change | FACT | #engine #pages #graceui |

## Entries

### 1. Why this log exists

`2026-11-14` · METHOD · `#process` · refs: CLAUDE, handover-2026-11-07

- Asked whether a change of model loses anything: the repositories, the container and the conversation carry over,
  but the reasoning behind each answer does not. The owner asked for that reasoning in brief, with tags, labels and
  references, so that it stays in view.
- So: this file, read with the handover at every start; CLAUDE.md says when to write an entry.

### 2. Smithbox 2.2.6 shows one Item container per language, `item_dlc02`

`2026-11-08` · FACT (the owner's screen: the container's source file `msg/engus/item_dlc02.msgbnd.dcx`) · `#smithbox #text`
· refs: S6.3.m, DataPiler S42, WHY§9

- Its File List groups the texts as Base, DLC 1 and DLC 2: one export holds all three layers (78 wrappers, 21,532
  entries in English).
- Not: separate `item` and `item_dlc01` containers, which DataPiler 0.2.0's steps asked for. 0.2.1 ends the names rows'
  wait on one export.

### 3. A text export's `Name` is its file name; the Export Output box shows the last export

`2026-11-08` · FACT (Smithbox `d144cc7`, tag 2.2.6: `Tools/TextDataTransferTool.cs` lines 300–325, `Tools/FmgExporter.cs`
lines 117–153 and 377) · `#smithbox #text` · refs: WHY§2

- With Export Filename empty, the file and the JSON's `Name` take the container's file name (`item_dlc02`); a typed
  name replaces both. DataPiler recognises a container by that `Name` (with `SourceContainer` first, which 2.2.6 does
  not write).
- The Export Output box is filled only when an export is written, so it shows the last export, not the selection. An
  earlier answer read it as a preview and was wrong; the owner's stray `item` import came from a typed name.

### 4. Smithbox forgets the Text Editor's Export Directory on restart

`2026-11-08` · FACT (`TextDataTransferTool.cs` line 19: a plain field, not a setting) · `#smithbox`

- Unset, exports go to `<project>\.smithbox\Workflow\Exported Text\` (`Utils/TextUtils.cs`, `GetStoredTextDirectory`).
- Next: DataPiler could watch that folder too (offered for a later version).

### 5. The game's unused text entries read `[ERROR]`

`2026-11-08` · FACT (the owner's export) · `#text #datapiler` · refs: DataPiler S40

- As a text, `[ERROR]` would count as a name and make the guard's check 3 refuse any shared string holding "error".
  0.2.1 skips it on import and cleans 0.2.0's imports.
- en-0001's local file held no `[ERROR]` name (the owner's count, 0), so its counts stood.

### 6. Why the map placements failed: probably no Map Editor in the owner's project

`2026-11-08` · LIKELY · `#smithbox #maps` · refs: WHY§16

- Seen: `SearchObjects` answered UNKNOWN; the owner's Smithbox shows only the Param and Text Editors.
- Smithbox's `SearchObjects` reads `MapData.PrimaryBank.Maps` (`Utilities/SoapstoneService.cs` lines 430–620 at
  `d144cc7`); with no map editor that is probably null, and an unhandled exception reaches gRPC as UNKNOWN.
- Next: enable the Map Editor in the project before any map work; DataPiler should say so in its error.
- **Later (2026-11-14):** wrong cause. The UNKNOWN came from DataPiler's request, which lacked its search clause
  (WHY§39); the Visual Map Editor is still needed for the map data (WHY§37).

### 7. Row reads are one call per row; the batch path reads the wrong bank

`2026-11-08` · FACT (`GetParamRowsRequest` has one `row_id`; `SoapstoneService.cs` lines 430–560) · `#smithbox #datapiler`

- 7,045 NpcParam rows took about 62 s. `SearchObjects` on params returns many rows at once, but from the project's
  `PrimaryBank`, not the vanilla bank DataPiler promises, so it is not used. Concurrent calls are the remaining
  speed-up (not built yet).

### 8. What this session can and cannot do on GitHub

`2026-11-08` · FACT (observed) · `#release #process`

- The session's git proxy refuses tag pushes and branch deletions (403 by policy); GraphQL is unavailable, so pull
  requests go through REST (`gh api repos/.../pulls`).
- So releases are drafts made by the workflow (`workflow_dispatch` on the default branch, `--target` the commit);
  GitHub makes the tag when the owner publishes. Branch clean-ups are the owner's.
- **Later (2026-11-14):** also seen that week: `gh repo create` is refused (the session is bound to its own
  repositories), so the owner creates a repository and it is attached afterwards; `workflow_dispatch` runs only once the
  workflow is on the default branch; the release workflow's gate job skips a tag-push run whose release is already
  published.

### 9. The names' text ids: 18, 328 and 428

`2026-11-08` · FACT (the owner's data) · `#roster #text` · refs: O59, WHY§14, DataPiler S22

- 95.5% of NpcParam's distinct name numbers resolve in wrappers 18 (base game), 328 (DLC 1) and 428 (DLC 2) of
  `item_dlc02`. 18 and 328 are confirmed by names found in them (en-0005); 428 holds none that first appears there.
- **Later (2026-11-14):** 95.5% (floored) of the distinct name numbers of 0 or above means 214 of 224: about 10 do not
  resolve, on 20 rows, plus 1 row with no name number (WHY§15).

### 10. "Named" is not "a named character"

`2026-11-08` · FACT · `#roster` · refs: WHY§11

- en-0001 counted 7,024 of 7,045 rows as named, which looked like nearly every row. en-0002 showed only 214
  distinct names among them, two of them on more than 20 rows each.

### 11. Name number 0 is the ordinary enemies' default

`2026-11-08` · FACT (en-0003) · `#roster` · refs: O59, S6.3.o

- Name 0 is on 6,325 rows across 35 teams; every other name is on at most 22 rows. The game shows no name for
  ordinary enemies, so name 0 is their default, and the roster is the 213 other names on 699 rows (en-0004).

### 12. Why the roster took four jobs (DP8)

`2026-11-08` · METHOD · `#roster #datapiler` · refs: DataPiler DP8, O59

- A share file may hold ids only for rows a job picked: named rows, or an explicit list of row ids; and the guard
  refuses tables covering more than half of a game table. All named rows (7,024) were too many, and a pick by name
  number is not allowed, only by row id.
- So: counts first (en-0002, en-0003) to find the cut, then a filter `named and nameId ≠ 0` (en-0004, 10% of the
  table). The job files are in `docs/research/eldenring/jobs/`.

### 13. NPC row ids: the middle digits match the name number

`2026-11-08` · LIKELY (en-0004) · `#roster` · refs: WHY§11

- Rows `52NNN0000`, `53NNN0000` and `54NNN0000` carry name `1NNN00` (for example 52348…, 53348… and 54348… carry
  134800). The 52 rows are mostly teams 26 and 27, the 53 rows team 2, the 54 rows team 27: one character's friendly,
  hostile and summoned forms, probably.
- Next: check in game (O48) before EldenNet's identity data relies on it.

### 14. The expansion's characters: 50 names first in DLC 1

`2026-11-08` · FACT (en-0005, DataPiler 0.2.2's `layer`) · `#roster #text` · refs: S6.3.o, WHY§9

- 163 names first appear in the base game's text, 50 in DLC 1 (49 of them 140200–143200, and 163000), none in DLC 2.
  Written beside each name in `docs/research/eldenring/leads/roster-rows.json`.
- Use: a player without the expansion never meets those 50; the spoiler rules can treat them as a layer.

### 15. The 21 unresolved rows are left out

`2026-11-08` · LIKELY · `#roster` · refs: S6.3.o

- 21 rows (about 10 name numbers, all in NPC teams) have no text in any layer. The game shows a character's name from
  that text, so these have none to show: probably cut or unused characters. The owner agreed to leave them out.
- Next: revisit if EldenNet meets one in game, or with the map work (placed or not).

### 16. Some bosses may be missing from NpcParam's names

`2026-11-08` · LIKELY · `#roster #lore` · refs: WHY§6, O43

- A boss's bar name is set by the event scripts when the fight starts, not necessarily from its NpcParam row, so some
  bosses may carry name 0 and fall outside the 213. DataPiler cannot read event scripts.
- Next: research a source (a Smithbox export of the event scripts, map placements, or EldenNet reading the bar's
  name id in game).

### 17. Which kinds speak: evidence, not guesswork

`2026-11-09` · DECIDED (O61) · `#lore #voice`

- A kind speaks only if the game shows one of its people speaking (Boc for demi-humans, Alexander for living jars,
  Gurranq for beastmen, Rykard for man-serpents). Inventing speech for the rest would be inventing lore.
- Corrected on the way: an answer listed Placidusax and the Fire Giant among bosses to be voiced; by O61 they stay
  mute (O61's note: named bosses follow their kind).

### 18. The game has one spoken tongue and no dragon language

`2026-11-09` · LIKELY (from knowledge of the game; the expansion's detail less certain) · `#lore` · refs: O61

- Every speaker uses one common tongue; no dragon speaks, and dragon power comes by Dragon Communion, not words. The
  nearest other "language" is the Two Fingers' trembling, read by Finger Readers.

### 19. Why voiceless speakers get generated voices, not borrowed ones

`2026-11-09` · DECIDED (O62 and its note) · `#voice`

- SkyrimNet can clone voices for everyone because Skyrim shares generic voice types among many NPCs. Elden Ring gives
  most voices to one character each, so a soldier in a named character's voice would sound wrong. Speakers the game
  never voices get a voice designed to fit; SkyrimNet's own creature voices may serve where they fit, once Min
  confirms.

### 20. Why Melina was mute, and why that changed

`2026-11-09` · DECIDED (O63) · `#lore #voice` · refs: O19, ER04

- O19's reasons were technical and about spoilers, not lore: she is reached through the grace menu and cutscenes,
  not as a world NPC, and a read-only v1 cannot add to that menu. The lore has her travelling with the Tarnished, so
  she now converses in EldenNet's own overlay while resting at a grace, once she has met them.

### 21. DataPiler 0.2.1 and 0.2.2 went out as patch zips

`2026-11-09` · METHOD · `#datapiler #release` · refs: S6.3.n

- With little usage left that day, the owner asked for the new version without the release round, so each went as a
  zip of the changed files over 0.2.0, on the branches `feat/v0.2.1` and `feat/v0.2.2`; no pull request or release.
- Next: a proper release when the owner asks; 0.2.2's branch holds both.

### 22. The design's row leads for Melina, Kalé and Boc are in the data

`2026-11-14` · LIKELY (rows and numbers FACT, en-0004; who they are not checked) · `#roster` · refs: NPC§1.3, O59,
O48, WHY§13

- The design's guessed rows (NPC §1.3, §2) all exist with a real name: 21800000 (Melina) carries name 110000, on 5
  rows; 32000000 (Kalé) carries 180000; Boc's default 41109000 carries 122300 and his later form 523420000 carries
  122310, so one character's forms can show different names, as the design's `expect_name_id` per form allows.
- Doubt: 180000 is on 18 rows (32000000 to 32020035), more than one merchant would have; it may be the nomadic
  merchants' shared name, which would make the Kalé lead wrong.
- Next: the owner can look these four ids up in DataPiler's local `roster-named-rows.json` and confirm the names in
  chat; until then they are leads for `expect_name_id`, unverified.
- **Later (2026-11-14): FACT.** The owner's local file confirms all four: Melina, Merchant Kalé, and Boc under his
  demi-human name and, in his later form, his seamster's name. The doubt was wrong: 180000 is Kalé's own name, on 18
  rows. The name numbers go into NPC §1.3's examples as `expect_name_id`, still `verified = false` until checked in
  game (O48).

### 23. WorldMapPlaceNameParam has only 10 rows

`2026-11-08` · LIKELY · `#roster #maps` · refs: S6.3.m

- Extract's places row counted 10 WorldMapPlaceNameParam rows and 422 BonfireWarpParam rows (sites of grace and
  other warps, probably). Ten cannot be the game's place names, so places come some other way (map regions, or the
  place-name text by map); not settled.
- Next: look at it in the next place job, before EldenNet relies on either table.

### 24. TalkParam is the route from a character to its recordings

`2026-11-09` · METHOD · `#voice #datapiler` · refs: O62, ER03

- TalkParam (10,309 rows, all but one linked to a message) gives each line its message and a voice id, the Wwise
  event `Play_v<id>` (ER03). A DataPiler job can map a character's lines to its recordings, so the player's set-up
  for O62 picks the right samples; the audio stays in the player's install.

### 25. Detect works on the owner's PC: Smithbox's project record is read right

`2026-11-08` · FACT (0.2.0's first run) · `#datapiler #smithbox` · refs: DataPiler DP13, smithbox-projects.md

- The game's folder was found from three sources, the project from Smithbox's own record
  (`%LOCALAPPDATA%\Smithbox\Projects\<GUID>.json`, project type 8 for Elden Ring), and the Steam manifest listed four
  DLC app ids. So DataPiler's leads on Smithbox's record hold for 2.2.6.

### 26. Smithbox ships two exes side by side

`2026-11-08` · FACT (the owner's PC) · `#smithbox #datapiler`

- One folder holds `Smithbox.Vulkan.exe` and `Smithbox.OpenGL.exe`, of the same build; `Smithbox.exe` is only a lead.
  0.1.0 looked for `Smithbox.exe` alone and saw nothing; 0.2.0 matches `Smithbox*.exe` and reads the running one's
  build.

### 27. How to read Smithbox's source here

`2026-11-08` · METHOD · `#smithbox #process`

- Single files from `raw.githubusercontent.com/vawser/Smithbox/<commit>/<path>`, at a pinned commit (2.2.6 is
  `d144cc7`), into a folder of their own, read with `python3 -I`; never a clone, and the GitHub API is not enabled
  for that repository in this session. Directory listings are unavailable, so paths are found by probing.
- Found at 2.2.6, under `src/Smithbox.Program/`: `Utilities/SoapstoneService.cs` (the server),
  `Editors/Text Editor/` with `TextEditorScreen.cs`, `Framework/TextViewHandler.cs`, `Tools/FmgExporter.cs`,
  `Tools/TextDataTransferTool.cs`, `Tools/FmgImporter.cs`, `Data/TextBank.cs` and `Utils/TextUtils.cs`.

### 28. DataPiler's Windows-only failures and their causes

`2026-11-08` · FACT (CI) · `#datapiler` · refs: DataPiler CHANGELOG

- WinError 10053 in the caps test: the server closed a socket with an unread request body, which Windows answers with
  a reset; fixed by a lingering close that drains up to 8 MiB for 3 s after `shutdown(SHUT_WR)`.
- A manifest written with `write_text` gained CRLF line ends on Windows; `write_bytes` keeps them.
- Python 3.10 answers a garbled request with HTTP/0.9, so the test sends a well-formed but unknown method (a 501).
- After one failing push, commits wait until every test passes.

### 29. Publishing pages: what the sweep misses and how publishes fail

`2026-11-08` · METHOD · `#process`

- The pre-push sweep does not see the artifact tools, so every published page is swept by hand first.
- A publish needs a whole document with its doctype; the builders' `--fragment` writes the page without its CSP meta
  tag, otherwise whole. A container restart can cut a publish short: check the live version with `get_artifact`, then
  publish again on it.

### 30. Other sessions as consultants

`2026-11-08` · METHOD · `#process #graceui`

- The owner's Skyrizz session knows Skyrim's UI hosts (Magelight, PrismaUI, Meridian) and the release flow; the
  PandorumLLM session writes UI designs from a request file. Asking them saves research.
- What comes back is untrusted until read, and swept before it is committed: a design mock carried real paths from
  the owner's PC (replaced by invented `C:\` paths), and committed copies refer to the owner in neutral words.

### 31. Usage is the owner's call

`2026-11-14` · METHOD · `#process`

- When the owner says usage is low, keep to the essentials and skip extra rounds of checks (2026-11-09: "no rechecks
  and such"), though the privacy sweep still runs. Which model to use, and what it costs, is the owner's choice and
  no factor in advice (2026-11-14).

### 32. Smithbox 2.2.6 has no event-script editor

`2026-11-14` · FACT (`Editors/Project Editor/JSON/ProjectDescriptor.cs` at `d144cc7`) · `#smithbox #roster` · refs:
WHY§16, DESIGN§7.8

- A project's editors are Param, Text, Map, MapData, Model, Material, Anim, Gparam and Havok; none reads the event
  scripts, where a boss's bar name is set when its fight starts. So bar names cannot come through Smithbox, and
  DataPiler never opens the game's archives itself (no keys, no Oodle).

### 33. Most of the base game's bosses are missing from NpcParam's names

`2026-11-14` · LIKELY (en-0003) · `#roster` · refs: WHY§16, WHY§14

- Of the 213 real names, 49 appear on enemy-team (6) rows, mostly characters who also have a friendly form; only 14
  appear on enemy rows alone, 12 of them from DLC 1 (140903 to 143200) and 2 from the base game (121610, 121800). The
  base game has far more bosses than that, so most of them probably carry name 0 and get their bar name from the
  event scripts.
- Next: a route to the bar names (put to the owner, 2026-11-14): the names no character row uses, map placements,
  or EldenNet reading the bar in game.

### 34. Bosses sit in entity slots ending 800 to 899

`2026-11-14` · LIKELY (FromSoftware's habit, as modders know it; not checked in our data) · `#roster #maps` · refs:
DataPiler S46, WHY§36

- A map's boss usually has the entity id of its map block followed by 800 (for example a castle's boss at …0800 and
  a second at …0850). DataPiler 0.2.3's boss-slot pick keeps placements ending 800 to 899, so it may catch some
  helpers and miss a boss numbered otherwise.
- Next: en-0007's rows, checked against which bosses live in which dungeon, then in game (O48).
- **Later (2026-11-14):** en-0009 loaded the tutorial dungeon's map (m18_00_00_00, 53 placed characters): two
  boss slots, entity ids 18000800 and 18000850 (rows 46400007 and 43113906, models 4640 and 4311), as many as the
  bosses that map is known to hold. Neither row is among the 699 named rows, so both carry name 0, as WHY§33 expected.
- **Later (2026-11-14):** en-0007 found boss slots in 24 of the 27 legacy-dungeon maps that hold characters (WHY§46).
  The slots also hold helpers and adds, as feared, so a boss is told by its model and its count, not by its slot alone.

### 35. Smithbox lists every map as if loaded; DataPiler 0.2.x misread it

`2026-11-14` · FACT (`Utilities/SoapstoneService.cs`, `SearchObjects`, at `d144cc7`) · `#smithbox #maps #datapiler` ·
refs: DataPiler S44, WHY§6

- A file search over the map bank returns every map Smithbox knows, loaded or not; entries come only from loaded maps.
  DataPiler 0.2.x took the file search for the loaded maps, so it would have read every map as loaded at once and
  found no placements. 0.2.3 tells a loaded map by an entry search, and gains the list of all maps for free.
- The first map run's UNKNOWN came before any of this (WHY§6): the project's missing Map Editor.

### 36. The two routes to the bosses, and why both

`2026-11-14` · METHOD · `#roster #datapiler` · refs: WHY§32, WHY§33, WHY§34, DESIGN§7.7

- (a) en-0006: the character names no NpcParam row uses. A boss whose row carries name 0 shows its bar name from
  those entries, so they hold the bar names EldenNet binds bosses by, among unused or cut names.
- (b) en-0007: the characters placed in boss slots of the legacy dungeons. They give the bosses' rows, entity ids
  and models; EldenNet can bind by entity id as well (DESIGN §7.7).
- Neither links a boss's row to its bar name; the event scripts do, and Smithbox cannot read them. EldenNet sees both
  at once in game. Until then the two lists, with what is known of where each boss lives, are leads.
- Risk: check 6 refuses en-0006's ids if more than half of the names are unused; the card then still shows the
  count, and the next step is the owner's.
- **Later (2026-11-14):** the two routes meet after all, though not through the event scripts: most unused names are
  numbered by a model, and the boss slots give each boss's model (WHY§47). That narrows each boss to a few candidate
  bar names; the binding itself still waits for the game.

### 37. Only Smithbox's plain Map Editor makes the map data its server reads

`2026-11-14` · FACT (`Editors/Project Editor/ProjectEditorHandler.cs` lines 136–140 and 272–275 at `d144cc7`) ·
`#smithbox #maps` · refs: WHY§6, WHY§35

- `MapData`, which `SearchObjects` reads, is made only when the project's `EnableMapEditor` is on, and the Map Editor
  screen only when `MapData` exists. "Map Data Editor" (`EnableMapDataEditor`) is another editor. The owner first
  ticked Map Data Editor, and en-0007 met UNKNOWN again.
- So: tick the Map Editor in the project's Available Editors, press Update Project, restart Smithbox, keep its Map
  Editor tab open while a map job runs.

### 38. A whole text container makes ordinary words look like game text

`2026-11-14` · FACT (en-0006 and Copy for sharing, DataPiler 0.2.3) · `#datapiler #text` · refs: DataPiler DP18, DP17

- With the whole English item container imported (21,532 entries, descriptions included), check 3 found the table
  name `unused` inside some description and refused the share file, and refused a log copy for one line that listed
  file names. The names are the only texts DataPiler writes, so the owner narrowed check 3 to them (DP18).
- The same run showed 269 of 483 character-name entries unused (56%), over check 6's half; the owner let such
  number-only lists be shared whole (DP17). Job en-0008 runs en-0006 again on 0.2.4.

### 39. The map UNKNOWN was DataPiler's own request

`2026-11-14` · FACT (SoapstoneLib's `SoapstoneServer.cs` lines 108–121 and 223–227, at Smithbox `d144cc7`) ·
`#smithbox #maps #datapiler` · refs: WHY§6, WHY§37, DataPiler S51

- Smithbox's server library checks `request.Search` is set and throws otherwise, which a client sees as UNKNOWN.
  DataPiler's map listing sent no search clause from 0.1.0 on, so every map job failed, with or without the Visual
  Map Editor; the fake server in its tests answered such requests, so nothing caught it.
- 0.2.5 sends an empty clause (it goes on the wire as an empty field, checked), and the fake refuses a search without
  one. The rest of the map search was checked in the same source: the resource needs a type and a game, and the map
  key template is one the library knows.
- Lesson: a fake must refuse what the real server refuses; read the server's own argument checks before trusting a
  fake.

### 40. Smithbox loads maps only while its Map Editor is drawn

`2026-11-14` · FACT (`Editors/Map Editor/MapEditorStub.cs` lines 23–75, `Framework/MapCommandQueue.cs` lines 22–61, at
`d144cc7`) · `#smithbox #maps` · refs: WHY§39, DataPiler S52

- `OpenResource` queues `map load <id>`; Smithbox takes one queued command per frame and passes a "map" command to the
  Map Editor's `OnGUI` only while that editor's window is drawn (`ImGui.Begin` true), which then calls `LoadMap`. With
  Smithbox minimised or another tab in front, nothing loads: en-0007's first batch timed out after 900 s that way.
- Also learned: DataPiler's loaded check is sound (an empty search matches every entry, and map entries are a key type
  the library knows), and the old per-batch limit would have failed big dungeons anyway; 0.2.6 waits as long as maps
  keep loading and shows the progress.
- Next: en-0009 loads one small map family first, to see loading work and time it, then en-0007.
- **Later (2026-11-14):** en-0009 worked with the Map Editor in front: map loading through DataPiler is confirmed.

### 41. Smithbox loads one map at a time; ten at once kept one

`2026-11-14` · FACT (`Editors/Map Editor/MapUniverse.cs` lines 43–86, at `d144cc7`) · `#smithbox #maps #datapiler` ·
refs: WHY§40, DataPiler S53

- `LoadMap` refuses while another map is loading ("map already loading") unless a caller allows concurrent loads,
  which the command path never does. DataPiler 0.2.3 to 0.2.6 asked for ten at once, so Smithbox loaded the first and
  dropped nine, and the job waited for them in vain (en-0007's second run: 1 of 49 maps, then no more).
- 0.2.7 asks for one map at a time and asks again every 10 s until Smithbox takes it; the fake server refuses a load
  while another runs, so the tests now cover it.
- Parallel jobs ("array jobs", the owner's idea) cannot help here, for the same reason; they do help row reads, which
  Smithbox answers each on its own (0.2.7 reads four at once).
- **Later (2026-11-14):** confirmed by en-0007's third run on 0.2.6: 1 of 49 maps loaded within the first minute, then
  none for 15 minutes, as predicted. Timings from the same log: the tutorial map took 6.2 s (en-0009), the first
  legacy-dungeon map under a minute.

### 42. Where a map load's time goes, and why more PC does not help

`2026-11-14` · FACT (`MapUniverse.cs` lines 64–80 and 120–245, at `d144cc7`) · `#smithbox #maps` · refs: WHY§41

- Before the async load, Smithbox loads the map's Havok collision unless `MapEditor_SkipHavokLoad` is on; then, on the
  Vulkan build with `Viewport_Enable_Rendering` on, every model and (with `Viewport_Enable_Texturing`) every texture.
  The OpenGL build never loads map models or textures. DataPiler needs only the MSB's placed characters.
- The owner saw about 3.5 minutes for the first legacy-dungeon map with the PC nearly idle: Smithbox's own one-at-a-time
  work, not a lack of CPU, GPU or memory, so DataPiler cannot speed it up by using more of the PC.
- Next: time en-0009 with rendering off and Havok skipped (or on the OpenGL build) against the first run.
- **Later (2026-11-14):** with Enable Model Rendering off, the first map of en-0007's fourth run loaded in 2 s.

### 43. One map that never loads held up the rest

`2026-11-14` · LIKELY · `#smithbox #maps #datapiler` · refs: WHY§41, WHY§42, DataPiler S57

- en-0007 on 0.2.7: map 1 loaded in 2 s, map 2 never did in 15 minutes although DataPiler asked again every 10 s and
  Smithbox kept answering. Smithbox clears its loading state in `finally` (`MapUniverse.cs` lines 236–266), so it was
  not stuck loading map 1; the likely cause is that map 2's own load fails every time, its exception kept by Smithbox.
- DataPiler's log did not name the map, so it could not be looked up. 0.2.8 names each map, skips one not loaded
  after 5 minutes, leaves it out of the placements and stops only after three skipped in a row.
- Also found that day: 0.2.7's newest-first order sorted on a time the page never received (fixed in 0.2.8).
- Next: the map 0.2.8 skips, and what Smithbox's own log says when it fails.

### 44. What DataPiler can and cannot learn from Smithbox about a failed map

`2026-11-14` · FACT (`Smithbox.cs` lines 120–160, 228–250 and 485–495; `Application/CFG.cs` lines 1415–1465; at
`d144cc7`) · `#smithbox #maps #datapiler` · refs: WHY§43, DataPiler DP19, S58, S59

- Smithbox's log goes to its own windows (an in-memory provider behind the Action Log and Warning Log); no file and no
  server call carry it, so DataPiler cannot read why a map failed. The owner reads the Warning Log; DataPiler names the
  map and when.
- Its preferences live in `Configuration.json`, written when Smithbox closes or saves its configuration. DataPiler
  0.2.9 reads two of them (DP19) to set the first map's limit, and corrects from the first measured load on.
- A timeout is the only failure signal; with model rendering off a map loads in about 2 s, so 10 s a map is safe,
  with a second try at the end for any skipped.
- **Later (2026-11-14):** where the owner finds it (`Logger/GUI/LoggerUI.cs` lines 95–160): two small arrow buttons in
  Smithbox's top menu bar, after Help, open the Action Log and the Warning Log; each log window has Clear Log and Copy
  to Clipboard. en-0007 on 0.2.9 runs, skipping some maps after the short limit.

### 45. en-0007's full load: 46 of 47 maps, one empty, and Smithbox's auto-save

`2026-11-14` · FACT (DataPiler's and Smithbox's logs, 0.2.9; `SoapstoneService.cs` lines 192–215 at `d144cc7`) ·
`#smithbox #maps #datapiler` · refs: WHY§43, WHY§44, DataPiler S60, S61

- With model rendering off, 47 legacy-dungeon maps took about 13 minutes: small ones 2 s, big ones 20 to 55 s
  (Leyndell's m11_00_00_00 first skipped at 10 s, then counted as it loaded late). Smithbox's "a map is already in the
  process of loading" lines are DataPiler's 2 s re-asks, harmless.
- m10_01_00_99 never showed as loaded, yet Smithbox's auto-save named it among its loaded maps from the 0.2.7 run on:
  it loads with nothing an entry search lists (LIKELY an empty map), so WHY§43's "map that never loads" was this map,
  not a failing load.
- Reading placements then stopped on a placed character without TalkID: Smithbox leaves out a property a part lacks;
  0.2.10 reads it as -1.
- Smithbox's auto-save (every 5 minutes) wrote every loaded map, and the params, into the project folder ("Saved map:
  ..."): unchanged copies of the game's own, but files in the owner's Smithbox project, outside the game. DataPiler
  never asks Smithbox to save; loading maps for research makes them part of the project's auto-save.
- Also: thousands of "Unable to allocate texture descriptor" lines while 50 maps were loaded; restarting Smithbox
  after a map job frees what they hold.
- **Later (2026-11-14):** the empty map is handled since DataPiler 0.2.11, which tells loaded maps by their file keys
  (WHY§50).

### 46. What the legacy dungeons' boss slots hold

`2026-11-14` · FACT (en-0007 on DataPiler 0.2.10; `leads/boss-leads.json`) · `#roster #maps` · refs: WHY§34, WHY§33,
WHY§45, roadmap S6.3.p

- 50 maps read (m10_01_00_99 asked for twice and left out, as WHY§45 expected: the card's "50 of 51" counts it); 27
  hold characters, 24 of them boss slots. 141 placements in the slots, 46 models: 44 at …800 to …809, 23 at …850 to
  …859, the rest at …810 to …849 and …860 to …869.
- Not all are bosses. Four models fill most of the helpers' sub-ranges at once (24, 15, 8 and 4 of a kind), and
  twelve maps put rows at both …800 and …801, of one model or of two numbered one apart (a boss and its other form
  or partner, LIKELY). Five models with bar names sit in boss slots of two maps each, so one body serves two bosses
  or one boss met twice.
- 13 slot placements have rows that carry a real name in NpcParam (three names, 132400, 132500 and 120000), likely
  characters who also appear in another role, as WHY§33 expected; 120000 is on both rows of one two-form boss.
- The last two digits of a slot's row id are, with few exceptions, one value per map (LIKELY an area code), and rows
  ending 00 are shared base rows. Noted, never relied on.

### 47. Bosses' bar names are numbered by their model

`2026-11-14` · LIKELY (en-0007 and en-0008 side by side; the event scripts that state the link are out of reach,
WHY§32) · `#roster #text` · refs: WHY§36, WHY§46, O48, O59, DESIGN§7.7

- 240 of the 269 character names no NpcParam row uses are numbered 90, then a four-digit model, then three digits
  (102 models). Of the 45 non-human models in the boss slots, 34 have such names. The 11 without are a second model
  beside a named one (four cases, its number one off: a boss's other form under one bar, LIKELY), the four
  many-of-a-kind helpers of WHY§46, and three single placements.
- The layers agree with it: every expansion model (5000 and up) in the slots has its names in DLC 1's layer, every
  base-game model in the base game's; the human model's names (900000…) are in both.
- Variants: 000 to 002 look like a dungeon's own boss and its forms; 300s, 500s and 600s likely the same kind of foe
  met elsewhere (SPECULATION). 67 named models are in no legacy-dungeon slot: bosses of the smaller dungeons, the
  evergaols and the open world.
- So the leads give each boss a few candidate bar names. EldenNet's in-game check (S2 records the bar and its holder)
  confirms one, and nothing is bound by the pattern alone (O48).
- Next: en-0010 (the smaller dungeons, m30 to m45); the open world later, in parts, with Smithbox restarted between
  jobs and its auto-save off.
- **Later (2026-11-14):** en-0010 holds the pattern (WHY§49). The era check goes by map, not by model number: model
  7100 is placed in two base-game catacombs and its names are in the base game's layer, so "5000 and up" above was a
  gloss, not the rule.

### 48. The other unused names: 13 in the named range, 16 of one unknown family

`2026-11-14` · LIKELY (en-0008) · `#roster #text` · refs: WHY§33, WHY§47

- 13 six-digit numbers, in the range of named characters' names (6 base game, 7 DLC 1), are used by no row. They are
  likely names shown only by event scripts (a named character's bar name, or a name revealed later); 121810 sits next
  to 121800, a base-game name only enemy rows carry.
- 16 eight-digit numbers form one family: 101 to 117 (no 106), then five digits, all from the base game. What they
  name is unknown (SPECULATION: not characters the roster needs).
- The owner can tell their kind apart by looking a few up in DataPiler's local workspace; the names stay on the PC
  and in chat, never here.
- **Later (2026-11-14):** 10 of the 13 share their first four digits with real names on rows (the same character's
  other names, LIKELY). 130901 sits beside 130900, the name on two human rows in one cave's boss slots (WHY§49): a
  likely bar name for that character.

### 49. The smaller dungeons hold the model pattern: 61 of 101 named models placed

`2026-11-14` · FACT (en-0010 on DataPiler 0.2.10; `leads/boss-leads.json`) · `#roster #maps` · refs: WHY§46, WHY§47,
WHY§48, roadmap S6.3.p

- 71 maps of m30 to m45 hold characters, 60 of them boss slots: 115 placements, 42 models. Most dungeons have one
  boss at …800 (91 placements at …800 to …809), with helpers at …810 to …819 and a few second bosses at …850.
- 35 of the 41 non-human models have names of WHY§47's shape. The six without are again helpers (4 and 3 of a kind),
  the two odd single models of en-0007 (rows 10000000 and 1000000, likely stand-ins), and two numbered near a named
  model (3701 by 3700, 4603 by 4600). Every model in a base-game map has its names in the base game's layer, every
  model in an expansion map (m40 to m43) in DLC 1's: no exception in 256 slots.
- With en-0007: 76 non-human slot models, 61 with candidate bar names; 61 of the 101 named non-human models are now
  placed. The other 40 (25 base game, 15 expansion) are in no dungeon slot: likely the open world's bosses.
- Characters without boss slots: three m34 maps, five expansion maps (m40_02, m42_00 to m42_03) and the three m45
  maps, which hold one character each, numbered outside 800 to 899. 12 slot placements carry real row names: 130900,
  137600 and 142700 (nine of one, at m41_02's …800 and …810 to …817).
- Next: the open world, one job per family so no tile is missed: en-0011 (m60, the base game) and en-0012 (m61, the
  expansion), Smithbox restarted before each. If one is too much for Smithbox at once, it is split by tiles.

### 50. Empty tiles looked like failed loads; Smithbox's file keys tell them apart

`2026-11-14` · FACT (DataPiler's and Smithbox's logs of en-0011 on 0.2.10; `SoapstoneService.cs` `GetObjects` and
`GetServerInfo`, `Universe.cs` `LoadMapAsync` line 180, `SoulsKey.cs` `ParseFileKey`, at `d144cc7`) ·
`#smithbox #maps #datapiler` · refs: WHY§45, WHY§43, DataPiler S62, DP4

- en-0011 (m60, 829 maps) stopped twice at the same place, 452 of 829: three tiles in a row (m60_43_56_00 to
  m60_43_58_00, with m60_42_56_00 and two others skipped earlier) never showed as loaded. Smithbox's log shows each
  of them loading at once with nothing but its texture jobs: tiles with nothing in them. An entry search lists no
  entry for such a map, so 0.2.3 to 0.2.10 took it for a failed load, and three in a row for a stuck Smithbox.
- Smithbox sets a map's container once its MSB is read, entries or none, and answers a map's file key
  (`map/mapstudio/<id>.msb`) through GetObject and BatchGetObjects only while that container exists. Both calls are on
  DP4's list, so DataPiler 0.2.11 asks BatchGetObjects which maps are loaded, a few hundred keys a call. GetServerInfo
  also lists the loaded maps, but it is not on the list, and it carries the paths of Smithbox and the project.
- Also in those logs: Smithbox failed to read a few tiles' navmesh files (NVA), caught inside its loader, harmless to
  the read; its auto-save still ran, writing every loaded map into the project folder every five minutes.
- Next: en-0011 again on 0.2.11, then en-0012.

### 51. The graphics driver went down with Smithbox holding about 720 maps; jobs now keep what they read

`2026-11-14` · LIKELY (DataPiler's log of en-0011 on 0.2.11; Smithbox's logs of the runs before; `Universe.cs`
`LoadMapAsync` at `d144cc7`) · `#smithbox #maps #datapiler` · refs: WHY§50, WHY§45, DataPiler S63

- en-0011's third run loaded 718 maps in 33 minutes, then Smithbox (its Vulkan build) and the GPU's driver went down
  while loading m60_51_56_00, and the job failed with UNAVAILABLE. The owner's PC has had no driver crash in games or
  AI inference.
- DataPiler never touches the GPU; it only sends requests to Smithbox. Smithbox keeps every loaded map. On the Vulkan
  build it prepares each map's model and texture lists even with rendering off, and refreshes its textures after
  every load. Its log showed thousands of "Unable to allocate texture descriptor" lines from about 400 open-world maps
  (and after a few dozen legacy-dungeon maps). So Smithbox running out of graphics resources with hundreds of maps
  held is the likely cause, not the hardware. Windows' Event Viewer (System log, near 19:17) would say which driver
  reset; not checked yet.
- DataPiler 0.2.12 (S63): each map's placements are read as soon as it loads and kept on disk at once, so a crash
  costs only the map in hand; and a job pauses once Smithbox holds 300 maps, asking for a restart of Smithbox. The
  0.2.11 run's maps were not kept: that version had no such file.

### 52. What Smithbox puts on the graphics card for a map, and the OpenGL build as the way round

`2026-11-14` · FACT (`Universe.cs` `LoadMapAsync` lines 124–246 and `Smithbox.cs` lines 60–121 at `d144cc7`; the
owner's Smithbox folder holds both builds, DataPiler S1x facts of 2026-11-08) · `#smithbox #maps #datapiler` · refs:
WHY§51, DataPiler DP20, S64

- After reading a map's MSB, Smithbox's Vulkan build prepares its model and texture lists whatever the rendering
  setting (`Vulkan || Viewport_Enable_Rendering`), and every load ends with a texture and compass refresh; the models
  and textures themselves load only with rendering on. None of it serves DataPiler, which reads the entries only, from
  ordinary memory. The OpenGL build (`Smithbox.OpenGL.exe`, the same program started on its OpenGL context) skips the
  lists with rendering off. No program with a window runs without the GPU, so "no GPU" is not on offer, only less.
- Whether the OpenGL build spares the card in practice is untried (the refresh still runs). The owner chose a setting,
  on by default, that map jobs wait for the OpenGL build (DataPiler DP20, 0.2.13); the next open-world run is the
  trial, and if it shows no "Unable to allocate texture descriptor" lines, the 300-map pause of S63 can be raised.

### 53. The OpenGL build's first 300 maps: no texture warnings; one-go jobs, Resume job and an icon

`2026-11-14` · LIKELY (DataPiler's and Smithbox's logs of en-0011 on 0.2.13 with `Smithbox.OpenGL.exe`) ·
`#smithbox #maps #datapiler` · refs: WHY§51, WHY§52, DataPiler S65, S66, DP20

- On the OpenGL build, en-0011 loaded and read 300 open-world maps in 10 minutes (about 2 s a map, as on Vulkan),
  until 0.2.13's pause. Smithbox's Warning Log held only the navmesh (NVA) read errors, and no "Unable to allocate
  texture descriptor" line; on Vulkan those began near 400 maps, so the trial is short of proof, but the OpenGL build
  likely spares the graphics card.
- The owner wants a map job in one go: DataPiler 0.2.14 drops the pause (S65). Each map is still kept as it is read,
  so a crash costs the map in hand.
- 0.2.14 also adds Resume job (S66): a stopped run's card shows the maps kept and the next map, and Resume goes on
  from them; Start again asks first and starts from the beginning. Cards stopped by 0.2.12 or 0.2.13 show the count
  from their file.
- The owner asked for an exe icon after a famous blood-red katana of the game. DataPiler's rules keep game art out of
  a public repository, so the icon is our own drawing in its spirit (a blood-red katana in a rising slash, with
  drops), in three levels of detail so it reads at 16 px. The patch carries an exe built by BUILD.md's local route,
  with the toolchain's licence notices; the sweep's allow list now passes the authors' addresses in those notices,
  in that file only.

### 54. The open world's boss slots: 98 of 101 named models placed

`2026-11-14` · FACT (en-0011 and en-0012 on DataPiler 0.2.14; `leads/boss-leads.json`) · `#roster #maps` · refs:
WHY§47, WHY§49, WHY§53, roadmap S6.3.p

- en-0011 (m60, 829 maps, resumed after 0.2.13's pause, then in one go on the OpenGL build): 293 maps hold
  characters (14,716), 72 of them boss slots, 107 placements, 46 models; 38 of the 45 non-human ones have names of
  WHY§47's shape, all in the base game's layer. en-0012 (m61): 132 maps hold characters (8,741), 31 of them boss
  slots, 95 placements, 28 models; 16 of the 27 non-human ones have such names, all in DLC 1's layer.
- Open-world entity ids have ten digits and keep the …800 to …899 habit. Characters live mostly in the small tiles
  (_00); a tile's _10 version holds some too, and repeats 16 of its _00 tile's boss placements (same entity, row and
  model): kept once each. A few large tiles (_01, _02, _12) hold characters as well.
- Of all 101 named non-human models, 98 are now placed in a boss slot; 3253, 3575 and 3700 are in none (a boss
  outside the slots, or a cut one). Of the 128 non-human models in boss slots, 98 have candidates; the rest are
  helpers in batches (the expansion's world has more: one model 16 times), stand-ins (100, 1000) and models numbered
  next to a named one. Five slot rows carry real names in the expansion's world (142100 to 142400), one in the base
  game's (130401).
- The research for the bosses' leads is done; binding each boss to its bar name waits for EldenNet in game (O48).

### 55. The named characters' flag blocks, and the jobs for their placements and forms

`2026-11-14` · METHOD · `#roster #flags #datapiler` · refs: ER08§2, O51, O59, DESIGN§7.7, WHY§54

- Flag blocks: the owner allowed FromSoftSaveManager (Apache-2.0) as a source. Read at the commit ER08 pinned
  (`f2980db`), as text from an untrusted folder, never built or run. Its Elden Ring data gives 55 characters' blocks
  keyed by their first flag (3100 to 4800). The block's first four flags mark the character's status (friendly,
  hostile, dead), the base + 5 to + 18 their questline's stages, and 112 progress steps point into 238 flags; 6
  endings have reached, unlock and lock flags. `leads/npc-flags.json` keeps the numbers only, with slugs of ours as
  labels (DESIGN §7.7's "ids, numbers and our slugs"); its names, stage texts and ending names stay out, and the
  source, commit and licence are named in the file. Binding a block to a character goes through the roster's ids
  in game (O48).
- en-0013 reads every map once more (about 1,300, all families; resumable on 0.2.14) and keeps the placements of the
  699 named rows: each character's maps, entity ids, models and talk ids. en-0014 re-reads the named rows' team and
  type, which roster-rows.json did not keep, to tell friendly, hostile and summoned forms apart. Both pass
  DataPiler's checker (DP8: rows picked by an explicit list).
- Next: the two share files; then join placements, forms, flag blocks and boss leads into one census per character.

### 56. The map: the game's map only as the player's own copy; trails are sightings; masking against one upload

`2026-11-14` · METHOD · `#graceui #map #spoilers #licences` · refs: G21–G26, O47, O48, O51, GraceUI:docs/proposals/0002-map.md

- The game's map image is FromSoftware's artwork: an altered copy is still derived from it, so it is never committed,
  shipped or shown in our documents. It can be used only as the player's own copy, read at run time from their game,
  on the basis on which game text is read. The mock-up therefore uses an invented island and a painted stand-in.
- A map mod may change the image's size or framing, so the base map is a setting with a fit check, and GraceUI's own
  drawn base is the fallback (G21, after the owner's point about custom maps).
- SPECULATION: the game loads its map textures only when its own map opens, so replacing that map by the map key (G25)
  may leave nothing to read in memory; the archives are the other route (keys read from the player's game, never
  shipped). The spike S7.9.c settles it; no route means the drawn base is the map.
- Trails are sightings (D89): the game's characters move between places as their stories advance and are seen only
  when loaded near the player, so a trail is the places the player met them, never tracking.
- Invariant 1 (views receive projections only) meets invariant 11 (one texture upload, because hudhook 0.9.3 leaks a
  buffer per upload) in game: new areas are reached during play. Put as G26, recommending one upload at start masked
  at draw time, with a tested exception to invariant 1.
- The owner asked for glow; invariant 14 (no loops) is kept in the recommendation (still glow, one-shot flares), and
  put as G22.
- Later (2026-11-14): the owner requires the map's exact shape, areas and accurate placements in every base, our own
  texture included. A shape traced from the game's map is a copy of its art, so none ships: the drawn base is built on
  the player's PC from their own game (D97), and the spike (S7.9.c) must also find exact sources for it (the map
  image; the game's map data and terrain where a map mod does not fit).

### 57. The named characters' census: placements, forms and the talk id's two patterns

`2026-11-14` · FACT · `#roster #datapiler #talk` · refs: O59, O43, O45, O48, WHY§55, WHY§54

- en-0013 and en-0014 ran on game 2.7.2.0 (DataPiler 0.2.17). 204 of the 213 name numbers are placed on some map:
  422 of their 699 NpcParam rows, in 546 placements (242 in the open world, 67 in the expansion's, 237 in dungeons and
  other maps). Nine names are placed nowhere: spawned by scripts, or unused (leads only).
- A talk id's last four digits are its map's code (the first four digits of a dungeon map's id; 6000 in the open
  world, 6100 in the expansion's) in 341 of 341 placements. Its first three digits, the talk group, equal the name
  number // 100 - 1000 in 324 of 341; the 17 others are one name using another's group. So name number, NpcParam rows
  and talk scripts join on numbers alone, before any game text.
- 142 placed names are on one map; 62 are on two to eleven (map variants _10 counted once): the characters who move
  as their stories advance, the places the map's trails (D89) and the stage tracks will need.
- teamType splits the rows into families (26 and 27 mostly talk; 2 mostly in the 53xxxxxxx rows; 6 mostly without
  talk), but what each number means is not taken from any label: EldenNet checks it in game (O48).
- No simple numeric rule links the 55 flag blocks (WHY§55) to the talk groups; that join is EldenNet's in-game check.
- `leads/named-census.json` keeps it all as numbers, one name per line.

### 58. Where the map's coordinates can come from: five map tables, but no placement positions through Smithbox

`2026-11-14` · FACT · `#map #datapiler #graceui` · refs: O48, O59, G21, D97, WHY§56, WHY§57, DataPiler S67 and DP21

- Placed parts' positions cannot be read through Smithbox's server: at the build the owner runs (2.2.6), SoapstoneLib's
  `ToPropertyValue` turns only scalars and strings into property values, a part's `Position` is a vector, and
  Smithbox's `Entity.GetPropertyValue` takes a property's name with no path into it. Read as text from an untrusted
  folder at Smithbox's commit d144cc7. Characters' positions therefore come from EldenNet in game (the sightings,
  D89), not from DataPiler.
- Smithbox's definitions for Elden Ring name five map tables whose rows carry coordinates as plain numbers (an area, a
  grid cell and a position): the resting places (`BonfireWarpParam`), the map's points (`WorldMapPointParam`), the
  place names (`WorldMapPlaceNameParam`, each tied to a map piece), the pieces the map reveals
  (`WorldMapPieceParam`: rectangles, a flag, a centre and scales) and a conversion of the dungeons' coordinates onto
  the open world (`WorldMapLegacyConvParam`: source and destination area, grid and position, with base points).
  These are labels until verified (O48): DataPiler 0.2.18's `check_fields` (job en-0015) asks the owner's own
  Smithbox which fields exist, with no values.
- Sharing the coordinates themselves needs an exception to DataPiler's DP8, put to the owner as DP21 (recommended: a
  narrow one for these tables' coordinate columns, shared for research and never committed).
- Later (2026-11-14): en-0015 ran on the owner's PC (DataPiler 0.2.19, game 2.7.2.0). All 91 fields named are in the
  server's answers, each with the type Smithbox's definition gives (u8 area and grid numbers, f32 positions,
  rectangles and scales, u32 flags). The tables hold 422 resting places, 472 map points, 10 place names, 34 map
  pieces and 196 conversion rows. The fields are verified in DataPiler's sense (P4); reading their values waits for
  DP21.
- Later (2026-11-14): the owner decided DP21 in wider terms, "using game data for research and development is fine,
  we are just careful what we ship online". DataPiler 0.2.20 shares the five tables' rows whole under the label
  `map_coords`; job en-0016 reads them. The share file is research material: it stays out of every repository, and
  only what is derived from it (a fitted conversion, counts, error figures) is committed. The owner also chose the
  map's terrain (GraceUI G27 (a)): heights built locally, a 3D relief in the browser, our own sprites as the fallback.

### 59. The world map's frame: about one map unit per metre, north up, three sheets

`2026-11-14` · LIKELY · `#map #datapiler #graceui` · refs: WHY§58, G21, G27, D97, DataPiler DP21

From en-0016 (DataPiler 0.2.20, game 2.7.2.0), the five map tables' rows, read as research material that stays out of
the repositories. Only these derived numbers are kept here.

- **Three sheets.** Each resting place shows on one of three maps by its display mask: 284 on the first, 32 on the
  second (all of them in area 12, the underground), 105 on the third (the expansion's areas). Map pieces fall in three
  id ranges to match (0 to 20, 100 to 105, 1000 to 1004, plus 70 and 1070), each opened by a flag of its own.
- **One frame for the open world.** A world position is the grid cell times 256 plus the position in the cell. With a
  uniform scale s, map x = s (X - X0) and map y = s (Z0 - Z) (north up) places all four place names of area 60 inside
  their own pieces' rectangles and all 166 open-world resting places inside the first sheet's pieces for every s from
  0.90 to 1.02, and best near s = 1.0 with X0 about 7,700 and Z0 about 16,700. So the pieces' rectangles are very
  likely in a map space of about one unit per metre. The bounds are loose (100 to 1,000 units of slack): the exact fit
  needs anchors, a set of places whose map position is known, from the player's map image or the game in the spike.
- **The dungeons' conversion.** 196 rows, 194 marked base points, carry positions from 25 dungeon and underground
  areas into the open world (118 into area 60, 45 into 61, 7 into area 70) and a few into other dungeons: the frames
  the drawn base needs to place marks inside legacy dungeons on the world map.
- Only 10 place names exist as map labels, tied to pieces; the 422 resting places and 472 map points are the dense
  anchors.
- Next: anchors for an exact fit (map image or in game), then the fit's error per sheet, recorded here.
- Later (2026-11-15): the dungeons' conversion is mostly plain offsets. Grouping its rows by source area and grid cell
  and destination area, 44 of the 50 groups with two or more rows move every point by the same offset (within 0.5 m),
  so a dungeon's frame maps onto the world by one translation; 6 groups spread up to about 3.5 km (several pieces of a
  dungeon sharing one cell, to be split by their own base points), and 64 groups have one row only.
- Later (2026-11-15), a lead (O48: unverified): Smithbox's definitions name `MapNameTexParam` and `MapPieceTexParam`
  (and `_m61` twins for the expansion) as colour keys (red, green, blue) to an area-name id, a save-name id and a
  multiplayer area. So the game very likely keeps its areas as a colour-coded texture, one colour per area: the exact
  area shapes G21 and D97 ask for, readable from the player's install like the map image, and a second source of
  anchors for the exact fit. `MapGridCreateHeightLimitInfo` holds a minimum and maximum height per row: a possible
  coarse height lead for G27's fallback, its key to be found.
  Later the same day: en-0017 found neither of that table's two fields in Smithbox's answer (it may hold no rows, or
  other names); it was left out, as the relief comes from the terrain itself (G27).
- Later (2026-11-15): the exact fit is WHY§78. On the base map X0 is 7,041 and Z0 16,688 with s = 1 (the first guess
  was 659 units off across, which the pieces' rectangles allowed); the expansion has its own (7,120 and 16,684); the
  underground and the dungeons go through the conversion. Of the six groups of conversion rows that spread, two are
  rows to different maps of one area: grouped by source map and destination map, three spread far (areas 13, 15 and
  25, 551 m to 3.2 km) and one by 1.2 m.

### 60. The areas' colour keys: 16 area names and 49 finer areas on the base map, 11 names in the expansion

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§59, G21, D97, DataPiler DP21, DP24, DP25

From en-0017 (DataPiler 0.2.23, game 2.7.2.0): the four colour-key tables, research material kept out of the
repositories. Derived numbers only here.

- **Area names.** The base map's key gives 32 colours for 16 area-name numbers in five groups of three or four
  (61000 to 65002), each name under two colours (one with the name's number negative, one positive). The expansion's
  key gives 11 names, several rows switched off by their disable flag.
- **Finer areas.** The second key gives 49 colours on the base map for multiplayer areas in the same five groups
  (61xxxxx to 65xxxxx: 10, 13, 10, 7 and 9 of them), and 18 named areas among 42 colours in the expansion. 18 of the
  base colours are the very colours the name key uses, so one image very likely serves both keys: each pixel's colour
  says the area's name and its finer area. The order differs between the two (one name's colour is another's finer
  area number), so they join by colour, never by number.
- **What it gives the map.** If the image is found among the textures, the exact area shapes for the drawn base, the
  masking and the area labels come from it, and its colours checked against where the resting places stand give the
  exact world-to-map fit. Its name is not known yet; en-0018 (the map tiles, counted) is the next step.
- Later (2026-11-15): found, converted (WHY§80). The game ships the image with each pixel holding the two keys'
  row ids (red: finer area, green: area name) in 72 squares of 1,024 m at 4 m a pixel, not the keys' colours.

### 61. The game's map tiles: four sheets, five zoom levels, a variant per revealed piece

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§59, WHY§60, G21, G26, D90, DataPiler DP25

From en-0018 (DataPiler 0.2.23): counts of the map tiles the owner exported from Smithbox's Texture Viewer, numbers
only; the images stay on the owner's PC.

- **FACT, the layout.** 28,469 tiles, every one 256 by 256 pixels. Sheets 0 and 1 have five zoom levels on square
  grids of 41, 31, 11, 6 and 3 tiles a side; sheet 10 has three (41, 31 and 9 a side); sheet 11 has three small ones
  (its level 0 in a window of 16 by 10 tiles). Every grid position has a variant with code 0 at every level (1,681 =
  41 squared at level 0, down to 9 = 3 squared at level 4).
- **LIKELY, the codes.** The other variants carry codes that are sets of bits (0x10, 0x20, 0x40, 0x4000, 0x40000 on
  sheet 0; single low bits on sheets 1 and 10), so the code is very likely the set of map pieces revealed over that
  tile, and the game draws each tile in the variant for the pieces the player holds. Sheet 0 is the base map, sheet 1
  the underground and sheet 10 the expansion (their counts match WHY§59's three sheets); sheet 11 is a small fourth.
- **LIKELY, the scale.** Level 0 spans 41 x 256 = 10,496 pixels, which holds the pieces' rectangles (0 to about
  9,650), so the map's units are very likely level-0 pixels, and by WHY§59 about one level-0 pixel per metre.
- **What it gives the map.** EldenNet can draw the player's own map exactly as the game does, choosing each tile's
  variant from the pieces the player holds (their flags, WHY§58): the reveal G26 and D90 need comes from the game's
  own variants, not from shapes of ours. The colour-coded area image of WHY§60 is not among the map tiles.
- Next: en-0019, each level-0 tile's position and code, to tell which bit is which piece and test map units against
  level-0 pixels exactly.

### 62. Each tile code bit is one map piece, and map units are level-0 pixels

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§59, WHY§61, G21, G26, D90

From en-0019 (each level-0 tile's sheet, column, row and code, 12,442 tiles) against the pieces' rectangles of en-0016.
Numbers only.

- **Bits are pieces.** The base sheet uses exactly 21 code bits and has 21 pieces (0 to 20); the underground 6 and 6
  (100 to 105); the expansion 5 and 5 (1000 to 1004). Matching each bit's tiles to the rectangle that holds most of
  them, with no order imposed, pairs bit n with piece n on the base sheet for 18 of 21 bits (the rest involve pieces
  13 and 14, whose rectangles are identical, and one bit whose tiles span the whole map), bit n with piece 1000 + n
  on the expansion (each 73 to 100 per cent inside), and bit n with piece 100 + n underground (54 to 74 per cent).
- **Units are level-0 pixels, rows from the bottom.** The fit is best with one map unit to one level-0 pixel, no
  horizontal shift (within 64 pixels), and the tile rows counted up from the bottom of a map 41 x 256 = 10,496 pixels
  high: tile row = (10,496 - unit y) / 256. Shares under 100 per cent fit a reading that the pieces' rectangles bound
  where the map may be scrolled when the piece is open, not every tile it reveals.
- **With WHY§59** a world position goes to map units (about one per metre, north up), map units to level-0 pixels,
  and a tile's variant follows the bits of the pieces the player holds. What is still loose is §59's world-to-map
  offset (hundreds of units of slack): it needs anchors from an image.
- Later (2026-11-15): the offset is settled in WHY§78 (base map x = X − 7,041, y = 16,688 − Z, one unit a metre).

### 63. A tile's pieces that never meet have a variant each, laid together

`2026-11-15` · FACT (the variant sets), LIKELY (how the game draws them) · `#map #datapiler #graceui` · refs: WHY§61,
WHY§62, G26, D90

From en-0019's codes per level-0 tile position, and measurements of the owner's en-0020 overviews (numbers only; the
images stay out of every repository).

- **Variant sets.** On every sheet, a position's exported codes are exactly every subset of a few *widest* codes
  (codes no other code of the position contains), and nothing else. On the base sheet 290 of 1,681 positions have the
  base alone, 514 one widest code, 531 two, 315 three and 31 four; underground 191 positions have two; the expansion
  2. Widest codes can share a bit (152 positions on the base sheet): pieces 1 and 2 meet, 2 and 4 meet, 1 and 4 not.
- **So the game lays them together.** With every piece found there is often no single variant to show; the drawing
  is the base with each widest variant over it. DataPiler 0.2.26 showed only the code with the most bits, so the
  other pieces' parts of a tile stayed unexplored: these were the "joins" in the overviews. The joins themselves line
  up (across each tile edge the colour step matches the step inside a tile, and no shift or doubled border fits
  better), so the bottom-up layout of WHY§62 stands.
- **For GraceUI (G26, D90).** A map drawn from the player's own tiles picks, per position and widest code, the subset
  of the pieces the player holds, and lays those variants over the base: one texture per widest code, not one per
  position.

### 64. Level 2's export holds a second drawing of the map; levels 2 to 4 nest by two

`2026-11-15` · FACT (the nesting and the second drawing), LIKELY (that the game never draws it) · `#map #datapiler
#graceui` · refs: WHY§61, WHY§62, WHY§63, G26, D90, DataPiler S78

From measurements of the owner's en-0020 overviews of DataPiler 0.2.26 and 0.2.27 (numbers only; the images stay out
of every repository).

- **Levels 2, 3 and 4 nest by two, from the bottom left.** Each level-2 tile at the edge of the map matches the level-3
  image at twice its size, at its own place counted from the bottom left (correlation 0.82 to 0.96 at full size, about
  0.99 on small thumbnails); counted from the top it does not (about 0). Levels 0 and 1 do not nest so (41 and 31
  tiles a side: three quarters).
- **A second drawing.** A block of about 7 by 7 tiles at level 2 instead shows the whole map again at about three
  quarters of that scale, clouds and all, ending partway through a tile, with the right image beyond its edge. Its
  tiles match level 3 at their place at most about 0.4. They are variant files like the others, told apart by no
  name; laying every widest variant (0.2.27) brought more of them in than 0.2.26's one variant did.
- **So variants are checked, not trusted.** DataPiler 0.2.28 measures each variant against the next level's variant
  for the same pieces (and its base) and leaves out one below 0.7: on tiles cut from these images, the right drawing
  scored 0.90 to 1.00 and the other at most 0.46. That the game never shows the second drawing is LIKELY (its map at
  that zoom shows one image); whether it is a leftover or drawn at another zoom is open. en-0021 measures every tile
  of levels 2 to 4.
- **For GraceUI (G26, D90).** A map drawn from the player's own tiles must use only variants that pass this check (or
  the game's own rule, once known); the code alone does not say which file is part of the map.

### 65. With the check, level 2 is one image; its base and pieces 15 to 20 stand apart

`2026-11-15` · FACT (the scores, the pieces' rows), LIKELY (what they mean) · `#map #datapiler #graceui` · refs: WHY§62,
WHY§64, G26, D90, DataPiler S78

From the owner's en-0021 overviews (DataPiler 0.2.28; measured here, numbers only, the images kept out of every
repository) and en-0016's piece rows.

- **One image.** Laid with the check, every level-2 land tile matches level 3 at its place at 0.99 or more, but for a
  handful around Leyndell (0.52 to 0.84) and north of Caelid (0.56); dark margin tiles score low, as their texture
  does not line up. Level 3 itself is clean: nearly every variant scores 1.00 against level 4.
- **The base is not part of it.** Level 2's code-0 tiles are a dark margin with a faint outline of the land at another
  scale, transparent over part of column 8 and row 0: not the same drawing as the level's variants. Where a variant
  covers a tile this does not show.
- **Pieces 15 to 20 are not fragments.** Their open flags are 62004 to 62009 with no pickup event (the fragments, 0 to
  14, open by 62010 to 62053 with pickup events 63010 to 63053), their rectangles are wide regions (a quarter of the
  map or more), and levels 3 and 4 have no variant for them. At level 2 their variants are among those left out (0.2
  or less), with some fragment codes at some places (scores near 0) and partial ones (0.5 to 0.7) where level 3 lacks
  the same mix of pieces.
- Open: what flags 62004 to 62009 open, whether the left-out variants are stale files the game never asks for, and a
  rule for the mid scores. The per-tile numbers of en-0021 (`map-tile-check.json`) will tell more.

### 66. Level 2's export is two tile sets under one set of names; Smithbox's export overwrites by name

`2026-11-15` · FACT (the numbers, Smithbox's export code), LIKELY (that two sets were mixed by it) · `#map #datapiler
#graceui` · refs: WHY§64, WHY§65, G26, D90, DataPiler S78

From en-0021's `map-tile-check.json` (DataPiler 0.2.28, numbers only) and Smithbox's texture export source (read in an
untrusted copy, commit d144cc7).

- **Level 2 holds two sets.** Of its 2,045 variants, 424 match level 3 (379 at 1.00) and 1,621 do not; every one that
  matches has a variant of the same code at its level-3 place, and 1,391 of those that do not have none. All 854
  with bits 15 to 20 fail; level 3 and 4 use bits 0 to 14 only. Level 3 is clean against level 4 (519 of 525 match,
  504 at 1.00), and level 4 against level 3 (4,195 of 4,516).
- **One copy of each name, all in one folder.** Every tile was found once, in a single folder.
- **Smithbox writes a container's textures into one folder by their names alone** ("Export textures from container":
  each texture of each TPF in the binder, `File.WriteAllBytes` on `<name>.<type>`), with no check for a file already
  there. Two sets of tiles with the same names (two containers, two TPFs in one binder, or an earlier export left in
  the folder) end as one mixed set, the last written winning name by name.
- **So, LIKELY:** level 2's export is a set drawn for the levels as they are (codes of bits 0 to 14, matching level 3)
  mixed with a set at three quarters of that scale (bits 0 to 20, its own base), the second winning wherever both had
  a name. That explains the gaps (places where no right variant is left) as well as the second drawing.
- Next: the owner re-exports the map tiles container into an empty folder, with older exports moved out of
  DataPiler's folder, and notes which containers hold `MENU_MapTile` textures; en-0021 run again tells which.
- Later (2026-11-15): not the export. Smithbox lists one container of map tiles, one file per tile, and shows the same
  images as the export for two checked tiles, under the same names. The mixing is in the game's container. WHY§67.

### 67. The game's container holds both level-2 sets; the one that fits levels 0, 3 and 4 is incomplete

`2026-11-15` · FACT (the checks), LIKELY (that the game draws level 2 from neither alone) · `#map #datapiler #graceui` ·
refs: WHY§62, WHY§64, WHY§66, G26, D90, DataPiler S78

From the owner's look at two level-2 tiles in Smithbox, en-0021's numbers and en-0019's level-0 codes.

- **In the container.** `menu/71_maptile.tpfbhd` is the only container of map tiles; each file holds one texture of its
  own name. Two level-2 files the check left out (column 2, row 3, code 0x20, and column 4, row 8, code 0x100000) show
  in Smithbox as they were exported: part of a dark margin with land at its edge, and open sea with a city.
- **Set B fits the other levels exactly.** For all 424 level-2 variants that match level 3, every piece of the code is
  among the level-0 codes of the 4 by 4 level-0 tiles beneath it, with no shift: level 2 = level 0 at a quarter, level
  3 = level 2 at a half. Shifted by a tile either way, the share falls to about a quarter.
- **Set A fits neither.** For the 1,621 others it is 0.27 at a quarter scale and at most 0.65 at three sixteenths with
  any shift; it has all of level 2's code-0 tiles and every code with bits 15 to 20. Set B has no code-0 tile, files at
  67 of 121 places, and 59 subsets missing from its own widest codes.
- **So** the map's own drawing at level 2 is set B, with gaps; the game must draw that zoom from something else, or
  from set A at another scale (en-0022, the level-1 codes, tests whether set A is level 1 at a quarter). For GraceUI:
  draw from levels 0, 3 and 4 (and 1 once checked), and from level 2 only where a variant passes the check.

### 68. Level-1 codes do not settle levels 0 and 1; set A is not level 1 at a quarter

`2026-11-15` · FACT (the shares), LIKELY (what they leave open) · `#map #datapiler #graceui` · refs: WHY§62, WHY§67, G26,
D90

From en-0022 (each level-1 tile's sheet, column, row and code: 8,078 tiles, 4,660 on the base sheet) against en-0019's
level-0 codes and en-0021's level-2 sets. The test, as in WHY§67: is every piece of a variant's code among the codes of
the finer tiles beneath it?

- **Level 1 against level 0, at three quarters:** 2,873 of 3,699 variants (78%) pass with no shift, and about as many
  with any shift (best 80%): no clean nest of the kind level 2's set B shows (100% in place, 27% shifted). Variants
  with bits 15 to 20 pass 95%; those with bits 0 to 14 only, 69%.
- **Set A against level 1:** 58% at a quarter with no shift, best 87% at a third with a shift of 192 pixels: set A is
  not level 1 at a quarter. Set B against level 1 at a third: 79% in place, best 92% shifted.
- **What it leaves open:** a level-1 tile spans one and a third level-0 tiles a side, so the codes beneath hold most
  bits anyway (the shifted share barely drops): codes alone cannot tell whether levels 0 and 1 hold one drawing each.
  That needs their images, at a size an overview can take (level 1 at a half, level 0 at a quarter). Until then the
  map's fit uses level 3, which is clean against levels 2 (set B) and 4.

### 69. Every base-map level checked: level 0 is mixed like level 2, level 1 hardly passes

`2026-11-15` · FACT (the scores), SPECULATION (what level 1 is) · `#map #datapiler #graceui` · refs: WHY§64, WHY§67,
WHY§68, DataPiler S79

From en-0023 (DataPiler 0.2.29, numbers only): every variant of the base map's five levels against level 3 (level 3
against level 4), at LEVEL_SCALES from the bottom left.

- **Levels 2 to 4 as before.** Level 3 passes 519 of 525, level 4 4,195 of 4,508, level 2 424 of 2,045 (the same
  variants as en-0021).
- **Level 0 is mixed.** 1,353 of 5,169 measured variants pass (26%), in two clear groups (978 score 0.9 or more,
  1,403 below 0); 633 of the 1,391 places with variants have one that passes, spread over the whole map. Of 1,796
  variants with bits 15 to 20, 3 pass. The scale and anchor are right for those that pass: scores of 0.9 cannot come
  by chance.
- **Level 1 hardly passes.** 38 of 3,698 (1%), at 32 of 806 places, with failing scores near 0 both ways. Either
  level 1's scale or anchor is not three quarters from the bottom left, or it is all of the other drawing.
- Next: en-0025 lays levels 0 and 1 out unchecked, and level 1's base, to fit their scale against level 3 by image.

### 70. Level 1 sits at the top of its grid; levels 0 and 1 hold an old tile set four columns off

`2026-11-15` · FACT (the offsets), LIKELY (what the old set is) · `#map #datapiler #graceui` · refs: WHY§64, WHY§67,
WHY§69, DataPiler S80

From the owner's en-0025 images (levels 0 and 1 laid out without the check, shrunk four times), each tile located on
its own in the level-3 image by normalised cross-correlation (numbers only; the images stay out of every
repository), and en-0024's check of the other sheets.

- **Two sets at level 0, one scale.** Of level 0's tiles that match level 3 well (0.6 or more), 97 sit where their
  names put them and 326 sit exactly 128 level-3 pixels further right: 1,024 map units, four level-0 tiles, with no
  vertical offset. Level 1 shows the same two groups (three level-1 tiles apart). So the container holds a second set
  of tiles at the same scale whose columns are numbered four apart: an older layout of the same map, LIKELY left
  behind by a patch. Level 2's odd set is not this shift (WHY§67), and stays left out.
- **Level 1 sits at the top.** At three quarters of level 0 its tiles match about 12 level-3 pixels lower than a
  bottom anchor puts them, the same in every row (11 to 13): the 64-pixel gap its 31-tile grid leaves over the map
  (10.7 level-3 pixels) is at the bottom. Assuming the bottom is why level 1 passed 1% in en-0023; the underground's
  level 1 (7% in en-0024) has the same grids.
- **The other sheets (en-0024).** The underground: levels 3 and 4 pass 95% and 96%, level 2 67% and level 0 63%.
  The expansion passes almost nothing at any level, so its scales or anchors differ from LEVEL_SCALES; sheet 11 has
  base tiles only. en-0026 re-checks levels 0 and 1 with level 1 lifted and the old set's place measured, and lays
  out the expansion's levels to fit its geometry.
- **Later (2026-11-15, WHY§73):** reversed. The tiles that match level 3 four columns further right are today's,
  as the game's masks name them, and level 3 is the older layout; the tiles that matched it in place are that
  layout's leftovers. Level 1 at the top of its frame stands.

### 71. The expansion's levels 1 and 2 sit at the top left; level 2 is 2,089 / 10,496 of level 0; the old set fills 960 places

`2026-11-15` · FACT (the fits and counts), SPECULATION (sheet 11) · `#map #datapiler #graceui` · refs: WHY§67,
WHY§70, DataPiler S81

From the owner's en-0026 images of the expansion's map (sheet 10, every level laid out unchecked; numbers only, the
images stay out of every repository) and en-0026's check of levels 0 and 1 with the old set's place measured.

- **The expansion's geometry.** Its level 0 is the base map's 41 by 41 tiles. Level 1 is three quarters of it at the
  top of its 31-tile grid, as on the base map. Level 2 is not a quarter: fitted against level 0 shrunk four times it
  is 1 / 1.258 of that, so 2,089 of level 0's 10,496 pixels a side (0.199), at the top left of its 9-tile grid
  (2,304 pixels), with no offset. DataPiler 0.2.31 holds these as the sheet's own scales and anchors.
- **What the old set adds.** On the base map's level 0, 1,654 variants fail at their own place but match 1,024 map
  units to the right. At that place, 694 are already covered by today's passing variant of the same code, 485 stand
  in for today's variant of the same code that fails, 456 add pieces today's passing variants lack, and 19 fill a
  place with no passing variant: 960 in all. Level 1 has 788 (296, 231, 243 and 18), the underground's level 0 243
  (188 covered, 55 standing in) and its level 1 146 (109, 37). An overview at levels 0 and 1 now moves them there.
- **Still open.** The third set, which fails both in place and moved (level 0 2,162, level 1 2,569), and level 2's
  odd set. Sheet 11's grids (34 by 21, 26 by 17 and 7 by 6 tiles) fit neither table; SPECULATION: a smaller map with
  its own scales, which its overviews in en-0028 will show.
- Next: en-0028 re-checks the expansion at its own scales and lays out every sheet's levels 0 to 2 with the old set
  moved; en-0027 unpacks the game's masks (WHY§72).
- **Later (2026-11-15, WHY§73):** the expansion's geometry is every sheet's. The variants 0.2.31 moved were today's
  tiles, moved into the older layout; DataPiler 0.2.32 withdraws the moving.
- **Later (2026-11-15, WHY§76):** measured on the game's own overviews, level 2 is 0.19875 of level 0 (about
  2,086 of 10,496 pixels), not 2,089, and level 1 0.74857, not three quarters.

### 72. The game's tile masks are a KRAK-packed binder of four files, one per sheet; DataPiler opens it with the game's Oodle

`2026-11-15` · FACT (the file and its layers), SPECULATION (what a mask holds) · `#map #datapiler #smithbox` · refs:
WHY§70, WHY§71, DataPiler DP28, S81

From the owner's Smithbox unpack of the game's `menu` folder and the file they uploaded (kept in scratch only).

- **The file.** `menu/71_maptile.mtmskbnd.dcx` sits beside the tiles' own containers. It is a DCX packed with Oodle
  (KRAK): 5,873 bytes that unpack to 300,712, a BND4 binder of four `.mtmsk` files, `menu_maptile_m00`, `m01`, `m10`
  and `m11`, one per sheet, in the game's ScaleformMaptile mask folder. Smithbox unpacks it but shows nothing of it.
  SPECULATION: each mask holds, for every tile place and level, which pieces reveal it: the game's own rule for which
  variant to draw, which would settle WHY§70's old set and the third set at once.
- **Opening it.** DataPiler 0.2.31 reads the DCX and BND4 layouts itself (learnt from SoulsFormats, MIT, read only)
  and loads the game's own `oo2core_6_win64.dll` from the Game folder for KRAK (DP28); the owner's copy is the
  64-bit library with `OodleLZ_Decompress` among its exports, read without loading it. Its call was checked here
  against a stand-in library built for the purpose.
- **The owner's full unpack.** Smithbox unpacked the whole game into DataPiler's folder quickly, but failed to write
  the same 75 or so files in two runs: patched shaders, two map variants (`m60_42_40_10`, `m60_47_42_10`), a few
  sounds, one script and the game's two network key files. None is in `menu`; the keys were never written (no such
  folder), and DataPiler refuses a job that names a key or the regulation.
- Next: en-0027 unpacks it on the PC; the owner sends the four masks, kept in scratch, to decode their layout.
- **Later (2026-11-15, WHY§73):** en-0027 opened it on the owner's PC with the game's own Oodle. The four files are
  `MENU_MapTile_M00.mtmsk` to `M11` in `menu/ScaleForm/maptile/mask`, and the speculation holds: each is an XML
  list of the pieces that touch each tile.

### 73. The game's masks name today's tiles; levels 3 and 4 are an older layout; one geometry on every sheet

`2026-11-15` · FACT (the masks, counts and fits), LIKELY (why the older layout is there) · `#map #datapiler #graceui` ·
refs: WHY§63, WHY§67, WHY§70, WHY§71, WHY§72, DataPiler S82

From the four mask files en-0027 unpacked on the owner's PC (kept in scratch only), against the tile lists of en-0019
and en-0022 and the scores of en-0026 (numbers only), and the owner's en-0021 image of level 2's base tiles.

- **What a mask is.** Each `MENU_MapTile_Mxx.mtmsk` is plain XML: `MapTileMask` entries whose id is level * 10,000 +
  column * 100 + row, as the tiles' names number them, and whose mask is the pieces that touch that tile, for levels
  0 to 2 only. The base map has 1,369, 784 and 65 entries, the underground 418, 250 and 26, the expansion 480, 288 and
  30; sheet 11's 2,434 are all marked as not existing (it has base tiles only).
- **The game's rule.** At every entry of sheets 0, 1 and 10 every subset of the mask is exported, the mask's own code
  always. So the game draws the variant whose code is the mask without the pieces not yet revealed, and any other code
  at a place is not drawn. The base map's level 0 has such leftovers at 877 places (2,319 variants): exactly S77's
  places with several widest codes, which were never pieces to lay together.
- **The older layout.** The leftovers fall inside today's masks four columns to the left at level 0 (704, against 636
  at five) and three at level 1, with no row shift. Against level 3 the image check passed 1,324 leftovers in place and
  3 of today's variants, while 1,304 of today's passed four columns right. So levels 3 and 4, which have no masks, are
  an older layout whose ground sits 1,024 map units right of today's, and the leftovers are its tiles: WHY§70's "old
  set" is today's. LIKELY: the map was laid out anew when the expansion came, and the container kept every file no
  new one replaced.
- **One geometry.** Fitting each piece's tile extent at levels 1 and 2 against level 0 gives, on the base map, 0.748
  (across) and 0.750 (down) at level 1 with the map at the top (offset 88 pixels; 64 expected) and 0.191 and 0.199 at
  level 2 (offset 189; 215 expected); the expansion, the control, gives about 0.75 and 0.20 (offset 212). Level 2's
  base tiles on the owner's en-0021 image turn transparent at 8.16 tiles across and 0.84 up: a 2,089-pixel map at the
  top left of a 9 by 9 frame. So every sheet has the expansion's geometry (WHY§71), and WHY§67's quarter was the older
  layout's level 2.
- DataPiler 0.2.32 draws overviews from the masks, adds `in_mask` to the check, uses the one geometry and never takes
  level 3 or 4 as the reference. en-0028 was withdrawn before it ran; en-0029 re-checks levels 0 to 2 of every sheet
  and lays them out from the masks.
- **Later (2026-11-15, WHY§76):** the one geometry holds, with level 1 at 0.74857 and level 2 at 0.19875 of level
  0, measured to a third of a pixel on the game's own overviews.

### 74. en-0029: today's tiles match at levels 0 and 1 and on the expansion; level 2 met the older layout's level 3

`2026-11-15` · FACT · `#map #datapiler` · refs: WHY§73, DataPiler S82, S83

From en-0029 (DataPiler 0.2.32, numbers only): every variant of levels 0 to 2 of sheets 0, 1 and 10 against the
reference level (level 2; level 2 itself against its nearest level), with `in_mask` from the game's masks.

- **Today's tiles match.** Of the variants the masks name, the base map's level 1 passes 1,982 of 1,982 (median
  0.961) and level 0 2,312 of 2,873 (0.840); the underground's level 1 326 of 326 and level 0 458 of 519 measured; the
  expansion's levels 0, 1 and 2 634 of 760, 502 of 502 and 98 of 98. So the one geometry of WHY§73 holds on every
  sheet. Of the other layout's variants, 31 of 2,296 pass in place on the base map's level 0.
- **Level 2 met the wrong reference.** On the base map and the underground no variant of level 2 passed (median
  0.099): as the reference itself it was measured against its nearest level, and level 3, the older layout, tied with
  level 1 and won as the coarser. The expansion, which has no level 3, passed 98 of 98. DataPiler 0.2.33 measures each
  level the game draws against its nearest such level (level 0 against level 1), never 3 or 4.
- **Base tiles score lower** at level 0 (554 of 1,681 pass, median 0.614): unrevealed parchment, whose grain does not
  scale between levels as the drawing does, compared with level 2 at a fifth. Against level 1 more should pass.
- Next: en-0030 checks again on 0.2.33. en-0029's overviews, drawn from the masks without any check, show each level
  as the game draws it.

### 75. en-0030: levels 1 and 2 match on every sheet; level 0 against level 1 drifts from the top left

`2026-11-15` · FACT (the scores, the speed), LIKELY (a scale off by about a thousandth) · `#map #datapiler` · refs:
WHY§73, WHY§74, DataPiler S83

From en-0030 (DataPiler 0.2.33, numbers only): every variant of levels 0 to 2 of sheets 0, 1 and 10 against its
nearest level the game draws.

- **Levels 1 and 2 match everywhere.** Today's variants pass 1,982 of 1,982 and 737 of 737 on the base map, 326 of 326
  and 55 of 55 on the underground, 502 of 502 and 98 of 98 on the expansion (medians 0.96 to 1.0); level 2's bases too.
  The other layout's pass almost none (2% at most). So the masks' tiles and today's geometry hold at levels 1 and 2.
- **Level 0 drifts.** Against level 1 (cells of 6 level-1 pixels) today's level-0 variants pass 54% on the base map,
  66% on the underground and 57% on the expansion, fewer than against level 2 in en-0029 (80%, 88%, 83%). On the base
  map the share falls from the top left: by columns from 70% (0-4) to 30-48% (25-39), by rows from 90% at the top to
  32-47% low down; en-0029's numbers show the same fall, gentler. LIKELY: level 0 is not exactly 4/3 of level 1 about
  the top left (a thousandth off moves the far side by some 10 level-1 pixels); the images will say.
- **Speed.** On the owner's PC en-0030 took about a minute, where checks of this size took about half an hour before
  0.2.33 (worker processes, Windows' own decoder).
- Next: fit level 0 against level 1 on en-0029's overviews (each drawn from the masks), then the exact world fit.

### 76. Every level of every map draws whole from the masks; the levels' true sizes are 0.74857 and 0.19875

`2026-11-15` · FACT (the images, the fits), SPECULATION (sheet 11) · `#map #datapiler #graceui` · refs: WHY§71, WHY§73,
WHY§75, DataPiler S85

From the owner's en-0029 overviews, each drawn from the game's masks with every piece revealed (levels 0 and 1 shrunk
four times; the images stay in scratch, never in a repository).

- **Whole maps.** Levels 0 to 2 of the base map, the underground and the expansion each show their map fully revealed,
  with no stray tile and no seam: the Lands Between filling 92% of its canvas at every level, with the dark margin to
  the right and below; the underground's three cave regions; the Realm of Shadow under its great tree, inside its faded
  border. Sheet 11 shows only a grey grid with a rough placeholder in one corner, and its masks are all marked as not
  existing: SPECULATION, an unused or test sheet.
- **The true sizes.** Matching 128-pixel patches of each level inside the next and fitting each axis by least squares:
  level 1 is 0.74855 of level 0 on the base map and the expansion (0.74866 on the underground) and level 2 0.19875 on
  both, level 2 against level 1 0.26545; every offset within half a pixel of the top left, residuals 0.3 pixel over 100
  to 335 patches. So three quarters (WHY§70) and 2,089 / 10,496 (WHY§71) were near but not exact, and WHY§75's drift
  is the difference, about 14 level-1 pixels at the far side. DataPiler 0.2.34 holds the measured sizes.
- Next: en-0031 confirms the check on 0.2.34; then the world-to-map fit at level 0, the map now known tile by tile.

### 77. en-0031: at the measured sizes every variant the game draws passes, at every level of every map

`2026-11-15` · FACT · `#map #datapiler` · refs: WHY§75, WHY§76, DataPiler S85

From en-0031 (DataPiler 0.2.34, numbers only): every variant of levels 0 to 2 of sheets 0, 1 and 10 against its
nearest drawn level, at the measured sizes.

- **All of today's variants pass:** the base map's 2,873, 1,982 and 737 at levels 0, 1 and 2, the underground's 520,
  326 and 55, the expansion's 760, 502 and 98 (medians 0.98 to 1.0). Level 0's bases pass as well (1,681 of 1,681 on
  the base map, median 0.99, where en-0030 had 0.44): the parchment scales with the drawing after all, and only the
  sizes were off.
- **The other layout's tiles do not** (0 to 2%; 2 of 20 on the expansion): the masks tell them apart exactly.
- So the map tiles are settled: which tile the game draws at every place and level (its masks, WHY§73), and where each
  level lies against level 0 (WHY§76). Next: the world-to-map fit at level 0.

### 78. The world-to-map fit: one transform per open-world sheet; the underground and the dungeons go through the game's conversion

`2026-11-15` · FACT · `#map #datapiler #graceui` · refs: WHY§59, WHY§62, WHY§76, WHY§77, G21, D97, DataPiler DP21

Measured in scratch on the five map tables (en-0016) and the game's own level-0 overviews (en-0029), which stay out of
the repositories; only these derived numbers are kept.

- **Map units.** One map unit is one level-0 pixel, from the top left of the sheet's 10,496-pixel frame (WHY§62); a
  level-1 or level-2 pixel is the unit times 0.74857 or 0.19875, also from the top left (WHY§76). A world position is
  X = grid x × 256 + position x and Z = grid z × 256 + position z, in metres.
- **The base map (sheet 0, area 60):** map x = X − 7,041 and map y = 16,688 − Z: one unit a metre, north up. Fitted on
  the place-name banners drawn into the tiles, each name centred on its banner. Measured the same way, the five
  banners on this sheet (four names of area 60 and one carried from area 10 by the conversion below) lie −17 to +2
  units from their names across and within about 20 up and down. All 166 open-world resting places land on drawn land.
- **The expansion (sheet 10, area 61):** map x = X − 7,120 and map y = 16,684 − Z. Its one place name lies within about
  10 units of its banner, and all 59 of its open-world resting places land on drawn land.
- **The underground (sheet 1) and the dungeons have no transform of their own.** A position in any other area is
  carried onto the open world by `WorldMapLegacyConvParam`, then drawn with a transform above: the base map's for the
  underground (its sheet shares the base map's frame) and for the dungeons that end in area 60, the expansion's for
  those that end in 61. The rule that fits: take the rows whose source is the position's own map (its area and both
  grid numbers, which name a map in a dungeon and a cell in the open world); prefer a row straight onto area 60 or 61;
  else follow a row to another map and start again (one underground map goes through two other areas). Where one map
  has rows to the same destination with different offsets (areas 13, 15 and 25, 551 m to 3.2 km apart: pieces of one
  dungeon placed apart), the nearest base point is used (LIKELY: no check here tells them apart).
- **Checks.** The three underground place names land on their banners (0, +4 and +36 units across, within about 20 up
  and down). All 30 underground resting places with a row land on the drawn caves; 118 dungeon resting places land on
  the base map's pieces and 46 on the expansion's, the sheet each one's display mask names (WHY§59: 284, 32 and 105).
  Three have no row (two in one underground map, one in area 19); how the game places them is not known.
- **Precision.** About ±20 units on the base map and the expansion (the banners' own placement is the limit, and the
  expansion has only one); about ±40 underground, where two paths through the conversion can differ by up to 36 units
  (a loop of rows does not quite close) and one banner sits 36 units off. A resting place's icon is larger than that
  at level 0. A few units better would need the game's own icons: one screenshot per sheet at level 0 with resting
  places showing.
- The transforms are ours, fitted. The conversion rows are game data: read from the player's install at run time,
  never committed.
- Next: EldenNet's map projection and GraceUI's map page (G21) use these: a world position to its sheet, map units
  and level pixels.

### 79. The areas' colour image is very likely the map-information binders, one per kilometre square

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§60, WHY§78, O48, DataPiler S81

- Smithbox's file dictionary for the game (read as text from an untrusted copy) names 72 files
  `other/mapinfotex/m6x_XX_ZZ_02.mapinfotexbnd.dcx`: 48 for area 60 and 24 for area 61, one per level-2 cell of the
  open world (very likely 4 × 4 cells of 256 m, so 1,024 m a side). They fill area 60's cells 8 to 14 across and 7
  to 16 up, and area 61's 10 to 14 and 8 to 13. Nothing else in the dictionary is named for map names, areas or name
  textures.
- So the colour keys' image (WHY§60) is very likely cut into these squares, a texture or more in each binder. A name
  in a dictionary is a label until the files are read (O48).
- Jobs en-0032 (the 48) and en-0033 (the 24) unpack them on the owner's PC with DataPiler's unpack step (0.2.31
  onwards). The inner files stay local, and the owner uploads them as research material. Next: read their textures
  in scratch, check their colours against the keys and the resting places (WHY§78's transforms), and if they hold,
  record the areas' shapes.
- Later (2026-11-15): confirmed in WHY§80. The binders hold the areas' image, converted to the keys' row ids.

### 80. The areas' image ships converted: each map-information square holds the two keys' row numbers

`2026-11-15` · FACT · `#map #datapiler #graceui` · refs: WHY§60, WHY§78, WHY§79, G21, D97, DataPiler S81

From en-0032 and en-0033 (DataPiler 0.2.34): the 72 binders unpacked on the owner's PC and read in scratch as research
material. Numbers only here.

- **What a binder holds.** Two uncompressed 256 × 256 images (24-bit BMP), so 4 m a pixel over its square. The colour
  keys' own definitions call their colours those of "the map image before conversion", and the game ships the image
  converted. In the first image the red channel holds a row id of the finer-area key (`MapPieceTexParam`: 48 of the
  base map's 49 values, all 42 of the expansion's) and the green channel a row id of the area-name key
  (`MapNameTexParam`: 29 of 30, and 31 of 31). The one other value is 0, and 255 (a row of both keys) marks no area.
  The blue channel holds a third set of numbers that neither key has (24 values on the base map); its meaning is not
  known.
- **Placement.** A binder `m6x_XX_ZZ_02` covers world X from 1,024 × XX − 128 to 1,024 × XX + 896 (the 256 m cells
  are centred on their numbers), and Z likewise, with rows from north to south. So a world position's square is
  ((X + 128) // 1,024, (Z + 128) // 1,024), and its pixel follows at 4 m.
- **Check.** Read that way, 224 of the 225 open-world resting places land on a named area, and 93 per cent of them on
  the main name of their own resting-place sub-category. The other readings (no 128 m shift, or rows south to north)
  give 160 to 199 and 67 to 76 per cent. Drawn on the game's level-0 overviews through WHY§78's transforms, the area
  borders follow the map's own cliffs, rivers and coasts on both sheets: an independent check of those transforms.
- **The second image.** On the base map it is empty (255) but for three quarters of the northernmost square, which
  holds 0 in both images. On the expansion its red channel holds finer-area ids where the first image has an area (46
  values, four of them not rows of the expansion's key). Very likely a second layer for places stacked one above
  another; which layer applies where is not known yet.
- **What it gives the map.** The exact shapes of the area names (16 on the base map, 11 on the expansion) and of the
  finer areas, at 4 m, for G21 and D97. They are read from the player's install at run time and never committed, and
  the names' text comes from the game's own place names, by `mapNameId`, at run time too.
- Next: the blue channel and the second layer; then GraceUI's map labels from these shapes.
- Later (2026-11-15): the blue channel and the second image are very likely the weather and mimicry keys (WHY§81).

### 81. The map-information images' other channels: very likely weather (blue) and the Mimic's Veil (second image)

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§80, O48

From en-0032 and en-0033 (research material, read in scratch) and Smithbox's table definitions (an untrusted copy,
read as text; labels until the rows are read, O48). Numbers only here.

- **Four keys, not two.** The definitions name four tables keyed by "the map image before conversion": the area names,
  the finer areas, a weather key (`WeatherLotTexParam`: a weather lottery id) and a mimicry key
  (`MimicryEstablishmentTexParam`: a per-map mimicry probability id, which decides what the Mimic's Veil turns the
  player into), each with an expansion twin. The expansion's mimicry key notes that the expansion added choosing the
  mimicry targets place by place "through the map information image param", so the images carry these keys too.
- **The blue channel** is a key of its own. Its regions are large and smooth and cross both the name and the
  finer-area borders: 78 per cent of the base map's land lies in its finer area's main blue value, 98 per cent on the
  expansion. The base map holds 22 values plus 0 (31 per cent of the land, very likely "none") and 255; the
  expansion 29. Very likely the weather key's rows.
- **The second image** is empty on the base map. On the expansion it copies the first image's red value on 89 per cent
  of the land and differs on 44,568 pixels (11 per cent) in six squares, always by a neighbouring number (92 to 93,
  50 to 56, 52 to 55, 40 to 42, 47 to 48), most of them rows of neither known key. Very likely the mimicry key's rows:
  a key the expansion added, laid out on the finer areas with a few splits. It is not a second floor of finer areas:
  the two expansion resting places on the differing ground read as the first image's finer area.
- **Heights.** The expansion's weather key holds a border height in metres and separate weather lots for high and low
  ground (`isUseHeightInfo`, `borderHeight`), so ground stacked above other ground is split by height inside a key's
  row, not by a second image. The mimicry key's own high and low ids are marked unused, in favour of the multiplayer
  area.
- **To confirm:** the two keys' row ids against these values. DataPiler does not read these tables yet: they are not
  among its params, and a share file may hold their rows whole only under DP21's research label. Either a new DataPiler
  version or Smithbox's own CSV export, done by hand, would give them.
- Later (2026-11-15): en-0034 (DataPiler 0.2.36) read the base weather key, 25 rows, then stopped: the owner's
  Smithbox (2.2.6) names only the five fields the four keys share (the colour and the weather or mimicry id), not the
  expansion keys' height fields, which come from a newer Smithbox's definitions. So the height split stays a label
  (O48). en-0035 reads the five shared fields of all four keys.
- Later (2026-11-15): confirmed by en-0035 (WHY§82): blue is the weather key, the second image the mimicry key.

### 82. Confirmed: the blue channel holds the weather key's rows, the second image the mimicry key's

`2026-11-15` · FACT · `#map #datapiler #graceui` · refs: WHY§80, WHY§81, DataPiler S89

From en-0035 (DataPiler 0.2.36): the four keys' rows on the owner's PC, against en-0032's and en-0033's images. Numbers
only.

- **Weather.** Every blue value of the first image is a row of its sheet's weather key (`WeatherLotTexParam`, and
  `_m61` on the expansion), on every pixel of land: the base map's 24 values among its key's 25 rows (row 51 unused,
  row 0 included), the expansion's 30 among its 30.
- **Mimicry.** Every red value of the expansion's second image is a row of the expansion's mimicry key
  (`MimicryEstablishmentTexParam_m61`, 48 rows, row 72 unused), on every pixel, the neighbouring numbers of WHY§81
  included. The base map's mimicry key has one row, 255, with no mimicry id (-1), which is the value its second image
  holds everywhere but the northernmost square's 0.
- So the map-information images carry four keys, all rows of the game's own colour-key tables: finer area (red),
  area name (green), weather (blue) and, on the expansion, mimicry (the second image's red).
- The expansion keys' height fields (WHY§81) are not in the owner's Smithbox 2.2.6 (en-0035's check: absent), so
  splitting stacked ground by height stays unverified (O48).

### 83. The design fold: proposal 0001's parts adopted by reference, what waits for G18 to G20, and three settlements of the fold's own

`2026-11-15` · DECIDED · `#graceui #design` · refs: GraceUI G15–G17, G21–G27, GraceUI DESIGN §2.1, §4.13, D69–D101; S7.8.c

- **How.** Proposal 0001's §4 (but §4.10) is part of GraceUI's design by reference, with DESIGN §2 as its binding
  summary and D69 to D81 and D83 to D88 in §13; proposal 0002's design is GraceUI DESIGN §4.13, with D89 to D98 as its
  §14.3 and the answers revise them. Where a proposal and the design differ, the design holds.
- **What waits.** D82 and proposal 0001 §4.10 (EldenNet's pages in EldenNet's repository) wait for G20, so GraceUI's
  §4, SP1–SP9 and the goldens stay in GraceUI for now; when the framework opens and ABI 1 is frozen waits for G18;
  SkyrimNet-derived material in GraceUI waits for G19.
- **Settled by the fold** (GraceUI D99 to D101): the Map is a later-version statement in v1.0's rail (principle 8,
  G7); a fault in GraceUI's menu turns off the menu alone, never consumers' panels and notices; the settings D84 moves
  keep their names (`hotkeys.menu`, not proposal 0001's `menu.key`).
- **EldenNet's side** is GraceUI's eldennet-changes §11, folded into DESIGN, ARCH and NPC the same day; the older items
  of §1 to §10 (the F10 to F5 change among them) stay S7.1's.

### 84. Under G15 EldenNet links no hook library, so any later hook of its own needs the owner

`2026-11-15` · FACT · `#graceui #hooks #design` · refs: GraceUI invariant 16, G15; DESIGN R12, §3.6, §13; ARCH §8.11; O49

- GraceUI's invariant 16 leaves the process one hook library, GraceUI's MinHook, and no consumer may link one. R12 is
  now "EldenNet hooks nothing".
- Two later EldenNet hooks the design keeps as options would need one: O49's hook-based event sources (behind the
  `hooks` feature, compiled out in v1, already needing a spike and the owner) and the `CreateFileW`-family save
  redirect of DESIGN §3.6 (deferred to v1.x).
- So each, when proposed, also needs the owner's decision on how to hook beside GraceUI's MinHook: a hook service in
  GraceUI's ABI, or an amendment to invariant 16. Nothing in v1 is affected. Recorded in DESIGN R12, §3.6 and §13.
- Later (2026-11-15): decided by the owner as O65 (a): EldenNet may link one hook library of its own for functions
  GraceUI does not hook, each hook with its spike and approval; GraceUI's invariant 16 became "GraceUI's functions have
  one owner", with its published list (GraceUI D102).

### 85. Two of the inventory's suggestions rejected: G1's later HTML spike and its fallbacks stay

`2026-11-15` · METHOD · `#graceui #design` · refs: GraceUI G1, G16, proposal 0001 §4.2; GraceUI DESIGN §10, §11

- The fold's inventory suggested dropping "HTML in game if G1 ever changes" from GraceUI's §10 and "an Ultralight
  spike" from §11's fallbacks, reading G16 (a) as ruling out HTML in game.
- Rejected: G16 (a) rules out HTML from other mods in game, but proposal 0001 §4.2 keeps G1's later spike, with its
  three approvals, for GraceUI's own pages. Both lines stay.
- Also kept apart: proposal 0001 §6 freezes ABI 1 at 1.0, while G18 (a) publishes a frozen ABI 1 with 1.1. GraceUI's
  §10 leaves the freeze to G18's answer.

### 86. One owner per hooked function: what the evidence on hook libraries rules out, and what it does not

`2026-11-15` · FACT · `#hooks #graceui #design` · refs: ER02 §7.1, WHY§84, O65, GraceUI invariant 16

- ER02 §7.1: the conflict er-mods-rs names is "two MinHook instances detouring one prologue and corrupting each other's
  trampolines"; two DXGI Present compositors conflict; hudhook bundles its own MinHook, so a second hudhook-based DLL is
  exactly that case.
- So the evidence rules out two hook libraries on one function, and two overlays over one screen and one set of keys.
  It says nothing against a second library hooking other functions.
- GraceUI's invariant 16, as folded on 2026-11-15, bans any hook library in a consumer, which is stricter than this.
  Proposal 0001 (plugins) puts the narrower rule to the owner as O65 (a), with EldenNet's own hooks installed only
  after `connect` has answered, so that GraceUI's are already in place and the two never patch at the same moment.
- Later (2026-11-15): O65 decided (a), on this reading.

### 87. SkyrimNet's plugin model: plugins call its published functions, and nothing hooks SkyrimNet

`2026-11-15` · FACT · `#plugins #skyrimnet #design` · refs: SkyrimNet's `PublicAPI.h` (MIT, read in the Skyrizz
checkout), O68, O53

- SkyrimNet's plugin header (MIT, its licence covering the plugin API headers) is loaded at run time with
  `LoadLibraryA` and `GetProcAddress`, from an SKSE plugin's data-loaded handler. It names 45 functions.
- They fall into: versions; actions and their categories, registered from C++; events (registering an event type, and
  event callbacks); decorators (context for prompts); memories, diaries and world knowledge (read, add, update,
  delete); actors (names, bios, engagement, related actors, search, marking one busy); recent events and dialogue, the
  timeline and the player's context; plugin settings; and custom prompts and decisions sent to the model.
- So a SkyrimNet plugin never patches SkyrimNet: the game's script extender loads it, and it calls SkyrimNet.
  EldenNet's code plugins would take the same shape (proposal 0001 §7), with GraceUI's ABI rules and EldenNet's
  projections, re-checked against Min's blueprint when it arrives (O53).
- Later (2026-11-15): SkyrimNet 1.2.0's header holds 51 function pointers, not 45: the count above left out the
  named-place functions and the package probe (WHY§97).

### 88. One plugin cluster: what a shared host can tell apart, and why a library clash needs a check

`2026-11-15` · FACT · `#plugins #design` · refs: O70, proposal 0001 §13.2, §13.3

- **Libraries.** Microsoft's account of the DLL search order: when a DLL is found by name and a module of that name is
  already loaded, Windows uses the loaded one, whichever folder it came from (redirection and manifests aside). So two
  plugins in one process that ship different versions of one library cannot both have theirs: the reason for the
  validator's clash check, and for Detached as its way out.
- **Attribution.** In one host the plugins share memory: a crash in a plugin's own code on its own thread names that
  plugin, but one plugin can damage memory another then trips over, so a crash elsewhere can only be "most likely".
  A process per plugin would make every attribution certain; the owner chose one cluster with a per-plugin Detached
  row instead, which gives certainty where it is wanted.
- **Limits.** Windows' job limits apply to a process as a whole, so in a cluster memory cannot be capped per plugin;
  each plugin's processor time can still be measured per thread.

### 89. The terrain's heights are very likely the ground models, stored at three levels of detail

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: G27, WHY§79, WHY§78, O48

- Smithbox's file dictionary (read as text from an untrusted copy) lists, for the open world (area 60), 850 model
  binders (`mapbnd`) at each of three levels: in the 532 small tiles' folders (`m60_XX_YY_00`, 256 m), the 152 middle
  squares' (`_01`, 512 m) and the 48 large squares' (`_02`, 1 km, the same 48 as the area image's, WHY§79). Each
  model's number names its small tile (`423600` for tile 42_36), so every piece of ground is very likely stored three
  times, at three levels of detail. No file in the dictionary is named for heights or terrain.
- So the heights G27 needs are very likely the vertices of these ground models, placed by each square's placement file
  (`msb`); the collision files beside them are a second source, which DataPiler cannot open yet. A name in a
  dictionary is a label until the files are read (O48).
- Job en-0036 unpacks one large square (`m60_10_09_02`: 32 models), the middle square inside it holding the tile
  (`m60_21_18_01`: 10) and that small tile (`m60_42_36_00`: 4), with their three placement files. Next: read the
  models' vertex positions and the placements in scratch, build a height grid, compare the three levels, and check the
  heights against the resting places' own (WHY§78's points).
- Later (2026-11-15): confirmed on one large square by en-0036 (WHY§90): the ground models hold the heights, and the
  tile numbers in their names alone place them. The placement files were not needed, and were not read.

### 90. The terrain's heights are the ground models: one 1 km square read, its tiles placed by their numbers alone

`2026-11-15` · FACT · `#map #datapiler #graceui` · refs: G27, WHY§89, WHY§78, O48

- en-0036's 46 models were read in scratch with a reader of our own for the model format (vertex positions and
  triangle lists; the files stay in scratch). Every piece's vertices are in its small tile's own frame, centred on the
  tile. Placed by the tile numbers in their names alone, 256 m a tile, the large square's 32 pieces for its 16 tiles
  meet at every shared edge: over 2,531 samples 2 m apart, the nearest surfaces on the two sides differ by a median
  0.04 m, and 95% by 0.19 m or less. So the tiles share one vertical datum, and the ground needs no placement offset.
- Against the resting places' own heights in tile 42_36 (WHY§78's points): open ground agrees to 0.02 m and 1.34 m;
  two points on structures read 3.6 m and 15.2 m above the ground, since structures are models of their own.
- The three levels agree. In tile 42_36 the middle level differs from the finest by an rms of 0.01 m (at most 0.35 m),
  and the coarsest by 0.03 m (at most 0.95 m). The ground's triangle edges are a median 2.0, 2.8 and 2.9 m long at
  levels 0, 1 and 2 (90% under 2.8, 4.5 and 8.2 m). So the 1 km level is enough for the relief: 32 models and 3.5 MiB
  unpacked for 16 tiles, against 10 models and 2.0 MiB for 4 tiles at the middle level.
- Large steps at tile edges are the land itself: overhangs and cliffs, where the topmost surface changes (35 of the 70
  steps over 1 m meet within 0.5 m on another surface), and one corner where ground at about 170 m falls to a flat
  floor at −52 m.
- Gaps: at 4 m a sample, 93.5% of the square has ground, from −52 to 301 m. One tile at the square's edge (40_38) has
  a single piece, covering 18% of it (the piece's own bounding box says the same, so the reader misses nothing); the
  others lack 0 to 11%. What lies in the gaps (water, or a structure's own models) is not read yet. Each piece's name
  carries a two-digit prefix before its tile number; three values occur here.
- Next, with the spike (S7.9.c): the run-time read of these models from the player's archives, a rule for the gaps,
  and the rest of the map, the expansion and the underground included, none of it read yet. The collision files stay
  unread.
- Later (2026-11-15): the prefix takes 16 values in this square, one per tile, not three (the three above were only the
  pieces looked at): its first digit is the tile's column in the large square (0 to 3), its second the row plus 4 (4
  to 7).
- Later (2026-11-15): pieces can reach past their tile (3 of these 32, by up to 50 m), so the edges were measured
  again with all pieces together (WHY§92): top-surface steps over 1 m cross 1.0% of the edge samples against 0.2%
  across the tiles' middles. Tile 40_38's gap stands, and the far view has no ground there either.

### 91. The expansion, a far view and the underground in the file dictionary, and two jobs to read them

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: G27, WHY§89, WHY§90, O48

- Smithbox's file dictionary (the untrusted copy, read as text) gives the expansion's open world (area 61) the same
  three levels as area 60: 425, 423 and 423 model binders, over 22 large squares of 1 to 58 models each.
- Both areas also have a second set at levels 10, 11 and 12, numbered like levels 0 to 2: 53 models a level in area 60
  (four large squares) and 58 in area 61 (one). Perhaps ground that changes with the world's state; not read yet.
- Each area has a level 99, one map for the whole area: 86 models in area 60 and 17 in area 61, each numbered by a
  large square after a two-digit kind (80, 81, 90, 91 and 92 occur). Perhaps the land as seen from afar: if so, a
  coarse relief of the whole map would take a few dozen models.
- The underground (area 12) is eight maps of 4 to 134 models, numbered without tiles, so where each piece sits is for
  the maps' placement files to say; three maps have a second placement file, numbered 99. The model numbers start with
  a two-digit group, and 90 occurs in each of the five largest maps, as in level 99.
- A job takes at most 64 steps, so the reading is split in two. en-0037 (63 steps): one expansion square whole
  (`m61_12_10_02`: 50 models, 9 resting places), one of its tiles at the finest level (`m61_48_40_00`: 5), the far-view
  models for that square and for en-0036's (3), and five placement files. en-0038 (64): one underground map whole
  (`m12_03_00_00`: 63 models, 6 resting places) and its placement file. Next: WHY§90's checks on the expansion, the far
  view against level 2, and in the underground the placements and how to tell a floor from a ceiling.
- Later (2026-11-15): read in WHY§92. The two-digit prefix of an eight-digit model number is its tile's x and z
  numbers modulo 8 (04 for tile 40_36, 13 for 49_43): 2,758 of the dictionary's 2,768 eight-digit names fit, and the
  other 10 name no tile of their square.

### 92. The expansion, the far view and the underground: what en-0037 and en-0038 show

`2026-11-15` · FACT · `#map #datapiler #graceui` · refs: G27, WHY§90, WHY§91, WHY§78, O48

- The placement files were read with a reader of our own (the param lists, the model names, and each part's type,
  model, position, rotation and scale). In tile 42_36 one resting place and three map points sit exactly (0.00 m) on
  parts of one type, and the four ground pieces at the tile's origin, so its positions are checked against the
  owner's own data (O48).
- The expansion square (`m61_12_10_02`, 50 models, 5.6 MiB): its 1 km placement file puts each piece at its tile's
  offset from the square's centre, unturned and unscaled, the same as placing by the tile numbers. 7 of its 9 resting
  places sit within 0.03 m of the ground and 5 of its 9 map points within 0.12 m (the others 0.9 to 30 m off, on
  structures). Level 2 against level 0 in tile 48_40: rms 0.01 m, at most 0.18 m. Heights −572 to 463 m; ground on 30
  to 100% of each tile.
- Its pieces are of two kinds: one per tile numbered under 50 (here a deep floor, −573 to 10 m), and pieces from 50 up
  (the land above it, −85 to 464 m), which reach up to 133 m into the next tile. Top-surface steps over 1 m cross
  8.6% of the tile edges' samples against 1.4% of the tiles' middles (the base game: 1.0% against 0.2%). At the larger
  steps the land stops exactly at a tile edge, and the far view goes on at the land's height across 52 of 72 steps
  over 50 m: there the land continues in other models (the 1 km placement file also places 4,177 assets).
- The far view (level 99) is one model per 1 km square, centred on it and placed by its square in the level-99
  placement file. Against level 2 its nearest surface lies a median 0.4 m away, but 74 m (base game) and 296 m
  (expansion) at the 95th percentile: coarse at cliffs (2,807 triangles for the base square). It lacks 19% of the
  base square's ground and adds 12% to the expansion square's where level 2 has none. The second kind (91) covers a
  third of the base square, so it is no relief by itself; the base square's gap (tile 40_38) has no far view either.
- The underground map (`m12_03_00_00`, 63 models, 31.5 MiB): 60 pieces sit at the map's origin, two are moved by
  0.3 m, and one by about 620 m across and 91 m up, turned 42.2° about the vertical (which way is not settled: it
  meets no other piece's floor either way). Three pieces 16 to 39 km wide are a backdrop; the other 60 lie within 1.7
  by 1.4 km. Floors and ceilings face opposite ways in the files' winding: 5 of the 6 resting places stand on a
  floor-facing surface within 0.2 m (the sixth 25.6 m above one, on a structure), with ceilings 58 to 622 m above.
  In 8 m cells there are up to 34 floor layers (90% at most 7), so the map needs a rule for which floor to show; one
  tried with triangle centres missed the large ceiling triangles, and the rule stays open. The low-detail groups (90
  and 91, 1.0 MiB) cover under half of the floors.
- So level 2 gives the open worlds' ground exactly; where the expansion's land is made of other models, the far view
  fills it coarsely (or its assets would); the underground is read whole, placed by its placement file, with floors
  told by their facing. Each model's lower-detail triangle sets were left out this time (level 2 against level 0
  unchanged). Next, for the spike (S7.9.c): the underground's floor rule, and whether the expansion's assets are worth
  reading.
- Later (2026-11-15): the far view's second kind does fill the base square's gap: model 911009 covers 66% of tile
  40_38's gap, at −39 to 417 m (a median 321 m, where the ground around it lies near 180 m), so the gap is most
  likely a large structure, kept in its own map and seen from afar only in that kind (LIKELY). "No far view either"
  above, and in WHY§90's Later line, holds for the first kind only. How the relief shows a structure's footprint is
  for the spike.

### 93. The underground's floor rule: the topmost floor no steeper than 50°, checked on 250 placed characters

`2026-11-15` · FACT · `#map #datapiler #graceui` · refs: G27, WHY§92, O48

- In `m12_03_00_00` (en-0038) the placement file places 356 characters (enemies, players and stand-ins) and the map
  has 6 resting places. 250 of them stand on a map piece's floor-facing surface within 1.5 m below them; the others
  stand on structures, in water or in the air. Those floors are gentle: 95% at most 14°, the steepest 23°.
- Rules tried on the pieces' layers under each of them, without its own height (the 3 backdrop pieces and the 13
  low-detail ones left out): the topmost floor-facing surface picks their floor for 90%; the topmost one no steeper
  than 40 to 60° for 91% (228 of 250; 93% without the stand-ins), and never one too low; the lowest floor for 82%. A
  roof test adds nothing: the caves' ceilings lie in the backdrop pieces, over everything.
- The 22 misses all stand under an upper level, 11 to 74 m above them with a ceiling between: 10 under one piece's
  flat upper floors (48 and 58 m up), 10 stand-ins under one piece's floor 74 m up, and 2 under overhangs. A relief
  seen from above shows the upper level there, as a map would; the browser's 3D view could show both.
- So the rule, for now: the topmost floor-facing surface no steeper than 50°, from the map pieces alone. Jobs en-0039
  (57 steps: `m12_05_00_00`, `m12_08_00_00`, `m12_09_00_00`) and en-0040 (40: `m12_07_00_00`, `m12_04_00_00`) read
  five more underground maps whole, to check it there; the two largest (`m12_01`, `m12_02`) would take five more jobs.
- Later (2026-11-15): it does not hold elsewhere (WHY§94). On the five maps of en-0039 and en-0040 it picks the
  characters' floor for 91% in one, 41% in a map of stacked levels, and for none in three small maps, where a
  floor-facing surface about 55 m above everyone, with nothing between, is the rock's outer top. Over all six maps:
  73% (635 of 873).

### 94. The underground's floor rule, revised: the topmost walkable floor reachable from where the characters stand

`2026-11-15` · FACT · `#map #datapiler #graceui` · refs: G27, WHY§92, WHY§93, O48

- en-0039 and en-0040 read five more underground maps whole (`m12_05`, `m12_08`, `m12_09`, `m12_07` and `m12_04`: 92
  models, about 117 MB unpacked). The plain rule of WHY§93 fails in four of them (its Later line).
- The revised rule, our own: a grid of 2 m cells holds each walkable floor layer (floor-facing, no steeper than 50°,
  from the map pieces alone, the backdrop and low-detail pieces left out), joined to the layers of the four
  neighbouring cells within a step of 2.9 m. The parts joined to a floor that a placed character or a resting place
  stands on are the reachable floor, and each cell shows its topmost reachable layer.
- Tested by seeding with half of each map's characters and checking the other half (454 over six maps): their floor
  is reached for 99.1%; the rule picks it for 85.9%, a floor above it for 13.7% (an upper level the seeds reach too,
  a median 71 m higher), one below it for 0.2%, and none for 0.2%. The plain topmost floor on the same grid: 69.6%.
  In the three small maps the rule goes from none to all (36 characters), in the map of stacked levels from 36% to
  70%.
- The reachable floor is 43 to 79% of each map's walkable layers; the rest is rock tops and ledges no one stands on.
- Where levels stack, the relief shows the upper one, as a map seen from above; the browser's 3D view could show each
  reachable level. A part with no character and no resting place would not show, and floors joined only by
  structures (bridges, lifts or stairs made as assets) join only if both sides hold a character; neither was seen to
  matter here (99.1% reached).
- At run time EldenNet needs, for each underground map: its pieces, its placement file (for the placements and the
  seeds) and the resting places' positions, all read from the player's install.

### 95. Min's engine arrived: a closed Windows DLL with an MIT interface, not a source blueprint

`2026-11-15` · FACT · `#engine #process` · refs: O53, O60, O74, O75, S6.1

- On 2026-11-15 the owner supplied the engine Min released to early adopters: FatelessAiEngine 0.2.0. One Windows x64
  DLL (47 MB) and 22 libraries it loads (ggml for the CPU, Vulkan and CUDA; whisper; parakeet; ONNX Runtime; FFmpeg),
  MIT headers for a COM-style interface, a host guide and a binary licence: about 304 MB, about 135 MB without the CUDA
  library, models downloaded on request. It is kept outside the repositories and never committed.
- The licence: use the DLL and ship it unmodified inside one's own product, with its licence and notices; no
  modifying, reverse engineering or shipping it on its own; the names not used to promote a product; its online
  services not granted. The headers are MIT. The FFmpeg libraries are LGPL: separate, replaceable files, their source
  shipped or offered. So O53's "read, build, run and modify" does not reach the engine itself: EldenNet builds on its
  interface.
- It gives: LLM requests with variants, failover, streaming, structured output and typed decisions (four at once by
  default); prompt templates in Inja with SkyrimNet's additions, the host pushing every template and answering its
  functions; speech to text (local whisper), voice detection, text to speech (a local model, or Min's cloud); model
  downloads on request; a sign-in to Min's service; telemetry; tracing.
- It does not give: a context engine, memories, event history, a content store, pacing, budgets, per-request
  deadlines, audio playback, game access or a UI. Min says the content store comes next.
- How it behaves: one engine a process; events arrive on its worker threads; creating, reloading and releasing its
  services blocks for seconds, so a host calls it only from workers of its own. The host chooses every folder; a log
  is always written. It contacts Min's service only if the host creates the sign-in or telemetry services (telemetry
  sends nothing before consent); otherwise only the LLM providers configured, OpenRouter being the default endpoint of
  a slot that names no provider. Keys come only from a plain-text file the engine rewrites at each load.
- Next: proposal 0003 (S6.1.b), with O80 to O83 for the owner.
- Later (2026-11-15): the owner chose our own engine (O84); Min's DLL is not used.

### 96. SkyrimNet's own bio files confirm SN03 §5: ten prose parts, and the same parts empty for the player

`2026-11-15` · FACT · `#engine #roster` · refs: SN03, O46, O84

- The owner shared two of SkyrimNet's character bio files: one NPC's bio and the player's template, kept outside the
  repositories and never quoted. Each is ten named blocks of prose and nothing else, in the order and with the names
  SN03 §5.1 records from the older copies; the player's template is the same ten blocks, empty, for the player to fill.
  So SN03 §5's schema, its audiences and its render modes describe SkyrimNet's current format.
- The NPC's bio shows the spoiler question plainly: its background and aims state the whole of that character's story,
  which SkyrimNet trusts the model to hold back. EldenNet's rule (O36) gives an NPC only what is safe to hear now, so
  the shape is taken and the secrets are not ([proposal 0004](proposals/0004-character-bio.md)).
- The owner's direction (O46's Later note): ideas from it, a structure that fits Elden Ring, and possibly followers,
  for whom parts such as skills matter (O85).

### 97. SkyrimNet 1.2.0 read: our SN notes stand, with two corrections and a few new parts

`2026-11-15` · FACT · `#engine #roster` · refs: SN01 to SN08, WHY§87, WHY§96, O53, O84

- The owner shared SkyrimNet 1.2.0 and its dev tools, kept outside the repositories and read by two agents with Read,
  Grep and counting scripts only; no binary was run and nothing was copied. In everything readable it is the beta30-rc2
  behind SN01 to SN08 with a few changes, so those notes still describe it: the 1,507 bios are byte-identical, and the
  memory, decision, diary and bio-update logic, the configuration, the headers and the record roots are unchanged; 3 of
  137 prompt files are new and 16 changed.
- Corrections: a bio's prose is a median 512 words (p10 430, p90 606; unique 518, generic 494), not SN03 §5.1's 570,
  which counted the tags too; the plugin header holds 51 function pointers at API version 14, not WHY§87's 45.
- New: a pickpocket event whose audience depends on detection; combat sides from the game's groups, a stance for each
  person towards the speaker (enemy, ally, turned, bystander), stated as ground truth that outranks the history; a
  notice of unseen attackers; masks while the player is hidden; outfit descriptions. Speech: PocketTTS (an MIT C++ port)
  replaces Piper and Kokoro, with voice cloning and 27 voice packs; espeak-ng is gone.
- SkyrimNet's own spoiler discipline slips: about 3% of public summaries state the character's secret, 100 bios assume
  the player's deeds, and memories, diaries and bio updates are written with the full bio, so a secret can enter a
  memory and return for good. No evaluation checks spoilers.
- Licences: the plugin headers are MIT (Fateless.Ai); SkyrimNet itself carries no licence in either archive and is not
  open source. EldenNet takes its logic and structure under O53 and O84, copies and ships nothing from either archive,
  and takes libraries from their original publishers.

### 98. What SkyrimNet 1.2.0 shows our design lacks: thirteen points for our own engine

`2026-11-15` · LIKELY · `#engine #roster #process` · refs: WHY§97, O84, O46, ARCH§11, DESIGN§10.5

- **The client (ARCH §11).** (1) A 400 is retried without `extra_body`, which holds the reasoning switch, so a
  reasoning model may spend the whole budget thinking: keep the switch, retry at minimal effort, and record it about
  the model.
  (2) Decisions are reserved, not designed: our own format over chat completions (an enum schema, probabilities from
  logprobs where offered, options the engine builds, a rules fallback always, a `decisions` slot). (3) No backup
  request: start the fallback when a cloud reply's first token is late (about 2.5 s, EST), keep whichever streams
  first, and add a quantisation floor where a provider offers one.
- **The world.** (4) A stance towards each viewer, "turned" included, so a charmed foe or a summon is not just "not
  hostile" (to verify in game). (5) Unseen attackers, and a concealed Tarnished who is not greeted.
- **Plugins and the store.** (6) The code-plugin interface exposes rollback and promises ids never reused. (7) The C++
  store avoids SkyrimNet's traps: SQLite's own statement parsing, 64-bit ids, locale-proof numbers. (8) The template
  function reference and the lint's allowlist are generated from one registry.
- **Spoilers and memory.** (9) Memory, diary and lived-layer jobs get the same gated layers as dialogue. (10) Public
  fields, `stirs` and `ties` are checked mechanically against the NPC's locked terms, as well as reviewed. (11) Each
  witness writes its own first-person memory of a scene, as schema-bound data (place, emotion, importance, tags, type),
  with actor ids, stamped with the horizon it was written under; recall searches twice and then asks a gate.
- **Evaluation.** (12) Adopt SkyrimNet's 1-to-5 scale and pairwise comparisons by axis, run in both orders, and add
  what it lacks: a spoiler check, perception, voice and register, invented lore. (13) In v1, decisions only for the
  next speaker (silence always offered) and, later, the recall gate.
- Next: S8.1.f folds these into ARCH §11, MEM and the design; proposal 0004 takes (9) and (10) at once.
- Later (2026-11-15): O86 keeps spoilers by grounding alone, so (10) becomes a review when fields are written, and
  (12)'s spoiler check a few test questions per character on the model's own knowledge of the game.

### 99. The Skyrizz session's measurements of SkyrimNet: code gathers, models pick; eight points for our engine

`2026-11-16` · LIKELY · `#engine #process` · refs: WHY§97, WHY§98, O68, O84, O86

- Source: the Skyrizz session's reply to our questions (2026-11-16), from its bench runs on local models, captured
  SkyrimNet payloads and logs, and static reading; Skyrizz has not run in game, and none of it is verified here.
- **Recall.** Code gathering only the records a question needs reached 93 to 100%, against 0 to 25% when the model
  was given an index or told the joins. A recall gate picking the right memory among 4 to 12 near misses reached 83 to
  100%, best when the question named the setup, the matter and the outcome. SkyrimNet writes memories in a background
  cycle about every five minutes (importance 0.2 or more, one per participant and witness); linked identities make
  duplicates; diary entries are memory rows; recall keeps up to 12 above a similarity of 0.52.
- **Decisions.** Typed decisions on a small model never failed to parse (432 requests, 0.04 to 0.15 s) but were weak on
  subtle readings, and SkyrimNet's 0.70 confidence cut-off lost about a quarter of right answers. Chat on an
  OpenAI-compatible endpoint matched or beat them (0.10 to 0.32 s); its misses were format, fixed by readers that go by
  meaning and by plain yes/no questions. Thinking cost 2 to 35 s a step, with rare runaways near 50k tokens.
- **Templates.** Unresolved variables reached the model 12 times in the captures. Traps seen: `not` binding tighter
  than `==` and passing a null; JSON at column 0; `capitalize("")` crashing; stock Inja failing 55 of SkyrimNet's
  1,622 files, with `loop.last`, `loop.index0`, indexing and `in` silently wrong.
- **Records.** SkyrimNet renders the full profile in every full-mode prompt, with no allow-list. Instructions planted
  in records were obeyed 10 times in 10, and models copied ties from their own task lines.
- **Plugins and cost.** An event's audience bounds only characters' own history, not the GameMaster or the speaker
  picker; the interface passes C++ library types, and its threads and runtime linkage are undocumented. Outside
  scenes, about 6k input tokens a request and 506 requests an hour; decorator values may be up to 55 s stale.
- **For our engine** (with WHY§98, in S8.1.f): (1) code gathers the records a call needs, by id, and the model only
  picks or writes; (2) decisions over chat, read by meaning, with plain yes/no questions, thinking off by default and
  no hard confidence cut-off; (3) the template engine fails loudly on unknown names, keeps null and missing apart, has
  real booleans, a real substring test and built-ins safe on empty strings, and nothing unresolved reaches a model;
  (4) records enter prompts as quoted data, plugin content is validated, and examples use names no record holds (O86
  stands: no runtime output filter); (5) the lived layer changes only what cited memories justify; (6) the code-plugin
  interface (O68) has binding audiences, a C ABI, documented threads and reads by id; (7) the next speaker and its
  addressee are checked by code; (8) a budget of requests per hour, and a bound on how stale a cached value may be.

### 100. The engine's first increment: parts join exactly; two libraries fetched, not vendored; the template root

`2026-11-16` · METHOD · `#engine #process` · refs: O84, NPC§7.1, ARCH§11.1, ARCH§11.8, WHY§99

- Source: building roadmap S8.1.a (en-prompt, en-llm and the `eldennet-engine` command, on the engine branch) and its
  run against a mock server on 127.0.0.1.
- **Parts join exactly.** en-prompt keeps a message as parts that join with nothing added: a merged message's later
  part starts with its own blank line, and a part after a mid-message `[ cache ]` with its own line break. en-llm
  first put a blank line between parts and read only a message's last mark, so a breakpoint inside a merged message
  was lost and a part's own break would have doubled. Rejected: a separator added by the client, and moving the marks
  to message ends (that caches less, or caches changing text). Chosen: one rule in both modules; en-llm reads
  en-prompt's `parts` list and refuses one that does not join to its `content`, and a provider that takes breakpoints
  gets the parts as they are.
- **Two libraries fetched, not vendored.** The sweep flags the contact details in third-party headers (seven authors'
  e-mails in nlohmann/json and toml++). Workaround: those two are downloaded at configure time from their own
  projects, pinned by SHA-256, into the build tree; inja and doctest stay vendored with their licences. A sweep run
  in a pipe reports the last command's status, so a commit tests the sweep's own exit status (`set -o pipefail`).
- **The template root.** A template name with `.`, `..` or an empty folder name, and a link that leads out of the
  prompts folder, are refused, for `render-prompt` and for every include.
- **Measured:** 126 unit tests pass (en-prompt 62, en-llm 64). The mock received both breakpoints as marked parts and
  the key only in its header; the decision call sent an enum schema, temperature 0 and the top 5 logprobs. With the
  server stopped, the decision took its rule's pick and the chat command failed with no address in its message. The
  MinGW-w64 build compiles with no warnings.

### 101. The engine's second increment: WinHTTP by its full path; the handle's and the probes' choices

`2026-11-16` · DECIDED · `#engine #process` · refs: O84, ARCH§1.3, ARCH§4.6, ARCH§4.10, ARCH§11.1, WHY§100

- Source: building roadmap S8.1.g in three parts, each in its own worktree, merged on the engine branch.
- **WinHTTP by its full path.** ARCH §4.10 lists `winhttp.dll` proxies that sit beside the game (EROverlay, the EAC
  Toggler), and §4.6 already loads our own DLL by its absolute path. A DLL imported by name binds to a loaded module of
  that name, or to the game folder's copy, before the system's, so every request and the key's header would pass through
  the proxy. The transport loads `<system directory>\winhttp.dll` by its full path, resolves its functions with
  `GetProcAddress`, keeps it loaded, and the build fails if the tests' import table names `winhttp.dll`. Any later
  system DLL that sees a secret is loaded the same way (impl).
- **The handle** (ARCH §1.3's C9, C10): requests start one at a time in priority order, so a provider's last free
  connection goes to the request that comes first; a Busy request goes back to its place, and a live one ends Busy
  once its slot's first-token time has passed; an Ended event never waits for room, so the event queue may pass its
  bound by one per live request and shutdown never blocks; a `bark_pool` job evicted from a full queue ends Dropped (a
  new class), a `memory` job Preempted, to be requeued; a successful warm-up counts as serving for the 4-minute rule,
  so each approach does not send another. Rejected: a thread-local hand-off of the attempt observer into decision
  calls; one private `run_chat` path serves `chat` and `decide`.
- **The probes** (ARCH §11.4): an empty model list counts, since a server that is up with no model loaded is the
  commonest local mistake and the wizard can say so only if it sees the server; Ollama is confirmed by a `version`
  string, llama.cpp by three of its props fields, and a server that answers as both is taken for neither.

### 102. Wine 9.0 runs the Windows build, and its WinHTTP differs from Windows' in three ways

`2026-11-16` · FACT · `#engine #process` · refs: ARCH§11.1, WHY§101

- Source: the engine's Windows build (MinGW-w64, GCC 13 with the win32 thread model, the runtime linked statically)
  run under Wine 9.0 from the distribution's packages, and Wine's own source for WinHTTP.
- **What it gives.** The Windows tests and commands run in the container, so a Windows-only path is tested, not just
  compiled: 127 client tests in each of three runs, en-prompt's 62, and every command end to end. Proton is Wine, so
  the Steam Deck meets the same WinHTTP.
- **The three differences:** a chunked reply arrives one chunk late (a read that ends a chunk completes only when the
  next one begins); closing a request does not stop a receive already waiting, so a cancelled exchange's connection
  stays open until the server sends or closes; the automatic proxy mode means no proxy, and an IPv6 address in
  brackets does not resolve. `WinHttpReadData` can also block its caller, and a closed handle's value is reused at
  once, which is why each exchange has a driver thread that alone calls WinHTTP on its handle.
- **What Wine cannot show** waits for Windows itself (roadmap S8.1.h): the system's proxy, TLS with real
  certificates, IPv6 loopback and cancelling on Windows.

### 103. The design on C++: one mapping, the behaviour unchanged, and what the re-base found

`2026-11-16` · METHOD · `#engine #graceui #process` · refs: O60, O84, O25, G15, ARCH§1.3, ARCH§2.3, ARCH§2.4, WHY§102

- Source: roadmap S8.1.e. Five agents re-based DESIGN, ARCH, NPC, MEM, game data and GraceUI's design from one mapping
  of Rust terms to C++ ones, changing how things are built and never what they do; the main session reconciled them.
- **The mapping:** crates became CMake module targets of the same names; `serde` nlohmann/json; `toml` toml++; `tokio`
  and `reqwest` en-llm's own threads over libcurl and WinHTTP; `crossbeam` bounded queues of our own, a waiting thread
  woken by one signal; `Result` a small `Expected`; `unsafe` the rule that raw game addresses stay in `en-gal`'s reader
  and `sys` files (a CI grep and `clang-tidy`); panics no exception across a foreign entry (`ffi_guard`); `clippy`
  `clang-tidy`; `miri` and `cargo fuzz` the sanitizers and libFuzzer; `cargo deny` `THIRD_PARTY.md` and an allowlist
  check, with the banned names kept.
- **Found on the way:** `std::atomic<std::shared_ptr>` is not lock-free in MSVC's library or libstdc++ (both take a
  lock), and its reader could drop the last copy, so the core-to-overlay hand-off (C6) became a triple buffer the core
  owns; GCC, and so the MinGW build, has no `__try`, so that build leaves out `en-gal`'s reader and `sys` and the DLL,
  which only clang-cl builds; Dear ImGui's Win32 backend polls XInput every frame unless built with
  `IMGUI_IMPL_WIN32_DISABLE_GAMEPAD`, and its DX12 backend uploads textures when the font atlas grows (read in 1.92.6,
  to confirm in 1.93.0), so GraceUI turns the gamepad code off and bakes the atlas once.
- **The built engine against the re-based rules** (roadmap S8.1.j): `en-prompt` reads a folder while `en-core` must do
  no I/O; `LlmRequest` and `LlmEvent` live in `en-llm`, not `en-proto`; the transports' C callbacks have no
  `ffi_guard`; some local statics have destructors that run at exit; the SSE caps and the net threads' stacks differ
  from ARCH's.
- **Left as history:** the design process records (P1 to P3, J1 to J3, the review), the research notes and the
  proposals keep their Rust wording; decisions changed only by dated notes.

### 104. GraceUI's server is our own: an independent review's reasons, and its correction of the connection cap

`2026-11-16` · DECIDED · `#graceui` · refs: G30, WHY§103

- Source: GraceUI's research note SRV (`docs/research/server-basis.md` there), an independent review the owner asked
  for; the owner decided G30 the same day, as recommended.
- **Why our own:** every rule of GraceUI's §7.2 is our code whatever the basis; a library takes over only the head
  parser and the WebSocket framing, and brings features that must stay off, which is where 2025-26's bugs were (chunked
  bodies, form parsing, URI decoding, HTTP/2, compression, file serving). civetweb's 2025 fixes sit on an unreleased
  master; cpp-httplib runs a thread per WebSocket; Crow's 2026 advisories include an authentication bypass on the
  upgrade; even Boost.Beast, the runner-up (two outside audits, continuous fuzzing), fixed frame-length and
  `Content-Length` checks in 2026.
- **The correction:** "two connections at once" holds for WebSockets only; browsers open several HTTP connections and
  Chrome opens idle ones ahead of need, so plain HTTP gets its own cap and an absolute 5 s deadline for a head (GraceUI
  D111).
- **What it asks of our code:** parsers as pure functions, fuzzed on Linux with a chunking differential;
  `Connection: close`; static paths matched byte for byte; RFC 9112's and RFC 6455's strict rules;
  `SO_EXCLUSIVEADDRUSE`; one address shown, `127.0.0.1`, with a redirect for the other two loopback names (GraceUI
  D112, its §7.2 items 19 to 25 and §7.6).

### 105. The gear's names are in WeaponName (11) and ProtectorName (12), by row id; no row needs the expansion's layers

`2026-11-16` · FACT · `#gamedata #datapiler` · refs: DESIGN§4.5, DATA§3.1, ER01§8.3, WHY§104

- Source: DataPiler 0.2.40's jobs en-0041 to en-0043 on the owner's install (2.7.2.0, English text, Smithbox
  2.2.6), counts only (`docs/research/eldenring/leads/gear-names.json`).
- **Weapons:** 3,636 `EquipParamWeapon` rows; 2,909 (80.0%) find a name by their row id, every one of them in the base
  table 11. **Armour:** 838 `EquipParamProtector` rows; 641 (76.4%) find one, all in 12. The rest have none, so a
  piece without a name is left out of block 9, as a missing name is anywhere.
- **The expansion's layers name no row.** The pattern from the character and place names (310 and 410 above the base
  table) gives 321 and 421, 322 and 422, but every named row is already in 11 or 12. Those layers hold names no row
  uses (weapons: 489 only in 321, 1 only in 421; armour: 22 and 1), as the base tables do (310 and 2); what they are is
  not known, since names never leave the PC. The character names differ: 50 of 213 were only in their expansion layer
  (en-0005).
- So ER01's lead for the base tables holds, and DESIGN §4.5 lists them; S2 still checks them in game (gap 3).

### 106. The built engine on the re-based rules: what Clang's static-object errors, R3 and the caps took

`2026-11-16` · METHOD · `#engine` · refs: ARCH§3.1, ARCH§1.7, ARCH§10.1, ARCH§10.2, ARCH§11.8, DESIGN§2.6, WHY§103

- **Static objects.** With `-Werror=global-constructors` and `-Werror=exit-time-destructors` on every module the DLL
  links (`en_dll_module`), Clang found six: two in en-prompt and four in en-llm. Constants became `constexpr` arrays
  behind spans; the long-lived ones (the system clock, the no-proxy list) are made once with `new` and never destroyed;
  and the per-thread render stack became a linked list of frames on the stack under one `thread_local` pointer, since a
  `new` per thread would leak an object for every thread. The rule against dynamic initialisers is ARCH §3.1 (the brief
  and the CMake helper had cited §1.6, the mutex: corrected).
- **A decision call's rules pick is data.** `DecisionCall::rule` is the key the task's rules picked from the same state,
  computed by en-core when it builds the call. The rules are a pure function of that state, so nothing is lost: no
  worker runs the caller's code, the call has a JSON form, and en-llm refuses a key the call does not offer before
  anything is sent.
- **The JSON caps fit what is parsed whole, not model lists.** 256 KB and 32 deep cover stream events, bodies, error
  bodies, decision answers and the probe's checks. A provider's model list can be larger (OpenRouter's very likely is),
  so lists, read as they parse and keeping only their ids, keep their own bounds (1 MB from a local probe, 8 MB from a
  provider) with the same depth check (ARCH §10.2's row says so).
- **R3 on the transports, tested.** A transport callback never throws in a test by itself, so the test program alone
  replaces `operator new` with one that, once armed, fails one allocation of a chosen size on any thread but the test
  servers'. libcurl and WinHTTP allocate with their own allocators and never meet it; the fault lands in our code inside
  their callbacks, which trip `LlmTransport` and fail the exchange at once.
- **2 MB stacks.** `std::thread` cannot set a stack size: `_beginthreadex` with `STACK_SIZE_PARAM_IS_A_RESERVATION` on
  Windows and `pthread_attr_setstacksize` elsewhere (`StackThread`). The size reads back through
  `GetCurrentThreadStackLimits`, which Wine reports, and glibc's `pthread_getattr_np`.
- **Running the suite many times found two test defects.** Two runs at once shared one temporary folder, since an
  unseeded `std::rand()` gives every process the same name: fixed. The handle's give-way test failed twice in about
  twenty runs of the suite under heavy load, and in none of about 1,000 since (the test alone and the whole suite, in
  parallel and in sequence); it now reports how the call-out ended and the order of the events, and roadmap S8.1.l keeps
  it open.
- Counts: GCC and Clang, en_proto 15, en_llm 138 and en_prompt 68 test cases; MinGW under Wine, 15, 140 and 68; no
  warnings.

### 107. EldenNet's pages moved to PAGES: where the fold cut GraceUI's §4, and what G20 left open

`2026-11-16` · METHOD · `#graceui #design` · refs: G18, G19, G20, G29, GraceUI D82, D92, PAGES§1, PAGES§2, PAGES§4.5;
S7.8.d, S7.12.e

- **The cut.** By subsection, never inside one, but for §4.9, which G20 splits itself: what is about EldenNet's content
  moved to PAGES under GraceUI's numbers (§4.1, §4.2.1, §4.4 to §4.6, §4.8, PV1, PV2, PV4, PV5 and PV8, §4.10 to
  §4.12; §9.1's spoiler goldens row, §9.2's fixtures; D61). The frame (§4.2.2 to §4.2.5), search (§4.3) and the
  settings row (§4.7) stayed, as D82 and proposal 0001 §4.10 name them, though they still use EldenNet's space as
  their example. Every heading and rule id that left stays in GraceUI with a one-line pointer, and the parts GraceUI
  kept stand in PAGES as pointers under the same numbers, so either document resolves a reference by number.
- **The map stayed in GraceUI §4.13.** It is GraceUI's own proposal 0002 under its G21 to G27, with D89 to D98 in its
  log and the framework's map panel kind (D91) at its centre; D92 tied only the Map page's repository to G20, and that
  page's rail entry and settings rows moved with PAGES §4.2.1 and §4.5. If the owner wants all of EldenNet's spoiler
  review in EldenNet, the map's content (its layers, lore notes and goldens) can follow in a fold of its own.
- **Left open by G20 (FACT).** GraceUI's §4.5 deferred to G20 "how GraceUI's own space (D79: Menu, Hotkeys, Browser
  view, …) meets EldenNet's Settings groups", and (a) does not say. GraceUI's own keys (`hotkeys.menu`, `ui.scale`,
  `ui.menu.*`, `dashboard.*`; D84) are homed on EldenNet's pages (Settings › Menu, Browser view and Hotkeys; Voice ›
  Text and subtitles; Home › Customise), while GraceUI §6.9 describes GraceUI's settings in its own space and D81 keeps
  commands within their space. The homes stay as listed until a design pass or the owner settles it.
- **Not moved: the protocol schema.** GraceUI §6.5 and §6.6 still list EldenNet's topics and commands, generated in
  `grace-protocol`; G20 (a) names the pages and their rules, not the schema, so GraceUI §6.1 now says they stay there.
- **G29's EldenNet side** is ARCH §4.13 and DESIGN §3.2: the launcher links `grace-serve`, `eldennet.dll` never does;
  the rest of the off-game dashboard waits for O76's fold (S7.12.c).
- **A stale phrase (FACT).** GraceUI's PV3 says "EldenNet keeps only its hash" of a paired device's cookie, but since
  D84 the paired devices are in GraceUI's user folder. PV3 is kept as worded; a later pass should correct it.

*Later (2026-11-16):* both open points settled by GraceUI's D115 (roadmap S7.8.e): GraceUI's own settings live on
GraceUI's own pages, linked from EldenNet's; `ui.menu.home_cards` and `ui.menu.show_world` became EldenNet's
`ui.home_cards` and `ui.show_world`; PV3 now says GraceUI keeps the paired device's hash, in its user folder.

### 108. The engine on real Windows, run 1: WinHTTP works over the LAN and over TLS; two transport tests fail where Wine passes

`2026-11-16` · FACT · `#engine` · refs: ARCH§11.1, ARCH§11.2, WHY§101, WHY§106

- Source: the owner's run of the Windows check (roadmap S8.1.h), the MinGW Release build of the engine branch, on
  2026-11-16; its reports were scrubbed of the user's and the PC's names and the servers' addresses before they left the
  PC.
- **Works.** The three test programs: en_proto 15 of 15, en_prompt 68 of 68, en_llm 138 of 140. A llama-server on the
  home network, through WinHTTP: its model listed, a reply streamed (first text 378 ms, all of it 862 ms) and a decision
  picked by the model, with its probability, in 85 ms. A cloud provider's model list (623 ids) over HTTPS with real
  certificates. `probe` found nothing on the PC's usual ports, which was right for that PC.
- **A cloud key rejected.** NanoGPT answered 401: it takes the key as a Bearer token, which the engine sends, so the key
  itself was refused. The provider was then off until its configuration changes, and the decision fell back to its
  rules, both as designed (ARCH §11.2).
- **Two tests fail on Windows and pass under Wine.** (1) A connect to a listener with a full backlog lasts about 2 s,
  since Windows retries the connection before it gives up, and WinHTTP's own connect timeout (300 ms in the test) does
  not cut it short; it ends as "could not connect", not as a timeout. (2) TLS against a server that answers in plain
  HTTP is reported by real Windows as a broken connection, not a TLS failure, so the message names no TLS. The fix
  (roadmap S8.1.h): the driver's own connect deadline, and a TLS failure told by how far the exchange got.
- Not yet tried on Windows: cancelling a reply part-way (Ctrl+C).

*Later (2026-11-16, run 2):* with a valid NanoGPT key the cloud reply went through, and the backup request (ARCH §11.1)
worked against a real provider: DeepSeek V3.2's first text came at 2,854 ms, the backup on V4.1 Flash had started at 2.5
s, lost the race and was cancelled, and NanoGPT's own log shows both cancelled requests as cancelled, billed only for
what they made (2 tokens and none). The cloud decision missed its 1.5 s first-token deadline and the rules picked
(`FirstTokenTimeout`), while the home network's server decided in 82 ms; so O88's presets measure their decisions model
before they ship.

### 109. WinHTTP's connect deadline and TLS wording: what Wine shows, and the rules that do not rest on it

`2026-11-16` · METHOD · `#engine` · refs: ARCH§11.1, WHY§102, WHY§108

- **What Wine 9.0 does** (FACT, checked by experiment while fixing run 1's findings): it reports `CONNECTED_TO_SERVER`
  only after the TLS handshake, and a handshake with a server that answers in plain HTTP fails as
  `ERROR_WINHTTP_SECURE_CHANNEL_ERROR`; real Windows named the same case as a broken connection (WHY§108). With
  WinHTTP's own connect timeout raised to 10 s, the full-backlog case under Wine still ended at 301 ms once the driver
  kept the deadline.
- **The connect deadline is ours.** A request that has not begun to go out (`SENDING_REQUEST`) by its connect timeout is
  closed as `cancel()` closes it and fails as "timed out connecting"; WinHTTP's own timeout stays as a backstop.
  Resolving the name, connecting and the TLS handshake all count, and so, on a PC that detects a proxy automatically,
  does that detection: run 3 shows whether a cloud request's 3 s fits it.
- **TLS by phase, not by code.** For `https`, a failure before the request goes out is a TLS failure whatever its code
  (`CONNECTION_ERROR`, `INVALID_SERVER_RESPONSE`, the secure codes), worded by the secure flags when a `SECURE_FAILURE`
  gave them, and a handshake that runs out of time reads "TLS failed: the handshake timed out". So the rule holds
  whether a system reports the connection before the handshake or after it.
- **Tested everywhere.** The wording is a pure function (`winhttp_failure`), built on every system with WinHTTP's codes
  written out and checked against `winhttp.h` on Windows builds; the two transport tests print what they saw when they
  fail. GCC and Clang en_llm 143 cases, Wine 145 (roadmap S8.1.m).

### 110. O88 folded: a preset's providers are matched, not defined; the reserved slots; what "same as dialogue" shows

`2026-11-16` · DECIDED (folding O88) · `#engine #design` · refs: O88, ARCH§11.1, ARCH§11.4, ARCH§11.7, DESIGN§11.5,
PAGES§4.5, WHY§108

- **A preset's providers are expectations, not endpoints.** In either form a preset's provider says what it expects (a
  base URL, else a kind and flavour) and is matched to an entry of `providers.toml`, as the one-provider form always
  was. The key and every setting of the endpoint (the reasoning switch, concurrency, JSON mode, the floor) stay with
  that entry, which the player owns and AI › Providers edits, so the wizard writes the entry beside the preset.
  Rejected: presets that define endpoints of their own, with keys found by the preset's names; they would clash with
  the player's entry of the same id, keep endpoints off AI › Providers and put reasoning switches in shipped content.
- **One name, one provider.** TOML refuses a name given twice in a preset; a preset's name that is another entry's id
  in `providers.toml` is refused too, since it would stand for two. A preset with neither form is now refused, where it
  used to take the only provider there was.
- **The reserved slots.** `actions` gets a decision call's tokens and temperature (100, 0), since an act is to be picked
  as a decision is (DESIGN §13), and no act on failure, as R2 fails closed; its times and retries, and all of
  `vision`'s budgets, are set with their features. Until then a request naming either falls through to `dialogue`, and
  a preset's `all` does not reach them.
- **"Same as dialogue"** (PAGES §4.5). O88's Settings › AI is the AI page's Models. A slot with no model of its own,
  from the preset or the player, follows Conversation, as the engine's borrowing does, and its row says so until it is
  changed (Decisions follows Quick lines); a preset's own picks show as picks, since they do not follow a change to
  Conversation. A `[slots]` entry without a provider takes the one the preset gives that slot.
- **A key is read, never tried.** The wizard knows OpenRouter's keys by `sk-or-` and asks about any other; trying a key
  on each provider to see which accepts it would send it where it does not belong.
- **Measured before shipping.** The 1.5 s first-token rule binds the presets that name their models (the cloud ones),
  before they ship and whenever a model changes; a local preset's models are the player's, and the self-test measures
  them. The engine reads both forms and the reserved slots (roadmap S8.1.n): GCC and Clang en_llm 155 cases, Wine 157;
  NanoGPT's models wait on run 3 (S8.1.h).

### 111. The clang-cl build with xwin, and the SEH copy: what clang does to `__try`, guard pages, and the pinned CRT

`2026-11-16` · FACT · `#engine #gal #build` · refs: ARCH§2.6, ARCH§6.5, DESIGN§2.6, O60

- **The SEH sketch was unsafe under clang-cl** (ARCH §6.5, corrected): a `memcpy` written inside the `__try` becomes the
  `llvm.memcpy` intrinsic, which no scope-table entry covers. Probed under Wine: at `/Od` the fault went unhandled; at
  `/O2` clang deleted the `__except` and the copy always answered success. A never-inlined callee doing the `memcpy` is
  covered (the tests pass at `/Od` and at `/O2` with ThinLTO; the linked function has one call in its guarded range,
  under `__C_specific_handler`).
- **Guard pages.** The sketch's filter (access violation, in-page error) let `STATUS_GUARD_PAGE_VIOLATION` (0x80000001)
  through, which would end the game; the filter takes it too. Reading a guard page lifts its guard (tested under Wine).
  LIKELY: reading another thread's stack guard takes that thread's stack growth away, so `GuardedReader` is never
  pointed into a stack (spike S0-D).
- **The CRT is 14.43.** MSVC's standard library 14.44 (VS 17.14) refuses a Clang older than 19 (`yvals_core.h`,
  STL1000); 14.43 (MSVC 14.43.34808) asks for Clang 18. With LLVM pinned at 18 the lock takes CRT 14.43.17.13 and SDK
  10.0.26100 from manifest 17.14.45, and the static CRT (`/MT`) in every configuration.
- **Two diagnostics in the whole engine at `/W4 /WX`:** MSVC's CRT marks `getenv` deprecated, so en-llm's `system_env`
  uses `_dupenv_s` there; and Wine 9.0's `CreateSymbolicLinkW` answers success and makes nothing (MinGW's library never
  calls it), so en-prompt's link tests check that a link exists before judging it.
- **Getting xwin** (METHOD): the session's network refuses crates.io's download host, so xwin 0.10.0 came from its own
  GitHub release archive for x86_64-unknown-linux-musl, checked against the release's SHA-256. Microsoft's licence
  covers the CRT and SDK; they stay in a cache outside every repository (O60).

### 112. The sanitizers and the first fuzz targets on Linux: two test defects, no finding in our code

`2026-11-16` · METHOD · `#engine` · refs: ARCH§12.1, ARCH§10.2, WHY§106

- **AddressSanitizer found one defect, in a test:** the R3 test of the net threads' 2 MB stacks registered a notify
  callback capturing locals declared after the handle, which the dispatcher could call until the handle shut down
  (stack-use-after-scope); the state now comes first.
- **ThreadSanitizer could not link `en_llm_tests`:** its static runtime defines every form of `operator new` and
  `delete` strongly, so a program replacing them clashes; the thread build links the shared runtime, and the tests
  replace all twenty forms, not six, so no memory passes between their allocator and a runtime's.
- **No finding in our code or a library's:** the three fuzz targets (SSE frames, the JSON caps, the template compile)
  took about 3 million inputs in two runs each, and a chunking bug planted in the SSE parser was caught on the seeds at
  once. nlohmann/json 3.11.3 refuses a number past a double's range (`1e400`), so no infinity reaches the JSON forms.
- **The give-way test (S8.1.l):** ThreadSanitizer saw no race in 38 runs of the suite (24 of them four at once), 150 of
  the test alone under load and 600 without the sanitizer; what failed twice is not a race it sees, and S8.1.l stays
  open.

### 113. Direct3D 12 runs under Wine in the cloud container (vkd3d on Mesa's software Vulkan, on Xvfb); the toolchain saved as a setup script

`2026-11-16` · FACT · `#graceui` `#build` `#process` · refs: GraceUI:D85, GraceUI:DESIGN§2.2, WHY§111

- **Direct3D 12 without a GPU** (FACT): a probe of our own, built with clang-cl against the xwin SDK, ran under Wine 9.0
  on Xvfb with Mesa's software Vulkan (lavapipe, Ubuntu's `mesa-vulkan-drivers`). It created a device, a direct queue
  and a flip-model swap chain, cleared the back buffer, presented, and read it back. Five frames gave the exact colour
  cleared. Wine's `d3d12.dll` is vkd3d over that Vulkan driver. So GraceUI's `tools/grace-harness` can run the host's
  Present hook and Dear ImGui's DX12 backend on Linux, and check the pixels. That answers D85's open question ("WARP in
  CI: LIKELY") another way, on Linux. Wine reports an invented adapter name, which no file records.
- **The toolchain as a setup script** (METHOD): the packages and the xwin splat that WHY§111 describes are saved as a
  cloud environment's setup script, run end to end here twice before saving. It installs LLVM 18, MinGW-w64, Wine,
  Xvfb and Mesa's Vulkan driver, compiler-rt, libcurl and 7-Zip, then xwin 0.10.0 checked by SHA-256 and the pinned CRT
  and SDK. It takes about two minutes, and its splat matched the earlier one file for file. A step that fails says so,
  and the session still starts.

### 114. A Direct3D 12 swap chain's GetDevice names the device under Wine and vkd3d-proton, not the queue; the present queue found four ways

`2026-11-16` · FACT · `#graceui` `#overlay` · refs: GraceUI:D116, GraceUI:D102, GraceUI:DESIGN§2.9

- **GetDevice answers with the device** (FACT): a probe of ours under Wine 9.0 created a swap chain on the second of two
  direct queues; `IDXGISwapChain::GetDevice` answered `E_NOINTERFACE` for `ID3D12CommandQueue` and the device for
  `ID3D12Device` and `IUnknown`. Wine 9.0's `d3d12_swapchain_GetDevice` queries the device, and so does vkd3d-proton's
  `dxgi_vk_swap_chain_GetDevice`, to which DXVK's DXGI swap chain forwards (their source, read). Microsoft's page for
  `IDXGIDeviceSubObject::GetDevice` says nothing of Direct3D 12, so what Windows answers is not known here (S4).
- **So the present queue is found four ways, in order** (GraceUI D116): the swap chain's own answer, where a runtime
  gives the queue; the queue the game names in `ResizeBuffers1`; a direct queue executed inside the original `Present`
  (OBS's capture does this; neither Wine nor vkd3d-proton executes one there: Wine signals a fence, vkd3d-proton enqueues
  internally); and the device's only direct queue seen executing, after a whole frame. Two direct queues and no other
  answer: nothing is drawn until the game names one. The harness checks the last two ways under Wine (the only-queue way
  draws; with two queues nothing is drawn until `ResizeBuffers1`).
- **Rejected:** the first queue seen executing (an overlay habit that races the present when a game has two direct
  queues), an offset into the runtime's swap chain object, and hooking `CreateSwapChain*` (not in D102's list, and
  GraceUI may start after the swap chain exists).

### 115. Dear ImGui 1.93's DX12 backend waits for the GPU on texture uploads; its static atlas mode avoids every upload during play

`2026-11-16` · FACT · `#graceui` `#overlay` · refs: GraceUI:D117, GraceUI:DESIGN§3.16, GraceUI:CLAUDE.md invariants 10, 11

- **The wait** (FACT, its source read): `ImGui_ImplDX12_UpdateTexture` copies through an upload buffer, executes on
  `InitInfo.CommandQueue` and then `WaitForSingleObject(..., INFINITE)` on its fence; `ImGui_ImplDX12_RenderDrawData`
  calls it for every texture in `ImDrawData::Textures` whose status is not OK. With the dynamic atlas (1.92 onwards) a
  new glyph or size can ask for one during play. The backend's header names the way out: `Textures = nullptr`, and the
  application handles textures itself.
- **The static mode** (FACT): with `ImGuiBackendFlags_RendererHasTextures` cleared after the backend's `Init`,
  `ImFontAtlasBuildMain` preloads every glyph range at the font's size, and Dear ImGui sets `ImFontAtlas::Locked` from
  `NewFrame` to `EndFrame`, so no glyph or size is added during a frame (the nearest bake is scaled). GraceUI uploads the
  RGBA atlas itself once, on the present queue, polls its fence, and sets the texture's id and status. With
  `IMGUI_DISABLE_OBSOLETE_FUNCTIONS` the build uses the internal `ImFontAtlasBuildMain` (the obsolete `Build` calls it),
  and leaves out the CC BY 4.0 table of Japanese characters that `GetGlyphRangesJapanese` holds.
- **Also seen:** the backend's `CommandQueue` is used only by that texture path; its frame index advances once per
  `RenderDrawData` (not when the display size is zero), which is why GraceUI's frame slots follow its calls (D119).

### 116. Wine's wined3d on OpenGL hung on Xvfb when a second thread made a DXGI factory; its Vulkan renderer did not

`2026-11-16` · FACT · `#graceui` `#build` `#wine` · refs: GraceUI:D120, GraceUI:BUILD.md, WHY§113

- **The hang** (FACT): in at least two of four harness runs the host's init thread never returned from
  `CreateDXGIFactory1`. Wine's factory starts wined3d, whose default renderer is OpenGL; a backtrace showed the thread
  in `wglCreateContextAttribsARB` (wined3d's adapter probe), while the harness's main thread was in `PeekMessage`.
- **The way round** (FACT): `HKCU\Software\Wine\Direct3D`, `renderer = vulkan` in the prefix; none of seventeen runs
  since has hung. Direct3D 12 (vkd3d) runs on Vulkan either way; Proton's DXGI (DXVK) does not use wined3d.
- **For players** (LIKELY): a game run on plain Wine (not Proton) with wined3d on OpenGL could meet the same hang in
  GraceUI's init thread, which would then never hook: the game would run, without the overlay. S4 is on Proton and
  Windows; the guide can name the setting if a player reports it.
- **Method:** `printf 'bt all\ndetach\nquit\n' | WINEPREFIX=... wine64 winedbg <pid>` prints every thread's backtrace of
  a running Wine process (`winedbg --command "info proc"` lists the pids).

### 117. The render half's cost under Wine on software Vulkan, and how it was measured

`2026-11-16` · METHOD · `#graceui` `#overlay` `#perf` · refs: GraceUI:D119, GraceUI:BUILD.md, GraceUI:CLAUDE.md
invariant 10, EldenNet ARCH (T3)

- **What is measured** (METHOD): a frame's cost is the Present detour's time less the original `Present`'s; within it,
  the time inside the runtime's `ExecuteCommandLists` and `Signal` on GraceUI's own submission is counted apart, since
  Mesa's lavapipe can run the "GPU" work on the submitting thread. The rest is GraceUI's own work. Allocations and frees
  are counted on the present thread inside the detours, in graceui.dll's own `operator new`/`delete` and Dear ImGui's
  allocator, in a test build only.
- **Figures** (FACT; clang-cl Release, eleven runs of 600 frames each, the test panel drawn on 480×270 frames): with
  the GPU idle before each frame, GraceUI's own work median 98–123 µs, p99 0.37–0.69 ms; the whole detour median
  116–143 µs, p99 0.61–1.44 ms, the submission calls' p99 being 0.14–1.36 ms of it. Run as a game runs (two frames in
  flight), the submission calls reached 2.5–4 ms at p99 and the own-work p99 crossed 1 ms in some runs, as lavapipe's
  threads shared the four cores. Nothing to draw: median 0.5–0.7 µs, p99 1.3–20 µs. The MinGW-w64 build matched (own
  work median 106 µs, p99 0.57 ms). Allocations and frees after warm-up: 0 and 0 in every run. No frame was skipped
  for a busy slot.
- **Instrumented once** (FACT): the slow frames' time was in `ExecuteCommandLists` (0.8–2.9 ms) and occasionally `Signal`
  (1.4–1.7 ms); Dear ImGui's frame took 6–40 µs, the allocator and list reset 30–240 µs, `RenderDrawData` 14–200 µs.
- **So** the figures are upper bounds for a real GPU; S4 measures the cost in the game against the 1 ms p99 budget for
  panels with the menu closed.

### 118. The engine's release binaries carry the build machine's absolute PDB path; /pdbaltpath removes it

`2026-11-16` · FACT · `#engine` `#build` `#privacy` · refs: ARCH§2.6, WHY§111, GraceUI:D123

- **Found** (FACT): `llvm-readobj --coff-debug-directory` on the engine's clang-cl release test program shows
  `PDBFileName` as the absolute path of the build tree (under the home folder). `-ffile-prefix-map` maps paths inside
  debug information, not the PDB path lld-link writes into the binary.
- **The fix** (FACT): `/pdbaltpath:%_PDB%` at link makes it the PDB's file name alone; GraceUI's toolchain file has it,
  and its graceui.dll names `graceui.pdb`. The engine's `cmake/clang-cl-xwin.cmake` should take the same flag before a
  binary ships (a one-line change, left to the engine's session).

### 119. Dear ImGui's and MinHook's licences read for adoption; Dear ImGui's contribution rule on AI prose is not a licence term

`2026-11-16` · FACT · `#graceui` `#licence` · refs: GraceUI:D107, GraceUI:NOTICE, EldenNet O25

- **Read in full at the pinned commits** (FACT): Dear ImGui 1.93.0, MIT (Copyright (c) 2014-2026 Omar Cornut);
  MinHook 1.3.4, BSD 2-Clause (Tsuda Kageyu), with the Hacker Disassembler Engine 64 under BSD 2-Clause (Vyacheslav
  Patkov). No clause on AI use in either. Dear ImGui's sources also carry stb (MIT or public domain) and two embedded
  fonts, ProggyClean and ProggyForever (MIT); NOTICE reproduces every text a binary distribution needs.
- **The one mention of AI** (FACT): Dear ImGui's pull-request template asks contributors not to submit AI-generated
  prose. That governs contributions to that project, not use of its code; GraceUI contributes nothing upstream.
- **Contact details** (METHOD): MinHook's `AUTHORS.txt` holds e-mail addresses, so it is neither fetched nor copied; the
  licence files fetched hold none, and NOTICE names authors without contact details.

### 120. The GAL's types in en-proto: what the design left open, and what C++ took

`2026-11-16` · METHOD · `#engine` · refs: ARCH§2.2, ARCH§6.4, ARCH§7.1, DESIGN§2.3, O89

- **ResolveParams names its param.** DESIGN §2.3's sketch has only ids, but the core asks for NpcParam rows and for the
  grace and place-name rows (ARCH §9's Ready export), whose ids overlap; without the param the task cannot answer. How
  the Ready export pages through every row is still open (part 4 or 5).
- **RegionKey is a key of regions.toml** (`FixedString<32>`, lower-case letters, digits, `_`), which the task leaves
  empty: the game knows no regions, and the core maps the block and place to a key in its own copy of the snapshot.
- **The enums are our own words**, each with `Unknown` (not read): MultiplayRole Host, Cooperator, Invader, Other;
  LobbyState None, Hosting, Joined, Other; Screen InGame, Loading, Title; HudState as ARCH §6.3 names them; BodyType
  A, B. The layout's enum tables map the game's raw values to them, so no game value is in the code.
- **ReadHealth** is, per group, Untested, Ok or Tripped (a failed self-test trips the group), whether phase B passed in
  this playthrough (ARCH §9's Title → Playthrough needs it), and the faults this tick; with the task's last and p99
  times. **Capabilities** is the set of groups discovery resolved and phase A passed.
- **A nested struct's default member initialisers are not usable until the enclosing class is complete**, so a
  `std::variant` member whose first alternative has them is not default-constructible inside it (GCC and Clang alike):
  GameCommand puts `Noop` first (a default command does nothing); Lifecycle's `DataReady` has none of its own.
- **The snapshot is 12.9 KB**, not ARCH §7.1's "about 8 KB": the 16 text slots of 256 bytes and the 32 param rows of 8
  fields are most of it. Trivially copyable, so C1's copy is one `memcpy`.
- **JSON forms:** a float that is not finite is `null` and reads back as NaN (a `bearing_deg` without the camera);
  flag frames are hex strings with trailing zero bytes left out; ReadHealth's groups an object keyed by name.
- **Expected** is our own `std::expected` without `value()`: its accessors have preconditions, which en-gal's lint is
  to enforce; it holds a `std::variant`, so it is trivially copyable when both sides are, and never valueless (both
  sides must move without throwing).
- **The token** has only a move constructor: no assignment of either kind, so a token cannot be swapped into a
  variable that outlives its tick by assignment either.
- Counts: en_proto 42 test cases (26 new) on GCC, Clang, ASan+UBSan, MinGW under Wine and clang-cl under Wine; 6
  compile-fail tests in every build; `en_fuzz_game` about 2 million inputs in 3 minutes, no finding.

### 121. en-gal's offline part: the reader, the rules and the schemas, and the choices the design left

`2026-11-16` · METHOD · `#engine #gal` · refs: ARCH§6.1, ARCH§6.2, ARCH§6.3, ARCH§10.1, O27, O48, O89

- **Portable, and without exceptions.** en-gal builds with every compiler now; only `reader.cpp` stays clang-cl's.
  toml++ is built in en-gal with `TOML_EXCEPTIONS=0` (it keeps the setting in an ABI namespace, so en-llm's build with
  exceptions links beside it).
- **The reader copies, never points.** It reads through a `ByteSource` (the file offline, `sys`'s guarded image view in
  game), so the portable part holds no raw address; the gate and discovery take an `ImageInfo` (ARCH §2.2's
  `ImageView` is `sys`'s source).
- **A hit in a function's fragment is refused**, as is a hit running past its function's end: ARCH §6.2's rules speak
  of the containing function, which for a fragment (chained unwind information, or an entry pointing at another) is not
  the function whose prologue other mods hook. The derivation cuts patterns inside primary functions.
- **An unreadable 1 MiB copy is read again page by page**, on the source's page boundaries, so only the pages that
  fail are lost, and counted (`Unscanned` when a pattern then matches too few times).
- **`expect_matches` above 1** means every hit must resolve to the same function or static (else `Ambiguous`).
- **The idiom's schema is ours**: ARCH §6.3 shows only functions and statics. An idiom takes `expect_matches` as a
  range, and named `captures`, each with its target's section, alignment, and whether it must start a function; the
  path string after the variable gap is the FD4 finder's (part 3).
- **The region** comes from the file version's fourth field (0 ww, 1 jp; ER01 §1, a lead for S0-A), checked in
  builds.toml and in the gate. ARCH §6.3's cross-check against the translation's language is not built: the design
  gives no languages, and the exact fingerprint pins the build anyway.
- **A layout entry without a `status` is a lead**, so a release build refuses it. `dlvector` and `dlmap` are 16 bytes
  (two pointers; a pointer and a count) by our own definition, and `off` names where we read them.
- **The read groups' needs are the caller's** (`LayoutNeeds`), so no class or field name of the game is in the code;
  part 4 states them.
- Counts: en_gal 47 test cases on GCC, Clang, ASan+UBSan and MinGW under Wine, 55 on clang-cl under Wine (the 8 of the
  guarded copy); fuzzing, with ASan and UBSan: `en_fuzz_pe` 3.1 million inputs in three rounds of 2.5 to 3 minutes,
  `en_fuzz_pattern` 7.2 million in three rounds, and `en_fuzz_galtoml` five findings in toml++ (WHY§122), then 4.3
  million inputs in 12 minutes with none.

### 122. toml++ 3.4.0's traps: a header without a key, non-ASCII where whitespace may stand, `}` for a value, a date, a space and a digit

`2026-11-16` · FACT · `#engine #gal` · refs: ARCH§10.2, ARCH§12.1, WHY§112

- **Found by `en_fuzz_galtoml`**, each within minutes of the one before, each an assertion in a build without
  `NDEBUG` (the program aborts: the fuzz build, the Debug sanitizer builds, and clang-cl and MinGW builds with no build
  type) and undefined behaviour with `NDEBUG`:
  - `parse_table_header` calls `parse_key()` without checking that a key starts after `[` (`[!]`, `[=]`, `[ \x01]`,
    `[é]`, `[[ !]]`), and `parse_key()` opens with `TOML_ASSERT_ASSUME(a key starts here)`: `__builtin_assume`,
    `__assume` or `[[assume]]` with `NDEBUG`. Key-value lines and inline tables check first; only table headers do not.
    Clang 18 and GCC 13 at `-O2 -DNDEBUG` happen to take the error path today, which no rule promises.
  - `is_non_ascii_horizontal_whitespace` reaches `TOML_UNREACHABLE` (`__builtin_unreachable` with `NDEBUG`) for every
    character of its lookup's blocks 0, 5, 7, 11 and 63 that is not whitespace: U+00A1 to U+0499 (Latin-1, Greek,
    Cyrillic), U+1482 to U+187B, U+1C76 to U+206F, U+2D5A to U+3153 and U+FB06 to U+FEFE, with the few exceptions it
    names. It runs wherever whitespace may stand: after a key or a value (`schema Ä = 1`, `x = 1 é`), after a
    backslash in a multi-line basic string, while a key is recorded, and inside a bare value, which toml++ scans quotes
    and all up to a space, `]`, `}`, `,` or `#` (`x = 0E1"…é`, the fuzzer's fourth input: a quote there opens no
    string). UBSan reports "execution reached an unreachable program point".
  - `parse_array` reads a value wherever it finds neither `,` nor `]`, and `parse_value()` opens with
    `TOML_ASSERT_ASSUME(!is_value_terminator(*cp))`: a `}` there (`[}]`, `[1, }]`) breaks it. Key-value pairs check
    for a terminator first; arrays do not.
  - A value whose first ten characters look like a date (`-` at 4 and 7) followed by a space and one digit makes
    `parse_value` advance over the space and the digit and call its scan again, which assumes it does not start at a
    terminator (`1979-05-27 7,`, or the fuzzer's `5887-he-0a 6,`).
- **Probed by hand beside the fuzzer**: some 80 odd values (`1e`, `0x`, `+nan`, `1979-05-27T`, `12:3`, `1__2`, `.5`,
  `{ a = 1, }`, `[1,,2]`, …) all take toml++'s error paths; only the `}` in an array did not (the date trap needs a
  space and then a single digit, which the probes missed and the fuzzer found).
- **en-gal checks before toml++ sees a text** (`toml_read::unsafe_for_toml`, five rules): a line whose first
  character after spaces and tabs is `[` must go on, after `[` or `[[` and spaces or tabs, with a letter, digit, `_`,
  `-`, a quote, `]` or the end (it also refuses such a line inside a multi-line array); a character outside ASCII
  stands only in a comment, and a string writes one as `\uXXXX` (a first version allowed it in strings, and the
  fuzzer's fourth input went through a quote that toml++ reads as part of a bare value); there is no multi-line string;
  a `}` never follows `[` or `,` (never valid TOML); and a bare key or value that starts with a digit has no `-` or `:`
  after it (no dates or times, which gal/'s files never hold, so toml++'s date-time paths are never taken). ARCH §6.3's
  example `leads = ["ER01 §3.2"]` therefore becomes `leads = ["ER01 \u00A73.2"]` in a real gal/ file. The five inputs
  are regression seeds in `fuzz/galtoml/corpus/` (`finding-table-header-not-a-key`,
  `finding-non-ascii-where-whitespace-may-stand`, `finding-brace-where-a-value-must`,
  `finding-non-ascii-after-a-quote-in-a-value`, `finding-date-space-digit`).
- **en-llm is exposed to all four**: its `parse_toml` (`src/en-llm/src/config.cpp`) reads the player's settings, models
  and secrets files with toml++ (with exceptions; an exception does not help against an assertion or an assumption), so
  a stray `[!]` line, an accented or Cyrillic character after a value or in a mistyped one, `[1, }]` or
  `1979-05-27 7` in a file the player edits is undefined behaviour in the release DLL. Suggested: the same check
  before every `toml::parse` in the engine (the scan needs no toml++; a small header both modules may include, since
  en-llm may not depend on en-gal), its message worded for players, and a fuzz target for en-llm's configuration
  files (ARCH §12.1 lists "TOML content" among the fuzz stage's parsers). Also worth reporting upstream to toml++. Not
  done here: outside parts 1 and 2.
- **A question the main session may put to the owner**: each fuzzing round of a few minutes found the next trap, and
  the five rules close the four found; a last round of 12 minutes (4.3 million inputs) found nothing more. The rules are
  ours and narrow gal/'s TOML to what it needs, but they lean on toml++'s internals. Options: (a) keep toml++ with the
  pre-check, and fuzz it in CI; (b) our own reader of the TOML subset gal/ and the configuration files use (tables,
  arrays of tables, strings, integers, booleans, arrays, inline tables), a few hundred lines, fuzzed; (c) another
  library, checked for licence and fuzzed. Recommendation: (a) now, (b) before v0.1 if CI's fuzzing keeps finding
  traps, since en-llm reads files the player writes.

### 123. The fake game: our own stand-in for a game, its invented facts and the choices later parts inherit

`2026-11-16` · DECIDED · `#engine #gal #process` · refs: ARCH§6.2, ARCH§6.4, ARCH§3.2, O89, O91

- **What it is.** `tests/fakegame/fakegame.exe` (clang-cl and MinGW) and `tests/vehprobe/vehprobe.dll`, roadmap
  S10.1.a part 8. Every name, path, pattern, offset and value is invented; `fakegame --describe` prints them all as one
  JSON object, the truth that the derivation kit (part 3) and the GAL's tests (parts 4 and 6) compare with.
- **Choices later parts inherit.** (1) Its registration ABI is its own: rcx the task, rdx the hub instance, r8d the
  group, unlike the lead's order, so `register_abi` must be data (which register carries what), not a fixed order in
  code. (2) Its singletons have invented names (`TaskLoom` stands for CSTask, `ActorRoster` for WorldChrMan, …;
  `--describe` maps each role), and its stand-in group is `Fg_ActorSettle`, so the GAL must take singleton names, RTTI
  names and the group's name from `gal/` data, never from code. (3) Its task object is a vtable pointer and 0x18 bytes
  the hub writes, three slots (0 attach once, 1 run every frame, 2 query every 64th): the probe's slot counts have
  something to classify. (4) Its enums are invented (screen 1 title, 2 loading, 4 in game; lobby 10, 11, 12; role 2
  host, 3 guest), so the self-tests read enum ranges from the layout.
- **The harness's PE checks** (`harness.py`) carry ARCH §6.2's FD4 finder (placements, naming through the class
  object, ambiguity, refusals) and its RTTI walk (type descriptor, locator with signature 1 and its self RVA, the
  vtable after the locator's slot, the actor set from the hierarchy) in about 150 lines of Python: a reference the C++
  derivation kit can be compared with.

### 124. Wine on Xvfb for tests: the crash dialog that waits, the desktop bound to no display, and what reaches Linux

`2026-11-16` · FACT · `#engine #build #process` · refs: WHY§102, WHY§113, ARCH§12.1

- Source: our fake game under Wine 9.0 (Ubuntu's packages) in the cloud container, with `xvfb-run -a`.
- **The crash dialog.** With a display, an unhandled exception starts winedbg, whose crash dialog waits for a click:
  the process hangs until killed. `HKCU\Software\Wine\WineDbg` `ShowCrashDialog = 0` (set by `wine reg add` once per
  prefix) makes it end at once. With no display there is no dialog.
- **A desktop with no display.** After a Wine command run with no `DISPLAY` (a `reg add`), the next program in the same
  wineserver session could not create a window (CreateWindowExW answered null) although it had a display; it worked
  once the wineserver had ended. So the harness runs every Wine command under xvfb-run and ends each case with
  `wineserver -k` (about 1.5 s per cold start).
- **What reaches Linux.** A GUI-subsystem program started from Linux gets standard handles from the Unix file
  descriptors, so its lines reach the pipe. Its exit code reaches Linux as the low 8 bits only (0xC0000005 as 5,
  0xC0000409 as 9), so a check that needs the whole code reads it from inside (a filter's line) or from a Windows
  parent (the watcher).
- **Exceptions.** An access violation (read, write, executing data), a divide by zero, `ud2`, `int3` and a stack
  overflow each reach the program's unhandled-exception filter with their code; `__fastfail` ends the process past
  every handler and filter with 0xC0000409. A vectored handler sees an access violation before the frame handler and
  before the filter; one that ends the process leaves no frame handler a chance (S0-D asks the same on Windows).
- **The C runtimes' own filters.** Both MSVC's static CRT and MinGW's install an unhandled-exception filter at start,
  so the first `SetUnhandledExceptionFilter` a game or EldenNet makes replaces one: EldenNet's chained filter (part 6)
  meets a previous filter even in a game that installs none.
- **Little stack after a stack overflow.** In the release build a filter that formatted its line with the CRT's
  `vsnprintf` overflowed the stack again while handling the first overflow, and Wine ended the process with code 1,
  with no exception reported and no line written (Wine's trace: "stack overflow 2112 bytes"). A filter that builds its
  line by hand into 80 bytes and calls `WriteFile` works. EldenNet's crash filter (part 6: guarded, chained, no
  allocation) must also do its work with a few KiB of stack, or hand it to another thread.

### 125. Assembly whose bytes do not move: clang relaxes every jump at -O0

`2026-11-16` · FACT · `#engine #build` · refs: ARCH§6.2, WHY§111

- Source: the fake game's FD4-style sites, AOB functions and registration front, written as file-scope assembly in
  C++ (`asm.cpp`), built with clang-cl 18 at /Od and /O2 with ThinLTO, and with MinGW-w64's GCC 13 and GNU as.
- **At -O0 clang assembles with `-mrelax-all`:** every jump it can lengthen gets its long form (`jnz` became
  `0F 85 rel32` instead of `75 rel8`), so a pattern written in assembly changes with the optimisation level. Short
  jumps written as bytes (`.byte 0x75, 1f - 2f` between two labels) keep the bytes fixed; with them GNU as and LLVM's
  assembler gave the same bytes for every pattern, in every build.
- **Faults the optimiser undoes** (clang-cl /O2 with ThinLTO): `1 / b` becomes a comparison with no division, so a
  divide by zero needs both operands unknown; a `volatile` local array is cut down to the elements used, so a
  stack-overflow frame needs its array's address to leave the function. The harness's release run found both.
- `.seh_proc` gives a leaf function a `.pdata` entry too, so the name getter is "the start of a `.pdata` function"
  as ARCH §6.2 checks; the module's own unwind data covers the registration front, whose fault is caught by the
  caller's `__try` through it.

### 126. Wine 9.0 on Xvfb hands injected keys to DirectInput, but releases them on X focus and grab events; a scan code alone gives no virtual key

`2026-11-16` · FACT · `#graceui` `#wine` `#input` · refs: GraceUI:BUILD.md, GraceUI:D125, GraceUI:D128

- **Two paths** (FACT, probes of ours): a key injected with `SendInput` reaches DirectInput's keyboard through Wine's
  low-level hook, and the focused window as `WM_KEYDOWN`; a key injected by scan code alone (`KEYEVENTF_SCANCODE`)
  reaches the window with virtual key 0, and one by virtual key alone never reaches DirectInput. So the harness injects
  both.
- **The resync** (FACT, traced message by message): Wine brings its key state into line with the X server's on X focus
  and pointer-grab events. A key injected before a click, a focus change or a cursor confinement is released by Wine
  (a `WM_KEYUP` nobody sent), since the X server never saw it pressed. A click outside the window, where the cursor
  starts under Xvfb, also gives the desktop the focus, and later keys then reach no window at all.
- **So** the harness keeps the cursor in the window, lets those events pass before injecting a key, presses a button
  before a key, and checks a held key with `GetAsyncKeyState`. Real keyboards under Proton come through X events to the
  focused window and are not resynced this way (LIKELY; S4 on Proton).

### 127. Dear ImGui 1.93's Win32 backend asks the calling thread for keys and the cursor, and its io calls are not thread-safe: GraceUI feeds Dear ImGui itself

`2026-11-16` · FACT · `#graceui` `#overlay` `#input` · refs: GraceUI:D130, GraceUI:D122, GraceUI:invariant 10

- **The backend** (FACT, its source read): `ImGui_ImplWin32_NewFrame` sets the display size from `GetClientRect`,
  reads the mouse with `GetCursorPos` and the modifiers with `GetKeyState`; its window-procedure handler calls
  `TrackMouseEvent` and `SetCapture`. `GetKeyState` answers for the calling thread's queue, and `SetCapture` and
  `TrackMouseEvent` act on the calling thread's windows. The game's present thread need not be its window's.
- **The core** (FACT): `io.AddKeyEvent` and its kin push onto a vector that `NewFrame` resizes ("FIXME-MULTITHREADING"
  in `imgui.cpp`), so calling them from the window thread while the present thread runs a frame is a race.
- **So** (D130): the window procedure's filter queues compact events in a one-producer ring, and the present thread
  replays them into Dear ImGui before `NewFrame`, in back-buffer pixels, with the modifiers tracked from the events
  themselves; the backend's `NewFrame` is never called (D122 amended). The backend's key mapper,
  `ImGui_ImplWin32_KeyEventToImGuiKey`, left non-static for others' use, is called on the present thread.

### 128. Dear ImGui's 16-bit characters turn every code point past U+FFFF into U+FFFD; its queue takes one change of a key or button a frame

`2026-11-16` · FACT · `#graceui` `#overlay` `#input` · refs: GraceUI:D126, GraceUI:D130, GraceUI:§5.6

- **Characters** (FACT, `imgui.cpp` read): `AddInputCharacter` queues the full code point, but `UpdateInputEvents`
  stores `c <= IM_UNICODE_CODEPOINT_MAX ? c : U+FFFD`, and the maximum is U+FFFF unless `IMGUI_USE_WCHAR32`. Pairing
  surrogates before Dear ImGui is not enough without it: GraceUI's configuration defines it (D126).
- **Trickling** (FACT): with `ConfigInputTrickleEventQueue` (the default) a frame processes one change of a key or
  button and defers the rest, and text after a mouse move waits a frame. A harness that typed a key change, a character
  and a move every frame grew Dear ImGui's queue past its bound and dropped events at GraceUI's ring. A player types
  well under one change a frame; the host now hands Dear ImGui at most 64 events a frame while its queue holds fewer than
  128, and merges consecutive mouse moves (D130).
- **Focus events** (FACT): `AddFocusEvent(false)` then `(true)` in one frame clears nothing (the last wins, for
  multi-viewport backends); between holders the host empties Dear ImGui's queue in place and calls `ClearInputKeys` and
  `ClearInputMouse` itself. `ClearEventsQueue` would free the vector, an allocation on the present thread later.

### 129. Wine's DirectInput and XInput as GraceUI hooks them: one GetDeviceState for keyboard and mouse, five XInput DLLs, no pad

`2026-11-16` · FACT · `#graceui` `#wine` `#input` · refs: GraceUI:D127, EldenNet:ER02§7.2

- **DirectInput** (FACT, a probe under Wine 9.0): a keyboard and a mouse made through one `IDirectInput8W` share one
  vtable and one `GetDeviceState`; the ANSI interface's `GetDeviceState` is another function. GraceUI reads the function
  from both devices and hooks each distinct one (one here), on the Unicode interface the game uses (ER02 §7.2); what
  Windows' `dinput8.dll` shares is for S4.
- **XInput** (FACT): `xinput1_4`, `xinput1_3`, `xinput9_1_0`, `xinput1_2` and `xinput1_1` all load, each with its own
  `XInputGetState`; with no pad each answers `ERROR_DEVICE_NOT_CONNECTED` (1167). The harness loads three before GraceUI
  starts and finds three hooks, and the answers unchanged.
- **IMM** (FACT): `ImmGetContext` gives the window a context under Wine, and `ImmSetCompositionWindow` reads back with
  `ImmGetCompositionWindow`, so the composition window's placement is checked in the harness; no IME runs on Xvfb.

### 130. The game's window procedure is filtered by a subclass in the window's own character set, not a detour on the game's code

`2026-11-16` · DECIDED · `#graceui` `#overlay` `#input` · refs: GraceUI:D125, GraceUI:D102, GraceUI:research magelight.md

- **Choice** (D125): `SetWindowLongPtrW` on a Unicode window, `SetWindowLongPtrA` on an ANSI one, made once by the init
  thread after the table's hooks are enabled, passing on through `CallWindowProc`, never removed. The design named "the
  game's window procedure" in D102's list without saying how; this records how.
- **Rejected**: a MinHook detour on the procedure (it is the game's code, which GraceUI never writes);
  `SetWindowSubclass` (only on the window's own thread, and the init thread is not it); a `WH_GETMESSAGE` hook (it cannot
  keep a sent message from the game). A Unicode subclass on an ANSI window changes what later ANSI subclasses receive;
  in Magelight's reports, plugins that subclassed in ANSI crashed against its Unicode subclass.
- **Risks for S4**: another mod restoring the procedure it saw before GraceUI's (GraceUI would be cut out, unseen), and a
  game that recreates its window (the new one is not filtered: holders then get no characters, though the detours still
  mask).

### 131. The closing press is often down before any read sees it: masks are armed at the first read after a close, with a 250 ms window

`2026-11-16` · DECIDED · `#graceui` `#input` · refs: GraceUI:D128, GraceUI:invariant 8

- **The problem**: the press that ends a holder (Escape, F5, B) usually reaches the window procedure before the game's
  next DirectInput or XInput read. Masking only what the last read before the close saw held would let the closing
  press reach the game at the next read: Escape would open the game's menu, B would dodge.
- **Choice** (D128): each reader arms its masks at its first read after a close: what is held then and was held at the
  read before, or all that is held if the close was 250 ms ago or less (EST). A key pressed fresh within those 250 ms
  is masked until released, the conservative error; after them a fresh press is the game's.
- **The packet number**: XInput's is the pad's own plus one per grant and per close, so the game reads every change at
  rest and back; EldenNet NPC §5.5's "bumped once" leaves the return to the real state unread when the numbers meet.

### 132. The input half's cost under Wine, and how it was measured

`2026-11-16` · METHOD · `#graceui` `#input` `#perf` · refs: GraceUI:BUILD.md, GraceUI:invariant 10

- **What**: GraceUI's own work per call, timed inside each filter and detour with the original's time left out, in a
  test build; 1,000 messages and reads with no holder, then 300 frames of a holder fed input as a player types.
- **Figures** (Wine 9.0, two runs of each build): with no holder, the window filter 0.2–0.4 µs median and 0.9–2.5 µs
  p99 per message, `GetDeviceState` 0.5–0.8 and 1.1–3.3 µs per read, `XInputGetState` 0.1–0.2 and 0.2–0.3 µs; with a
  holder 0.6–0.9 and 1.8–3.3, 0.9–1.2 and 2.0–3.6, 0.1 and 0.3–0.4 µs; the MinGW build the higher figures. A holder's
  frame, its input fed and Dear ImGui's cursor drawn, the GPU idle before each: GraceUI's own work 94–107 µs median and
  289–543 µs p99, against 96–108 and 291–441 µs for the test panel alone. No allocation and no free in any of it after
  warm-up, on any of those threads.
- **Caveat**: Wine's DirectInput and its message queue are not Windows'; S4 measures in the game.

### 133. What the harness cannot show of the input half, for S4

`2026-11-16` · METHOD · `#graceui` `#input` `#spike` · refs: EldenNet:DESIGN§16.1 (S4), GraceUI:§5.2, GraceUI:D127

- **Pads**: Wine has no pad; the masking, the packet number and the opening press on a pad are checked in the unit tests
  only. S4: B and Start held at a close never reach the game; LT and RT and the sticks held through a close; Steam
  Input's own reading (it may bypass XInput).
- **What the game reads**: whether it reads keys through `GetDeviceData` (buffered), the ANSI interface,
  `XInputGetStateEx` or raw input (`WM_INPUT`), none of which GraceUI hooks but the last (routed to `DefWindowProc` while
  a holder takes the device); and whether it calls `TranslateMessage`, without which no `WM_CHAR` arrives (a holder
  would then need characters made from key-downs).
- **The window**: whether the game recreates its window, restores its procedure, or takes the IME's input context
  away (no composition window then); the IME under Proton (§5.6); F5 free in the default layout; Escape held 1 s and the
  menu key twice in 2 s as the player feels them; camera drift while the mouse is held (EldenNet DESIGN §8.1); the
  cursor's confinement under Proton.

### 134. ABI 1's first table keeps D73's order: typed where the design names a slot's shape, the bridge protocol's four reserved in their places

`2026-11-16` · DECIDED · `#graceui` `#abi` · refs: GraceUI:D131, GraceUI:D132, GraceUI:D73, GraceUI:D109

- **Choice** (D131): `graceui.h`'s `GraceUiApi1` holds all twenty of D73's slots in D73's order. Each slot whose shape
  the design names is typed now, taking the mod's handle and a struct that starts with its size, so that S10.4.c and d
  add fields rather than change a slot; `register_pages`, `publish`, `poll` and `answer`, whose shape is §6's bridge
  protocol (GraceUI 0.2), are `GraceUiReservedSlot`s that keep their place and get their type when built.
- **Rejected**: declaring only the slots built (the order would then be the increments', not D73's); typing every slot
  now, the protocol's four included (a guess bound by the append-only check).
- **The check** (D132): the first header is kept as `src/grace-abi/baseline/graceui-abi1-0.h`, and `check_safety.py`
  refuses a constant, typedef or export changed, and a struct changed otherwise than by appending; a reserved slot may
  be typed in its place. Its `--abi-self-test` edits the header seventeen ways to show it. While ABI 1 is a preview, a
  deliberate break updates the baseline in the same commit, which says why.

### 135. GraceUI's anti-cheat check runs in the first GraceUI_RequestApi, not on the init thread: connect's first answer must already know it

`2026-11-16` · DECIDED · `#graceui` `#anticheat` · refs: GraceUI:D133, GraceUI:D71, GraceUI:D88, EldenNet:ARCH§5.3

- **The problem**: D71 listed the check among the init thread's work, but a mod holds the table, and may connect, the
  moment the first request returns; the init thread can still be waiting for the game's window. `connect` would then
  answer ok and change its mind later, the silence D73 forbids.
- **Choice** (D133): the check runs once, in the first request of the copy that takes the mutex, on the mod's thread,
  before any table is returned and before the init thread starts. It costs one list of the process's modules and one
  Toolhelp snapshot of the processes. ARCH §5.3's clauses as they are: a module whose file name starts with
  `EasyAntiCheat`, or a parent whose image is `start_protected_game.exe`, ASCII letters without their case; a parent
  that has exited is skipped.
- **A hit**: the hooks go in as usual (D88), input is off from the start, so every detour and the window filter pass
  everything to the game (the menu key too), nothing is drawn but notices, `connect` answers inactive (anti-cheat) with
  a handle and `notice` works; GraceUI shows a notice of its own for 20 s.
- **Not done here**: no re-check during the session; D81's 60 s re-check and a provider's anti-cheat flag close the
  server, which comes later.

### 136. An anti-cheat hit simulated with no anti-cheat software: names a test build is told; Wine's Toolhelp names a process's parent

`2026-11-16` · METHOD · `#graceui` `#anticheat` `#wine` · refs: GraceUI:D133, GraceUI:BUILD.md

- **Method**: a test build exports `GraceUI_Test_SetAntiCheatNames`, called before the first request. The harness's
  "anti-cheat" scenario loads an empty DLL of its own, `grace-harness-shield.dll`, and has the check look for that
  prefix; "anti-cheat-parent" has it look for `grace-harness.exe`, the image of every scenario's parent. The real
  gathering (`EnumProcessModules`, a Toolhelp snapshot) runs either way; the real names' matching is unit-tested on
  Linux. No file in the repository is named after an anti-cheat.
- **Seen** (FACT, Wine 9.0): `Process32FirstW` and `Process32NextW` report a process's `th32ParentProcessID`, and
  the parent's `szExeFile` as its image name; both clauses hit, in both builds. Every other scenario runs the real
  names, and none hit.

### 137. Dear ImGui frees a draw list unused for 60 s of its own time, and its time leaps when frames resume: a notice would allocate during play

`2026-11-16` · FACT · `#graceui` `#overlay` · refs: GraceUI:D134, GraceUI:D117, GraceUI:invariant 10

- **Seen** (Dear ImGui 1.93.0, `imgui.cpp`): `NewFrame` deletes a viewport's foreground and background draw lists, and
  compacts a window's buffers, once they have gone unused for `io.ConfigMemoryCompactTimer` (60 s) of Dear ImGui's own
  time, and a later `GetForegroundDrawList` makes the list again. GraceUI builds frames only when it has something to
  draw, and its time step is the real time since the last one: after a quiet spell of a minute, the next notice's frame
  would free and make the list again, and grow its buffers, on the present thread.
- **Choice** (D134): `ConfigMemoryCompactTimer` at `FLT_MAX` (a negative value means "compact everything, every
  frame"), and warm-up ends with one frame built and never submitted, which makes the foreground list and reserves room
  for the most a frame of notices draws (Dear ImGui reserves four vertices and six indices for every byte of a text
  before it gives back what it did not draw). The harness counts no allocation and no free over 300 frames of notices
  coming, changing and going.

### 138. Two copies of GraceUI share only their two exports: the active copy names the others; an idle one asks only after its own first request

`2026-11-16` · DECIDED · `#graceui` · refs: GraceUI:D135, GraceUI:D72, GraceUI:invariant 18

- **The problem**: the active copy must name a second copy in a notice, but the copies have no channel between them
  besides `GraceUI_Version` and `GraceUI_RequestApi`.
- **Choice** (D135): at each of its requests the active copy lists the loaded modules and names every other one that
  exports `GraceUI_RequestApi`, once each, by its name relative to the game's folder (or its file name, "outside the
  game's folder"), never a path. An idle copy, once its own first request is over, calls every other copy's
  `GraceUI_RequestApi(1)`, so the active one hears of it at once. Asking only after its own first request has finished
  means no two copies wait on each other: a request that comes while another thread's first is running waits for it.
- **Seen**: two copies asked at once from two threads, in the harness: exactly one table, the other idle, named once.

### 139. The harness's ABI, copies and anti-cheat scenarios under Wine: a renderer that warms up after the hooks, and two flaky tests made sound

`2026-11-16` · METHOD · `#graceui` `#wine` `#process` · refs: GraceUI:BUILD.md

- **Scenarios**: six more processes, each one's first request its own: "abi" (the example mod found by name after
  polling; the table; connect's answers; struct sizes; two mods each way round; disconnect; the unbuilt slots;
  notices and their allocations), "copies", "found-by-scan", "found-by-path" (run from a folder of its own, as a game's
  folder), "anti-cheat" and "anti-cheat-parent": 68 checks in all, 43 of them the earlier ones.
- **Figures**: a frame of up to three notices, the GPU idle before each, costs GraceUI's own work 93–167 µs median and
  401–683 µs p99 across the last six runs (three of each build), beside 83–118 and 346–518 µs for the test panel alone
  in the same runs; all six passed all 68 checks.
- **A flake, fixed**: "example's notice" read three frames after the hooks went in and once saw none, because the
  renderer warms up over the frames after the hooks (finding the present queue, the atlas's upload); the check now reads
  up to 60 frames, and saw the notice at the third in each of six runs. Another, in grace-input's unit tests: the torn-read test's reader
  could finish its reads before either writer had published, under load; it now waits for the first value.
- **Noise**: the windowless scenarios end while GraceUI's init thread still looks for a window, and Wine may print a
  "wine client error" line as the process ends; harmless, but count checks by each process's summary line.

### 140. A panel's draw list is in the panel's own pixels, copied and checked on the mod's thread, and drawn from its triple buffer where it lies: no retire slot

`2026-11-16` · DECIDED · `#graceui` `#overlay` `#abi` · refs: GraceUI:D137, GraceUI:D76, GraceUI:§2.9, GraceUI:invariant 10

- **The problem**: no slot of ABI 1 tells a mod the display's size, so a list in the display's pixels could not be
  anchored; and §2.9 gives `std::atomic<std::shared_ptr>` snapshots with a retire slot, which allocate a control block
  per submission and still need the present thread to hand each list to Dear ImGui's DX12 backend as an ImDrawList.
- **Choice** (D137): a list is in the panel's own pixels; GraceUI moves it where the anchor puts the panel and clips
  every command to the panel and the display. `submit` checks the counts before it copies, copies into the panel's
  back buffer on the mod's thread, checks GraceUI's copy (which the mod can no longer change) and publishes it only if
  it passes. A triple buffer's back buffer is never the reader's, so the writer grows or frees it on its own thread and
  no retire slot is needed. The present thread moves the newest list's vertices in place once, then lends its arrays to
  an ImDrawList of its own (one a panel slot, made at warm-up) for the one `RenderDrawData` that copies them, and takes
  them back before it returns: it copies, allocates and frees nothing.
- **Rejected**: copying each list into GraceUI's own ImDrawList every frame (room for 64 panels' largest lists reserved
  at warm-up, about 32 MiB, or allocation during play).
- **Seen**: the unit tests' writer thread, whose lists grow and shrink, never hands the reader a list half written,
  and the reader's side counts no allocation or free; the harness counts none on the present thread over 600 frames
  with seven panels shown while the example publishes a list every 10 ms.

### 141. Dear ImGui 1.93's DX12 backend takes one draw data a frame, honours each command's vertex offset, and grows its buffers itself

`2026-11-16` · FACT · `#graceui` `#overlay` · refs: GraceUI:D137, GraceUI:D119, Dear ImGui 1.93.0 `backends/imgui_impl_dx12.cpp`

- **Read**: `ImGui_ImplDX12_RenderDrawData` advances its frame index at every call and writes that frame's vertex and
  index buffers, so a second call in one frame would overwrite buffers still in flight (and fall out of step with
  GraceUI's four frame slots, D119): every layer's lists go in one ImDrawData, in layer order. It reads only the draw
  data's lists, totals, display position and size and each command's clip, texture id, offsets and count, and asserts
  that the totals match the lists exactly. It sets `ImGuiBackendFlags_RendererHasVtxOffset` and draws each command from
  its own `VtxOffset`, which is how D76's per-command vertex ranges reach the GPU unchanged. When a frame's totals
  exceed its buffers it releases them and makes new ones of the total plus 5,000 vertices (10,000 indices): a Direct3D
  buffer made on the present thread, without waiting and with none of GraceUI's heap, the first time a larger frame is
  drawn.
- **So**: GraceUI composes one draw data a frame from its own lists and Dear ImGui's, and the growth is noted in D137
  for S4 to measure in the game.

### 142. Dear ImGui 1.93 for a mod's headless panel: its atlas built with ImFontAtlasBuildMain and given its texture id before the first frame; API renames that bite

`2026-11-16` · FACT · `#graceui` `#overlay` `#abi` · refs: GraceUI:D144, GraceUI:D76, examples/hello-panel

- **Seen** (1.93.0, building `examples/hello-panel`): with no backend, `ImGuiBackendFlags_RendererHasTextures` is off,
  and `NewFrame` asserts that the atlas is built; the obsolete `GetTexDataAsRGBA32` and `Build` are gone with
  `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`, and `ImFontAtlasBuildMain` (`imgui_internal.h`) builds it. `ImDrawCmd::GetTexID`
  asserts when a command's texture data has no id, so the mod registers its atlas as a texture and calls `SetTexID` and
  `SetStatus(ImTextureStatus_OK)` before its first frame.
- **Renames in 1.93**: `ImDrawFlags_RoundCorners*` became `ImDrawFlags_Round*`, and `AddRect`'s last two parameters
  were swapped in 1.92.8 (thickness before flags), with the old order deleted; mods' code from earlier versions fails to
  compile rather than misdraw.
- **Also**: Dear ImGui's recoverable errors (`IM_ASSERT_USER_ERROR`) assert only while `ConfigErrorRecoveryEnableAssert`
  is on; with it off, `AddFontFromMemoryTTF` on data stb_truetype cannot parse rolls back and answers null. GraceUI turns
  it off only while it adds the mods' faces (D139).

### 143. Textures: the registration window ends at a mod's first submit; a later mod's textures are one upload at the next frame; nothing is freed on the present thread

`2026-11-16` · DECIDED · `#graceui` `#overlay` · refs: GraceUI:D138, GraceUI:D76, GraceUI:invariant 11, proposal 0001 §4.2

- **The problem**: invariant 11 forbids uploads after load, but §3.16 lets "a late mod" bring its own atlas texture,
  and D76 names a "registration window" without its bounds.
- **Choice** (D138): the window runs from connect to the mod's first `submit` (proposal 0001 §4.2: "before the mod's
  first submission"), within 8 textures and 8 MiB a mod and 64 and 64 MiB in all (EST). Textures registered before
  warm-up join the atlas's upload; a later mod's go up together at the next frame, on the present queue, never waited
  on, each drawn once its copy's fence has passed; the upload counter counts each batch. A texture's pixels are freed
  on a mod's thread once the present thread has copied them; its GPU objects, once its mod has gone, are released by
  the present thread after the last frame that could draw it (a Direct3D release, none of GraceUI's heap), and its slot
  is freed on a mod's thread.
- **Seen**: the harness's late mod's eight textures went up in one upload at the frame after its first submit, and its
  texture after that submit answered too late.

### 144. Faces: warm-up seals the table slot by slot, so a face being written then is late, never lost; a mod's broken font is left out, not thrown

`2026-11-16` · DECIDED · `#graceui` `#overlay` · refs: GraceUI:D139, GraceUI:D80, GraceUI:D121

- **The problem**: a mod may be copying a 4 MiB font on its thread at the moment the present thread warms up; a flag
  checked by the writer could let a face be answered ok and never be baked.
- **Choice** (D139): each face slot moves empty, writing, committed by compare-and-swap; warm-up turns every empty or
  writing slot to sealed, slot by slot, and bakes the committed ones. A writer's commit then fails, the call answers
  too late, and the writer frees its copy. A mod's file is checked first (an sfnt whose table directory lies within it
  and names a face's tables; ranges outside the surrogates, bounded), and Dear ImGui's recoverable errors are answered
  rather than thrown while the mods' faces are added, so a file it cannot parse is left out with one log line instead
  of turning the render path off (D121).
- **Rejected**: one atomic count of faces with a closed bit (writers serialised on it, a slow copy holding the others).

### 145. ThreadSanitizer's runtime defines the global allocation functions: a counting operator new cannot run under it

`2026-11-16` · FACT · `#graceui` `#build` `#test` · refs: GraceUI:BUILD.md

- **Seen**: linking a test binary that replaces `operator new` and `delete` (to count allocations per thread) under
  `-fsanitize=thread` with Clang 18 fails with multiple definitions against `libclang_rt.tsan_cxx`; AddressSanitizer's
  runtime lets the replacement through.
- **So**: the counter is compiled out under ThreadSanitizer (counts stay 0 there): the ThreadSanitizer run checks the
  hand-overs, and the AddressSanitizer and GCC runs check that the reader's side allocates and frees nothing.

### 146. The harness's panels scenario under Wine: what it reads back, its figures, and a frame-cost check that fails under another build's load

`2026-11-16` · METHOD · `#graceui` `#wine` `#perf` · refs: GraceUI:BUILD.md, GraceUI:D137 to D144

- **Scenario**: "panels", a process whose warm-up comes after the second example mod and a mod of the harness's own
  have registered (nothing is presented until they have): 13 checks, 82 in all across eleven scenarios. It reads back
  the example's ground, its checker's squares, its own font's ink and its gold line from the shared atlas's glyph
  table; each layer over the one below where two overlap; anchors after a resize; the checker's refusals through
  `submit`; a late mod's textures; input through ABI 1 with the player's exit; providers; a disconnect.
- **Figures** (Mesa's lavapipe on four cores, the GPU idle before each frame): seven panels and the test panel cost
  GraceUI's own work 232–335 µs median and 525–804 µs p99 over 300 frames in the passing runs, within the 1 ms budget
  for all panels together; notices alone 98–126 µs median. Most of the panels' cost is the DX12 backend's recording,
  a call or two a draw command, each a translation to Vulkan here.
- **Under load**: three runs in a row of each build passed all 82 checks; two other clang-cl runs each failed one
  frame-cost check alone ("notice allocations" p99 1,546 µs, "panel frames" p99 1,562 µs, their medians as usual, no
  allocation) while another agent compiled EldenNet (load average 3.5 to 4.8 on 4 cores). A frame-cost check under
  Wine is a check of the machine as much as of GraceUI: rerun before calling it a failure.
- **Fuzzing**: the checker's libFuzzer target ran 188,667 inputs in 60 s with AddressSanitizer and UBSan, none failing.

### 147. What S10.4.c leaves to S4 and to the bridge protocol: a panel holder's events, the backend's buffer growth in the game, a mod's untrusted font

`2026-11-16` · METHOD · `#graceui` `#spike` `#input` · refs: GraceUI:D137, GraceUI:D139, GraceUI:D142

- **A panel holder's events** (D142): `request_input` takes the controls from the game for a mod's panel, but what the
  player types, and the holder's own end, reach the mod only through `poll`, which comes with §6's protocol (0.2); a
  panel holder in 0.1 gets the masking, the player's exit and Dear ImGui's cursor, nothing else. EldenNet's chat box
  waits for it, as its increments plan.
- **For S4**: the DX12 backend's buffer growth on the first larger frame, and the textures' one-batch upload, on a real
  GPU and under Proton; the atlas's size with every face a session registers (512 x 1,024 in the harness with four
  faces).
- **Trust**: stb_truetype, which Dear ImGui bakes with, is not hardened against hostile fonts; GraceUI checks a mod's
  file's table directory and ranges, and a native mod can crash the game in other ways anyway.

### 148. Our x86-64 length decoder against LLVM: the choices where processors and LLVM differ

`2026-11-16` · FACT · `#engine #gal` · refs: ARCH§6.2, O48, O89

- Source: en-gal's decoder (`x86.hpp`) against llvm-objdump 18 over every instruction of our own binaries, and over
  20,000 seeded random byte strings each after a symbol of its own (`tests/x86_objdump.py`, `en_gal_x86_check`).
- **Our binaries: no disagreement.** Every instruction of the build's own `eldennet-engine` and `en_gal_tests`:
  1,246,712 (GCC, -O0), 1,052,573 (Clang, -O0); and of the Windows binaries (`fakegame.exe`, `eldennet-engine.exe`
  and `en_gal_tests.exe` from clang-cl, `fakegame.exe`, `eldennet-engine.exe` and `vehprobe.dll` from MinGW):
  3,069,635, all agreeing (26 bytes LLVM cannot decode, refused alike).
- **Where LLVM 18 chooses** (we follow it, since compilers never emit these forms): a 66 prefix shortens JMP's
  (E9), Jcc's (0F 8x) and XBEGIN's displacement to 16 bits but not CALL's (E8); a REX followed by another prefix is
  printed alone and ignored; a REX before VEX is refused. It refuses 0F 20 to 0F 23 with ModRM's mod other than 11
  (we follow the manuals: mod is ignored there, no displacement).
- **Known kinds in random bytes** (counted apart, not findings): opcodes a map leaves undefined, which LLVM refuses
  and we size by the map's shape; a REX before another prefix, which LLVM decodes in ways of its own (it dropped a 66
  after one); APX's REX2 (D5) and EVEX map 4, which LLVM 18 already decodes and we refuse. Of 20,000 random strings
  (seed 1, weighted to prefixes, escapes, VEX, EVEX and XOP): 9,342 agree, 3,464 refused alike, 5,097 undefined
  opcodes, 2,077 stray REX, 20 APX, no other disagreement.
- **llvm-objdump's listing:** lone prefixes (`lock`, `data16`, `rex.W`) on lines of their own, to be joined to the next
  instruction; a vector compare's predicate or a carry-less multiply's selector folded into the mnemonic (`cmpltps`);
  FMA4's and the VEX blends' fourth register printed from the immediate's high nibble; no RIP target where its comment
  says something else or the target lies below zero. The checker reads all four.

### 149. The derivation round's tool: leads, the numbers-only writer, and how each fact is found offline

`2026-11-16` · METHOD · `#engine #gal #datapiler #privacy` · refs: O89, O48, O73, O74, O75, ARCH§6.2, ARCH§6.3, ARCH§6.10, DataPiler DP36

- **No name in the code.** What to look for comes from a leads file (names and shapes, never bytes); the code knows
  none, the tests use invented programs, and only the leads' labels (`[a-z][a-z0-9_]*`) reach derive.json, never a
  name read from the executable.
- **Numbers only by construction.** derive.json is written through a writer with a closed vocabulary (whole numbers,
  booleans, fixed words, labels, patterns of at most 64 bytes in canonical form) and checked by `lint --report` before
  it is written. The lint reads tokens, not a parsed document: nlohmann/json keeps only the last of a repeated key, so a
  string could hide behind a later number. The report fuzzer found the writer itself writing a key twice, which the
  lint refuses; the writer now refuses it too.
- **The FD4 idiom** is cut from the sites that load a path ending with the suffix: before each load, the closest
  `mov reg,[rip+x]; test reg,reg; jnz` whose jump passes the load; the largest group of sites with the same head; their
  shared bytes, a displacement or immediate that differs anywhere wildcarded whole, and the jump kept where its target
  lies within the pattern. On the fake game it equals the fake game's own idiom, byte for byte. The name layout is the
  class-object offset that points at an identifier in .rdata for the most of the accepted sites.
- **register_task** from the direct calls after a load of the hub's static, a small register model following rcx, rdx,
  r8 and r9 back to the start of the block: the instance, one 64-bit and one 32-bit argument; candidates by callee, the
  shaped ones first.
- **The group's index** from a table of pointers to names. The fake game's two name tables lie end to end (8 and 10
  entries): the run alone gave 13; cut where the code addresses a table (`lea reg,[rip+table]`) it gives 5.
- **Patterns** cover whole instructions at entry + 5 or later in a primary function, at most 64 bytes, with RIP-relative
  and other 32-bit displacements, outside branch targets, relocated bytes and immediates beyond a signed byte
  wildcarded; unique in .text; 20 bytes with 12 fixed preferred, then the smaller function (a first version kept a
  32-bit stack offset fixed and chose a 71 KB function; now the fake game's four AOB statics are each cut in the fake
  game's own small function); then checked by part 2's scan as patterns.toml would hold them.
- **What only R1 can find:** the four AOB statics' slots (their instances exist only at run time); R0 gives their
  classes' vtables for R1's search, and `--static label=rva` cuts their patterns afterwards. The task's run slot (R0's
  candidate: the one slot every task class fills with its own function) and object size; the names' layout against
  the instances' classes; the group's name at the index.

### 150. probe-exe on fakegame.exe: what R0 recovers, by toolchain

`2026-11-16` · FACT · `#engine #gal` · refs: O89, ARCH§6.2, WHY§123

- clang-cl (Debug): the fingerprint; the FD4 idiom equal to `--describe`'s, name layout +0x18 ASCII, eleven
  singletons with their statics, class objects, getter and site counts; Duplicata ambiguous, Strayling refused (path),
  Rockbound refused (static in .rdata); register_task at its entry with rdx, rcx, r8d and its four call sites, a
  20-byte pattern at entry + 13; the stand-in group's index 5 of 10; the actor set (five primary vtables, one
  subobject), the player set, the four task classes with three slots and run slot 1, the three AOB statics' classes'
  vtables. Given the slots: each static's pattern in its own AOB function (entry + 5, + 5, + 8, + 21).
- MinGW: the same but RTTI (GCC's own RTTI: no locators, as `--describe` says).
- The tool reads only the file: 0.27 s on the clang-cl fakegame.exe (229,000 instructions); a sweep decodes about
  3.5 million instructions a second here (1.05 million in 0.3 s on MinGW's 3.5 MB of code), in about twice the
  file's size of memory.

### 151. toml++ built two ways in one program: two vtables of one name

`2026-11-16` · FACT · `#engine #build` · refs: ARCH§2.4, WHY§121, WHY§122

- Source: `eldennet-engine probe-exe`, the first program to link en-gal (toml++ with `TOML_EXCEPTIONS=0`, part 2) and
  en-llm (toml++ with exceptions), crashed reading its leads in GCC's build (a memcpy from a bad pointer inside
  `toml::parse`); Clang's build ran.
- **Cause:** toml++ 3.4.0 puts its parse functions and its error type in ABI namespaces by the setting (`ex`, `noex`),
  but not its internal `impl::utf8_reader_interface`, which has a virtual `error()` only without exceptions: two
  vtables of one name, merged by the linker, so the reader called the wrong slot. Part 2's note ("toml++ keeps either
  setting in an ABI namespace of its own, so en-llm links beside it") holds for the functions, not for this class.
- **Fix:** one setting for every module (`en_third_party` defines `TOML_EXCEPTIONS=0`); en-llm's one parse reads the
  `parse_result` and throws its own `ConfigError` as before. The DLL, which links both, is covered by the same
  definition. All tests pass in every build after it.

### 152. World-state claims and an input grant: read where the providers write them, fenced on both sides

`2026-11-16` · DECIDED · `#graceui` `#input` `#overlay` · refs: GraceUI:D145, GraceUI:D129, GraceUI:D143, GraceUI:invariant 9

- **The problem** (the review's M1): the input half kept copies of the providers' combined claims, written after the
  providers' lock was released, so the last provider to apply its copy won, not the last to report: danger could be
  lost (a holder granted in combat) or stick (every request refused). And a request read the claims, then granted by
  compare-and-swap, while a provider stored danger, then looked for a holder to close: both can miss each other even
  with sequentially consistent operations, since it is a check-then-act, not only a reordering.
- **Choice**: no copies; a request reads the combined word, grants, then reads again behind a full fence and ends the
  new holder if danger or loading (or input or the overlay off) is found; every writer fences, then reads the newest
  conditions and ends what they forbid (`Capture::settle`). With a fence on each side, at least one sees the other.
- **Shown**: a unit test that claims danger inside the request's first read (the interleaving made deterministic), and
  2,000 two-thread rounds under ThreadSanitizer; in the harness, 3,000 claims from two providers' threads while a third
  asks for input, ending on danger: no holder left.

### 153. The window filter follows the bound swap chain's window; a console's window is never the game's

`2026-11-16` · DECIDED · `#graceui` `#input` · refs: GraceUI:D146, GraceUI:D120, GraceUI:D125

- **The problem** (M2): the init thread filters the largest visible window of the process at its first look, before
  the game's window may exist; a mod loader's console (conhost names the console's client as the window's owning
  process) or a splash window could be taken, leaving input off for the session or the cursor clipped elsewhere.
- **Choice**: console windows are skipped; at each bind, if the swap chain's top-level window is not the one filtered,
  GraceUI's own thread filters it. One subclass per window given (four at most), only the newest active; an earlier one
  passes everything on and is never removed (removing a subclass under another mod's is unsafe).
- **Shown**: the harness's decoy-window scenario: a larger window of its own made first is filtered first, then the
  filter moves; F5 posted to the decoy reaches its procedure, F5 posted to the game's does not.

### 154. The shared atlas has a room, and is baked on GraceUI's own thread, which stays for the session

`2026-11-16` · DECIDED · `#graceui` `#overlay` `#perf` · refs: GraceUI:D147, GraceUI:D148, GraceUI:D117, GraceUI:D139

- **The problem** (M3): face counts alone let mods ask for more than Dear ImGui's 8192-pixel atlas; the review's probe
  saw four 128 px faces of a Latin font's whole BMP assert ("Out of texture memory"), turning the overlay off for every
  mod, and two take 544 ms on the present thread, with 256 MiB of pixels kept for the session.
- **Choice**: each face's area is estimated when it is registered (the glyphs its font's character map gives its
  ranges, each a box of (2 × size + 2) by (size + 2)), within a room of 8192 × 4096 estimated pixels, the mods other than
  EldenNet half of it (EST); past it, `full`. The bake runs on the init thread, which now stays asleep on an event for
  the session; should Dear ImGui still run out of room, it bakes again with half the mods' faces, then none of theirs.
  It also fills the atlas's upload buffer and frees the CPU copy; the present thread only records the copy.
- **Rejected**: an Alpha8 atlas (the DX12 backend's shader would sample it as red, and D107 forbids changing it); a
  thread made for each job; baking on the present thread within a smaller room (a CJK face would not fit).
- **For S4**: the bake's time and the atlas's size with EldenNet's real faces; a CJK face of the whole unified block at
  24 px is about 27 million estimated pixels, so EldenNet registers only the ranges it needs.

### 155. Dear ImGui 1.93's atlas: an 8192-pixel limit that asserts, a context made on one thread and used on another, ClearTexData for a legacy atlas

`2026-11-16` · FACT · `#graceui` `#overlay` · refs: GraceUI:D148, GraceUI:D117

- **Seen** in the pinned source: `TexMaxWidth` and `TexMaxHeight` default to 8192; when the packer has grown to them
  and tried three times, the glyph loader asserts. In static mode every code point of every range is preloaded, but a
  code point the font maps to glyph 0 is not packed, so a face's cost is its mapped code points, not its ranges.
- **Seen**: `GImGui` is a plain global in GraceUI's configuration, so a context made on one thread may be used on
  another once handed over with release and acquire; `CreateContext` keeps an earlier current context current, so a
  second one needs `SetCurrentContext`. A context whose bake asserted is left as it is (only its atlas's pixels freed):
  Dear ImGui's state after a failed assertion is not trusted to be torn down.
- **Seen**: `ImFontAtlas::ClearTexData` is Dear ImGui's own call to free a legacy atlas's CPU copy once a renderer
  without the texture path has uploaded it; nothing reads the pixels afterwards in GraceUI's use.

### 156. GraceUI lets go of a removed or replaced device; the render path stays off until a later version rebuilds

`2026-11-16` · DECIDED · `#graceui` `#overlay` · refs: GraceUI:D149, GraceUI:D150

- **The problem** (L3): Direct3D 12 hands out one device per adapter while a reference lives, so GraceUI's would give a
  game recovering from a removal the removed device again, and keep a replaced device's memory for the session.
- **Choice**: on removal (checked each frame) or a bind on another device, the render path turns off, input with it,
  and everything GraceUI made on the old device, and the device itself, is released at once for a removed device or
  once its last frame's fence has passed; the queues held for finding the present queue are let go too.
- **Shown**: the harness's device-change scenario under vkd3d: the old device's references 24 before GraceUI, 58 while
  it drew, 24 once the game moved to a new device.
- **Left**: rebuilding on the new device; the mods' texture pixels are freed after their upload, so it would need
  them registered again, or 64 MiB kept for the session.

### 157. The review's smaller fixes, and what they leave as it is

`2026-11-16` · DECIDED · `#graceui` `#input` `#abi` · refs: GraceUI:D150 to D153

- **Fixed**: hooks live after a failed enable pass everything through (L1); the log reaches the debugger from GraceUI's
  own thread, never from the thread that wrote it (L2); the render path off turns input off, F5 included (L4); the
  watchdog hears only frames drawn or with nothing to draw (L5); an answer's size is read once and a command stride
  bounded at 256 bytes (L6); the fourth way to the present queue takes its reference inside the queue's own call (L7);
  the render targets' modules are pinned (L8); a request during the first waits 2 s at most (L9); the cursor's
  confinement is undone by the granting thread when the holder ended meanwhile (L10); nothing goes to a new present
  queue until the old has drained (L11); a long copy name keeps its end (L12); UTF-8 as an ANSI code page is joined
  (L13); a holder's panel hidden or removed on another thread ends it, and a leaving mod's slot is not reused before its
  clean-up (L14).
- **Left as it is**: a holder closed and another granted within the same microseconds may leave the first's saved clip
  for the second (Windows drops a clip on a foreground change); an HDR swap chain's colour space, which GraceUI cannot
  read back, for S4.

### 158. Under Wine a Unicode folder needs a UTF-8 locale; the fuzzers' coverage needs the rules instrumented

`2026-11-16` · METHOD · `#graceui` `#wine` `#test` · refs: GraceUI:BUILD.md

- **Seen**: under the C locale, Wine 9.0 cannot make a folder named in hiragana (`CreateDirectoryW` fails, then
  `CopyFileW` with error 3); with `LC_ALL=C.UTF-8` it can, and every other scenario passes the same. A Linux file
  system takes 255 bytes a name, so a name longer than a notice's room in UTF-8 is made of two nested folders.
- **Seen**: the fuzz targets linked the rules' library without the fuzzer's coverage, so libFuzzer was guided only by
  the target's own file (coverage 118 for the font target); with `-fsanitize=fuzzer-no-link` on the library, 262.
- **Figures**: after the fixes both builds passed all 95 harness checks across sixteen scenarios; one MinGW-w64 run
  failed "notice allocations" alone (p99 1.33 ms, load average about 3 on four cores), and three reruns passed.

### 159. en-gal in game: the seam, our own labels, and what the design left open

`2026-11-16` · METHOD · `#engine #gal` · refs: ARCH§6.1, ARCH§6.3, ARCH§6.4, ARCH§6.6, ARCH§6.7, ARCH§7.3, O48, WHY§123, WHY§164, WHY§165, WHY§168

- **Portable above the seam.** `SysLayer` keeps only what needs Windows (the module and a guarded image view, the copy
  primitive, the stack table, the clock, the getter fallback, the one registration); discovery, `GuardedReader`'s
  rules, the groups and the task's cadence are portable and run on Linux over `FakeSys`, a process of byte regions.
  ARCH §6.4's `singleton()`, `rtti_vtables()` and `param_table()` became en-gal code over that seam (WHY§165).
- **Our labels, the game's names as data.** `roles.hpp` names 16 items, 35 classes and 97 fields in our words; the
  layout keys its classes by those labels, with the game's RTTI and singleton names as values. A field is checked by
  kind, not exact type. `register_abi` is a table of registers (the fake game's own order: rcx task, rdx instance,
  r8d group; the FakeWorld's: r8, rcx, rdx; WHY§164).
- **What a group needs** is its items resolved, its fields present with their kinds, its enum tables naming only
  values the snapshot knows, and its classes' RTTI found; otherwise it is off from the start (NotDiscovered), and a
  structural one stops registration.
- **Choices**: ARCH §6.6's three-tick rule counts per item, once a tick (an item's object is copied once a tick and
  shared by the groups that read it); a list entry that faults stops the walk (a hard fault), one of another class is
  dropped (soft); a spent budget is never a group's fault; phase A waits up to 8 ticks for a null instance, then fails
  it; phase B runs on the first full snapshot of a playthrough, which also sends `PlaythroughReady`; flag frames ride
  on full snapshots only; the render feed re-validates the last walk's addresses per frame (WHY§168).

### 160. NativeSys: guard pages are never touched, and the stacks are found by them

`2026-11-16` · DECIDED · `#engine #gal` · refs: WHY§111, ARCH§6.4, ARCH§6.5, R2, R14

- The image view reads only ranges VirtualQuery reports committed, readable and without PAGE_GUARD, so discovery never
  lifts a guard in the game's image; a refused range falls back to page-by-page reads (part 2's scan).
- The stack table is filled by walking the process's regions with VirtualQuery: every allocation that holds a guard
  page is added whole (a thread's stack: reserve, guard, committed top), and in an image only its guard pages. Each
  thread adds its own stack (GetCurrentThreadStackLimits) as it asks: T1 at construction, the game thread at its first
  tick. A thread made after the walk is covered only once `refresh_stacks` runs again. No TEB is read.
- The task object: one block (vtable pointer, `object_size` zeroed bytes, our body's pointer) and a fake vtable of
  `vtable_slots` entries, each a distinct guarded no-op counting its calls (64 template instances), made read-only
  once filled; the run slot is `en_task_run`, which runs the body only when `self` is our block, and returns 0 in rax.
  Never freed (R14). `set_registered` comes before the call: the getter is refused from then on, and nothing retries.

### 161. Wine's ReadProcessMemory on its own process ignores page protection and costs about 0.1 ms a copy

`2026-11-16` · FACT · `#engine #gal #wine` · refs: ARCH§6.5, S0-D, WHY§111

- Measured under Wine 9.0 (`test_native_sys.cpp`, the `gal-rpm` case): ReadProcessMemory(GetCurrentProcess(), ...)
  answers success, with zeros, for a page reserved with PAGE_NOACCESS, a released region and a guard page, whose guard
  stays (it does not raise the guard page fault). LIKELY the mechanism: the Wine server reads the process's memory
  file (/proc/<pid>/mem), which ptrace's rules let past page protection.
- Its cost there is about 0.1 ms a copy (a snapshot of about 150 copies took about 7 to 15 ms against 0.18 ms with
  the SEH copy), so the task throttled to 2 Hz (R10), as designed; Windows' cost (ARCH §6.5's "about 1 µs") is S0-D's
  to measure. The SEH copy under Wine behaves as Windows' (WHY§111).
- So under Wine the `rpm` setting cannot show a fault on a no-access page; the test injects `Injected` faults instead.

### 162. The GAL in the fake game under Wine: what the test DLL finds, and what it costs

`2026-11-16` · FACT · `#engine #gal #wine` · refs: O89, ARCH§3.2, ARCH§6.2, ARCH§7.2, WHY§123

- `galtest.dll` in `fakegame.exe` (clang-cl, under Wine 9.0 on Xvfb), from gal/ written from `probe-exe --derive`
  with R1's slots: one discovery pass (113 ms in the Debug build, 6.9 ms in the Release one with ThinLTO; the fake
  game's image is about 2 MB, the game's about 80) resolves all 16 items (11 singletons by their class objects' names,
  no getter call; the 4 AOB statics and register_task by derived patterns) and every group; one registration in
  `Fg_ActorSettle` (index 5) from T1; phase A at the title; `DataReady`; then 4 Hz by time (about one snapshot per 15
  frames at 60 FPS), a flag frame every fourth (flags 100, 7005 and 4321010 set, group 999 missing), the asked text
  (two names, one empty) and row; the slot counts: the run slot every frame, the hub's attach once, its query every
  64th frame; and no allocation or free on the game thread after the first three snapshots.
- The task's cost there, with 6 characters: Debug p50 27 µs an invocation, p99 159 µs, a build's p99 about 170 µs;
  Release p50 7 µs, p99 55 µs, a build's p99 90 µs (about 120 copies a snapshot). ARCH §7.2's budgets (mean 0.15 ms,
  p99 0.5 ms at 32 characters) are S0-B's to measure on the real game.
- The stack table under Wine holds 2 ranges at T1 (its own stack, and one allocation with a guard page; Wine's thread
  stacks show no PAGE_GUARD to VirtualQuery), then the game thread's own at its first tick: the per-thread
  `note_own_stack` is what covers the threads that read, and S0-D should count what the walk finds on Windows.
- The probe's static-slot search found each AOB static's slot (the slots R1 would give `probe-exe --static`) among 110
  candidates, with 123 faults caught and up to 3 candidates refused as stacks on the way (WHY§170).
- The Release build (ThinLTO) passes the same gal cases: the calls under SEH in register.cpp keep their guard.
- Moving statics (tombstones or released), loading screens, the session going online, injected faults in camera (L3)
  and world (inert, then no copy), the ReadProcessMemory copy, and a group table shifted since the derivation (the
  first tick finds `Fg_ActorMove` at index 5 and the layout's name at 6, and goes inert) each behave as ARCH §3.2 and
  §6.6 to §6.8 say.

### 163. One TOML pre-check for the engine, and a quote inside a bare word

`2026-11-16` · DECIDED · `#engine #gal #llm` · refs: WHY§122, ARCH§11.5, ARCH§12.1

- en-gal's pre-check moved to en-proto (`toml_check.hpp`), which en-gal and en-llm both include, with two sets of rules:
  gal/'s (ASCII outside comments, as before) and the player's, for providers.toml, models.toml, settings.toml and
  secrets.toml: a letter outside ASCII may also stand inside a quoted string, where it truly opens one. A refusal names
  the file, the line and a fixed problem worded for the player; it never quotes the text (a key may be there).
- New in both: the check follows TOML's contexts far enough to know where toml++ scans a bare value, and refuses a quote
  inside one (WHY§122's fourth finding reached the trap that way; with the player's rules, that is what lets non-ASCII
  stand in real strings), control characters, and anything after a value but a terminator or a comment.
- `en_fuzz_config` drives `parse_config` with four texts. With the first version of the player's rules it found two
  ways in within minutes, both reaching toml++'s unreachable branch (`is_non_ascii_horizontal_whitespace`): `= 2`,
  control characters, then a quote and U+FD7F; and `= 4=["`, then `é`. toml++ scans a bare value through anything up to
  a space, a tab, `]`, `}`, `,`, `#` or a line break, so `=`, `[`, control characters and quotes are all inside it, and
  what looked like a string opened inside a bare value. The check now follows TOML's contexts (a key, a header, a
  value's start, a bare value, what follows a value, arrays and inline tables, nested at most 32 deep), refuses control
  characters (TOML allows none but the tab), a quote inside a bare value, and anything after a value but a terminator
  or a comment. Both inputs are regression seeds (`fuzz/config/corpus/finding-*`). Then en_fuzz_config ran 5.4 million
  inputs in 10 minutes and en_fuzz_galtoml 3.6 million in 5 minutes, with no finding. WHY§122's question (keep toml++
  behind the check, or a reader of our own for the subset) stands, and this round leans toward (b) for the player's
  files: every fuzzing round so far has found the next way in.

### 164. The layout's schema grows past ARCH §6.3's sketch: registers as data, our own labels for the classes, the game's names as values

`2026-11-16` · DECIDED · `#engine #gal` · refs: ARCH§6.2, ARCH§6.3, ARCH§6.9, WHY§123, WHY§149, WHY§159, O48

- **`register_abi` is a table of registers**, `{ task = "rcx", instance = "rdx", group = "r8" }` (three different ones
  of rcx, rdx, r8 and r9), not ARCH's `"v1"`. Why: WHY§123 found that the ABI must be data, and the derivation reports
  registers, so a variant name in code would need a table of the game's argument orders in code.
- **The game's names reach the code only through the layout.** A class may name the FD4 singleton behind it
  (`singleton = "Name"`); `[fmg]` takes `npc_names` and `place_names`, the tables in search order (ARCH §6.9's DLC
  order as data); patterns.toml's `fd4_idiom` takes `path_suffix` (ARCH §6.2 step 2's suffix as data). Why: as for
  the derivation (WHY§149), the code holds no name of the game's.
- **The classes the code reads are our own labels** (`characters`, `save_data`, `vitals`, ...; roles.hpp), not the
  game's class names, which appear only as `rtti` and `singleton` values: ARCH §6.3's class table keyed by a game class
  becomes `[class.character]` with `rtti = "<the class>"`. Fields are checked by kind (any whole-number width, a
  pointer, a bit, ...), so the layout, not the code, says what the game holds. The patterns' ids are the items' labels:
  `register_task`, `module_handle`, `save_data`, `game_state`, `field_area`, `fd4_idiom`.
- **Next**: fold into ARCH §6.3.

### 165. The SysLayer is narrower than ARCH §6.4's sketch, so that FakeSys tests the code that runs in game

`2026-11-16` · DECIDED · `#engine #gal` · refs: ARCH§6.4, WHY§159, WHY§160

- ARCH §6.4's `singleton()`, `rtti_vtables()` and `param_table()` are not the seam's: discovery and the readers are
  portable en-gal code over the image and the copy primitive the seam gives (WHY§159 lists what the seam keeps).
- **Why**: `FakeSys` then tests the very code that runs in game, in every build; only what needs Windows is left to
  `NativeSys` (WHY§160).
- **Next**: fold into ARCH §6.4.

### 166. The real game's layout is a fresh file; some of ARCH §6.1's fields are placed differently for now

`2026-11-16` · DECIDED · `#engine #gal` · refs: ARCH§6.1, ARCH§6.3, O89, WHY§164

- The real game's layout is a fresh file; the fake game's (`tests/fakegame/gal-layout.toml.in`) shows every class and
  field the groups read. The task's `object_size`, the `[selftest]` ids and the `[param.*]` rows are R1's.
- Placed differently for now, each a choice R1 and S1 confirm: `ride_state` is read from the player's class (ARCH puts
  it in a ride module); the clock is two fields, `hour` and `minute` (ARCH: one packed u64, whose packing is no fact we
  hold); `legacy`, `overworld` and the region are left to the core, from the block (regions.toml); the bearing uses the
  view matrix's third row as forward (S1 confirms the convention); `player_set_len` counts non-null entries (the
  chr_type filter waits for S1's values).

### 167. The task goes Dormant also when its own reading says the session is not solo

`2026-11-16` · DECIDED · `#engine #gal` · refs: ARCH§6.8, ARCH§3.2

- ARCH §6.8 says Dormant while the gate is false. The task cannot see the core's gate, so it also goes Dormant,
  reading only the session and the lifecycle, whenever its own reading says the session is not solo: a lobby other
  than None, a role other than Host, or not the player's own world.
- The core's gate stays the authority.

### 168. The render feed keeps the last walk's addresses between snapshots, checked again at every read

`2026-11-16` · DECIDED · `#engine #gal #perf` · refs: ARCH§6.6, ARCH§7.2, ARCH§7.4, R5, WHY§159

- The feed re-checks, every frame, the addresses the last snapshot's walk found (the character's vtable in the set and
  its handle unchanged) instead of walking the list every frame. Why: a walk every frame would not fit ARCH §7.2's
  0.03 ms for the feed.
- R5 says no instance pointer is kept across ticks; this keeps one between snapshots, for the feed alone, re-validated
  at every read, and a fault there is no group's fault.
- **Next**: a decision for the design, in ARCH §6.6 or §7.4.

### 169. What part 4 leaves unbuilt, and why

`2026-11-16` · DECIDED · `#engine #gal` · refs: ARCH§6.7, ARCH§6.9, ARCH§12.1, R1, R2, R3, R4, R14

- ARCH §6.9's text cache: at most 16 look-ups a tick do not need it yet.
- The C14 atomics in the session: they are the DLL's (part 6); the backend leaves them false.
- The stall detector: part 6's T1, or the launcher.
- The `boss_fight_active` cross-check beyond reading the flag.
- ARCH §6.7's "every npc_param_id resolves" and the flags' catalogue check in phase B: both need the core's catalogue
  or a large tick; the per-entry row look-ups are available through ResolveParams.
- The CI greps (R1's, R2's, R3's, R4's and R14's) are still to be written; the test functions of `test_native_sys.cpp`
  and the fake game's own callbacks would need the tests' folders left out.

### 170. The probe's slot search faults on purpose on the game thread: R1 without a crash reporter, or with the rpm copy

`2026-11-16` · METHOD · `#engine #gal` · refs: O89, S0-D, WHY§161, WHY§162

- **Seen**: the search reads arbitrary candidates (every 8-aligned value of the writable data sections that points
  outside the image) on the game thread under SEH: in the fake game 110 candidates, 123 copy faults caught and 3
  refused as stacks over three searches (WHY§162).
- **What follows**: that is a probe build's business (R1, at the title, O89), never a release's; but under a crash
  reporter's vectored handler (S0-D) those faults are seen first, so R1 should run without one, or with the `rpm` copy
  (WHY§161).

### 171. R0 refused the protected game's executable: the PE reader was stricter than the loader on sections

`2026-11-16` · FACT · `#engine #gal` · refs: O89, WHY§121

- **Seen**: probe-exe on the owner's executable (about 80 MB, protected) stopped with exit code 3, "a section is out
  of order, overlaps another, or runs past the image or file (at 0x400)". Behind it, besides the loader's own rules,
  the reader refused a section whose raw data ran past the end of the file, and one whose span, rounded to the section
  alignment, passed SizeOfImage unrounded. With the usual NT-header offsets, 0x400 is the entry of a section of index 7
  or more, not the first. Which rule the game breaks is not known: R0b's line and `--sections` will say, in numbers.
- **What follows** (engine branch, wip/pefix): the reader keeps only the loader's rules (virtual ranges aligned,
  ascending, disjoint and within SizeOfImage rounded to the alignment and to 4 KiB; no raw range that wraps). Raw data
  in any order, overlapping, shared, empty or past the end of the file is read as loaded, zeros past what the file
  holds. A refusal names the rule, the section's index and its numbers, never its name.
- **Checked**: 662 of 662 PE files of a fresh Wine prefix's system32 and 53 of our own binaries read. Two Wine DLLs
  had first been refused for exception-directory entries of no length (GNU toolchains emit them; the loader never
  reads the table); such entries are now left out. The tidy fake game derives byte for byte as with R0's tool.
- **Left as it was**: a section of no size is let through (the loader refuses one), an exception directory whose size
  is not whole entries is still refused, and PointerToRawData is read where it points; the loader is said to round it
  down to 512 bytes (LIKELY, from public notes on the format), which matters only for an offset off that boundary.

### 172. An early-loaded DLL's crash filter is replaced by the game's own C runtime when it starts

`2026-11-16` · FACT · `#engine #launcher` · refs: O71, S0-D, ARCH§3.2, ARCH§4.6, WHY§170

- **Seen** (the fake game, clang-cl, under Wine): galtest.dll injected into the suspended game installed its
  unhandled-exception filter on T1 at once; a crash on the game thread later reached no filter of ours, and the watcher
  had only the exit code. Loaded at the window or by the game's own loader, the same filter ran every time. The game's
  C runtime installs a filter of its own while it starts (MSVC's, which does not chain), after an early DLL's T1 has
  run.
- **What follows**: EldenNet's crash filter (part 6) is installed once the game has started, after WaitEngine (the
  statics are non-null), never in Pin or Safety; galtest does so before its registration. A game that replaces the
  filter later (the `launcher-crash-filter-replaced` case) still leaves the exit code and the last module list (5 s
  old at most): S0-D's row "beside the game's own handler" is that case on the real game.

### 173. The launcher's watch block: one page of shared memory, made by whoever comes first

`2026-11-16` · DECIDED · `#engine #launcher` · refs: O71, O90, ARCH§1.1, ARCH§2.3, ARCH§4.6

- **Chosen**: en-proto's `watch.hpp`, a 464-byte layout with fixed offsets (static_asserts in every build, since the
  launcher builds with MinGW and clang-cl and the DLL with clang-cl), named by the game's PID
  (`Local\EldenNet.Watch.<pid>`, two events beside it). The launcher makes it before it injects; under another loader
  EldenNet's watcher (`--watch <pid>`) makes it. EldenNet writes T1's state, its status (the core's phase, level and
  gate flags, discovery's counts, snapshots, faults, trips, p99) and, from its filter, the crash record; the launcher
  writes the spike, a crash request (probe builds) and the Marks. Numbers and one module file name only.
- **Why in en-proto**: the DLL may not depend on en-launcher and the launcher on nothing but en-gal's offline part
  (ARCH §2.3); en-proto is plain data both already reach (en-gal's offline part exposes it). ARCH §2.3's row for
  en-launcher should name en-proto (its watch block, `ffi_guard`) and nlohmann/json and toml++ (its records and
  settings), and drop BLAKE3's C implementation (WHY§174).
- **Rejected**: a pipe (needs a reader thread on both sides and a protocol), a file (no wake-up, and a crash leaves it
  half written), the DLL writing the record itself (it must not allocate or do I/O in its filter).

### 174. BLAKE3's hash mode is our own, against its published vectors

`2026-11-16` · DECIDED · `#engine #launcher` · refs: ARCH§2.3, ARCH§4.4, O84

- **Chosen**: the backup manifests and their de-duplication need a 32-byte hash of a file; `en_launcher_core`'s
  `blake3.cpp` (about 150 lines, from the specification: the compression function, 1,024-byte chunks, the tree of
  chaining values) instead of BLAKE3's C implementation, which ARCH §2.3 names. The C sources would have added a C
  compiler to every build (the MinGW toolchain file names none, and the sanitizer and fuzz builds set only Clang for
  C++), a third-party compiled target and its notices, for one function.
- **Checked**: 16 of the official test vectors (0 to 102,400 bytes, every chunk and tree boundary), each fed whole and
  in pieces of 1, 63, 64, 1,000, 1,024 and 4,097 bytes. en-core's keyed hashes (the playthrough key's fp8, the
  recordings' salts, DESIGN §10.5, ARCH §7.5) may extend it (keyed and derive-key modes are a few lines more) or adopt
  the C implementation then.

### 175. The gate's badge after a loading screen, and the gate when snapshots stop

`2026-11-16` · DECIDED · `#engine #core` · refs: ARCH§5.1, ARCH§5.4, ARCH§6.8, DESIGN§2.7

- **Seen**: ARCH §5.1's predicate is not clean during a loading screen (neither in a world nor at the title), so the
  gate closes at every loading screen and holds its 5 s again after it. Shown as written, "EldenNet paused
  (multiplayer)" would follow every fast travel for 5 s.
- **Chosen**: the badge is up only while in a world with the gate closed for a reason of the session (`not_solo`:
  role, world, lobby or player set), until the gate has held again; a loading screen alone never raises it. The gate
  itself is as ARCH §5.1 says (Dormant through the hold; speech returns 5 s after each loading screen). If that delay
  is unwanted, a design change would exempt the hold after a loading screen whose session fields were clean before it.
- **Left as it was**: the gate changes only on snapshots. When the task stops sending them (a hang), it stays as it
  was; the stall detector (part 6) is the place to act, not the gate.

### 176. The core's slice decides a playthrough from PlaythroughReady, and the store's step is a no-op for now

`2026-11-16` · DECIDED · `#engine #core` · refs: ARCH§9, DESIGN§10.5

- **Chosen**: a playthrough is identified by PlaythroughReady's slot, name, class and keepsake (equality, no hash: the
  store's fp8 is its file name). Title → Playthrough needs it (slot 0–9, a name, the snapshot's slot), phase B on the
  structural groups and the play time rising since the previous snapshot. Loading → InWorld needs the world on two
  consecutive snapshots with the same load counter, and the same character: a newer PlaythroughReady's identity, or,
  without one, the same slot and class; another identity is a new playthrough. Without the store, Playthrough →
  InWorld happens on the same step (the store's `Opened` will gate it). The title ends a playthrough: the same
  character again is a new one (the store reopens its database).
- **Rebuilt from snapshots** when C2 loses a message: DataReady (structural groups Ok in the Gate phase) and a
  structural trip (Tripped in a snapshot's health). A later anti-cheat hit in a snapshot is L1 with its banner, for the
  session.

### 177. The launcher's late loads, closing the game, and what it checks of a DLL

`2026-11-16` · DECIDED · `#engine #launcher` · refs: ARCH§3.3, ARCH§4.3, ARCH§4.6, O89, O90, O91

- `--inject-at title` cannot see the title from outside (the launcher never reads the game's memory, O73): it waits
  `--title-wait` seconds (15 by default) after the window. S0-A's late mode should set it to what the owner's game needs
  to reach its title; the run's report says where T1 found the game.
- A spike's run closes the game with `WM_CLOSE` to its windows, then `TerminateProcess` with "ENLC" (0x454E4C43) after
  10 s, which the records call `closed`, never a crash.
- LoadLibraryW's thread exit code is the module handle's low half, 0 for a module at a 4 GiB boundary: the game's
  module list decides whether the DLL loaded; a 0 only names the failed step.
- A failed late load ends the game too (E10 says "The game was closed again"), as an early one does.
- The development options (`--inject-at`, `--also`, `--console`, `--spike`, `--gal`, `--steam`, `--appdata`,
  `--no-steam-check`, `--no-dialogs`) are in every build, listed apart; ARCH §2.5 has no launcher option to compile
  them out, and none changes anything unless given.

### 178. Wine's module list of another process and the watcher's timing

`2026-11-16` · FACT · `#engine #launcher #wine` · refs: O71, WHY§102

- **Seen**: under Wine 9.0 `CreateToolhelp32Snapshot(TH32CS_SNAPMODULE)` of the game lists its modules (26 with the
  test DLL), while the game waits in EldenNet's crash filter too. A bug of ours had listed again after the crash (an
  unsigned difference of times taken out of order), with an older, shorter list; fixed. Every module name is reduced to
  its file name and to letters, digits and `._-+ `.
- **Seen**: a process the case starts outside the harness's own (the watcher galtest starts) outlives the game by a
  moment; the harness waits for the prefix's processes (`wineserver -w`) before it stops the server, or the watcher's
  record would be cut off.

### 179. Once, the fake game ended with 0xC0000409 after its last line, with galtest loaded at the window

`2026-11-16` · SPECULATION · `#engine #launcher #wine` · refs: O71, S0-D

- **Seen once** in about 30 injected runs (clang-cl, Wine 9.0): `launcher-inject-window`'s fake game printed its end
  line (720 frames, exit 0) and its process then ended with 0xC0000409 (a fast fail, past every filter); the watcher
  recorded it as a crash from the exit code alone, as it should. Not seen again in 12 reruns of the injection cases.
- **Guess**: the process's exit racing galtest's T1, which now allocates every 100 ms (en-core's step); a fast fail
  needs `__fastfail` or a /GS check, so it is not a lock left by a killed thread. **What follows**: EldenNet's T1 and
  core thread (part 6) should allocate nothing once idle; S0-B's two hours and S0-D's records will show whether a real
  game's exit does the same.

### 180. R0's FD4 finder refused every hit: it took the first read-only load after the idiom for the path

`2026-11-16` · FACT · `#engine #gal` · refs: O89, ARCH§6.2, ER01§4, WHY§171

- **Seen**: R0's report derived an idiom from thousands of sites (the head only: the static's load, its test, the
  jump, the class object's load and the getter's call, with the registers' bytes wildcarded) but accepted no hit: most
  were refused because "the path does not end with the suffix". The sites differ after the getter's call, so the idiom
  has no path capture, and the finder took the first RIP-relative load into `.rdata` in the block the jump skips for
  the path, whatever its text. The derivation had itself seen each site's path reference inside that block, so another
  read-only load comes first.
- **Likely**: it is the log's format string, which ER01 §4 places before the path (`lea r8, [fmt]` before `lea rcx,
  [path]`). Either way the rule was the kit's mistake: it ignored the suffix it then checked.
- **What follows** (R0c): the path is the load, of every load in the block, whose string ends with the suffix:
  whichever comes first, in any register, before or after the getter's call; a RIP-relative address, a RIP-relative
  pointer to the string, or a 64-bit absolute address; else a load hoisted before the head, within the function. Each
  accepted hit's load is reported (place, length, the read-only loads before it, register, opcode), and R0's rule is
  run beside it over the same hits, so R0c's report shows both. The review's record: `docs/ops/probes/2026-11-16-r0.md`.

### 181. The idiom's wildcards also match loads that are no RIP-relative statics: every hit is decoded now

`2026-11-16` · LIKELY · `#engine #gal` · refs: O89, ARCH§6.2

- **Seen**: a few dozen of R0's hits were refused because their "static" lay outside `.data` or outside the image.
- **Likely**: the idiom wildcards the static's REX and ModRM bytes (the sites use several registers), so it also
  matches `mov reg, [base+disp32]`, whose displacement is no RIP-relative address.
- **What follows**: `fd4::find` decodes each hit and refuses one whose static, class object or path is not a
  RIP-relative operand, or whose getter is no relative call, where the captures say (counted as `unreadable`). It
  applies in game too, where the same finder reads patterns.toml's `fd4_idiom`.

### 182. An idiom has as many hits as the game has inlined accessors: the scan's cap of 4,096 was too low

`2026-11-16` · FACT · `#engine #gal` · refs: O89, ARCH§6.2, ARCH§6.3

- **Seen**: part 2's scan, which checks the idiom as patterns.toml would hold it, refused R0's idiom unread: its hits
  passed the cap every pattern had (4,096 hits kept, and at most that many expected).
- **What follows**: an idiom may expect, and the scan keeps, up to 65,536 hits; every other kind keeps 4,096. In game
  this changes only the scan's verdict on the idiom: the FD4 finder always read up to 65,536.

### 183. Accepted FD4 sites without a name layout leave the singletons to R1's getter calls

`2026-11-16` · DECIDED · `#engine #gal` · refs: O89, ARCH§6.2

- **Decided**: the report's `fd4.status` now says whether the idiom was derived (its sites all match, the scan
  resolves it, the finder accepts a hit), and the name layout is reported apart. Where hits are accepted but no name
  layout reads, each singleton is `depends`, and the confirm list asks R1 for the name layout without a value: in game
  each singleton is named through its getter once (ARCH §6.2 step 3), as the design already allows.
- **Why**: whether the runtime class objects hold their names in the file (relocated pointers) or only once the game
  has run (zeros past the file's raw data) is not known. R0c's report counts both.

### 184. A table of one is no table: the group's index from a lone pointer was an artefact

`2026-11-16` · FACT · `#engine #gal` · refs: O89, ARCH§6.3, ER01§3.2

- **Seen**: R0 found one pointer to the task group's name and grew a "table" of one entry from it, index 0, which the
  confirm list passed on as the index to confirm.
- **Speculation**: the pointer is a field of a table whose entries are wider than a pointer (an index or flags beside
  the name), so its neighbours eight bytes away are no name pointers.
- **What follows**: tables are now looked for at strides of 8 to 64 bytes, the longest run kept, cut where code
  addresses an entry's start (which gives the name pointer's offset in the entry), with an index field the entries may
  hold; a lone pointer is listed last and gives no index, and the group is `derived` only from a table of two entries
  or more, or from code.

### 185. A static whose class has no RTTI cannot be found offline; R1 finds its slot

`2026-11-16` · LIKELY · `#engine #gal` · refs: O89, ARCH§6.2, ARCH§6.3

- **Seen**: R0 found no type descriptor for `CS::GameDataMan` (as a class or a struct), and the confirm list offered R1
  a vtable of zero.
- **Reasoning**: offline, such a static is a slot of `.data` that holds zero in the file; the code that reads it names
  nothing; RTTI is the only name a file gives an instance's class, and a lead may carry a name or a shape, never bytes
  (ARCH §6.3). So no rule of ours can single it out offline.
- **What follows**: R0c counts type descriptors of the class's own name in any namespace (a wrong namespace would show
  there). If there are none, R1 finds the slot in game by what the instance holds, and `probe-exe --static` cuts the
  pattern from it offline. The confirm list no longer offers a vtable that does not exist.

### 186. Product names are compared without symbols, in the derivation and at the gate

`2026-11-16` · DECIDED · `#engine #gal #datapiler` · refs: O89, ARCH§6.3, DataPiler DP36

- **Seen**: R0 reported the product name `differs` from the leads' (an exact comparison).
- **Likely**: a trademark sign after the name.
- **Decided**: both the derivation and `gal/builds.toml`'s gate compare names once symbols are removed (the
  trademark, registered and copyright signs and their like, zero-width characters; spaces made one; ASCII and every
  other letter kept). The report keeps schema 1's word `matches`, with the name's characters and those kept beside it:
  the word `matches_without_symbols` would make DataPiler 0.2.41's guard refuse the whole report.

### 187. The bytes R0 could not decode are no gap of the decoder's

`2026-11-16` · METHOD · `#engine #gal` · refs: O89, ARCH§6.2, WHY§148

- **Checked**: en-gal's decoder against LLVM 18's disassembler, with the repository's own harness, over Microsoft's
  C++ runtime libraries from the pinned SDK (the game's compiler's own code) and over code compiled for AVX2 and
  AVX-512: no disagreement. The only ones met anywhere were APX's REX2 prefix and data, in a Linux crypto library.
- **Likely**: what R0 stepped over is data in `.text` (MSVC puts switch tables in the code section) and perhaps a
  protection layer's code; it matters to the finder only where decoding from a function's entry does not land on a
  site, which R0c counts per reference, beside a census of every executable section (runs, where they start, bytes
  inside functions).

### 188. derive.json's schema 1 is frozen by DataPiler 0.2.41's copy of it

`2026-11-16` · DECIDED · `#engine #gal #datapiler` · refs: O89, DataPiler DP36

- **Seen**: DataPiler 0.2.41's guard holds its own copy of schema 1's keys and words and refuses any other.
- **Decided**: R0c's diagnostics add no key and no word: they nest schema 1's keys under `stats` (and a few beside the
  results), each place's meaning listed in en-tools' README; a unit test pins the vocabulary to DataPiler 0.2.41's,
  key for key and word for word. A new key needs a schema 2 and a DataPiler that reads it.

### 189. The executable has a second code section the derivation does not read

`2026-11-16` · SPECULATION · `#engine #gal` · refs: O89, WHY§171

- **Seen**: the section table holds a second section named `.text`, after the import table, and another executable
  section of another name; the derivation reads only the first `.text`.
- **Speculation**: a protection layer's code. R0c's census decodes every executable section and counts references to
  the FD4 paths from each, so the next report says whether anything of ours lives there.

### 190. A stack range outlives its thread: the reader refused the game's own objects where a stack had been

`2026-11-16` · FACT · `#engine #gal #wine` · refs: WHY§111, WHY§160, ARCH§6.5, R4

- **Seen** (eldennet.dll in the fake game, clang-cl, Wine 9.0): the world group tripped after three snapshots, every
  one with a read of the near list refused as a stack. NativeSys's table, made from every allocation holding a guard
  page, still held two ranges whose threads had ended: the launcher's loading thread (Wine frees an ended thread's stack
  late, so it was still mapped at the first scan and at a rescan just before registration) and T1, which ends at Ready.
  The game's next allocations (its world's objects, made at the first world entry) were placed in that memory. galtest
  never showed it: its T1 never ends.
- **Chosen**: NativeSys::rescan_stacks on T1 just before the one call (the table made again, without T1's own stack,
  which the game never points into); and on the rare path of a refused read, forget_stale_stacks checks each
  overlapping range again with VirtualQuery and drops one whose allocation no longer starts there or holds no guard
  page, and the read goes on (counted: `stale_stacks` in the DLL's reads line; 1 in the fake game's runs).
  Allocation-free, on the game thread only after registration, so the table has one writer.
- **What follows**: the real game's threads come and go (pools, loaders), so without this the structural groups could
  trip in S0-B's two hours; its counter is worth watching there. A periodic rescan from another thread would need a
  double-buffered table; the lazy check needs none.

### 191. eldennet.dll's start as built, where it departs from ARCH §3.2

`2026-11-16` · DECIDED · `#engine #dll` · refs: ARCH§1.4, ARCH§1.6, ARCH§3.2, R16, WHY§172

- R16's mutex is taken at Pin, not in Safety: a second copy loaded by the game's own loader a moment after the
  launcher's copy (the `launcher-eldennet-twice` case) could otherwise overtake the first while it gates. The second
  copy still writes nothing but one OutputDebugStringW line.
- Headless: Connect and ConnectB are logged as skipped (`overlay.connect` stays off); the banner reaches the core as a
  BootBanner and the status block, and nothing is drawn.
- The crash filter goes in after WaitEngine (main path) or after WaitWindow (banner path), never earlier (WHY§172):
  the game's window comes after its C runtime's start.
- Discover runs before WaitEngine, as galtest did, and ARCH's ReadyScan is not a separate pass: one discovery pass finds
  the readiness items with the rest, then WaitEngine polls the hub's instance. FD4 names in class objects the game fills
  at run time make the retries at +5, +15, +30 s the readiness wait for the names (ARCH §3.2's WaitEngine also asks the
  DLRF name to be readable).
- T1 ends at Ready (ARCH: Ready → Exit), which is what exposed WHY§190; in R1 it stays as the probe's reporter.
- C14's atomics are applied by the core thread to each snapshot's session before the core judges it, not by the task
  (ARCH §1.3 has them read by the task; the task's backend leaves them false, WHY§169).
- *Later (2026-11-20):* the Headless bullet no longer holds. Part 12 connects to GraceUI at Connect and ConnectB
  (`overlay.connect` on by default), with the banner as a notice and the F11 view behind `debug.panel` (WHY§215 to §221).

### 192. The recorder writes numbers-only JSON Lines, each line a report of the kind "recording"

`2026-11-16` · DECIDED · `#engine #dll #datapiler` · refs: ARCH§7.5, O73, S10.1.a plan §1.1, §1.3

- **Chosen**: each record is one line, `{"report":"recording","schema":1,"t":<mono ms>,"k":<kind>,...}`, written through
  en-gal's closed-vocabulary writer on one line (`Style::Line`, a `Float` type for positions, distances and bearings),
  so `eldennet-engine lint --recording` and DataPiler's guard check every line by the same rules as derive.json. The
  header is a `k = "header"` record (game version as four numbers, region, EldenNet's version, the spike, probe build,
  the file's number); ARCH §7.5's text-valued header fields (`build`, `content_hash`, `seed_root`, `salt`) wait for
  the keyed hashes. Plain JSON Lines, no zstd (plan §1.3: DataPiler's Python 3.10).
- **Left out by construction**: text look-ups' answers (table, id and found only), param rows' values (param, id,
  field count), the character's name (its length), and SteamID (not read). A NaN bearing is left out.
- **Cost**: about 3.3 KB a snapshot line with the fake game's six characters, so about 47 MB an hour at 4 Hz unzipped
  (ARCH §7.5's 4 MB is zstd's); a full snapshot (32 characters, every flag of 2,048 set) stays within a line's 256 KiB
  (en_shell_tests).

### 193. R1 in the DLL: the derivation over the live image, then one call at the Mark, then the game thread's searches

`2026-11-16` · DECIDED · `#engine #gal #dll #r1` · refs: O89, O90, S10.1.a plan §2, R0c, ARCH§6.2, ARCH§6.10, WHY§170

- **Chosen**: R1 reuses part 3's derivation over the mapped image (ImageLayout::Memory): the FD4 class objects the game
  fills at run time name the 217 singletons, which gives the hub's static and so `register_task`'s candidates and ABI
  from the game's own call sites; the group table is read as loaded. Its report is derive.json's schema 1
  (`r1-derive.json`), which DataPiler 0.2.41 already reads; what derive.json has no keys for goes in a second report,
  `r1-probe.json` (kind `probe`).
- **The call** only once the gate, the name layout, the registration (a function, three distinct agreeing registers),
  the group index (in the loaded table, and the draft layout's where there is one) and the hub's instance are confirmed,
  and at the first Mark (the owner's Pause at the title with no save loaded: no reading of the game's memory from T1
  can tell the title). The run slot is not known before it, so every slot of the fake vtable counts and runs the body
  for our own object (TaskShape::every_slot); slots called on T1 inside the call are counted and do nothing. The object
  is `probe.object_size` (0x800) zeroed bytes, and R1 reports how many the hub wrote.
- **The base class without a lead**: the hub census follows the hub's instance to the objects it points at (heap and
  the image's writable data, to a depth of three) and keeps their vtables; the type descriptors their classes list
  among their bases, by count, are the candidates, each with task_from_base's slots and run slot. In the fake game (its
  leads' base replaced by a marker, as the real game's are) the first candidate is the task base, with run slot 1.
- **GameDataMan without RTTI**: the rising-counter search over every heap static of the image (up to 65,536); its rule
  for a static's slot is one heap static with a field that rose at least twice and stood still at least once (a play
  time; a process clock never stands still). A field is followed from its first rise, so its still passes must come
  after it: R1's fast travel should give them if the game's play time stops during a loading screen (LIKELY, not
  known). If it never stops, no slot is taken automatically, and the hits (slot, offset, rose, still, other) are the
  reviewer's to read.
- **Left for the reviewer and the next round**: the layout's fields (the probe's object searches run on instances
  r1-derive.json gives), and `dlrf_name_layout`, `register_abi`, the group index, the task's shape and the statics'
  slots into gal/ from the two reports.

### 194. settings.toml for v0.1: strict, ARCH §11.5's later tables accepted, three tables of this build's own

`2026-11-16` · DECIDED · `#engine #dll #settings` · refs: ARCH§11.5, S10.1.a plan §1.1

- Read: `schema` (1 or 2), `enabled`, `debug.record_sessions`, `debug.log_level`; `overlay.connect` (noted and kept off
  in this build), `gal.copy` (`seh` or `rpm`, S0-D), and in a probe build `probe` (`round`, `register`, `object_size`,
  `settle_s`, `fault_group`, `fault_every`, `fault_after_s`). ARCH §11.5's tables for later increments are accepted as
  tables, unread, so that a full file is not refused; any other key, a wrong type or a value out of range refuses the
  whole file (the defaults, with each problem's place in the log). `probe` in a release build is refused.
- `ELDENNET_APPDATA` in the environment stands for %APPDATA% (the launcher's `--appdata` for the DLL): only where the
  DLL's files go, never what it reads of the game.

### 195. The DLL's imports, exports and greps

`2026-11-16` · FACT · `#engine #dll #ci` · refs: ARCH§2.4, ARCH§3.1, ARCH§10.1, ARCH§12.1, R3, R12, R14

- Debug and Release: one export, `eldennet_instance_marker`; imports from kernel32 (the C runtime's and ours) and user32
  (EnumWindows, GetWindow, GetWindowThreadProcessId, IsWindowVisible). No network, no hook API, nothing into another
  process. `imports.allow` is the union, reviewed.
- The eldennet module holds no cast: the EnumWindows search keeps its answer in a thread_local, not behind an LPARAM,
  and DllMain's start, the pin and the crash filter's module look-up are en-gal's `sys_boot.hpp`.

### 196. Under Wine, a stack overflow passed on by any DLL's crash filter hangs the process

`2026-11-16` · FACT · `#engine #dll #wine #launcher` · refs: O71, S0-D, WHY§172, WHY§179

- **Seen** (clang-cl, Wine 9.0, the launcher watching): the fake game's `--crash stack-overflow` on its game thread,
  with eldennet.dll or galtest.dll loaded and its filter installed, never ended (killed after 180 s), even with a
  filter that returned EXCEPTION_CONTINUE_SEARCH at once without doing anything. With no filter of a DLL's (the
  launcher's vanilla start, or the DLL loaded by the game itself with no watch block, so no filter), Wine printed
  "Unhandled stack overflow ... starting debugger" and the process ended at once (exit 1). An access violation passed on
  by the same filter ends as it should (the `launcher-*crash` cases).
- **Chosen**: for EXCEPTION_STACK_OVERFLOW alone, EldenNet's filter writes its record (without the module look-up),
  wakes the watcher, waits for it at most 2 s, then ends the process with TerminateProcess and the overflow's code; it
  does not pass it on. Nothing after it could have run on that stack anyway; the cost on Windows is that WER and a
  crash reporter's filter before ours do not see a stack overflow. The `launcher-eldennet-overflow` case checks it.
- **Open**: whether Windows does the same (S0-D's crash rows on the owner's PC); galtest's filter (part 7's) still
  passes it on.

### 197. R0c into gal/: what became a lead, where it lives, and a lead [task] that holds only its group

`2026-11-17` · DECIDED · `#engine #gal #r1` · refs: O89, ARCH§6.3, R0c, WHY§180, WHY§183, WHY§184, WHY§185

- **Chosen**: `builds.toml` holds R0c's fingerprint for 2.7.2.0 WW (App 1.17.2, DESIGN's header), `dlrf_name_layout`
  false; `patterns.toml` holds the FD4 idiom (24 bytes, 7 fixed; captures static at 3, class at 15, getter at 20;
  `expect_matches = [7151, 7151]`, R0c's count in the first `.text`) with no `builds` (confirmed nowhere), and its
  note `derivations/fd4_idiom.md`; `layouts/2.7.2.0-ww.toml` holds a lead `[task]` with only the group's name and
  index 110 (R0c's table of 168 entries of 32 bytes, UTF-16 names at +16; no field of the entries holds an index).
  Everything else (register_task and its ABI, the task's shape, the statics, the classes) is left out, not guessed.
- **The schema**: a confirmed `[task]` keeps every key; a lead may hold only its group, and is `partial` then:
  discovery is never ready with it, `install_task` refuses it, the first tick reads nothing. ARCH §6.3's sketch
  wants `register_abi`, `vtable_slots`, `run_slot` and `object_size` everywhere, which would have forced placeholders.
- **The table's shape** (entries, stride, name offset, encoding) has no home in the layout's schema: it is in the
  layout's comments and the derivation's record; R1's r1-derive.json reports it again as loaded.
- `lint --gal` checks the folder (each file as its reader reads it, a note per pattern, confirmed builds known);
  ARCH §6.3's O48 check (no byte string quoted in docs/research/) needs the design branch beside the engine, and waits.

### 198. R1 calls only on the reviewed leads; its group check had never compared

`2026-11-17` · FACT · `#engine #dll #r1` · refs: O89, WHY§193, S10.1.a plan §2

- **Found**: part 6's R1 read the draft layout's `[task] group_index`, a key the layout's schema never had (it is
  `[task] group = { name, index }`), so the group's index was never compared with the layout's; the fake game's R1
  case passed with the check `confirmed` on the loaded table alone.
- **Chosen**: the draft layout is read with `load_layout` in a dev build's mode, and its group's index must equal
  the loaded table's; an unreadable or refused layout fails the check. And a new check, `fd4_idiom`: the idiom the
  live image gives must be patterns.toml's, byte for byte and capture for capture, or no call; the live hits are
  reported beside R0c's count and do not decide. So the one call rests only on derivations a review has seen (O89
  (a)): the fingerprint (the gate), the idiom, the group's index.
- **Risk accepted**: if Arxan's image in memory differs from the file where the idiom is cut, R1 refuses and the
  owner's round costs one more run; that is the safe side.

### 199. The launcher's --probe asks through the watch block; a refusal stops the DLL

`2026-11-17` · DECIDED · `#engine #launcher #dll #r1` · refs: O89, O90, WHY§173, WHY§194, S10.1.a plan §1.2

- **Chosen**: `--probe` writes R1 into the block's `control.probe` (its spare field, the block's version kept: a 0
  is "no request") before loading anything, and EldenNet answers there at Settings: a probe build accepts and runs R1
  with the file's other probe settings, `register = "auto"` becoming `"mark"`; a release build, or EldenNet off,
  refuses and ends T1 (Exit: no normal start, nothing called). No settings.toml is written, so nothing outlives the
  session. `--probe` brings `--console`, refuses `--no-backup`, `--vanilla-offline` and `--spike`, and makes a failed
  backup fatal (no "continue without a backup").
- **The console** tells each step once in plain words (`EldenNet R1: ...`, console only; the log keeps `r1
  step=<word>`), and the launcher lints both reports when the game ends, with the vocabularies `lint --report` uses.
- **Not chosen**: a launcher-written settings.toml (it would stay for the next start), or the launcher reading the
  DLL's version resource to tell a probe build (the block's answer says it, and covers EldenNet turned off).

### 200. The console polls every 200 ms: a quick step passes it unseen in a Release build

`2026-11-17` · FACT · `#engine #launcher #wine` · refs: WHY§199

- **Seen** (the R1 package's own binaries, clang-cl Release, on the fake game under Wine): the console showed
  `asked`, `press_mark`, `task_confirmed` and `reports_clean`, but not `reading` (the live derivation took under 200
  ms) or `called` (the first tick came within one poll); and the live derivation's second pass, once the statics'
  slots were found, logged after `task_confirmed`. The Debug build showed every step.
- **Chosen**: the harness requires the essential steps in order and allows the two quick ones to be missing. On the
  real game the derivation takes seconds, so `reading` shows; `called` may not, and need not.

### 201. spike-report reads DESIGN §16.1's pass rules so

`2026-11-17` · DECIDED · `#engine #spike` · refs: DESIGN§16.1, O91, S10.1.a plan §1.2

- S0-A: per load mode (`suspended` early; `window` or `title` late), at least 20 runs (`--need`), each Ready or L1
  (a banner or inert), none a crash; `both_modes` is `not_measured`, not failed, until both loads have run. The run's
  mode is now in the launcher's spike log (older logs read `unknown`).
- S0-B: at least 7,200 s of snapshots, no fault (a snapshot's `faults` are its own tick's, summed), no group tripped,
  the C13 p99 at the session's end at most 500 µs, no crash.
- S0-C: "Gate false in every session" read as the gate never left open on a snapshot of a session that is not one's
  own solo world (the core closes it on the first such snapshot, so its change line follows that snapshot's);
  connections beyond the PC at most 0; "no online menu" the owner's to observe, never measured, and not counted
  against the verdict. **Open**: if the design meant the game's own online flag, the rule changes.
- S0-D: "20/20 caught" read as at least 20 forced faults counted with no crash; a breaker tripped; every s0d-crash
  run's crash recorded by the watcher.
- The verdict: `pass` when every measured rule passes, `incomplete` when one is not measured, else `fail`.

### 202. Replay agrees: en-core's slice answers the same changes on a recording's inputs

`2026-11-17` · FACT · `#engine #core #spike` · refs: ARCH§7.5, ARCH§12, WHY§192

- **Seen**: on the DLL's own recording in the harness (clang-cl, the fake game, `launcher-eldennet`) and on four
  invented sessions written as the DLL's core thread writes them, `replay` stepped the recorded lifecycle messages,
  banners and snapshots through `en::core::Core` again and answered the recorded changes exactly (what, from, to), in
  order; one change line removed makes it disagree at that place.
- **How**: inputs in their recorded order, each stepped at the latest time seen (a snapshot's line carries its own
  `mono_ms`, the others the step's time; the core's Tick does nothing yet, and a snapshot is judged by its own time);
  a playthrough's name, never recorded, is a stand-in of its recorded length. If a later core reads more of a
  snapshot than the recording keeps, replay will say so by disagreeing.

### 203. The R1 package, 0.1.0-spike.1

`2026-11-17` · METHOD · `#engine #r1 #spike` · refs: O89, S10.1.a plan §1.2

- `EldenNet-R1.zip` (`tools/spikepkg/build.py r1`, part 10): the clang-cl Release launcher, the probe DLL and
  eldennet-engine.exe at the label `0.1.0-spike.1`, gal/ byte for byte as `lint --gal` passed it, README-R1.txt, the
  licences and SHA256SUMS. Checked: the sums; the help and version under Wine; the owner's steps on the fake game
  with the package's own binaries (the harness's `launcher-eldennet-probe`, PASS); the privacy sweep (clean).
- No `fakegame.exe` and no `--selftest` in it: the smoke run (plan §1.2, risk 3) is the S0 package's, spike.2.

### 204. C1's slot could answer an older snapshot after a newer one

`2026-11-17` · FACT · `#engine #dll` · refs: ARCH§1.3, WHY§192, R6

- **Seen**: en_shell_tests' two-thread case of `SnapSlot` failed about once in four runs on a loaded machine (GCC),
  a snapshot taken whose `seq` was not above the last one's. Part 6's `take()` chose the newest Ready slot by its
  order at the scan, then claimed it; between the two the writer could overwrite that slot and publish it again,
  newer than the other slot it had left Ready, and `take()`, still judging the other by the scan's order, kept it,
  so the next take answered it: older. In game the core would have seen the snapshots' clock go back once (its gate
  starts its 5 s hold again), and a snapshot taken out of order.
- **Fixed**: the chosen slot's order is read once it is claimed (the writer cannot touch it then), and the other is
  claimed before its order is read, so a slot the writer has just published again is never freed unread. 60 runs of
  the case pass; the DLL's harness cases pass on the fixed build.

### 205. Under Wine, GetKeyState from the window procedure's filter costs about 100 µs a call: GraceUI reads the modifiers only for a fresh press of a bound key

`2026-11-17` · FACT · `#graceui` `#input` `#perf` · refs: GraceUI:D155, WHY§126, WHY§133

- **The cost** (FACT, Wine 9.0 on Xvfb): `GetKeyState` called from GraceUI's window-procedure filter takes about 100 µs
  a call, sometimes more. Reading Ctrl, Shift and Alt on every key message, for the hotkeys brought into 0.1 (D155),
  raised the input-cost check's p99 with a holder from about 1.5 µs to 130–250 µs, against its 50 µs budget.
- **The way round** (DECIDED, GraceUI D155): the filter reads the modifiers only for a fresh press of a key that some
  hotkey binds, and only while no holder has input; the p99 is back at about 1.5 µs.
- **For S4** (METHOD): measure `GetKeyState`'s cost on Windows, where it is expected to be far cheaper.

### 206. R1 run 1 on the owner's PC: lld-link's merged manifest stopped the launcher on Windows (spike.2 fixed it); then the live derivation named 9 of 217 FD4 class objects, so no call was made; spike.3 reads namespaced names and reports where the name pointers lead

`2026-11-17` · FACT · `#gal` `#r1` `#derive` `#launcher` · refs: O89, WHY§180, WHY§189, engine 92b368b

- **The manifest** (FACT): package 0.1.0-spike.1's launcher did not start on Windows ("side-by-side configuration is
  incorrect"). lld-link, given `launcher.manifest` as a source, merged it with its own and wrote the trustInfo with
  namespaced attributes (`ms_asmv1:level`), which Windows refuses and Wine accepts. Since spike.2 the resource script
  embeds the file as written and lld-link makes none (`/MANIFEST:NO`); the packager refuses a merged manifest.
- **The run** (FACT, spike.2, game 2.7.2.0 ww): the gate passed; the live image gave R0's FD4 idiom byte for byte
  (7,151 hits, as expected), 7,081 accepted sites over 217 class objects, and the task group at index 110, as R0 found.
  The name search named 9 of the 217 (offset 0x68, ASCII into `.rdata`) over four passes from 17 to 77 s, under the
  half it needs, so every singleton stayed unnamed and the probe refused the call, as designed.
- **Why** (LIKELY, untested): the search took a name only as a plain identifier in `.rdata`; a class name joined to its
  namespace by `::` would be refused, and one held outside the image (the heap) is not read at all.
- **Spike.3** (DECIDED): names joined by `::` count, and a lead matches a name exactly or as its last part, once only
  (two such names: ambiguous). `r1-probe.json` gains `names`, per offset 0 to 0x78: where the value points (null,
  `.rdata`, `.data`, code, other, outside the image) and what text it reaches, numbers only. If the names prove to be on
  the heap, the next step is a guarded read of them in the probe.

### 207. R1 run 2: the game's FD4 class objects hold no names in the image (their words point to the heap), so R1 names the singletons by their instances' RTTI (O92, spike.4)

`2026-11-17` · FACT · `#gal` `#r1` `#derive` · refs: O92, WHY§206, engine 7840217

- **The run** (FACT, spike.3, game 2.7.2.0 ww): as run 1, the idiom, the 7,081 sites and the group at 110 were
  confirmed, and the probe refused the call: 9 of 217 class objects named, none of them qualified by a namespace.
- **The numbers** (FACT, `r1-probe.json`'s `names`): at every offset from 0 to 0x78, 48 to 147 of the 217 words were
  null and 52 to 153 pointed outside the image; at most 13 pointed into `.rdata`, 20 into `.data` and 3 into code, and
  none of the objects held a name inline. Offset 0 held no pointer into the image at all (127 null, 90 outside), so the
  objects carry no vtable of the image's there either. The names, if held, are reached through the heap.
- **So** (DECIDED, O92): spike.4 names a singleton by its instance's class: the static's instance (live `.data`), its
  first word read once through the crash-guarded copy with ARCH §6.5's plausibility rules (on T1, before any call), and
  that vtable's class from the RTTI walk; a lead matches its name or its name with Imp, once only. The probe's report
  counts the statics tried, null, unread, unclassed, classed and matched (`inst_*`).

### 208. R1 run 3: CSTask named by its instance's RTTI, the checks passed and register_task returned ok at the title screen, but the task never ticked there; run 4 loads a save after the call (owner's choice)

`2026-11-17` · FACT · `#gal` `#r1` `#task` · refs: O89, O92, WHY§207, engine cab98b7

- **The run** (FACT, spike.4, game 2.7.2.0 ww): one derivation pass at 15 s named three of the eleven singletons by
  their instances' RTTI (CSTask, CSCamera and SoloParamRepository; 219 statics tried: 98 null, 2 unread, 35 of no
  known class, 84 classed). The registration's function and its three argument registers (task, instance, group in r8,
  rcx, rdx) and the group's index 110 were confirmed, and every check before the call passed.
- **The call** (FACT): at the owner's Pause, 46 s after the checks, `register_task` returned ok. The game ran on for 78
  s at the title screen and exited cleanly; the task was never invoked (no first tick, no slot called, nothing of our
  object written).
- **Why** (LIKELY, untested): the group is the characters' post-physics step, and the title screen has no characters.
- **Next** (DECIDED by the owner, 2026-11-17, "Load a save after the call"): run 4 uses the same package; after the
  call the owner loads a character, stands at a Site of Grace for about a minute and quits normally. The launcher has
  backed up the saves. If the task ticks in the world, the probe's census and searches complete R1.

### 209. R1 run 4: with a world loaded the task still never ran, and the game crashed on quit in a module not ours; spike.5 searches the hub for where our task was stored

`2026-11-17` · FACT · `#gal` `#r1` `#task` `#crash` · refs: WHY§208, engine cab98b7

- **The run** (FACT, spike.4): as run 3, every check passed and `register_task` returned ok at the title screen. The
  owner then loaded a character and stood at a Site of Grace for several minutes: the task was never invoked (no slot
  called, nothing of our object written). The title screen was not the reason.
- **The quit** (FACT): the owner quit to the desktop through the menu and saw nothing wrong, but the launcher's crash
  record holds an access violation on a thread neither the game's main thread nor ours, in a module the record names
  `160_E658700.bin`, then exit code 0xC0000409. Runs 1 to 3 (no world loaded) ended clean. The cause is unknown: our
  registered task at shutdown, or the other DLLs in the game folder (`dinput8.dll`, `dxgi.dll`; O91's third-party rows).
- **So** (LIKELY): the call returned without making our task run: the group's index, the task object the function
  expects, or the function itself is not what R1 derived.
- **Spike.5** (DECIDED by the owner, 2026-11-17, "find where it went"): 2 and 20 s after the call, on T1, the hub's
  reachable memory is searched breadth first from its instance, three pointers deep, read-only through the
  crash-guarded copy under the plausibility rules, within 6,000 blocks and 6 MB, for our task object's address; the
  report gives where it was found (the offsets followed) and the 32-bit words equal to the group's index near it. On the
  fake game it finds the task one pointer deep. Run 5 stays at the title screen.

### 210. Wine 9.0 aborts at start-up ("free(): invalid pointer") when TMPDIR is set, as the cloud environment now sets it: every Wine process runs without it

`2026-11-20` · FACT · `#wine` `#ci` `#graceui` · refs: WHY§113, engine e3b2670

- **The failure** (FACT): after the container's restart on 2026-11-20 every Wine process, in an old prefix or a fresh
  one, printed "free(): invalid pointer" and aborted before running anything; the fake game's prefix set-up failed, so
  every harness case was not run. Wine, glibc and our builds had not changed; the environment had: it now sets
  `TMPDIR=/tmp`.
- **The cause** (FACT, gdb): Wine 9.0's `ntdll.so` frees, during `__wine_main`, a pointer into the environment strings
  at the top of the stack, which is `TMPDIR`'s value; with the variable unset the same command runs.
- **The way round** (DECIDED): EldenNet's harness drops `TMPDIR` from the environment it gives Wine (`wine_env`), and
  hand runs use `env -u TMPDIR`. Grace was told the same day, for GraceUI's harness.

### 211. The launcher no longer links en-core: R1's probe report vocabulary moved into en-gal, and a configure-time check refuses any link that breaks ARCH §2.3's layering

`2026-11-20` · DECIDED · `#layering` `#launcher` `#build` · refs: ARCH§2.3, engine 1c30a02

- **The problem** (FACT): to lint R1's two reports after a probe session, the launcher linked `en_shell`, and through it
  en-core, which ARCH §2.3 does not allow: the launcher may link only its own core, en-gal and en-proto.
- **The change** (DECIDED): R1's probe report, its findings and closed vocabulary, moved from the shell into en-gal's
  offline part as `en::gal::probe`, beside the derivation report it pairs with; the probe settings stay in the shell.
- **The guard** (DECIDED): `cmake/link_rules.cmake` checks every module's links at configure time, following links
  through other libraries, and stops with the chain that breaks a rule; `tests/linkrules` holds one clean case and five
  that must be refused (through another library, a deep chain, a link-only dependency, a condition, an alias).

### 212. GraceUI DESIGN §3.3's contrast table rounds each ratio twice, so eight cells read 0.1 high; the tokens reproduce it within 0.06 and the thresholds are checked on the unrounded ratio

`2026-11-20` · METHOD · `#graceui` `#tokens` `#contrast` · refs: GraceUI:D157, GraceUI:DESIGN§3.3, GraceUI feat/foundations 2ae00d0

- (METHOD, the Grace session) DESIGN §3.3's contrast table rounds each ratio twice, to 0.01 and then to 0.1, so eight
  cells sit 0.1 above the ratio rounded once (11.3479 is written 11.4). The tokens reproduce the table within 0.06,
  which `tools/tokens/test_build.py` checks; the thresholds (invariant 13's 4.5:1 and the others) are checked on the
  unrounded ratio (D157).

### 213. nlohmann/json 3.12.0 parses a 250 KiB state frame in about 3.3 ms and writes it in 0.7 ms, against GraceUI §8's 0.5 ms each way for the bridge's largest payload: a risk for S4

`2026-11-20` · FACT · `#graceui` `#protocol` `#perf` · refs: GraceUI:DESIGN§8, GraceUI:D55, GraceUI feat/foundations beef3ee

- **The measure** (FACT, the Grace session; GCC -O2, this build machine): nlohmann/json 3.12.0 parses a 250 KiB JSON
  state frame in about 3.3 ms and dumps it in 0.7 ms, about 80 MB/s parsing. GraceUI §8 budgets the in-process bridge
  at 0.5 ms each way for EldenNet's largest payload (`kinds`, 256 KiB, v0.4).
- **Our layer** (FACT): with grace-protocol's frame-bounded bodies moved out of the parsed frame and frames written
  straight to text, grace-protocol adds nothing measurable: the cost is the library's own.
- **Next** (DECIDED): a risk for S4 to measure on the player's hardware (GraceUI D159, §11). If it stays over, the ways
  out are a faster parser behind grace-protocol (yyjson, MIT, first) or `publish` checking the body's syntax and passing
  its bytes through; the second amends D55 and touches EldenNet's T6, so the two sessions agree it first.

### 214. grace-serve's head parser refuses a NUL, a bare CR, a bare LF or an empty first line as it arrives, so a client that never ends its head is refused at once; everything else waits for the whole head, then Host first

`2026-11-20` · METHOD · `#graceui` `#server` `#security` · refs: GraceUI:D158, GraceUI:DESIGN§7.2, G29, GraceUI feat/foundations 630639d

- (METHOD, the Grace session) grace-serve's head parser refuses a NUL, a bare CR, a bare LF or an empty first line as
  it arrives, rather than waiting for the whole head. A client that never ends its head (bare-LF line endings) is then
  refused at once instead of holding a connection until the 5 s head deadline. Everything else waits for the whole
  head, then Host is checked first (421), as SRV §5.2 advises (D158).
- (DECIDED, D158; for EldenNet's launcher as a host of the same library, G29) the session cookie and the ticket are 32
  random bytes written as 43 characters of base64url; the loopback origin is always `http://127.0.0.1:PORT`, with
  `localhost` and `[::1]` redirected there by 307.

### 215. graceui-sys's headers are vendored into EldenNet at GraceUI f51bcc7, each checked by SHA-256, not fetched; Dear ImGui's core is fetched at GraceUI's own pin

`2026-11-20` · DECIDED · `#engine` `#graceui` `#licences` · refs: ARCH§2.3, GraceUI:D136, GraceUI:D156, engine 789e4cb

- (DECIDED) GraceUI's headers (`graceui.h`, `graceui_sys.h`, `graceui_sys.hpp`, `graceui_imgui.hpp`, and the test
  build's `graceui_test.h`) are copied unmodified into `third_party/graceui-sys` from GraceUI f51bcc7 with GraceUI's MIT
  licence, each checked against its SHA-256 at configure time. Not fetched: a fetch URL would name the GitHub account in
  the repository, which the privacy rule keeps out; the headers are small, and the same owner's and licence.
- (DECIDED) Dear ImGui 1.93.0's core is fetched (`en_fetch`) at GraceUI's own pin, file by file by SHA-256, in the
  Windows builds only (THIRD_PARTY.md).

### 216. en-overlay's look for GraceUI ends at once when none is installed, rather than after graceui-sys's 30 s

`2026-11-20` · DECIDED · `#engine` `#graceui` `#dll` · refs: ARCH§1.4, ARCH§3.2, GraceUI:D51, GraceUI:D83, engine ba05179

- (DECIDED) graceui-sys looks every 250 ms for at most 30 s. When nothing answers, no `GraceUI\graceui.dll` lies in
  the game's folder and no module is named `graceui.dll`, en-overlay's `find` stops after the first look, so a start
  without GraceUI (every harness case without it, and R1's) is not held up 30 s. Both load orders still work: before,
  found by its name; after, loaded from the game's folder by EldenNet itself.
- Not covered: a copy under another name loaded later from another folder, which GraceUI's packaging rules out.
- *Later (2026-11-20):* graceui-sys holds this rule now, as `GRACEUI_SYS_STOP_IF_ABSENT` (GraceUI D164); en-overlay
  passes the flag instead of its own check (WHY§230).

### 217. The development Mark changes hands through the watch block, on Pause: the launcher lets its key go when EldenNet's GraceUI hotkey is registered

`2026-11-20` · DECIDED · `#engine` `#launcher` `#graceui` · refs: O90, ARCH§4.14, GraceUI:D155, engine 5e97d91

- (FACT) The launcher's Pause is a system hotkey (RegisterHotKey), which takes the key from the game's window, so
  GraceUI's hotkey on the same key could never fire beside it, and two writers to the block's ring would race.
- (DECIDED) `Marks::reserved` became `Marks::source` (None, Launcher, Asked, EldenNet): the launcher marks Launcher
  when its hotkey registers; EldenNet, once its GraceUI `mark` hotkey is registered, asks; the launcher unregisters,
  takes the queued presses as its last and hands over; EldenNet then stamps every Mark (at T6's poll, at most 100 ms
  after the press) and the launcher's record reads them back. The owner presses Pause either way; without GraceUI
  nothing asks and R1's flow is unchanged. The block's layout and version are unchanged (a reserved field took a
  meaning).
- (LIKELY) GraceUI's `register_hotkey` answers ok whether the key bound or not, so if another mod held Pause first
  nobody would take the presses; asked of the Grace session the same day.

### 218. The F11 view is behind debug.panel (SP8): without it the view, its panel, atlas and hotkey are not made and F11 reaches the game

`2026-11-20` · DECIDED · `#engine` `#dll` `#graceui` · refs: PAGES§4.8, DESIGN§12, ARCH§11.5, engine ba05179

- (DECIDED) DESIGN §12 has the debug view off by default and SP8 puts F11 behind `debug.panel`, so the view, its
  panel, atlas and F11 hotkey are made only with `debug.panel = true`; otherwise F11 reaches the game. Its first line
  is SP8's ribbon; SP8's confirmation each session waits for input in the view (GraceUI D142).

### 219. E12, a failed GraceUI pre-flight, is told and the game starts headless; --probe and the spikes only log it

`2026-11-20` · DECIDED · `#launcher` `#graceui` · refs: ARCH§4.3, ARCH§10.3, GraceUI:D83, GraceUI:D88, engine 5e97d91

- (DECIDED) Until EldenNet carries a GraceUI release to install, the launcher cannot mend a failed pre-flight, and
  ARCH §10.3 allows a headless start. So E12 names what is missing or damaged (by its name in GraceUI's folder, never
  a path) and the game starts; `--probe` and the spikes, headless by design (O90), log the check and show nothing, so
  R1's flow gains no dialog. The pre-flight reads `graceui.toml`, checks every listed file's size and SHA-256 (our own
  implementation), ABI major 1 and the version floor.

### 220. Dear ImGui's core in eldennet.dll brought thread-safe statics and ReadConsoleW; /Zc:threadSafeInit- on the core removes the first, the rest are reviewed imports

`2026-11-20` · FACT · `#engine` `#dll` `#ci` · refs: ARCH§12.1, WHY§195, engine 789e4cb

- (FACT) Compiled into eldennet.dll as GraceUI's `gu_imgui_core` compiles it, Dear ImGui 1.93.0's core brought the
  runtime's thread-safe statics (`AcquireSRWLockExclusive`, `ReleaseSRWLockExclusive`, `SleepConditionVariableSRW`,
  `WakeAllConditionVariable`) and, through its `sscanf`, `ReadConsoleW`.
- (DECIDED) With `/Zc:threadSafeInit-` on that library alone (one thread at a time uses its context) the first four
  went; `ReadConsoleW` and graceui-sys's `GetTickCount64` were reviewed into `imports.allow`. It holds no dynamic
  initialiser and no exit-time destructor (Clang's checks kept as errors on it).

### 221. The first end-to-end run with GraceUI: the launcher, eldennet.dll and GraceUI's test build in our fake game on Direct3D 12 under Wine, F11's panel read back from the frame

`2026-11-20` · FACT · `#engine` `#graceui` `#wine` `#test` · refs: WHY§113, GraceUI:D154, GraceUI:D156, engine e0d016e

- (FACT) The fake game, presenting on Direct3D 12 (Wine 9.0, vkd3d on lavapipe, Xvfb), with eldennet.dll injected by
  the launcher and GraceUI's test build in `game\GraceUI\`: EldenNet loaded GraceUI by path at Connect 0.1 s after
  the window and connected with the view ready and the Mark's hotkey registered; the launcher handed its Pause over
  0.3 s later. F11 into the window showed the panel, whose ground covered 97% of the probed rectangle of the frame read
  back (224,639 of 231,800 pixels), 12 lists accepted and none refused; F11 again hid it (0 pixels); Pause stamped
  Mark 1 as EldenNet's, which the launcher's record took.
- (FACT) Before-order on the banner path: the banner notice in the warning tone; beside the anti-cheat module: connect
  inactive (anti-cheat), the banner the one notice of EldenNet's, no panel, no provider.
- (METHOD) The harness installs GraceUI as its release lays itself out (graceui.dll, fonts\, a graceui.toml it writes
  with each file's SHA-256) and gives the fake game `--d3d12`, `--press-f11-at` (keys into the focused window, with the
  scan code, after the focus events) and `--grace-report` (GraceUI's test exports, and a read-back of a rectangle of
  the frame). GraceUI's own harness could not host eldennet.dll: it loads its examples by name and checks their ids.
- (LIKELY) In the fake game's world the danger stand-in claimed danger throughout, probably from its invented lock-on
  or boss fields; nothing checks it yet.

### 222. Augur runs straight after en-core's lifecycle slice in the same step, not before it, so its lifecycle events and the loading screen are seen in the step they happen

`2026-11-20` · DECIDED · `#engine` `#augur` `#core` · refs: ARCH§8, ARCH§9, DESIGN§6.6, engine wip/s10-p11

- **Chosen**: in `Core::step`, the v0.1 slice (gate, phases, ladder) judges the input first, then Augur takes the same
  input with the slice's changes as its Lifecycle facts, before anything that consumes events (no subscriber yet).
  ARCH §8's opening ("it runs first in every `Core::step`") is to be amended (roadmap S10.1.i).
- **Why**: ARCH §8.2's Lifecycle source produces GateChanged, LoadingStarted and the rest, which are the slice's
  decisions; running Augur first would see them a step late, so a loading screen's snapshot would be processed as a
  world (engagements not cleared, trackers kept) and LoadingStarted's `ALL` interrupt would lag by up to 100 ms.
- **Rejected**: Augur first with the previous step's changes (a lag and an ordering inversion); Augur deriving the
  lifecycle itself (a second lifecycle machine beside en-core's).

### 223. What ARCH §8 left open, settled for Augur v0.1: lifecycle events as rule files, a 1 s Struck window, engagement re-armed only once apart, FNV-1a hashes and others

`2026-11-20` · DECIDED · `#engine` `#augur` · refs: ARCH§8.4, ARCH§8.5, ARCH§8.7, ARCH§8.12, src/en-augur/README.md

- **Chosen** (the module README's list in full): lifecycle events as rule files on a `fact` detector; the schema's
  additions (`subjects.in_combat`, `changed_stable.to`, `engagement.on`, `fact.value`) and the signals
  `crit_by_player`, `native_talk`, `lifecycle`; Struck's window 1 s (ARCH §8.5's example has 300 ms), so a hit across
  a dropped snapshot still fires; an engagement re-armed after a Timeout only once the pair has been apart; squads by
  live team; `fight_id` a per-session ordinal; Victory evidence from the fight's start; loading clears characters but
  not the Tarnished's or the world's detectors; K merges within one rule; interrupts from the kind's table; FNV-1a 64
  hashes and splitmix64 for `AugurCtx::rng` (BLAKE3 stays in the launcher).
- **Why**: each is the simplest reading that keeps Augur pure and deterministic and lets every catalogue event of v0.1
  live in a rule file.
- **v0.1 emits** (25 rule files): PlaceEntered, BossFightStarted and Ended, NativeTalkStarted and Ended (no persona),
  PlayerDied, MountChanged, Struck, LowHp, Healed, Engaged and Disengaged, AllyDied, CritLanded, JourneyChanged and the
  eight lifecycle events. KilledNear, CameNear, AutonomyTick and IdleNear wait for personas and cards; FlagWatch's
  events for v0.2; the hook sources stay reserved (§8.11).

### 224. Under 50% snapshot loss Augur invents no event and fires none earlier; brief states and per-hit events are what differ

`2026-11-20` · FACT · `#engine` `#augur` `#test` · refs: ARCH§8.12, ARCH§5.1

- **Seen**: on the three synthetic routes with half the snapshots dropped at random (12 seeds each), no event is
  invented and none fires earlier; every lasting state's event is there with its payload, at most two gaps and a
  second later. What differs: Struck (the hits of a dropped snapshot arrive as one drop, so merges and `hard` change),
  states briefer than a gap (a 0.5 s near pass, LockOnly before LockClosing, a heal within 1 s, a critical hit's two
  snapshots, a low HP killed by the next hit), NativeTalkEnded's `held_ms`, and the gate, whose 5 s hold starts again
  after a gap over 1 s, so it may open late or never in a lossy run.
- **So**: the backpressure test compares by event and payload for lasting kinds and only "never invented, never
  earlier" for the rest; §8.12's "except for documented timing differences" is read as this list.

### 225. A recording's reader now rebuilds what Augur reads of a snapshot, so replay and golden drive Augur from the DLL's recordings, schema unchanged

`2026-11-20` · DECIDED · `#engine` `#augur` `#spike` `#datapiler` · refs: ARCH§7.5, ARCH§12.3, WHY§192, WHY§202

- **Chosen**: en_shell's `read_recorded_line` rebuilds the characters, the Tarnished's combat fields, the UI, the
  place, the clock, the boss bars and the flag frame from the numbers the recorder already writes, so that `replay` and
  `golden` drive Augur from a recording; the recording's format is unchanged (schema 1). What it never held stays
  empty: text look-ups' text, param rows' values, a region key, a bearing without the camera.
- **Seen**: a route's inputs written as the DLL writes them replay to byte-identical events (7 significant digits of
  each float are enough for every derivation on the synthetic routes).
- **For C12** (DESIGN: "golden events over 3 recorded routes"): the three shipped routes are synthetic; the exit check
  waits for three recordings of the owner's game, redacted so they may be committed, each made a route with
  `golden update <recording>`.

### 226. Augur v0.1's step costs about 26 µs on a busy synthetic step (32 characters, 25 rules), p99 51 to 71 µs, against ARCH's 1 ms budgets; and a CMake tree's cached Python can vanish

`2026-11-20` · FACT · `#engine` `#augur` `#perf` `#build` · refs: ARCH§8.10

- **Seen**: `en_augur_bench` (Clang 18 Release, the build machine's 4 cores): `Augur::step` on 32 characters with the
  shipped 25 rules, about 7 events a step: mean 25 to 27 µs, p50 23 to 25 µs, p99 51 to 71 µs over 20,000 steps (the
  most, 0.9 to 2.1 ms, is the machine's scheduling). ARCH §8.10 allows 1 ms for the snapshot diff and world model and
  1 ms for the rules. Loading the rule set: 0.5 ms warm, 5 ms cold. An unoptimised GCC build: 766 µs mean.
- **Also seen** (FACT, build): during the run, `/root/.local/bin/python3.10`, the Python CMake had cached for the
  harness, vanished: it was a link into a scratch folder that the session removed while freeing disk. Trees configured
  before need `-DPython3_EXECUTABLE=/usr/local/bin/python3` (3.13); the harness runs on it unchanged.

### 227. R1 run 5: with no other mods, register_task returned ok again and our task's address was nowhere in 4 MB of the hub's memory three pointers deep; spike.6 searches all memory and summarises the callee

`2026-11-20` · FACT · `#gal` `#r1` `#task` · refs: WHY§208, WHY§209, engine 4edc9cf

- **The run** (FACT, spike.5, game 2.7.2.0 ww): the owner removed the other DLLs from the game's folder
  (`other_mods=none`, 130 modules against 176 to 179 before). Six singletons named by their instances' RTTI, every
  check passed, and at the Mark `register_task` (callee RVA 0xEB5680, task r8, instance rcx, group rdx, index 110)
  returned ok. The task was never invoked; the exit was clean.
- **The search** (FACT): 2 and 20 s after the call, breadth first from CSTask's instance three pointers deep, about
  4,600 blocks and 4.1 MB read (about 120 faults, one refused): our object's address found nowhere, and nothing pointing
  inside it.
- **So** (LIKELY): the call kept our task nowhere the hub reaches: the callee is not the registration, an argument's
  meaning is wrong (the group's index, the task object's form), or the task went somewhere the search does not reach.
  Whether run 4's crash on quit was ours stays open; run 5, with no other mods and no world loaded, ended clean.
- **Next** (DECIDED): spike.6 adds, with no new call into the game, a summary of the callee from the live image (its
  calls, its branches before the first call, what it does with each argument register: stores, moves, compares with
  small immediates) and a search of every readable committed region of the process for our task's address after the
  call, our own copies counted apart.

### 228. Cinzel publishes static faces only at Regular, Bold and Black, so GraceUI's 500 and 600 weights take Regular and Bold; of the faces, EB Garamond and Noto Sans Mono carry the arrows U+2190 to U+2193

`2026-11-20` · FACT · `#graceui` `#fonts` · refs: GraceUI:D167, GraceUI:NOTICE

- (FACT, the Grace session, from the faces' cmap tables, 2026-11-20) Cinzel (its publisher's repository) publishes static
  TTFs only at Regular, Bold and Black, so the tokens' 500 and 600 take Regular and Bold until an instancer is in the
  build. EB Garamond and Noto Sans Mono carry U+2190 to U+2193; Noto Sans and Cinzel do not.
- (FACT) GraceUI's fonts/ holds eight faces (Cinzel Regular and Bold; EB Garamond Regular, Medium and SemiBold; Noto Sans
  Regular and SemiBold; Noto Sans Mono), each pinned from its publisher, OFL, credited in NOTICE.

### 229. Under Wine on lavapipe a full-screen overlay's frame-cost p99 is the software renderer's: GraceUI's menu holds its 1 ms present-thread budget at 640x360 and is measured at 1920x1080, the game's run settling it

`2026-11-20` · METHOD · `#graceui` `#perf` `#wine` · refs: GraceUI:D167, GraceUI:D154, WHY§113

- (METHOD, the Grace session) Under Wine on lavapipe, a full-screen 1920×1080 overlay's frame-cost p99 is the software
  renderer's, which shares the machine's cores (median 0.15 ms, p99 1.1 to 1.9 ms). The budget is held at 640×360
  (p99 0.36 to 0.78 ms, under 1 ms) and the full size is measured and printed; S4's run in the game settles it.
- (FACT) GraceUI's menu page at 1920×1080: 2,752 vertices, 6,612 indices and 4 commands, laid out and tessellated in
  0.06 to 0.3 ms on GraceUI's own thread; its atlas 1024×2048, baked in 25 to 41 ms at warm-up; no allocation on the
  present thread.

### 230. en-overlay takes GraceUI's own rules: GRACEUI_UNBOUND is a success with an id, an unbound Mark leaves the launcher's Pause in place, and graceui-sys's STOP_IF_ABSENT replaces our own early stop

`2026-11-20` · DECIDED · `#engine` `#graceui` `#dll` · refs: GraceUI:D164, GraceUI:D155, GraceUI:D136, WHY§216, WHY§217, engine d953b14

- (DECIDED) `register_hotkey`'s `GRACEUI_UNBOUND` (11) is a success that carries an id. F11 unbound: the view is made
  all the same, logged at Warn, keeping the id so that a binding on GraceUI's Hotkeys page (0.6) opens it; there is
  no other way to the view in v0.1. The Mark unbound: registered with its id, and eldennet.dll asks the launcher for
  Pause only for a bound Mark, so the launcher's Pause stays the Mark; a harness case checks it.
- (DECIDED) `find` calls `graceui_sys_find(GRACEUI_SYS_LOAD | GRACEUI_SYS_STOP_IF_ABSENT, …)`; en-overlay's own early
  stop is gone, with the same behaviour.
- Not followed in v0.1: a binding made after connect (GraceUI 0.1 has no Hotkeys page).

### 231. graceui-sys at 3f1352b leaves installed at 0 for a GraceUI found by its name; en-overlay reads a find by name as installed until it is fixed

`2026-11-20` · FACT · `#graceui` `#engine` · refs: GraceUI:D164, engine 10a3b0d

- (FACT) `graceui_sys_find_once` returns from its by-name branch before it sets `found->installed`, which
  `graceui_sys_find` zeroed, so a GraceUI loaded before EldenNet (the launcher's `--also`) reports `installed=0`. D164
  defines `installed` as 1 when a module named `graceui.dll` was loaded. Nothing is decided by it there (the stop rule
  applies only when nothing answered).
- (DECIDED) en-overlay reports `installed` as graceui-sys's word or a find by name; the harness's `-before` and
  `-unbound` cases check `installed=1`. Passed to the Grace session; the workaround goes when graceui-sys sets it.
- *Later (2026-11-20):* fixed in graceui-sys at GraceUI c781a85 (D170): the by-name branch sets `installed = 1`.
  EldenNet's workaround goes when it re-vendors graceui-sys from c781a85 or later.

### 232. Under Wine 9, loading graceui.dll before the game's first window can hang in dxgi.dll's load while other threads make USER calls; EldenNet loads it only after the window

`2026-11-20` · FACT · `#wine` `#graceui` `#ci` · refs: WHY§210, engine 533df70

- (FACT) The fake game, standing in for another mod, loaded `GraceUI\graceui.dll` itself at boot, before its window,
  with the launcher and eldennet.dll's threads running: 3 runs of 4 hung for ever inside `LoadLibraryW(L"dxgi.dll")`,
  before any GraceUI code (graceui.dll imports dxgi, d3d12 and D3DCOMPILER_47); alone, 6 runs of 6 passed in under
  0.1 s.
- (LIKELY) A lock-order inversion in Wine between the loader lock (dxgi's and wined3d's attach, which bring up the
  display driver) and the USER side, where EldenNet's T1 polls `EnumWindows` every 100 ms. After the game's first
  window the driver is up, so later loads never hang. Unknown on Windows (SPECULATION: not expected).
- (DECIDED) The harness loads GraceUI with the launcher's `--also` into the suspended process; EldenNet itself loads
  GraceUI only after the window (T1's WaitWindow), so it is not affected. Passed to the Grace session: delay-loading
  Direct3D's DLLs would spare an early-loading mod.
- *Later (2026-11-20):* GraceUI c781a85 (D170): graceui.dll imports no Direct3D DLL. CreateDXGIFactory1,
  D3D12CreateDevice and D3DCompile are bound on GraceUI's init thread after the game's window, from the module the game
  loaded or else from System32 alone; its imports are checked against a reviewed list. An early load no longer pulls
  in dxgi.

### 233. The launcher's pre-flight is tested on a manifest GraceUI's own manifest.py wrote, compiled into the tests byte for byte, with the extra keys accepted

`2026-11-20` · METHOD · `#engine` `#launcher` `#graceui` · refs: GraceUI:D165, GraceUI:D52, ARCH§4.3, engine 4334a8d

- (METHOD) `src/en-launcher/tests/graceui-manifest.toml` is what GraceUI's `tools/release/manifest.py` at 3f1352b wrote
  for an invented folder of three files of ours, whose bytes the test holds. CMake reads it as hex into a generated
  `.inc`, so the Windows tests under Wine read the committed bytes with no path. The test checks the folder it lists
  and accepts `protocol`, `caps` and `released` with other values: the pre-flight reads only the version, the ABI and
  the files (ARCH §4.3), and never fails on the rest.

### 234. R1 spike.6: the callee summarised from the live image, and the whole process searched for our task, our own copies told apart by the probe DLL's own heap

`2026-11-20` · DECIDED · `#gal` `#r1` `#task` · refs: WHY§227, WHY§209, engine fbf4c8b, 773a810

- **The callee** (DECIDED): after the checks and before the call, the probe decodes register_task's callee from the live
  image with en-gal's decoder, within its function of the exception directory (else up to its first return), at most
  4 KiB, following no call. `r1-probe.json`'s `callee_code` gives its calls (direct with target RVA, through a register
  or memory, RIP-relative), the conditional branches before the first call, whether a path returns through no call,
  and per argument register (rcx, rdx, r8, r9): its stores before the first call (base, displacement, width), the
  callee-saved registers it moved to, and its compares with an immediate up to 65,536. Arguments are followed through
  register moves and one load of [argument + 0]; an instruction outside the modelled forms ends the tracking, and
  `modelled_to` says where (METHOD: nothing guessed).
- **The search** (DECIDED): 2 and 20 s after the call, every committed readable region but no-access, guard,
  uncached or write-combined ones, never a known stack, read through the crash-guarded copy in 16 KiB chunks, within
  8 GiB and 30 s. `memory_search` counts the words equal to our address and pointing inside our object, with up to 16
  hits' region kind, protection, an image's module and section kind, RVA, and the group's index nearby.
- **Ours apart** (DECIDED, METHOD): in the probe build the DLL allocates from a heap of its own, walked before each
  search; a region is ours when its allocation base is our module's, our task block's or one of that heap's.
- **Also**: while the task has not run, T1 itself reads how much of our object the hub wrote.
- **On the fake game** (FACT): the summary shows the fake hub keeping the task at [instance + 0x30] and comparing the
  group with 512; each search read about 90 MB in about 30 ms and found the game's 4 copies, our own 15 counted apart.
- **For the owner's run** (LIKELY): at that rate 8 GiB is a few seconds, so both searches end long before the console's
  "No confirmation" at 30 s.

### 235. R1 run 6: register_task's callee is a 93-byte wrapper that keeps the group and the task in saved registers and hands them to two inner calls; 8 of 10.6 GiB searched in a second held no copy of our task

`2026-11-20` · FACT · `#gal` `#r1` `#task` · refs: WHY§227, WHY§234, engine fbf4c8b

- **The callee** (FACT, spike.6, game 2.7.2.0 ww, no other mods): RVA 0xEB5680 is 93 bytes, 22 instructions, fully
  modelled: one conditional branch before its first call (offset 25, to 73, past both calls), two direct calls (offsets
  34 and 61, to RVAs 0x1EC4A40 and 0x1EBCE00), no path returning through no call. Of its arguments, rcx is neither
  stored nor saved; rdx (the group, by R1's reading) is saved in rdi and r8 (the task) in rbx; none is stored or
  compared before the first call. So it is a wrapper: the work, and any test that drops our task, lie in what it calls,
  or after its branch (a jump at its end is not counted as a call).
- **The search** (FACT): at 2 and 21 s after the call, 8 GiB of 3,166 readable regions read in 0.7 to 1.0 s, no fault,
  stopped by the 8 GiB cap with about 2 GiB unread: no copy of our task's address but EldenNet's own two (its module's
  data and its probe heap), nothing pointing inside our object. The hub never wrote our object (object_written 0).
- **So** (LIKELY): the game kept no pointer to our task object, at least in the 8 GiB read. Either it stored something
  else (our vtable, or a wrapper it built from our object's fields), or the wrapper's branch or an inner call refused
  it.
- **Next** (DECIDED): spike.7 reads all memory (a cap far past the process's size), also searches for our vtable's
  and slots' addresses, diffs the hub's reachable memory before and after the call, and summarises the two inner
  callees with the arguments followed into them.
- Later (2026-11-20): the two direct calls' targets are RVAs 0x1EC4940 and 0x1EBCD00, as runs 7 and 8 report them in
  decimal; the hexadecimal values above (0x1EC4A40, 0x1EBCE00) were converted wrongly (WHY§248).

### 236. R1 spike.7: the task followed past the wrapper: arguments mapped into its calls, its callees summarised one level deeper, all memory read, our vtable and slots searched, the hub's memory diffed across the call

`2026-11-20` · DECIDED · `#gal` `#r1` `#task` · refs: WHY§235, WHY§234, WHY§227, engine bddcaa7

- **The flow** (DECIDED, METHOD): `summarise_callee` now tracks what each register holds along the control flow, to a
  fixed point: where paths meet a register keeps only what they agree on, unreached code holds nothing, and an
  unmodelled instruction clears its path. Spike.6 read in address order, which could carry one path's register into
  another's. Byte registers 4 to 7 without REX are read as ah to bh.
- **The mapping and the guard** (DECIDED): each call and tail jump gives which of the function's arguments (or the
  first word of one, its vtable) are in rcx, rdx, r8 and r9 there (`pass`). Each conditional branch before the first
  call (up to 8) gives its condition, its target and the instruction that set its flags (cmp, test, other or none),
  with its operands as kinds: a register and the argument it holds, a memory operand (base, the argument in it,
  displacement), a RIP-relative slot's RVA, an immediate up to 65,536 (`callee_code.guard`).
- **The inner callees** (DECIDED): each direct call's and tail jump's target (up to 8, one level) is summarised with the
  call site's mapping as its entry state, so its stores, saves, compares and calls read in the wrapper's arguments
  (`callee_code.inner`).
- **The search** (DECIDED): caps 64 GiB and 30 s, the time the bound; regions past a cap are still walked unread, so
  `committed_bytes` is the process's total. Besides our object it counts words equal to our fake vtable's address and
  to any of its slot thunks', the game's and ours apart, with how many distinct slots each side names.
- **The diff** (DECIDED): just before the call T1 makes spike.5's walk of the hub (same bounds) and keeps the blocks'
  bytes on its own heap; at 2 and 20 s it walks again and compares block by block, by path (else by address, else new).
  `hub_diff` gives the changed 8-byte words (up to 32, with path, depth, and each value's kind: zero, small, group,
  task, vtable, ours, game with section and RVA, heap, other; `fresh` for a pointer to a block not reached before), the
  blocks moved, gone and new. My choice beyond the brief: the 20 s diff marks a word `steady` when it held the same new
  value at 2 s and lists those first, so that per-frame counters do not crowd out what a registration leaves.
- **The fake game** (FACT, clang-cl Release, Wine 9.0): its registration is now a wrapper of run 6's shape (a byte
  test of a flag on the hub, then one call that keeps the task, else a tail jump). R1 read the guard as a byte test of
  [instance + 0x14] against 1, mapped the task from r8 through rbx to the store at [instance + 0x30] one level deeper,
  found the game's one copy each of our vtable and one run slot, and diffed 7 changed words (the task and vtable
  pointers fresh, the group's counts), 6 of them steady at 20 s and the frame counter not. Each search read about
  90 MB in about 50 ms.

### 237. grace-input's record of the last close lost the holder, so a close by the menu key or the player's exit skipped the host's close bookkeeping

`2026-11-20` · FACT · `#graceui` `#input` · refs: GraceUI:D171, GraceUI's invariants 7 and 17

- (FACT) grace-input packed a close's reason into the byte that held the holder's phase, so `last_close().holder`
  always read as not holding. The window procedure's menu-key path hands that holder to the host's close bookkeeping,
  which therefore skipped it: no "no longer holds input" line and no cursor restore after a close by the menu key or
  the player's exit. Found when the menu's close had to stop a mod's watch (D171); `last_close()` now gives the holder
  as it held, with tests.

### 238. GraceUI, not the space, enforces a destructive command's confirmation

`2026-11-20` · DECIDED · `#graceui` `#protocol` · refs: GraceUI:D173, GraceUI:DESIGN§7.3

- (DECIDED) The destructive actions are read from the space's own page data, and a command for one without
  `"confirm": true` is refused (`confirm_required`) by the server for browsers and by the registry for the menu. A mod
  cannot forget the confirmation, and EldenNet may rely on it.

### 239. A stream is cleared by an empty batch past a gap, with no message kind of its own

`2026-11-20` · METHOD · `#graceui` `#protocol` · refs: GraceUI:D161, GraceUI:D172

- (METHOD) The sequence numbering already says "start over" at a gap, so an empty `rows` batch past a gap clears the
  stream in every view, and a view that held rows never shows one twice (protocol 1.1).

### 240. R1 run 7: the wrapper hands the group and the task to a tail-called function that bounds the group by 168 and refuses through a null test; all readable memory held no copy of our task, its vtable or a slot

`2026-11-20` · FACT, with a LIKELY reading · `#gal` `#r1` `#task` · refs: WHY§235, WHY§236, engine bddcaa7

- **The wrapper** (FACT, spike.7, game 2.7.2.0 ww, no other mods): its one branch (offset 25, to 73, past both calls)
  is a `jne` on a 64-bit `test rcx, rcx` at offset 22, where rcx holds none of its arguments (loaded in its first 22
  bytes). Its first call is passed rdx, r8 and r9 unchanged, its second none of them. At offset 88 it tail-jumps to RVA 0xEB5D20 with rdx (the group) and r8 (the task) restored from rdi and rbx;
  our rcx (CSTask's instance, by R1's reading) is never used.
- **The tail function** (FACT): 32 bytes, 10 instructions, no call: `cmp edx, 168` and `ja` to offset 29, then a
  64-bit test of rcx (again none of our arguments) and `je` to 29, and a return at 31 that both branches reach. The
  path that does not branch leaves by an instruction spike.7 does not count (a jump through memory or a register).
  The two inner calls are short: 33 bytes with one call, and 29 bytes with one call.
- **The search and the diff** (FACT): at 2 and 22 s, 8.6 of 10.7 GB committed, read in about 1 to 1.5 s, stopped
  `done` (the 2 GB unread are what the rules skip: no-access, guard and device memory, and stacks). Not one copy of our object's,
  our vtable's or a slot's address but EldenNet's own. The hub diff (4,405 blocks, 4 MB kept) changed 64 and then 132
  words, none of them our task, vtable, module or the group's index; 12 steady at 20 s, all heap pointers replacing
  pointers into the game's data, three entries 32 bytes apart.
- **The call** (FACT): NativeSys calls register_task as a function returning nothing, so "ok" only means it returned
  without a fault; the return value was never kept.
- **So** (LIKELY): the wrapper is a static helper that uses a singleton of its own (lazily made, rcx loaded from a
  global), and the tail function looks up an entry by group in it (bounded by 168) and returns, likely 0, when the
  entry is null; otherwise it jumps on through the entry. Nothing kept our task, so the entry for group 110 was most
  likely null, or the function is not FD4's register_task at all.
- **Next** (DECIDED): spike.8 keeps the call's return value, lists the wrapper's and the tail function's instructions
  as forms (no bytes), models a jump through memory or a register as a tail call, reads the singleton's global and the
  entries before and after the call (which groups are non-null, and group 110's), compares the singleton with CSTask's
  instance, and makes a control diff of the hub over the same interval before the call.

### 241. A mod's panel that takes input gets no characters through ABI 1

`2026-11-20` · FACT · `#graceui` `#abi` · refs: GraceUI:D77, GraceUI:D175

- (FACT) GraceUI feeds the input holder's keys and characters to its own Dear ImGui context. ABI 1's events for a mod
  are hotkeys and the protocol's only. A mod that needs typed text (EldenNet's chat box, S11.4.c) needs an input event
  that ABI 1 has still to gain. Found when the S4 spike's typing panel had to count characters from the test build's
  counter instead.

### 242. A vtable-slot overlay loaded before GraceUI is hooked through; one loaded after calls through GraceUI's hook

`2026-11-20` · METHOD · `#graceui` `#overlay` · refs: GraceUI:D86, GraceUI:D175

- (METHOD) Loaded before GraceUI, an overlay that swaps the swap chain's Present slot makes GraceUI's read of its
  target return the overlay's own Present: GraceUI hooks that function, and both run. Loaded after, the overlay's slot
  calls dxgi's Present, which GraceUI's hook already covers. Shown with grace-standin.dll in the harness, both orders.

### 243. Under Wine 9 the process's handle count and private bytes read 0

`2026-11-20` · FACT · `#graceui` `#wine` · refs: GraceUI:D175, WHY§210

- (FACT) GetProcessHandleCount and GetProcessMemoryInfo's PrivateUsage answer 0 under Wine 9, so a leak check on them
  means nothing in the harness. GraceUI's spike reader shows them and does not judge them; on Windows they are real.

### 244. ABI 1.1's input flag is appended after the reserved word, not renamed from it

`2026-11-20` · METHOD · `#graceui` `#abi` · refs: GraceUI:D176, GraceUI:D109

- (METHOD) ABI 1's append-only check (GraceUI's tools/check_safety.py, against the 1.0 baseline) refused renaming
  GraceUiInputRequest's reserved word to `flags`. The flag is appended after it, with a second reserved word, so a
  1.0 request (24 bytes) reads `flags` as 0 by its size.

### 245. graceui-sys's bufferless poll asked with a 16-byte event, so a mod using it never saw a holder's events

`2026-11-20` · FACT · `#graceui` `#abi` · refs: GraceUI:D176, GraceUI:D169

- (FACT) Found by the spike observer's typing test in GraceUI's harness: its panel received 0 of 10 characters. The
  wrapper now asks at ABI 1.1's size where `has_cap("holder-events")`, and reads a space's `GRACEUI_FULL` as "nothing
  waiting". That holds because GraceUI hands out a holder's events and hotkeys before a space's. EldenNet's vendored
  graceui-sys must be re-vendored from 0.2.1 or later before the chat box uses it.

### 246. A holder that ended on another thread between its grant and the event mailbox's enable never got INPUT_ENDED

`2026-11-20` · FACT · `#graceui` `#input` · refs: GraceUI:D176

- (FACT) Danger or a loss of focus could end a holder between its grant and `HolderMail::enable`, so its close routine
  found nothing to end and the mod was never told. Fixed by a check after enable, fenced (seq_cst) against `closed()`'s
  fence, posting the end once. Found by review, not by a test: the window is too narrow to hit reliably.

### 247. R1 spike.8: the call's return value kept, instructions reported as forms, loaded values followed to their sources, the tail function's table read before and after the call, and a control diff

`2026-11-20` · DECIDED · `#gal` `#r1` `#task` · refs: WHY§240, WHY§236, engine 9a6e3ea

- **The return value** (DECIDED): `RegisterFn` returns rax, reported as a kind (`register.returned`: zero, small
  with its value, ours, the game's with section and RVA, heap, our task's block, other), with its low byte apart for
  a `bool` (`returned_low`).
- **Sources** (DECIDED, METHOD): besides "holds argument k", a register can hold "the value loaded from source s":
  a RIP-relative slot, or `[base + index*scale + disp]` with what base and index hold, at most 3 loads deep. Beyond
  the brief: a call's result stored to a global counts as that global's value, so a lazily made singleton is followed
  on both paths. Where the paths still disagree on rcx, the table is read on the path past the wrapper's null test
  (`via_guard`).
- **Forms and tail jumps** (DECIDED): each summary's first 64 instructions as forms (offset, length, class, operand
  kinds; no bytes, no opcodes), and a jump through memory or a register counted as a tail call with its base's source.
  A function with no unwind data ends at its first jump out that no branch goes past. The report stays within 256 KiB,
  dropping inner summaries' forms first.
- **The table** (DECIDED): when run 7's shapes match (the wrapper's rcx from a slot S, the tail function's bound and
  its null test of `[entry rcx + group*8 + disp]`, then a jump through the entry's first word), T1 reads just before
  the call and 2 s after:
  - the singleton's kind, and whether it is CSTask's instance or another named singleton's;
  - which entries from 0 to the bound are set;
  - the group's entry, its first word and the slot the final jump uses, whose target is summarised one level deeper.

  A failed shape names the first that failed, and nothing is read.
- **The control diff** (DECIDED): at the Mark T1 walks the hub, waits 2 s, walks it again and makes the call. Each
  changed word of the call's diffs is marked `in_control` when it also changed in those 2 s before the call.
- **The fake game** (FACT, clang-cl Release, Wine 9.0): its registration now has run 7's shape. The table matched
  (disp 0x10, bound 31, slot 0x10), 10 entries were set before and after the call, the group's entry was on the heap
  with its vtable in rdata, the call returned 1, and the control diff changed only the frame counter, marked
  `in_control` in both of the call's diffs. In a variant with no entry for the group, the call returned 0, the entry
  read zero, the 9 entries set left the group out, and the task never ran, as in run 7.
- **The owner's steps** (DECIDED): after "No confirmation", wait one more minute at the title screen before quitting,
  so that the 2 s and 20 s reads and searches finish.

### 248. R1 run 8: the call returns 1, and the group's entry exists, yet nothing keeps our task; the tail function hands the task to a function of the group's own

`2026-11-20` · FACT, with a LIKELY reading · `#gal` `#r1` `#task` · refs: WHY§247, WHY§240, engine 9a6e3ea

- **The return value** (FACT, spike.8, game 2.7.2.0 ww, no other mods): `register.returned` is small, value 1, low
  byte 1. The call accepted our task, by its own answer.
- **The table** (FACT): the shapes matched. The singleton comes from the data slot at RVA 0x4592F28, is on the heap
  and is not CSTask's instance. Its table at displacement 8 has a bound of 168, and all 169 entries are set, before
  the call and 2 s after. Group 110's entry is on the heap, with its first word in the game's rdata at RVA 0x2C0B330.
- **The tail function** (FACT, its forms): it bounds the group by 168, loads `[singleton + group*8 + 8]`, returns 0
  when that is null, and otherwise moves the task (r8) into rdx and jumps directly to RVA 0xEB6040. So the work is
  `entry->f(task)` at 0xEB6040, with rcx the group's entry and rdx our task: a direct call, not one through the entry's
  vtable. Spike.8 did not summarise it, being one level too deep.
- **The search and the diffs** (FACT): at 2 and 21 s, 8.6 of 10.7 GB read, `done`. There was no copy of our object's,
  our vtable's or a slot's address in the game's memory, and nothing pointing inside our object. The hub walk from
  CSTask's instance changed 142 and then 220 words, 78 and 99 of them also in the 2 s control. None of them is ours,
  and the walk never starts from the singleton or the group's entry. Our task never ran, and no slot of ours was
  called. The exit was clean, and both reports lint clean.
- **So** (LIKELY): the group's own function took our task and answered 1, but kept no plain pointer to it. It may keep
  a tagged or offset pointer (a high-bit tag would escape the search), copy fields of our object but not its first
  word, or queue it somewhere that a later step drops, for example on a field of our object that is zero.
- **Next** (DECIDED): spike.9 summarises 0xEB6040 and its callees with the group's entry and the task followed in,
  including every read of a task field (`[task + off]`) and what is done with it. It walks and diffs from the
  singleton and group 110's entry, with the control. It also searches for words whose low 48 bits equal our address,
  and names the entry's class by its RTTI against the leads.
- Corrects WHY§235's hexadecimal call targets.

### 249. An observer's run (S0-E, S4) stops T1 at Register; what a stopped start leaves running

`2026-11-20` · FACT · `#engine` `#dll` `#spike` `#graceui` · refs: S10.1.g, S11.1.a, WHY§240, engine 5c5b3bc

- (FACT, the code) T6's loop and the overlay's view reach the task only through `s.task_set`, which is null before
  Register. T10 and T11 do not depend on it. The recorder opens at T6's start and writes its header (with the spike's
  word) then, and the crash filter is in by the end of WaitEngine. So a stop at the start of Register leaves T6 (the
  core, the block's status, the banner's notice, the world-state claims, F11's view), T10, T11 and the recorder
  running, and nothing waits for the task.
- (FACT, the fake game under Wine) `launcher-eldennet-spike-s0e`, `-s4` and `-graceui-spike-s0e` pass: `register
  skipped=1 why=spike`, the `spike_run` banner (a GraceUI notice, with F11's view, in the GraceUI case), status lines
  after T1 ended, a clean end, and no registration from outside.
- (DECIDED) "No call" is about calls. Discovery's guarded reads and WaitEngine's reads still run before the stop, so
  S0-E's numbers include that start-up reading, which ends within the first seconds. A probe build sets R1 aside in
  such a run (`probe round=r1 skipped=1 why=spike`).

### 250. In the fake game, a rack line is no sign of our call; the hub's live count is

`2026-11-20` · METHOD · `#fakegame` `#harness` `#spike` · refs: WHY§240, engine 5c5b3bc

- (FACT) Since spike.8 the fake game's own four tasks register through the same wrapper, which makes its rack on the
  program's own first call, about 60 ms after boot. A first check that read "no rack line" as "no call of ours"
  failed.
- (METHOD) The measure of "no call into the game" is the end line's `live` (the hub's own 4) with
  `foreign_tasks=0`, and no `FAKEGAME register` line of any outcome.

### 251. The observer's runs' words join schema 1; the game's environment is always the launcher's own block

`2026-11-20` · DECIDED · `#engine` `#launcher` `#spike` `#formats` · refs: WHY§188, engine 5c5b3bc

- (DECIDED) `s0e`, `s0e_alone`, `s4` and the banner reason `spike_run` join schema 1 of the recording, `rec_stats`
  and `spike`, not a schema 2. formats.md's rule says a new word means a new schema, but no reader outside the engine
  holds these words (DataPiler's guard holds only `derive`'s, WHY§188), and a schema 2 would make every older
  recording reader refuse every recording. formats.md has a dated note: the rule holds from the first outside copy.
- (DECIDED) The launcher always gives CreateProcessW an explicit environment block, its own through
  `game_environment`, so that a leftover `ELDENNET_SPIKE_DIR` (in any letter case) never reaches the game.
- (DECIDED) The spike folder's number is one more than the highest `<word>-<n>` already in `spikes\` (names compared
  ignoring case), so a run never reuses a folder that may hold a report.

### 252. ABI 1 never had the display-changed event that GraceUI's design promised

`2026-11-20` · FACT · `#graceui` `#abi` · refs: GraceUI:D180, GraceUI:D84, O93

- (FACT) GraceUI's DESIGN §3.13, its D84 and proposal 0001's G6 row promise a display-changed event that carries the
  scale, but ABI 1 never had one: GraceUI 0.2.1's poll kinds were 1 to 9. Found when EldenNet asked for ui.scale's
  default after D177 (O93). Built as D180 in GraceUI 0.2.2: the display at connect and `GRACEUI_EVENT_DISPLAY` (10),
  ABI 1.2.

### 253. Dear ImGui bakes a face at a whole-pixel size, so GraceUI rounds a scaled face's size itself

`2026-11-20` · FACT · `#graceui` `#fonts` · refs: GraceUI:D180

- (FACT) Asked for 10.67 px, Dear ImGui 1.93 baked 11. GraceUI now rounds a scaled face's size to a whole pixel itself,
  so the rule is GraceUI's and documented, not the library's; the glyph table gives the size baked.

### 254. A rise in a frame-cost median after a change with nothing per-frame was the machine's: measure old and new alternately

`2026-11-20` · METHOD · `#graceui` `#ci` · refs: GraceUI:D180, GraceUI:D178

- (METHOD) When the harness's "menu frame cost" medians rose about 25% after a change with nothing per-frame in it,
  the previous commit was built apart and measured alternately with the new one. The medians were the same, so the rise
  was the machine's, not the change's.

### 255. The owner's package for S0-E and S4: one run file each, GraceUI copied into the game's folder, F11's view switched on

`2026-11-20` · DECIDED · `#engine` `#launcher` `#graceui` `#spike` · refs: S10.1.g, S11.1.a, WHY§249, engine def31b4

- (DECIDED) `tools/spikepkg/build.py --kind s0e-s4` packs the launcher, the release eldennet.dll, `gal\`, GraceUI's
  test build (here 0.2.2 at f5c16a2), grace-spike.dll, grace-standin.dll and one `.cmd` file per run (A1 to A4, A0, B).
  Each file runs `cd /d "%~dp0"` and adds `--console`, so the launcher's lines show. The package leaves out GraceUI's
  `user\` folder (D179's kept settings) and refuses a `graceui.toml` that lists one.
- (DECIDED) The README asks the owner to copy `GraceUI\` into the game's own `Game` folder (A3, A4 and B have EldenNet
  load it from there, as a release will), to be deleted afterwards, and to switch on F11's view (`debug.panel`), which
  is off by default.
- (METHOD) A harness case whose program reads relative paths now names its start folder: Wine maps it to its drive,
  as cmd.exe's `cd /d "%~dp0"` does on the owner's PC.
- *Later (2026-11-23):* repacked as 0.1.0-spikes.2 from engine fd6e23f with GraceUI 0.2.22 at b488e3c (Grace's
  request); the same runs and the same rules. Package case A2 stalled once, under two agents' builds, after GraceUI's
  observer was injected late (killed at 240 s), and passed three times alone; told to Grace (S10.1.g).

### 256. EldenNet's vendored graceui_test.h lagged GraceUI 0.2.1; the GraceUI case now reads the version it expects from graceui.toml

`2026-11-20` · FACT · `#engine` `#graceui` `#harness` · refs: GraceUI:D175, engine 672cae3

- (FACT) GraceUI 0.2.1 appended fields to `GraceUiTestState` (D175), and `GraceUI_Test_GetState` refuses a smaller
  size. The fake game, reading through EldenNet's vendored `graceui_test.h` (3f1352b), saw zeros, and
  `launcher-eldennet-graceui-spike-s0e` failed on that alone. `graceui_test.h` is re-vendored at 2131839 with its
  SHA-256 pin. The rest of graceui-sys stays at 3f1352b until S11.4.
- (METHOD) `launcher-eldennet-graceui` expected GraceUI 0.1.0 and ABI 1.0 by name. It now reads both from the GraceUI
  folder's own `graceui.toml`, so a newer GraceUI under test is not a failure.

### 257. Two OFL fonts' glyph bytes read as an address to the sweep; a narrow allow line in Skyrizz

`2026-11-20` · FACT · `#privsweep` `#graceui` · refs: Skyrizz c7920ee

- (FACT) In the package's zip, privsweep's `email` rule matched byte runs in NotoSans-Regular.ttf and
  NotoSansMono-Regular.ttf (in their `glyf` and `GPOS` tables; their `name` tables hold no `@`). GraceUI downloads its
  fonts at configure time, so no push had swept them, and this package was their first upload.
- (DECIDED) An allow line in Skyrizz's `tools/privsweep/allow.txt` covers those two files' strings view only, under a
  `GraceUI/fonts/` path. A later font change is swept and reviewed again.

### 258. R1 spike.9: the group's own function read with our task and the entry followed in, the fields it reads, walks from the singleton and the entry, and tagged pointers

`2026-11-20` · DECIDED · `#gal` `#r1` `#task` · refs: WHY§248, WHY§247, engine a51ab03

- **Direct tail jumps** (DECIDED): in a function with no unwind data, a direct jump past the code decoded is a tail
  jump. So the tail function's jump to RVA 0xEB6040 becomes a call site, and the table's shape takes it as its end.
- **The group's function** (DECIDED): summarised before the call, with rcx the group's entry and rdx the task, and its
  own direct callees and tail jumps one level further (16 summaries at most, each callee once).
- **Field reads** (DECIDED, METHOD): every load based on the task, the entry or a value read from either is reported
  (`task_reads`, `entry_reads`): the field's offset and width, and what the value is used for along the flow (compared
  with which immediate, tested, stored where, passed to which call, called through, changed). Beyond the brief: a value
  changed by or, add, shift and the like stays followed, two changes deep, so that a tagged pointer's store is seen.
- **Roots** (DECIDED): spike.8's walks and diffs (with the 2 s control) run from the singleton and from the group's
  entry as well as from CSTask's instance.
- **Tagged pointers** (DECIDED): a word whose low 48 bits are our object's or our vtable's address under a non-zero
  high 16 bits is counted, in the diffs (`task_tagged`) and the whole-memory search (`tagged_found`), with its tag.
- **Classes** (DECIDED): the singleton's and the entry's classes by their RTTI, matched against the leads by name or
  name+"Imp" (O92's rule), reported as a lead's index only.
- **The fake game** (FACT): its group function now reads two task fields (one compared with 0x51), keeps the task as
  `task | 0x7E01 << 48` and answers 1. With `--task-gate` and our object's zero in the compared field, the task never
  runs, as in run 8. R1 reports both reads with the compare, the tagged store, one tagged copy (tag 0x7E01), the diffs'
  `task_tagged` words, and the entry's class matched to the hub's lead.
- **Also** (FACT): two existing stack-overflow cases had failed at 5c5b3bc. The fake game's recursion now starts at a
  fixed stack depth, and they pass.

### 259. G32's switch is a command of GraceUI's own space, not a setting

`2026-11-20` · DECIDED · `#graceui` `#notices` · refs: GraceUI:D181, GraceUI:G32

- (DECIDED) The per-mod Notices switch is a command of GraceUI's own space (`mods.notices`, `{id, on}`), its state in
  the Installed mods page's table, not a setting in the settings store: the mods are known only at run time, so a
  setting per mod would change the store's keys with each connect. The switch's one home is still the Mods page
  (GraceUI's invariant 12).

### 260. A quiet mod's notice is checked, answered ok and kept, never dropped

`2026-11-20` · DECIDED · `#graceui` `#notices` · refs: GraceUI:D181

- (DECIDED) A quiet mod's notice is checked as any other (a bad text or tone is still refused), then answered ok and
  kept on the Installed mods page, not drawn over the game. The mod cannot tell (in the spirit of "absence is silent"),
  and the player can still read what was missed. So EldenNet never relies on a notice being seen (S11.4.a).

### 261. A test export that read GraceUI's thread's state without a lock crashed a full harness run

`2026-11-20` · LIKELY · `#graceui` `#harness` · refs: GraceUI:D181

- (LIKELY) The quiet scenario crashed the full run (a read at address 8), though it passed alone twice. The likely
  cause: a test export reading a topic's JSON on the harness's thread while GraceUI's thread replaced it. With the
  export reading a copy under a lock (`GraceUI_Test_GetTopic`), it passed four times alone and in both toolchains'
  full runs.
- (METHOD) Run a new scenario in the full run too, not only alone; reading state another thread writes needs a locked
  copy, whatever an export's comment says about reading while idle.

### 262. GraceUI's kept settings and a new ui.scale were saved and told only when the menu next opened

`2026-11-20` · FACT · `#graceui` `#settings` · refs: GraceUI:D179, GraceUI:D180, GraceUI:D181

- (FACT) D179 wrote `user\settings.json` only from the menu's thread of work, so a change from a browser while the menu
  was closed was kept, and a new ui.scale told to the mods (D180), only when the menu next opened. GraceUI's thread now
  does both at every wake.

### 263. A pad chord's order is carried by splitting ABI 1's pad_chord word, key low and anchor high

`2026-11-20` · METHOD · `#graceui` `#input` · refs: GraceUI proposal 0005, GraceUI:D155, NPC§5.6

- (METHOD) An XInput button mask cannot say which of two buttons comes first. So GraceUI's proposal 0005 carries a
  chord's order by splitting `register_hotkey`'s `pad_chord` word: the key's bit in the low 16, the anchor's in the
  high 16 (EldenNet's chord: `XINPUT_GAMEPAD_BACK | (XINPUT_GAMEPAD_DPAD_UP << 16)`). GraceUI has refused to read that
  word until now (`not_yet`), so no mod can have relied on it.

### 264. An independent review given the code and the rules found GraceUI's faults at the joins between modules

`2026-11-20` · FACT · `#graceui` `#security` · refs: GraceUI:D183, S11.2.q

- (FACT) One read-only reviewer, given the code and the rules but not our conclusions, probed a live grace-dev on
  loopback. It found the server's wire layer sound: Host, Origin, Fetch Metadata, the cookie and ticket, the CSP and
  headers, static files, HTTP and WebSocket framing, the bridge's routing, the settings store, and the web app's text
  and secrets. It also found six faults in the layers the server's own tests do not reach: a world-state flag wired
  only to a log line, a rule kept only by the page lint, a reserved id never reserved, and others.
- (METHOD) Review the joins between modules, not only each module, and give the reviewer the code, not our
  conclusions.

### 265. A space's settings publications are held to its registration, and a secret's value is masked rather than refused

`2026-11-20` · DECIDED · `#graceui` `#security` · refs: GraceUI:D183, GraceUI's invariant 5

- (DECIDED) `publish` on settings must send the registered schema and description or none; a mismatch is refused. A
  secret's value in a publication is replaced by "set" or "" with one log line rather than refusing the frame, so a
  mod's mistake costs it nothing visible and leaks nothing. EldenNet publishes secrets' status only.

### 266. GraceUI 0.2's in-game menu draws a mod's settings and shortcut rows only, so EldenNet's Home cards show in the browser alone

`2026-11-20` · FACT · `#graceui` `#engine` · refs: GraceUI:D171, GraceUI:D181, S11.4.a, engine 75e6303

- (FACT) GraceUI 0.2.x's in-game menu watches only a mod space's `settings` topic. Its component blocks (key-value
  lists, check lists, tables, a log tail) read "Shown in the browser view." in game. So in game EldenNet's Home and
  System show only settings and shortcut rows. The start state, the banner's reason and a failed start are in game
  only in the banner notice, which a player may quiet (D181), and in the F11 view (developers).
- (DECIDED) Asked of GraceUI: draw a mod space's key-value and check lists in game, watching their topics while the
  page is open. Until then, D181's rule that what the player must know is also on EldenNet's pages holds in the browser
  alone.

### 267. EldenNet's first pages: settings written on T11, a refused settings file never written over, and the developer settings on Settings › Developer

`2026-11-20` · DECIDED · `#engine` `#pages` · refs: PAGES§4.5, ARCH§1.2, engine 75e6303

- (DECIDED) The pages' settings changes go through T11, not T6. T6 posts the session's edits after 300 ms of quiet as
  one exchange; T11 reads, edits and renames settings.toml over. T6 stays non-blocking (ARCH §1.2). A change from the
  pages rewrites the file through toml++: keys and tables kept, comments and layout not (acceptable while
  settings.toml is EldenNet's to write, D14).
- (DECIDED) While settings.toml is refused at start (the defaults in use), every change from the pages answers
  `not_now` with words saying why, so nothing writes over the player's file. A "Reset the file" behind a destructive
  confirmation can come later.
- (DECIDED) The `debug.*` settings live on Settings › Developer, their home in PAGES §4.5, with Logs' shortcut row
  for the log's detail; System holds none. The space's settings description takes settings.toml's schema number (2).
- (DECIDED) EldenNet's GraceUI floor rises from 0.2.3 to 0.2.5 with the release that ships the pages, so that no
  player meets a GraceUI without D183's server fixes.

### 268. A browser of our own joins GraceUI's server from Linux while the fake game runs under Wine

`2026-11-20` · METHOD · `#engine` `#graceui` `#harness` · refs: engine 75e6303

- (METHOD) `tests/pages/browser.py` joins GraceUI's server on loopback from the host while the fake game runs under
  Wine: the page load's cookie, a ticket, a WebSocket with masked frames. The harness's cases take a `during` hook for
  it, so a case reads every topic and sends commands (accepted, `invalid_value`, `confirm_required`) in both views.

### 269. A range-for over a member of a temporary nlohmann::json dangles in C++20

`2026-11-20` · FACT · `#engine` · refs: engine 75e6303

- (FACT) `for (x : json::parse(t)["k"])` iterates a member of a temporary that is destroyed before the loop's body:
  the fake GraceUI's first publish check found it. Bind the parsed value to a name first.

### 270. One body per topic: GraceUI's components read fixed fields, so a page with two lists needs two topics

`2026-11-20` · FACT · `#graceui` `#pages` · refs: GraceUI:DESIGN§6.5, engine 75e6303

- (FACT) GraceUI's components read fixed fields of a topic's body (`items`, `checks`, `columns` and `rows`), so a page
  with two key-value lists needs two topics. EldenNet's slice adds `system.compatibility` beside `system.status`.
- (DECIDED) Asked of GraceUI: either list `system.compatibility` in its DESIGN §6.5, or add a block prop naming the
  body's field (`props.field`), so that one topic feeds several blocks. Also asked: an empty-state text per block
  (Problems' "No problems"), a newest-first log tail, a greyed page state for a mod at L1, and the in-game shortcut row
  drawn as the control rather than a link.

### 271. GraceUI's menu kept its copy of a topic only while it watched it, so a static status watched first by the browser would never have reached it

`2026-11-20` · FACT · `#graceui` `#pages` · refs: GraceUI:D184, GraceUI:D171

- (FACT) A mod publishes on a change. The menu's copy of a topic was kept only while the menu itself watched it, so a
  topic the browser watched first reached the menu only at the mod's next change, and a static status never would
  have. Since GraceUI 0.2.6 the registry keeps a topic's newest state while any view watches it, and the menu's
  subscription is answered at once.

### 272. F11's view lists Augur's events in Augur's own words, en-core's changes kept second, each event's source marked by colour

`2026-11-20` · DECIDED · `#engine` `#augur` `#overlay` `#test` · refs: DESIGN§14, ARCH§2.2, ARCH§2.3, ARCH§8, S10.2.d, engine 2f2f843

- (DECIDED) The view shows Augur's last ten events, newest first: kind, class, subject kind and index, a reason word
  with at most one number (a place's id, a count, a slot), merges, the audience (radius, rows, first row), and the
  age. Never an HP fraction, a distance, a handle or a name. en-core's last five changes follow, under DESIGN §14's
  "ladder": they hold what Augur's lifecycle events leave out (the gate's reason, the banner, the badge). One
  vocabulary, en-augur's `words.hpp`, serves the golden files and the view. T6 keeps both lists in fixed rings and
  hands the view a fixed snapshot.
- (METHOD) Each event's line starts with a 6 × 6 square in its source's colour (lifecycle, world, EldenNet's own):
  GraceUI's test exports read pixels, not text, so a square of a known colour lets the fake game count events drawn,
  by source.
- (DECIDED) The words are mapped in the DLL, since en-overlay depends on en-proto alone (ARCH §2.3).
- (FACT) On the fake game with GraceUI 0.2.5's test build (`launcher-eldennet-graceui-events`), F11 showed the ring
  full: five world events and five lifecycle events, none before F11 or after it closed. Replaying the case's
  recording gives the same 11 events, the view holding the last ten.

### 273. Check an "as the other view does" premise against the code before building to it

`2026-11-20` · FACT · `#graceui` `#pages` · refs: GraceUI:D189, GraceUI:DESIGN§4.7

- (FACT) Proposal 0006's fifth item, drawing a shortcut row's control in game "as the browser does", rested on the
  browser drawing it; the browser drew only the value as text, while GraceUI's DESIGN §4.7 asks for the control and
  the link. Built to §4.7 in both views (D189).
- (METHOD) Check a premise about what another view or module does against its code before building to it. EldenNet's
  request 6 of its first pages had carried the same premise.

### 274. EldenNet's AI pages: keys as GraceUI secret settings, kept with a one-user access list, and what GraceUI 0.2 cannot yet do for them

`2026-11-21` · DECIDED · `#engine` `#pages` `#security` · refs: PAGES§4.6.2, PAGES§4.5, ARCH§11.5, ARCH§12.1, O88, GraceUI:D183, engine 80ef2fe

- (DECIDED) Each provider's key is a GraceUI secret setting: write-only, published as its status ("set" or ""),
  never logged or put in a report. It is held as en-llm's Secret (wiped after use) and written to secrets.toml through
  `write_private`, whose access list is the current user's alone, as ARCH §11.5 says. PAGES §4.5's "keys are not
  settings rows" is amended in practice and is to be amended in text: GraceUI 0.2's only write-only input is a secret
  setting.
- (DECIDED) eldennet.dll now imports advapi32.dll for that access list, together with the C runtime's thread-support
  functions from kernel32, all reviewed into `tests/imports.allow` (ARCH §12.1). A key also passes briefly through
  GraceUI's frame and the poll buffer, which EldenNet cannot wipe: best effort, accepted.
- (DECIDED) Until GraceUI has a form, a per-row action and a picker fed at run time (asked of GraceUI):
  - providers come only from the shipped presets, and keys only under a preset provider's name;
  - Remove is "Remove unused providers", with D183's confirm text;
  - a custom provider and its key stay in the player's files.
- (DECIDED) The handle (T7 and T8) is made on the first self-test or model list, not at Ready, until v0.2's dialogue
  needs it. A hand edit of providers.toml during a session is read at the next start, until T10's hot reload.
  en-llm's own log lines are not copied into eldennet.log, since a server's text could reach them. The AI pages grey at
  L1 only: PAGES §4.4 keeps them usable at L2 and when paused, with the cause leading.

### 275. Keeping std::filesystem's imports out of eldennet.dll, the handle told the gate when made, and toml++'s output

`2026-11-21` · FACT · `#engine` · refs: ARCH§12.1, engine 80ef2fe

- (FACT) Linking en-llm into eldennet.dll pulled in std::filesystem's imports through config.cpp's folder loader
  (CopyFileW, CreateHardLinkW, SetCurrentDirectoryW and more). Moving `load_config_dir` to a file of its own keeps
  them out, since a static library's object is linked only when named.
- (FACT) en-llm's handle must be told the core's gate when T6 makes it, not at the next wake: the first self-test from
  the browser otherwise ended GateClosed at once.
- (FACT) toml++ 3.4.0 writes strings in single quotes and a table's keys in name order, so a rewritten models.toml
  comes back sorted.
- (METHOD) The AI case's fake model server runs in the harness's own process. The invented key is made each run and
  never stands on a command line. After the case removes its provider, no file, frame, log or report line of the run
  holds it.

### 276. A key-shaped value in a test and the sweep: allowed by a pattern, since an allow line holding the literal is itself a finding

`2026-11-21` · METHOD · `#privsweep` `#engine` · refs: Skyrizz tools/privsweep/allow.txt, WHY§257

- (FACT) The AI page's test needs a key-shaped value to prove that a key never reaches a topic, log or report. The
  sweep's secret rule refused the push for that invented value in `src/en-overlay/tests/test_space.cpp`.
- (METHOD) An allow line in Skyrizz names that one file and matches the value by a pattern, not the literal. A line
  holding the literal was itself refused by the sweep on Skyrizz's own push, so it was rewritten before the unpushed
  commit was pushed.

### 277. The game's default PC layout leaves `T` free: the talk key's default (O94), to be checked in game by S4

`2026-11-21` · LIKELY · `#input` `#hotkeys` · refs: O94, NPC§5.1, S11.1.a

- (LIKELY) Published guides to the game's default PC keyboard and mouse layout agree on: W, A, S, D to move; Alt to
  walk; Space to dodge, backstep and dash; F to jump; X to crouch; Q (or the wheel click) to lock on; R to use an item;
  E for the event action, and with the mouse buttons to two-hand, with the arrows for the pouch; G for the map; Esc for
  the menu; the left button to attack and the right to guard, with Shift for the strong attack and the skill; the
  arrows to switch spells, items and armaments. None binds `T`. G and Esc cannot be rebound in the game. They
  disagree in two places (one gives C for crouch, one Ctrl for guard). One counts 22 default keyboard binds.
- (METHOD) Found by web search on 2026-11-21; the guide pages themselves were outside this session's network, so the
  list is from the search results (Shacknews' "Elden Ring controls and PC keybindings", PC Gamer's "Elden Ring's
  half-assed mouse and keyboard controls", keymap.io's Elden Ring page). Not a FACT until S4 presses `T` in game in
  the default layout and the game does nothing.

### 278. GraceUI's browser rail kept categories with no page, which the game's rail leaves out: compare both views on a real mod's pages

`2026-11-21` · FACT · `#graceui` `#pages` · refs: GraceUI D191 (feat/foundations c397464), S11.2.t

- (FACT) The browser view's rail kept a category with no page whenever no page was of a later increment (its
  increment check returned early), while the game's view skips any category with no page present. EldenNet's space
  declares six categories with no page yet (Voice, Characters, Map, History, Behaviour, Tools), so the two views
  differed. Fixed in GraceUI's D191 (feat/foundations c397464).
- (METHOD) Compare both views' pictures of a real mod's pages, not only the demo space's goldens.

### 279. The lamp's four tones measure 6.0 to 8.2:1 on their ground, its word 10.2:1; glint is a pale blue-grey

`2026-11-21` · FACT · `#graceui` `#lamp` · refs: GraceUI invariant 13, S11.4.a

- (FACT) On the lamp's ground, in both themes: moss 7.7:1, ember 7.0:1, glint 8.2:1, rot 6.0:1; the word 10.2:1.
  A graphic needs 3:1, and each clears the 4.5:1 GraceUI asks of readable text. Glint is a pale blue-grey, not gold.
- (FACT) The lamp is drawn only in the browser view, in the top bar beside the theme switch, not beside the name;
  the game's menu has none today.

### 280. System One, the decision protocol SkyrimNet uses for its classifier, is published and offered by OpenRouter and Ollama

`2026-11-21` · LIKELY · `#llm` `#decisions` · refs: O96, ARCH§11.8, SN01§6.1

- (LIKELY) Jev is TypeSafe's first System One model: it answers typed questions about a given state with an option and
  probabilities, not prose, and returns no reasoning. System One is the protocol `POST /v1/systemone`. OpenRouter offers
  Jev through two of its APIs, Decisions and System One; Ollama added a matching endpoint in 0.35; TypeSafe's SDKs work
  against any server that implements System One, by its base URL. The provider receives the state sent, as any cloud
  provider does. One provider's documentation calls Jev experimental.
- (METHOD) From web search results on 2026-11-21 (Spring AI's System One decision page, OpenRouter's Jev guide,
  LangChain's TypeSafe integration, AIMLAPI's model page); the pages themselves were not read. A client of ours would be
  written from TypeSafe's and OpenRouter's published references, never from SkyrimNet's (O84).

### 281. Row actions and row secrets are declared on a table's block in the page data, not in a topic's body, so registration can check them

`2026-11-21` · DECIDED · `#graceui` `#pages` · refs: GraceUI:D192, GraceUI:D194, GraceUI:G34, S11.2.u

- (DECIDED) Proposal 0007 sketched row actions and row secrets as columns in a topic's body. GraceUI 0.2.8 declares
  them on the table's block in the page data instead (`row_actions`, one to three, and `row_secret`), because the
  safety the proposal promised needs them known at registration, before any body is published: a destructive row
  action refused without its confirm words, the server requiring `"confirm": true`, and a secret command's result
  sent back without a body. A secret column's cells are only "set" or "".

### 282. Folding O96 and O97: the player's addressee is picked before the reply, and pairs make one speaker with several addressees a decision

`2026-11-21` · LIKELY · `#decisions` `#llm` `#npc` · refs: O96, O97, ARCH§11.8, NPC§9.2, NPC§5.1, WHY§280, S10.1.j

- (LIKELY) The player's addressee decision must finish before `PlayerSaid` is committed, since the line's direct and
  overheard audience and its conversation depend on it. So it runs in series before the reply, within the first-page
  target, with the `decisions` slot's first-token time as its deadline (1.5 s, 3 s local), after which the header's
  target answers. Sending the reply beside it would save the time on a cloud provider only. Marked for the owner's
  review; X3 measures the added time.
- (LIKELY) Counting options as pairs under "no call with one option" turns a single speaker with two legal addressees
  into a decision; the reservation rule keeps those calls within the talk window. Marked for review.
- (METHOD) A name the player types for a candidate is replaced by that candidate's neutral label before the call, which
  keeps NPC §9.2's rule of no runtime names in a decision's inputs. Marked for review.
- (DECIDED, by reuse) A move to another addressee reuses NPC §5.7's `retarget` end: no new state, and memory jobs run
  per conversation as before. The header's new name is the only cue in v1 (marked for review). Who may answer the
  player's line, and the addressees each motive allows, are written in NPC §9.2 and §9.4 (marked for review).
- (FACT) PAGES §4.6.2's Requests filter has no purpose for decision calls; a word for it is left to the pages work.

### 283. en-llm took any host starting with "127." for this PC, letting a key go in clear text elsewhere; fixed

`2026-11-21` · FACT · `#engine` `#security` · refs: ARCH§11.1, S11.4.a

- (FACT) en-llm's loopback test took any host whose name began with "127." for this PC, so a provider at a name such
  as `127.example.invalid` over `http://` passed ARCH's rule that no key crosses a network in clear text. Fixed by
  en-proto's `is_loopback_host` (a dotted 127.0.0.0/8 address, `localhost`, `[::1]`), shared with the pages' form.

### 284. One base-URL check for the form, the providers file and the key rows, each refusal in words

`2026-11-21` · DECIDED · `#engine` `#pages` `#security` · refs: O98, ARCH§11.1, PAGES§4.6.2, S11.4.a

- (DECIDED) en-proto's check (`check_base_url`, with `url_extras`, `is_private_host` and `reach_of`) is the one rule
  for AI › Providers' form, the hand-written providers file and the key rows: `https://` to any host; `http://` to this
  PC; `http://` to a literal private address (RFC 1918, fc00::/7) as a home-network server with no key (O98); never a
  host name over `http://`, never a name, password, query or fragment in the URL. A key (`key_ref`, or a header's
  secret) is refused on any provider reached by `http://` on another computer.
- (FACT) A hand-written `http://` to a home server by name is now refused; only a literal private address passes.
  Each refusal is named, with its line, on System › Status and Home's Problems.

### 285. The AI pages' pickers: path-shaped model ids left out, a job's model row holds the model now, provider name rules

`2026-11-21` · DECIDED · `#engine` `#pages` `#privacy` · refs: PAGES§4.6.2, PAGES§4.9, S11.4.a

- (DECIDED) A server's listed model ids reach AI › Models' pickers alone, and an id shaped like a path is left out,
  since a local server can list model files by a path holding the Windows user name (PV5).
- (DECIDED) A job's model row holds the job's model now (the preset's or the player's pick); its reset removes the
  pick. A provider of one's own takes a name of 1 to 32 characters, unique ignoring case, and never one a key is
  already kept under. Remove refuses a provider a job uses, naming the jobs.

### 286. GraceUI's server pings each WebSocket with a protocol message every 15 s; a test browser must answer it

`2026-11-21` · FACT · `#graceui` `#harness` · refs: tests/pages/browser.py

- (FACT) GraceUI's server sends `{"t":"ping"}` on each WebSocket every 15 s, not a WebSocket ping frame, and closes the
  socket after two unanswered. EldenNet's test browser answered only the frame, so a session longer than 45 s closed;
  it now answers `{"t":"pong"}`.

### 287. Test literals and the sweep: private addresses built at run time, invented names clear of its endpoint words

`2026-11-21` · METHOD · `#privsweep` `#harness` · refs: WHY§276, S11.4.a

- (METHOD) The sweep refuses a private IPv4 literal in a test, and its endpoint rule matches words such as `.home`,
  `.lan` and `.local` even inside an invented name. Tests build private addresses from their numbers at run time and
  use names such as `storeroom.invalid`, rather than adding allow lines.
- (METHOD) A fake-game case whose browser starts from the game's report should have reports early: the AI case's
  browser waited for its first report and finished 0.6 s before the menu needed its result.

### 288. A row's changed mark is the space's own list, not a value compared with its default

`2026-11-21` · DECIDED · `#graceui` `#settings` · refs: GraceUI:D198, S11.2.v

- (DECIDED) Both of GraceUI's views take a row's changed mark from the settings body's `changed` list, in the protocol
  from the start and published by every space, rather than comparing a value with its default, which misreads a
  setting whose default stands for something the space resolves (EldenNet's per-job model picker, whose default is the
  preset's model). A new `default_label` field, which would have been protocol 1.3, was not needed.

### 289. R1 run 9: the group's function hands our untouched task to a second singleton's method; nothing in memory keeps it

`2026-11-21` · FACT, with a LIKELY reading · `#gal` `#r1` `#task` · refs: WHY§235, WHY§240, WHY§248, WHY§258, S10.1.f

- **The group's function** (FACT, spike.9, game 2.7.2.0 ww, no other mods): RVA 0xEB6040 (114 bytes, 29 instructions)
  null-tests the task (null answers 0) and reads nothing else of it; it reads one 32-bit word of the entry, at +0x54,
  and calls no slot of the entry. It loads a second singleton, G, from the data slot at RVA 0x48642A0 (calling the same
  null-singleton report pair as the wrapper only when the slot is null), then tail-jumps through an 8-byte stub that
  zeroes r9 to 0x2658FB0, with rcx G, rdx our task, r8 the entry's word and r9 0. `task_reads` and `task_stores` are 0
  to depth 3, so the 1 the call answers is 0x2658FB0's, and whatever drops our task is there or below.
- **The classes** (FACT): the first singleton and the entry are both RTTI-classed, and neither matches a lead.
- **The memory** (FACT): at 2.0 and 21.6 s after the call, 8.6 of 10.7 GB read: no plain, inside or tagged copy of our
  object's address, and none of our vtable or slots. Walks from the singleton (204 blocks) and group 110's entry (775
  blocks) changed nothing beyond the control but one counter, which run 8's control shows changing before any call.
  Our object was never written and no slot was called in 21 minutes at the title screen.
- **One change unexplained** (FACT; its cause SPECULATION): in runs 7 to 9 an array behind CSTask's instance at +0x3B0,
  then +0x3D0, elements 240 bytes apart, took fresh heap pointers after the call, never in a control. It holds nothing
  of ours.
- **So** (LIKELY): G's method takes our task and answers true without keeping it, because of something it reads or
  checks. A list that runs only in a world is ruled out by the memory and by run 4. The leading candidate (SPECULATION)
  is our zeroed object, with no derived base class (R1 never found the game's task shape), lacking fields a real task
  has; a phase check at the title, or a function that does not register at all, remain open.
- **Next:** spike.10 summarises 0x2658FB0 through the stub, with the task followed in, and its callees one level
  further; reads the entry's word, G's slot and class, and whether G is the object the entries reach at +0x58; walks
  from G with its control; times and classes the array's change; and reads what the game's own 8 call sites pass as
  the task. A fix waits on that: building our object in the game's own task layout, never by guesswork.

### 290. R1 run 9: the launcher's log stopped at the call, so no 30 s text showed and the crash watcher was blocked

`2026-11-21` · FACT, with a LIKELY reading · `#launcher` `#r1` `#console` · refs: O71, O90, WHY§248

- **The log** (FACT): run 9's launcher log ends at the call (25.5 s), with no status, no-confirmation, end, lint or
  finish line, though the game ran 21 more minutes and EldenNet logged and reported to the end. Run 8 carries them all.
- **So** (LIKELY): the watch loop blocked right after it. Its next step is a console write under the output lock, and
  the launcher writes the console before the file, so a suspended console stops the file log too. Clicking inside a
  classic console window starts a selection, which suspends its output until Esc (SPECULATION as the cause here).
  The owner never saw the 30 s text, which explains the 21-minute wait, and the crash watcher (O71) was blocked.
- **Next:** the launcher turns QuickEdit off for its console, writes the file first, and keeps console output off the
  watch loop (a writer thread with a bounded queue that drops console lines rather than waiting). Until then, the
  README asks the player not to click inside the console.

### 291. R1 spike.10: G's method read through its stub, the entries' words, G's walk, the array's views and the game's own call sites

`2026-11-21` · DECIDED, with FACT on the fake game · `#gal` `#r1` `#task` · refs: WHY§289, WHY§248, WHY§240, S10.1.f

- **G's method** (DECIDED): the group function's first tail jump (else call) that passes our task with rcx loaded from
  a 64-bit RIP-relative slot is the site, and rcx there is G. A stub of 16 bytes or fewer, with no call, branch or
  return and a direct tail jump last, is followed without spending a depth level, with the registers it zeroes. G's
  method is summarised at depth 3 and its direct callees and tail jumps at 4, within the group's 16 summaries, with
  `task_reads`, `task_stores`, `g_reads` (loads based on G) and `word_uses` (the entry's word compared, tested, stored,
  passed, changed or used as an index), the word followed through 32-bit moves, movsxd and lea.
- **A fix** (METHOD): a direct jump to a function start from the exception directory now ends a summary; spike.9 let a
  forward jump extend the code decoded.
- **The numbers** (DECIDED), just before the call and 2 s after it: G's slot (zero or heap) and its class against the
  leads (O92's rule); whether CSTask's instance holds G at +8 and the entry at +0x58; group 110's word, and over every
  entry how many hold their own index and how many distinct words there are. G is walked as a fourth root, with its
  control, unless it is a root or the object at +8 or +0x58. The array behind +0x3B0 and +0x3D0 is read again at 50 and
  250 ms and at 2 s, its new rdata words classed, a small word compared with the main window's thread and with the
  calling thread.
- **The game's own tasks** (DECIDED): at each of the first 16 call sites of register_task, the task argument read back
  along the straight line to its last write (member, static, load, result, argument, stack or other, with one level of
  its base); the sibling at 0xEB5540 summarised to depth 4, 12 summaries at most. Past 256 KiB the forms go first, the
  group function and G's method last (eleven passes; the fake game's report is about 210 KB).
- **On the fake game** (FACT): its group function reproduces run 9's shape. Spike.10 finds G's method through the stub,
  with 2 task reads (one compared with a small immediate), 3 reads of G and 4 uses of the word; G on the heap, classed,
  no lead, held at neither offset; every word its entry's own index; the array's change seen at 50 ms and the main
  thread's stamp from 250 ms; the four call-site forms as built. With a strict G the call answers 1 and keeps nothing,
  as in run 9.
- **A self-test kept honest** (METHOD): with r9 zeroed by the stub, the fake game's swapped call (the hub passed as the
  task) ran and faulted; the fake now keeps the hub it was given, so that case is refused at the hub again.

### 292. The launcher's console off its watch loop: the log first, a bounded queue, QuickEdit off

`2026-11-21` · DECIDED · `#launcher` `#r1` `#console` · refs: WHY§290, O71, O90

- **The order** (DECIDED): every line goes to launcher.log first, then into a queue of 256 lines for one console writer
  thread, under the same lock; a line that finds the queue full is dropped and counted, never waited on. The first drop
  is noted in the log, and the end logs the totals; at exit the writer gets one second at most.
- **QuickEdit** (DECIDED): with the console (`--console`, `--probe`) the launcher turns QuickEdit off while it runs and
  restores the mode it found; with no console (a pipe, as in the tests) it changes nothing.
- **Tested** (FACT): with the console writer blocked after three lines, launcher.log kept every line to the end (the
  status lines, the no-confirmation step at 43.6 s, both lints and the end), with 79 console lines dropped; a crash with
  the console blocked is still recorded. README-R1.txt asks players not to click inside the console until this
  launcher has replaced the old one.

### 293. R1 run 10: G's method wraps our task in a 32-byte request and runs it at once or queues it; the task itself is never read

`2026-11-21` · FACT, with LIKELY and SPECULATION readings · `#gal` `#r1` `#task` · refs: WHY§289, WHY§291, S10.1.f

- **G's method** (FACT; RVA 0x2658FB0, 190 bytes, reached through the stub): it answers 0 when the task is null or G's
  word at +0x40 is zero. Otherwise it builds a 32-byte request on its stack: a vtable of its own (rdata RVA 0x32990A0),
  our task at +8, G's word at +0x48 at +16, the entry's word at +24 and r9's low byte (0) at +28. When G's byte at
  +0x38 is zero it calls 0x1267F0 with the request and answers what that answers; otherwise it allocates 32 bytes
  through an allocator object (data slot 0x4863D60, its slot at +80), copies the request in, and calls 0x26A4C10 on
  G's +0x18, which locks (+8), adds the request to a container (+16) and unlocks. It never reads our task.
- **The rest** (FACT): 0x1267F0's exception-directory extent is 68 bytes, yet its branches go to +126 and +267, so most
  of it was not read. G is on the heap and unclassed, at neither +8 nor +0x58. The word is 0x9000006E (110 with high
  flag bits); all 169 entries' words are distinct, none equal to its index. Every walk, G's (1,285 blocks) included,
  changed as its control did; no copy of our task in 8.6 of 10.7 GB at 2.3 and 22 s; the array behind +0x3B0 did not
  change this time. Of the game's 8 call sites, 7 pass their own function's argument and 1 a load from it (+0x218).
- **The launcher** (FACT): its fix held: the no-confirmation step, both lints and the end are in the log; 76 console
  lines, none dropped.
- **So** (LIKELY): registration is a deferred request, and the request does the real work, now (0x1267F0) or later
  (G's queue). With no copy of our task left at 2.3 s, the stack path (byte +0x38 zero) is the likelier
  (SPECULATION). Either way, what drops our task is in 0x1267F0's unread part or in the request's own methods.
- **Next:** spike.11 reads the fields of G the method reads (the byte says which path), the request's vtable (class,
  slots) and its methods with its fields followed, 0x1267F0 whole, G's queue across the call, the class and bases of
  G's +0x48, and the game's own requests in memory, whose +8 leads to the game's own tasks and their classes.

### 294. A look review made from a real mod's pages finds what the demo space's goldens cannot

`2026-11-21` · METHOD · `#graceui` `#review` · refs: S11.2.d, GraceUI's D199

- **The review** (METHOD): the owner's look review was made from EldenNet's pages at engine 8008dd1 as well as from the
  demo space, in game (graceui.dll in the D3D12 harness under Wine, 1920×1080) and in the browser, with invented data
  only. It found a fault the demo space's goldens could not: a picker whose default is empty was drawn blank, which
  EldenNet's per-job model pickers are. GraceUI fixed it as D199 (0.2.10): an empty default reads "Default" and is
  offered first; EldenNet has nothing to change.
- **The scenario** (METHOD): GraceUI's page-shots scenario now walks GraceUI's own pages, saves each page's end, and takes
  a theme and a pointer position, so the next review is one command per set.

### 295. R1 spike.11: the request G's method builds, its methods read in parts, G's fields and queue, and the game's own requests

`2026-11-21` · DECIDED, with FACT on the fake game · `#gal` `#r1` `#task` · refs: WHY§293, WHY§291, S10.1.f

- **Parts** (METHOD): a summary whose code goes past its exception-directory extent is decoded in parts: the entries
  chained to it (UNW_FLAG_CHAININFO), and past a conditional branch's target in no entry, linear code up to 4 KiB until a
  return, a jump through memory or a direct jump out (`unlisted`); 8 parts at most. Only conditional branches open an
  unlisted part, since a direct jump into no entry is how a function tail-jumps to a leaf.
- **The request** (DECIDED): found in G's method by its shape, not run 10's offsets: an rdata address stored at
  [rsp + d] with the task at [rsp + d + 8]; each field's role from the method's entry state (vtable, task, manager from
  G, the word, the zeroed register). Its class and slots (16 at most); its first six slots summarised at depth 4 with the
  request in rcx, and the now path's function too, one level further, within 24 summaries: a load of the request's task
  field is the task, so the task's later reads are `task_reads`.
- **G and its queue** (DECIDED): the fields of G its method reads, just before the call and at about 50, 250 and 2,000
  ms; `path_taken` now or queued, by the byte compared before the allocator. G's queue is the fifth root, walked at the
  same times, each block whose first word is the request's vtable read as a request.
- **The game's own** (DECIDED): each memory search counts words equal to the request's vtable outside our memory, reads
  each of the game's (32 at most) as a request, and for each class of their tasks (8 at most) reports its slots and one
  task's first 16 words as kinds. The classes' bases (type-descriptor RVAs only) are reported for G's manager, the
  request, CSTask's instance, the group's entry and those tasks, with the bases they share.
- **On the fake game** (FACT): both paths are found and reported. On the now path the second, unlisted part reads two
  fields of the task; with a strict check it drops ours, answering 1 with no copy anywhere, as in run 10. On the queued
  path our request sits in G's queue at 50 and 250 ms and is gone by 2 s. The game's own two waiting requests are found
  in each search, their tasks in two classes sharing one base.

### 296. R1 run 11: the request runs at once and looks the group's word up in the manager's map; our task never reaches the code that uses it

`2026-11-21` · FACT, with LIKELY and SPECULATION readings · `#gal` `#r1` `#task` · refs: WHY§293, WHY§295, S10.1.f

- **The path** (FACT): G's byte at +0x38 was 0 at all four reads, so the request took the now path: 0x1267F0, which is
  the request's own slot 1 (its vtable at rdata 0x32990A0 has no RTTI class). Decoded in three parts (68 bytes primary,
  58 and 164 chained), it reads the request's word, manager and task.
- **What it does** (FACT, from its reads and calls): it tests the word. With a word of 0 it hands our task to
  0x26A3460 for each entry of a list on the manager (begin and end at the manager's +0x30 and +0x38). With any other
  word, ours being 0x9000006E, it walks a tree from the manager's +0x18, testing a byte at each node's +25 and comparing
  the word with each node's key, and on a match calls 0x26A3460 with the node's value, our task, the word and the
  manager. Only after that does it test the request's byte at +28 (0 for us, zeroed by the stub) and, on another branch,
  call our task's slot 1.
- **The rest** (FACT): our slots were never called and no copy of our task was found anywhere, as before. No request of
  the game's own was in memory and the queue held none: the game's requests are on the stack and gone at once. Every
  entry's word has its own index in its low 16 bits and 0x9000 in its high 16 bits, so 0x9000006E is group 110's as the
  game names it. Of the classes, only CSTask's instance and the group's entry have RTTI; G and its manager have none.
- **So** (LIKELY): the tree is an MSVC `std::map` keyed by the group's word (the node's byte at +25 is its nil flag, and
  the head's +8 is the root). Our task reaches 0x26A3460 only if group 110's word is in the manager's map. The leading
  reading (SPECULATION) is that at the title screen it is not, so the request finds nothing, drops our task and answers
  1. If the word is there, 0x26A3460 drops it instead.
- **Next:** spike.12 reads the manager's map at the title and, after a save is loaded with no second call, in the world:
  its size and keys, and whether group 110's word is among them; it summarises 0x26A3460. If the word appears only in
  the world, the call moves there, by the owner's decision.
- **Later (2026-11-21):** two numbers above are wrong, misread from run 11's decimal RVAs: the function the match calls
  is 0x26A3260 (40,514,144), not 0x26A3460, and the call before our task's slot on the other branch goes to 0x1EC9340
  (32,281,408), not 0x1EC8E40. Run 11 had summarised both at depth 5, but the report's size passes left them out.
  Spike.12 finds both from the code, never by number (WHY§299).

### 297. The AI pages' fourth slice: a job's provider, a key's other states, the changed mark, and the floor at GraceUI 0.2.9

`2026-11-21` · DECIDED, with FACT readings · `#engine` `#pages` `#graceui` · refs: S11.4.a, O98, WHY§283, WHY§288

- **The floor** (FACT): GraceUI 0.2.8 takes page data with D197's cells and D198's `changed` list without refusal, but
  draws Refused and Not needed as Missing and marks every picker row changed; EldenNet's floor rose to 0.2.9.
- **A job's provider** (DECIDED): written at once with the job's model when the provider's list holds it; otherwise held
  in the space, unwritten, until one of its models is chosen. A reset of either row clears the job's pick, and a job's
  rows count as changed while the providers file holds `[slots.<slot>]`, even when the pick equals the preset's.
- **A key's cell** (DECIDED): Not needed for a provider with no key's place until a call asks for one (401 or 403), then
  Refused, with the field; Refused also when a key sent was refused. A server on the home network (O98) never takes one.
  A refused provider is no job's choice until its key is pasted again.
- **Two faults fixed** (FACT): a reload of en-llm cleared its record of refused keys, so an unrelated edit made a refused
  key read Set; the DLL now keeps each refusal until the provider answers or its key changes. And a fallback with no
  provider of its own would have moved silently with its job; its provider is now written.
- **A pitfall again** (METHOD): a range-for over a member of a temporary dangled in a new test, the third time
  (WHY§269); hold the body in a variable first.

### 298. A schema's field order is held by a round-trip test; run every tree's full suite before pushing a schema change

`2026-11-21` · FACT, with a METHOD · `#graceui` `#pages` · refs: G36, GraceUI's D202, S11.2.x

- **The fault** (FACT): grace-pages' round-trip test holds the built-in page data to the schema's field order (the
  encoder's). A section written in TOML with its new fields (D202's chip, status and actions) before its blocks failed
  it in every Linux tree; fixed before GraceUI 0.2.12 was pushed.
- **The method** (METHOD): run every Linux tree's full ctest before pushing a schema change, not only the suites the
  change touches.

### 299. R1 spike.12: the manager's map read at the title and in the world, the match's call, and the owner's world step

`2026-11-21` · DECIDED, with FACT on the fake game · `#gal` `#r1` `#task` `#launcher` · refs: WHY§296, WHY§295, S10.1.f

- **The map's layout** (DECIDED): read from the now path's function's summary, never from run 11's numbers: the head is
  the manager's field used as a base, the root the head's +8, the nil byte the root's 1-byte field compared with 0, the
  key the memory operand of the word's compares, the value the match call's rcx (else 8 bytes past the key). MSVC's
  children (+0, +16) are assumed; another root displacement fails as `shape`.
- **The walk** (DECIDED): in order, with an explicit stack; the head and every node on the heap, nil bytes as expected,
  no node met twice, 4,096 at most; a failed check names itself and reads no further. Reported: the node count, the keys'
  low halves in order (256 at most) and high halves counted, whether group 110's word is present or else its nearest keys
  below and above, and the values' kinds and classes; the manager's list's count and kinds too.
- **When** (DECIDED): just before the call, at about 50 ms, 250 ms and 2 s after it, then every 10 s while the game runs;
  kept are the first, each reading where the node count, the word's presence, a failure or the stage changes (24 at
  most, each logged), and the newest. The stage is world while a derived singleton's instance points to an object of a
  character class from the leads (title otherwise, loading screens included), since R1 builds no world snapshot.
- **The two functions** (DECIDED): the match's call (0x26A3260 in run 11) is summarised at depth 5 with the map's value,
  our task, the word and the manager, and the other branch's call with the task; their callees at depth 6. Their forms
  and the now path's function's are kept to the last of the size passes; past 256 KiB the report is first written on one
  line (run 11's was 96 KB that way).
- **The owner's world step** (DECIDED): after the 2 s search, when our task has not run and the complete search found
  no copy of it, R1 answers `absent` through the watch block, and the launcher's no-confirmation line asks the owner to
  load a save, stay two minutes in the world and quit; otherwise the old "do not load a save" stands.
- **On the fake game** (FACT): with the word present at the title our task runs through the match's stand-in; with the
  map filled late (run 11's end) the call drops our task, no copy is found, the console gives the world step, and the map
  reads 4 nodes at the title (the word absent, between keys 4 and 7) and 9 in the world (the word present).
- **Also** (FACT): GCC 13's Release build warned twice in rtti.cpp (`-Wmaybe-uninitialized` on two optionals), from part
  3; each optional is now copied into a plain value after its check, and GCC is checked in Release from now on.

### 300. The AI pages' fifth slice: one card per job, a job's fallback and status, and a page's subscriptions against GraceUI's rate limit

`2026-11-21` · DECIDED, with FACT readings · `#engine` `#pages` `#graceui` · refs: S11.4.a, S11.2.x, G36, WHY§297

- **The cards** (DECIDED): AI › Models is one section per job (GraceUI's D202): the slot as chip; the job's state as a
  live status from its provider's health (Ready, Cooling down, Resting after failures, Failing, No key yet, No model);
  its provider, model, fallback and fallback's provider; its fixed budgets read-only from en-llm's slot table. The
  Jobs table is gone, each of its facts on a card.
- **A job's fallback** (DECIDED): chosen as its model is, only for a job with a model of its own and never the job's own
  model, written with its provider named. Its reset removes the fallback alone; the job's reset removes its whole table.
  A picker's Default (D199) is the row's reset. The changed mark follows what the table gives (a model, a fallback).
- **The floor** (FACT): GraceUI 0.2.9 to 0.2.11 take page data saying `"minor": 3` without refusal and draw no chip and
  no status, so with the Jobs table gone a job's state would show nowhere; EldenNet's floor rose to 0.2.12.
- **A rate limit** (FACT): GraceUI's server counts every message of a connection, a subscription included, against 20 a
  second and drops the rest unread, while its web shell paces commands but not subscriptions. AI › Models reads 14
  topics, so a browser opening it straight after connecting can lose one (a picker with no choices, a card with no
  status). Seen in the tests' own browser, which now paces its subscriptions; GraceUI asked to pace them too.

### 301. GraceUI's browser view paces every frame it sends, so a page with many topics loses none against the server's limit

`2026-11-21` · FACT · `#graceui` `#browser` · refs: GraceUI's D203, WHY§300, S11.2.z

- **The fault** (FACT): the server counts every frame of a connection, subscriptions included, against 20 a second
  (GraceUI's §6.8), while the web transport paced only commands, so a page with many topics could lose a subscription
  right after connecting. Found by EldenNet's preview browser on AI › Models (14 topics).
- **The fix** (FACT): pacing every frame in the page at 16 a second, in one ordered queue, keeps the server's bound and
  loses nothing; an unsubscription of a subscription still waiting takes it back (GraceUI 0.2.13).

### 302. R1 run 12: group 110 is in the manager's map at the title, and the code it leads to removes a task: we have been calling the game's unregistration

`2026-11-21` · FACT, with a LIKELY reading · `#gal` `#r1` `#task` · refs: WHY§296, WHY§299, S10.1.f

- **The map** (FACT): at the title, before the call and 131 s after it, the manager's map held 169 nodes in order, keys
  0 to 168 in the low 16 bits and 0x9000 in the high, its count word 169: every group, group 110's word among them. So
  the now path's lookup finds group 110 and calls 0x26A3260 with the node's value, our task, the word and the manager.
- **0x26A3260** (FACT, 261 bytes, read whole): it takes a lock at the value's +136, searches a list of 32-byte entries
  (begin and end at the value's +16 and +24) for one whose +8 is our task, and if it finds one destroys the object at
  the entry's +16 (its slot 0, then a free through an allocator's slot at +104) and erases the entry; it then searches
  a list of 24-byte entries (+48, +56) for one whose +0 is our task and erases it with a move; it answers 1 when it
  removed something, else 0, and unlocks. It never adds anything.
- **So** (LIKELY): the function R0c derived as register_task, 0xEB5680, removes a task from its group, and the call does
  nothing because our task was never in the lists. Its sibling, 0xEB5540, which derive already finds with the same
  shape, reaches G's other method, 0x26590A0, through the stub just before ours (0x2659D00, 16 bytes earlier); that
  method calls the request's slot 5 (0x1267A0), whose callees take the request's task. The sibling is the likely
  registration.
- **The world step** (FACT): the 4.4 s and 21.6 s searches found two words equal to our task's vtable in the game's
  private memory (none in runs 9 to 11), so the probe did not answer `absent`, the console kept "do not load a save",
  and every reading was at the title. Whether those two words are a copy of our object, or a buffer of our own not
  counted as ours, is unknown.
- **Next:** spike.13 reads the sibling's chain as deeply as ours (its arguments, its stub, G's other method, the request
  it builds and the slot it runs, down to the code that touches the group's lists), classes each chain's end as one that
  appends or one that erases, and makes the Mark's one call to the sibling only when its chain appends and its
  arguments are derived; otherwise no call. It also reads what lies around the two vtable words.

### 303. Spike.13 not built: a safety classifier stopped the work, and the route is put to the owner (O100)

`2026-11-21` · FACT · `#r1` `#method` · refs: WHY§302, O100, S10.1.f

- **What happened** (FACT): the subagent building spike.13 had read its briefs and run 12's report. While it was
  writing out the decoded forms of the game's functions from that report, a safety classifier blocked its output. It
  stopped there, changed nothing and suggested the owner look at the spike's direction. Its worktree was removed unused.
- **What follows** (DECIDED): no work of that kind goes on, and the classifier is not worked around. The rest of the
  game-task route is more of the same reading, so the route itself is put to the owner as O100, with the option of
  reading the game without a game task.

### 304. Lodestar's shape: a table of numbers per build, each id re-found at start through its witnesses, no bytes in gal/

`2026-11-21` · METHOD · `#gal` `#lodestar` · refs: O100, O48, O89, O102, proposal 0005

- **The shape** (METHOD): a table of numbers per game build, each id re-found at start through its witnesses (a few
  places in the code that use it, kept as offsets), with no search at start and no bytes in `gal/`.
- **Why**: the handover's §3 rules out byte patterns copied from the game; a start then reads a few hundred known places
  instead of scanning the code; a patch that only moves things needs no in-game round for most ids.
- **Not chosen**: keeping wildcarded patterns (bytes in the repository, O102 (b)), or matching shapes over the whole code
  at every start (seconds).

### 305. A function EldenNet might call carries a verdict of our own tool; unknown means no call

`2026-11-21` · DECIDED (Lodestar's, pending the owner's O101) · `#gal` `#lodestar` `#task` · refs: WHY§302, WHY§303,
O101, proposal 0005 §7.2

- **The rule** (DECIDED): a function EldenNet might call carries a verdict of our own tool: adds, removes, reads or
  unknown; unknown means no call. The derived register_task enters the catalogue as `task_remove` with its verdict
  recorded, and its sibling as `task_add`, a lead with none yet.
- **The tool's output** (DECIDED): it writes out no instruction in any form; a reviewer reads our classifier and its
  tests on the fake game.

### 306. Lodestar L1: the registration's label is task_add, and a build's table lives in its layout file

`2026-11-21` · DECIDED (Lodestar's) · `#gal` `#lodestar` · refs: proposal 0005 §4, §5, O100, WHY§188, S10.1.k

- **The label** (DECIDED): the catalogue's label for the registration is `task_add`; `register_task` stays as its `was`,
  because derive.json's schema 1 vocabulary is pinned to DataPiler 0.2.41 (WHY§188) and `patterns.toml` still uses it;
  both go when schema 2 and the recipes land.
- **The table's home** (DECIDED): a build's table lives in its layout file (schema 2), not in a file of its own, so that
  one loader, one gate and one lint cover a build; the catalogue cross-check is the caller's (`LayoutNeeds::ids`), so
  `load_layout`'s signature and R7 stand.
- **Built** (FACT): `gal/ids.toml` with 20 ids of our own and no fact about any build; en-gal's `parse_ids` and layout
  schema 2 (items with placement, witnesses, digest, verdict, checks and status, and a provenance block; no key can carry
  bytes); `lint --gal` and the galtoml fuzz target read the catalogue. Every build passes with no warning (engine
  feat/engine fb999ca).

### 307. A witness says the bytes at a place refer to the id, not that an instruction starts there

`2026-11-21` · FACT, with a DECIDED · `#gal` `#lodestar` · refs: proposal 0005 §7.1, WHY§304, S10.1.k

- **The finding** (FACT): a witness one byte into a prefixed load still decodes, with our decoder, to a load of the same
  static (the prefix is skipped and the displacement's end does not move), so the check accepts it.
- **Decided** (DECIDED): a witness says that the bytes at a place refer to the id, not that an instruction starts there.
  On a build the gate has matched, a wrong table or a patched place is what the check must catch, and it does; proving
  an instruction boundary would need a decode from the function's start, which a start does not otherwise make. Tested,
  and said in en-gal's README.
- **Built** (FACT): en-gal's resolver (`resolve.hpp`, engine feat/engine c2401d0): an item of a build's table is re-found
  in the image as loaded with no search, by its placement (a static in written data with room for a pointer, a function
  at a start the function table lists, a table within one section that is not run, a class's method tables in read-only
  data) and its witnesses (one read of at most 15 bytes at each recorded place, decoded by our own decoder, which must
  refer to the id); only a checked item's place is handed out. Nothing in the DLL reads through it yet.

### 308. The accessor's shape is stricter than the pattern it replaces, and loses none of its accepted sites

`2026-11-21` · METHOD, with a LIKELY · `#gal` `#lodestar` · refs: O102, proposal 0005 §6.1, WHY§181, WHY§197, S10.1.k

- **Stricter** (METHOD): the load must be 64-bit, the test must be of the register just loaded, and the branch must go
  forward; the pattern wildcarded those bytes and left the rest to the finder's decoding. So what the pattern hit and
  then refused as no idiom, the shape never hits, and on the tests' three invented programs the accepted sites are the
  same, with every place, verdict and name.
- **On the real game** (LIKELY): the shape's hits are fewer than R0c's 7,151 and its accepted sites equal R1's 7,081;
  L3's offline run reports both counts, and a difference in accepted sites is a finding to read before the pattern goes.
- **Every offset** (METHOD): the shape is looked for at every offset of a section, not only where a straight decode
  lands, since a pattern scan finds a site wherever it lies and the shape must lose none; a three-byte test on the first
  form's encoding keeps it as cheap.
- **Built** (FACT): en-gal's `shape.hpp`, a run of at most 16 instruction forms in our own grammar over our decoder's
  fields, with no byte of any program; `fd4::find` runs on the shape beside the pattern (engine feat/engine 33cf307). The
  pattern stays in use until the derivation, discovery and R1 read the shape from `gal/recipes.toml`.

### 309. GraceUI assumes nothing per frame of a mod's data, so O101's reader fits ABI 1.2 as it is

`2026-11-21` · FACT · `#graceui` `#lodestar` · refs: GraceUI's graceui.h (Panels, World-state providers), GraceUI's D24, D71,
D76, D78, D143, D151; O101; proposal 0005 L4

- **No per-frame assumption** (FACT): a panel's newest draw list is drawn every frame until replaced, with no age check,
  hiding or smoothing (D76, D137); world-state claims hold until replaced or disconnected, with no heartbeat or expiry
  (D78, D143); the per-frame budgets measure cost, not freshness, and D151's watchdog counts GraceUI's own frames. So
  O101's reader, at about ten readings a second, fits ABI 1.2 as it is.
- **Two consequences, EldenNet's** (FACT): floating labels step at the reader's rate (smoothing every frame would need
  D71's `on_present`, designed but not in ABI 1.2, an append under a GraceUI proposal); and the danger close and the
  open gate follow the world by up to one reading (D24's 0 ms is the close's motion, not a reaction time).

### 310. O102 carried out: gal/patterns.toml's idiom withdrawn, gal/recipes.toml's accessor in use

`2026-11-21` · DECIDED (O102 carried out) · `#gal` `#lodestar` · refs: O102, O89, O101, WHY§308, S10.1.k

- **Withdrawn** (DECIDED): `gal/patterns.toml`'s idiom and its note are removed, and `gal/recipes.toml`'s accessor, a
  shape of five forms with no bytes, replaces it (note `gal/derivations/accessor.md`); `gal/` holds no byte of the game.
  The recipe is a lead until it is counted on the game itself (L3).
- **Where it is read** (DECIDED): with a recipe, discovery finds the FD4 singletons by its shape and reads no idiom even
  where one is given; eldennet.dll's boot, galtest, `lint --gal`, the galtoml fuzz target and the fake game's harness
  read it. The pattern reader stays for the fake game's generated folder and for the statics' and the function's
  patterns, until discovery goes through the table (L3, L4).
- **R1** (FACT): R1's byte-for-byte idiom check now refuses its call on the repository's `gal/`, which O101 wants; the R1
  package and its README are legacy until L5's read-only package replaces them.
- **History** (FACT): the 24-byte pattern (7 fixed bytes) stays in feat/engine's history (55487c0), where O89 allowed it
  at the time.
- **Checked** (FACT): every build with no warning; the fake game's harness 105 of 105 under Wine; engine feat/engine
  6da3291.

### 311. Digests move from L2 to L7, since under O101 (b) nothing in v1 would read one

`2026-11-21` · DECIDED (Lodestar's) · `#gal` `#lodestar` · refs: proposal 0005 §7.1, O101, S10.1.k

- A digest says a function's code is what the review saw. Under O101 (b) v1 calls no function of the game, so nothing in
  v1 would read one; digests move to L7, the task service's research, and L3 comes next.

### 312. Lodestar L3: offline, the finder reports the managers' slots unnamed, and counts the shape against the pattern

`2026-11-21` · METHOD · `#gal` `#lodestar` `#devkit` · refs: proposal 0005 §6.2, §11, O102, O92, WHY§188, WHY§207, S10.1.k

- **Unnamed offline** (METHOD): the finder cannot name the managers' slots offline, since the game writes their class
  objects' names at run time (WHY§207), so it reports every slot the accessor's accepted sites load, unnamed, with its
  witnesses; the in-game read-only round (L5) names them by their instances' classes (O92), and the table is written
  then.
- **Evidence before the pattern goes** (METHOD): the finder cuts the withdrawn pattern again in memory, never writing it,
  and reports only counts: `both`, `only_shape`, `only_pattern`. A run is complete only when no site is accepted by one
  alone.
- **A new kind** (METHOD): the report is kind `lodestar`, schema 2, rather than new keys in derive's schema 1, which
  DataPiler 0.2.41's guard pins; it holds whole numbers, booleans, our own words and labels, and no fraction or key of
  the pattern type (a test pins that).
- **Built** (FACT): en-gal's `lodestar::find`; `eldennet-engine lodestar find <exe> --gal <folder> --out <file>` with the
  disclosure; `lint --report` knows the kind; `tools/spikepkg/build.py find` makes EldenNet-Find.zip (the engine, `gal/`,
  `find.cmd` and README-FIND.txt; no launcher, no DLL). On the fake game the shape and the pattern accept the same 20
  sites. Engine feat/engine ffc172b.

### 313. A package's licence notices and the sweep: toml++'s address, kept as its licence asks, allowed in those files only

`2026-11-21` · FACT, with a DECIDED (the owner's) · `#process` `#privacy` `#spike` · refs: O75, BUILD.md "The spike package",
S10.1.k

- **The refusal** (FACT): the sweep reads archives member by member, and refused Lodestar's finding package because its
  THIRD-PARTY-NOTICES.txt holds toml++'s MIT licence, whose copyright line carries its author's address (rule email).
  The licence obliges us to keep the notice unchanged, so the address is neither removed nor the licence left out. Any
  package with `notices()` or `spikes_notices()` meets this.
- **Why earlier packages passed** (FACT): Skyrizz's working branch `feat/skyrizz-lite` already allows rule email only in a
  file whose place ends in THIRD-PARTY-NOTICES.txt (since 2026-11-15, Skyrizz c68a948); a zip member's place is
  `<zip>!<path>`, so it covers notices inside a package. Skyrizz's default branch `main` lacks that line, and Lodestar's
  sweep was installed from `main`. The handover now says to install from `feat/skyrizz-lite`.
- **The owner's decision** (DECIDED): in Lodestar's chat, "Allow that one address (Recommended)": one narrower line,
  rule email, that address only, only in THIRD-PARTY-NOTICES.txt and tomlplusplus/LICENSE, with its reason, committed in
  Skyrizz on Lodestar's branch (cece89f), for the owner or Skyrizz's session to bring into `feat/skyrizz-lite`.
- **Sent** (FACT): EldenNet-Find.zip, label 0.1.0-lodestar.1, built from engine 9edf1c0, checked as BUILD.md says and
  swept, sent to the owner by Lodestar on 2026-11-21.
- **Later (2026-11-21):** with the sweep installed from `feat/skyrizz-lite`, the package sweeps clean as it is, under
  the existing line. The narrower line on Lodestar's Skyrizz branch (cece89f) is redundant and is not to be merged;
  Lodestar told the owner so. Lodestar's earlier install was the stricter one, so nothing passed that should not have.

### 314. The accessors' class objects are no objects with a table of methods; O92's way is the only one

`2026-11-22` · FACT · `#gal` `#lodestar` `#derive` · refs: WHY§206, WHY§207, O92, docs/ops/probes/2026-11-22-l3.md, S10.1.k

- **The finding** (FACT): of the 217 class objects the shape's accepted sites pass to the getter on 2.7.2.0, 110 lie at
  odd offsets, 46 at offsets aligned to 8, and 54 are one byte from the next. They are not objects with a table of
  methods, so no name layout can be read from them, in the file or in memory.
- **What follows** (FACT): builds.toml's `dlrf_name_layout` can never become true for this game, and O92's way (the
  instance's own class) is the only one. This is the cause behind R1's runs 1 and 2. The name-layout branch of discovery
  and of the layout schema (`[dlrf]`) is dead for this game and can go when discovery moves to the table (L4).

### 315. On 2.7.2.0 the accessor's shape stands at 7,077 places, all also the pattern's; the pattern accepts 4 more

`2026-11-22` · FACT, with a LIKELY · `#gal` `#lodestar` · refs: WHY§308, WHY§312, WHY§208, O102,
docs/ops/probes/2026-11-22-l3.md, S10.1.k

- **The owner's run** (FACT): on 2.7.2.0 with the game closed (package 0.1.0-lodestar.1), the finder reproduces R0c: the
  withdrawn pattern, cut again in memory, has 7,151 hits and 7,081 accepted; 11 sections, 235,897 functions; the
  character set of 6 classes and the player's of 2; the groups' table of 168 entries of 32 bytes, the lead's group at
  index 110; a known build. The shape stands at 7,077 places, every one accepted, none refused, all among the pattern's
  accepted sites; the pattern accepts 4 sites the shape does not, and reaches 219 statics to the shape's 217.
- **Likely** (LIKELY): those 4 sites load the 2 statics whose instance R1's run 3 could not read at all, so no pointers to
  objects, and the shape is right to leave them out. The second run's report says which step of the shape they fail.
- **Built for it** (FACT): the finder now says, for each site the pattern alone accepts, the first step of the shape that
  differs and how, and the statics only those sites load (engine feat/engine ddd7e63). A second package, 0.1.0-lodestar.2,
  went to the owner for the same five-minute run.
- **Later (2026-11-22), the second run:** the likely mechanism above was wrong. The 4 sites are not narrower loads: at
  each, the test is a 64-bit test of another register than the one loaded, so the pattern took another load for the
  manager's slot, and it gave two managers two statics each (219 to 217). The shape's same-register rule tells them
  apart and is kept; the recipe's expected count is 7,077 for this build (WHY§316).

### 316. One class object for each static, and the reverse, is an invariant of the accessors worth checking by itself

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: WHY§315, proposal 0005 §7.1, docs/ops/probes/2026-11-22-l3.md, S10.1.k

- **The check** (METHOD): the finder counts the class objects that go with more than one static, and the statics that go
  with more than one class object (`shared_class_objects`, `shared_statics`); a run with either above 0 is to read
  first. It would have shown the pattern's two false statics without any comparison, and it is what a later build's run
  is judged by once the pattern's comparison is retired with the derivation's cutting.
- **The verdict** (METHOD): a run is complete when the shape's count is within the recipe's range (7,077 on 2.7.2.0),
  nothing is accepted by the shape alone, and both counts are 0; the sites the pattern alone accepts are reported with
  their reason and no longer count against it. Not chosen: keeping "no site that only one of the two accepts" as the
  verdict, which on 2.7.2.0 would call every run incomplete for a difference now understood.
- **Built** (FACT): `gal/recipes.toml`'s accessor has `expect_matches = [7077, 7077]` with its note (engine feat/engine
  4624553). L3 is done: the tool, both of the owner's runs and their record.

### 317. v1's start makes no call into the game; the reader is a thread of EldenNet's own

`2026-11-22` · DECIDED · `#gal` `#lodestar` `#boot` · refs: O101, proposal 0005 §9.4, §10, S10.1.k

- **The start** (DECIDED): T1 goes Discover, WaitEngine, Reader, FirstReading, Ready, with no `install_task` and no getter;
  only the probe's R1 round still makes those calls, until L5 replaces it. The reader (T2) is a thread of EldenNet's own,
  made suspended so that the stack table can be made again with its stack in it and without T1's, then let go; it steps
  the task closure about 30 times a second and builds a snapshot about 10 times a second.
- **The watch block** (DECIDED): its states `reader` and `first_reading`, and the thread kind `reader`, were appended,
  never renumbered; `register` and `first_tick` stay for the game's task, which only the R1 round uses. In a crash
  record "game" now means the thread of the game's window. en-proto's ReadError gained `Unsteady`. Engine feat/engine
  3f65014.

### 318. The table takes over id by id: an id it holds is never searched for

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: proposal 0005 §7.1, §9.1, S10.1.k

- Discovery asks the build's table first; an id the table holds is never searched for, even when its check fails, and
  an id it does not hold is found the old way. So a wrong entry cannot be rescued by a search that happens to agree with
  it, and a partial table is still usable. Tested with a table that puts one static on another's slot while the pattern
  for the true slot sits in the folder: refused as `witness_elsewhere`, the pattern not asked. An id whose check fails is
  refused alone; a required one stops the start.

### 319. Steady reading out of step with the frame: torn, held and unconfirmed, each counted

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: proposal 0005 §10, O101, WHY§309, S10.1.k

- Three rules, each counted: **torn** (a structure word changed between the reading and its re-reading: the reading is
  dropped), **held** (a change of lifecycle waits for a second sighting, so a one-reading blip is never published),
  **unconfirmed** (a group's fault that did not come twice running does not count toward its breaker). A dropped reading
  counts against no group, and the next snapshot carries `gap`.

### 320. On the fake game the own-thread reader reads the same truth as the registered task, the hub untouched

`2026-11-22` · FACT · `#gal` `#lodestar` `#fakegame` · refs: S10.1.k

- Under Wine, in every case tried (the world, late statics, moves with the old places released, loading screens, online,
  faults in an optional and in a structural group), the reader on EldenNet's own thread read what the registered task
  read, with no registration, no task of ours live and none of our slots called. About 142 snapshots in 14.5 s (the
  game's task gave 58), no allocation on the reader, a step's p99 under 0.2 ms; lifecycle changes held for a second
  sighting 2 in a plain run and 7 across loading screens; no torn reading. The fake game's harness 112 of 112, seven
  cases new, and the launcher cases check that the DLL never took `register` or `first_tick`.

### 321. With no getter and a dead name layout, the real game's managers can be named only by the table

`2026-11-22` · FACT · `#gal` `#lodestar` · refs: WHY§314, O101, S10.1.k

- With no getter (O101) and the name layout dead for this game (WHY§314), the real game's managers can be named only by
  the build's table. Until L5 writes it, a v1 start on the real build ends at the discovery banner, as it must.

### 322. The in-game round names a manager by its instance's class and writes only the catalogue's number and label

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: O92, proposal 0005 §6.1, S10.1.k

- For each static the accessor's sites load, the round reads the instance it holds as the game runs, that instance's
  first word through one guarded read, and the word's class by the image's own type information; a lead names the one
  static whose class has its name (or its name with Imp). The class's name is compared in memory and never written, so
  the report and the draft table hold nothing of the game; a lead's label is the catalogue's label or its `was`. Later
  passes fold in managers made only in the world. Engine feat/engine 7995413.

### 323. The draft table is checked before it is written, by the same checks a start makes

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: proposal 0005 §7.1, WHY§318, S10.1.k

- An entry whose placement fails is left out, a witness that refers elsewhere is dropped, and `checks` lists only what
  passed. Every entry is a lead; `tool_sha256` is zeros in game and is set at review from the package's own digest of the
  DLL.

### 324. A class that holds a GuardedReader must be built where en-gal is built with and without the probe option

`2026-11-22` · FACT · `#build` `#gal` · refs: S10.1.k

- The reader has more members with the probe option on, and the tests are built with it on: a trial defined in the
  offline part of en-gal read its own fields at the wrong places (its period as a slot's address). Caught by its first
  test; the trial now lives in the part built both ways.

### 325. On the fake game the read-only round names all eleven managers and reads them with no fault

`2026-11-22` · FACT · `#fakegame` `#lodestar` · refs: S10.1.k

- Under Wine the round names none of the eleven managers at a pass made before the game has them and all eleven ten
  seconds later, reads them about 340 times in 34 s with no fault (a reading's p99 under 0.05 ms), and the hub sees no
  task of ours. The launcher's `--read` (ProbeRequest `Read`, appended), the read package (0.1.0-lodestar.3) and three
  harness cases (by settings, by the owner's flag, and refused by a release build) pass; the fake game's harness 115 of
  115.

### 326. On 2.7.2.0 in game, eight of the eleven managers are named by their instances' classes

`2026-11-22` · FACT · `#gal` `#lodestar` `#owner-run` · refs: O92, proposal 0005 §6.1, docs/ops/probes/2026-11-22-l5.md, S10.1.k

- The owner's read-only round (package 0.1.0-lodestar.3, about 19 minutes, no call into the game) named eight of the
  eleven managers by their instances' classes, none twice, each by the lead's name with Imp: task_hub, session, menus,
  loading, camera and param_store at the first pass, at the title; characters and hud at pass 56, likely the first pass
  after the save loaded. Their statics are among the accessor's 217. event_flags, world_clock and messages were not
  named. The first table for 2.7.2.0 (`gal/layouts/2.7.2.0-ww.toml`, schema 2) holds the round's 11 entries (the eight
  managers, the two character classes, the groups' table) as leads, with the package's digest of its DLL; engine
  feat/engine d8b3777.

### 327. The reader on EldenNet's own thread read the real game for 19 minutes with no fault: the first evidence for O101 (b)

`2026-11-22` · FACT · `#gal` `#lodestar` `#owner-run` · refs: O101, proposal 0005 §10, docs/ops/probes/2026-11-22-l5.md

- 11,552 readings in 1,155 s (10.0 a second), 242,244 guarded copies, no fault of any kind, no slot seen to change
  between its two reads, no object of another class behind a named slot. A reading of eight managers took 3
  microseconds at the median, 6 at the 99th percentile and 319 at the longest. Every manager but task_hub was empty for
  some seconds in all (11 to 17 s), counted but not timed.

### 328. The image as loaded gives the finder the same statics, class objects, sites and witnesses as the file

`2026-11-22` · FACT · `#gal` `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, docs/ops/probes/2026-11-22-l3.md

- In game the finder found what it found in the file, value for value: 7,077 places, all accepted, 217 statics, 217
  class objects, one getter; the classes' sets and the groups' table the same. Witnesses found offline can be relied on
  in game.

### 329. A third of the game's managers hold objects with no type information the walk reads

`2026-11-22` · LIKELY · `#gal` `#lodestar` · refs: O92, docs/ops/probes/2026-11-22-l5.md

- At the round's last pass, of the 217 statics, 139 held an instance of a class the image's type information gives, 71
  an object with no such class, 6 were empty and 1 could not be read. O92's way cannot name a manager among the 71; one
  that EldenNet needs among them needs the invariant recipe (L6). Whether the three unnamed managers are among them, go
  by other names than our leads, or were not yet made, is not known yet.

### 330. A round's report must tell "not there" from "there under another name"

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§329

- For each lead, the report should count what the image holds under that name (the classes of its name and of its name
  with Imp). This round did not, and so left its one open question open. Two shortcomings for the next round: naming
  stopped after 60 passes (ten minutes), so the world had only about five; and the trial did not count its readings by
  stage.

### 331. Ten of the eleven managers' leads have exactly one class of their name in the image; world_clock's has none

`2026-11-22` · FACT · `#gal` `#lodestar` `#owner-run` · refs: O92, WHY§329, docs/ops/probes/2026-11-22-l5.md, S10.1.k

- The owner's offline count (package 0.1.0-lodestar.4, the game closed): on 2.7.2.0 ten of the eleven managers' leads
  have exactly one class of their name in the image (nine by the name with Imp, event_flags by the plain name), each
  with one table of methods; the eight the in-game round named agree. world_clock's lead has none, and its name stands
  nowhere in the image as a type's name: O92's way cannot name it, so it needs the invariant recipe (L6), or its lead's
  name is not the game's.

### 332. A manager with a class of its name that the round did not name was not missed for want of type information

`2026-11-22` · LIKELY · `#gal` `#lodestar` · refs: O92, WHY§329, WHY§331, docs/ops/probes/2026-11-22-l5.md

- messages and event_flags have classes of their names, yet the round found no instance of either. Likely causes, in
  the order Lodestar will try them: the instance lies inside the image, where the round refused to read it as an instance
  (it counted one static unread), a rule R1's derivation had too; the manager was among the six statics still empty
  when naming stopped; or its static is none of the accessor's 217. The next in-game round, read-only as the last, reads
  an instance inside the image through the image's own view, searches the data slots for an object of a lead's class
  (R1's slot search, which only reads), and starts L6's invariant searches on the reader's thread with the play time.

### 333. A search that follows pointers it has not validated asks before it reads

`2026-11-22` · DECIDED · `#gal` `#lodestar` `#safety` · refs: WHY§111, WHY§160, O91, S10.1.k

- A read of a guard page lifts the guard, by an exception or by ReadProcessMemory alike, and a search that follows
  whatever a data slot holds could land on one. So a search asks first: every page of the range committed and readable,
  none a guard page or no-access, by VirtualQuery, which reads nothing; only then is the range copied, by
  ReadProcessMemory on our own process. It raises no exception in the game and lifts no guard, whatever a pointer holds;
  what it leaves alone is counted (`search_refused`). The v1 reader follows only objects it has checked and keeps its
  own copy primitive; this rule is for searches. Engine feat/engine merge of 2ebeb19.

### 334. The first invariant recipe of our own, the clock: minutes and hours by how they step

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: proposal 0005 §6.1, WHY§331, S10.1.k

- A field that only ever held 0 to 59 and only ever stood still or stepped forward by 1 to 5, round 60, behaves as
  minutes; one beside it that only held 0 to 23 and stepped by one, as hours. It concludes nothing: its hits are counts
  for the review. A counter that jumps or counts down is no hit. The play time is looked for by the rising search.

### 335. A manager no accessor's static holds is looked for among all the data's slots, and stays a lead

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: O92, WHY§332, S10.1.k

- A manager whose class the image holds but which no accessor's static holds is looked for among all the data's slots
  (the slot search, en-gal's own). One slot names it; the table then gives it by its instance's class alone, with no
  agreement claimed and no witness, so it stays a lead until a second recipe agrees. An instance inside the image is now
  read through the image's view, which the first round refused.

### 336. Two sessions of the game give the eight named managers the same statics, tables and witnesses

`2026-11-22` · FACT · `#gal` `#lodestar` `#owner-run` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§326, S10.1.k

- The owner's second read-only round (package 0.1.0-lodestar.5) gave the same eight managers at the same statics, with
  the same tables of methods and witnesses as the first session, value for value, and the same draft table entry for
  entry: the build's table is the same from one session to the next. The static the first round would not read holds an
  object inside the image whose class is none of the leads'. event_flags and messages are still not named, by the passes
  or by the slot search.

### 337. A manager's slot is empty for seconds in the ordinary course of the game; v1 must treat it so

`2026-11-22` · FACT · `#gal` `#lodestar` `#owner-run` · refs: O101, WHY§327, docs/ops/probes/2026-11-22-l5.md

- The reader again met no fault: 4,402 readings in 8.9 minutes (6.6 in the world), 102,864 guarded copies, none refused
  or faulted, no torn slot, no change of class, 3 microseconds at the median. Six of the eight managers were empty in one
  stretch only, 0.1 to 3.2 s, never in the world; the HUD's in five stretches, 2.7 s of them in the world; task_hub
  never. A slot going empty for seconds while the game starts or a save loads, and the HUD's in the world, is the game's
  ordinary course: v1 treats an empty slot as such, not as a fault.

### 338. No clock in whole minutes and hours among the unnamed managers' objects; one candidate for the play time

`2026-11-22` · FACT · `#gal` `#lodestar` `#owner-run` · refs: proposal 0005 §6.1, WHY§334, docs/ops/probes/2026-11-22-l5.md

- The clock search found no clock among the 209 unnamed statics' objects (one slow counter, no hours beside it): the time
  of day is not kept as whole minutes and hours in the first 256 bytes of such an object. The play time has one strong
  candidate: a 32-bit field that rose by the clock's milliseconds in 107 of 119 passes, behind a slot that is none of the
  accessor's statics; every other hit rose in 1 to 3 passes.
- **Leads next** (LIKELY, to be counted offline before anything rests on them): our research notes on how another tool
  finds the messages' global say its accessors put an instruction between the load and the test, which our shape refuses,
  so its static would be none of the 217; the same notes name a second manager behind the event flags. The next offline
  run counts a second accessor shape (one instruction allowed between the load and the test) and alternative names for
  the three leads.

### 339. Searches on the reader's thread must be budgeted by time, not by count

`2026-11-22` · METHOD · `#gal` `#lodestar` · refs: WHY§333, docs/ops/probes/2026-11-22-l5.md

- Asking first makes a search's read cost two system calls, and a thousand a step took the reader's thread from 29 steps
  a second to 14, so this run's trial rate (8.2 readings a second) is the searches' doing; the game's own threads are not
  touched. Searches get a time budget per step, and a trial that measures the reader does not share its thread's step
  with a search, or says when it did. The rising search is to rank its hits, not keep the first 32.

### 340. The second round's logs: the slot search found the messages manager, and the report lost it

`2026-11-22` · FACT · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§336, S10.1.k

- The second read-only round's slot search found the messages manager behind 2 slots, seven times over; the report said 0
  because `merge()` dropped a search's finding at the next naming pass. The event flags' search found no slot. Mended in
  engine 66c8675 (merged as 8815db2), with a test: a search's finding and numbers are kept, several slots holding one
  object name the manager, two objects leave it ambiguous, and a search that found something is not rerun.

### 341. 97 in 100 of the data's values that could be a heap address are refused when Windows is asked

`2026-11-22` · LIKELY · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§333

- A slot search had about 121,000 candidates and found about 118,000 unreadable; over the round the searches' failed
  reads and the ask-first copy's refusals were the same number, 1,662,371: no read faulted, each was refused beforehand.
  Those values are LIKELY no addresses (two small numbers read as one). The third round asks Windows once for every
  candidate before searching, and counts the refusals by reason.
- **Later (2026-11-22), the third round:** confirmed. Of 123,336 values that could be a heap address, 119,045 had no
  memory behind them, none had memory that may not be read, 4,291 were live (WHY§345).

### 342. 56 of the accessor's 217 statics hold a manager only while a save is loaded

`2026-11-22` · FACT · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md

- 62 of the 217 statics are empty at the title, 6 with a save loaded, and 62 again back at the title: 56 hold a manager
  only while a save is loaded.

### 343. The accessor's other shapes are recipes of the same kind under ids of their own

`2026-11-22` · METHOD · `#lodestar` `#gal` · refs: O102, O48, docs/ops/probes/2026-11-16-r0c.md, WHY§338, S10.1.k

- Five variant recipes of the accessor's kind (`accessor_near`, `accessor_gap`, `accessor_gap_near`, `accessor_compare`,
  `accessor_compare_near`), each with its note in `gal/derivations/`, from R0c's own counts of the path's references
  (26 places with five other heads; 1,379 references after a 64-bit load of a static; 676 after a compare of a quadword
  with zero) and, for the gap, our research notes as a lead; two new forms in the grammar, `skip_one` and `compare_slot`.
  No byte of the game and nothing from another project. Discovery still reads `accessor` alone; the finder counts each
  variant beside it and never changes its counts; a variant gets its expected count only from a count on the owner's
  executable, and is withdrawn if it stands nowhere.
- **Also in the third round** (DECIDED, Lodestar's): candidates asked for first (VirtualQuery, which reads nothing), the
  searches sharing 4 ms of each reader step, the rising search ranking its hits, and a member search (the first 0x800
  bytes of each static's object, for an unnamed lead's object or a pointer to one), all read-only. The report is schema 3,
  new keys only, still a closed vocabulary.

### 344. Third read-only round: messages named by the search of the data's slots; twelve leads in the build's table

`2026-11-22` · FACT · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§340, S10.1.k

- The owner's third read-only round (package 0.1.0-lodestar.6) named messages by the slot search: 2 slots of the data
  hold one object of its class, named by the first; neither is a static of any accessor shape. The trial read it 3,557
  times with its class, 52 empty, no fault. The eight others, the 217 statics, the two classes and the groups' table
  were unchanged, a third session. The build's table (`gal/layouts/2.7.2.0-ww.toml`) now holds 12 leads, messages added
  with no witness (engine feat/engine f95f3cb).
- The reader: 3,960 readings in 403 s (9.8 a second), 21 steps a second, a p99 of 5 microseconds and the longest 17,
  103,547 copies with none refused; the searches used their 4 ms in 7,808 of 7,844 steps.

### 345. The second round's refusals were values that are no addresses

`2026-11-22` · FACT · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§341

- Of 123,336 values of the data that could be a heap address, 119,045 had no memory behind them (asked of Windows,
  nothing read), none had memory that may not be read, and 4,291 were live. Of 590,869 reads asked, 14,850 were refused
  as gone since, none failed once allowed, and nothing faulted.

### 346. The accessor's other shapes confirm the 217 statics rather than add to them

`2026-11-22` · FACT · `#lodestar` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§343, gal/recipes.toml

- On 2.7.2.0: gap 675 places, compare 598, near 2, compare_near 4, all accepted, reaching 89 of the 217 statics with
  the same class objects and 2 new ones (1,279 places beside the 7,077). gap_near stands nowhere and is withdrawn with its
  note; the four standing variants have their expected counts.

### 347. event_flags is behind none of the places looked; world_clock has a class only under a longer name

`2026-11-22` · FACT · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§331, WHY§338

- event_flags is not behind any static, any slot of the data, in the image's data, or within 0x800 bytes or one pointer
  of a static's object, in six tries over 6 minutes of play (3,938 to 4,174 live candidates; the member search's 1,270
  object copies and 72,001 pointers followed). world_clock has no class of its name or with Imp, but one class whose name
  holds the lead's (`related` 1). No clock in minutes and hours in the first 256 bytes of 211 unnamed statics' objects.

### 348. The play time: one field behind one slot rose by the clock in two sessions, a lead for L6

`2026-11-22` · LIKELY · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§338

- One 32-bit field behind one slot of the data rose by the clock in 107 of 119 passes in the second round and in 85 of
  90 a session later, same slot and offset; the next best rose in 14. A lead for L6.
- *Later, 2026-11-22:* found, in four sessions (WHY§369).

### 349. Pad chords for mods' hotkeys are built: an anchor, a key held 0.4 s, one hotkey event, or a tap replayed

`2026-11-22` · FACT · `#graceui` `#input` · refs: GraceUI's D204, GraceUI's proposal 0005, G35, O95, S11.4.c

- Built in GraceUI 0.2.14 (ABI 1.3). grace-input's PadChords runs on XInput's reads after the pad ledger: the anchor
  passes; a key 150 ms or more after it is held back; 0.4 s more fires one hotkey event and hides both until each is
  released, the packet number changed; a release sooner replays the key as one tap of two reads and 50 ms, a key still
  held after it hidden until released.
- It stands aside for a holder (a resolving chord dropped, no tap) and while request_input answers not_now or only
  notices show (none starts; one resolving fails with its tap). One reader per pad under a try-lock; a second never
  waits. A mod connected with ABI minor 2 or less still gets not_yet, since a mod built before 1.3 retried on not_yet
  and could take refused as final.

### 350. Pad behaviour is tested with no pad; what only a real pad shows waits for testers, since the owner has keyboard and mouse only

`2026-11-22` · METHOD · `#graceui` `#testing` · refs: GraceUI's D205, G35, S11.1.a, S15.3.b, S16.2.a

- The owner, asked whether GraceUI should build the chords (2026-11-22): "alright, but keep in mind I don't have a pad,
  controller, VR set, or handheld to test with, only a PC with keyboard and mouse."
- So every pad behaviour is checked without a pad: unit timelines in grace-input, and the harness's chords scenario
  under Wine on both toolchains, through a test export, GraceUI_Test_PadRead (the game's view of one read, its packet
  number included). The real-pad facts (the chord's three timings, that the chord is free in the game, the Steam Deck)
  wait for testers' machines, never for the owner; the owner's own runs are keyboard and mouse.

### 351. The read-only round may search all of the game's readable private memory once for a missing manager's object, counts and places only

`2026-11-22` · DECIDED · `#lodestar` `#probe` · refs: O105, S10.1.k, WHY§347, docs/ops/probes/2026-11-22-l5.md

- The owner's answer to Lodestar, 2026-11-22: "Yes, scan it (Recommended)". Built as ObjectScan (engine 606167d), on the
  reader's thread once a save is loaded: private, writable, committed memory outside the game's image, EldenNet's own
  heap and every thread's stack, 16 KiB at a time, Windows asked for each region and again after every mebibyte, in a
  6 ms share of each step of its own; then up to three sweeps for the holders of what it found. Nothing read is kept:
  counts, classes as offsets in the image, offsets into objects, and whether the image's data holds a node's address.
  Package 0.1.0-lodestar.7.

### 352. Where the image has no class of a lead's name, a class whose name holds the lead's may name the manager, marked by_related

`2026-11-22` · METHOD · `#lodestar` · refs: O92, WHY§347, docs/ops/probes/2026-11-22-l5.md

- Only where the image has no class of the lead's own name, with or without Imp. Weaker than O92's rule, so such a
  manager stays a lead until confirmed (the world's clock first, `related` 1 in the third round).

### 353. A manager no accessor loads takes its witnesses from the code's references to its slot, and of several slots the one referred to most

`2026-11-22` · METHOD · `#lodestar` `#gal` · refs: resolve.hpp, WHY§340, WHY§344

- The witnesses are instructions with a RIP-relative operand that is the slot, each read from its function's start, as
  the resolver reads any witness. Where several slots hold one object, the manager's is the slot the code refers to
  most, with those places as its sites (`lodestar::references`). `messages`, named by the slot search, should get its
  witnesses this way.

### 354. The event flags' store is behind one of the accessor's statics from the title on; no object of the flags' manager's class in 7.7 GiB

`2026-11-22` · FACT · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, O105, WHY§347

- The fourth read-only round (package 0.1.0-lodestar.7, the owner's run). The scan found one object of the store's class
  (the lead on trial, `flag_store`), and the image's data holds its address in one slot, one of the accessor's 217
  statics (183 sites). One static held an object of that class at each of 67 passes, from the title on.
- No object of the flags' manager's class (`event_flags`, 1008) in what the scan read: its first sweep went through
  7,698 MiB of private writable memory and found no word that is that class's table of methods; ten slot searches found
  none.

### 355. The world's clock is one object inside the object behind one of the accessor's statics: a field of that manager, no static of its own

`2026-11-22` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§347, WHY§352

- One object of the class under the longer name, 1,024 bytes into the object behind one of the accessor's statics (139
  sites), with a pointer to it 192 bytes in. The scan found the same object, held by one word 256 bytes after the start
  of an object of another class, so the static points 64 bytes into a larger object. No slot of the data points at it.
- *Later, 2026-11-22 (the fifth round, WHY§368):* that object does not hold the time. Through the pointer 192 bytes into
  its manager's object it was of the right class at 1,597 of 1,597 readings, but nothing in its first 512 bytes changed
  in play: no run of bits behaved as minutes, and no fraction rose. The "1,024 bytes in" was a neighbour in memory.
  Where the time is kept is still open.
- *Later, 2026-11-22:* found, elsewhere (WHY§394). The class under the clock's longer name does not hold the time.

### 356. messages' slot is the one of its two that the code refers to 238 times; the build's table has it with eight witnesses

`2026-11-22` · FACT · `#lodestar` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§353, gal/layouts/2.7.2.0-ww.toml

- Of the two slots, 238 instructions refer to one and 3 to the other; named by the first, with eight witnesses the round
  checked. The scan found the same object and slots. The table's other eleven entries are unchanged, value for value
  (engine 2522e1f).
- The eight others as before, a fourth session; 6,059 readings with no fault. The play time's candidate rose in 32 of
  35 passes, same slot and offset, a third session (WHY§348).

### 357. The object scan read 19,750 MiB in 781 s without finishing and slowed the reader's thread; it stays off

`2026-11-22` · FACT, with a LIKELY cause · `#lodestar` `#probe` · refs: O105, docs/ops/probes/2026-11-22-l5.md, WHY§351

- First sweep 192 s, second 431 s, the third unfinished (7.7 GiB a sweep; 40, then 18 MiB a second). The reader's thread
  fell to 7.5 readings a second (2.3 at worst) and 9.3 steps a second. No call, nothing written, the game's own threads
  unused, no fault. Lodestar's estimate was far too low.
- LIKELY cause: Windows had to bring much of that memory in before it could be read, so the game's memory in use grew
  while the scan ran. Whether the game stuttered is asked of the owner.
- The scan stays off from here. Were one ever wanted again: only pages already in use, the time checked after every
  piece, one sweep, and the owner asked again (O105's note).
- *Later, 2026-11-22:* the owner saw the game stutter during the scan: "yes it did several minutes in. but only a few
  times within a few seconds, and then I had no more stutters." The fifth round no longer starts the scan; ObjectScan
  stays in en-gal with its test (WHY§365).

### 358. A round's report keeps what was seen in the world, not the last pass's state, which is made as the game closes

`2026-11-22` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md

- The fourth round's report kept the last pass's counts, so `others` and `new_statics` show the closing game; the log
  has the store's count for every pass. Mended in the next build.

### 359. event_flags (1008) is the object the event flags are read through, the store's class its lead; world_clock (1012) waits for L6

`2026-11-22` · DECIDED · `#lodestar` `#gal` · refs: WHY§354, WHY§355, proposal 0005 §4.1, ARCH§6.1

- Engine's ruling on Lodestar's two points, as Lodestar proposed. An id is ours and says what a thing is for EldenNet,
  never the game's name for it (proposal 0005 §4.1): 1008 `event_flags` was never placed, and what EldenNet needs is the
  object the flags are read through. So 1008 keeps its number and label, its lead in `gal/leads.toml` becomes the store's
  class, and `flag_store` as a separate trial lead goes; the next round names it by O92's own rule, with the
  accessor's sites as witnesses. A new id with 1008 retired was the alternative: rejected, since nothing used 1008.
- 1012 `world_clock` stays reserved and unplaced until L6 gives the clock's manager an id and the clock a field under it.
  Nothing in v1's start needs the clock as a static. ARCH §6.1 carries a dated note.
- *Later, 2026-11-22 (WHY§394):* 1012 `world_clock` is the object that holds the time of day, entered by the invariant
  recipe when a round names it so. The lead under the clock's longer name stays a lead for its own class and is no
  longer the clock's. Lodestar's intent; Engine agrees.

### 360. A holder's end could be lost when its box was full; since GraceUI 0.2.15 it always comes

`2026-11-22` · FACT · `#graceui` `#input` · refs: GraceUI's D207, GraceUI's D176, S11.4.c

- D176's box pushes the end, and when full empties the ring and pushes again. In Vyukov's bounded queue a slot that
  another thread's pop has claimed is not free until that pop stores its sequence, so the second push could still read
  full and drop the end, and EldenNet's chat box would not hear that it lost input. Now such an end waits in the box's
  own slot, given first by take(); nothing is reordered, since an ended holder's events are discarded anyway.
- Found by GraceUI's TSan tree under load, in the holder-mail stress test, whose own check (reading its ended flag after
  take) raced its other taker and now reads it before.

### 361. GraceUI's key names follow the browser's KeyboardEvent.code, and pad buttons the Xbox pad's own names

`2026-11-22` · DECIDED · `#graceui` `#hotkeys` · refs: GraceUI's D206, GraceUI:DESIGN§5.9, S11.2.ab

- KeyboardEvent.code without its Key and Digit prefixes, 1 to 12 letters and digits as the settings' key names allow
  (A, 7, F5, PageUp, NumpadEnter), so a browser capture maps across. Pad buttons as the Xbox pad prints them (A, B, X,
  Y, LB, RB, LS, RS, View, Menu, D-pad Up).

### 362. Every mod's hotkeys are rebound on GraceUI's Hotkeys page; EldenNet's three key settings become registered defaults

`2026-11-22` · DECIDED · `#graceui` `#hotkeys` `#pages` · refs: G37, GraceUI's proposal 0009, O94, O77, PAGES§4, S11.2.ac

- The owner's answers to GraceUI's G37, put by Engine with Grace's and Engine's shared recommendation (2026-11-22):
  "GraceUI's Hotkeys page (Recommended)", "Ask, then move it (Recommended)", "Pad chords by two pickers (Recommended)", "Mods read their key (Recommended)", "Now, in 0.2.x (Recommended)". So: (1) every binding is changed on GraceUI › Hotkeys and kept in GraceUI's settings under the mod's and the
  action's ids; (2) a taken key is asked about, then moved, leaving the other unbound; (3) pad chords are chosen with two
  pickers, and capture on a pad waits for testers; (4) ABI 1.4's describe_hotkey and a changed event; built now, in
  0.2.x, not in 0.6.
- EldenNet's answer to proposal 0009 §8 item 5: yes. `hotkeys.talk`, `hotkeys.skip_page` and `hotkeys.debug` stop being
  EldenNet settings and become the defaults it registers (T, its skip key, F11). EldenNet's Settings › Hotkeys becomes a
  read-only list of its own hotkeys from GraceUI's topic, with a link to GraceUI › Hotkeys. O94 stands: T is the default,
  rebound on GraceUI's page. The call-out names the key from describe_hotkey.
- Off game (O76, O77): the bindings follow where GraceUI's Hotkeys page is served. If the launcher's host does not
  serve it, they are on O77's short list of settings that need the live game. Asked of Grace.
- *Later, 2026-11-22:* Grace answered (GraceUI's D211): not for now. A hotkey exists only as a mod's registration in
  game, and the launcher's host holds neither GraceUI's own space nor its hotkey table, so the bindings are on O77's
  short list, in game only (O77's note). Built as GraceUI 0.2.16 the same day (WHY§366).

### 363. R1 is retired: what goes, what stays, and who removes it

`2026-11-22` · DECIDED · `#engine` `#lodestar` `#probe` · refs: O106, S10.1.f, S10.1.k, WHY§302, WHY§303

- By O106. What goes, from the engine: R1's package and its README, the DLL's R1 probe mode (`probe_r1.cpp`), the
  launcher's `--probe` and its texts, R1's request in the watch block, the fake game's R1 cases, and the derivation and
  call of `register_task`, wherever nothing else uses them.
- Lodestar removes them, since the probe and en-gal are its area. It works on feat/lodestar, so nothing conflicts with
  its next round, and it puts dated notes in the design's sections that describe R1 (ARCH §2, §3.2, §4.1, §4.14, §6.1,
  §6.2, §6.10; DESIGN §16.1).
- What stays: R0, as Lodestar's `lodestar find`; the read-only probe build (`--read`); the findings, in WHY§206 to §303
  and `docs/ops/probes/`; and the code in git history. O92's naming rule stays too, since Lodestar names managers by it.
- *Later, 2026-11-22:* done by Lodestar (engine 3e11238, merged as 9050db4): 51 files and about 14,800 lines out. Gone:
  the DLL's R1 mode, `--probe`, R1's package, eight fake-game cases, en-gal's hub, callee, site and map probes, and the
  report's R1 vocabulary. The watch block's two R1 numbers are kept as retired words. Kept, since other parts use them:
  the read round and `--read`, the searches, and the task and registration code that en-gal's tests and the fake game's
  register cases use. One case was added: the read round with a blocked console. Engine's rulings on the seven doubtful
  items are in WHY§372.

### 364. The first conversation, offline: its plan in four parts, and Engine's rulings on the survey's open points

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: S11.3.a, S11.3.b, S11.3.e, S11.4.c, O107, DESIGN§8, DESIGN§14, NPC§5, NPC§7, NPC§8

- The owner chose it on 2026-11-22, while Lodestar removes R1: v0.2's talk with one NPC, built and tested with no game,
  on the fake game and a fake model server. A read-only survey mapped the design against the engine. Four parts follow.
  Part 0 is the shared types: en-proto's UI model and events, the chat request's stop sequences, and the core's wider
  input and output. Then two parts in parallel: A, speech and the prompt (the prompt's blocks, the sanitizer, the
  lint, pages, `render-prompt --fixture`); and B, the conversation and the eval (content and identity, the states and
  timers, gating, commit on display, `eval --llm mock`, `mockllm`). Last, C, the surfaces in the DLL (the chat box,
  subtitles and hint as GraceUI panels, holder input, the pad chord), under Wine.
- Engine's rulings on the open points, from the design:
  - (1) The end-to-end test uses an invented test persona, bound to an invented fake-game character through the
    overrides folder (DESIGN §11.4), in tests only.
  - (3) With no `on_present` in GraceUI's ABI yet, T6 submits at most 10 times a second: fades step, and `PageShown`
    means the first submitted frame.
  - (5) The call-out box waits for v0.6, as DESIGN §16.2 has it.
  - (6) The pad chord's order is GraceUI's, D-pad Up then View (its D204), and the Steam floating keyboard stays
    unbuilt in v0.2.
  - (7) v0.2 keeps the event log in memory behind the store's interface; the log written per page waits for the store
    (v0.3).
  - (8) Since G15 T6 alone calls GraceUI, so ARCH §1.3's present-thread channels get a dated note.
  - (9) Prompts use en-prompt's syntax (`[ system ]`, `[ cache ]`, `join()`) in place of NPC §7.3's sketch.
  - (10) CI's eval runs mock and recorded models only; live and judged sets wait for a key. A report holding model text
    is a local-only kind, never committed or uploaded.
  - (11) The truth tables are written now and gated until S11.3.c's `known(P)` lands.
  - (12) The since-line waits for v0.3.
- Asked elsewhere: the slurs' list (13) became O107, decided as hashed. The chat box's faces (2) and the 50 ms open
  against T6's wait (4) are asked of Grace, and part C waits for the answer.
- *Later, 2026-11-22:* Grace answered. (2) Faces as (a): the chat box's text is built from GraceUI's glyph_table
  (advances, quads, UVs into the shared atlas, the face's metrics, the atlas's texture id and generation; no kerning).
  If Dear ImGui 1.93 will not take glyphs baked elsewhere, EldenNet emits each glyph's quad itself. A scale change waits
  for the next load until GraceUI rebuilds its atlas; then the box re-reads the table when the generation changes.
  `on_present` is not planned soon, so ten submits a second stand. (4) A wait handle per mod (GraceUI's D71) comes as
  ABI 1.5: GraceUI sets a mod's Win32 event whenever something is queued for it, never a callback. T6 then waits on it,
  with its 100 ms as the fallback (S11.2.ad).

### 365. L6's first tool, the watch: a way to an object, its first 512 bytes once a second, and counts of how each word behaves

`2026-11-22` · METHOD · `#lodestar` `#l6` · refs: proposal 0005 §6.1, docs/ops/probes/2026-11-22-l5.md, WHY§355, WHY§359

- The fifth read-only round (engine 24e2918, package 0.1.0-lodestar.8), on the reader's thread within the searches'
  4 ms. A way to an object is a static, an offset and at most one more read. Once a second the watch copies the first
  512 bytes through the ask-first copy: each named manager's object, the world's clock (the pointer 192 bytes into its
  manager's object, and the object 1,024 bytes in) with the object it was found in, and the object whose field rose by
  the clock.
- For each 8-byte word that was ever not zero, it counts how often each half changed and rose, and what the word looked
  like (zero, small, a table of methods, the image, the heap, other; heap asked of VirtualQuery, nothing read). In the
  clock it also counts runs of 6 bits that behave as minutes with runs of 5 that behave as hours, and 32-bit fields that
  rise steadily read as a fraction. Counts and offsets only, never what a word held.
- The same round names 1008 `event_flags` through the store's class (WHY§359), and its counts are the fullest pass's,
  not the closing game's (WHY§358). No scan.

### 366. Rebinding is built: one home for every binding, kept by mod and action ids, applied at each registration

`2026-11-22` · DECIDED · `#graceui` `#hotkeys` · refs: G37, GraceUI's proposal 0009, GraceUI's D208 to D211, GraceUI's D115, S11.2.ac, WHY§362

- GraceUI 0.2.16 (protocol 1.4, ABI 1.4). Every binding lives on GraceUI › Hotkeys, kept by mod and action ids and
  applied at each registration. A hotkey that lost its key to a move is kept with none, so the next session's order of
  registration cannot undo the move. The menu's key is never moved from it. Off game, bindings are in game only for now
  (D211; O77's note).
- describe_hotkey (has_cap("hotkey-bindings")) and GRACEUI_EVENT_HOTKEY_CHANGED reach only a mod connected with minor 4
  or more. EldenNet keeps its hotkeys' action ids stable across releases, since the bindings are kept by them.

### 367. A mod may register before GraceUI reads its settings file, so kept bindings also go over registered hotkeys; that path is reviewed, not yet seen run

`2026-11-22` · FACT · `#graceui` `#hotkeys` · refs: GraceUI's D208, WHY§366

- A mod's hotkeys then register with their defaults, so when the file is read the kept bindings go over them, each kept
  one freed first so that a key one gives up is free for another. In the harness's runs the file was read before the mods
  registered, so the at-registration path ran and the late one did not: the late path is reviewed, not yet seen run.

### 368. The fifth read-only round: event_flags named through the store, ten of eleven managers placed, 30 minutes with no fault

`2026-11-22` · FACT · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§359, WHY§365

- Package 0.1.0-lodestar.8, the owner's run, 32 minutes, no scan. event_flags was named at the first pass, at the
  title: one of the accessor's statics (183 sites), read 15,878 times, always there, always of its class. Ten of the
  eleven managers are placed. The build's table has 13 entries, every one a lead (engine d613651).
- The reader: 15,878 readings of ten managers, 472,090 guarded copies, no fault of any kind; p50 4 µs, p99 9 µs, the
  longest 284 µs.
- The flags' store's first 512 bytes never changed but for one counter. The word 40 bytes in is an address of live
  memory, the notes' lead for the way to the flags. Of 219 statics in play, 140 hold an object with a class, 72 one
  without and 7 nothing.

### 369. The play time is the low half of the word 160 bytes into the object behind one slot of the data, in four sessions

`2026-11-22` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§348

- It rose at each of 1,556 compared readings of the watch, a second apart. The rising search saw it in 296 of 307
  passes. Four sessions, the same slot and offset.

### 370. An ask-first read costs about half a millisecond on the owner's PC, a hundred times a plain guarded copy

`2026-11-22` · FACT · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§343

- VirtualQuery, then ReadProcessMemory on our own process: about 0.5 ms, against 4 µs for a plain guarded copy. The 4 ms
  share was checked only after 4 to 16 such reads, so the reader fell to 8.7 readings a second. A share of time for such
  reads must be checked after every one.

### 371. A long round's report can pass the writer's 256 KiB and is then refused whole, so a round bounds what it writes

`2026-11-22` · METHOD · `#lodestar` · refs: report.hpp, docs/ops/probes/2026-11-22-l5.md

- The fifth round's report passed the numbers-only writer's 256 KiB after 30.5 minutes and was refused 49 times, the last
  100 s, so what was sent is the round as it stood then. A round must bound what it writes (the watch's words), or stop
  adding to it. Also to mend before another run: the search named the HUD before a pass did, and the round kept the
  search's weaker entry.
- *Later, 2026-11-22 (the sixth round):* the fallback must be graded. The sixth round's report passed the limit when its
  two 16 KiB ways joined (415 s in), and the fallback dropped every watched word at once, the flags' next step's among
  them; finds and counts were kept. A long way writes only the words that changed, and an over-large report loses the
  largest object's words first.

### 372. R1's leftovers: remove what nothing uses, keep retired words in append-only vocabularies, keep what L6 and the task path use

`2026-11-22` · DECIDED · `#engine` `#lodestar` `#probe` · refs: O106, WHY§363

- Engine's rulings on the seven items Lodestar left in place after O106's clean-up:
  - (1) TaskShape::every_slot, which only R1 set: remove it, if the register and gal cases still pass without it.
  - (2) probe.hpp's vtable, meter, UTF-16 and u32 offsets, which R1 never used: Lodestar's call, keep them if L6 will use
    them.
  - (3) derive's read_instance_word and task_from_base, used outside derive only by R1: remove them, unless R0's
    offline derivation needs them.
  - (4) BannerReason::RegisterFaulted: keep it as a retired word of the recording vocabulary, which is append-only. Nothing
    pushes it.
  - (5) The DLL's imports allowlist: drop HeapLock, HeapWalk and HeapUnlock, since nothing imports them.
  - (6) The fake game's options that no case passes (--table-gap, --task-gate, --task-queue, --map-late,
    --press-pause-every): remove them and the code only they reach. Keep the parts the register and gal cases go
    through.
  - (7) The harness's fd4_idiom pattern in the fake game's patterns.toml: remove it, since patterns.toml is withdrawn
    (O102).
- *Later, 2026-11-22:* done in engine 532e751 (merged as the commit after it). Item 2 kept, by Lodestar's call (L6's fields
  are found that way); of item 3, task_from_base kept, since the offline derivation uses it for the leads' task base.

### 373. A wait handle for each mod's events: one auto-reset event per mod, set whenever something is kept for its poll

`2026-11-22` · FACT · `#graceui` `#abi` · refs: GraceUI's D212, GraceUI's D71, S11.2.ad, NPC§5.1, WHY§364

- GraceUI 0.2.17, ABI 1.5, at the owner's word. The event is made at the first ask and never closed while GraceUI runs,
  so a thread that read it can always set it; the mod gets a SYNCHRONIZE handle of its own (wake_event,
  has_cap("wake")). It is also set once at the ask, so the first wait polls what may already be there.
- It is set from whichever thread keeps something for the mod's poll: the window procedure, XInput's reads, the server's
  I/O thread, GraceUI's thread, and the present thread for a resize. The holder mail's and the registry's hooks are plain
  function pointers taking no lock, so the I/O thread's wake cannot meet the registry's lock.
- Under Wine: 1 to 5 ms from a hotkey's key to the wake, about 1 ms after a holder's key, 2.5 to 7 ms after a resize;
  with nothing to poll it stayed unset. T6 waits on it, with its 100 ms as the fallback.

### 374. A static with no class to name it by is found by what its object holds: save_data by the play time

`2026-11-22` · METHOD · `#lodestar` `#l6` · refs: proposal 0005 §6.1, gal/leads.toml, WHY§369

- The slot behind which one field rises by the clock in nearly every pass is the save data's (1002 `save_data`, found_by
  invariant). It must lead in at least ten passes, by three times the next best, and no lead's manager may sit at it. The
  code's references to the slot are its sites and witnesses. This is proposal 0005 §6.1's invariant recipe, which the
  lead in `gal/leads.toml` has asked for since R0c. Four sessions gave the same slot and offset before the round was let
  write it (the sixth round, engine 48e5003, package 0.1.0-lodestar.9).

### 375. What the profile editor needs that GraceUI lacked: a text field in game, master-detail, a topic per row, a longer deadline

`2026-11-22` · FACT · `#graceui` `#pages` · refs: GraceUI's proposal 0010 §1, GraceUI's D57, GraceUI's D169, GraceUI's D195, S7.2.a

- GraceUI had no text field in game (D57 is designed, not built), and neither view drew master-detail. There was no
  topic per row, so the old §6.5's `named/<id>`, `journal/<id>` and `conversation/<id>` could not be declared. Answers
  had a 10 s deadline, against the draft helper's 60 s.
- A command's answer body (`result.body`) already goes only to the view that asked, and is never kept. So check
  messages and drafts need no new route.

### 376. The profile editor is the one content editor in v1, by O43; its rules are EldenNet's since G20

`2026-11-22` · DECIDED · `#graceui` `#editor` · refs: GraceUI's D213, G38, O43, O46, O86, S7.2.a

- GraceUI's D2 gets one exception, D213: the profile editor. Content packs stay read-only. The editor's pages, fields,
  checks, commands and draft helper are EldenNet's (S7.2.b, NPC §4.7), and so are the rules the needs list asked
  GraceUI to change: SP1's subjects and the player's own text, SP4's people met, and PV4's draft text (PAGES §4.8,
  §4.9).
- The needs list was brought to O46 and O86 in proposal 0010 §2. What GraceUI owes was put to the owner as G38:
  topic families, master-detail in both views, the editor block, slow commands, and the text field in game (protocol
  1.5).

### 377. EldenNet's answers to GraceUI's proposal 0010: the eight kinds suffice, with keyed and locked list items, unset fields and file-level messages

`2026-11-22` · DECIDED · `#engine` `#editor` `#pages` · refs: GraceUI's proposal 0010 §6, NPC§4.5, NPC§4.7, ARCH§1.3, O46, S7.2.b

- (1) **The eight kinds cover NPC §4.5 and O46**, mapped as follows:
  - `line`: `brief`, `display_name_fallback`.
  - `prose`: `identity`, `public.*`, `private.*`, `voice.*`, `allegiance`, `home`, `instructions`, and the Tarnished's
    two fields.
  - `choice`: `register`, `speech`, `pronouns`.
  - `pick`: `base`.
  - `number`: `words_max` and `reply_words_max`.
  - `switch`: `replies`, `ambient`.
  - `list`: `knows`, `lexicon`, `subjects` (with the shipped list as reference), and one per seed key, at most 3.
  - `shown`: `[[seen]]`'s "Met".
  - A `knows_when` entry is a list item with a world stage and its text.

  Two additions:
  - A list item may carry an opaque `key` of EldenNet's, echoed back. This keeps an entry's per-item fields (a
    `knows_when`'s `spoiler` and `lexicon`) across reorders.
  - A list item may carry `locked`, shown and movable but not editable, for a hand-written `when` the picker does not
    offer. It is sent back unchanged.
- (2) **`doc` as {field id: value} suits C17.** Field ids may be dotted (`public.summary`, `seed.greet_player`).
  EldenNet's handler merges the doc with what the editor never shows (`schema`, `base_hash`, `[[seen]]`, hand-only keys
  and locked items) and writes the TOML into `ProfileWrite`'s text. One addition: `null` means "not set here", distinct
  from empty, so a layer can leave a field to the shipped profile.
- (3) **Messages as {field, item, level, words} and `conflict` for a stale rev suit `profile.check` and `profile.save`.**
  - A message with no `field` is about the whole file (a newer schema, the 8 KB cap) and shows at the top.
  - The save is answered when T10's ProfileResult comes back on C11, well within 10 s. A full C17 answers busy.
  - The member's body is then republished with the new rev.
- (4) **{draft, notice?} as a slow command suits the helper** (20 s to the first token, 60 s in all, within 90 s). If the
  view unwatches the member while a draft runs, EldenNet cancels its call, using the UNWATCHED event it already polls,
  so GraceUI needs no cancel.
- EldenNet agrees with GraceUI's recommendations to the owner on G38: (1) (a) all of it now; (2) (a) unsaved edits kept
  in the menu's memory, never on disk; (3) (a) prose as one paragraph, since every player field is one paragraph of at
  most 60 words.
- *Later, 2026-11-22:* the owner decided G38 in Grace's chat the same day, all three as recommended. GraceUI builds
  proposal 0010 now, the editor block last.

### 378. GraceUI's own text field in game: one field model for every kind, every character handed over in order, a secret never laid out

`2026-11-22` · DECIDED · `#graceui` `#input` · refs: GraceUI's D214, GraceUI's D57, G38, GraceUI's proposal 0010 §3.5, S7.2.c

- GraceUI 0.2.18, G38 (1) (a)'s first part; protocol 1.4 and ABI 1.5 unchanged. One pure field model serves every
  kind: text, a secret masked as discs, a form's fields, a row secret and search. It holds one paragraph, edited by code
  point.
- While a field edits, every key is the field's. The host hands GraceUI's thread every character in order with the
  keys, so none is lost to an edit not yet begun, and drops there the ones no field wants, with no new layout.
- A secret is never laid out as glyphs, copied or kept: the view holds only whether it is set. In game, EldenNet's text
  settings, secret settings, forms and row secrets are typed from 0.2.18, with the clipboard and the IME. Search opens
  with / or Ctrl+F.

### 379. Posted key messages do not order as a keyboard's: the character comes after the key-up, so the harness types through SendInput

`2026-11-22` · FACT · `#graceui` `#harness` · refs: GraceUI's D214, WHY§378

- With WM_KEYDOWN and WM_KEYUP both posted, TranslateMessage's WM_CHAR is queued after the key-up. So the character
  comes for a key already released, and the message rules rightly give it to no holder. A real keyboard's key-up waits
  in the input queue behind the posted character.
- The typing scenario therefore injects a key it types through SendInput, and posts directly the characters it needs
  typed.

### 380. A search hit or a shortcut row on a page with tabs now opens the row's own tab, section and disclosure

`2026-11-22` · FACT · `#graceui` `#pages` · refs: GraceUI's D214, GraceUI's D189, GraceUI:DESIGN§4.3

- Until 0.2.18 both opened the page's first tab, with the row unfocused. They now open the row's own tab, its section
  and its Advanced disclosure.

### 381. The sixth read-only round: save_data in the table by the invariant recipe, 14 leads, the reader at 9.6 a second

`2026-11-22` · FACT · `#lodestar` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§374, WHY§370

- Package 0.1.0-lodestar.9, the owner's run, 17.7 minutes, no scan. save_data is the slot behind which the play time
  rises: the field rose in 33 of 36 comparisons, the next best in 5, at the same slot and offset as in four sessions
  before. 736 instructions refer to the slot. The build's table has 14 entries (engine 8e7295d), the HUD named by a pass
  again.
- The reader: 10,082 readings in 1,054 s, 9.6 a second (8.7 before) and 24.9 steps a second (11.9 before). The median by
  ten-second stretches was 9.8, the least 7.6. 296,651 guarded copies, no fault. With the share checked after every
  ask-first read, it ran out in 25,802 of 26,270 steps, as intended. Ten managers where they were, a sixth session.

### 382. No clock packed into bits in any static's object, nor in 16 KiB of the clock class's object; one slowly rising fraction is the only candidate

`2026-11-22` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§355

- 219 wide ways of 72 readings each, and 16 KiB of the object of the clock's class and of its manager (589 readings
  each): no run of bits went round as minutes do, over 17 minutes of play.
- Three fractions kept rising in three statics' objects. Two rose by about one a second, likely seconds as fractions.
  One rose by one to two thousandths a second, the only candidate for a time of day kept as a fraction. A rate written in
  thousandths a second is too coarse for so slow a rise.
- The flags' next step, the objects behind the store's addresses 40, 56, 80 and 136 bytes in, was read about 978 times
  each; its words were lost to the report's fallback (WHY§371's Later line).
- *Later, 2026-11-22 (Lodestar's correction):* the first bullet says more than the round showed. The minutes test takes a
  run of six bits only if it steps by 1 to 5 between readings, and the statics' objects were read about every 15 s. So
  what holds is this: no slow clock in the first 512 bytes of any static's object, and no clock of any pace in the 16
  KiB of the clock class's object or its manager, which were read every second or two. A clock whose minutes pass faster
  than one in three seconds of real time is not ruled out in a static's object (WHY§385).

### 383. Master-detail is a page's, not a block: the page's own tabs and sections are the chosen row's detail

`2026-11-22` · DECIDED · `#graceui` `#pages` · refs: GraceUI's D215, GraceUI's D36, GraceUI's proposal 0010 §3.2, S7.2.c, S11.2.af

- GraceUI 0.2.19, protocol 1.5, no ABI change. A page's master list chooses a row, and the page's own tabs and sections
  are its detail. A block bound to the master's family draws the chosen row's member. Proposal 0010 §3.2 had it as a
  block, which would have needed blocks and tabs nested in the page data. Named NPCs' list, then its Profile, Allowances
  and Reactions tabs, is the page's own shape.
- A family is a state topic declared `family = true`, whose members `<family>/<key>` are topics of their own, each heard
  as WATCHED and UNWATCHED under its own id. EldenNet's `named/<id>`, `journal/<id>`, `conversation/<id>` and
  `profiles/<target>` fit. The first row is chosen as soon as the list has one, and a page keeps its chosen row for the
  session, never in the address, storage or a log.

### 384. A family's members in the server: 64 slots a space, taken at the first watch, each state carrying its slot's generation

`2026-11-22` · METHOD · `#graceui` `#serve` · refs: GraceUI's D215, WHY§383

- Each space has 64 slots after its declared topics. A slot is taken by a member's first watch, and given back only once
  nobody watches it and the space has polled its unwatch, so the space hears every member's watch and unwatch under its
  own id.
- A slot's generation is set when it is taken and carried by each published state. It keeps a state published for one
  member from reaching the next member in the slot.

### 385. A test for a clock must not depend on its pace against the reading's: hours alone, and a steady rise with a jump

`2026-11-22` · METHOD · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§382

- Test the hours alone: five bits, never 24 or more, stepping by one. And look for a steady rise with a jump when the
  player lets time pass. The seventh round, being built, tells the clock by such a jump: steady rises and jumps of
  fractions and whole numbers, the hours standing alone, and each jump's second with its number before and after.

### 386. A slow command is named in the space's page data, not in its answer, so both views know its deadline before it is sent

`2026-11-22` · DECIDED · `#graceui` `#protocol` · refs: GraceUI's D216, GraceUI's D169, GraceUI's proposal 0010 §3.4, S7.2.c

- GraceUI 0.2.20, protocol 1.6; the ABI unchanged but for a constant. A space names at most 8 of its own commands in
  `slow_commands`, never `settings.set` or `settings.reset`. GraceUI waits up to 90 s for such an answer from either
  view, instead of 10 s, and answers internal past that.
- In game the control reads "Working…" and takes no press until the answer comes. The browser waits 95 s. A slow command
  still counts in a view's 20 a second and in the space's queue of 32. EldenNet names `profile.draft`.

### 387. EldenNet keys a profile's family member by the same slot as its list row, with a dot for the colon

`2026-11-22` · DECIDED · `#engine` `#editor` `#pages` · refs: GraceUI's D215, NPC§4.7, WHY§383, S7.2.b

- Grace widens D215 for part d: on a page with a master list, a block bound to any family the space declares draws
  `<that family>/<the chosen row's key>`. Named NPCs can then carry a "Your notes" tab whose editor is bound to
  `profiles`.
- A family key takes only a-z, 0-9, '.', '_' and '-', so a slot's colon becomes a dot: `persona.<id>`,
  `character.<id>`, `mine.<id>`, `named.<param>`, `card.<id>` (NPC §4.7). A list row and its profile member share that
  key, so GraceUI needs no field naming a second key's column.
- When the player writes a profile for a `named.<param>` row, the row becomes `mine.<id>`, as NPC §4.7 has it, and the
  page chooses the new row. If S7.2.b finds a case this does not cover, EldenNet asks for the column field then.

### 388. The prompt's blocks 2 and 11: block 2 stays at 120 tokens, checked whole by the lint; block 11 rises to 70 with one tic note

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§7.3, NPC§3.1, NPC§4.5, WHY§364

- Part A found two caps smaller than their fields allow. Block 2's cap is 120 tokens, but its fields at their schema
  caps come to about 300 (identity 40 words, role 20, summary 60, appearance 40), and the test fixture's ordinary fields
  reached 129. Block 11's cap is 40, but its ask alone is about 26 tokens, a tic note about 20 and a reminder about 12.
- Raising both to fit would bring the caps' sum (3,460, NPC §4.5) past the 3,500 hard cap. So Engine rules:
  - **Block 2** stays at 120 and is never trimmed. The content lint checks it as a whole: rendered past its cap it is an
    error for shipped and test content, and a warning for the player's own files, which still render under the hard cap.
    The field caps stay as maxima; a writer keeps the whole within the block.
  - **Block 11** rises to 70 and carries at most one tic note, the newest, beside the reminder. The caps' sum becomes
    3,490.
- Built by the joining of parts A and B (S11.3). NPC §7.3 gets a dated note when that lands.
- *Later, 2026-11-22:* built so (WHY§428, §429), but block 11 does not always fit its 70: the worst case (a long name,
  a long tic, the reminder) comes to about 85 tokens, and is reported past its cap, never refused or trimmed.

### 389. A flag is told by a single bit that flips once in memory that otherwise keeps still

`2026-11-22` · METHOD · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§368, O105

- Sweep the stretch, keep each page's own first flips, and list only quiet words of quiet pages. Memory in use changes
  everywhere and would crowd a shared list.
- The seventh round (engine 9f451ec, package 0.1.0-lodestar.10) sweeps up to 2 MiB from each of at most eight
  addresses the flags' store keeps, and up to 256 KiB from each of at most 32 one step further: about 24 MiB in all.
  It reads a piece of 16 KiB at a time, within the searches' 4 ms share of a step, with a pass every thirty seconds at
  most. It keeps the place, bit, direction and second of a flip, never what the memory holds. This is not O105's wide
  scan, which stays off; by O105's note it is put to the owner before the run.

### 390. A round's report may carry a clock candidate's reading before and after a jump, and nothing else the memory held

`2026-11-22` · DECIDED · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§385

- Without it, a jump cannot be matched to the time of day the owner chose at the grace. So for a number that jumped, the
  report keeps the number before and after each of its first three jumps, and the second each came. That is the clock's
  own reading if the number is the clock: a state of the owner's own session, not the game's content.
- Every other value stays out: the report holds offsets, sizes, counts and our labels as before. The README says so in
  plain words. Engine agrees, on that bound.

### 391. An editor's unsaved edits are kept by the document's topic, not by the page, so leaving a page loses nothing

`2026-11-22` · DECIDED · `#graceui` `#pages` · refs: GraceUI's D217, G38, GraceUI's proposal 0010 §3.3, S7.2.c, WHY§377

- GraceUI 0.2.21, protocol 1.7, no ABI change: the editor block in both views, with EldenNet's five additions (an
  item's key, locked items, dotted ids, null as "not set here", file-level messages).
- Both views hold a map from the document's topic (a family's member) to the player's edits for the session. So neither
  leaving a page nor closing the menu loses anything, and neither view asks then; the browser asks only before its tab
  closes. A draft still goes when its page is left, as proposal 0010 §3.3 has it. This changes §3.3's "leaving the page
  asks first" (D217).
- The commands are check and save {target, rev, doc}, reset {target, all, confirm}, restore {target} and draft {target,
  notes}. A stale save answers `conflict`, shown with Reload. With this, all four parts of G38 are built on GraceUI's
  side.

### 392. Prose in game moves by line from the last layout's boundaries, so the view needs no font outside the layout

`2026-11-22` · METHOD · `#graceui` `#view` · refs: GraceUI's D217, GraceUI's D214, WHY§378

- grace-view's layout keeps each boundary's offset, x and line top for the prose field being edited. Up, down and the
  pointer take the nearest boundary on the line above, below or under it.

### 393. A save names the row the list chooses next, since a list alone cannot tell a renamed row from a removed one

`2026-11-22` · DECIDED · `#graceui` `#pages` · refs: GraceUI's D218, GraceUI's D215, WHY§387, S7.2.c

- GraceUI 0.2.22, protocol 1.7 and the ABI unchanged. A save's answer body may carry `chosen: "<key>"`, a member's key of
  the page's master family. The view that saved takes it as the chosen row once its list has that row, whichever of the
  answer and the list's new state comes first, unless the player chooses a row by hand in between. Focus stays on Save
  once the new row's member draws. A key that is no member's, or one on any answer but an ok save, is ignored.
- EldenNet sends it when a `named.<param>` row becomes `mine.<id>` (WHY§387). Also fixed: for one render after a block's
  topic changed, the browser could still draw the last topic's body.

### 394. The time of day: the hour and minute packed in one word behind an accessor's static, told by the owner's two passings of time

`2026-11-22` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§385, WHY§390, WHY§355, WHY§382

- The seventh read-only round (package 0.1.0-lodestar.10, the owner's run, 16.7 minutes). The object is behind the
  accessor's static 64422792 (29 sites; an object without a class). The 8-byte word 8 bytes in holds the hour in the
  five bits from bit 34, and the minute in the six bits from bit 39. The word 24 bytes in holds the same. The same two
  words stand 216 and 232 bytes into the object behind static 64422928 (18 sites); the first object may lie inside the
  second, to be checked.
- Over 146 readings, the hours stepped by one five times, and the minutes stepped forward 142 times and went round five
  times. At the two moments the owner let time pass (525 s and 727 s into the watch), the hour went from 17 to 12 and
  from 13 to 20, as the README asked, at the same second in all four words. A game hour took about 190 s, a game minute
  about 3 s. The sixth round missed it because its readings were about five game minutes apart.
- The reader: 9,834 readings in 986 s, 9.97 a second; 4 µs at the median, 7 at the 99th percentile, 89 the longest; no
  fault. Ten managers where they were, a seventh session. The graded fallback worked: 13 objects lost their words one at
  a time and said so, and the report was written to the end.
- *Later, 2026-11-23 (the eighth round, WHY§447):* world_clock is in the build's table by the invariant recipe. The second
  static's object keeps the same words in other memory.

### 395. The flags' blocks are likely the plain data behind the store's address 40 bytes in, and their tree behind 56

`2026-11-22` · LIKELY · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§389

- Behind the address 40 bytes in: plain data, 2 MiB readable at every pass, of which only 585 of 262,144 words ever
  changed in 16 minutes, by single bits here and there. Behind the address 56 bytes in: records of 64 bytes, each three
  addresses and a small number, the shape of a tree's nodes.
- Not yet checked against a flag the owner sets: the sweep's list (24 flips a target, the quietest pages first) was
  filled by the first comparison, half a minute in, so what the owner did later is among the 852 flips counted but not
  listed.
- *Later, 2026-11-23 (the eighth round, WHY§448):* bits are set in that stretch where the owner does something the game
  remembers, and nothing else flipped there around the marks: the first candidate for the blocks holds so far.
- *Later, 2026-11-23 (the ninth round, WHY§451):* confirmed: the holder of the blocks, 125 bytes each at the holder plus
  the block's index times 125, and the tree behind 56 bytes in.

### 396. A list of what changed must be told when to look: the tester marks a task done

`2026-11-22` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§395, WHY§398

- The tester marks a task done, and the round keeps what flipped after each mark. A list filled in the order seen is
  used up by the first comparison.

### 397. An invariant asks for what the thing does, not for a margin over the next candidate

`2026-11-22` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§374

- The play time rises at every comparison and never falls. In the seventh session its field rose in 41 of 43 passes at
  the same slot and offset as ever, but another field rose in 35. The rule asked for three times the next best, and so
  refused the right one: that round's draft table has no save_data, and the build's table keeps the sixth round's.
- Likewise 24 statics' objects held runs of bits that passed as hours; the clock stood out only by its jumps and the
  hours they landed on.

### 398. The owner asks for a tester's panel: each task of a round in order, with a timer and a mark when done

`2026-11-22` · FACT · `#lodestar` `#owner` · refs: S10.1.l, WHY§396

- The owner, after the seventh round (2026-11-22): "these time windows will be hard to follow, even if the terminal states them. For the next scanner/tester, can you add a timer that shows each phase and description? Basically a UI frame that shows all the tasks in order to be done, with a timer for each task (and a confirm method that the task was done and seen by the tester, such as grey out those tasks that have been done). You could ask grace agent if that's already feasible."
- Lodestar asked Grace whether GraceUI's overlay can show such a panel in the real game today. If it cannot yet (its
  hooks in game wait for S0-E's spike), Lodestar builds a window of the launcher's own, outside the game, which needs no
  hook. The tester's marks are also what the sweep needs to know when to look (WHY§396). A new field in the launcher's
  watch block (en-proto) would be agreed with Engine first.

### 399. The UI model is state published whole; the UI events are five, versioned at the top only

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§5.1, NPC§6.1, NPC§8.6, NPC§8.7, NPC§10.1, ARCH§1.3, WHY§364

- The first conversation's part 0 (engine b045026). `UiModel` is state, published whole after every step, the newest
  winning: an optional page, the chat box, an optional hint and the toasts up now. `UiEvent` is one of OpenChat,
  Submit, PageShown, Close and Skip. Their JSON forms carry `"version": 1` at the top alone, the one field never left
  to its default. `ConvState`, NPC §6.1's seven states, lives in en-proto, since the box shows it.
- A page carries its kind: Speech (acknowledged and committed), Card or Failure (the "…" page; neither acknowledged).
  Its `last` is set only once the reply has ended with nothing left, so the surface knows when to fade out.
- `PageShown.t_ms` is on T6's clock, which since G15 is the overlay's too. `Skip` names the page shown when Tab went
  down, so a late skip never ends the next page. The hint's verbs are Speak, Answer and Next; no CallOut before v0.6.

### 400. The draft is the core's, and every Close hands it back

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§5.4, NPC§5.5, NPC§6.1

- NPC §6.1's `Conversation` holds the draft and §5.4 keeps it 60 s per target, so the core keeps it. The box takes the
  model's `draft` each time it opens, and every `Close` carries what the input held. When the core closes the box
  (danger, an ending) the overlay answers `Close{Shut, draft}`, so the draft comes back whichever side closed.
- The other reasons: Escape, FocusLost, InputLost (GraceUI's watchdog, the player's exit), Refused and NotNow
  (request_input's answers, `detail` naming the holder or the reason, for the toast), and Fault.

### 401. Toasts are a list with ids; their policy stays in the core

`2026-11-22` · DECIDED · `#engine` `#conversation` `#graceui` · refs: NPC§10.5

- The model lists the toasts up now, each with an id unique in the session. The core keeps NPC §10.5's policy (two
  visible, 6 s, a queue of 8, the rate limit, held in boss fights): one policy, pure, tested on a simulated clock. The
  surface posts each id once as a GraceUI notice and lets go of any the model no longer lists, as part C built it.

### 402. Speech is three factories of per-reply objects; only the lint's tally passes between replies

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§8.2, NPC§8.3, NPC§8.4, NPC§8.5, NPC§8.6

- `Sanitizer`, `Lint` and `Pager` are const factories held by reference; each opens one object per reply that holds
  that reply's state, and a cut-off or a regeneration opens new ones. Only `LintTally` (the opener used, the tics'
  counts, the told name's uses) goes from one reply's lint to the next one's context.
- The lint decides §8.4's action itself: Pass, Drop (a duplicate), Regenerate, Deflect or Stop. The pager owns the word
  caps (§8.5): `push()` answers Kept, Full (cancel the stream) or Over. Sealing is lazy: `next(now)` answers a page, a
  wait (page 1's 250 ms) or done.
- The sanitizer reports a removal by its reason and bytes, never the text removed (PV rules); the trailer's intent is
  the one text it keeps, for the speaker-only log.
- A context struct may gain a field whose default changes nothing; any other change to an interface was agreed between
  parts A and B.

### 403. The prompt takes the persona's record as JSON, not a struct both parts would edit

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§3.1, NPC§7.1, NPC§7.3, ARCH§2.3

- `DialogueContext.record` is the persona's record as the content pack holds it, JSON with its stage resolved, which
  en-prompt renders as template data. So parts A and B share NPC §3.1's file format, not a C++ struct both would edit.
  Block 9 is a typed `Scene`; history and recap are turns and strings; blocks 7 and 8's other sources join with the
  store (v0.3).
- `dialogue_request()` sets the slot, the purpose and the stop sequences. max_tokens and the temperature stay en-llm's
  slot budget (140, 0.8), so they are not set twice.

### 404. Stop sequences go to every flavour; a model that refuses them gets the stripped retry

`2026-11-22` · DECIDED · `#engine` `#conversation` `#llm` · refs: NPC§7.3, ARCH§11.2

- `ChatRequest.stop` goes out as `stop`, the first four that are not empty (OpenAI's limit), in a full body only. Every
  flavour en-llm knows takes the field on its chat completions (OpenAI's and OpenRouter's APIs, and the
  OpenAI-compatible endpoints of Ollama, llama.cpp, LM Studio and KoboldCpp), so none is left out.
- A model that refuses it (OpenAI documents its reasoning models so) answers 400 and gets the existing stripped retry,
  with no `stop`, and starts stripped for the rest of the session; the sanitizer's step 3 cuts the same run-on either
  way. `stop` is one of the client's own fields, so a provider's `extra_body` may not set it, as for `max_tokens`.

### 405. The core's new inputs were ticks until the conversation was wired, proved by twin cores

`2026-11-22` · METHOD · `#engine` `#conversation` · refs: ARCH§1.3

- `CoreInput` took `UiEvent`, `LlmEvent` and `NetHealth`, each handled at first as a tick, so v0.1 was unchanged. Twin
  cores stepped on the same session, one with ticks and one with each new kind in their place, gave the same state,
  changes, commands and events at every step.
- `Outputs` gained `ui`, `requests` and `cancels`; cancels came at once, since a cut-off, a regeneration and an ending
  cancel (NPC §6.3, §8.4) and part C's glue reads them. `NetHealth` is en-proto's, so recordings and scenarios can
  script it; its one flag is `dialogue`, and the slots of barks and memory join as their users do.

### 406. en-core's build takes folders and links en-prompt, so the parallel parts never edit it

`2026-11-22` · DECIDED · `#engine` `#conversation` `#build` · refs: ARCH§2.3

- en-core's sources and tests are globbed by folder, and en_core links en_prompt (ARCH §2.3 allows it) before the
  renderer needed it, so parts A and B added files without touching the build: speech and the prompt in `src/speech/`
  and `src/prompt/`, content, identity and the conversation in folders of their own, the tests in matching folders.

### 407. A nested variant's alternative may not have a default member initializer

`2026-11-22` · FACT · `#engine` `#conversation` `#build` · refs: en-proto's ui.hpp and lifecycle.hpp

- A `std::variant` of structs declared inside `UiEvent` is not default-constructible when its first alternative,
  OpenChat, has a default member initializer: GCC 13 deletes the variant's default constructor while the enclosing
  class is incomplete, as Lifecycle's DataReady showed before. OpenChat has none; value-initialised, its `via` is
  TalkKey (0).

### 408. en-proto's integer reader is shared, never copied

`2026-11-22` · METHOD · `#engine` `#conversation` `#build` · refs: en-proto's json_read.hpp

- The JSON reader for integer types other than int and uint64 moved from game_json.cpp into json_read.hpp, so
  ui_json.cpp uses the same template; a second copy in another file would have been an ODR clash.

### 409. The sanitizer gives a sentence out once it is final, reading the stream again from its start

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§8.2, NPC§8.6

- Part A (engine d179850). A sentence is given out once nothing later can change it: once the next has begun, or the
  stream ended or was cut. So a sentence mid-reply comes one delta after its boundary, the last at `finish()` or the
  cut. What lies between two sentences counts with the earlier, and what follows the last with the last, so a
  trailing stage direction ("Farewell. *waves*") is counted.
- Each feed analyses the raw text again from its start, in passes that each stop where more text could still change
  their answer. A reply is at most 4 KB, so a delta costs at most 4 KB of work; in exchange a stream split anywhere,
  even inside a character, gives what the whole stream gives, byte for byte, removals included (seeded property tests,
  `fuzz/sanitize`).
- A cut's removals count deterministically: nothing found after a cut counts, and only OverLimit depends on the delta
  that crossed 4 KB.

### 410. The sanitizer's general "Name:" pattern also strips real openings

`2026-11-22` · FACT · `#engine` `#conversation` · refs: NPC§8.2

- Step 5's pattern on a reply's first sentence, as NPC §8.2 writes it, also takes "But know this:" or "I know the
  Tarnished:" off its start. Built as the design says; the eval is to count NamePrefix removals, so that a narrower rule
  (the persona's own names, or two words at most) can be judged on data.

### 411. The sanitizer's spans, NFC within the whitelist's letters, and apostrophes

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§8.2

- Step 4: a single `*…*` or `_…_` on one line goes with what it holds (a stage direction); a run of two or more is a
  bold marker and goes alone, the words kept; a lone `*` goes, a lone `_` is left to the whitelist, and `_` opens or
  closes a span only at a word's edge (`snake_case` stays one word). `<tag>` and `<?…?>` close on their line within 200
  characters, `[…]` on its line, a link keeps its text, `(…)` holds at most 60. A span not yet closed holds back the
  text after it until its line ends.
- NFC composes only within Latin-1 and Latin Extended-A, the only letters the whitelist keeps; its tables are generated
  from the Unicode Character Database's decompositions and checked by the tests. The emoji ranges approximate
  Extended_Pictographic's blocks; a character outside them still goes by the whitelist.
- Typographic apostrophes become `'` before the whitelist, so "don’t" keeps its apostrophe. A single quote opening a
  sentence is a quotation mark unless it begins 'tis, 'twas, 'twere, 'twill, 'em or 'ere.

### 412. The safety list's fingerprint and normalisation (O107)

`2026-11-22` · DECIDED · `#engine` `#conversation` `#safety` · refs: O107, NPC§8.3

- A fingerprint is SHA-256 (en-core's own, from FIPS 180-4, checked on NIST's examples), in lower-case hex, of
  `eldennet.safety.v1:` and the normalised word or phrase of at most three words.
- The text is cut into chunks at white space and at anything but letters, digits, look-alikes (`@ $ ! |`) and
  separators; a chunk's edge separators and `!` go; letters lose accents and case; in a chunk with a letter, inner
  separators go and look-alikes become letters; three or more one-letter words in a row are one word; a repeated letter
  becomes one. A reply's words, and their runs of two and three, are looked up.
- The lint reads the sanitised sentence, what the player would read: a look-alike outside the whitelist (a Cyrillic
  letter) is already gone.
- `lexicon fingerprint` prints the fingerprints sorted and without repeats, so the order of the owner's list leaks
  nothing, and names a refused line by its number alone. `safety.toml` ships with no entries: filling it is the
  owner's step (S11.3.h).

### 413. banned.toml leaves out words with a sense of their own in the land

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§3.3, NPC§8.3

- NPC §3.3's table gives game, player, loot and cool as examples, and also says that common words with in-world senses
  (save, load, button) are deliberately absent. A hard hit costs a regeneration and then a deflection, so the list
  follows the second rule: game, player, loot, cool, console, mob, lag, screen, coop, password, battery and online are
  left out, and the game senses come in forms that mean nothing else (video game, game over, player character, gamer).
  quest, boss and buff stay, as the design keeps them. Assistant-speak is a hard category of its own (as an ai,
  language model, roleplay, feel free to).

### 414. The renderer builds its template data from the record, every name always present

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§3.1, NPC§7.1, NPC§7.3, WHY§403

- The record is NPC §3.1's fields as JSON, the active stage resolved with its inherited fields filled in, under
  `stage`; absent fields are unset. The renderer builds the template data itself, every name always present (null, ""
  or []), so the templates' names are fixed and linted when the pack loads, and a field of the wrong type refuses the
  render naming the field, never its value. Phrases (role, honesty, where, mood) lose a final full stop.
- Each block is rendered alone, to be measured against its cap and trimmed; a block that renders nothing is told from a
  failure by en-prompt's own message for an empty prompt. The breakpoints are NPC §7.1's three: cache A after block 1,
  one after block 5, cache B after 5b (left out with 5b when it is empty).
- The contract leaves out NPC §8.1's "one reserved trailer channel": the model would be told of a channel without its
  form, and `actions_block`, which will give the form, is empty in v1 (DESIGN §13). No input of the renderer carries
  the player's name; block 8's told-name line waits for memory (v0.3).

### 415. The pager: Over fills the reply too, and a short sentence waits for its partner

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§8.5, NPC§8.6

- A sentence that would cross the cap and 10 words, or 600 bytes, is left out and fills the reply: `Take::Over`, with
  no `Full` before it. The conversation cancels on `Full` or `Over`, whichever comes first.
- A sentence under 12 characters joins the next on its page: it waits for it, with no deadline, while the reply goes
  on; leaves a page's end for the next page when the next does not fit beside it; and is sealed alone once the reply
  has ended. Once the reply is full or has ended nothing waits, so page 1 waits its 250 ms only while more may come.
- The over-long split takes a mark only when a space follows it, so it never splits inside a token. `PageContext{0}`
  takes the schema's default of 40 words.

### 416. Part A's additions to the shared interfaces, each with a default that changes nothing

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: WHY§402, WHY§364

- `LintContext` gained `self_names` (for third-person self-reference; none: unchecked) and `tic_max` (each tic's
  `max_per_conversation`; missing: 1); `DialogueContext` gained `tally` (block 11's tic note) and `tied` and `land`
  (block 5b's lines, empty until the known set, S11.3.c). The real parts have headers of their own; nothing else of
  part 0's changed, and the stubs stayed.
- The prompt goldens live beside their fixtures (`tests/fixtures/prompt/golden/`, with a CHANGES.md of their own), so
  the route goldens' log in `tests/golden/` is left alone.

### 417. MSVC's std::map may throw on a move, so it is no Expected's value

`2026-11-22` · FACT · `#engine` `#conversation` `#build` · refs: en-proto's expected.hpp

- clang-cl refused `proto::Expected<std::map<std::string, std::string>, std::string>`: en-proto's Expected asserts that
  its value moves without throwing, and the MSVC standard library's `std::map` move constructor is not noexcept (it
  allocates a sentinel node). The GCC and Clang builds, on libstdc++, took it. A reader that hands back a map fills an
  out-parameter instead (en-tools' `read_pack_prompts`).

### 418. A test persona stays test_only; only the tests' realm, which a release DLL refuses, lets it resolve

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§2.1, DESIGN§11.4, WHY§364

- Part B (engine f6207dd). en-core's identity builds its index for a realm: `Game` (the DLL's) leaves out every
  `test_only` row, an override's included; `Eval` (the self-test, the eval, the tests) keeps them. The tests' overrides
  bind the test persona `lantern_keeper` to the fake game's invented row 81100 and keep `test_only`, since its stage's
  `beckon_src = "ci:speak_up"` is allowed only on a test persona (W13).
- Rejected: an override that clears `test_only`. It would bind in the game's realm, but it breaks W13 and makes "never
  resolves in game" rest on the files.
- Part C built the tests' switch: `ELDENNET_TEST_REALM=eval`, taken by a probe build alone (CI refuses EN_PROBE in a
  release build). The release DLL logs `realm asked=eval accepted=0` and keeps the game's realm, whatever a player's
  overrides hold. T1 reads the overrides folder (DESIGN §11.4) and T6 loads it.

### 419. Only a threat ends a conversation after 10 s of danger; a menu or a hidden overlay holds the pages

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§5.4, NPC§5.8, NPC§6.2

- Any failing clause of `safe_context` closes the box at once ("The moment has passed.", the draft kept), but only a
  threat counts towards the 10 s that end the conversation with `danger`: the Tarnished down or riding, a boss, combat,
  hostiles near, or a drop in HP. `Menu` (the HUD not at Default, a banner) and `NotActive` (the ladder below L3) do
  not: a menu or a hidden overlay holds the pages (§5.8's 30 s), and a model that is not ready leaves the conversation
  to its idle timer.
- Why: with the HUD hidden, `Menu` fails too, so counting it would end every hidden hold at 10 s and §6.2's 30 s could
  never be reached. `HudState::Unknown` reads as hidden (fail closed).

### 420. The conversation's targets, the observer and the talk key, where the design is silent

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§5.1, NPC§5.2, NPC§5.3, NPC§5.8

- Targets by NPC §5.2's rules 3 to 5 only (lock-on, cone, nearest): rule 0 needs own-initiative lines, 1 and 2 S3's
  observer of the game's own talk, and 6 the call-out (v0.6). From no target the hint shows at once; from one target
  to another it waits for the 500 ms lead; to none it goes at once.
- The observer's "3 s since PersonaApproached" is 3 s since the persona last came within 8 m: no rule of Augur's emits
  PersonaApproached yet.
- `Muted` never fires in v0.2, which reads no `speech.toml`, `types.toml` or name ids. A stage's `mute` toasts "They
  have nothing to say to you." (2 s), as an allowance turned off does; dead, hostile, struck, busy, native talk and
  unavailable are silent.
- The talk key with the box hidden reopens it on the partner with the draft, once safe; on another conversable persona
  it retargets (ending after the current page, then opening on that one). With the box open the key types.

### 421. The conversation's failures, holds and closes, where the design is silent

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§5.7, NPC§6.1, NPC§6.3, NPC§8.7, DESIGN§2.7

- A failed request or render shows the "…" page (kind Failure, its own id for each failure) for 2.4 s, never
  acknowledged, with the toast of its class: DESIGN §2.7's words for Auth and RateLimited, "No answer: provider timed
  out" for the timeouts, and "No answer: the provider failed" (ours) for the rest. A queued line goes at once after a
  failure; the new reply's first page waits for the "…" page to go.
- When a page's hold ends and the next is not sealed yet, the page stays up until it is. A page up but not yet
  acknowledged when a line cuts its reply off waits 500 ms for its ack (then stays 2 s from it, or goes); one up but
  unacknowledged at an end goes at once to the 500 ms grace.
- `Close{Refused}` or `{NotNow}` before any line ends the conversation quietly (`closed`); `FocusLost` and `InputLost`
  close the box and leave the conversation pacing, to its idle timer; `Fault` ends it with a toast. The timers paused
  while Held move on by the time held, the 20 s total too.
- Block 9 has only the time of day for now, in our own words from the snapshot's clock ("at dawn" 5–8, "by day" 8–17,
  "at dusk" 17–20, "at night"), its bands unchecked against the game (where the game keeps its clock: WHY§394).

### 422. The conversation's request ids, and the ladder's L3 and L4

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: ARCH§6.8

- The conversation's request ids start at `1 << 48` (`kConversationRequests`); barks and memory take the next ranges.
- `core::Level` gains L3 and L4 after L2 (the values only grow; Augur's ladder had them already). `ai_allowed` is the
  gate open at L2 or above; speech needs L3. clang-cl's warnings as errors then refused the DLL's switch from the
  core's level to the pages', so L3 and L4 show there as `Observing` for now (WHY§442).

### 423. The record part A renders, the pack's other files, and a load during a conversation

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: NPC§3.1, NPC§3.2, DESIGN§11.4

- `Persona::resolved(stage)` is `DialogueContext::record`: the file as JSON with `stages` replaced by `stage`, the
  active one, its `road`, `at`, `hears` and `knows` inherited, and nothing that is never rendered (a whitelist since the
  joining, WHY§430). `ContentPack::others` carries the pack's files outside `identity/` and `personas/`, overrides
  applied, for the templates and lexicons. The shipped index and records are built into en-core (`builtin_content()`),
  as Augur's rule files are, so the DLL's core reads no file for them.
- A `when` that v0.1's predicates refuse is Unknown, a syntax error included: the content lint is to tell the two
  apart.
- v0.2 loads content once. A new load that changes the index or the records while a conversation is open ends it
  (`fault`, no toast), its request cancelled and what was up dropped, and the targets are worked out again: nothing
  read from the old content outlives it (WHY§427). Hot reload (DESIGN §11.4) is to keep the conversation.

### 424. The fixture flag states, written cumulatively, and the truth tables

`2026-11-22` · DECIDED · `#engine` `#conversation` `#knowledge` · refs: MEM§7.8, MEM§12.4, MEM§12.5, MEM§12.11, WHY§364

- The design names the eight states and gives the contents of two of them (`after_radahn`, `after_rykard`) and of
  `ashen`. The other five are written cumulatively in story order from their names, `fresh` holding none and each
  later one adding its bosses, the star and, last, the burned tree; the great runes (`runes.geN`) follow the bosses.
- A file holds catalogue aliases only (`[flags]`, each true and verified, an absent alias false), persona blocks as
  offsets set and cleared (`[blocks]`, the design's +N) and track steps (`[tracks]`): no flag id, no text.
- The truth tables cover v0.2's personas, Kalé (the pilot) and the test persona, at the eight states (`ashen` is block
  5b's alone), and are gated until `known(P)` lands (S11.3.c). Tanith's track steps read "none" until S6.

### 425. mockllm is a scripted model on the caller's clock; its SSE server is not built

`2026-11-22` · DECIDED · `#engine` `#conversation` `#eval` · refs: ARCH§12.3, DESIGN§14, WHY§364

- `lib/mockllm` answers the core's requests as en-llm's events would come, on the caller's clock: chunks, delays, a
  think leak and its Reset, a trailer, en-llm's error classes once its retries and fallback are spent, cancels.
  `eldennet-engine mockllm --script` prints a script's events in the recorded form that `--llm recorded` replays.
- ARCH §12.3's local SSE server, for en-llm's live path, is left: the eval and the tests need the scripted model alone.
- The eval's local-only report (WHY§364 (10)) is `kind = "eval-local"`, with no default path; `eval-reports/` is
  ignored by git, and what `eval` prints is numbers only (a failed `last_npc` names its line, never the text).

### 426. CTest's CONFIGURATIONS keeps a test out of the default run

`2026-11-22` · FACT · `#engine` `#conversation` `#build` `#eval` · refs: ARCH§12.1

- A test added with `CONFIGURATIONS eval` is left out of a plain `ctest` (and of `ctest -N`) with Ninja's single
  configuration, and runs with `ctest -C eval -L eval`, as checked on a project of its own. The eval's tests use it,
  labelled `eval`.

### 427. The core's lint and prompt stand for whichever content is loaded

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: ARCH§2.3, DESIGN§11.4, WHY§364, WHY§388

- Parts A and B joined (engine c29ea5d). The conversation holds its speech parts by reference for the core's life, but
  the lint and the prompt depend on the pack's lexicon and templates. So `BuiltinSpeech` holds two forwarders, swapped
  at each successful load: a reply already open keeps the lexicon it opened with, and a render reads the newest
  templates. Before any load no persona exists; were either reached then, it fails closed (every render refused, every
  sentence stopped).
- The pack is checked whole (prompts, lexicon, block 2) whatever speech parts the core was given, so a test on the
  stubs still refuses a broken pack. The shipped prompts and lexicon are built in at configure time with the persona
  files, so the DLL and the Windows tests read no path for them.
- The pack's hash still covers the index and the records alone: a change to the prompts or the lexicon does not end a
  conversation, which takes the new templates at its next render (hot reload's behaviour, DESIGN §11.4).

### 428. Block 2's content lint: who is held to the cap

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: WHY§388, NPC§7.3

- WHY§388's "shipped and test content" is read as a record from the shipped pack or any `test_only` persona, wherever
  its file came from; "the player's own files" as a record from the overrides folder whose row is not `test_only`. Each
  enabled persona's block 2 is rendered alone at each of its stages, under its fallback name. Past 120 tokens it is an
  error naming the file, the stage's line and the estimate, never a value, or a warning kept in
  `Core::content_warnings()`.
- The prompt goldens' fixtures are test content: `golden check` and `update` refuse a fixture whose block 2 passes its
  cap, and write nothing. All four fixtures and the test persona come to 102 tokens or less.

### 429. Block 11's one tic note is the newest by the tally, and its worst case passes 70

`2026-11-22` · DECIDED · `#engine` `#conversation` · refs: WHY§388, NPC§3.3, NPC§7.3

- "The newest" needs an order the counts alone do not give, so `LintTally` gained `spent`: the tic whose count reached
  its `max_per_conversation` most recently (a tic already past its cap does not take it again), carried between
  replies like the counts. A tally without one (a fixture's) notes the last at its cap in the file's order.
- With "in this talk" dropped from the note, the ask, one note and the promise reminder come to 67 of 70 tokens for
  the fixture's 18-byte name. The worst case (a 32-byte name, a 12-word tic, the reminder) is about 85: block 11 is
  then reported past its cap, never refused or trimmed, as the design has it for every block but 6 and 9.

### 430. The resolved record is a whitelist of the fields the renderer reads

`2026-11-22` · DECIDED · `#engine` `#conversation` `#spoilers` · refs: NPC§3.1, NPC§3.2, O36, O86, WHY§423

- `Persona::resolved()` copies only the 24 fields the renderer reads (`kRecordFields`, prompt.hpp), so nothing that is
  never rendered (`forbid`, the gate fields, the seed, the deflections, the id, knowledge, awareness) can reach the
  renderer through a field added later. The stage's `at`, `hears` and `knows` are still inherited, but left out until
  block 7 reads them (S11.3.c), when they join the list.
- A test proves that the renderer reads every listed field (a wrong type in each refuses the render) and that the
  record holds nothing else.

### 431. The real speech changed nothing in part B's scenarios

`2026-11-22` · FACT · `#engine` `#conversation` `#eval` · refs: NPC§8.2, NPC§8.6

- Part B's scenarios and their `last_npc` expectations held unchanged with part A's sanitizer, lint, pager and prompt,
  mock and recorded: the openers are the persona's own, the tic's first use is within its cap, and a short "Hm." joins
  the next sentence on its page, which the commit joins back as before. No speech piece needed a fix. Two scenarios
  joined them: a regeneration, then a deflection; and the word cap cutting a stream.
- A deflection cancels the regeneration's stream as well, so a line that regenerates and then deflects costs two
  cancels. The conversation cancels on `Take::Over` as on `Full`, and deflects through `DeflectionRing`, one per persona
  for the session (the store keeps its order per journey from v0.3), rebuilt if a reload changes the persona's count of
  deflections.

### 432. A one-page reply sealed while it streams is never last, so a queued line waits for its hold

`2026-11-22` · LIKELY · `#engine` `#conversation` · refs: NPC§6.3, NPC§8.6

- NPC §6.3 sends a queued line when the reply's last page starts showing, so that generation overlaps reading. With
  lazy sealing, `Page::last` is set only on a page sealed after the reply ended or filled. A one-page reply whose page
  was sealed while it still streamed is followed by `Done`, so its queued line goes when that page's hold ends, up to
  6.5 s later than §6.3 intends.
- Replies usually end long before page 1's hold does, so later pages are mostly sealed after the end. A fix would let
  the conversation send the queued line when the stream ends with the last page up (S11.3.b).
- *Later (2026-11-23, WHY§494):* closed: a queued line goes once the stream ends with the last page up.

### 433. The DLL links en-prompt for real: the folder code split off, one runtime import reviewed

`2026-11-22` · FACT · `#engine` `#conversation` `#build` `#dll` · refs: ARCH§2.3, ARCH§12.1, src/eldennet/tests/imports.allow

- Until the core rendered the dialogue prompt, eldennet.dll linked nothing of en-prompt. Then it linked en-prompt's
  environment and MemorySource and, in the same objects, FolderSource and the Environment over a folder: MSVC's
  std::filesystem, sixteen kernel32 imports that imports.allow does not list (clang-cl's `eldennet_dll_checks`). Those
  two moved unchanged to en-prompt's own `folder.cpp`, which only the tools and the tests reach, and fifteen imports
  went.
- One stays, `SetEndOfFile`: inja's parser holds an `std::ifstream` for its file includes (which en-prompt turns off,
  but the code is compiled), whose `std::_Fiopen` brings the UCRT's `open.obj` and, through `_chsize_nolock`,
  `chsize.obj`. No code of ours opens a file through it. It was reviewed into the DLL's imports.allow (engine
  22f3f9c), as the runtime's other unused imports were.

### 434. The surfaces draw GraceUI's glyphs as quads of their own; Dear ImGui keeps only the box's line

`2026-11-22` · DECIDED · `#engine` `#conversation` `#graceui` · refs: NPC§10.1, GraceUI:docs/eldennet-changes.md §31 and §33, WHY§364

- Part C (engine b4451c8). Grace offered an ImFont over glyph_table, or quads emitted by hand (WHY§364's Later line).
  Dear ImGui 1.93 bakes through an ImFontLoader into its own atlas, so a font whose glyphs live in GraceUI's atlas
  would need a loader that bakes nothing and a texture it does not own. So en-overlay emits each glyph's quad itself
  (surfaces.hpp's Painter, pure, in GraceUI's draw-list form) on the shared atlas's id, scaled from the size GraceUI
  baked to the surface's size.
- The box's own headless Dear ImGui context keeps the input line alone (its text, caret and selection, with NPC §8.5's
  limits through a CallbackEdit), fed one holder event at a time with a frame after each, so a fast typist's keys are
  taken in order at T6's pace. It is never drawn and its atlas never registered: the line is drawn from GraceUI's
  glyphs at the editor's caret, scrolled so the caret shows.
- The pointer is not fed in v0.2: a click would take the field's focus in the editor's coordinates, which are not the
  box's. Activated by focus, Dear ImGui selects the whole line, so the editor moves the caret to the end: a kept draft
  is typed on, never replaced by the first key.

### 435. EldenNet's faces are GraceUI's own font files, registered as two scaled faces

`2026-11-22` · DECIDED · `#engine` `#conversation` `#graceui` `#fonts` · refs: NPC§10.1, GraceUI:docs/eldennet-changes.md §33, GraceUI:D139

- "GraceUI's faces only" and NPC §10.1's EB Garamond and Noto Sans meet in GraceUI 0.2.20's folder, which ships both.
  T1 reads `EBGaramond-Regular.ttf` and `NotoSans-Regular.ttf` from the folder graceui.dll was loaded from, and at
  connect registers them as EldenNet's two GRACEUI_FACE_SCALED faces (the serif at 30 px, the sans at 18; smaller sizes
  are drawn scaled down from them). No font file of ours. After GraceUI's warm-up they answer too_late, and the
  surfaces draw GraceUI's notices' face at their sizes.
- GraceUI allows a mod two faces for now (an estimate; D139 gives EldenNet eight), so the page's name line, the history
  and the input are drawn from the 30 px bake scaled down until GraceUI raises it. No italic face exists, so the
  Tarnished's lines in the history are coloured only.

### 436. Panels are made for the display, lazily, and each is submitted at most every 100 ms

`2026-11-22` · DECIDED · `#engine` `#conversation` `#graceui` · refs: NPC§10.1, NPC§10.2, WHY§364

- GraceUI's panels cannot move or grow, and the display is usually unknown at connect. The three panels (the lower
  third, the box, the hint) are made at the first display event, and a new set for a new display while the mod's
  sixteen last, never while the box holds input.
- Each panel's list is submitted at most every 100 ms; the box's first frame on opening goes at once. A page's first
  submitted frame is never transparent (its alpha is the fade's first step), and its time is PageShown's (WHY§364 (3)).
- The card's "…" pulses by stepped alpha. Rounded corners (NPC §10.1's 4 px) are not drawn: the boxes are plain
  rectangles.

### 437. The box's closes, as the surfaces answer GraceUI's holder ends

`2026-11-22` · DECIDED · `#engine` `#conversation` `#input` · refs: NPC§5.1, NPC§5.5, NPC§5.7, GraceUI:D176, WHY§400

- GraceUI's holder ends map to Close: focus lost to FocusLost; a fault to Fault; the menu key, danger, loading, no
  render and the player's exit to InputLost; `released` (the box's own release, or its panel hidden) to nothing. Esc
  and the pad's B are Escape; the model closing the box while it holds input is Shut; each carries the draft.
- request_input refused is Close{Refused}, its `detail` the holder's mod id; not now is Close{NotNow} with not_ready,
  danger, loading or game_menu; a box the model opens before warm-up or before a display is NotNow, not_ready. Input is
  asked for once each time the model opens the box.
- A second Esc while the reply paces (NPC §5.7) cannot reach EldenNet: with the box closed nothing holds input, and
  GraceUI takes no hotkey on Escape. The core's rule waits for a way in (S11.4.d).
- The talk hotkey cannot say whether its key or its chord fired it: OpenChat says TalkKey either way, and the swallow
  is armed with the bound key's character (describe_hotkey), harmless after a chord.

### 438. Tab is masked from the game whenever it is bound

`2026-11-22` · LIKELY · `#engine` `#conversation` `#hotkeys` · refs: NPC§5.1, NPC§8.6, S11.1.a

- NPC §5.1 masks "Tab while a page shows", but GraceUI's hotkeys have no unregister or pause: registered, Tab is never
  the game's while it is bound. S4 is to check that the game does not need Tab; if it does, the skip hotkey takes
  another default.

### 439. T6 waits on GraceUI's handle, and the talk key's latency is measured from its SendInput

`2026-11-22` · METHOD · `#engine` `#conversation` `#graceui` · refs: NPC§5.1, GraceUI:D212, WHY§373

- T6 waits on its own wake signal and GraceUI's wait handle together (WaitForMultipleObjects, reviewed into the DLL's
  imports), with 100 ms the fallback. Within one wake: poll, the player's events to the core, the model drawn, what that
  raised (PageShown, Close) to the core, and drawn again, a few rounds at most.
- The harness's fake game logs the QPC time of the talk key's SendInput, and the DLL the box's first submit on the same
  clock. The fake game now pumps its messages while a key is held, as a game's loop does, instead of sleeping 60 ms
  first.

### 440. No chat text in the DLL's log

`2026-11-22` · METHOD · `#engine` `#conversation` `#privacy` · refs: PAGES§4.9

- The DLL logs the conversation by kinds and numbers alone: `chat what=open_key|opened|submit|page_shown|close|skip`
  (with `chars=` and `reason=`), `commit kind= conv= reply= page= chars=`, `llm what=submit result= id=`. The harness
  checks that neither the typed line nor the reply's words reach the log.

### 441. From the talk key to the box's first frame: 1 ms on the fake game

`2026-11-22` · FACT · `#engine` `#conversation` `#fakegame` · refs: NPC§5.1, GraceUI:D212, WHY§373

- In `launcher-eldennet-graceui-talk` (clang-cl, Wine 9.0, GraceUI 0.2.20's test build), the box's first list was
  submitted 1 ms after the talk key's SendInput, and T6's own part (the hotkey's poll to the submit) took under 1 ms:
  GraceUI's handle wakes T6 at once, and the core's step and the box's first frame are cheap. NPC §5.1's 50 ms holds
  with room; in the game the present thread's next frame adds up to one frame before the box is seen.
- Anti-aliased text never matches its ink exactly: EB Garamond at 15 px (the content scale 0.5 of a 960 by 540 frame)
  has no pixel within 3 of its colour, so the fake game's probe takes an optional tolerance (72 here) for text.

### 442. What part C left to the core: the refusal's toast and the pages' speaking level

`2026-11-22` · LIKELY · `#engine` `#conversation` · refs: NPC§5.1, NPC§5.5

- The core toasts a refused or not-now box in fixed words ("Chat cannot take the keyboard just now."). The Close's
  `detail`, the holder's mod id or the reason, is passed but not named in the toast, as part C's brief asked
  (S11.3.a).
- The pages still show L3 and L4 as Observing, since en-overlay's Level has no speaking level: words players read, left
  for review (S11.4.a).
- Part C's third point, that the prompts and lexicons must reach the DLL's core, the joining had already met: they are
  built in with the persona files (WHY§427).
- *Later (2026-11-23, WHY§495, §496):* the toast names the holder by GraceUI's id, and the pages show L3 and L4 as
  speaking levels; their words go to the owner.

### 443. The content's problems reach the player as one notice, never a value

`2026-11-22` · DECIDED · `#engine` `#conversation` `#notices` · refs: NPC§4.7, DESIGN§11.4

- The DLL reads the overrides folder for `.toml`, `.md` and `.j2`, since prompts joined the pack at the joining. A
  content load's first error, else its first warning (`content_warnings()`), is shown once for 15 s as a GraceUI notice
  under EldenNet's key 5, in en-core's fixed words (`file:line: key: problem`) and "(and N more)", never a value of the
  file. The log holds the counts alone. The words are drafts.

### 444. The tester's panel is a window of the launcher's own, not GraceUI's overlay, since a read-only round hooks nothing

`2026-11-22` · DECIDED · `#lodestar` `#launcher` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§398, S10.1.l

- It is always on top and never takes the keyboard. GraceUI's overlay hooks have not had their spikes on the owner's PC
  (S10.1.g, S11.1.a), and Grace advises keeping read-only rounds on the launcher's window even after.
- The list is data in one place, and the confirm is one named action, so the panel can move to a GraceUI panel later.
  Built in the eighth round (engine 3030114, package 0.1.0-lodestar.11): eight tasks in order, each with its
  description and timer; the done ones greyed with the time they took; the one under way highlighted. A waiting task
  ends by itself; a tester's task ends at Pause (the launcher's system hotkey, as the Mark always was) or a click.

### 445. The tester's confirmations are Marks in the watch block's ring, with no new field

`2026-11-22` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§396, WHY§444

- The launcher stamps a Mark at a marked task's confirmation, and the round reads it with `block::marks()`, as the
  shell does. en-proto is unchanged.
- The sweep keeps the flips seen from 15 s before a mark to 60 s after it under that mark, and starts a pass at once.
  It gives up a target of which more than 1,024 words change again and again. Its sizes are unchanged (about 24 MiB at
  most), within the owner's yes (O105's note).
- The same round writes the clock's static into the draft table as `world_clock`, by what its object holds (minutes
  going round twice with hours beside them). The play time's invariant now asks what a play time does (WHY§397).

### 446. The first conversation verified whole on four toolchains, the talk case with the real speech under Wine

`2026-11-23` · FACT · `#engine` `#conversation` · refs: WHY§364, S11.3, S11.4.c

- The whole first conversation (engine b4451c8: parts 0, A and B joined, and C) builds with 0 warnings on GCC
  (Release), Clang, MinGW-w64 and clang-cl. The merging broke nothing.
  - GCC and Clang: ctest 34/34, the eval label 7/7.
  - MinGW: the 9 unit programs pass under Wine, harness 60/60.
  - clang-cl: the 9 unit programs pass, `eldennet_dll_checks` passes (177 imports, SetEndOfFile and
    WaitForMultipleObjects as reviewed), harness 119/119 with GraceUI's 0.2.20 test build.
- The talk case on clang-cl with the real speech:
  - The test persona was bound through the overrides folder in the Eval realm, and a 12-character line was sent.
  - The scripted reply passed the sanitizer and the lint with no regeneration. It was shown as one page of 67
    characters in the lower third and committed on display. Its first frame came about 226 ms after Enter.
  - The talk key reached the box's first submitted frame in 1 ms, against NPC §5.1's 50 ms.
  - The input-leak case passed. The release DLL refused the Eval realm (`accepted=0`), and the log held no chat text.
- O108's change (engine a7f8509, after this check) passed GCC's 34/34 and 7/7.

### 447. world_clock is in the build's table by the invariant recipe, an eighth session

`2026-11-23` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§394, WHY§359

- The eighth read-only round (package 0.1.0-lodestar.11, the owner's run, 10.7 minutes) named it by what its object
  holds. It is static 64422792, the word 8 bytes in: the minute from bit 39, the hour from bit 34. The minutes stepped 141
  times and went round four times, the hours stepped four times, and nothing else changed. 113 instructions refer to the
  static.
- The build's table has 15 entries (engine a6cdf26). save_data was named again by the mended rule, rising in 12 passes of
  12. The reader: 6,190 readings in 620 s, 9.98 a second, no fault. Ten managers were where they were, an eighth
  session.
- A second static's object (64422928) keeps the same words 216 and 232 bytes in, in other memory.

### 448. Bits are set in the flags' stretch where the owner does something the game remembers

`2026-11-23` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§395, WHY§396

- Behind the store's address 40 bytes in, all set around the owner's two marks. Around the first: byte 3,288 (bit 1)
  and byte 1,453,189 (bit 0). Around the second: byte 2,928 (bit 0), byte 1,453,199 (bit 5) and byte 1,453,751 (bit 6).
  Nothing else flipped there around the marks.
- The report was at the writer's limit again, since stretches the sweep had given up still listed their flips; mended in
  the ninth round.
- *Later, 2026-11-23 (the ninth round, WHY§451):* the flips decode, by the flags' tree, to round flag numbers.

### 449. The report may carry the numbers of a flags' tree record joined to a bit the owner set

`2026-11-23` · DECIDED · `#lodestar` `#probe` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§390, WHY§448

- Without them a block cannot be given its group. The ninth round (engine 99b3de1, package 0.1.0-lodestar.12) reads
  the tree beside the flags once: the object 56 bytes into the store, records of 64 bytes reached by their first three
  words, at most 32,768, one record a read within the searches' share. It counts each of a record's sixteen 32-bit
  fields over all records, finds the key by the tree's order, and joins each flip around a mark to the record that names
  its block.
- A joined record is written as its sixteen numbers, the group's number among them, an address written as 0, for at most
  48 records. This is the second place the report carries numbers the memory held, after the clock's jumps (WHY§390).
  Engine agrees, on that bound.

### 450. The tester's panel ran on the owner's PC over the game, with Pause as its key

`2026-11-23` · FACT · `#launcher` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§444, S10.1.l

- The game ran windowed. Seven tasks were done in order, the tester's three by Pause, and both Marks were taken by the
  round.
- The owner asked for one change: "make the blue button with instructions be aware of the task, because for example it
  tells the user to press pause or click here, when the task is about to quit the game." In the ninth round the button
  speaks for its task, and the last task has none (engine 99b3de1).

### 451. The flags' layout: the store's holder and tree, a record's key, mode and block index, a flag as group times 1,000 plus its place

`2026-11-23` · FACT · `#lodestar` `#l6` `#flags` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§395, WHY§448, WHY§449, ARCH§6.9

- The ninth read-only round (package 0.1.0-lodestar.12, the owner's run, 12.2 minutes), one session. The store (static
  64419032) keeps the holder 40 bytes in and the tree's head 56 bytes in. The tree was read whole: 11,930 records of 64
  bytes, each of the tree's shape, none unreadable.
- A record holds three links, then the key 32 bytes in (32 bits, different in every record: the group; four bytes of
  padding after it), the mode 40 bytes in (1 in all of them), and the block's index 48 bytes in (different in every
  record, from 1 to 14,297).
- A block is 125 bytes at the holder plus its index times 125. A flag is the group times 1,000 plus its place in the
  block, the first flag of a byte in its top bit. The nineteen flips at the owner's marks decode to round numbers, as flag
  numbers are; the other bit order does not give them.
- The clock and the play time were again found by their invariants. The reader: 7,157 readings at 9.92 a second, no
  fault. Ten managers were where they were, a ninth session. The panel's changed button ran, and all eight tasks were
  done.
- *Later, 2026-11-23 (the tenth round, WHY§455):* the layout holds in a second session, and the fields are in the
  build's layout as leads.

### 452. Our reader's lead was wrong in one thing: a record's value is a block's index, not a byte offset

`2026-11-23` · FACT · `#lodestar` `#flags` · refs: docs/ops/probes/2026-11-22-l5.md, ARCH§6.9, WHY§451

- Only 94 of the 11,928 values are multiples of 125, and the flags decode only with index times 125. So en-gal's
  `ErBackend::flag_block` must return holder + value × 125 for mode 1, where ARCH §6.9 had holder + value. Mode 2 (a
  block by its address) was not seen.
- Engine agreed on 2026-11-23 that Lodestar change it in en-gal (readers.cpp), with the fake game's records, en-gal's
  fake world, the tests and the harness. ARCH §6.9 carries a dated note.

### 453. A join must follow the layout the counts show: the index, not the start

`2026-11-23` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§449

- The ninth round's join took a block's start before its index, so the five flips in the stretch's first blocks were
  joined to other blocks' records, and their groups are not in the report. A start met in the index's field belongs to
  another block's record.
- Also mended: one key in seven, with its padding, looked like an address, so the order's check passed it over.

### 454. en-gal's flag reader takes a mode-1 record's value as the block's index

`2026-11-23` · DECIDED · `#lodestar` `#flags` · refs: readers.cpp, WHY§452, ARCH§6.9

- `ErBackend::flag_block` returns the holder plus value × 125 for mode 1, bounded (engine 6d56207, with Engine's word).
  The fake game and en-gal's fake world keep indices too. This rests on one session's finding.
- The tenth round (package 0.1.0-lodestar.13) tries the reader's way in the game: it joins each flip by the block's index
  first, takes the key by the tree's order, goes down from the head's middle link by the key to each joined record, and
  writes whether that way arrives at the same record. It also reads the store's first 256 bytes once and writes only
  where 1,000 and 125 stand. Its other reading is the ninth round's.

### 455. The flags' layout holds in a second session, and the reader's way down the tree arrives at every joined flip

`2026-11-23` · FACT · `#lodestar` `#l6` `#flags` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§451, WHY§454

- The tenth read-only round (package 0.1.0-lodestar.13, the owner's run, 8.1 minutes). The tree was read whole again:
  11,930 records, the same fields. It is ordered by the key 32 bytes in, the first link the smaller side. For every
  flip joined to a record (13 in the flags' stretch, 48 in all), the way from the head's middle link down by the key
  arrived at that same record, after 5 to 16 records.
- The store keeps 1,000 as a 32-bit number 28 bytes in (the divisor our reader checks) and 125 at 32 bytes in. The
  flags read by number come from other blocks than the ninth round's, a second session.
- The fields are in the build's layout as leads (engine e864140): `[class.event_flags]` (divisor 0x1C, map 0x38,
  holder_base 0x28) and `[class.flag_node]` (left 0x0, parent 0x8, right 0x10, key 0x20, mode 0x28, value 0x30).
- To mend: one join of the thirteen took the wrong record (index 1 met the number 1 in another field first), and the
  report was at its limit a fourth time.

### 456. Step L6's three subjects are placed after ten read-only rounds, with no fault in some 80,000 readings

`2026-11-23` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§447, WHY§451, WHY§455, proposal 0005 §6.1

- The time of day (`world_clock`) and the play time (`save_data`) by the invariant recipe, and the event flags by the
  store's and the record's fields, all in the build's layout as leads. Ten managers have been where they were in ten
  sessions. The reader on EldenNet's own thread has run at about 10 readings a second, with no fault in some 80,000
  readings.

### 457. EldenNet's own flag reader runs first inside a read-only round, beside the round's tree reader, before the start uses it

`2026-11-23` · DECIDED · `#engine` `#lodestar` `#flags` · refs: WHY§456, S10.1.k, S10.1.b, ARCH§6.9

- Lodestar asked whether EldenNet's own reader (`ErBackend`) should next read the flags through the build's layout at
  the start, on feat/engine, or first in the probe build. Engine's ruling, as Lodestar recommended: in the probe
  build first. A round runs ErBackend's flags group on T2 and compares it with the round's own tree reader, with the
  owner's panel and the self-test flag. It touches nothing of boot.cpp.
- So the real reader's first run stays inside a read-only round. The start's use of the layout (discovery through the
  table, the reader at start: spikes S0-A and S0-B, S10.1.b) follows once that round agrees.
- *Later (2026-11-23, WHY§462):* the round ran. EldenNet's own reader read the flags through the build's layout
  on the game, 44 frames with no fault, and the flag set in every playthrough as set; the marked flags' comparison
  moves to the twelfth round.
- *Later (2026-11-23, WHY§468):* the comparison is done: the backend read the four flags the owner changed
  and the always-set one as set, five of five.

### 458. One group of the reader can be read alone before the others' fields are placed: a partial discovery and a tick for that group

`2026-11-23` · METHOD · `#lodestar` `#flags` · refs: backend.hpp, discovery.hpp, WHY§457

- The game's layout places no `game_state` and no `field_area` yet, and both are required statics, so a discovery pass
  with the files as they stand is refused. With `DiscoveryOptions::partial` nothing is required of the patterns: a
  missing static is simply missing, the Discovery is not ready, and `has(group)` says what can be read. It is never for
  a start, and boot.cpp does not set it.
- `snapshot()` cannot reach the flags without the lifecycle's and the session's fields, so `ErBackend::flags_alone` is a
  tick of its own. It begins the tick, re-reads the statics, runs `read_flags` as it stands (the divisor's check
  included), then reads the known flag as phase B does. The result is returned, not written into the book. Nothing is
  read where the group is tripped.
- The eleventh round (engine 998d078, package 0.1.0-lodestar.14) uses both. T1 runs discover once through the build's
  files on EldenNet's own thread (no getter call), and T2 reads the flags group alone every ten seconds: flag 100 first,
  then each flag a marked flip was joined to. Each read is compared with what the sweep saw. This is the first time the
  table's checks (placement, witness) run on the game's own image through discover.
- Both additions are additive; nothing that exists changes its behaviour.

### 459. What a ready start still needs from the layout: one static and 36 fields, by L6's ways

`2026-11-23` · FACT · `#lodestar` `#gal` · refs: roles.cpp, gal/layouts/2.7.2.0-ww.toml, WHY§456, S10.1.m

- Lodestar's count against en-gal's own lists (16 items, 97 fields in 35 classes) and the build's layout (15 items; 2
  classes, 9 fields). A ready start, not partial, still needs:
  - `game_state`, the one required static still unplaced, with its field `save_slot`;
  - Session's 6 fields;
  - World's 19 fields that are not optional;
  - Player's 10 such fields beyond the play time;
  - for the snapshot's mode, `menus.screen_state` (with `loading.load_counter`, optional).
  That is 36 fields and one static.
- The ways, by L6: an invariant with the owner's panel tasks (hp, position, runes, death count, the HUD and menus, the
  camera, the place); a search for pointers and lists with the classes the image already names (the main player, the
  near list, the parts, vitals and motion, player_data); and leads from our notes, checked by their plain properties,
  for constants (the session's roles and lobby state, a character's ids, the archetype and the like).
- `game_state` is the hardest: its one field the start reads never changes in a session, so the static must be told
  by the code's references or by a second field. Optional groups (place, flags, ui, combat, camera, text, params) only
  turn themselves off.
- The clock's hour and minute are runs of bits in one 64-bit word, which the layout's field types cannot yet say.

### 460. The order of the start's layout, and how far a start may rest on the notes' leads for constants

`2026-11-23` · DECIDED · `#engine` `#lodestar` `#gal` · refs: WHY§459, WHY§457, DESIGN§3, DESIGN§4, ARCH§6.7, ARCH§6.8, S10.1.b, S10.1.m

- Engine's ruling on Lodestar's list. The work comes in this order:
  1. `game_state` first, in a round of its own, since nothing is ready without it.
  2. The pointers and lists by search and class: `characters.main_player`, the player set and its slot, the near list,
     the parts, vitals and motion, and `player_data`.
  3. The moving numbers by invariant, with tasks on the owner's panel: hp and max hp, position, the death count, runes,
     `menus.screen_state`, `ui.hud_state` and `ui.banner_id` (the conversation's gate reads the last two), and dead and
     on screen. A task that costs the owner something (dying drops runes; spending runes; levelling up) is optional and
     says what it costs.
  4. The constants as leads from our notes, checked by their plain properties.
- How far a start may rest on such leads:
  - The session gate's fields (`multiplay_role`, `is_my_world`, `lobby_state`) may rest on leads checked for their
    offline values. The launcher's offline start without Easy Anti-Cheat is the first guard, and any other value
    makes EldenNet dormant: the gate fails closed.
  - A character's ids may rest on leads checked as constant per character and different between characters. A wrong
    `npc_param_id` cannot bind a persona to the wrong NPC unseen: binding needs an exact row, and the row is checked
    against the param table where the Params group reads.
  - A lead that fails its check trips its group, as the degradation ladder has it.
- A start may run with the structural groups whole and the optional ones off: an optional group only turns itself off
  (ARCH §6.7, §6.8). That is enough for v0.1's in-game checks (S0-A, S0-B). v0.2's first conversation in game needs the
  conversation's groups too (session, world, player, flags, ui, place and params), so those follow before it.
- The clock's runs of bits: a field type of a word with a shift and a width, in en-gal's layout, by Lodestar. The play
  time's place becomes a field of `save_data` by Lodestar.

### 461. No second Esc after the chat box closes: Esc and the pad act on the box only while it is open

`2026-11-23` · DECIDED · `#engine` `#conversation` `#graceui` `#input` · refs: GraceUI:G39, NPC§5.7, NPC§5.6, S11.4.d, WHY§437

- The owner declined GraceUI's G39, answer (c), so proposal 0011's armed Escape (one Esc for a mod after its box has
  closed) is not built. In the owner's words: "esc should only work whilst the chatbox is open, press Enter to enter
  the written text. Press Esc to cancel and close down the chatbox. Once the reply is out or chatbox is closed, Esc
  function is immediately back to normal (game's function)"; and on the pad: "no need to stop reply, user either
  closes chatbox with Esc or enter reply with Enter. You can implement gamepad buttons to work as such too, but we
  only care about while the chatbox is open, if the chatbox is not open those keys are back to normal function."
- So while the box is open, Enter (and the pad's confirm, if one is mapped) sends the line, and Esc (and the pad's B,
  NPC §5.6) cancels and closes it. Once the box has closed or the reply is out, both are the game's at once. A reply
  still pacing after its box has closed finishes; no press stops it.
- This answers WHY§437's third point. The core's second-Esc rule, which nothing can reach, is removed with the
  conversation's polish (S11.4.d).
- *Later (2026-11-23, WHY§493):* the core's rule is removed; an Esc with the box closed changes nothing.

### 462. EldenNet's own discovery and flags reader work on the game, through the build's layout

`2026-11-23` · FACT · `#lodestar` `#gal` `#flags` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§457, WHY§458, S10.1.k

- The eleventh read-only round (package 0.1.0-lodestar.14; 7.8 minutes) ran discover through the build's layout as a
  partial pass on a thread of our own: 19 ms, 12 items resolved, all 12 from the table, none refused by its checks
  (placement and witness, on the game's own image for the first time through discover). Not ready, as expected.
- `ErBackend::flags_alone` gave a frame 44 times in 45 (the 45th after the game let its store go at the quit), with no
  reader fault and no breaker trip, and read the flag set in every playthrough (100) as set.
- The flags the owner's marks changed were not compared with the backend: the tree's walk was 47 records short of
  11,930 when the owner quit (WHY§464). The clock and the play time held by their invariants; the round's reader ran at
  9.92 readings a second, 4,490 readings, no fault.
- *Later (2026-11-23, WHY§468):* the twelfth round compared them: five of five agree.

### 463. game_state is sought by what it holds at the title and in the world, over four looks

`2026-11-23` · METHOD · `#lodestar` `#l6` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§460, O105, S10.1.m

- The first piece of S10.1.m, in WHY§460's order. Over the data's slots that point at live memory, the first 8 KiB of
  each object is read in four looks, told by the tester's marks: the title, the world, the title again, the world
  again. A 32-bit field that reads -1, then a number from 0 to 15, then -1, then the same number is a hit (at most 32
  listed). Each slot's value is taken from the image as loaded.
- One slot hit, with the title clean and the slot not already named, enters the round's draft table as `game_state` by
  the invariant recipe, with the code's references to the slot as witnesses. Nothing enters `gal/` before Lodestar's
  review of the files.
- About 5,000 objects, some 40 MiB a look: more than the owner's yes for the flags (24 MiB), so Lodestar asked again
  first, and the owner agreed (O105's third note).

### 464. A search that must end inside a session gets the searches' whole share until it is through

`2026-11-23` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§462

- The tree's walk ran on part of the searches' share in the eleventh round and ended 47 records short of 11,930 in a
  session of 7.8 minutes, so the marked flags could not be compared.
- The twelfth round gives the walk the searches' whole share until it is through, about a minute; a look of the state
  search likewise holds the whole share while it runs, the other searches waiting.

### 465. The menu's frame-cost check in GraceUI's harness is noisy on a four-core container

`2026-11-23` · FACT · `#graceui` `#harness` · refs: GraceUI:BUILD.md, GraceUI:D219, S11.2.aj

- With its medians unchanged (about 175 to 190 µs), the menu's p99 at 640 by 360 went over 1,000 µs on 0.2.22's build and
  0.2.23's alike, by turns, run alone and back to back; on the earlier, larger machine it passed whole.
- So on a four-core container a full run counts that check as the machine's, not a regression, when the medians hold
  (Grace's full run of 0.2.23: 232 checks, all but that one on both builds).

### 466. The layout has a field type for a run of bits in a word; the clock's two fields and the play time enter it as leads

`2026-11-23` · DECIDED · `#lodestar` `#gal` · refs: ARCH§6.3, WHY§459, WHY§460, S10.1.m

- 2.7.2.0 keeps the hour and the minute as two runs of one 64-bit word, so en-gal's layout gains the field type `bits`
  { word, shift, width }: a run in a little-endian word of 1, 2, 4 or 8 bytes, width 1 to 32, read as a whole number;
  a range lies within what the run holds; refused in a param row. A group takes it wherever it takes a whole number.
- `world_clock` (hour and minute, the word at 0x8, shifts 34 and 39, widths 5 and 6) and `save_data` (the play time, a
  u32 at 0xA0) enter the build's layout as leads (engine a5efb98, by Lodestar; ARCH §6.3's dated note).

### 467. Where a stalled GraceUI init stops: a step set before each part, readable from outside

`2026-11-23` · METHOD · `#graceui` `#spike` · refs: GraceUI:D220, GraceUI:D175, WHY§255, S11.2.ak

- GraceUI's log is drained by its init thread, so a stall before the install left only host=started (package case A2's
  one stall under load: killed at 240 s, install_runs 0).
- The init now sets a step before each part (fonts, the game's window, Direct3D, the hook targets, the input targets,
  the install, hooked), read through GraceUI_Test_InitStep, written by the spike observer as init_step and named by
  report.py when the host never hooked. A later stall says where it stopped.

### 468. EldenNet's own flags reader agrees with the game on flags the owner changed

`2026-11-23` · FACT · `#lodestar` `#gal` `#flags` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§457, WHY§462, S10.1.k

- The twelfth read-only round (package 0.1.0-lodestar.16; 8.6 minutes): four bits were set in the holder's stretch at
  the owner's fourth mark, and the tree's walk, through by then, gave them their numbers. `ErBackend::flags_alone`, on
  the build's layout, read each of the four as set, and the always-set flag (100) too: five of five agree. 46 calls,
  45 frames, no reader fault, no trip. WHY§457's comparison is done.
- Discovery through the layout took 29 ms: 12 items from the table, none refused, with the bits fields and the play
  time's field parsed in the game for the first time. The round's reader ran at 9.94 readings a second, 5,010
  readings, no fault.
- To mend (Lodestar's): the report's `backend.known_set` read false although the flag's own entry read set, because the
  last call after the quit overwrote it.

### 469. game_state: one candidate after the twelfth round, by the owner's word for the slot

`2026-11-23` · LIKELY · `#lodestar` `#l6` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§463, S10.1.m

- The state search ran as built: 1,397 candidates, the four looks in 5.9 s, 4.5 s, 0.24 s and 0.18 s, none overrun,
  the title clean. Of 52,501 fields that read -1 at the title, 251 read a small number in the world, 37 read -1 again
  and 22 the same number again, in 15 slots; so the round named none.
- Asked which place in the list of saves the rounds' character holds, the owner answered that it is third (the title's
  Continue loads it). The third place is slot 2 counting from 0. Of the 22 hits, twelve read 0, nine read 1 and one
  read 2: the static 64543992 (0x3D8DCF8), the 32-bit field 1,136 bytes (0x470) into its object, a static the
  accessor's shape reaches (5 sites, 30 references), its object of a class no lead names.
- LIKELY, on one session and the owner's word; not in the table. The thirteenth round confirms it by loading another
  character, then the usual one.
- *Later (2026-11-23, WHY§504):* the candidate read 4 and 2 for two characters: a number of the character, which
  fits the slot if the list runs last slot first; a third load is to settle it.
- *Later (2026-11-23, WHY§507):* the candidate is ruled out by the owner's save file: it is not the slot.

### 470. A search by what an object holds in each state takes its candidates in each state and lists its near misses

`2026-11-23` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§463, WHY§469

- The twelfth round's state search took its candidates at the title only: 1,397 slots of the data point at memory in
  use there, some 4,000 in the world. An object that moved between looks was dropped (716 alive after the first look,
  82 in the world), and the 214 fields that failed only the third look were not listed.
- So such a search takes its candidates in each state, keeps both numbers a field reads in the world, and lists the
  near misses; otherwise a lead that is half wrong leaves no trace.

### 471. The pilot Kalé and known(P) built offline; the core watches the catalogue's verified aliases only

`2026-11-23` · DECIDED · `#engine` `#knowledge` `#conversation` · refs: S11.3.c, S11.3.d, MEM§6, MEM§12, NPC§3.4, WHY§364, WHY§424

- en-core gains `knowledge.hpp`: the flag catalogue, `regions.toml`, the world stages and tracks, predicates in three
  values, `derive`, `known(P)` and `ground()` (block 5b's lines). The truth tables (T2) now run and pass for Kalé and the
  test persona at the eight fixture states (engine 567dcc6 to 9dc4691, merged as 5d6577e).
- The core's watch list holds the catalogue's verified aliases only. None is verified in game before S6, so the core
  asks for no flag and every alias stays Unknown: in game the pilot speaks from his road alone. Watching the unverified
  leads too (MEM §6.2) would start the flag reader for nothing it may use; S6's wide watch can widen the list later.
- A catalogue entry whose evidence is a `ci:` record verifies in the eval's realm only. The tests' overrides
  (`tests/scenarios/talk/pilot-overrides/`) give a catalogue of invented ids verified so, which carries a fixture state
  through the core as a flag frame. A fixture's `[tracks]` sets every alias of the named step's `when`.

### 472. The pilot's draft voice was past block 4's cap; the load now holds block 4 to it

`2026-11-23` · FACT · `#engine` `#content` · refs: NPC§3.4, NPC§7.3, NPC§4.8, WHY§388

- NPC §3.4's voice rendered to about 273 tokens in block 4 (cap 230); shortened, it is 224. PL9 (v0.3) wants room for a
  player's longest note, about 70 tokens more: the voice would need about 160 then, or the rule another look (put to
  the owner with the review list).
- The load checks block 4 against its cap as it checks block 2 (an error for shipped and test content, a warning for
  the player's own), and runs the part of PL4, PL5 and PL12 the first conversation can check.
- Openers are written as bare words and a tic without its full stop: the template joins openers with a comma and ends
  each clause, so "Ah." and "Well," had rendered as "Ah., Well,." The test persona's still doubles a full stop: a wart
  in test content.

### 473. Block 5b's lines are joined by a space

`2026-11-23` · DECIDED · `#engine` `#prompt` · refs: MEM§12.6, NPC§3.6

- The template joined a group's lines with "; ", made for part A's phrase-like fixture lines. The world's and the ties'
  lines are whole sentences, and NPC §3.6 renders them one after another, so the join is a space; NPC §3.6's
  `after_radahn` block now renders word for word. No existing golden changed.
- `ground()` stops at the first line that would take block 5b past 200 tokens (MEM §12.6); that line and the rest are
  kept by id for block 7's topic offers, which wait for the KB (v0.3).

### 474. runes >= n means two things for now: great runes in the knowledge, runes held in v0.1's predicates

`2026-11-23` · LIKELY · `#engine` `#predicates` · refs: MEM§6.6, DESIGN§10.4

- The knowledge's predicates read `runes >= n` as MEM §6.6 does, the great runes. v0.1's predicates (Augur's, and the
  tests' rig's) read it as the runes held. The conversation takes its stage from the knowledge; Augur still uses v0.1's.
- One meaning must win before a rule or a stage relies on it. Engine's lean: MEM §6.6's (great runes), with the runes
  held under a name of its own; settled with the conversation's polish.
- *Later (2026-11-23, WHY§498):* settled: `runes` means the great runes everywhere; the runes held are `runes_held`.

### 475. Kalé's index row keeps the design's NpcParam lead and leaves its entity lead out

`2026-11-23` · DECIDED · `#engine` `#identity` · refs: NPC§2.1, NPC§0, O43

- The row keeps NPC §2.1's NpcParam lead and flag block, unverified: a dev build binds him by the row, release mode
  refuses it until S1.
- The entity lead is left out: its own note says the prefix does not fit the church, and a wrong entity would voice
  another character as Kalé. S1's capture gives his entity id.

### 476. The "you, now" line and the behaviour switches, as far as v0.2 goes

`2026-11-23` · DECIDED · `#engine` `#conversation` · refs: NPC§7.3, DESIGN§5.5, ARCH§11.5, O35

- Wounds by DESIGN §5.5's bands, the boundaries inclusive below (75 per cent is still "unhurt"); "in the thick of a
  fight" when the Tarnished struck the persona in the last 8 s, since v0.2 tracks no other engagement; statuses wait
  for S8. A calm, unhurt persona gets no line.
- `behaviour.hpp` carries `features` and `range` with ARCH §11.5's defaults and bounds; `Core::set_behaviour` refuses a
  value out of bounds and keeps the old. v0.2 reads `features.chat` (off: no hint, the key opens nothing, an open
  conversation ends `closed` at the next step) and `features.self_awareness`; the conversation's reach stays NPC §5.2's.
  The DLL's settings reader does not hand them to the core yet (the polish).
- *Later (2026-11-23, WHY§497):* the DLL's settings reader hands `features` and `range` to the core.

### 477. The pilot's smaller choices: the catalogue's sources, clears left out, fixtures by pack

`2026-11-23` · DECIDED · `#engine` `#content` · refs: MEM§6.1, MEM§12.2, MEM§12.10

- The catalogue's `src` points to our research documents (ER05 §7 and the like), never a label of the game's.
- The world stages leave out `clears` (W10's, lint only) until the personas whose flags it names exist (v0.5).
- A prompt fixture may name a persona of the pack and a fixture state, so the pilot's goldens follow his shipped file;
  the eval's scenarios take a fixture state and `flags` steps and check what the prompt holds. The pilot's scenario
  speaks at two states, and his knowledge differs as it should.

### 478. windows.h defines near and far as nothing: en-core's public names avoid both

`2026-11-23` · FACT · `#engine` `#build` · refs: WHY§446

- A field named `near` in an en-core header broke clang-cl's DLL build: the DLL includes windows.h first, which defines
  `near` and `far` as nothing. It is `near_certainty` now (engine 9dc4691).

### 479. An S0 run's facts reach the launcher and spike-report through a report in the run's folder, not the watch block

`2026-11-23` · DECIDED · `#engine` `#spikes` `#launcher` · refs: DESIGN§16.1, ARCH§4.14, S10.1.b, engine wip/s12-s0 57fbe42 to 7f66a3b

- S0-A's table checks, S0-B's steady counts and what a stopped start waits for are not in the watch block, whose
  layout the brief keeps (an append-only field only after asking). So every S0 run gets a folder of its own, as the
  observer's runs do (`spikes\<word>-<n>`, named in ELDENNET_SPIKE_DIR), and eldennet.dll writes
  `eldennet-spike.json` there (kind `spike_run`, en_shell's spikerun.hpp): T1 before it publishes its terminal state,
  T6 every 10 s after. The DLL accepts the variable only as its own `<data>\spikes\<word>-<n>`.
- The launcher reads a few words and numbers of it (never logging anything but plain words); spike-report reads the
  spikes folder whole and joins each recording to its run by the game's process id in the recording's name.
- Rejected: an appended block field (asked for first by the brief), parsing eldennet.log (another process's file),
  a recording line of a new kind (DataPiler's schema, and S0-A records nothing).

### 480. A discovery refused only for a required static with no way to be found is a layout not complete yet

`2026-11-23` · DECIDED · `#engine` `#boot` `#gal` · refs: WHY§459, WHY§460, S10.1.b, S10.1.m, engine wip/s12-s0 57fbe42 to 7f66a3b

- On the real game today the start never reaches discovery's retries: with no patterns.toml and game_state neither in
  the table nor found by a recipe, `parse_patterns` refuses ("lacks the required pattern game_state") and the refusal
  is permanent. boot.cpp now tells this refusal (every problem a missing required pattern) from one of gal's files:
  the banner's detail is `layout`, and an S0 run's report says `waits = layout`. The start stops at once, so an S0-A
  run on the real game takes seconds of EldenNet's own, not 30.
- In an S0 run alone, T1 then makes one `partial` discovery pass (en-gal's option for a pass that refuses nothing for
  such a static) to check the build's table as a start would, so that S0-A checks the table's ids on today's real
  game; the start stops at the banner regardless. A normal start is unchanged.

### 481. S0-A's "untouched image": the table's checks made again once the start has stopped

`2026-11-23` · METHOD · `#engine` `#spikes` `#lodestar` · refs: proposal 0005 §7.1, §12, DESIGN§16.1, engine wip/s12-s0 57fbe42 to 7f66a3b

- At its terminal state T1 builds en-gal's `Library` again over the image as loaded and compares it with what
  discovery took from the table: each id's place, each class's tables of methods, the witnesses that still refer to
  their ids. `table_unchanged` passes when nothing was refused or moved. It shows that nothing of ours (and nothing of
  anyone's) changed the game's code where the table points between discovery and the end of the start; that
  EldenNet patches and hooks nothing rests on its imports (dll_checks) and on `getter_calls` staying 0 (`no_call`).

### 482. Under Wine ReadProcessMemory on our own process copies a reserved no-access page

`2026-11-23` · FACT · `#gal` `#wine` `#spikes` · refs: S10.1.b, engine wip/s12-s0 57fbe42 to 7f66a3b

- S0-D's trial copied a page reserved with PAGE_NOACCESS twenty times by each primitive: the SEH copy faulted and was
  caught each time, ReadProcessMemory succeeded each time under Wine 9.0 (zeros), where Windows refuses it. The trial's
  RPM copies read the null page instead, as the probe build's injection already did for that copy. LIKELY the same
  on Windows either way; the owner's run shows it.

### 483. A group never on shows Tripped in every snapshot; S0-B's no_trip counts the session's own trips

`2026-11-23` · FACT · `#gal` `#spikes` `#tools` · refs: DESIGN§16.1, engine wip/s12-s0 57fbe42 to 7f66a3b

- On the fake game with a whole table, the `status` group (not discovered) read `Tripped` in every recorded snapshot,
  and spike-report's no_trip failed with no trip in the session. It now counts a group only if it was in the first
  DataReady's caps (every group with no DataReady). On the real game, where most optional groups are off at first,
  the old count would have failed S0-B for every one of them.

### 484. A start stopped at Discover has no crash filter of ours; S0-D's crashes there rest on the watcher alone

`2026-11-23` · FACT · `#engine` `#spikes` · refs: ARCH§3.2, O71, engine wip/s12-s0 57fbe42 to 7f66a3b

- The crash filter goes in after WaitEngine (or the window, on the banner path). A start that stops at the discovery
  banner, as on the real game today, never installs it, so S0-D's crashes on request there are recorded by the watcher
  from the exit code and the module list, with no crash record of EldenNet's (filter_records 0). S0-D's rule asks for
  the watcher's record only, so it passes; whether a stopped start should install the filter is left to the design.
- *Later (2026-11-23, WHY§510):* decided: a start stopped at a banner after the window installs the filter.

### 485. The panel's confirmations go on after the Mark's hand-over

`2026-11-23` · METHOD · `#launcher` `#spikes` `#graceui` · refs: WHY§444, ARCH§4.14, S10.1.g, engine wip/s12-s0 57fbe42 to 7f66a3b

- Once EldenNet's own Mark (GraceUI's hotkey on Pause) holds the ring, the launcher has no key: in a spike's run (S0,
  and the observer's runs, which now show the panel too) each new Mark in the ring confirms the task under way
  (`task … by=mark`), and the launcher stamps none; a press queued at the hand-over confirms as a press does. The
  read round keeps its own way. Shown under Wine with GraceUI 0.2.22's test build
  (`launcher-eldennet-graceui-spike-s0e`). Whether the panel was shown goes into the run's line, the session's
  record and the run's `launcher-run.json`, so that the observer's frame counts are read knowing a topmost window was
  over the game.

### 486. The probe DLL's p99 on the fake game under Wine sits near S0-B's 0.5 ms

`2026-11-23` · FACT · `#gal` `#spikes` `#wine` · refs: WHY§320, DESIGN§16.1, engine wip/s12-s0 57fbe42 to 7f66a3b

- With the build's table and the probe DLL, the reader's C13 p99 on the fake game under Wine on this shared 4-core
  machine was 511 to 895 µs across runs (WHY§320 saw under 0.2 ms for the own-thread reader alone). The fake game's
  S0-B case allows either status for task_p99_us; the owner's PC decides the rule.

### 487. The S0 package's README is README-SPIKES; the s0e-s4 kind's template moves aside

`2026-11-23` · DECIDED · `#spikes` `#tools` · refs: S10.1.b, S10.1.g, engine wip/s12-s0 57fbe42 to 7f66a3b

- The brief named README-SPIKES.txt.in for the S0 kind, which was the s0e-s4 kind's template: it is now
  `README-S0E-S4.txt.in`, still packed as README-SPIKES.txt in EldenNet-Spikes, and the S0 kind's template is
  `README-SPIKES.txt.in`. The S0 package needs no settings.toml: S0 sessions record by the spike, and S0-D's faults
  are the probe build's own (the trial, and the first optional group read where settings.toml names none).

### 488. A field found by what it holds in the game's states is told from its look-alikes by a second save

`2026-11-23` · METHOD · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§469, O105, S10.1.m

- The thirteenth round's state search keeps both numbers a field reads in the world: a hit is -1, 0 to 15, -1, 0 to
  15, and hits that are one field of one object seen through several slots are numbered alike. With two characters
  loaded (the first of the list, then the usual one), the save's slot reads two numbers, and what reads one number is
  something else; the round names `game_state` where one field in one slot alone read two numbers.
- The owner agreed to load the first character once (O105's fourth note). Nothing enters `gal/` before Lodestar's
  review of the files.
- *Later (2026-11-23, WHY§504):* with two characters seven fields in 25 slots read two numbers, so that rule alone
  names nothing; a field that changed for the same character between sessions is ruled out.
- *Later (2026-11-23, WHY§507):* the candidate it found is not the slot; the next search seeks the slot's known numbers.

### 489. The pointers and lists are sought by class from the characters manager's own object

`2026-11-23` · METHOD · `#lodestar` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§460, O105, S10.1.m

- The character search (four looks, each after the state search's, with the searches' whole share) reads the manager's
  object and what follows it (up to 256 KiB), the first 256 bytes behind each address in it (8,192 at most), up to
  16 KiB of each run two neighbouring addresses bound (64 at most), 16 KiB of the main player's object and 4 KiB of the
  save data's; 12,000 reads at most a look.
- Each address is followed once and told by its target's table of methods. A list is two neighbouring addresses that
  bound a run of equal elements each holding a character's address; the list by distance has a fraction beside the
  address that never falls along the run. It writes offsets, sizes, counts and tables of methods only, never a value
  of any object. The owner agreed to the reading (about 6 MiB a look at most), and to the save data's 4 KiB (O105).

### 490. G9's pairing is built as decided, with three choices of GraceUI's own

`2026-11-23` · FACT · `#graceui` `#security` · refs: GraceUI:D221, GraceUI:D222, GraceUI:D225, G9, S11.2.al

- The device names itself on its pairing page (a phone's keyboard is to hand; only the code is read in game).
- The PC's network address is shown on the in-game sheet alone, never in a topic, since a topic reaches every browser,
  a paired phone's too (PV3).
- A refused command gets one answer from GraceUI before its space sees it (not_now, "Only on the PC the game runs on."),
  so no space needs a device check of its own; a space marks its sensitive commands and settings (S11.4.e).
- Accepted and recorded in D225, not built: a device's cookie is a bearer token over plain HTTP on the home network, so
  a device that can read or redirect the phone's traffic could replay it until the player forgets the device; and the
  listener takes the PC's first four private addresses, a VPN's or a virtual switch's among them (the sheet lists each).

### 491. Limits shared between listeners need a share per listener; a limit shared between peers needs a cap per peer

`2026-11-23` · METHOD · `#graceui` `#security` · refs: GraceUI:D225, WHY§264

- A second review of the joins, given the code and not our conclusions, again found the wire layer sound and the gaps
  at the edges: a shared limit (five tries a code) that one peer could spend for everyone, shared caps a device could
  take from this PC, and a sensitive list that left out the hotkeys. All five fixed before release.

### 492. A modal driven by the host must be open before the host's first look at it

`2026-11-23` · FACT · `#graceui` · refs: GraceUI:D222, GraceUI:D223

- The harness's scenario through the real menu caught what each side's unit tests passed: the menu's first refresh of
  the pairing sheet ran before the sheet was shown, took it as closed and voided the code.

### 493. The second Esc removed; an Esc that reaches the core with the box closed changes nothing

`2026-11-23` · DECIDED · `#engine` `#conversation` `#input` · refs: WHY§461, NPC§5.7, GraceUI:G39, engine wip/s12-polish e4736b4 to e2519d2

- The core's rule for a second Esc is gone (the owner's G39 (c)): `on_close` ignores an Escape while the box is not
  open, before it touches the draft, so the reply paces out and the conversation ends after it, as NPC §5.7's first
  half says. The overlay sends Escape only while it holds input, so nothing reaches the rule in game; the guard is for
  any surface that would.
- Found on the way: the box opened again with the talk key while an Esc-closed reply paced still carried the
  "end when it is done" mark, so the conversation would have ended under the reopened box. The reopen now clears it.
- Tests: the second-Esc subcase now checks that one after the close changes nothing and the reply paces out; the
  commit test that used two Escs to end at once ends by walking away (> 12 m for 2 s) instead, its checks unchanged.

### 494. A queued line goes when the stream ends with the last page up (WHY§432 closed)

`2026-11-23` · DECIDED · `#engine` `#conversation` · refs: WHY§432, NPC§6.3, engine wip/s12-polish e4736b4 to e2519d2

- Without changing the pager's interface: once no sentence follows (`sentences_done`) and the page up is the newest
  sealed, the pump asks the pager for the next page. `Done`: the page up is marked last, and a queued line goes at once
  if it has shown, else at its PageShown (the existing path). `Page`: it is kept and published at the hold's end as
  before. Once the stream is closed the pager's `next()` is time-independent (no first-page wait, no short-sentence
  hold), so sealing early gives the same page; the stubs behave alike.
- A queued line exists only while nothing has shown (Awaiting, Held); it can outlive page 1 when page 1 was sealed
  while streaming, so the rule covers a later page too, which a test checks.

### 495. The refusal's toast names the holder by GraceUI's id; not-now by its reason

`2026-11-23` · LIKELY · `#engine` `#conversation` `#graceui` · refs: WHY§442, GraceUI:D77, GraceUI:D142, engine wip/s12-polish e4736b4 to e2519d2

- GraceUI's request_input answer names the holder by its mod id alone ("graceui" for its menu; GraceUI's own
  "‹Mod› has the controls" notice uses the display name, which the ABI does not give another mod). So the toast says
  "GraceUI's menu" for `graceui`, "another mod (‹id›)" for an id as GraceUI checks one at connect (3 to 32 of a-z, 0-9,
  '-'), generic words for anything else (GraceUI's own fallback "a mod" among them) and for EldenNet's own id. Not-now
  names danger, loading and a game menu; not ready and anything else keep the generic words. Words for the owner.
- A display name would need an ABI append from GraceUI (a holder's display name in the answer); asked for nowhere yet.

### 496. L3 and L4 on the pages

`2026-11-23` · LIKELY · `#engine` `#pages` · refs: WHY§422, WHY§442, S11.4.a, engine wip/s12-polish e4736b4 to e2519d2

- en-overlay's Level gains SpeakingInPart (L3) and Speaking (L4), the values growing as en-core's do; the DLL's map is
  one to one, which clang-cl's warnings now take. Lamp: "Speaking, in part" (ember) and "Speaking" (moss); Dormant over
  either; "Offline and solo" on System › Status from L2 up; the AI pages' lead at L3 and L4 "Conversation is ready:
  NPCs answer when you speak to them." Words for the owner. No fixture state is at L3 or L4, so the topics.json
  fixtures and grace-dev goldens are unchanged.
- Left: L2's words still say "NPCs speak from a later version" and "EldenNet only observes in this version", which a
  v0.2 build makes untrue; changing them moves the fixtures and the goldens (a grace-dev build at a9aac78).

### 497. The behaviour settings reach the core (WHY§476 closed)

`2026-11-23` · DECIDED · `#engine` `#settings` · refs: WHY§476, ARCH§11.5, ARCH§11.6, engine wip/s12-polish e4736b4 to e2519d2

- en_shell reads `features` (the nine switches, booleans) and `range` (numbers, TOML integers too, within en-core's
  own bounds, NaN and inf refused) strictly: a problem refuses the whole file as any other does. T6 hands the result to
  `Core::set_behaviour` as it makes the core; a pages' change of a `features.*` or `range.*` key is checked against the
  core's bounds before it is applied or written, then applied on the behaviour the core holds after overlay_tick. No
  page offers these keys yet, so that path waits for the Behaviour pages; SettingsEdit gained a number for it.
- Settings are not hot-reloaded from the file yet (T10 watches nothing): "when settings change" is the pages' path.

### 498. One meaning of `runes >= n`: the great runes (WHY§474 closed)

`2026-11-23` · DECIDED · `#engine` `#predicates` · refs: WHY§474, MEM§6.6, engine wip/s12-polish e4736b4 to e2519d2

- v0.1's predicates: `runes_held` is the runes held; `runes` is refused with a fixed text naming `runes_held`, as
  `flag()` is, since they read no catalogue alias. A persona's `when` on `runes >= n` is so Unknown in the directory
  (an unread flag's treatment) and evaluated by the knowledge.
- Moved to `runes_held`: en-core's tests/test_augur_wiring.cpp (Augur's `requires`), tests/identity/test_identity.cpp
  (Wren's `rich`), tests/conversation/test_gating.cpp (Wren's `sulk` and `night`, and the rig's facts); core.cpp's
  facts. Kept as the great runes: tests/content/test_content.cpp (never evaluated), tests/knowledge/test_knowledge.cpp
  (the knowledge's own). No shipped content or Augur rule file used `runes`.

### 499. disable.toml (S11.3.g)

`2026-11-23` · DECIDED · `#engine` `#content` · refs: ARCH§11.6, DESIGN§11.4, PAGES§4.6, engine wip/s12-polish e4736b4 to e2519d2

- Read in load_content after the index, from the pack's `disable.toml` (the overrides folder's root, which the DLL
  reads with its other files). Schema: ARCH §11.6's six lists of ids, strict. `personas` turns rows off (as
  `enabled = false`), so no record is read and the index, directory and knowledge leave them out; the file is in the
  hash. Warnings, never errors, name the list and place, never the id: an unknown persona; every card (none shipped in
  v0.2); one each for a non-empty `characters`, `kb`, `triggers`, `rules`. Core::content_warnings() carries them first.
- A persona another names (a tie's `stage()`) cannot be turned off without the knowledge refusing the pack, as with the
  index's own `enabled = false`; none in v0.2's content.
- The engine's pages have no Content packs page yet (PAGES §4.6): the ids are not shown; the log's `content_load`
  counts them (`disabled=`).

### 500. A failed configure-time download leaves an empty file that the next configure keeps

`2026-11-23` · FACT · `#engine` `#build` · refs: BUILD.md, engine wip/s12-polish e4736b4 to e2519d2

- A transient HTTP error on Dear ImGui's `imgui_widgets.cpp` failed the MinGW configure; CMake's `file(DOWNLOAD)` left
  a 0-byte file, and `en_fetch` skips any file that exists, so the next configure passed and the link failed with
  undefined ImGui symbols. Deleting the empty file and configuring again fixed it. The top-level CMakeLists.txt is not
  this job's; a fix would remove the file on a failed download or check the hash of an existing one.
- *Later (2026-11-23):* fixed by Engine: `en_fetch` checks a kept file's hash and downloads it again if it differs, and
  checks a new download's hash itself, removing the file on any failure (EXPECTED_HASH stops the configure first).

### 501. The polish's tests on the four toolchains; Lodestar's read-clock case passes

`2026-11-23` · FACT · `#engine` `#tests` · refs: WHY§364, engine wip/s12-polish e4736b4 to e2519d2

- GCC (Release) and Clang: 0 warnings, ctest 37 of 37, the eval label 8 of 8. MinGW-w64: 0 warnings, every unit test
  program under Wine, the fake game's 60 cases. clang-cl with GraceUI 0.2.24's test build (feat/foundations 8eacc6c):
  0 warnings, every unit test program under Wine, eldennet_dll_checks, and the harness's 121 cases, the ten GraceUI
  cases among them; `launcher-eldennet-read-clock` passed (133 s), after failing twice on the pilot's branch before
  Lodestar's latest merge. (CTest runs the Windows unit test programs without Wine, so they show as failed there.)

### 502. The characters manager's places, by search and class, the same with two characters

`2026-11-23` · LIKELY · `#lodestar` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§489, S10.1.m

- The thirteenth round (package 0.1.0-lodestar.17; 7.0 minutes): `main_player` is the word 0x1E508 into the manager's
  object, the one word that held the address of an object of the player's class, in both worlds, with two characters.
- The near list at 0x1F1E8: three addresses side by side (start, end, a third at or past the end); 16-byte elements,
  a character's address at 0 and a fraction at 8; 10 and 13 elements, never the main player. Two lists of addresses
  alone at 0x1F1C8 (31, the main player among them) and 0x1F218 (6 and 5).
- The player set at 0x10EF8: an address of plain memory whose first word is the main player's address (its stride and
  length not known).
- The manager is not there at the title (its static empty until a save is loaded, and again after a quit), and its
  object is over 127 KiB: every one of these lies past the layout's 0x4000 (WHY§506).

- Later (2026-11-23, WHY§538): the player's class is not the main player's alone: eight objects of it stood in the
  manager's words in one look; the main player is the object at characters.main_player.

### 503. In the player's object: the parts holder at 0x190 and the character's sheet at 0x580

`2026-11-23` · LIKELY · `#lodestar` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§489

- `character.parts` at 0x190: the address of plain memory that begins with 30 addresses, the first 16 of classed
  objects. 43 words of the player's object held the address of an object of the same kind at the same offset in both
  characters, all in its first 2,712 bytes.
- 0x580 is the address of an object of the class whose table is at 43954088; the save data's object points at that
  class 8 and 16 bytes in, so `save_data.player_data` is 0x8 or 0x10, and the player's object keeps the sheet too.

### 504. game_state's candidate is a number of the character: 4 for the list's first, 2 for the third

`2026-11-23` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§469, WHY§488, S10.1.m

- With two characters the candidate (static 64543992, 0x470) read 4 with the list's first character and 2 with the
  usual one (2 twice in the twelfth round): a number of the character, steady across sessions, but not its place from
  the top of the list. Counted from the bottom of five, the first is 4 and the third is 2: it fits the save's slot if
  the game lists its slots last first. A third load (the last of the list should read 0) is to make it firm.
- Seven fields in 25 slots read two numbers, so "one field in one slot that reads two numbers" names nothing by itself;
  a field that changed for the same character between sessions is ruled out by that (one read 1 twice before and 4
  now).
- *Later (2026-11-23, WHY§507):* corrected by the owner's save file: the list runs in slot order, so the candidate is
  not the slot, and the guess that the list runs last slot first is rejected.

### 505. A search of a manager's object is asked for only once the game is in the state it is to see

`2026-11-23` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md

- Made when the manager was first named (as the first load began), the character search's first look ran through the
  loading and saw the manager being filled, and its looks at the title were pointless (no manager there). The next
  round looks in the world only, after a confirmation.

### 506. Fields past a class's 0x4000 copy are far fields, each read by its own small copy

`2026-11-23` · DECIDED · `#engine` `#gal` `#lodestar` · refs: ARCH§6.3, WHY§502, WHY§489, S10.1.m

- Engine's ruling on Lodestar's question (the characters manager's four places lie at 0x10EF8 to 0x1F1F8, in an object
  of over 127 KiB, all past ARCH §6.3's `copy_size ≤ 0x4000`): option (a), a field read by its own small copy, not a
  class entry at an offset (b), which would need two windows or more of 58 KiB ten times a second.
- In the layout, a field may say `far = true`: its `off` may lie past `copy_size`, up to 0x100000 (1 MiB) into the object;
  it is read at the object's address plus `off` by a copy of its own size, at most 32 bytes (the whole numbers, `f32`,
  `bool8`, `bit`, `bits`, `ptr`, `handle`, `vec3f` and `dlvector`; never `utf16[n]`, `mat4f` or `array`). A class whose
  fields are all far may have `copy_size = 0`. Each far read counts against the tick's reads as any other copy.
- Lodestar makes both sides, the layout's schema and lint (en-gal's layout.cpp) and ErBackend's groups reading far
  fields, with tests on the fake world; Engine reviews, merges and adds ARCH §6.3's dated note.

### 507. game_state's candidate is not the save's slot: the owner's save file gives the slots

`2026-11-23` · FACT · `#lodestar` `#l6` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§469, WHY§488, WHY§504, S10.1.m

- The owner sent the save file, unasked, and said the usual character is the newest of the five. Lodestar read one fact
  from it offline, in a folder of its own with a script kept elsewhere, and kept nothing (its copy deleted; no name
  recorded): the characters stand in slots 0, 1, 2, 5 and 6, and the game's list runs in slot order.
- So the candidate (static 64543992, 0x470), which read 4 for slot 0's character and 2 for slot 2's, is some number of
  the character, not the slot. No hit of the state search read 0 and then 2: as a 32-bit number that reads -1 at the
  title, the slot is in none of the 1,399 objects the search could follow.
- Rejected with it: that the game lists its slots last first (WHY§504).

### 508. Ground truth first, then the search

`2026-11-23` · METHOD · `#lodestar` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§507

- With the characters' slots known from the owner's own save file (one fact read offline, nothing kept), the slot's
  field is sought by the numbers it must read in two worlds (6 for the last of the list, 2 for the usual one), over the
  data's slots in use in the world, not by a lead about what it reads at the title.

### 509. S0-E's run A1 crashed on the owner's PC at GraceUI's first Present, before a frame was drawn

`2026-11-23` · FACT · `#spike` `#graceui` `#engine` · refs: S10.1.g, GraceUI:D175, GraceUI:D220, WHY§255

- Package 0.1.0-spikes.3 (engine 31a98e4, GraceUI 0.2.24), run A1, twice: 0xC0000005 about 10 s and 24 s after the
  start, before the title screen's first task. Both times the observer's last state line read host=hooked,
  install_runs=1, init_step=7, hooks=9, presents=1, frames_drawn=0, standin=patched, standin_presents=1 and
  prologue=jump: the game's Present was already patched when GraceUI hooked it.
- Three other overlays were loaded in the game: a third-party frame-time overlay's hooks, Steam's overlay (later) and
  the graphics driver's capture overlay. Earlier read-only rounds ran for minutes beside them, but hooked nothing.
- EldenNet connected to GraceUI, posted its banner (the release DLL refuses the package's leads, as expected) and did
  nothing more. The watcher's record names no module (crash: null), and EldenNet's own filter was not in (WHY§484,
  §510). With Grace: a run with the frame-time overlay closed, and A0 (no EldenNet), are asked of the owner.
- *Later (2026-11-23, WHY§511):* A1 again and A0 (no eldennet.dll) crashed the same way, 5.8 to 6.0 s in, one
  Present passed, no queue found, nothing drawn; so EldenNet is out of it. The frame-time overlay's hooks were still
  loaded in both, so its part is not yet tested.
- *Later (2026-11-23):* Windows' Application Error events for all four crashes name no module (unknown, 0.0.0.0) and a
  fault offset of 0: the faulting address was 0, a call or jump through a null pointer on the first Present's path,
  not a bad read inside a DLL. Told to Grace; package 0.1.0-spikes.4 (A5 without the stand-in, frame_step) is to place it.

### 510. A start that stops at a banner once the window has shown installs the crash filter

`2026-11-23` · DECIDED · `#engine` `#boot` · refs: ARCH§3.2, WHY§484, WHY§509, WHY§172

- Answers WHY§484's open point. The filter went in after WaitEngine, so a start stopped at the discovery banner (the
  real game's case today) had none, and a crash after it was recorded by the watcher alone, with no faulting module
  (WHY§509). Now every banner after the window installs it, as the banner path already did; it only records, then
  passes the crash on (engine 2933494). S0-D's crash case expects the filter's record at the discovery banner too.

### 511. S0-E's next package: A5 with GraceUI and its observer alone, state lines every 200 ms, other overlays closed

`2026-11-23` · DECIDED · `#spike` `#graceui` `#engine` · refs: WHY§509, S10.1.g, GraceUI:D175, GraceUI:D220

- After four crashes (A1 three times, A0 once), all within seconds of GraceUI's install, with or without EldenNet, the
  s0e-s4 package gains run A5, first in the order: GraceUI and its observer alone, injected early, with no stand-in and
  no EldenNet. It tells GraceUI's own hook from the chain the stand-in and other programs' overlays make with it.
- Every run file sets `GRACE_SPIKE_STATE_MS=200` (Grace's request), so the observer's last state line lies within a
  frame or two of a fault; the launcher hands its environment to the game.
- The README asks for every other overlay to be closed, its background process too, since a closed window left one
  loaded in the owner's second runs. The next package also carries Grace's test build that records how far GraceUI's
  first frames got, when it is ready (engine 4f9e9d2).

### 512. G28's upload route: the ticket in a header, spent on its first match; the size before a byte, the hash on a worker

`2026-11-23` · FACT · `#graceui` `#security` · refs: GraceUI:D226, G28, S11.2.am

- The ticket travels in Grace-Upload-Ticket, never the URL, since a URL reaches logs and history; it is spent as soon as
  it matches, so a wrong size, a dropped connection or a changed file each end it.
- The size is checked before a byte is read, the hash after, on GraceUI's worker thread: 32 MiB hashed on the loop
  would stall every other page. GraceUI keeps nothing afterwards (its buffer zeroes itself).

### 513. A browser hashes only in a secure context, so uploads stay on this PC

`2026-11-23` · FACT · `#graceui` · refs: GraceUI:D226, GraceUI:D227, GraceUI:D114

- A browser computes SHA-256 only in a secure context: 127.0.0.1 is one, a private address over plain HTTP is not. So
  G9's rule (no secure-context features, D114) and G28's (this PC alone) meet: a paired phone could not hash a file even
  if the route took it, and the page names the view so it says so rather than offering a button that cannot work.

### 514. A count cap needs a byte cap when items vary that much, and every path to a command carries the same rule

`2026-11-23` · METHOD · `#graceui` `#security` · refs: GraceUI:D229, GraceUI:D225, WHY§264, WHY§491

- A third review of the joins, given the code and not our conclusions, found the door sound (no bytes to a space without
  a live, matching ticket; a race of two POSTs on one ticket gave 202 and 403) and the gaps behind it: a queue capped by
  count, which is no cap once each item may be 32 MiB; a read buffer that kept the tail of each file; the menu, a second
  path to a command the first guarded; an event held past the answer its view already had. All fixed before release.
- Accepted and recorded in D229, not built: the upload ticket is a bearer token on loopback, so a local program holding
  both the session's cookie and the ticket could use it; it travels only on that session's WebSocket and dies with it.

### 515. A spike run's own record lists the game's modules by name

`2026-11-23` · DECIDED · `#launcher` `#spike` · refs: WHY§509, WHY§511, S10.1.g

- Grace asked, after A1's crashes, to know whether the frame-time overlay's hooks were in the chain in a run that does
  not crash too. So launcher-run.json, in each spike run's folder, now lists the game's modules as last listed, by name
  alone and with no path, as a crash record keeps them (engine 7b8c987). The panel cases check the list.

### 516. A step set as the code passes, read by an observer that writes at each change, places a fall without a debugger

`2026-11-23` · METHOD · `#graceui` `#spike` · refs: GraceUI:D230, GraceUI:D220, WHY§509

- A1 fell four times between two state lines five seconds apart, with nothing to place it: the watcher's record had no
  faulting address. A step the code sets as it passes (D220 for the init, D230 for the first frames), read by an
  observer that writes at each change, places a fall within 50 ms; and Windows' own Application Error events (event
  1000), already on the player's PC, name the faulting module and offset for runs already done (asked of the owner).

### 517. Run A5 reached the title without the stand-in; the tester's panel moves below GraceUI's notices

`2026-11-23` · FACT · `#spike` `#graceui` `#launcher` · refs: WHY§509, WHY§511, S10.1.g

- Package 0.1.0-spikes.4, run A5 (GraceUI and its observer alone, no stand-in, no EldenNet): the game reached the title
  screen and stayed up, with GraceUI's notice drawn at the top right: the first run on the owner's PC past GraceUI's
  first frames. Every earlier run there had the stand-in loaded and fell within seconds at address 0 (WHY§509), so the
  stand-in's part in the chain is now the first suspect. The rest of the run and A1 are to confirm it.
- The owner saw the launcher's task panel cover GraceUI's notice at the game's top right. The panel now opens 72 px
  (at 96 dpi) below the work area's top, under two of GraceUI's one-line notices (engine b1f7900; Lodestar told).

### 518. The stand-in for another overlay caused the owner's crashes; no run loads it any more

`2026-11-23` · DECIDED · `#spike` `#graceui` · refs: WHY§509, WHY§517, GraceUI:D231, S10.1.g

- (LIKELY, Grace's reading) The stand-in finds Present's vtable slot through a short-lived swap chain of its own and
  writes its function there. Under Wine that slot is DXGI's shared table; on a real PC an overlay that hooks by vtable
  (the graphics driver's capture overlay was loaded in every run) can give a swap chain its own copy, freed with it, so
  the write lands in freed memory and a later call goes through what is left: address 0. A fault of our test tool, not
  of a shipped part; GraceUI 0.2.28's fault recorder (D231) could confirm it, but it is off GraceUI's path now.
- (DECIDED) The owner's PC has real overlays, which is what S0-E asks about, and A5 has shown GraceUI living beside
  them. So the s0e-s4 package's runs (A5, A1 to A4, B) load no stand-in and the package no longer ships it; A0 (GraceUI,
  the observer and the stand-in) goes (engine 5dd8ce1). The harness keeps the stand-in for its own cases.

- Later (2026-11-23, WHY§529): A1 still fell, the same way, with no stand-in loaded, so the stand-in cannot explain
  A1's falls; A0's alone remain on it. A1's cause is open, with Grace.

### 519. A game that turns its camera by the mouse keeps the cursor at its centre; an overlay's pointer follows the mouse's own motion

`2026-11-23` · FACT · `#graceui` `#input` · refs: GraceUI:D232, GraceUI:D130, WHY§517, S11.2.ap

- In A5 on the owner's PC GraceUI's pointer was pulled back to the centre each frame ("a rubberband"): the game keeps
  the system cursor at its centre whatever holds input, and the pointer followed the cursor's position.
- The mouse's relative motion, which the game itself reads (raw input or DirectInput) and GraceUI already sees, is the
  pointer's true source while GraceUI holds the mouse (GraceUI 0.2.29, D232; no new hook target). The harness's fake
  game never recentred, which is why the menu-mouse scenario passed; a fake game that does is now a scenario.

### 520. The profile editor's commands are answered later, through GraceUI's reply handle

`2026-11-23` · FACT · `#engine` `#editor` `#pages` · refs: GraceUI's D216, D217, D218, WHY§377 (3), S7.2.b, engine wip/s12-editor 47a63ba to c568fdf, merged at c223b6e

- A check runs en-core's own load, and a save is answered once T11 has written the file, so the space cannot answer
  them in the poll that brought them. `Space::command` takes GraceUI's reply handle; the profile editor's commands are
  checked at the entry point (invariant 3) and handed to the DLL as `ProfileRequest`s; the DLL answers through
  `Space::answer_profile`, and the Overlay hands each answer to GraceUI once (`answer_later`, and at every step before it
  publishes, so a save's answer comes before its document's next revision). Every other command is answered at once,
  as before.

### 521. A layer over a shipped persona keeps its persona's key; `mine.<id>` waits for the player's own characters

`2026-11-23` · DECIDED · `#engine` `#editor` · refs: NPC§4.5, NPC§4.7, WHY§387, WHY§393, S7.2.b, engine wip/s12-editor 47a63ba to c568fdf, merged at c223b6e

- NPC §4.7's slots: a layer over `persona:<id>` stays on that slot; `mine:<id>` is a character profile of the player's
  own, made for a `named:<param>` NPC from a sighting. v0.2's engine has no character profiles (NPC §4.5's index and
  files), no sightings store (MEM §8.7) and so no `named.<param>` rows: the roster is the shipped personas.
- So a save writes the player's layer and its answer names the same row (`chosen: "persona.kale"`, harmless, D218);
  the row that changes its key comes with the player's own characters. The build's brief, "`mine.<id>` as WHY§387 says", was read
  as that case; the layer's file is NPC §4.7's name, in the overrides folder the load reads (the brief), not
  `%APPDATA%\EldenNet\profiles\` (NPC §4.7): one folder fewer for the shell to read, as the brief asked.

### 522. Block 4 passes its cap with any note over the pilot: a warning on every save

`2026-11-23` · FACT · `#engine` `#content` · refs: WHY§472, O109, NPC§4.8 (PL9), S7.2.b, engine wip/s12-editor 47a63ba to c568fdf, merged at c223b6e

- Kalé's voice comes to 224 of block 4's 230 tokens; a note of a few words ("Also: …") passes the cap. As built
  (WHY§472) it is a warning for the player's own, so the editor saves it with "Saved, with warnings" and Check names it.
  PL9 would refuse the file; the owner kept the voice and looks at the rule again at v0.3, when the player's note gets
  room of its own (O109 (b)).

### 523. Sensitive marks: EldenNet's own command names, not the brief's

`2026-11-23` · FACT · `#engine` `#graceui` `#security` · refs: GraceUI's D221, eldennet-changes §60, S11.4.e, WHY§490, engine wip/s12-editor 47a63ba to c568fdf, merged at c223b6e

- §60's example list (`secrets.set`, `secrets.clear`, `providers.update`) names no command of EldenNet's space, and
  GraceUI's lint refuses a sensitive command the space does not declare. EldenNet's are `providers.add` (a provider of
  one's own, with its address), `providers.remove` and `providers.key` (a row secret, sensitive already; marked for the
  record). Settings marked: `debug.panel` (SP8's developer view) and `debug.record_sessions` (what is written to the
  PC's disk, PV8). Left unmarked: `debug.log_level` (the log holds no text and is scrubbed), the preset and each job's
  provider, model and fallback (choices among the player's own providers, the AI pages' everyday use).
- The editor's save is not marked (recommendation, not built): the player's own notes, the same from a phone as from
  the PC's browser, and the phone's keyboard is the editor's easiest; a draft sends the player's notes to the provider
  the player chose, as the PC would. A mark later is one line of space.toml.

### 524. grace-dev's dump draws a master page's detail empty

`2026-11-23` · FACT · `#graceui` `#pages` · refs: GraceUI's D215, tests/pages/preview.py, engine wip/s12-editor 47a63ba to c568fdf, merged at c223b6e

- `grace-dev --dump` gathers the components' topics once, before the list's first row is chosen, so a block bound to a
  family draws "Nothing to show yet." in the goldens of Named NPCs. The menu itself draws the chosen row's member (the
  fake game's case checks the editor's label drawn). For GraceUI: a second pass in the dump once the master's list is set.

### 525. A paired device in the fake game: the listener at home on 127.0.0.2

`2026-11-23` · METHOD · `#engine` `#tests` `#graceui` · refs: GraceUI's D221 to D225, G9, engine wip/s12-editor 47a63ba to c568fdf, merged at c223b6e

- The fake game's `--grace-lan`, with GraceUI loaded first (the launcher's `--also`), calls the test build's
  `GraceUI_Test_SetNetworkAddresses` at boot, before EldenNet registers its pages and GraceUI's server starts; Other
  devices is on in GraceUI's own `user\settings.json`, written by the case; the reports name the pairing sheet's code
  (`GraceUI_Test_GetPairingCode`). The tests' browser pairs on 127.0.0.2 with it (`/api/pair`), then joins with the
  device cookie.

### 526. The browser view's port, a case's own: another run of GraceUI's cases holds 8642

`2026-11-23` · FACT · `#engine` `#tests` · refs: BUILD.md (GraceUI's cases), GraceUI's dashboard.port, engine wip/s12-editor 47a63ba to c568fdf, merged at c223b6e

- With another agent's harness running GraceUI's cases at the same time, the editor's case found GraceUI's server not
  running (port 8642 held). The case writes its own free port into GraceUI's `user\settings.json`; the older GraceUI
  cases still take 8642 and can meet the same collision when two trees run them at once.

### 527. Lodestar's read-state case is timing-tight; read-chars fails on the base too

`2026-11-23` · FACT · `#tests` `#lodestar` · refs: tests/fakegame/harness.py (check_read_state, check_read_chars), engine wip/s12-editor 47a63ba to c568fdf, merged at c223b6e

- Base (4a0e8a5, a clang-cl build of its own, alone): read-state passes, read-chars fails. This branch: read-state fails
  four times (alone, and once with the base's fake game), read-chars fails.
- read-state's second look must read the fake game's first world; its mark comes about 16.2 s after the round is ready,
  the fake game's retitle at frame 1630 (27.25 s). The base's run had 351 ms between the mark and the retitle; this
  branch's 134 ms, and its look read after the retitle (characters=0, fields_after_world=0). Every stage of the round
  runs about 50 ms later here (its first, `found`, took 489 to 571 ms against 436), and the launcher's poll of the block
  adds the rest (its first task 197 ms after `ready` against 64). For Lodestar: a later retitle (or a mark tied to the
  phase, not the clock) would give the case room; nothing of this job's touches the round, the launcher or en-gal.

- Later (2026-11-23): on the merged engine (8424f15, and Lodestar's fourteenth round at 0e94488) both read-state and
  read-chars passed in the full clang-cl suite under Wine: 148 cases of 148, the editor's and the GraceUI cases among
  them; the nine unit programs, which this tree's ctest cannot start without Wine, passed run by hand (WHY§210).

### 528. A test package's runs check the game's GraceUI against the package's own before the start

`2026-11-23` · DECIDED · `#engine` `#spike` `#launcher` · refs: O113, WHY§529, S10.1.g, engine 8424f15

- The owner's spikes.4 run B loaded an older GraceUI (0.2.24) from the game's folder than the package carried
  (0.2.27): the package's folder had not been copied in. The owner asked that test packages warn of it.
- The launcher's `--expect-graceui` names the package's own `GraceUI\graceui.toml`; before anything starts it is
  compared with the game's (version, ABI, every listed file's size and SHA-256), and E13 names what differs and asks
  whether to start anyway; closing it stops with exit code 23. Every run of the s0e-s4 package passes it; the S0 and
  Lodestar packages carry no GraceUI and do not. The harness checks the match in each package case and E13 in
  `launcher-package-spikes-other-graceui`.

### 529. spikes.4: A5 and B ran to their end; A1 still falls at GraceUI's first Present with EldenNet connected and no stand-in

`2026-11-23` · FACT · `#spike` `#graceui` `#engine` · refs: WHY§509, WHY§511, WHY§517, WHY§518, S10.1.g

- The owner's runs on 2026-11-23, another overlay loaded in each and Steam's off: A5 (GraceUI 0.2.27 alone, early)
  drew 44,867 frames and every task was done; B (0.2.24 loaded by EldenNet from the game's folder, after the window)
  ran all six tasks, and the walk into the menu behaved as it should (the character stopped when F5 opened it). Both
  ended at quit with 0xC0000374 (heap corruption), with Grace.
- A1 twice (0.2.27 loaded by the launcher before EldenNet, early): 0xC0000005 on a game thread, module unknown, offset
  0, 6 and 9 s in. The observer's lines: EldenNet connected and registered its space 280 ms after GraceUI started, the
  swap chain bound, then one Present, no frame, frame_step 2 (bound, no queue) until the fall. EldenNet itself stopped
  at its banner at 243 ms (discovery refused: the package's gal/ is not the owner's game's reader data), having
  connected. A5 differs only by EldenNet's absence; B by GraceUI's version and load point. With Grace.
- In B the pages' Home row read "2.7.2.0, supported" beside the banner "not supported yet (reason: gal_files)"
  (WHY§530). The typing test's pointer still rubber-banded (0.2.24 and 0.2.27 are older than D232), so the menu's size
  went untried; the owner has no pad.

- Later (2026-11-23, Grace's reading of the code): GraceUI calls no code of a mod's (the ABI has no callbacks), and
  between its steps 2 and 3 it does only its own atomics, log and events; A5 found the queue on its second Present, and
  in A1 no second Present came, so the first original Present below GraceUI never returned or the render thread stopped
  elsewhere. The 2.5 s gap is likely a crash handler's dump. 0.2.30's observer carries D231's fault recorder (with
  0xC0000374 added), so spikes.5 keeps A1 to name the caller. GraceUI does nothing at process exit (an empty DllMain,
  no exit-time destructors). The chat box's keyboard-only request leaves the mouse wholly to the game (O113).

### 530. A start stopped by its reader's files says "not supported yet" of a build Home calls supported

`2026-11-23` · FACT · `#engine` `#pages` `#words` · refs: WHY§529, PAGES§4.5, S11.4.a

- `banner_text` gives every stop at discovery the unknown build's words, "game build X not supported yet (reason:
  gal_files)", while Home's Game build row reads the build table alone ("X, supported"). On the owner's PC both showed
  at once: the build is in the table, and the package's gal/ held no reader data for it.
- To fix in words the owner approves: a stop at discovery names what failed (EldenNet's files for this build, or its
  reading of it, not complete yet) without "not supported", and the build row says supported only once the start has
  read the game.

### 531. The moving numbers are sought by invariant on the player's parts, counted by what the tester is doing

`2026-11-23` · METHOD · `#lodestar` `#gal` · refs: docs/ops/probes/2026-11-22-l5.md ("What was changed for the fourteenth round"), WHY§460, WHY§503, engine 6d10b11

- Every 32-bit field of the objects the player's parts holder points at is compared a second apart, and its changes
  are counted by the tester's phase: a position changes in the walk alone; health falls at a hit and rises at a heal,
  with a steady number at least as large beside it. Only places and behaviour are reported, never values. The owner
  agreed to the reading and to the hit and the heal (Lodestar's fourteenth round, package 0.1.0-lodestar.18).

### 532. A waiting task of the tester's panel may carry a mark: the end of its time is stamped as one

`2026-11-23` · METHOD · `#lodestar` `#launcher` · refs: docs/ops/probes/2026-11-22-l5.md, WHY§444, engine 6d10b11

- So a round's phases that need no key (standing, walking) are bounded without the tester pressing anything. A list
  with no marked wait behaves as before; none of the S0, spike or observer lists marks one.

### 533. A holder that returns after an Alt+Tab waits for GraceUI's next frames before it asks for input again

`2026-11-23` · FACT · `#graceui` `#input` · refs: GraceUI:D233, G40, S11.2.aq

- GraceUI grants input only once it has drawn within 2 s, and a game, or the harness under Wine, may not present
  while in the background, so a request at WM_SETFOCUS is refused. The menu's return (G40) waits for the next frames,
  at most 5 s, and asks then; refused, late or a game menu open, it closes; danger or loading while away closes it at
  once. A mod's own holder still ends at the loss of focus (GraceUI 0.2.30).

### 534. Far fields are built as the ruling says: a copy of their own, into the object's buffer past the class's copy

`2026-11-23` · METHOD · `#lodestar` `#gal` · refs: WHY§506, ARCH§6.3, en-gal's README, engine 5024ee9

- The layout marks a field far; the reader fetches it by a guarded copy of its own into the object's buffer, past
  the class's copy at a multiple of eight; the object view finds it there, so the groups read a far field as any
  other. The characters manager's main player, near list and player set are in the build's layout as far leads
  (copy_size 0, no rtti). dlmap stays near (the flags' store keeps its map 0x38 in), and the 32-byte cap stands.

### 535. A test's port belongs in the test's environment, never the player's settings

`2026-11-23` · METHOD · `#graceui` `#tests` · refs: GraceUI:D234, WHY§526, S11.2.ar

- Writing a port into GraceUI's settings.json made two trees collide and could leave a test's port in a real file.
  Port 0 handed to the server was rejected too, since it would change the players' port rules for tests alone. The
  test build reads GRACEUI_TEST_PORT (free, or a port from 1024 to 65535), asks the system for a free port and binds
  it, asking again up to five times if another program wins the gap; a release build reads nothing of it (GraceUI
  0.2.31). EldenNet's cases are to set it and read the port from GraceUI_Test_GetServer, and the editor's case to stop
  writing one (GraceUI's docs/eldennet-changes.md §64).

### 536. A read round ran with GraceUI loaded: EldenNet took the Marks, and the owner's Pause presses became the round's marks

`2026-11-23` · FACT · `#lodestar` `#graceui` · refs: docs/ops/probes/2026-11-22-l5.md ("The fourteenth round in game: spoilt by GraceUI's presence"), WHY§444, S10.1.m

- Since the spike runs a GraceUI folder lay in the game's folder, so the fourteenth round's EldenNet connected to it
  (0.2.24), took the ring of Marks, and the launcher released its Pause key: the owner's 18 presses were stamped as
  Marks by EldenNet and the round took the first three as its moments, all inside the first world. Its searches looked
  at the wrong moments; the panel's own confirmations stamped nothing. The game ended with 0xC0000005 in ntdll at the
  quit, 590 s in; the seven read rounds without GraceUI before it ended clean (WHY§529, with Grace).

### 537. A read round is free of GraceUI by construction, and the panel's confirmations reach the round whoever holds the Pause key

`2026-11-23` · METHOD · `#lodestar` `#engine` · refs: WHY§536, WHY§444

- Not by a folder happening to be absent: a read round's start skips the connection to GraceUI (boot.cpp, where the
  probe's round is the read round and the run is not an observer's), so EldenNet never asks for the ring and GraceUI
  is never loaded; the launcher's --read says so on the console when a GraceUI folder lies in the game's folder (by
  Lodestar, Engine's agreement of 2026-11-23).

### 538. Objects of the player's class are several in a world: the main player is the object at characters.main_player

`2026-11-23` · FACT · `#lodestar` `#gal` · refs: WHY§536, WHY§502

- In one look in the first world eight different objects of the player's class stood in the manager's words (17
  words), most in a run of 16-byte elements 69,824 bytes in that held 538 characters' addresses. So "the object of the
  player's class" names no one object; the parts watch is to take the main player from the layout's
  characters.main_player. 0x1E508 held a player-class object with a third character too.

### 539. The quit's crash codes look like a damaged heap found at the quit, not damage done there

`2026-11-23` · LIKELY · `#graceui` `#crash` · refs: WHY§536, WHY§529, GraceUI:D231, GraceUI:D118

- graceui.dll runs nothing at process exit (DllMain empty; no static or thread-local destructor, checked on its
  compiled objects; its state never freed; its three threads hold only its own locks), and EldenNet's destructors free
  nothing of GraceUI's. An access violation inside ntdll and 0xC0000374 are the heap manager meeting damage done
  earlier, at the first moment many blocks are freed at once. If GraceUI is the cause, the write happened during the
  run, perhaps with a third program's hook beside it. What would place it: spikes.5's recorder at 0xC0000374, the
  faulting frames by module and offset, a page-heap run, and a heap guard on GraceUI's own allocations in its test
  build (asked of Grace for spikes.6 the same day).

### 540. ABI 1.6 grows GraceUiEvent: EldenNet's poll is asked at the larger size, and nothing else changes

`2026-11-23` · FACT · `#engine` `#graceui` · refs: GraceUI's D226, de8b596 (graceui.h), graceui.h's rules (D73), src/en-overlay/tests/fake_grace.cpp (poll), engine wip/s12-pin, merged at 4bfccf9

- graceui.h at 386d1ba appends `file`, `file_capacity` and `file_length` to GraceUiEvent (ABI 1.6), so
  `sizeof(GraceUiEvent)`, which graceui-sys's poll sets as the event's size, grows with the re-vendor. GraceUI reads
  and writes the smaller of the two sizes (D73), so the floor (0.2.22, ABI 1.5) still answers; the space names no file
  command, so GraceUI never writes a file into EldenNet's event (its `file` stays null). The table itself has no new
  slot. The fake GraceUI compares against the header's size and so needs no change while EldenNet polls only at 16
  bytes or the header's size; one test comment ("ABI 1.2's size") was corrected to ABI 1.6's.

### 541. A free port for every GraceUI case, from one environment of its own

`2026-11-23` · METHOD · `#engine` `#tests` `#graceui` · refs: GraceUI's D234, WHY§526, WHY§535, tests/fakegame/harness.py (GRACE_ENV, install_editor), engine wip/s12-pin, merged at 4bfccf9

- `GRACE_ENV = dict(EN_ENV, GRACEUI_TEST_PORT="free")`, used by the eleven cases that need GraceUI's test build and by
  the spikes package's six runs, which carry one; EN_ENV itself is left alone, since most of its cases load no GraceUI.
  The editor's case no longer writes `dashboard.port` (only `dashboard.lan`). Every case already read the port from the
  fake game's report, so nothing else assumed 8642. A GraceUI older than 0.2.31 ignores the variable.

### 542. grace-dev 0.2.31's dump: only the two Named NPCs goldens change

`2026-11-23` · FACT · `#engine` `#pages` `#graceui` · refs: WHY§524, GraceUI's 6a50f1d (D234), tests/pages/golden, engine wip/s12-pin, merged at 4bfccf9

- Regenerated with grace-dev at 386d1ba: `characters.named` and `characters.named.world` now draw the chosen row's
  general profile (five rows) and the editor block in place of the two "Nothing to show yet." lines (62 to 124 items);
  the world state's Draft is greyed with "Drafts wait until EldenNet may use the network.", as its fixture says.
  `characters.named.starting` and the other twelve goldens are byte for byte the same, and the fixtures needed no
  rewrite.
