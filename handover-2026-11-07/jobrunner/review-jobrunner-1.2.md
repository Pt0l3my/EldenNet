# Review of LLM Jobrunner 1.2 (read-only), for the session that builds it

From the EldenNet design session, through the owner, 2026-11-07. Nothing in the bundle was run, imported or built; all 15 files match manifest.sha256.txt. Three reviewers (security; files and privacy; robustness and the request), each checked by a skeptic. 37 findings were raised and 27 confirmed; duplicates are merged below (23 items). Line numbers are llm_jobrunner.py (P) unless a file is named.

## Verdict

No path from a jobpack, a server reply or a website to code on the PC was found: the page listens on 127.0.0.1 only, checks Host and Origin, requires the per-start token on /api/, sends no CORS headers and a strict CSP, makes no remote requests, and uses no eval, exec, pickle, shell or dynamic import. Outputs leave out cache figures and the model-server manager's server labels. One privacy defect breaks the request (item 1), so until it is fixed the owner should open a result zip and check failed items before sharing it. The rest are robustness fixes.

## High

### 1. The server's address and port end up in result errors, so they land in results.jsonl, the shared zip and run.log. (also S-2)

- **Where:** llm_jobrunner.py, lines 591
- **Problem:** The server's address and port end up in result errors, so they land in results.jsonl, the shared zip and run.log. This breaks the spec's rule of no addresses, ports or IPs in any output, and the README's promise at lines 85 and 96.
- **Evidence:** - down_words (591-599) builds at = "%s:%d" % (self.host(), port) into every refused or timeout message.
- execute() passes it to again() when the chat request fails to connect or times out before the headers arrive (777; socket timeout is min(timeout_s,120) at 767).
- On the sixth fault, again() fails the run with "it met %d faults: %s" % (attempts, why) (826), so r["error"] holds e.g. "<AI PC LAN IP>:<port> did not answer within 5 s ..." or "nothing answers at <IP>:<port> - the model-server manager on the AI PC ...".
- result_line copies r["error"] (861-862) into results.jsonl (871) and into the zip (1166).
- run.log gets "port %d %s#%d tried again: %s" with the same text (828).
- In local mode the same lines show 127.0.0.1:<port>.
- Also, "within 5 s" is wrong for this call.
- **Fix:** Build again()'s reason without host or port: 'refused', 'no answer within N s' (with N the real timeout), or 'the connection broke'. Keep down_words' text for srv['error'] (the card) only. Pass every error through said() in result_line as a backstop. Log the jobpack server name instead of the port at 828. Add a test that makes one run fail six times (refused, and a header timeout), then greps results.jsonl and the zip for the host and ':<port>'.

## Medium

### 2. When a stream ends early, the run either counts as a success or the port's worker thread dies.

- **Where:** llm_jobrunner.py, lines 778-819 (EOF handling); 813 (except)
- **Problem:** When a stream ends early, the run either counts as a success or the port's worker thread dies. If the cut falls inside a chunk, or the body is close-delimited or length-delimited, the loop just ends and the run is saved as ok:true with a partial reply and finish_reason null. A 200 answer with no data: lines is saved as an empty success. If the cut falls between chunks (the normal case for llama-server's chunked SSE when the server is stopped or restarted mid-reply), http.client.IncompleteRead is raised. Line 813 does not catch it, so it escapes execute() and the worker's try/finally (740-748).
- **Evidence:** Lines 789-790 break on [DONE], but reaching EOF also falls through to lines 815-819, which call done(j, r, True, ...). My own stdlib check (cscripts/stream_cut.py, no bundle code) found: 'chunked, cut after a chunk' raises IncompleteRead, which (OSError, ValueError) does not catch; 'cut inside a chunk', 'length, cut short' and 'no length, close' end the iteration with no exception.
- **Fix:** Set saw_end on [DONE] or on a non-null finish_reason. After the loop, if not saw_end, return self.again(port, j, r, "the stream ended early"). Add http.client.HTTPException to the except tuples at 776 and 813, and AttributeError and TypeError at 813 (see C-2). The Content-Type check is optional.

