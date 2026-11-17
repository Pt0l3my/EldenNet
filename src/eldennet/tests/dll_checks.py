#!/usr/bin/env python3
"""ARCH §12.1's checks on eldennet.dll (part 6 of the first in-game build), run by CTest in the clang-cl build:

- the greps over its sources (src/eldennet): DllMain does nothing but start T1 (no file, lock, allocation, wait or load
  in it: ARCH §3.1), there is no detach branch (R14), every WINAPI, CALLBACK or extern "C" function's body starts with
  ffi_guard (R3), no cast of an address (ARCH §2.4: the `eldennet` module holds none), no hook library or ImGui renderer
  backend (R12), no IsEnableOnlineMode (ARCH §5.5); and every module the DLL links keeps en_dll_module's rules (no
  dynamic initialiser, no exit-time destructor: ARCH §3.1, §1.7), which the compiler enforces once CMake asks for them;
- the built DLLs' exports (eldennet_instance_marker alone) and imports (the DLLs and functions of imports.allow, a
  reviewed list: no network, no process injection, no hook API), read with llvm-objdump.

    dll_checks.py --source src/eldennet --objdump llvm-objdump --dll eldennet.dll [--dll probe/eldennet.dll]
"""
import argparse
import os
import re
import subprocess
import sys

FORBIDDEN_IN_DLLMAIN = re.compile(
    r"\b(LoadLibrary\w*|CreateFile\w*|ReadFile|WriteFile|Sleep\w*|WaitFor\w*|EnterCriticalSection|AcquireSRWLock\w*|"
    r"new|malloc|calloc|HeapAlloc|VirtualAlloc|std::string|std::vector|std::wstring|GetModuleFileName\w*|"
    r"CoInitialize\w*|RegOpenKey\w*|MessageBox\w*|printf|fopen|log)\b")
HOOKS = re.compile(r"\bMH_\w+|ImGui_Impl\w*|\bDetour\w*|minhook|kiero|polyhook|subhook|funchook", re.I)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def body_after(text, start):
    """The text of the brace block that opens at or after `start`."""
    i = text.index("{", start)
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i + 1:j]
    raise ValueError("unbalanced braces")


def check_sources(src):
    failures = []
    files = []
    for folder, _, names in os.walk(src):
        if os.sep + "tests" in folder + os.sep:
            continue
        files += [os.path.join(folder, n) for n in names if n.endswith((".cpp", ".hpp"))]
    sources = {f: strip_comments(open(f, encoding="utf-8").read()) for f in sorted(files)}
    dllmain = [(f, t) for f, t in sources.items() if re.search(r"\bDllMain\s*\(", t)]
    if len(dllmain) != 1:
        failures.append("DllMain is defined in %d files" % len(dllmain))
    for f, t in dllmain:
        m = re.search(r"BOOL\s+WINAPI\s+DllMain\s*\([^)]*\)\s*noexcept", t)
        if not m:
            failures.append("%s: DllMain is not BOOL WINAPI DllMain(...) noexcept" % f)
            continue
        body = body_after(t, m.end())
        stmts = " ".join(body.split())
        want = ("return en::proto::ffi_guard(en::proto::Component::DllMain, TRUE, [&] { if (reason == "
                "DLL_PROCESS_ATTACH) en::gal::sys::boot::on_attach(module, &eldennet::init_main); return TRUE; });")
        if stmts != want:
            failures.append("%s: DllMain's body is not the load contract's (ARCH §3.1): %s" % (f, stmts[:200]))
        bad = FORBIDDEN_IN_DLLMAIN.findall(body)
        if bad:
            failures.append("%s: DllMain calls %s" % (f, sorted(set(bad))))
    for f, t in sources.items():
        rel = os.path.relpath(f, src)
        if "DLL_PROCESS_DETACH" in t or "DLL_THREAD_DETACH" in t:
            failures.append("%s: a detach branch (R14: there is none)" % rel)
        if re.search(r"\breinterpret_cast\b|\bstd::bit_cast\b", t):
            failures.append("%s: a cast of an address (ARCH §2.4: they live in en-gal's sys)" % rel)
        if HOOKS.search(t):
            failures.append("%s: a hook library's or a renderer backend's name (R12)" % rel)
        if "IsEnableOnlineMode" in t:
            failures.append("%s: IsEnableOnlineMode (ARCH §5.5)" % rel)
        # R3: a WINAPI, CALLBACK or extern "C" function's body starts with ffi_guard.
        for m in re.finditer(r'(?:\bWINAPI|\bCALLBACK|extern\s+"C")\s[^;{=]*?\)\s*noexcept\s*\{', t):
            body = " ".join(body_after(t, m.end() - 1).split())
            if not re.match(r"(return\s+)?en::proto::ffi_guard\(", body):
                failures.append("%s: an entry that does not start with ffi_guard (R3): %s" % (rel, m.group(0)[:80]))
        for m in re.finditer(r'(?:\bWINAPI|\bCALLBACK)\s+\w+\s*\([^)]*\)\s*\{', t):
            failures.append("%s: an entry that is not noexcept (R3): %s" % (rel, m.group(0)[:80]))
    cmake = open(os.path.join(src, "CMakeLists.txt"), encoding="utf-8").read()
    for target in ("en_shell", "${target}"):
        if "en_dll_module(%s)" % target not in cmake:
            failures.append("CMakeLists.txt: %s does not keep en_dll_module's rules" % target)
    return failures


