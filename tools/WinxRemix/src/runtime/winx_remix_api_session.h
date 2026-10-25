#pragma once
// Own Remix backend boundary, not reconstructed Sparkplug/Winx logic.
// The caller resolves the entry point and keeps its module loaded. This class
// owns only the API table; device, bridge, resources and logging stay outside.
#include <remix/remix_c.h>

namespace winx_remix {
class RemixApiSession final {
public:
  RemixApiSession() = default;
  RemixApiSession(const RemixApiSession&) = delete;
  RemixApiSession& operator=(const RemixApiSession&) = delete;

  bool Attempted() const noexcept { return attempted_; }
  remixapi_ErrorCode Result() const noexcept { return result_; }

  remixapi_ErrorCode Initialize(PFN_remixapi_InitializeLibrary initialize) {
    return InitializeFrom([initialize] { return initialize; });
  }

  template<class Resolver> remixapi_ErrorCode InitializeFrom(const Resolver& resolve) {
    if (attempted_) return result_;
    // Preserve the proxy's one-attempt/reentry policy, including failed init.
    attempted_ = true;
    const PFN_remixapi_InitializeLibrary initialize = resolve();
    remixapi_InitializeLibraryInfo info{};
    info.sType = REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
    info.version = REMIXAPI_VERSION_MAKE(
      REMIXAPI_VERSION_MAJOR, REMIXAPI_VERSION_MINOR, REMIXAPI_VERSION_PATCH);
    result_ = initialize ? initialize(&info, &api_) : REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
    if (result_ != REMIXAPI_ERROR_CODE_SUCCESS) api_ = {};
    return result_;
  }

  // Keep the existing proxy availability gate. Individual consumers still
  // check each required function; this does not qualify a complete runtime.
  remixapi_Interface* Get() noexcept { return api_.SetConfigVariable ? &api_ : nullptr; }

private:
  remixapi_Interface api_{};
  bool attempted_ = false;
  remixapi_ErrorCode result_ = REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
};
} // namespace winx_remix