### 3. The monitor and worker threads have no catch-all, so one unexpected exception stops them for the rest of the session.
- monitor() is started once and nothing restarts it. (also S-7)

- **Where:** llm_jobrunner.py, lines 633-655, 662-670, 712-717, 740-748, 795-808, 870-881
- **Problem:** The monitor and worker threads have no catch-all, so one unexpected exception stops them for the rest of the session.
- monitor() is started once and nothing restarts it. If it dies, server cards freeze, dead workers are never restarted, and a 'down' flag a worker set is never cleared, so that server's queue waits 'down' forever.
- If a worker dies mid-request, its run stays 'running'. The job then never finishes, Cancel has no effect and Remove is refused.
- A lone surrogate in a reply also makes /api/job, /api/live, /api/payload and the zip fail for that job.
- **Evidence:** - probe catches only HTTPError, (OSError, ValueError) and (OSError, ValueError, AttributeError, IndexError). BadStatusLine (non-HTTP bytes on the port) and IncompleteRead get through, as do KeyError/TypeError at line 648 when /v1/models 'data' is an object or a number.
- monitor (662-670) has no try and is started only at line 1436. Workers are restarted only by sync_servers (583-586). 'down' is cleared only by probe (659).
- In the worker, the busy check at 714 catches only OSError/ValueError/TypeError, and _open at 768-777 does not catch HTTPException.
- The SSE parser assumes types: ev.get at 795, ch.get at 799, d.get at 800, and "".join(text) at 808 fails on non-string content.
- write_result (870-881) catches only OSError. My check found that a lone surrogate from json.loads('"\\ud83d"') makes a UTF-8 text write raise UnicodeEncodeError, which is a ValueError.
- Once a run is set 'running' (734), finish_check at 894 never completes the job, cancel only sets abort (1017), which nothing reads any more, and remove is refused (1020-1021).
- send() (1251) and zip_bytes (1187-1191) encode with ensure_ascii=False and so fail on a lone surrogate.
- Minor trigger: /api/settings calls App.cfg.clear() and then update() (1373-1374) on the dict that threads read without the lock (550, 553, 570, 675, 765), so a KeyError in that window kills them too.
- **Fix:** As proposed: wrap sync_servers() and each probe() in monitor in try/except Exception, recording said(e) on the card. Wrap the worker loop body in try/except Exception, and if r is 'running', requeue it through again() or fail it with the reason. Add HTTPException to the network tuples (636-639, 714, 776, 813). Type-check ev, choices, delta and content in the SSE loop. Write files and pages with ensure_ascii=True, or replace lone surrogates on arrival. Swap settings with R.cfg = App.cfg = s rather than clear() then update(), and have readers use self.cfg.

### 4. The server has no Fetch Metadata check.

