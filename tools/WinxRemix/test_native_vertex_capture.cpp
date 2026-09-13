// Own bounded CPU fixture: original-call ABI and vertex provenance only.
// Synthetic owned headers/arrays; no game, COM methods, bridge, or GPU calls.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
#include "../../Sparkplug/Code/SparkplugDX/spPCDXVertexBytes.h"
#include <array>
#include <cstddef>
#include <functional>
#include <stdexcept>

namespace vertex_capture_test {
namespace source = native_mesh_source;
namespace abi = sparkplug::evidence::pc;
static unsigned checks = 0, originalCalls = 0;
static void Check(bool condition, const char* message) {
  ++checks; if (!condition) throw std::runtime_error(message);
}
static uint32_t Ptr(const void* value) { return uint32_t(reinterpret_cast<uintptr_t>(value)); }
struct Watchdog {
  HANDLE event = nullptr, thread = nullptr;
  static DWORD WINAPI Wait(void* event) {
    if (WaitForSingleObject(event, 30000) == WAIT_TIMEOUT) TerminateProcess(GetCurrentProcess(), 0xe0520c30u);
    return 0;
  }
  Watchdog() {
    event = CreateEventW(nullptr, TRUE, FALSE, nullptr); Check(event != nullptr, "watchdog event");
    thread = CreateThread(nullptr, 0, Wait, event, 0, nullptr); Check(thread != nullptr, "watchdog thread");
  }
  ~Watchdog() { SetEvent(event); WaitForSingleObject(thread, 1000); CloseHandle(thread); CloseHandle(event); }
};
struct NoAccess {
  void* value = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
  NoAccess() { Check(value != nullptr && Ptr(value) < 0x7fff0000u, "owned unreadable page"); }
  ~NoAccess() { if (value) VirtualFree(value, 0, MEM_RELEASE); }
};
static void ClearCapture() {
  source::buffers.clear(); source::retainedBytes = 0;
}
static void Word(std::vector<uint8_t>& bytes, size_t index, uint32_t value) {
  memcpy(bytes.data() + index * 4, &value, 4);
}
static uint32_t FloatWord(float value) { uint32_t word = 0; memcpy(&word, &value, 4); return word; }

struct Fixture {
  abi::spVertexBufferLayout vertices{};
  abi::spIndexBufferLayout indices{};
  abi::spDXMeshObservedLayout mesh{}, resultMesh{};
  abi::spDXVertexBufferLayout vb{}, aliasVB{}, resultVB{};
  abi::spDXIndexBufferLayout ib{}, resultIB{};
  std::vector<uint8_t> input, expected;
  std::array<uint16_t, 3> inputIndices{{2, 0, 1}};
  uint32_t tokens[3]{0xdead1001, 0xdead1002, 0xdead1003};
  uint32_t indexArgument = 0, vertexArgument = 0, keepArgument = 0;
  uint32_t result = 0xc0ffee7du;
  bool clobberInOriginal = false;
  unsigned calls = 0;
  abi::spVertexBufferLayout beforeVertices{};
  abi::spIndexBufferLayout beforeIndices{};
  std::vector<uint8_t> beforeInput;
  std::array<uint16_t, 3> beforeIndexBytes{};

