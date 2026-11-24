// The fake game's GraceUI parts (program.hpp; part 12): a Direct3D 12 swap chain it presents every frame, so that
// GraceUI's overlay binds it and draws on it, as a game's would be; F11 pressed into its window; and a report of what a
// test build of GraceUI says through its test exports (graceui_test.h), with a count of one colour's pixels in a
// rectangle of the frame just presented, read back after GraceUI drew on it; and, standing in for another mod, a
// connection of its own with a hotkey on Pause; and, for preview shots, a backdrop of our own, a swap chain of a given
// size and format, and the frame as presented written to a BMP file. Our own code, written for this fixture; nothing in
// it is a game's, and nothing GraceUI's but the BMP writer's way, which follows GraceUI's harness (shot below).
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tlhelp32.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "graceui_sys.hpp"
#include "graceui_test.h"
#include "program.hpp"

namespace fakegame {

namespace {

constexpr UINT kBuffers = 2;
constexpr float kClear[4] = {16 / 255.0f, 40 / 255.0f, 32 / 255.0f, 1.0f};  // the fake game's own ground

using CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using CreateFactoryFn = HRESULT(WINAPI*)(UINT, REFIID, void**);

struct Presenter {
  IDXGIFactory4* factory = nullptr;
  ID3D12Device* device = nullptr;
  ID3D12CommandQueue* queue = nullptr;
  IDXGISwapChain3* chain = nullptr;
  ID3D12DescriptorHeap* rtv_heap = nullptr;
  UINT rtv_step = 0;
  ID3D12Resource* buffers[kBuffers] = {};
  ID3D12CommandAllocator* allocators[kBuffers] = {};
  ID3D12CommandAllocator* readback_allocator = nullptr;
  ID3D12GraphicsCommandList* list = nullptr;
  ID3D12Fence* fence = nullptr;
  UINT64 fence_value = 0;
  UINT64 buffer_fence[kBuffers] = {};
  HANDLE event = nullptr;
  ID3D12Resource* readback = nullptr;
  UINT64 readback_bytes = 0;
  ID3D12Resource* backdrop = nullptr;  // --backdrop: the backdrop's pixels, in an upload buffer, in a buffer's layout
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT backdrop_layout{};
  UINT width = 0;
  UINT height = 0;
  DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
  UINT last = 0;  // the buffer presented last
  bool on = false;
};

Presenter g_p;

bool ok(HRESULT hr, const char* what) {
  if (SUCCEEDED(hr)) return true;
  status("d3d12 failed=%s hr=0x%08lX", what, static_cast<unsigned long>(hr));
  return false;
}

void wait_for(UINT64 value) {
  if (g_p.fence->GetCompletedValue() < value) {
    g_p.fence->SetEventOnCompletion(value, g_p.event);
    WaitForSingleObject(g_p.event, 10000);
  }
}

void wait_idle() {
  g_p.queue->Signal(g_p.fence, ++g_p.fence_value);
  wait_for(g_p.fence_value);
}

void barrier(ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
  D3D12_RESOURCE_BARRIER b{};
  b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.pResource = r;
  b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  b.Transition.StateBefore = from;
  b.Transition.StateAfter = to;
  g_p.list->ResourceBarrier(1, &b);
}

template <class Fn>
Fn export_of(HMODULE m, const char* name) {
  // The export's own type, as the header declaring it gives it (d3d12.h, dxgi1_4.h, graceui_test.h).
  return m == nullptr ? nullptr : reinterpret_cast<Fn>(reinterpret_cast<void (*)()>(GetProcAddress(m, name)));
}

// A pixel of the swap chain's format as 8-bit red, green and blue: R8G8B8A8_UNORM's bytes as they are, and
// R10G10B10A2_UNORM's 10-bit channels (red in the low bits) taken to the nearest of 256 levels.
void rgb_of(const unsigned char* px, int out[3]) {
  if (g_p.format != DXGI_FORMAT_R10G10B10A2_UNORM) {
    for (int c = 0; c < 3; ++c) out[c] = px[c];
    return;
  }
  std::uint32_t word = 0;
  std::memcpy(&word, px, sizeof word);
  for (int c = 0; c < 3; ++c) out[c] = static_cast<int>((((word >> (10 * c)) & 0x3FFu) * 255u + 511u) / 1023u);
}

// The newest presented frame, read back once the GPU is done with it (GraceUI's drawing included), handed to `use`
// with its rows (each `pitch` bytes apart), its width and its height while the read-back buffer is mapped.
template <class Use>
bool read_back(const Use& use) {
  if (!g_p.on) return false;
  wait_idle();
  ID3D12Resource* src = g_p.buffers[g_p.last];
  const D3D12_RESOURCE_DESC desc = src->GetDesc();
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
  UINT64 bytes = 0;
  g_p.device->GetCopyableFootprints(&desc, 0, 1, 0, &fp, nullptr, nullptr, &bytes);
  if (bytes > g_p.readback_bytes) return false;
  g_p.readback_allocator->Reset();
  g_p.list->Reset(g_p.readback_allocator, nullptr);
  barrier(src, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
  D3D12_TEXTURE_COPY_LOCATION to{};
  to.pResource = g_p.readback;
  to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  to.PlacedFootprint = fp;
  D3D12_TEXTURE_COPY_LOCATION from{};
  from.pResource = src;
  from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  from.SubresourceIndex = 0;
  g_p.list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
  barrier(src, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
  g_p.list->Close();
  ID3D12CommandList* lists[] = {g_p.list};
  g_p.queue->ExecuteCommandLists(1, lists);
  wait_idle();
  void* mapped = nullptr;
  const D3D12_RANGE all{0, static_cast<SIZE_T>(bytes)};
  if (FAILED(g_p.readback->Map(0, &all, &mapped))) return false;
  const bool used = use(static_cast<const unsigned char*>(mapped) + fp.Offset,
                        static_cast<std::size_t>(fp.Footprint.RowPitch), static_cast<UINT>(desc.Width), desc.Height);
  const D3D12_RANGE none{0, 0};
  g_p.readback->Unmap(0, &none);
  return used;
}

// The newest presented frame's pixels within the rectangle that are within `tolerance` of the colour on each channel,
// and the rectangle's area.
bool probe(const std::uint32_t rect[4], const std::uint32_t colour[3], int tolerance, std::uint64_t& matching,
           std::uint64_t& area) {
  return read_back([&](const unsigned char* rows, std::size_t pitch, UINT width, UINT height) {
    matching = 0;
    area = 0;
    for (UINT y = rect[1]; y < rect[3] && y < height; ++y) {
      for (UINT x = rect[0]; x < rect[2] && x < width; ++x) {
        int rgb[3];
        rgb_of(rows + static_cast<std::size_t>(y) * pitch + x * 4u, rgb);
        ++area;
        bool close = true;
        for (int c = 0; c < 3; ++c) {
          const int d = rgb[c] - static_cast<int>(colour[c]);
          close = close && d >= -tolerance && d <= tolerance;
        }
        matching += close ? 1 : 0;
      }
    }
    return true;
  });
}

// --backdrop: our own backdrop, computed for each pixel (no image file, nothing of a game's): a dark vertical gradient,
// slate at the top to near black at the foot, with one soft warm light above and right of the centre, away from where
// EldenNet's surfaces stand (the lower third, the hint, the box, the F11 view at the top left). A 4 by 4 ordered dither
// keeps the gradient from banding at the swap chain's depth. Written in the format's own layout at `pitch` a row.
void fill_backdrop(unsigned char* rows, std::size_t pitch, UINT width, UINT height, DXGI_FORMAT format) {
  static constexpr float kTop[3] = {0.105f, 0.118f, 0.160f};   // #1B1E29
  static constexpr float kFoot[3] = {0.027f, 0.031f, 0.043f};  // #07080B
  static constexpr float kWarm[3] = {1.00f, 0.56f, 0.22f};     // the light's colour, an ember's
  static constexpr float kStrength = 0.19f;                    // at its centre
  static constexpr int kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  const bool ten = format == DXGI_FORMAT_R10G10B10A2_UNORM;
  const float levels = ten ? 1023.0f : 255.0f;
  const float cx = 0.68f * static_cast<float>(width);
  const float cy = 0.30f * static_cast<float>(height);
  const float sigma = 0.26f * static_cast<float>(height);
  for (UINT y = 0; y < height; ++y) {
    const float t = height > 1 ? static_cast<float>(y) / static_cast<float>(height - 1) : 0.0f;
    const float down = t * t * (3.0f - 2.0f * t);  // smoothstep: the slate holds a little at the top
    unsigned char* row = rows + static_cast<std::size_t>(y) * pitch;
    for (UINT x = 0; x < width; ++x) {
      const float dx = static_cast<float>(x) - cx;
      const float dy = static_cast<float>(y) - cy;
      const float glow = kStrength * std::exp(-(dx * dx + dy * dy) / (2.0f * sigma * sigma));
      const float dither = (static_cast<float>(kBayer[y & 3][x & 3]) + 0.5f) / 16.0f - 0.5f;
      std::uint32_t q[3];
      for (int c = 0; c < 3; ++c) {
        const float v = kTop[c] + (kFoot[c] - kTop[c]) * down + kWarm[c] * glow;
        const float level = std::floor(v * levels + 0.5f + dither);
        q[c] = static_cast<std::uint32_t>(level < 0.0f ? 0.0f : level > levels ? levels : level);
      }
      unsigned char* px = row + static_cast<std::size_t>(x) * 4u;
      if (ten) {
        const std::uint32_t word = q[0] | (q[1] << 10) | (q[2] << 20) | (3u << 30);
        std::memcpy(px, &word, sizeof word);
      } else {
        for (int c = 0; c < 3; ++c) px[c] = static_cast<unsigned char>(q[c]);
        px[3] = 255;
      }
    }
  }
}

// The backdrop in an upload buffer, laid out as the back buffer's copyable footprint, copied into the back buffer each
// frame in place of the clear.
bool make_backdrop() {
  const D3D12_RESOURCE_DESC desc = g_p.buffers[0]->GetDesc();
  UINT64 bytes = 0;
  g_p.device->GetCopyableFootprints(&desc, 0, 1, 0, &g_p.backdrop_layout, nullptr, nullptr, &bytes);
  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC rd{};
  rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  rd.Width = bytes;
  rd.Height = 1;
  rd.DepthOrArraySize = 1;
  rd.MipLevels = 1;
  rd.SampleDesc.Count = 1;
  rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  if (!ok(g_p.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ,
                                              nullptr, IID_PPV_ARGS(&g_p.backdrop)),
          "backdrop")) {
    return false;
  }
  void* mapped = nullptr;
  const D3D12_RANGE none{0, 0};
  if (!ok(g_p.backdrop->Map(0, &none, &mapped), "backdrop_map")) return false;
  fill_backdrop(static_cast<unsigned char*>(mapped) + g_p.backdrop_layout.Offset,
                static_cast<std::size_t>(g_p.backdrop_layout.Footprint.RowPitch), g_p.width, g_p.height, g_p.format);
  g_p.backdrop->Unmap(0, nullptr);
  return true;
}

// A notice's text as one status word: spaces as '_', anything that is not printable ASCII '?'.
std::string word_of(const char* text) {
  std::string out;
  for (const char* p = text; *p != '\0'; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    out += c == ' ' ? '_' : (c < 0x21 || c > 0x7E) ? '?' : static_cast<char>(c);
  }
  return out.empty() ? "none" : out;
}

// --- Copies of GraceUI (D72, D83; GraceUI's invariant 18) ------------------------------------------------------------
struct GraceCopy {
  HMODULE module = nullptr;
  std::string name;  // relative to the game's folder, as GraceUI's own notice names a copy; else its file name
};

// A module's name relative to the folder of the process's executable (the game's), in ASCII ('?' for anything else);
// outside it, its file name after "outside:". Never a whole path.
std::string name_in_game(HMODULE module) {
  wchar_t path[1024];
  wchar_t program[1024];
  const DWORD n = GetModuleFileNameW(module, path, 1024);
  DWORD folder = GetModuleFileNameW(nullptr, program, 1024);
  if (n == 0 || n >= 1024 || folder == 0 || folder >= 1024) return "unknown";
  while (folder > 0 && program[folder - 1] != L'\\' && program[folder - 1] != L'/') --folder;
  const wchar_t* from = path;
  std::string out;
  if (folder > 0 && n > folder &&
      CompareStringOrdinal(path, static_cast<int>(folder), program, static_cast<int>(folder), TRUE) == CSTR_EQUAL) {
    from = path + folder;
  } else {
    from = path + n;
    while (from > path && from[-1] != L'\\' && from[-1] != L'/') --from;
    out = "outside:";
  }
  for (; *from != L'\0'; ++from) out += *from > 0x20 && *from < 0x7F ? static_cast<char>(*from) : '?';
  return out;
}

// Every module that exports GraceUI_RequestApi, in load order (as graceui-sys's scan and GraceUI's own look find them).
// A copy is never unloaded while the fake game runs: the launcher's load keeps its reference, and an asked copy pins
// itself (GraceUI D107).
std::vector<GraceCopy> grace_copies() {
  std::vector<GraceCopy> out;
  const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return out;
  MODULEENTRY32W entry{};
  entry.dwSize = sizeof entry;
  for (BOOL more = Module32FirstW(snapshot, &entry); more; more = Module32NextW(snapshot, &entry)) {
    if (GetProcAddress(entry.hModule, "GraceUI_RequestApi") != nullptr) {
      out.push_back(GraceCopy{entry.hModule, name_in_game(entry.hModule)});
    }
  }
  CloseHandle(snapshot);
  return out;
}

// A copy's host state (GRACEUI_TEST_HOST_*), or false where it is no test build.
bool state_of(HMODULE module, GraceUiTestState& st) {
  const auto get_state = export_of<uint32_t (*)(GraceUiTestState*)>(module, "GraceUI_Test_GetState");
  st = GraceUiTestState{};
  st.size = sizeof st;
  return get_state != nullptr && get_state(&st) != 0;
}

// The copy the report reads: the module named graceui.dll, as before, unless it idles behind another copy (D72); then
// the first copy that does not.
HMODULE reported_copy() {
  HMODULE named = GetModuleHandleW(L"graceui.dll");
  GraceUiTestState st{};
  if (named == nullptr || !state_of(named, st) || st.host != GRACEUI_TEST_HOST_IDLE_COPY) return named;
  for (const GraceCopy& c : grace_copies()) {
    if (state_of(c.module, st) && st.host != GRACEUI_TEST_HOST_IDLE_COPY) return c.module;
  }
  return named;
}

// In one copy: GraceUI's own notices (owner 0) that name another copy, the last one's text and tone, and the log lines
// still kept that do (GraceUI's abi.cpp words both; from 0.2.38, D244, in other words where GraceUI's own copy idles).
bool names_a_copy(const char* text) {
  return std::strstr(text, "A second copy of GraceUI") != nullptr ||
         std::strstr(text, "GraceUI's own copy is idle") != nullptr;
}
bool logs_a_copy(const char* line) {
  return std::strstr(line, "a second copy of GraceUI is loaded, and idle") != nullptr ||
         (std::strstr(line, "GraceUI's own copy (") != nullptr && std::strstr(line, "is loaded and idle") != nullptr);
}

struct CopyNames {
  std::uint32_t notices = 0;
  std::uint32_t logs = 0;
  std::string notice = "none";
  std::uint32_t tone = 0;
};

CopyNames copy_names(HMODULE module) {
  CopyNames out;
  const auto get_notice = export_of<uint32_t (*)(uint32_t, GraceUiTestNotice*)>(module, "GraceUI_Test_GetNotice");
  const auto get_log = export_of<uint32_t (*)(uint32_t, char*, uint32_t)>(module, "GraceUI_Test_GetLogLine");
  for (uint32_t i = 0; get_notice != nullptr; ++i) {
    GraceUiTestNotice n{};
    n.size = sizeof n;
    if (get_notice(i, &n) == 0) break;
    if (n.owner == 0 && names_a_copy(n.text)) {
      ++out.notices;
      out.notice = word_of(n.text);
      out.tone = n.tone;
    }
  }
  char line[512];
  for (uint32_t i = 0; get_log != nullptr && get_log(i, line, sizeof line) != 0; ++i) {
    if (logs_a_copy(line)) ++out.logs;
  }
  return out;
}

// Where a copy's installed hooks lead now (GraceUI_Test_GetHookLead, 0.2.35's D240; invariant 16), in
// GraceUI_Test_GetHookName's order: how many it answers (an idle copy none, from index 0), how many lead to its own
// detour in its own module, the jumps each followed, and which of them is the window procedure's (-1 for none).
// `known` is false where the copy has no such export (GraceUI before 0.2.35).
struct HookLeads {
  bool known = false;
  std::uint32_t answered = 0;
  std::uint32_t here = 0;
  std::string jumps = "none";
  int window_procedure = -1;
};

HookLeads hook_leads(HMODULE module) {
  HookLeads out;
  const auto get_lead = export_of<uint32_t (*)(uint32_t, GraceUiTestHookLead*)>(module, "GraceUI_Test_GetHookLead");
  const auto get_name = export_of<uint32_t (*)(uint32_t, char*, uint32_t)>(module, "GraceUI_Test_GetHookName");
  if (get_lead == nullptr) return out;
  out.known = true;
  const auto self = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(module));
  for (uint32_t i = 0; i < 32; ++i) {
    GraceUiTestHookLead lead{};
    lead.size = sizeof lead;
    if (get_lead(i, &lead) == 0) break;
    ++out.answered;
    if (lead.leads_here == 1 && lead.module_base == self) ++out.here;
    out.jumps = (i == 0 ? std::string() : out.jumps + ",") + std::to_string(lead.jumps);
    char name[128] = {};
    if (get_name != nullptr && get_name(i, name, sizeof name) != 0 &&
        std::strncmp(name, "the game's window procedure", 27) == 0) {
      out.window_procedure = static_cast<int>(i);
    }
  }
  return out;
}

// --grace-report's line of every copy, where more than one is loaded: for each, in load order, its name, host state,
// init threads, hook installs, hooks, mods connected, other copies named and where its hooks lead; then the reported
// copy's notices and log lines naming a copy, and the last such notice's text and tone.
void report_copies(std::uint64_t frame, HMODULE reported) {
  const std::vector<GraceCopy> copies = grace_copies();
  if (copies.size() < 2) return;
  std::string line;
  char part[256];
  for (std::size_t i = 0; i < copies.size() && i < 4; ++i) {
    GraceUiTestState st{};
    const bool test = state_of(copies[i].module, st);
    std::snprintf(part, sizeof part, " c%zu=%s c%zu_test=%d c%zu_host=%u c%zu_threads=%u c%zu_installs=%u c%zu_hooks=%u"
                  " c%zu_consumers=%u c%zu_named=%u", i + 1, copies[i].name.c_str(), i + 1, test ? 1 : 0, i + 1,
                  st.host, i + 1, st.init_threads_started, i + 1, st.hook_install_runs, i + 1, st.hooks_installed,
                  i + 1, st.consumers, i + 1, st.copies_named);
    line += part;
    const HookLeads leads = hook_leads(copies[i].module);
    if (leads.known) {
      std::snprintf(part, sizeof part, " c%zu_leads=%u c%zu_leads_here=%u c%zu_jumps=%s c%zu_window_procedure=%d",
                    i + 1, leads.answered, i + 1, leads.here, i + 1, leads.jumps.c_str(), i + 1,
                    leads.window_procedure);
      line += part;
    }
  }
  std::size_t reported_index = 0;
  for (std::size_t i = 0; i < copies.size(); ++i) {
    if (copies[i].module == reported) reported_index = i + 1;
  }
  const CopyNames names = copy_names(reported);
  status("grace_copies frame=%llu copies=%zu reported=%zu%s copy_notices=%u copy_logs=%u copy_tone=%u copy_notice=%s",
         static_cast<unsigned long long>(frame), copies.size(), reported_index, line.c_str(), names.notices,
         names.logs, names.tone, names.notice.c_str());
}

// --- The chat box's pad chord (--pad-chord-at; GraceUI D204) ---------------------------------------------------------
constexpr std::uint32_t kPadUp = 0x0001;    // XINPUT_GAMEPAD_DPAD_UP
constexpr std::uint32_t kPadView = 0x0020;  // XINPUT_GAMEPAD_BACK
constexpr std::uint32_t kPadPacket = 40;    // the pad's own packet number, which these reads never change
constexpr double kChordAnchorS = 0.25;      // Up alone (GraceUI holds the key back 150 ms after the anchor at least)
constexpr double kChordBothS = 0.65;        // then both (400 ms complete the chord)

using PadReadFn = uint32_t (*)(uint32_t, uint32_t, uint32_t*);  // graceui_test.h's GraceUI_Test_PadRead

struct ChordRun {
  bool on = false;
  PadReadFn pad_read = nullptr;
  std::uint64_t frame = 0;  // the frame it began at
  double started = 0.0;
  std::uint32_t reads = 0;
  std::uint32_t up_seen = 0;      // reads in which the game saw Up
  std::uint32_t view_seen = 0;    // ... View
  std::uint32_t view_reads = 0;   // reads with View down on the pad
  std::uint32_t packets = 0;      // reads whose packet number GraceUI changed
};
ChordRun g_chord;

double seconds_now() {
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return f.QuadPart == 0 ? 0.0 : static_cast<double>(c.QuadPart) / static_cast<double>(f.QuadPart);
}

}  // namespace

bool present_start(HWND window) {
  HMODULE d3d12 = LoadLibraryW(L"d3d12.dll");
  HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
  const auto create_device = export_of<CreateDeviceFn>(d3d12, "D3D12CreateDevice");
  const auto create_factory = export_of<CreateFactoryFn>(dxgi, "CreateDXGIFactory2");
  if (create_device == nullptr || create_factory == nullptr) {
    status("d3d12 failed=load");
    return false;
  }
  if (!ok(create_factory(0, IID_PPV_ARGS(&g_p.factory)), "factory") ||
      !ok(create_device(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_p.device)), "device")) {
    return false;
  }
  D3D12_COMMAND_QUEUE_DESC qd{};
  qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (!ok(g_p.device->CreateCommandQueue(&qd, IID_PPV_ARGS(&g_p.queue)), "queue")) return false;
  const Options& o = options();
  g_p.width = o.swap_width;
  g_p.height = o.swap_height;
  g_p.format = static_cast<DXGI_FORMAT>(o.swap_format);
  DXGI_SWAP_CHAIN_DESC1 sd{};
  sd.Width = g_p.width;
  sd.Height = g_p.height;
  sd.Format = g_p.format;
  sd.SampleDesc.Count = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = kBuffers;
  sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // a presented buffer keeps its content for the read-back
  IDXGISwapChain1* chain1 = nullptr;
  if (!ok(g_p.factory->CreateSwapChainForHwnd(g_p.queue, window, &sd, nullptr, nullptr, &chain1), "swap_chain") ||
      !ok(chain1->QueryInterface(IID_PPV_ARGS(&g_p.chain)), "swap_chain3")) {
    return false;
  }
  chain1->Release();
  D3D12_DESCRIPTOR_HEAP_DESC hd{};
  hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  hd.NumDescriptors = kBuffers;
  if (!ok(g_p.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g_p.rtv_heap)), "rtv_heap")) return false;
  g_p.rtv_step = g_p.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (UINT i = 0; i < kBuffers; ++i) {
    if (!ok(g_p.chain->GetBuffer(i, IID_PPV_ARGS(&g_p.buffers[i])), "buffer") ||
        !ok(g_p.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_p.allocators[i])),
            "allocator")) {
      return false;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_p.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(i) * g_p.rtv_step;
    g_p.device->CreateRenderTargetView(g_p.buffers[i], nullptr, rtv);
  }
  if (!ok(g_p.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_p.readback_allocator)),
          "allocator") ||
      !ok(g_p.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_p.allocators[0], nullptr,
                                        IID_PPV_ARGS(&g_p.list)),
          "list") ||
      !ok(g_p.list->Close(), "close") ||
      !ok(g_p.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_p.fence)), "fence")) {
    return false;
  }
  g_p.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_READBACK;
  D3D12_RESOURCE_DESC rd{};
  rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  const UINT64 row = (4ull * g_p.width + 255) / 256 * 256;  // rows padded to 256 bytes
  g_p.readback_bytes = row * g_p.height + 65536;           // with room
  rd.Width = g_p.readback_bytes;
  rd.Height = 1;
  rd.DepthOrArraySize = 1;
  rd.MipLevels = 1;
  rd.SampleDesc.Count = 1;
  rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  if (!ok(g_p.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                              IID_PPV_ARGS(&g_p.readback)),
          "readback")) {
    return false;
  }
  if (o.backdrop && !make_backdrop()) return false;
  g_p.on = true;
  status("d3d12 ok=1 width=%u height=%u buffers=%u format=%u backdrop=%d", g_p.width, g_p.height, kBuffers,
         static_cast<unsigned>(g_p.format), o.backdrop ? 1 : 0);
  return true;
}