- **Where:** llm_jobrunner.py, lines 1244, 1269-1278, 1315-1322
- **Problem:** The server has no Fetch Metadata check. A refused request leaves its unread body on a keep-alive connection, where it is parsed as the next request. The handler has no socket timeout, and the page sends no COOP header.
- **Evidence:** There is no Sec-Fetch-Site check anywhere (grep). `protocol_version = "HTTP/1.1"` (P:1244), and guard returns 403 (P:1283, P:1313) without reading the body or setting close_connection. The same holds for the 413 and 400 at P:1319-1322. The token still protects /api/*, so nothing leaks today, but this is a request-desync primitive. With no `timeout` on Handler, an idle connection holds a thread forever.
- **Fix:** The essential part: set self.close_connection = True before every refusal in guard and in the length checks, and refuse any request that has Transfer-Encoding or lacks Content-Length on POST. Alternatively, read and discard the body up to the cap. Add timeout = 30 to Handler. Sec-Fetch-Site and COOP are optional extras.

## Low

### 5. Settings changes and restarts can strand or drop jobs.
- If a port leaves Settings, or Servers on another PC is turned on (the manager's slot names stop resolving), that port's queued runs stay queued forever.

- **Where:** llm_jobrunner.py, lines 553, 573-575, 705-706, 951-953
- **Problem:** Settings changes and restarts can strand or drop jobs.
- If a port leaves Settings, or Servers on another PC is turned on (the manager's slot names stop resolving), that port's queued runs stay queued forever. Its worker has exited, and the page gives no reason.
- At start, a folder whose jobpack now fails a file-level check (header server unknown, a lower file cap) is skipped silently. Its results disappear from the page, and dropping the same file again starts it from scratch in a '(2)' folder instead of resuming by hash.
- **Evidence:** - sync_servers ignores the manager's slots in remote mode (553) and deletes ports that are no longer wanted (573-575).
- worker returns when its port is gone (705-706).
- load_folder does 'except (OSError, ValueError, Bad, KeyError): continue' (952-953).
- **Fix:** After a settings change, re-resolve queued runs' ports. Runs that no longer resolve show 'its server is no longer set'. At start, load folders that fail file-level checks as paused jobs with the reason, keyed by sha so a re-drop matches them.

### 6. Unpaired surrogates are refused in the jobpack but not in the dropped file's name.

- **Where:** llm_jobrunner.py, lines 1330, 916, 924-925, 1251
- **Problem:** Unpaired surrogates are refused in the jobpack but not in the dropped file's name. If the jobpack has its own name, a lone surrogate in the file name goes into title and file. On Windows the folder is still created, then every /api/state answer fails to encode and the page stops updating. save_state keeps the title, so the page stays broken after a restart until the folder is deleted by hand.
- **Evidence:** - _text_ok is applied only to the header and items (420, 459).
- fname comes straight from the POST body's 'name' (1330).
- send() does json.dumps(ensure_ascii=False).encode('utf-8') (1251). The UnicodeEncodeError is caught as a 400 at 1306, and the page's poll swallows errors.
- **Fix:** Before job_title, replace any code point that fails .encode('utf-8') in fname with U+FFFD, or refuse the drop.

### 7. Some type errors in settings and POST handlers are not caught.
- If defaults or caps is not an object, settings_check raises AttributeError; a config file that is a list raises TypeError.

- **Where:** llm_jobrunner.py, lines 83, 119-129, 147, 1324, 1345
- **Problem:** Some type errors in settings and POST handlers are not caught.
- If defaults or caps is not an object, settings_check raises AttributeError; a config file that is a list raises TypeError. settings_load does not catch either, so a hand-edited config stops the tool at start.
- defaults.max_tokens = true passes the check.
- A deeply nested POST body (RecursionError) and /api/nowait with a non-number port (TypeError) get no answer. Only the page can send these.
- **Evidence:** - Lines 120 and 128 call d.get and c.get without checking the type, and 120 has no bool check.
- Line 83 runs dict(s or {}).
- Line 147 catches only (ValueError, OSError, Bad).
- My check: json.loads of a 100000-deep array raises RecursionError, which is not a ValueError.
- The POST excepts (1379-1382) catch only Bad, ValueError, binascii.Error and OSError.
- **Fix:** In settings_check, require isinstance(d, dict) and isinstance(c, dict) and exclude bool for the default max_tokens. In settings_load, catch Exception, log it to the start log and fall back to the defaults. Optionally add RecursionError and TypeError to the POST excepts (1379).

### 8. A refused duplicate id gets the same (id, run) key as the original item.

- **Where:** llm_jobrunner.py, lines 448, 455-457, 927-930, 961, 966-972
- **Problem:** A refused duplicate id gets the same (id, run) key as the original item. Its refusal is written to results.jsonl as soon as the file is dropped. After a restart, resume matches results by (id, run), so:
- if the original had not run yet, it is restored as failed ('its id is used before in this file') and never runs;
- if it had run, the duplicate is restored as done with the original's reply.
Either way the zip carries two lines with the same id and run. An item with a bad id falls back to 'item-N', which can likewise collide with a real id.
- **Evidence:** - Line 455 sets r["id"] = it["id"] before the duplicate check at 456-457.
- add() writes refused runs to results.jsonl immediately (927-930).
- load_folder keys results by (o["id"], o["run"]) (961) and restores every run whose key matches (966-972).
- zip_bytes writes one line per done or failed run (1165-1166).
- **Fix:** Add the 1-based item position to every run and result line and key resume on (item, run). Alternatively, give refused duplicates and bad-id items an id outside ID_RX (for example '#2') so they cannot collide.

### 9. After a restart, finished runs in the zip report the parameters of the re-parse, not the ones actually sent.

- **Where:** llm_jobrunner.py, lines 951, 971-972, 858
- **Problem:** After a restart, finished runs in the zip report the parameters of the re-parse, not the ones actually sent. load_folder re-parses the jobpack with the current Settings defaults and restores only reply, reasoning, finish_reason, usage, timings, times and model. If the owner changed the default max_tokens, temperature or thinking between sessions, the zip shows params those runs were not run with.
- **Evidence:** - load_folder calls parse_jobpack(raw, ..., self.cfg, ...) at 951.
- r["res"] is rebuilt from only 8 fields (971-972).
- result_line writes "params": r["params"] (858).
- **Fix:** When a stored line is found, set r['params'] = o.get('params', r['params']). Leave messages as re-parsed.

### 10. timeout_s is not honoured before the first byte, and a slow prompt is resent.

- **Where:** llm_jobrunner.py, lines 767, 776-777, 598-599
- **Problem:** timeout_s is not honoured before the first byte, and a slow prompt is resent. The chat request's socket timeout is min(timeout_s, 120). A reply whose headers take longer than 120 s (llama-server sends them only after the first result) raises TimeoutError, which is handled as a network fault. The request is resent up to 6 times, the server repeats its prompt processing each time, and the card says it 'did not answer within 5 s'. When timeout_s is below 120, the cap is not applied before the first byte either.
- **Evidence:** - Line 767 opens the chat request with timeout=min(timeout, 120).
- Line 777 sends OSError (TimeoutError included) to again().
- again() allows up to 6 attempts (825).
- down_words returns 'did not answer within 5 s' for any timeout (598-599).
- **Fix:** Pass timeout_s as the chat request's socket timeout. Once the request has been sent, treat a read timeout as 'stopped at N s' (no retry). Have down_words report the timeout actually used.

### 11. 'Pause while I play' only stops new requests.

- **Where:** llm_jobrunner.py, lines 681-682, 1334-1338
- **Problem:** 'Pause while I play' only stops new requests. A request started a moment before the switch keeps the server's single slot for up to timeout_s (300 s), and the game's lines wait behind it. The request asks that a job never takes a slot the game is using.
- **Evidence:** - gate() returns 'paused while you play' only for the next pick (681-682).
- /api/play just sets play_pause (1334-1338).
- Nothing aborts the request in flight. README line 55 says 'stops new requests'.
- **Fix:** On switch-on, mark in-flight runs for requeue. In execute, check the mark on each line (and close resp from the /api/play handler to cover prompt processing), then return the run to 'queued' without counting an attempt. Optionally add a Settings choice to let it finish.

### 12. Not yet confirmed (check on the owner's llama-server build): usage is probably empty in results.jsonl and in the page's tokens column.

- **Where:** llm_jobrunner.py, lines 362-372
- **Problem:** Not yet confirmed (check on the owner's llama-server build): usage is probably empty in results.jsonl and in the page's tokens column. Recent llama-server builds send the streamed usage chunk only when the request sets stream_options.include_usage, and body_for never sets it. The sample zip's usage came from stand-in servers, so it does not show this either way.
- **Evidence:** body_for builds messages, stream, cache_prompt, max_tokens, the sampling fields and the thinking fields, with no stream_options. Usage is read only from ev.get("usage") (812, 816).
- **Fix:** Add "stream_options": {"include_usage": True} in body_for. When usage is absent, fill prompt_tokens and completion_tokens from timings.prompt_n and timings.predicted_n.

### 13. Three JSON Lines cases fail:
- text.splitlines() also splits at U+2028, U+2029 and U+0085, which JSON allows raw inside strings.

- **Where:** llm_jobrunner.py, lines 381-386, 393, 404
- **Problem:** Three JSON Lines cases fail:
- text.splitlines() also splits at U+2028, U+2029 and U+0085, which JSON allows raw inside strings. Such a file is refused whole as 'line N is not JSON'.
- A one-item JSON Lines file without a header parses as one whole object and is refused with 'the file holds no items'.
- A header line without 'format' is taken as an item. Its server and defaults are not applied, so every item fails with 'it names no server'.
- **Evidence:** - My check: a json.dumps(..., ensure_ascii=False) line holding U+2028 and U+0085 splits into 3 parts with splitlines().
- json.loads of a one-line JSONL text returns a dict, so line 386 returns whole.get("items"), which is None.
- Line 404 needs "format" in o to accept a header.
- README line 110 says JSON Lines works with an optional header.
- **Fix:** Split on '\n' only and strip a trailing '\r'. Treat a whole-file object that has 'id' and no 'items' as a one-item file. Treat a first line with no 'id' and any HEAD_KEYS as the header.

### 14. Folder names can be Windows device names (NUL, CON, AUX, PRN, COMn, LPTn), and the "(k)" loop has no bound. (also S-10)

- **Where:** llm_jobrunner.py, lines 1203
- **Problem:** Folder names can be Windows device names (NUL, CON, AUX, PRN, COMn, LPTn), and the "(k)" loop has no bound. A drop can hang forever (NUL) or fail with a 500 (the others).
- **Evidence:** - job_title keeps inner dots of the stem; splitext removes only the last extension (1197).
- safe_name has no reserved-name check (1203).
- On my re-typed copy, "nul.jobpack.json" gives folder "nul.jobpack 2026-11-07", "con.v2.json" gives "con.v2 ..." and "aux.probes.jsonl" gives "aux.probes ...".
- Per Microsoft's naming rules, "NUL.anything" is the NUL device (Windows 10 at least).
- `while os.path.exists(d)` (919-921) has no limit. For NUL, exists is true for every "(k)", so the request thread likely spins forever.
- For CON, AUX, COMn and LPTn, os.makedirs(d) (922) raises and the page gets a 500.
- **Fix:** In safe_name, if the part before the first dot, with trailing spaces stripped, matches (?i)^(CON|PRN|AUX|NUL|COM[0-9¹²³]|LPT[0-9¹²³])$, prefix the name with '_'. Bound the loop (k <= 999, else raise Bad).

### 15. Every run line repeats the item's full messages, image base64 included, in results.jsonl and the shared zip.

- **Where:** llm_jobrunner.py, lines 863
- **Problem:** Every run line repeats the item's full messages, image base64 included, in results.jsonl and the shared zip. Repeat multiplies the size, and the zip is built in RAM while holding the runner lock.
- **Evidence:** - All repeat runs share msgs (500). result_line adds messages to every line (863-864).
- results.jsonl always gets them (871). The zip gets them when include_messages is on, which is the default (43, 1166).
- _short_images is applied only to payloads.jsonl (873).
- A 1.5 MB image item with repeat 20 puts about 30 MB of base64 into the zip. Repeat may be 1000 (49) and runs 5000 (40).
- zip_bytes json-dumps all lines while holding self.lock (1161-1184), stalling the workers and the page.
- load_folder keeps every line in memory at start (957-961).
- **Fix:** In result_line, replace each image_url data URL with a descriptor, for example {'image':'png','bytes':N,'sha256':...}. The session already holds the jobpack. Optionally build the lines from a snapshot taken outside self.lock.

### 16. Parallel drops race on the folder name and the sha check. (also C-13)

- **Where:** llm_jobrunner.py, lines 919
- **Problem:** Parallel drops race on the folder name and the sha check. A same-stem second file fails with a 500 instead of getting "(2)", and the same file dropped twice at once makes two jobs with one id.
- **Evidence:** - The page posts every dropped file at once (1974-1981), and the server is threaded (1386).
- add() releases the lock after the sha loop (902-914), then calls os.path.exists and os.makedirs(d) without exist_ok (919-922), so the second file gets FileExistsError, caught as OSError and returned as a 500 (1381).
- For a double drop of one file, both pass the sha check and both get folders. self.jobs[sha[:12]] is replaced and self.order lists the id twice (932-933).
- **Fix:** Allocate the folder with try: os.makedirs(d) / except FileExistsError: k += 1 (bounded). Re-check the sha under self.lock just before registering, and if a duplicate has appeared, remove the new folder and return the existing job as a resume.

### 17. Jobs can vanish from the page and their names can be taken by empty folders, which leads to a re-run under "(2)".

- **Where:** llm_jobrunner.py, lines 952
- **Problem:** Jobs can vanish from the page and their names can be taken by empty folders, which leads to a re-run under "(2)". Failed writes leave temp files behind.
- **Evidence:** - add() creates the folder and jobpack.json (922-923) before the job exists. A later OSError leaves a folder without state.json. load_folder skips it (952), but it still takes the name.
- load_folder re-validates each stored jobpack against the current Settings (951). If the header server was renamed or removed in Settings (433), or file_mb or items was lowered (416, 444), the whole job is skipped with no note. Dropping the file again creates a new "(2)" folder and re-runs everything.
- write_atomic (152-158) does not remove *.<pid>.tmp when the write fails.
- **Fix:** In load_folder, parse with the caps that applied when the job was dropped, or skip the cap checks, and keep the job with its stored results. Mark only the queued runs whose server no longer resolves as failed, and write a start_log line for every folder skipped. In add(), remove the new folder if writing jobpack.json fails. In write_atomic, unlink the temp file in a finally block when os.replace did not run.

### 18. For non-ASCII titles the zip name differs from the card and folder name.

- **Where:** llm_jobrunner.py, lines 1353
- **Problem:** For non-ASCII titles the zip name differs from the card and folder name. Unicode control and format characters pass into folder names and titles.
- **Evidence:** - The page names the download from the ASCII filename= (1721), and the server maps non-ASCII to "-" (1353). "Ranni’s probes 2026-11-07" therefore becomes "Ranni-s probes 2026-11-07.zip" while the folder keeps the apostrophe, against README:79-80.
- safe_name strips only \x00-\x1f (1203). U+202E (right-to-left override), zero-width characters and \x7f survive (tested on my re-typed copy).
- **Fix:** In safe_name, also drop characters whose unicodedata.category is Cc, Cf, Zl or Zp. Then either have the page decode filename* (the UTF-8'' part, with decodeURIComponent), or use the same ASCII mapping for the folder and card title, so that all three names agree.

### 19. The launcher may run a planted pythonw.exe, py.exe or python.exe from its own folder or the current folder, ahead of the one on PATH.

- **Where:** launcher-src/launcher.cpp, lines 25-28, 54, 68, 74
- **Problem:** The launcher may run a planted pythonw.exe, py.exe or python.exe from its own folder or the current folder, ahead of the one on PATH. It also starts Python without -I, so a planted module beside the script can shadow the standard library, and it does not harden its DLL search.
- **Evidence:** L:27 `SearchPathW(NULL, name, NULL, MAX_PATH, buf, NULL)`. With lpPath NULL, Windows searches the folder the application started from and the current folder before PATH (the current folder comes early unless SafeProcessSearchMode is set). L:54 tries pythonw.exe, then py.exe, then python.exe. L:68 builds the command `"<py>" "<script>" --no-browser` with no -I. No call to SetDefaultDllDirectories or SetDllDirectoryW anywhere. The exe's static imports are KnownDLLs, but ShellExecuteW and MessageBoxW may load others at run time. This matters when the bundle is unzipped flat into Downloads or started from a shared folder. Anyone who can write the bundle folder could also edit the .py, so the extra risk comes from the current folder and shared folders.
- **Fix:** Keep the proposed fix: call SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32) and SetDllDirectoryW(L"") first. Search only the PATH value, passed as lpPath, or call SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE|BASE_SEARCH_PATH_PERMANENT). Refuse any hit in ExeDir() or the current folder. Start Python as "<py>" -I "<script>" --no-browser.

### 20. Some values from the server are inserted into innerHTML without escaping, so a server, or anything that answers at the configured address, can inject markup into the page.

- **Where:** llm_jobrunner.py, lines 1762, 1803, 1904 (data from 653, 816)
- **Problem:** Some values from the server are inserted into innerHTML without escaping, so a server, or anything that answers at the configured address, can inject markup into the page.
- **Evidence:** P:653 `slots=p.get("total_slots")` (raw JSON from /props) is rendered at P:1762 as `'<span class="chip">' + s.slots + " slot"`. Usage values are copied raw at P:816 and rendered at P:1803 `(r[7] == null ? "" : r[7])` and P:1904 `r.pt` and `r.ct`. The CSP (P:1260-1263) blocks scripts, styles, frames and outside images, so the token cannot be stolen. Markup can still be injected: a dynamically inserted `<meta http-equiv=refresh>` can take the tab to another site, and fake data-act buttons or overlays can be added.
- **Fix:** As proposed: wrap these values in esc() or Number() in the page, and keep slots and usage values on the server only when they are int or float and not bool.

### 21. Any local program can get the per-start token by asking for the page, so the token does not keep other local programs out.

- **Where:** llm_jobrunner.py, lines 1275, 1289, 1711
- **Problem:** Any local program can get the per-start token by asking for the page, so the token does not keep other local programs out. README:32-33 claims it does.
- **Evidence:** guard asks for the token only on paths that start with /api/ (P:1275). GET / returns PAGE with `__TOKEN__` replaced (P:1289), and the script holds it in `const TOKEN` (P:1711). A local process that sends `Host: 127.0.0.1:<port>` reads the token and then has full API control: running prompts, reading results, payloads and the manager's logs, and changing the output folder. Other user accounts, containers and WSL in mirrored mode count as local. Websites are kept out by the Host and Origin checks.
- **Fix:** Either use the one-time ticket from the port file, swapped for an HttpOnly SameSite=Strict cookie that is checked on / and /api/* (running_port can then tell the tool by its Server header on a 403), or state in the README and the spec reply that only websites are kept out.

### 22. The AI PC address check accepts more than home-network ranges, although its error text names only 192.168/16, 10/8 and 172.16/12.

- **Where:** llm_jobrunner.py, lines 97-104
- **Problem:** The AI PC address check accepts more than home-network ranges, although its error text names only 192.168/16, 10/8 and 172.16/12.
- **Evidence:** P:102 `if not (a.is_private or a.is_loopback) or a.is_link_local or a.is_unspecified`. Tested with an isolated script on Python 3.13, it accepts 127/8, 0.x.x.x, 192.0.0.x, 192.0.2/24, 198.18.0.0/15, 198.51.100/24, 203.0.113/24, 240.0.0.0/4 and 255.255.255.255. 198.18.0.0/15 is the fake-IP range used by TUN-mode proxy tools, so on a PC running one, a request to such an address can leave the home network.
- **Fix:** Accept only the three private IPv4 ranges of RFC 1918 (the 10/8, 172.16/12 and 192.168/16 networks). Refusing loopback is optional; keep it if the stand-in tests use 127.0.0.1 as the 'AI PC'.

### 23. The 300 s per-request cap and Cancel are checked only when a line arrives, and neither a line nor the whole reply has a size cap.

- **Where:** llm_jobrunner.py, lines 767, 780-784, 801
- **Problem:** The 300 s per-request cap and Cancel are checked only when a line arrives, and neither a line nor the whole reply has a size cap. A peer that drips bytes without newlines can hold a request indefinitely and grow memory without bound.
- **Evidence:** P:767 `_open(..., timeout=min(timeout, 120))` is a timeout per socket operation, not a total. P:780 `for raw in resp` reads lines of any length. The deadline check (P:783) and the abort check (P:781) run only between lines. `text` (P:801) has no cap.
- **Fix:** Bound time as well as size. Either arm a timer at the deadline, and have cancel do the same, that closes the response or shuts down its socket so the blocked read returns. Or use a short socket timeout with resp.read1() in a loop, checking the deadline and abort after each read and treating TimeoutError as 'check again' until the overall deadline. Keep a running byte cap, for example 8 MB, and fail the run past it.
