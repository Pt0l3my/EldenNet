Request from the EldenNet design session: a local job runner with a browser page

For the session that builds the owner's local model-server tools, through the owner. Please check the plan with the owner before building it.

## What the owner wants

A tool that runs entirely on the owner's machine and that the owner alone starts and stops. They drop files holding
any number of prompts onto a page in their browser; the tool runs them on the three llama-servers they already run
(three server slots, named in the owner's own settings, the same ones as before); it writes output files the owner
can download and share. No GitHub, no tunnel, no inbound connection, nothing that pulls work from anywhere: work
arrives only when the owner drops a file. It can be a plugin of the owner's model-server manager or a small
standalone Python app, whichever suits that manager; the behaviour below is the same either way.

## The page (browser, mouse first)

- Served by the tool on 127.0.0.1 only, at a random port or one the owner sets, opened in the default browser at start.
  It works offline: no CDN, no remote fonts or scripts, a strict Content-Security-Policy.
- **Drop zone** for one or more job files, with a file picker as a fallback. Each file is validated on drop and shows
  its item count, target server and any errors before anything runs.
- **Queue:** one row per file and per item (queued, running, done, failed, skipped), with start, pause, resume, cancel
  and remove. One request at a time per server (each server has one slot).
- **Live view** of the reply being generated (streamed), with its tokens per second.
- **Servers panel:** each server's model name (file name only), context size, slot state (idle or busy), and whether it
  answers. A "pause while I play" switch, and a skip-if-busy check before every request, so a job never takes a slot
  the game is using; a busy server makes the queue wait, it does not fail the job.
- **Results:** a table per finished file (id, status, first-token time, total time, tokens, a preview), and a
  **Download** button that saves one zip per job file. An "Open output folder" button for the owner's own use.
- **Settings:** server addresses (stored in the tool's own local config, never in a job or an output file), default
  sampling values, the caps below, and the output folder.

## The job file ("jobpack", version 1)

A JSON file (or JSON Lines, one item per line, with an optional first line holding the header):

```json
{"format": "jobpack/1", "name": "c14-probes-kale", "server": "<server>",
 "defaults": {"max_tokens": 256, "temperature": 0.8, "thinking": false},
 "items": [
   {"id": "p001", "system": "...", "prompt": "..."},
   {"id": "p002", "messages": [{"role": "system", "content": "..."}, {"role": "user", "content": "..."}],
    "max_tokens": 120, "response_format": {"type": "json_object"}}
 ]}
```

- `server`: one of the server aliases set in the tool's Settings (per file, or per item to override).
- Per item: `id` (1 to 64 characters from A-Z a-z 0-9 _ -, unique in the file), and either `prompt` (with an optional
  `system`) or `messages` (roles system, user, assistant). Optional: `max_tokens`, `temperature`, `top_p`, `top_k`,
  `min_p`, `seed`, `stop` (up to 4 strings), `thinking`, `response_format` (json_object or a json_schema of at most
  16 KB), and `repeat` (run the same item N times with seeds seed, seed+1, …, for rate measurements).
- **Images** only for the slot that takes images, only as `data:image/png` or `data:image/jpeg` base64 URLs, with the
  bytes checked (magic number, width and height read from the header, at most 4096×4096 and 8 MB) before the server
  sees them.
- Any other field is ignored and reported. The file is data only: nothing in it is ever executed, evaluated or used
  as a template.
- **Caps** (settings, with these defaults): 2 MB per file, 2,000 items, `max_tokens` at most 2048, 300 s per request.

## The output (one zip per job file)

- `results.jsonl`: one line per item run: `id`, `run` (for repeats), `ok`, `reply`, `reasoning` (if the model
  returned any), `finish_reason`, `usage`, `timings` (the server's own prompt and generation figures), `first_token_s`
  and `total_s` as measured by the tool, `model` (the model file's name only), `server` (the alias), and `error`
  when it failed. Optionally the item's own messages, so the file stands alone for review (on by default).
- `summary.json`: the jobpack's name and a hash of the file, counts by status, latency figures (median, 90th
  percentile, maximum) per server, the model names, the tool's version, and start and end times in UTC.
- **Nothing local in any output file:** no server addresses, ports, IPs, file paths, machine or user names, and no
  `cache_n` or other figures that reveal what the game had cached. The owner should be able to upload the zip anywhere.

## Robustness (lessons from reviewing the earlier worker)

- Validate every item on its own; a bad item fails alone with a clear error, and nothing in a file can crash the tool.
  Check types (a server name must be a string), JSON nesting depth (at most 32), and unpaired surrogates in text.
- Catch every network error per request, retry with backoff (a few times), and keep going; a server that stays down
  pauses its part of the queue and says so on the page.
- Write each result as soon as it arrives, so a stopped run resumes where it left off when the owner presses resume or
  drops the same file again (matched by the file's hash).
- The page's own API accepts requests only from the page: check the Host and Origin headers and a random per-start
  token, so other local programs or websites cannot drive it.
- Logs hold no prompt or reply text, only ids, statuses and timings.

## How the EldenNet session would use it

The session writes jobpacks (probe sets for world awareness and spoilers, latency checks, draft line pools) and sends
them to the owner as files. The owner drops them on the page, downloads the result zips and uploads them back to the
session, which reads them as data. Nothing connects the two machines.

Please send back to the owner: the tool's name, how to start it, a screenshot of the page, and a sample result zip from
a two-item test jobpack.
