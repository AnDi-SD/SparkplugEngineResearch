// Own CPU contract fixture: no DLL loading, D3D device, game or GPU.
#define REMIX_ALLOW_X86
#define REMIX_WINAPI_NO_LIBRARY_LOADER
#include "winx_remix_api_session.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <type_traits>

using winx_remix::RemixApiSession;
static unsigned checks, calls;
static bool provideConfig = true;
static remixapi_ErrorCode reply = REMIXAPI_ERROR_CODE_SUCCESS;
static RemixApiSession* reentrantSession;

static void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static remixapi_ErrorCode REMIXAPI_CALL Config(const char*, const char*) {
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL Camera(const remixapi_CameraInfo*) {
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL Initialize(
    const remixapi_InitializeLibraryInfo* info, remixapi_Interface* table) {
  ++calls;
  Check(info && table, "initialization has both inputs");
  Check(info->sType == REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO, "SDK struct type");
  Check(!info->pNext, "no accidental extension chain");
  Check(info->version == REMIXAPI_VERSION_MAKE(
    REMIXAPI_VERSION_MAJOR, REMIXAPI_VERSION_MINOR, REMIXAPI_VERSION_PATCH), "pinned header version");
  if (reentrantSession) {
    Check(reentrantSession->Attempted(), "attempt published before external call");
    Check(!reentrantSession->Get(), "no table exposed before initializer writes it");
    Check(reentrantSession->Initialize(&Initialize) == REMIXAPI_ERROR_CODE_NOT_INITIALIZED,
      "reentry does not invoke initializer again");
  }
  table->SetupCamera = &Camera;
  if (provideConfig) table->SetConfigVariable = &Config;
  return reply;
}

int main() {
  static_assert(!std::is_copy_constructible_v<RemixApiSession>);
  static_assert(!std::is_copy_assignable_v<RemixApiSession>);
  RemixApiSession success;
  Check(!success.Attempted() && !success.Get(), "fresh session");
  Check(success.Result() == REMIXAPI_ERROR_CODE_NOT_INITIALIZED, "fresh result");
  reentrantSession = &success;
  Check(success.Initialize(&Initialize) == REMIXAPI_ERROR_CODE_SUCCESS, "successful init");
  reentrantSession = nullptr;
  Check(calls == 1 && success.Attempted(), "single external init including reentry");
  auto* table = success.Get();
  Check(table && table->SetupCamera == &Camera && table->SetConfigVariable == &Config,
    "successful table preserved");
  Check(table->SetupCamera(nullptr) == REMIXAPI_ERROR_CODE_SUCCESS, "returned table is callable");
  Check(success.Initialize(nullptr) == REMIXAPI_ERROR_CODE_SUCCESS && success.Get() == table,
    "repeat keeps the same successful table");
  Check(calls == 1, "repeat makes no external call");

  for (const auto failure : {REMIXAPI_ERROR_CODE_INCOMPATIBLE_VERSION, REMIXAPI_ERROR_CODE_GENERAL_FAILURE}) {
    RemixApiSession failed;
    reply = failure;
    const auto before = calls;
    Check(failed.Initialize(&Initialize) == failure && failed.Result() == failure, "exact failure retained");
    Check(failed.Attempted() && !failed.Get(), "partially written table discarded after failure");
    reply = REMIXAPI_ERROR_CODE_SUCCESS;
    Check(failed.Initialize(&Initialize) == failure && !failed.Get(), "failure is not silently retried");
    Check(calls == before + 1, "failed session called initializer only once");
  }
  RemixApiSession missing;
  const auto before = calls;
  Check(missing.Initialize(nullptr) == REMIXAPI_ERROR_CODE_NOT_INITIALIZED, "missing entry point");
  Check(missing.Attempted() && !missing.Get(), "missing entry point yields no API");
  Check(missing.Initialize(&Initialize) == REMIXAPI_ERROR_CODE_NOT_INITIALIZED && calls == before,
    "missing entry point also follows one-attempt policy");

  RemixApiSession partial;
  provideConfig = false;
  Check(partial.Initialize(&Initialize) == REMIXAPI_ERROR_CODE_SUCCESS, "partial success result preserved");
  Check(!partial.Get(), "availability still requires SetConfigVariable");
  Check(success.Get() == table && table->SetConfigVariable == &Config, "sessions have independent tables");

  RemixApiSession resolved;
  unsigned resolverCalls = 0;
  provideConfig = true;
  Check(resolved.InitializeFrom([&] {
    ++resolverCalls;
    Check(resolved.Attempted() && !resolved.Get(), "attempt published before entry point resolution");
    Check(resolved.Initialize(nullptr) == REMIXAPI_ERROR_CODE_NOT_INITIALIZED,
      "entry point resolution reentry does not start another attempt");
    return &Initialize;
  }) == REMIXAPI_ERROR_CODE_SUCCESS, "resolved entry point initialized");
  Check(resolved.InitializeFrom([&]() -> PFN_remixapi_InitializeLibrary {
    ++resolverCalls; return nullptr;
  }) == REMIXAPI_ERROR_CODE_SUCCESS && resolverCalls == 1, "entry point resolved only once");
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"initializerCalls\":%u,\"gpu\":false,\"gameExecuted\":false}\n", checks, calls);
  return 0;
}
