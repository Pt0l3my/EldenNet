// The fake game's GraceUI parts (program.hpp; part 12): a Direct3D 12 swap chain it presents every frame, so that
// GraceUI's overlay binds it and draws on it, as a game's would be; F11 pressed into its window; and a report of what a
// test build of GraceUI says through its test exports (graceui_test.h), with a count of one colour's pixels in a
// rectangle of the frame just presented, read back after GraceUI drew on it; and, standing in for another mod, a
// connection of its own with a hotkey on Pause. Our own code, written for this fixture; nothing in it is a game's or
// GraceUI's.
#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "graceui_sys.hpp"
#include "graceui_test.h"
#include "program.hpp"

namespace fakegame {

namespace {

constexpr UINT kWidth = 960;  // wide enough for EldenNet's F11 view at the top left (680 by 500 at 24, 24)
constexpr UINT kHeight = 540;
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

// The newest presented frame, read back once the GPU is done with it (GraceUI's drawing included): the pixels within
// the rectangle that are within 3 of the colour on each channel, and the rectangle's area.
bool probe(const std::uint32_t rect[4], const std::uint32_t colour[3], int tolerance, std::uint64_t& matching,
           std::uint64_t& area) {
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
  const auto* rows = static_cast<const unsigned char*>(mapped) + fp.Offset;
  matching = 0;
  area = 0;
  for (UINT y = rect[1]; y < rect[3] && y < desc.Height; ++y) {
    for (UINT x = rect[0]; x < rect[2] && x < desc.Width; ++x) {
      const unsigned char* px = rows + static_cast<std::size_t>(y) * fp.Footprint.RowPitch + x * 4u;
      ++area;
      bool close = true;
      for (int c = 0; c < 3; ++c) {
        const int d = static_cast<int>(px[c]) - static_cast<int>(colour[c]);
        close = close && d >= -tolerance && d <= tolerance;
      }
      matching += close ? 1 : 0;
    }
  }
  const D3D12_RANGE none{0, 0};
  g_p.readback->Unmap(0, &none);
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
  DXGI_SWAP_CHAIN_DESC1 sd{};
  sd.Width = kWidth;
  sd.Height = kHeight;
  sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
  g_p.readback_bytes = 4ull * 1024 * kHeight + 65536;  // rows padded to 256 bytes, with room
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
  g_p.on = true;
  status("d3d12 ok=1 width=%u height=%u buffers=%u", kWidth, kHeight, kBuffers);
  return true;
}

void present_frame() {
  if (!g_p.on) return;
  const UINT i = g_p.chain->GetCurrentBackBufferIndex();
  wait_for(g_p.buffer_fence[i]);
  g_p.allocators[i]->Reset();
  g_p.list->Reset(g_p.allocators[i], nullptr);
  barrier(g_p.buffers[i], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_p.rtv_heap->GetCPUDescriptorHandleForHeapStart();
  rtv.ptr += static_cast<SIZE_T>(i) * g_p.rtv_step;
  g_p.list->ClearRenderTargetView(rtv, kClear, 0, nullptr);
  barrier(g_p.buffers[i], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  g_p.list->Close();
  ID3D12CommandList* lists[] = {g_p.list};
  g_p.queue->ExecuteCommandLists(1, lists);
  g_p.chain->Present(0, 0);
  g_p.queue->Signal(g_p.fence, ++g_p.fence_value);
  g_p.buffer_fence[i] = g_p.fence_value;
  g_p.last = i;
}

void press_key_into(HWND window, unsigned vk, unsigned scan) {
  // The cursor in the window, the window in front and holding the focus; under Wine the focus events are let pass
  // before a key is injected, since they bring Wine's key state into line with the X server's (GraceUI's harness found
  // the same).
  RECT client{};
  GetClientRect(window, &client);
  POINT middle{(client.left + client.right) / 2, (client.top + client.bottom) / 2};
  ClientToScreen(window, &middle);
  SetCursorPos(middle.x, middle.y);
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

void grace_report(std::uint64_t frame) {
  HMODULE grace = GetModuleHandleW(L"graceui.dll");
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
    std::snprintf(menu_part, sizeof menu_part, " menu_open=%u menu_page=%s menu_focused=%s menu_refusals=%u", menu.open,
                  menu.page[0] != 0 ? word_of(menu.page).c_str() : "none",
                  menu.focused[0] != 0 ? word_of(menu.focused).c_str() : "none", menu.refusals);
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
         " game_keys=%u game_chars=%u%s%s%s%s",
         static_cast<unsigned long long>(frame), st.host, st.notice_only, st.consumers, owner != 0 ? 1 : 0,
         caps.c_str(), st.notices, banner.c_str(), banner_tone, badge.c_str(), st.panels, st.panels_shown,
         static_cast<unsigned long long>(st.lists_submitted), static_cast<unsigned long long>(st.lists_refused),
         static_cast<unsigned long long>(st.panel_frames), st.textures, st.world_flags, st.providers,
         input.window_filter, static_cast<unsigned long long>(st.presents),
         input.holder_owner == 0 ? 0u : input.holder_owner == owner ? 2u : 1u, input.holder_kind, input.closes,
         input.last_close_reason, game_keys(), game_chars(), menu_part, server_part, shows_part, tail);
}

}  // namespace fakegame