def read_allowlist(path):
    allowed = {}
    current = None
    for line in open(path, encoding="utf-8"):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1].lower()
            allowed[current] = set()
        elif current is not None:
            allowed[current].add(line)
    return allowed


def check_binary(objdump, dll, allowed):
    failures = []
    out = subprocess.run([objdump, "-p", dll], capture_output=True, text=True, check=True).stdout
    exports = []
    in_exports = False
    for line in out.splitlines():
        if line.startswith("Export Table:"):
            in_exports = True
            continue
        if in_exports:
            m = re.match(r"^\s+\d+\s+0x[0-9a-f]+\s+(\S+)$", line)
            if m:
                exports.append(m.group(1))
            elif not line.strip():
                in_exports = False
    if exports != ["eldennet_instance_marker"]:
        failures.append("%s exports %s, not eldennet_instance_marker alone" % (os.path.basename(dll), exports))
    current = None
    imports = {}
    for line in out.splitlines():
        m = re.match(r"^\s+DLL Name: (\S+)", line)
        if m:
            current = m.group(1).lower()
            imports[current] = set()
            continue
        m = re.match(r"^\s+\d+\s+\d*\s*([A-Za-z_]\w*)$", line)
        if current is not None and m:
            imports[current].add(m.group(1))
    for name, funcs in sorted(imports.items()):
        if name not in allowed:
            failures.append("%s imports %s, which imports.allow does not list" % (os.path.basename(dll), name))
            continue
        extra = sorted(funcs - allowed[name])
        if extra:
            failures.append("%s imports from %s what imports.allow does not list: %s" %
                            (os.path.basename(dll), name, ", ".join(extra)))
    return failures, sum(len(v) for v in imports.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--source", required=True)
    parser.add_argument("--objdump", required=True)
    parser.add_argument("--dll", action="append", default=[])
    args = parser.parse_args()
    failures = check_sources(args.source)
    allowed = read_allowlist(os.path.join(args.source, "tests", "imports.allow"))
    for dll in args.dll:
        f, n = check_binary(args.objdump, dll, allowed)
        failures += f
        print("dll_checks: %s: %d imports" % (os.path.basename(os.path.dirname(dll)) + "/" + os.path.basename(dll), n))
    for f in failures:
        print("FAIL: %s" % f)
    print("PASS" if not failures else "FAILED")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
