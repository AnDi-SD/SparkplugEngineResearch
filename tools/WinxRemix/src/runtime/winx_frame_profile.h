#pragma once
// Own observational CPU attribution. Fixed storage only; no resource/native
// witness is retained and no result can authorize a draw or skip a fence.
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace winx_remix::frame_profile {
enum class Context : unsigned { IndependentPrepare, IndependentGroup, Other, Count };
enum class Metric : unsigned { CompareDrawInput, NativeInputCurrent, ResourceInput, GeometryExpand, GeometryIdentity,
  LightCapture, LightValidation, Count };
enum class LightStage : unsigned { Capture, Validation };
enum class LightCount : unsigned {
  CaptureValid, CaptureInvalid, CaptureAborted, CapturedRecords, RecordReadCalls, RecordReadSucceeded,
  VirtualQueryCalls, RegionCacheHits, RepeatedRegionQueries, RegionTrackingOverflows,
  ValidationAccepted, ValidationRejected, ValidationAborted, ValidationRecords, EligibleRecords,
  ConvertCalls, WitnessComparisons, OwnerLookups, Count
};
struct LightWork { uint64_t values[unsigned(LightCount::Count)]{}; };
struct Counter {
  uint64_t calls=0,timedCalls=0,qpcFailures=0,ticks=0;
};
struct State {
  uint64_t generation=1;
  Context context=Context::Other;
  Counter counters[unsigned(Context::Count)][unsigned(Metric::Count)]{};
  LightWork lightWork[unsigned(Context::Count)]{};
};
// Initialization may run on a different thread from Present. Each thread keeps
// bounded counters; DirectEndFrame emits/resets only its own render-thread data.
inline std::atomic_bool enabled{false};
inline State& Current() {static thread_local State state;return state;}
inline void Enable(bool value=true) noexcept {enabled.store(value,std::memory_order_relaxed);}
inline const Counter& Get(Context context,Metric metric) noexcept {
  return Current().counters[unsigned(context)][unsigned(metric)];
}
inline uint64_t GetLightWork(Context context,LightCount count) noexcept {
  return Current().lightWork[unsigned(context)].values[unsigned(count)];
}
inline void Reset() noexcept {
  auto& state=Current();const auto next=state.generation+1;
  state=State{};state.generation=next?next:1;
}
class ScopedContext final {
  State& state_=Current();
  const uint64_t generation_=state_.generation;
  const Context previous_=state_.context;
public:
  explicit ScopedContext(Context context) noexcept {state_.context=context;}
  ~ScopedContext() {if(generation_==state_.generation)state_.context=previous_;}
  ScopedContext(const ScopedContext&)=delete;
  ScopedContext& operator=(const ScopedContext&)=delete;
};
class ScopedMetric final {
  State* state_=nullptr;
  Counter* counter_=nullptr;
  uint64_t generation_=0;
  LARGE_INTEGER before_{};
  bool started_=false;
public:
  explicit ScopedMetric(Metric metric) noexcept {
    if(!enabled.load(std::memory_order_relaxed))return;
    state_=&Current();generation_=state_->generation;
    counter_=&state_->counters[unsigned(state_->context)][unsigned(metric)];++counter_->calls;
    started_=QueryPerformanceCounter(&before_)!=0&&before_.QuadPart>=0;
    if(!started_)++counter_->qpcFailures;
  }
  bool Active() const noexcept {return state_!=nullptr;}
  ~ScopedMetric() {
    // A reentrant frame boundary cannot credit an old timer to a new frame.
    if(!started_||generation_!=state_->generation)return;
    LARGE_INTEGER after{};
    if(QueryPerformanceCounter(&after)&&after.QuadPart>=before_.QuadPart) {
      counter_->ticks+=uint64_t(after.QuadPart-before_.QuadPart);++counter_->timedCalls;
    }else ++counter_->qpcFailures;
  }
  ScopedMetric(const ScopedMetric&)=delete;
  ScopedMetric& operator=(const ScopedMetric&)=delete;
};
// Work is local to an entered capture/validation, including a failing prefix.
// It never supplies native data or a currentness result. Generation checks
// discard both timing and work when a reentrant frame reset crosses the scope.
class ScopedLightMetric final {
  State& state_=Current();
  const uint64_t generation_=state_.generation;
  const Context context_=state_.context;
  const LightStage stage_;
  ScopedMetric timing_;
  LightWork work_{};
  bool resultSet_=false,accepted_=false;
public:
  explicit ScopedLightMetric(LightStage stage) noexcept :stage_(stage),
    timing_(stage==LightStage::Capture?Metric::LightCapture:Metric::LightValidation) {}
  bool Active() const noexcept {return timing_.Active();}
  void Add(LightCount count,uint64_t value=1) noexcept {
    if(Active())work_.values[unsigned(count)]+=value;
  }
  bool Result(bool value) noexcept {resultSet_=true;accepted_=value;return value;}
  ~ScopedLightMetric() {
    if(!Active()||generation_!=state_.generation)return;
    const auto outcome=stage_==LightStage::Capture?
      (!resultSet_?LightCount::CaptureAborted:accepted_?LightCount::CaptureValid:LightCount::CaptureInvalid):
      (!resultSet_?LightCount::ValidationAborted:accepted_?LightCount::ValidationAccepted:LightCount::ValidationRejected);
    ++work_.values[unsigned(outcome)];
    auto& destination=state_.lightWork[unsigned(context_)];
    for(unsigned i=0;i<unsigned(LightCount::Count);++i)destination.values[i]+=work_.values[i];
  }
  ScopedLightMetric(const ScopedLightMetric&)=delete;
  ScopedLightMetric& operator=(const ScopedLightMetric&)=delete;
};
struct BoundaryClock {
  LARGE_INTEGER counter{};
  bool valid=false;
  BoundaryClock() noexcept {valid=QueryPerformanceCounter(&counter)!=0&&counter.QuadPart>=0;}
};
inline void WriteFrameFields(FILE* output,const BoundaryClock& boundary,
                             const LARGE_INTEGER& frequency,bool frequencyValid) {
  frequencyValid=frequencyValid&&frequency.QuadPart>0;
  std::fprintf(output,",\"frameClock\":{\"counter\":%llu,\"frequency\":%llu,\"counterValid\":%s,\"frequencyValid\":%s}",
    boundary.valid?static_cast<unsigned long long>(boundary.counter.QuadPart):0ull,
    frequencyValid?static_cast<unsigned long long>(frequency.QuadPart):0ull,
    boundary.valid?"true":"false",frequencyValid?"true":"false");
  std::fprintf(output,",\"cpuProfile\":{\"schema\":2,\"enabled\":%s,\"inclusive\":true,\"contexts\":{",
    enabled.load(std::memory_order_relaxed)?"true":"false");
  static constexpr const char* contexts[]={"independentPrepare","independentGroup","other"};
  static constexpr const char* metrics[]={"compareDrawInput","nativeInputCurrent","resourceInput","geometryExpand","geometryIdentity",
    "lightCapture","lightValidation"};
  static_assert(sizeof(contexts)/sizeof(*contexts)==unsigned(Context::Count));
  static_assert(sizeof(metrics)/sizeof(*metrics)==unsigned(Metric::Count));
  for(unsigned context=0;context<unsigned(Context::Count);++context) {
    std::fprintf(output,"%s\"%s\":{",context?",":"",contexts[context]);
    for(unsigned metric=0;metric<unsigned(Metric::Count);++metric) {
      const auto& counter=Current().counters[context][metric];
      const auto pending=counter.calls-counter.timedCalls-counter.qpcFailures;
      std::fprintf(output,"%s\"%s\":{\"calls\":%llu,\"timedCalls\":%llu,\"qpcFailures\":%llu,\"pendingCalls\":%llu,\"ticks\":%llu,\"microseconds\":",
        metric?",":"",metrics[metric],static_cast<unsigned long long>(counter.calls),
        static_cast<unsigned long long>(counter.timedCalls),static_cast<unsigned long long>(counter.qpcFailures),
        static_cast<unsigned long long>(pending),static_cast<unsigned long long>(counter.ticks));
      if(frequencyValid) {
        const auto hz=uint64_t(frequency.QuadPart);
        const auto microseconds=(counter.ticks/hz)*1000000+(counter.ticks%hz)*1000000/hz;
        std::fprintf(output,"%llu",static_cast<unsigned long long>(microseconds));
      }else std::fputs("null",output);
      std::fputc('}',output);
    }
    std::fputc('}',output);
  }
  std::fputs("},\"lightWork\":{",output);
  static constexpr const char* lightCounts[]={"captureValid","captureInvalid","captureAborted","capturedRecords",
    "recordReadCalls","recordReadSucceeded","virtualQueryCalls","regionCacheHits","repeatedRegionQueries",
    "regionTrackingOverflows","validationAccepted","validationRejected","validationAborted","validationRecords",
    "eligibleRecords","convertCalls","witnessComparisons","ownerLookups"};
  static_assert(sizeof(lightCounts)/sizeof(*lightCounts)==unsigned(LightCount::Count));
  for(unsigned context=0;context<unsigned(Context::Count);++context) {
    std::fprintf(output,"%s\"%s\":{",context?",":"",contexts[context]);
    for(unsigned count=0;count<unsigned(LightCount::Count);++count)
      std::fprintf(output,"%s\"%s\":%llu",count?",":"",lightCounts[count],
        static_cast<unsigned long long>(Current().lightWork[context].values[count]));
    std::fputc('}',output);
  }
  std::fputs("}}",output);
}
}
