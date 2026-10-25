// Own callback-order fixture. No game, graphics API, device or process hooks.
#include "../../src/runtime/winx_frame_scheduler.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

using Scheduler = winx_remix::FrameScheduler;
static unsigned checks;
static volatile bool throwCamera;
static void Check(bool value, const char* message) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
  Scheduler scheduler;
  Scheduler::Context context{0, 0, 0x1000, 0x2000, 0x3000, 1, true};
  std::vector<char> order;
  bool current = true;
  const auto prepare = [&](bool camera = true, bool lights = true) {
    return scheduler.Prepare(context, [&] { order.push_back('c'); return camera; },
      [&] { order.push_back('l'); return lights; }, [&] { return current; });
  };
  auto ticket = prepare();
  Check(ticket.valid && order == std::vector<char>({'c','l'}), "camera precedes lights without any raster draw");
  Check(scheduler.Current(ticket, context, [&] { return current; }), "current completed preparation ticket");
  Check(!scheduler.Preparing() && scheduler.Stage() == Scheduler::Phase::Ready, "completed state restored");
  auto changed = context; ++changed.frame;
  Check(!scheduler.Current(ticket, changed, [] { return true; }), "frame boundary invalidates old preparation");
  changed = context; ++changed.deviceEpoch;
  Check(!scheduler.Current(ticket, changed, [] { return true; }), "reset/device generation invalidates ticket");
  changed = context; ++changed.sceneScope;
  Check(!scheduler.Current(ticket, changed, [] { return true; }), "different scene occurrence invalidates ticket");
  changed = context; changed.apiLights = false;
  Check(!scheduler.Current(ticket, changed, [] { return true; }), "lighting policy change invalidates ticket");
  current = false;
  Check(!scheduler.Current(ticket, context, [&] { return current; }), "adapter freshness predicate cannot be cached");
  order.clear(); ticket = prepare();
  Check(!ticket.valid && order.empty(), "stale input invokes no external preparation");
  current = true; order.clear(); ticket = prepare(false);
  Check(!ticket.valid && order == std::vector<char>({'c'}), "camera failure stops lights");
  order.clear(); ticket = prepare(true, false);
  Check(!ticket.valid && order == std::vector<char>({'c','l'}), "light failure yields no ready ticket");
  order.clear(); ticket = scheduler.Prepare(context,
    [&] { order.push_back('c'); current = false; return true; },
    [&] { order.push_back('l'); return true; }, [&] { return current; });
  Check(!ticket.valid && order == std::vector<char>({'c'}), "camera callback mutation stops light stage");
  current = true; order.clear(); ticket = scheduler.Prepare(context, [] { return true; },
    [&] { scheduler.Invalidate(); return true; }, [] { return true; });
  Check(!ticket.valid && !scheduler.Preparing(), "reset during light callback invalidates preparation");
  unsigned nestedCalls = 0;
  ticket = scheduler.Prepare(context, [&] {
    const auto nested = scheduler.Prepare(context, [&] { ++nestedCalls; return true; },
      [&] { ++nestedCalls; return true; }, [] { return true; });
    Check(!nested.valid, "nested preparation rejected"); return true;
  }, [&] { ++nestedCalls; return true; }, [] { return true; });
  Check(!ticket.valid && !nestedCalls && !scheduler.Preparing(), "reentry cancels outer ticket without duplicate calls");
  bool threw = false;
  throwCamera = true;
  try { scheduler.Prepare(context, []() -> bool { if(throwCamera)throw std::runtime_error("owned fixture");return true; },
    [] { return true; }, [] { return true; }); } catch (const std::runtime_error&) { threw = true; }
  Check(threw && !scheduler.Preparing() && scheduler.Stage() == Scheduler::Phase::Rejected, "exception restores preparation state");
  current = true; order.clear(); ticket = prepare();
  const auto old = ticket; order.clear(); ticket = prepare();
  Check(ticket.valid && order == std::vector<char>({'c','l'}) && !scheduler.Current(old, context, [] { return true; }),
    "fresh preparation revokes old ticket and invokes current adapters");
  changed = context;
  Check(!scheduler.Current(ticket, changed, [&] { ++changed.frame; return true; }), "predicate mutation of live context rejects readiness");
  auto changedTicket = ticket;
  Check(!scheduler.Current(changedTicket, context, [&] { changedTicket.valid = false; return true; }), "predicate revoking caller ticket rejects readiness");
  changedTicket = ticket;
  Check(!scheduler.Current(changedTicket, context, [&] { ++changedTicket.revision; return true; }), "predicate changing caller revision rejects readiness");
  Check(!scheduler.Current(ticket, context, [&] { scheduler.Invalidate(); return true; }), "predicate reentry cannot publish stale readiness");
  std::printf("{\"status\":\"PASS\",\"checks\":%u,\"gpu\":false,\"gameExecuted\":false}\n", checks);
  return 0;
}