void present_frame() {
  if (!g_p.on) return;
  const UINT i = g_p.chain->GetCurrentBackBufferIndex();
  wait_for(g_p.buffer_fence[i]);
  g_p.allocators[i]->Reset();
  g_p.list->Reset(g_p.allocators[i], nullptr);
  if (g_p.backdrop != nullptr) {
    barrier(g_p.buffers[i], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION to{};
    to.pResource = g_p.buffers[i];
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION from{};
    from.pResource = g_p.backdrop;
    from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    from.PlacedFootprint = g_p.backdrop_layout;
    g_p.list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    barrier(g_p.buffers[i], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
  } else {
    barrier(g_p.buffers[i], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_p.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(i) * g_p.rtv_step;
    g_p.list->ClearRenderTargetView(rtv, kClear, 0, nullptr);
    barrier(g_p.buffers[i], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  }
  g_p.list->Close();
  ID3D12CommandList* lists[] = {g_p.list};
  g_p.queue->ExecuteCommandLists(1, lists);
  g_p.chain->Present(0, 0);
  g_p.queue->Signal(g_p.fence, ++g_p.fence_value);
  g_p.buffer_fence[i] = g_p.fence_value;
  g_p.last = i;
}

// The frame as presented, as a 24-bit BMP (bottom-up rows of blue, green and red, each padded to 4 bytes), the way
// GraceUI's harness writes its shots for the owner's look review (save_shot in GraceUI's tools/grace-harness/main.cpp,
// MIT, GraceUI's own); a 10-bit format's channels taken to 8 (rgb_of). One `shot` line, never the folder's path.
void shot(std::uint64_t frame) {
  std::vector<unsigned char> out;
  UINT width = 0;
  UINT height = 0;
  const bool read = read_back([&](const unsigned char* rows, std::size_t pitch, UINT w, UINT h) {
    width = w;
    height = h;
    const std::size_t row = (static_cast<std::size_t>(w) * 3 + 3) & ~static_cast<std::size_t>(3);
    const std::size_t bytes = row * h;
    out.assign(54 + bytes, 0);
    const auto put32 = [&](std::size_t at, std::uint64_t value) {
      for (std::size_t k = 0; k < 4; ++k) out[at + k] = static_cast<unsigned char>(value >> (8 * k));
    };
    out[0] = 'B';
    out[1] = 'M';
    put32(2, 54 + bytes);
    put32(10, 54);
    put32(14, 40);
    put32(18, w);
    put32(22, h);
    out[26] = 1;
    out[28] = 24;
    put32(34, bytes);
    std::size_t at = 54;
    for (UINT y = h; y-- > 0;) {
      const unsigned char* src = rows + static_cast<std::size_t>(y) * pitch;
      for (UINT x = 0; x < w; ++x) {
        int rgb[3];
        rgb_of(src + static_cast<std::size_t>(x) * 4u, rgb);
        out[at + x * 3 + 0] = static_cast<unsigned char>(rgb[2]);
        out[at + x * 3 + 1] = static_cast<unsigned char>(rgb[1]);
        out[at + x * 3 + 2] = static_cast<unsigned char>(rgb[0]);
      }
      at += row;
    }
    return true;
  });
  bool written = false;
  if (read) {
    const std::wstring path = options().shot_dir + L"\\frame-" + std::to_wstring(frame) + L".bmp";
    const HANDLE file =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD done = 0;
    written = file != INVALID_HANDLE_VALUE &&
              WriteFile(file, out.data(), static_cast<DWORD>(out.size()), &done, nullptr) && done == out.size();
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
  }
  status("shot frame=%llu ok=%d width=%u height=%u format=%u", static_cast<unsigned long long>(frame),
         written ? 1 : 0, width, height, static_cast<unsigned>(g_p.format));
}

void press_key_into(HWND window, unsigned vk, unsigned scan) {
  // The cursor in the window, the window in front and holding the focus; under Wine the focus events are let pass
  // before a key is injected, since they bring Wine's key state into line with the X server's (GraceUI's harness found
  // the same).
  // With --cursor-once the cursor is put there at the first press alone, as GraceUI's harness puts it once: under Wine
  // the SetCursorPos before each press reached GraceUI as a mouse move, even to where the cursor already was, and put
  // its focus ring away again after the key had shown it (G42).
  static bool placed = false;
  if (!placed || !options().cursor_once) {
    RECT client{};
    GetClientRect(window, &client);
    POINT middle{(client.left + client.right) / 2, (client.top + client.bottom) / 2};
    ClientToScreen(window, &middle);
    SetCursorPos(middle.x, middle.y);
    placed = true;
  }
  if (GetForegroundWindow() != window) SetForegroundWindow(window);
  if (GetFocus() != window) SetFocus(window);
  Sleep(100);
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  INPUT in[2] = {};
  in[0].type = INPUT_KEYBOARD;
  in[0].ki.wVk = static_cast<WORD>(vk);
  in[0].ki.wScan = static_cast<WORD>(scan);
  in[1] = in[0];
  in[1].ki.dwFlags = KEYEVENTF_KEYUP;
  const UINT down = SendInput(1, &in[0], sizeof(INPUT));
  // When the key went down, on the clock EldenNet's log uses (QPC in milliseconds, en-proto's watch::mono_ms), for
  // the talk key's latency to the chat box's first frame (NPC §5.1).
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  const auto counter = static_cast<unsigned long long>(c.QuadPart);
  const auto frequency = static_cast<unsigned long long>(f.QuadPart);
  status("key_down key=0x%02X qpc_ms=%llu", vk,
         frequency == 0 ? 0ull : (counter / frequency) * 1000ull + (counter % frequency) * 1000ull / frequency);
  // The key is held 60 ms, its messages taken as they come (a game's loop pumps them every frame).
  const ULONGLONG held = GetTickCount64() + 60;
  do {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(1);
  } while (GetTickCount64() < held);
  const UINT up = SendInput(1, &in[1], sizeof(INPUT));
  status("pressed key=0x%02X sent=%u focus=%d", vk, down + up, GetFocus() == window ? 1 : 0);
}

void type_into(HWND window, const std::wstring& text) {
  if (GetForegroundWindow() != window) SetForegroundWindow(window);
  if (GetFocus() != window) SetFocus(window);
  MSG msg;
  UINT sent = 0;
  for (const wchar_t ch : text) {
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wScan = static_cast<WORD>(ch);
    in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    sent += SendInput(2, in, sizeof(INPUT));
    Sleep(25);
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  status("typed chars=%u sent=%u focus=%d", static_cast<unsigned>(text.size()), sent, GetFocus() == window ? 1 : 0);
}

void grace_take_pause() {
  // Only the GraceUI already loaded (the launcher's --also, into the suspended process): loading it here, on the main
  // thread before the window, loads Direct3D's DLLs, which graceui.dll imports, and under Wine 9 that load hung when
  // eldennet.dll's threads were running (dxgi.dll's own load hung the same way).
  GraceUiSysFound found{};
  found.size = sizeof found;
  const graceui::Api api(graceui_sys_find_once(0, &found));
  if (!api) {
    status("grace_other found=0 installed=%u", found.installed);
    return;
  }
  GraceUiConnectAnswer answer{};
  const GraceUiStatus connected = api.connect(
      graceui::Mod{"fakegame-other", "Another mod", GRACEUI_VERSION_PACK(1, 0, 0), 0, "hotkeys"}, answer);
  std::uint32_t id = 0;
  const GraceUiStatus hotkey = connected == GRACEUI_OK
                                   ? api.register_hotkey(answer.consumer, "pause", "Pause", VK_PAUSE, 0, id)
                                   : GRACEUI_NOT_CONNECTED;
  status("grace_other found=1 connect=%u hotkey=%u id=%u", connected, hotkey, id);
}

void grace_lan() {
  // Only the GraceUI already loaded (the launcher's --also), as grace_take_pause: its server starts once a mod registers
  // pages, after this, and takes these addresses then.
  HMODULE grace = GetModuleHandleW(L"graceui.dll");
  const auto set = export_of<void (*)(const uint8_t*, uint32_t)>(grace, "GraceUI_Test_SetNetworkAddresses");
  static constexpr uint8_t kHome[4] = {127, 0, 0, 2};
  if (set != nullptr) set(kHome, 1);
  status("grace_lan found=%d set=%d", grace != nullptr ? 1 : 0, set != nullptr ? 1 : 0);
}

void grace_report(std::uint64_t frame) {
  HMODULE grace = reported_copy();
  report_copies(frame, grace);
  const auto get_state = export_of<uint32_t (*)(GraceUiTestState*)>(grace, "GraceUI_Test_GetState");
  const auto get_consumer = export_of<uint32_t (*)(uint32_t, GraceUiTestConsumer*)>(grace, "GraceUI_Test_GetConsumer");
  const auto get_notice = export_of<uint32_t (*)(uint32_t, GraceUiTestNotice*)>(grace, "GraceUI_Test_GetNotice");
  const auto get_input = export_of<uint32_t (*)(GraceUiTestInput*)>(grace, "GraceUI_Test_GetInput");
  if (get_state == nullptr || get_consumer == nullptr || get_notice == nullptr || get_input == nullptr) {
    status("grace frame=%llu found=%d", static_cast<unsigned long long>(frame), grace != nullptr ? 1 : 0);
    return;
  }
  GraceUiTestState st{};
  st.size = sizeof st;
  get_state(&st);
  GraceUiTestInput input{};
  input.size = sizeof input;
  get_input(&input);
  // EldenNet's connection, and its notices by key.
  uint32_t owner = 0;
  std::string caps = "none";
  for (uint32_t i = 0;; ++i) {
    GraceUiTestConsumer c{};
    c.size = sizeof c;
    if (get_consumer(i, &c) == 0) break;
    if (std::strcmp(c.id, "eldennet") == 0) {
      owner = c.owner;
      caps = word_of(c.caps);
    }
  }
  std::string banner = "none";
  std::string badge = "none";
  uint32_t banner_tone = 0;
  for (uint32_t i = 0;; ++i) {
    GraceUiTestNotice n{};
    n.size = sizeof n;
    if (get_notice(i, &n) == 0) break;
    if (owner == 0 || n.owner != owner) continue;
    if (n.key == 1) {
      banner = word_of(n.text);
      banner_tone = n.tone;
    } else if (n.key == 2) {
      badge = word_of(n.text);
    }
  }
  // GraceUI's in-game menu (D167, D171) and its browser view's server (D169), where the test build has them: the page
  // open and the element focused (our own ids), and the server's port and spaces.
  const auto get_menu = export_of<uint32_t (*)(GraceUiTestMenu*)>(grace, "GraceUI_Test_GetMenu");
  const auto get_server = export_of<uint32_t (*)(GraceUiTestServer*)>(grace, "GraceUI_Test_GetServer");
  GraceUiTestMenu menu{};
  menu.size = sizeof menu;
  const bool have_menu = get_menu != nullptr && get_menu(&menu) != 0;
  GraceUiTestServer server{};
  server.size = sizeof server;
  const bool have_server = get_server != nullptr && get_server(&server) != 0;
  char menu_part[320] = "";
  if (have_menu) {
    // menu_ring: the focus ring drawn (GraceUI 0.2.33's G42; 0 from an older test build, whose struct ends sooner).
    std::snprintf(menu_part, sizeof menu_part,
                  " menu_open=%u menu_page=%s menu_focused=%s menu_refusals=%u menu_ring=%u", menu.open,
                  menu.page[0] != 0 ? word_of(menu.page).c_str() : "none",
                  menu.focused[0] != 0 ? word_of(menu.focused).c_str() : "none", menu.refusals, menu.focus_shown);
  }
  char server_part[128] = "";
  if (have_server) {
    std::snprintf(server_part, sizeof server_part, " server=%u port=%u spaces=%u pending=%u", server.running,
                  server.port, server.spaces, server.pending);
  }
  // GraceUI 0.2.6's MenuShows (D184): for each text FAKEGAME_SHOWS names ('|' between them, at most four), whether the
  // menu's last layout draws it within one of its texts; `shows` holds a 1 or a 0 for each, in order. A case names
  // words it wants drawn, and fragments of what must never be (an invented key's) through the environment, so that
  // they stand in no command line.
  const auto menu_shows = export_of<uint32_t (*)(const char*)>(grace, "GraceUI_Test_MenuShows");
  char shows_part[16] = "";
  char wanted[512] = "";
  const DWORD got = GetEnvironmentVariableA("FAKEGAME_SHOWS", wanted, sizeof wanted);
  if (menu_shows != nullptr && got > 0 && got < sizeof wanted) {
    std::string bits;
    for (char* text = wanted; text != nullptr && bits.size() < 4;) {
      char* bar = std::strchr(text, '|');
      if (bar != nullptr) *bar = '\0';
      bits += menu_shows(text) != 0 ? '1' : '0';
      text = bar != nullptr ? bar + 1 : nullptr;
    }
    std::snprintf(shows_part, sizeof shows_part, " shows=%s", bits.c_str());
  }
  // The pairing sheet's code while one shows (G9; GraceUI 0.2.25's GraceUI_Test_GetPairingCode): six digits.
  const auto pairing_code = export_of<uint32_t (*)(char*, uint32_t)>(grace, "GraceUI_Test_GetPairingCode");
  char pairing_part[24] = "";
  char code[16] = "";
  if (pairing_code != nullptr && pairing_code(code, sizeof code) == 6) {
    std::snprintf(pairing_part, sizeof pairing_part, " pairing=%s", code);
  }
  char tail[240] = "";
  const Options& o = options();
  std::size_t at = 0;
  for (std::size_t i = 0; i < o.grace_probes.size() && at < sizeof tail; ++i) {
    std::uint64_t matching = 0;
    std::uint64_t area = 0;
    const Options::Probe& pr = o.grace_probes[i];
    if (!probe(pr.rect, pr.colour, static_cast<int>(pr.tolerance), matching, area)) continue;
    const std::string n = i == 0 ? "" : std::to_string(i + 1);  // probe, probe2, probe3
    const int k = std::snprintf(tail + at, sizeof tail - at, " probe%s=%llu probe%s_area=%llu", n.c_str(),
                                static_cast<unsigned long long>(matching), n.c_str(),
                                static_cast<unsigned long long>(area));
    if (k > 0) at += static_cast<std::size_t>(k);
  }
  status("grace frame=%llu found=1 host=%u notice_only=%u consumers=%u eldennet=%d caps=%s notices=%u banner=%s "
         "banner_tone=%u badge=%s panels=%u shown=%u submitted=%llu refused=%llu panel_frames=%llu textures=%u "
         "world=0x%X providers=%u window_filter=%u presents=%llu holder=%u holder_kind=%u closes=%u close_reason=%u"
         " game_keys=%u game_chars=%u%s%s%s%s%s",
         static_cast<unsigned long long>(frame), st.host, st.notice_only, st.consumers, owner != 0 ? 1 : 0,
         caps.c_str(), st.notices, banner.c_str(), banner_tone, badge.c_str(), st.panels, st.panels_shown,
         static_cast<unsigned long long>(st.lists_submitted), static_cast<unsigned long long>(st.lists_refused),
         static_cast<unsigned long long>(st.panel_frames), st.textures, st.world_flags, st.providers,
         input.window_filter, static_cast<unsigned long long>(st.presents),
         input.holder_owner == 0 ? 0u : input.holder_owner == owner ? 2u : 1u, input.holder_kind, input.closes,
         input.last_close_reason, game_keys(), game_chars(), menu_part, server_part, shows_part, pairing_part, tail);
}

void grace_copy(std::uint64_t frame) {
  // Another mod's private copy: the first copy loaded that is not the game's own GraceUI\graceui.dll. Asked for ABI 1's
  // table as that mod would ask it (an idle copy answers null, D72); the mod then connects once, to that table if it
  // answered, else to the GraceUI graceui-sys finds (by name, then its scan), as the other mod of --grace-take-pause.
  static bool connected = false;
  const std::vector<GraceCopy> copies = grace_copies();
  const GraceCopy* own = nullptr;
  for (const GraceCopy& c : copies) {
    if (own == nullptr && lstrcmpiA(c.name.c_str(), "GraceUI\\graceui.dll") != 0) own = &c;
  }
  if (own == nullptr) {
    status("grace_copy frame=%llu found=0 copies=%zu", static_cast<unsigned long long>(frame), copies.size());
    return;
  }
  const auto request = export_of<GraceUiRequestApiFn>(own->module, "GraceUI_RequestApi");
  const void* table = request != nullptr ? request(GRACEUI_ABI_MAJOR) : nullptr;
  const char* look = "none";
  char connect[16] = "none";  // the connection's status, where this ask made one
  if (!connected) {
    GraceUiSysFound found{};
    found.size = sizeof found;
    const GraceUiApi1* api = static_cast<const GraceUiApi1*>(table);
    look = "own";
    if (api == nullptr) {
      api = graceui_sys_find_once(0, &found);
      look = found.how == GRACEUI_SYS_BY_NAME ? "name" : found.how == GRACEUI_SYS_BY_SCAN ? "scan" : "none";
    }
    const graceui::Api grace(api);
    GraceUiConnectAnswer answer{};
    const graceui::Mod mod{"fakegame-other", "Another mod", GRACEUI_VERSION_PACK(1, 0, 0), 0, "notices"};
    const GraceUiStatus st = grace ? grace.connect(mod, answer) : GRACEUI_NOT_CONNECTED;
    connected = st == GRACEUI_OK;
    std::snprintf(connect, sizeof connect, "%u", st);
  }
  status("grace_copy frame=%llu found=1 name=%s table=%d look=%s connect=%s copies=%zu",
         static_cast<unsigned long long>(frame), own->name.c_str(), table != nullptr ? 1 : 0, look, connect,
         copies.size());
}

void pad_chord_frame(std::uint64_t frame) {
  const Options& o = options();
  if (!g_chord.on) {
    bool starts = false;
    for (const std::uint64_t at : o.pad_chord_at) starts = starts || at == frame;
    if (!starts) return;
    g_chord = ChordRun{};
    g_chord.on = true;
    g_chord.frame = frame;
    g_chord.started = seconds_now();
    g_chord.pad_read = export_of<PadReadFn>(reported_copy(), "GraceUI_Test_PadRead");
    if (g_chord.pad_read == nullptr) {
      status("pad_chord frame=%llu found=0", static_cast<unsigned long long>(frame));
      g_chord.on = false;
      return;
    }
  }
  // One read a frame, as a game reads XInput once a frame: Up alone, then Up and View, then neither, which ends it.
  const double elapsed = seconds_now() - g_chord.started;
  const std::uint32_t buttons = elapsed < kChordAnchorS                ? kPadUp
                                : elapsed < kChordAnchorS + kChordBothS ? kPadUp | kPadView
                                                                         : 0u;
  std::uint32_t packet = kPadPacket;
  const std::uint32_t seen = g_chord.pad_read(0, buttons, &packet);
  ++g_chord.reads;
  g_chord.up_seen += (seen & kPadUp) != 0 ? 1u : 0u;
  g_chord.view_seen += (seen & kPadView) != 0 ? 1u : 0u;
  g_chord.view_reads += (buttons & kPadView) != 0 ? 1u : 0u;
  g_chord.packets += packet != kPadPacket ? 1u : 0u;
  if (buttons == 0) {
    status("pad_chord frame=%llu end_frame=%llu reads=%u view_reads=%u up_seen=%u view_seen=%u packets=%u ms=%.0f",
           static_cast<unsigned long long>(g_chord.frame), static_cast<unsigned long long>(frame), g_chord.reads,
           g_chord.view_reads, g_chord.up_seen, g_chord.view_seen, g_chord.packets, elapsed * 1000.0);
    g_chord.on = false;
  }
}

}  // namespace fakegame
