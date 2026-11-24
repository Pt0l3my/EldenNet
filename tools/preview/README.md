# Preview shots

`shoot.py` takes screenshots of EldenNet in GraceUI for the showcase: the frame as presented, on a backdrop of our own,
with nothing of a game's in it. It drives the fake game (`tests/fakegame/`) with the launcher, `eldennet.dll` and a
test build of GraceUI, through the fake game's harness (`tests/fakegame/harness.py`), as the harness's own GraceUI cases
do, and shoots each state once its fades have settled:

| Shot | Run | What it shows |
|---|---|---|
| `hint` | talk | the talk hint, before T is pressed |
| `chat` | talk | the chat box open with a draft typed |
| `lower-third` | talk | the reply's page in the lower third, under the speaker's name |
| `f11` | events | F11's view on Augur's events |
| `menu-mouse` | pages | F5's menu on EldenNet's Home as a mouse player sees it: no focus ring (GraceUI's G42) |
| `menu-ring` | pages | the same after a key press: the gold focus ring |
| `graceui` | pages | one of GraceUI's own pages, reached through F5's rail |

The talk run is `launcher-eldennet-graceui-talk`'s (the test persona, the fake model server's scripted reply, the line
the case types), the events run `launcher-eldennet-graceui-events`' and the pages run `launcher-eldennet-graceui-pages`'
title screen with presses of its own; only the cases' invented names and words appear. The fake game runs with
`--d3d12 --backdrop --swap-size 1920x1080 --swap-format 24` (R10G10B10A2_UNORM), `--shot-at` and `--cursor-once` (its
README: the cursor put in the window once, or the ring would go again after each key under Wine), and a check run
shoots the box again at format 28 (R8G8B8A8_UNORM) and compares the two.

```sh
python3 -I tools/preview/shoot.py --build build-cl --out <a folder outside the repository> --webp <another>
```

- `--build` is a clang-cl tree configured with `EN_GRACEUI_DIR` (BUILD.md, the fake game's GraceUI cases): the script
  reads the harness's command for those cases from CTest (`ctest --show-only=json-v1`), and makes the tree's Wine
  prefix first if it has none, as the harness's first test does.
- `--out` gets one BMP per shot (`chat.bmp`, …, and `chat-format28.bmp`) and each run's folder (`runs/`); it is refused
  inside the repository. `--webp` writes `preview-NAME.webp` at 1280 by 720 (Pillow, LANCZOS), lossless, or at
  quality 90 where lossless passes 400 KB, and says which.
- `--size`, `--format` and `--check-format` (`none` for no check run) change the swap chain; `--only talk,pages`
  makes some runs alone; `--work` names the harness's work folder (by default `tests/fakegame/preview` in the tree).
- Each run checks that every shot was written at the size and format asked, the box's input held where the box is
  shot, the menu open on the page meant where it is shot (no ring for `menu-mouse`, the ring for `menu-ring`), and the
  launcher's clean end, and prints GraceUI's state at each shot.
- The shots are not added to the repository here; they belong in the showcase's own assets.
