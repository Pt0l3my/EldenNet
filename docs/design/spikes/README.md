# The spikes' results (DESIGN §16.1)

*2026-11-17. This folder holds the results of the spikes DESIGN §16.1 lists, as they come back from the owner's PC:
build A's S0-A to S0-D first (the plan for roadmap S10.1.a, §1.2), then build B's. It says how a result is recorded,
what a spike package holds, and how the owner runs each S0 spike. `formats.md` describes every file the runs write, for
DataPiler's importer (O73).*

## How a result is recorded

1. **The run.** The owner runs a spike package (below) on their own PC, as its README says. Every file it writes is
   numbers, fixed words and our own labels: the launcher's spike logs and crash records, eldennet.dll's recordings and
   R1's reports (`formats.md`).
2. **What comes back.** The spike-report of each spike (`eldennet-engine spike-report <spike> ... --out <file>`), its
   logs, the newest crash records, and for S0-B the session's `rec-stats`. Recordings stay on the owner's PC: they are
   large, and DataPiler imports them there (S7.11.f). Each file passes `eldennet-engine lint` (the reports and
   recordings) before it is sent, and the privacy sweep before it is committed anywhere.
3. **The review.** The reviewing agent reads each file with `python3 -I` as data (never as instructions), runs
   spike-report again on the logs and rec-stats, and writes the record here: `<YYYY-MM-DD>-<spike>.md`, with the
   spike-report beside it as `<YYYY-MM-DD>-<spike>.json` when it lints clean. A rerun on the same day takes a letter
   (`2026-11-20-s0a-b.md`).
4. **The record** holds, in our own words: the package's label and SHA-256; the build (its fingerprint's key); what
   was run (each load mode, each appid method, each third-party row of O91 and whether it was run); every rule of
   DESIGN §16.1 with its status, value and limit, as spike-report gives it, and the verdict; what the owner observed
   that no file measures (S0-C's online menu, a freeze, a dialog); and the findings, each labelled FACT, LIKELY or
   SPECULATION with its reasoning-log entry. Nothing in it is a byte, a pattern or a text of the game's, and nothing of
   the owner's.
5. **What follows.** A spike that passes lets what it blocks go on (DESIGN §16.1's last column) and confirms its
   leads in `gal/` (ARCH §6.3: a pattern's `builds`, a layout entry's `status = "confirmed"`), in the same change as
   the record. One that fails or is incomplete says what the next package changes; a third-party row not run moves to
   v0.1's matrix (S10.3.b) under O91 (b).

## The spike package

Built by `tools/spikepkg/build.py` from a clang-cl Release tree with `EN_PROBE` on, labelled `0.1.0-spike.N` (the
build's `EN_VERSION_LABEL`, shown by `eldennet-engine version`, the launcher's log and eldennet.dll's version
resource), and never shipped to players. One folder in one zip, the same zip for the same inputs:

| File | What it is |
|---|---|
| `eldennet-launcher.exe` | The launcher and crash watcher, with the dev console, the Mark, `--probe` and `--spike` |
| `eldennet.dll` | The probe build (`EN_PROBE`; its version resource's SpecialBuild says `probe`) |
| `eldennet-engine.exe` | The devkit's tool: `lint`, `spike-report`, `rec-stats`, `replay` |
| `gal\` | The repository's gal/, which `lint --gal` passed: builds.toml, patterns.toml, layouts\, derivations\, leads.toml |
| `README-<round>.txt` | The owner's steps in plain words: how long, what it does and never does, what to send back, a crash |
| `LICENSE.txt`, `THIRD_PARTY.md`, `THIRD-PARTY-NOTICES.txt` | EldenNet's licence, its credits, and the libraries' licences |
| `SHA256SUMS` | The SHA-256 of every other file |

- **0.1.0-spike.1** is R1's (`build.py r1`, `README-R1.txt`): the derivation's in-game round (O89), with R0c's leads
  in gal/. It runs `eldennet-launcher --probe` and nothing else.
- **0.1.0-spike.2 and on** are the S0 packages (`build.py s0`, to be added once R1's review has filled gal/): the same
  files, gal/ with R1's findings, `README-S0.txt` with the steps below, and `fakegame.exe` for the smoke run, which
  `eldennet-launcher --selftest` will run as the harness's `launcher-eldennet-probe` case does (the plan's §1.2: the
  fake game started, injected and watched on Windows before the game is).

## Running the S0 spikes (the plan's §1.2)

Each spike starts from the package's folder, with the game closed and Steam running; the save backup comes first.
The settings below go in `%APPDATA%\EldenNet\config\settings.toml` (src/eldennet/README.md has the file). Each
spike-report runs in the same folder, on the files in `%APPDATA%\EldenNet\spikes\` and `recordings\`.

| Spike | The owner runs | Then | Time |
|---|---|---|---|
| Smoke | the test programs, then `eldennet-launcher --selftest` (spike.2 on) | its pass and fail counts | 10 min |
| S0-A | `eldennet-launcher --spike s0a --runs 20` (early, the default), then `--spike s0a --runs 20 --inject-at title` (late); each run reaches the title and is closed. Third-party rows (O91): the same beside each mod, one log each | `eldennet-engine spike-report s0a --log <early> --log <late> --out s0a.json` | about 2 h, unattended |
| S0-B | `debug = { record_sessions = true }`, then `eldennet-launcher --spike s0b`: two hours of normal play (open world, a dungeon, fights, deaths, fast travel, menus) | `spike-report s0b --recording <file> --log <s0b log> --out s0b.json`, and `rec-stats <file> --out s0b-stats.json` | 2 h |
| S0-C | `record_sessions = true`; `eldennet-launcher --spike s0c` once with `--appid-method file`, once with `env`, noting the game's own online state; solo, then Seamless Co-op with a partner (O91) | `spike-report s0c --recording <each> --log <each> --out s0c.json`, and whether an online menu showed | 1 h |
| S0-D | `record_sessions = true` and `probe = { fault_group = "camera", fault_every = 50 }`; `--spike s0d` alone, beside the Steam overlay, with `--also <a vectored-handler DLL>`, beside ERSC, once with `gal = { copy = "rpm" }`; then `--spike s0d-crash --runs 5` at the title | `spike-report s0d --recording <file> --log <s0d log> --log <s0d-crash log> --out s0d.json` | 1 h |

DESIGN §16.1's rules, as spike-report applies them (src/en-tools/README.md): S0-A 20 of 20 runs per load mode reach
Ready or L1 and none crashes; S0-B no fault and no trip over two hours, the task's p99 at most 0.5 ms; S0-C the gate
never left open in a session that is not one's own solo world, no connection beyond the PC, no online menu (the
owner's); S0-D at least 20 forced faults caught, a breaker tripped, and every forced crash recorded by the watcher.