  void* VB() { return &tokens[0]; }
  void* IB() { return &tokens[1]; }
  void* Interface() { return reinterpret_cast<uint8_t*>(&mesh) + 0x14; }
  explicit Fixture(bool packed = true, bool weighted = true) {
    const uint32_t prefixWords = weighted ? 4u : 3u;
    const uint32_t sourceWords = prefixWords + (packed ? 1u : 0u) + 6u;
    const uint32_t destinationWords = prefixWords + (packed ? 4u : 0u) + 6u;
    input.resize(3 * sourceWords * 4); expected.resize(3 * destinationWords * 4);
    const float palettes[3][4] = {{0.f, 1.f, 127.f, 255.f}, {3.f, 2.f, 254.f, 128.f}, {17.f, 31.f, 63.f, 255.f}};
    const uint32_t packedWords[3] = {0xff7f0100u, 0x80fe0203u, 0xff3f1f11u};
    for (unsigned v = 0; v < 3; ++v) {
      const size_t a = v * sourceWords, b = v * destinationWords;
      for (unsigned coordinate = 0; coordinate < 3; ++coordinate) {
        const auto value = FloatWord(float(v * 3 + coordinate + 1));
        Word(input, a + coordinate, value); Word(expected, b + coordinate, value);
      }
      if (weighted) { Word(input, a + 3, FloatWord(.25f * (v + 1))); Word(expected, b + 3, FloatWord(.25f * (v + 1))); }
      if (packed) {
        Word(input, a + prefixWords, packedWords[v]);
        for (unsigned component = 0; component < 4; ++component)
          Word(expected, b + prefixWords + component, FloatWord(palettes[v][component]));
      }
      const uint32_t tail[6] = {0x80000000u, FloatWord(1.f), FloatWord(0.f),
        0x7f123456u + v, FloatWord(.125f + .25f * v), FloatWord(.25f * (v + 1))};
      for (unsigned t = 0; t < 6; ++t) {
        Word(input, a + prefixWords + (packed ? 1 : 0) + t, tail[t]);
        Word(expected, b + prefixWords + (packed ? 4 : 0) + t, tail[t]);
      }
    }
    vertices.base.vtableAddress = abi::spVertexBufferVTable;
    vertices.vertexStride = uint16_t(sourceWords * 4); vertices.componentCount = uint16_t(sourceWords);
    vertices.componentFlags = 0x940u | (weighted ? 2u : 0u) | (packed ? 0x20u : 0u);
    vertices.componentOffsets[6] = uint16_t(prefixWords); // spDXMesh::PackedFieldComponentOffsetIndex is 6.
    vertices.vertexCount = 3; vertices.vertexSize = uint32_t(input.size()); vertices.initialized = 1;
    vertices.vertexData = Ptr(input.data());
    indices.base.vtableAddress = abi::spIndexBufferVTable; indices.initialized = 1;
    indices.type = 2; indices.primitiveCount = 1; indices.indexCount = 3; indices.indexData = Ptr(inputIndices.data());
    indexArgument = Ptr(&indices); vertexArgument = Ptr(&vertices);
    resultMesh.base.base.base.base.base.vtableAddress = abi::spDXMeshVTable;
    resultMesh.base.base.secondaryVTable = abi::spDXMeshInterfaceVTable;
    resultMesh.base.base.vertexComponentFlags = vertices.componentFlags;
    resultMesh.base.base.vertexCount = vertices.vertexCount; resultMesh.base.base.primitiveCount = indices.primitiveCount;
    resultMesh.vertexStride = destinationWords * 4; resultMesh.componentWeightCount = weighted ? 1u : 0u;
    resultMesh.indexType = indices.type; resultMesh.vertexBegin = 1; resultMesh.indexBegin = 1;
    resultMesh.vertexBuffer = Ptr(&vb); resultMesh.indexBuffer = Ptr(&ib);
    resultVB.base.vtableAddress = abi::spDXVertexBufferVTable; resultVB.direct3DVertexBuffer = Ptr(VB());
    resultVB.byteSize = resultMesh.vertexStride * 12;
    resultIB.base.vtableAddress = abi::spDXIndexBufferVTable; resultIB.direct3DIndexBuffer = Ptr(IB()); resultIB.byteSize = 32;
    Check(Ptr(input.data()) < 0x7fff0000u && Ptr(inputIndices.data()) < 0x7fff0000u, "CPU fixture arrays in readable x86 range");
  }
  void Snapshot() {
    beforeVertices = vertices; beforeIndices = indices; beforeInput = input; beforeIndexBytes = inputIndices;
  }
  void Unchanged() {
    Check(!memcmp(&beforeVertices, &vertices, sizeof(vertices)) && !memcmp(&beforeIndices, &indices, sizeof(indices)),
      "capture preserves authored source headers");
    Check(beforeInput == input && beforeIndexBytes == inputIndices, "capture preserves authored vertex and index bytes");
  }
};
static Fixture* current;
static uint32_t __fastcall Original(void* object, void*, uint32_t indices, uint32_t vertices, uint32_t keep) {
  auto& fixture = *current; ++fixture.calls; ++originalCalls;
  Check(object == fixture.Interface() && indices == fixture.indexArgument && vertices == fixture.vertexArgument && keep == fixture.keepArgument,
    "original receives exact this and all original arguments");
  fixture.Unchanged(); // This runs after capture, before the mocked original writes anything.
  fixture.mesh = fixture.resultMesh; fixture.vb = fixture.resultVB; fixture.aliasVB = fixture.resultVB; fixture.ib = fixture.resultIB;
  if (fixture.clobberInOriginal) {
    std::fill(fixture.input.begin(), fixture.input.end(), uint8_t(0xee)); fixture.inputIndices.fill(0xffff);
  }
  return fixture.result;
}
static void Invoke(Fixture& fixture) {
  fixture.Snapshot(); current = &fixture;
  const auto before = fixture.calls;
  const auto result = source::MeshInitialize(fixture.Interface(), nullptr, fixture.indexArgument, fixture.vertexArgument, fixture.keepArgument);
  current = nullptr;
  Check(fixture.calls == before + 1, "exactly one original MeshInitialize call");
  Check(result == fixture.result, "full original EAX preserved, not normalized AL");
  if (!fixture.clobberInOriginal) fixture.Unchanged();
}
static void Captured(Fixture& fixture) {
  Check(source::buffers.size() == 2, "one current VB/IB provenance pair");
  const auto& v = source::buffers.at(fixture.VB()); const auto& i = source::buffers.at(fixture.IB());
  Check(v.generation && v.generation == i.generation, "pair shares nonzero completed capture generation");
  Check(v.nativeBuffer == fixture.resultMesh.vertexBuffer && i.nativeBuffer == fixture.resultMesh.indexBuffer && !v.owner && !i.owner,
    "captured native headers retained and combiner owner remains zero");
  Check(v.partial && i.partial && !v.verified && !i.verified, "new range remains partial and upload-unverified");
  Check(v.data.size() == fixture.resultVB.byteSize && i.data.size() == fixture.resultIB.byteSize &&
    source::retainedBytes == v.data.size() + i.data.size(), "combined buffer sizes and accounting match native headers");
  const size_t offset = size_t(fixture.resultMesh.vertexBegin) * fixture.resultMesh.vertexStride;
  const size_t indexOffset = size_t(fixture.resultMesh.indexBegin) * 2;
  Check(source::Covered(v, offset, fixture.expected.size()) && source::Covered(i, indexOffset, sizeof(fixture.beforeIndexBytes)),
    "full uploaded ranges have captured provenance");
  Check(!memcmp(v.data.data() + offset, fixture.expected.data(), fixture.expected.size()),
    "captured bytes equal literal float4 expansion and preserve all authored tail bits");
  Check(!memcmp(i.data.data() + indexOffset, fixture.beforeIndexBytes.data(), sizeof(fixture.beforeIndexBytes)),
    "captured index16 bytes precede original mutation");
}
static void PositiveCases() {
  for (bool packed : {false, true}) for (bool weighted : {false, true}) {
    ClearCapture(); Fixture fixture(packed, weighted); Invoke(fixture); Captured(fixture);
    Check(fixture.resultMesh.vertexStride == fixture.vertices.vertexStride + (packed ? 12u : 0u), "packed field adds exactly twelve stride bytes");
    if (weighted) {
      source::Scope scope{}; scope.valid = true; scope.value = fixture.mesh;
      source::active = &scope; source::Geometry geometry{};
      Check(!source::Resolve(nullptr, {}, fixture.VB(), fixture.IB(), 0, fixture.mesh.vertexStride, geometry),
        "weighted provenance does not enable ordinary Resolve or any COM method");
      source::active = nullptr;
    }
    source::Forget(fixture.VB()); Check(!source::buffers.count(fixture.VB()) && source::retainedBytes == fixture.resultIB.byteSize,
      "observed VB release/write removes vertex provenance");
    source::Forget(fixture.IB()); Check(source::buffers.empty() && !source::retainedBytes, "observed IB release removes remaining provenance");
  }
  Fixture clobbered; clobbered.clobberInOriginal = true; Invoke(clobbered); Captured(clobbered);
  Check(clobbered.input != clobbered.beforeInput, "mock original really replaced the authored bytes after capture");
  ClearCapture();
}
static void CombinedRanges() {
  Fixture fixture; Invoke(fixture); Captured(fixture);
  const auto generation = source::buffers.at(fixture.VB()).generation;
  fixture.resultMesh.vertexBegin = 6; fixture.resultMesh.indexBegin = 6;
  source::buffers.at(fixture.VB()).verified = source::buffers.at(fixture.IB()).verified = true;
  Invoke(fixture); Captured(fixture);
  auto& v = source::buffers.at(fixture.VB()); auto& i = source::buffers.at(fixture.IB());
  const size_t stride = fixture.mesh.vertexStride;
  Check(v.generation > generation && v.ranges.size() == 2 && i.ranges.size() == 2, "disjoint combined ranges retain both records and advance generation");
  Check(source::Covered(v, stride, 3 * stride) && source::Covered(v, 6 * stride, 3 * stride) && !source::Covered(v, 4 * stride, stride),
    "combined vertex provenance retains first range and excludes the gap");
  Check(source::Covered(i, 2, 6) && source::Covered(i, 12, 6) && !source::Covered(i, 8, 2), "combined index provenance excludes the gap");
  Check(!memcmp(v.data.data() + stride, fixture.expected.data(), fixture.expected.size()), "later capture preserves earlier combined bytes");
  fixture.resultMesh.vertexBegin = 4; fixture.resultMesh.indexBegin = 4; Invoke(fixture); Captured(fixture);
  Check(v.ranges.size() == 1 && i.ranges.size() == 1 && source::Covered(v, stride, 8 * stride) && source::Covered(i, 2, 16),
    "bridging ranges merge coverage without claiming unused tails");
  ++i.generation; fixture.resultMesh.vertexBegin = 1; fixture.resultMesh.indexBegin = 1; Invoke(fixture); Captured(fixture);
  // The old v/i references expired when the mismatched pair was replaced.
  Check(!source::Covered(source::buffers.at(fixture.VB()), 6 * stride, stride), "mismatched old pair generation cannot retain previous ranges");
  fixture.resultMesh.vertexBuffer = Ptr(&fixture.aliasVB); Invoke(fixture); Captured(fixture);
  Check(source::buffers.at(fixture.VB()).nativeBuffer == Ptr(&fixture.aliasVB), "same COM address can bind only newly captured native header");
  ClearCapture();
}
static void Rejected(const char* message, const std::function<void(Fixture&)>& mutate) {
  ClearCapture(); Fixture fixture; mutate(fixture);
  const auto before = source::captures; Invoke(fixture);
  Check(source::buffers.empty() && !source::retainedBytes && source::captures == before, message);
}
static void RejectionCases() {
  Rejected("uninitialized authored VB rejects", [](Fixture& f) { f.vertices.initialized = 0; });
  Rejected("uninitialized authored IB rejects", [](Fixture& f) { f.indices.initialized = 0; });
  Rejected("foreign authored VB header rejects", [](Fixture& f) { ++f.vertices.base.vtableAddress; });
  Rejected("foreign authored IB header rejects", [](Fixture& f) { ++f.indices.base.vtableAddress; });
  Rejected("index32 remains outside captured index16 cohort", [](Fixture& f) { f.indices.formatFlags = 1; });
  Rejected("source byte extent shorter than count times stride rejects", [](Fixture& f) { --f.vertices.vertexSize; });
  Rejected("packed field outside authored stride rejects", [](Fixture& f) { f.vertices.componentOffsets[6] = f.vertices.vertexStride / 4; });
  Rejected("zero vertex count produces no capture", [](Fixture& f) { f.vertices.vertexCount = 0; });
  Rejected("overflow vertex count cannot request wrapped read", [](Fixture& f) { f.vertices.vertexCount = UINT32_MAX; });
  Rejected("overflow index count cannot request wrapped read", [](Fixture& f) { f.indices.indexCount = UINT32_MAX; });
  Rejected("null vertex data rejects safely", [](Fixture& f) { f.vertices.vertexData = 0; });
  Rejected("null index data rejects safely", [](Fixture& f) { f.indices.indexData = 0; });
  Rejected("post-init mesh primary identity required", [](Fixture& f) { ++f.resultMesh.base.base.base.base.base.vtableAddress; });
  Rejected("post-init vertex stride must match transformed stream", [](Fixture& f) { --f.resultMesh.vertexStride; });
  Rejected("post-init vertex count must match captured input", [](Fixture& f) { --f.resultMesh.base.base.vertexCount; });
  Rejected("post-init component flags must match captured input", [](Fixture& f) { f.resultMesh.base.base.vertexComponentFlags ^= 2; });
  Rejected("post-init index type must match captured input", [](Fixture& f) { f.resultMesh.indexType = 3; });
  Rejected("shared initializer ownership is not inferred from MeshInitialize", [](Fixture& f) { f.resultMesh.sharedMeshData = Ptr(&f.tokens[2]); });
  Rejected("post-init VB header identity required", [](Fixture& f) { ++f.resultVB.base.vtableAddress; });
  Rejected("post-init IB header identity required", [](Fixture& f) { ++f.resultIB.base.vtableAddress; });
  Rejected("nonnull output VB COM identity required", [](Fixture& f) { f.resultVB.direct3DVertexBuffer = 0; });
  Rejected("nonnull output IB COM identity required", [](Fixture& f) { f.resultIB.direct3DIndexBuffer = 0; });
  Rejected("one COM address cannot identify both VB and IB", [](Fixture& f) { f.resultIB.direct3DIndexBuffer = f.resultVB.direct3DVertexBuffer; });
  Rejected("transformed vertex range must fit combined buffer", [](Fixture& f) { f.resultVB.byteSize = uint32_t(f.expected.size()); });
  Rejected("index range must fit combined buffer", [](Fixture& f) { f.resultIB.byteSize = 6; });
  Rejected("native buffer storage remains bounded", [](Fixture& f) { f.resultVB.byteSize = 8 * 1024 * 1024 + 1; });
  Rejected("overflow vertex offset rejects", [](Fixture& f) { f.resultMesh.vertexBegin = UINT32_MAX; });
  Rejected("overflow index offset rejects", [](Fixture& f) { f.resultMesh.indexBegin = UINT32_MAX; });
  Rejected("original AL zero rejects capture but preserves EAX", [](Fixture& f) { f.result = 0xabcdef00u; });
  NoAccess unreadable;
  Rejected("unreadable authored VB header still calls original once", [&](Fixture& f) { f.vertexArgument = Ptr(unreadable.value); });
  Rejected("unreadable authored IB header still calls original once", [&](Fixture& f) { f.indexArgument = Ptr(unreadable.value); });
  Rejected("unreadable vertex bytes still call original once", [&](Fixture& f) { f.vertices.vertexData = Ptr(unreadable.value); });
  Rejected("unreadable index bytes still call original once", [&](Fixture& f) { f.indices.indexData = Ptr(unreadable.value); });
  Rejected("unreadable output VB header rejects after original", [&](Fixture& f) { f.resultMesh.vertexBuffer = Ptr(unreadable.value); });
  source::enabled = false; Rejected("disabled capture transparently calls original once", [](Fixture&) {}); source::enabled = true;
  const auto thread = source::ownerThread; source::ownerThread = thread + 1;
  Rejected("foreign capture thread transparently calls original once", [](Fixture&) {}); source::ownerThread = thread;
  Fixture valid; Invoke(valid); Captured(valid); ClearCapture();
}
static void HelperCases() {
  for (bool packed : {false, true}) {
    Fixture f(packed); const auto before = f.input;
    std::vector<uint8_t> bytes; uint32_t stride = 0;
    Check(sparkplug::reconstruction::BuildPCDXVertexBytesForAnalysis(f.input.data(), f.input.size(), f.vertices.vertexStride,
      f.vertices.vertexCount, f.vertices.componentFlags, f.vertices.componentOffsets[6], bytes, stride), "shared byte helper accepts owned authored stream");
    Check(bytes == f.expected && stride == f.resultMesh.vertexStride && f.input == before, "shared byte helper matches literal bytes without writing source");
    std::vector<std::byte> portable; uint32_t portableStride = 0;
    Check(sparkplug::reconstruction::BuildPCDXVertexBytesForAnalysis(f.input.data(), f.input.size(), f.vertices.vertexStride,
      f.vertices.vertexCount, f.vertices.componentFlags, f.vertices.componentOffsets[6], portable, portableStride), "shared helper supports recovered std::byte destination");
    Check(portable.size() == bytes.size() && portableStride == stride && !memcmp(portable.data(), bytes.data(), bytes.size()), "portable and observer helper instantiations agree");
    Check(!sparkplug::reconstruction::BuildPCDXVertexBytesForAnalysis(f.input.data(), f.input.size() - 1, f.vertices.vertexStride,
      f.vertices.vertexCount, f.vertices.componentFlags, f.vertices.componentOffsets[6], bytes, stride), "shared helper rejects insufficient source extent");
  }
  Fixture f; std::vector<uint8_t> bytes; uint32_t stride = 0;
  Check(!sparkplug::reconstruction::BuildPCDXVertexBytesForAnalysis(f.input.data(), f.input.size(), f.vertices.vertexStride,
    f.vertices.vertexCount, f.vertices.componentFlags, f.vertices.vertexStride / 4, bytes, stride), "shared helper rejects packed offset with no four-byte field");
}
static void Run() {
  Check(!source::enabled && source::buffers.empty() && !source::active && !testRemixApi, "fresh CPU-only source state");
  const auto original = source::originalMeshInitialize; const auto thread = source::ownerThread;
  source::originalMeshInitialize = reinterpret_cast<source::NativeMeshInitialize>(&Original);
  source::ownerThread = GetCurrentThreadId(); source::enabled = true;
  HelperCases(); PositiveCases(); CombinedRanges(); RejectionCases();
  source::originalMeshInitialize = original; source::ownerThread = thread; source::enabled = false;
  source::layouts.clear(); ClearCapture();
  Check(!source::active && !testRemixApi && source::buffers.empty(), "no leaked source scope, API, or provenance cache");
}
}
int main() {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  try {
    vertex_capture_test::Watchdog watchdog; vertex_capture_test::Run();
    printf("{\"status\":\"PASS\",\"checks\":%u,\"originalCalls\":%u,\"gpu\":false,\"nativeGameCodeExecuted\":false,\"comMethodsCalled\":0}\n",
      vertex_capture_test::checks, vertex_capture_test::originalCalls); return 0;
  } catch (const std::exception& error) { fprintf(stderr, "FAIL %s\n", error.what()); return 1; }
}
