# privsweep: the privacy and safety sweep

Nothing leaves a session before this sweep passes: no git push, Artifact, file sent to the owner, GitHub write,
Claude Docs write, web query or command-line upload. It protects the owner's privacy and safety and the
players': no personal detail, no secret, no local address goes out.

`privsweep.py` and `personal.txt` are identical copies of the owner's Skyrizz sweep (`tools/privsweep/` in
Pt0l3my/Skyrizz, decision d414 there). Keep them identical: when the sweep changes, copy the new version from
Skyrizz rather than editing it here. Only `allow.txt` and this README are EldenNet's own.

## Where it runs

| Gate | What it sweeps | Set up by |
|---|---|---|
| git's pre-push hook, on every repository | every object the remote doesn't have yet: file contents and names, commit and tag messages, author, committer and tagger | `install` (global `core.hooksPath` in a cloud session, the repository's own elsewhere) |
| Claude Code's PreToolUse hook | Artifact publishes and asset uploads, SendUserFile, GitHub writes, Claude Docs writes, WebFetch and WebSearch, and curl, wget, scp, rsync or rclone uploads; it also refuses a push that skips or redirects the git gate | `.claude/settings.json`, and in a cloud session the user's settings too |

The session-start hook (`.claude/hooks/session-start.sh`) runs `install` in each session. That copies the sweep
to `~/.config/privsweep`, writes the pre-push gate there and points git at it. If that folder is missing, run it
by hand:

```
python3 -I tools/privsweep/privsweep.py install
```

## What it looks for

A hit blocks. Its value is never printed whole: a refusal names the rule, the place and a masked value.

- **personal**: the owner's own details. Never allowed, wherever they are. They come from places that are never
  committed: `$PRIVSWEEP_PRIVATE` (values separated by newlines or commas, best set as a variable of the cloud
  environment) and `~/.config/privsweep/private.txt` (a value a line, or `re:` and a regular expression); and
  from `personal.txt`, which holds only salted hashes of words long and particular enough that a hash doesn't
  give them away.
- **handle**: the owner's handles, in a GitHub write outside the owner's own repositories.
- **identity**: a commit's or tag's e-mail that isn't a noreply address.
- **email**: an e-mail address that isn't a noreply or example address.
- **ip**: a private, link-local or shared (CGNAT) IPv4 address.
- **userpath**: a user's folder on Windows, macOS or Linux, generic and CI names aside.
- **drive**: a folder path on a drive other than C:.
- **machine**: a Windows machine name in its default form.
- **endpoint**: a URL to a `.local`, `.lan`, `.home` or `.internal` host.
- **secret**: API keys and tokens, private keys, JWTs, credentials in a URL, a literal bearer token or an
  assigned password or key.

Binaries are read through their strings (ASCII and UTF-16), archives (zip, 7z, tar, gzip, bzip2, xz) member by
member and name by name, and `data:` URIs decoded. Anything the sweep can't read, or a missing file, is refused:
it fails closed, and so does the hook if it breaks.

## A finding

Remove the value. For a false positive of a rule other than personal, add a line to `allow.txt`:
`RULE VALUE-REGEX [WHERE-REGEX]`, as narrow as it can be, with a comment saying why. Never bypass a gate: no
`--no-verify`, no change to `core.hooksPath`, no HOME or GIT_CONFIG override; the hook refuses them.

## By hand

Use these before anything the hooks don't cover, such as a message to another session:

```
python3 -I tools/privsweep/privsweep.py files PATH...          # files, folders, archives, builds
python3 -I tools/privsweep/privsweep.py repo DIR [--head|--all] # what DIR would push, HEAD's files, or all history
python3 -I tools/privsweep/privsweep.py text [WHERE] < FILE    # text on stdin
python3 -I tools/privsweep/privsweep.py hash < FILE            # personal.txt lines for the words in FILE
python3 -I tools/privsweep/privsweep.py selftest               # synthetic samples only
```
