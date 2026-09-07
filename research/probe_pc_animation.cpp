// Isolated x86 checks of whitelisted, unprotected PC instruction bodies.
// Does NOT load/launch WinxClub.exe. See probe_pc_animation.py for SHA, build,
// timeout and scope. External calls are redirected to explicit test seams.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include "../Sparkplug/Analysis/PC/spAnimationMath.h"

static_assert(sizeof(void*) == 4, "This probe requires MSVC x86.");
namespace
{
    using V3 = std::array<float, 3>;
    using Q4 = std::array<float, 4>;
    const float zero = 0.0f, one = 1.0f, epsilon = 0.001f;
    const float half = 0.5f;
    const std::uint32_t nextAxis[]{1, 2, 0};
    int checks = 0, failures = 0;
    void Check(bool pass, const char* label)
    {
        ++checks;
        if (!pass)
            ++failures;
        std::printf("%s %s\n", pass ? "OK" : "FAIL", label);
    }
    bool Near(float a, float b)
    {
        return std::fabs(a - b) < 0.00001f;
    }

    struct Body
    {
        std::uint32_t va;
        std::size_t size;
        unsigned char* memory;
        Body(const std::vector<unsigned char>& file, std::uint32_t address, std::size_t length)
            : va(address), size(length), memory(nullptr)
        {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
            const auto* pe =
                reinterpret_cast<const IMAGE_NT_HEADERS32*>(file.data() + dos->e_lfanew);
            const auto* sections = IMAGE_FIRST_SECTION(pe);
            const auto rva = va - pe->OptionalHeader.ImageBase;
            for (unsigned i = 0; i < pe->FileHeader.NumberOfSections; ++i)
            {
                const auto& s = sections[i];
                if (rva < s.VirtualAddress || rva + size > s.VirtualAddress + s.SizeOfRawData)
                    continue;
                auto offset = s.PointerToRawData + rva - s.VirtualAddress;
                if (offset + size > file.size())
                    throw std::runtime_error("body outside file");
                memory = static_cast<unsigned char*>(
                    VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                if (!memory || size > 4096)
                    throw std::runtime_error("allocation failed");
                std::memcpy(memory, file.data() + offset, size);
                return;
            }
            throw std::runtime_error("body outside PE sections");
        }
        ~Body()
        {
            if (memory)
                VirtualFree(memory, 0, MEM_RELEASE);
        }
        Body(const Body&) = delete;
        Body& operator=(const Body&) = delete;
        void Call(std::uint32_t at, std::uint32_t originalTarget, const void* target)
        {
            auto offset = at - va;
            if (offset + 5 > size || memory[offset] != 0xE8)
                throw std::runtime_error("call mismatch");
            std::int32_t displacement;
            std::memcpy(&displacement, memory + offset + 1, 4);
            if (at + 5 + displacement != originalTarget)
                throw std::runtime_error("callee mismatch");
            auto replacement = reinterpret_cast<std::uintptr_t>(target) -
                               reinterpret_cast<std::uintptr_t>(memory + offset + 5);
            std::memcpy(memory + offset + 1, &replacement, 4);
        }
        void Constant(std::uint32_t oldAddress, const void* target)
        {
            auto replacement = reinterpret_cast<std::uintptr_t>(target);
            for (std::size_t offset = 0; offset + 4 <= size; ++offset)
            {
                std::uint32_t value;
                std::memcpy(&value, memory + offset, 4);
                if (value == oldAddress)
                    std::memcpy(memory + offset, &replacement, 4);
            }
        }
        void Seal()
        {
            DWORD old = 0;
            if (!VirtualProtect(memory, 4096, PAGE_EXECUTE_READ, &old))
                throw std::runtime_error("RX protection failed");
            FlushInstructionCache(GetCurrentProcess(), memory, size);
        }
    };

    struct Sample
    {
        V3 pos{10, 20, 30};
        Q4 rot{0, 0, 0, 1};
        V3 scale{2, 3, 4};
        char hasPos = 1, hasRot = 1, hasScale = 1;
    };
    Sample sample;
    float forwardedTime = -999, sampledTime = -999, blendFactor = -999;
    int rotations = 0, samples = 0;
    Q4 writtenRotation{};
    double __cdecl AcosStub(float value)
    {
        return std::acos(static_cast<double>(value));
    }
    __declspec(noreturn) void UnexpectedBranch()
    {
        ExitProcess(4);
    }
    void __fastcall EvalStub(void*, void*, float time, float* p, float* r, float* s, char* hp,
                             char* hr, char* hs)
    {
        forwardedTime = time;
        std::memcpy(p, sample.pos.data(), 12);
        std::memcpy(r, sample.rot.data(), 16);
        std::memcpy(s, sample.scale.data(), 12);
        *hp = sample.hasPos;
        *hr = sample.hasRot;
        *hs = sample.hasScale;
    }
    void __fastcall RotationStub(void*, void*, const float* rotation)
    {
        ++rotations;
        std::memcpy(writtenRotation.data(), rotation, 16);
    }
    void __fastcall FromMatrixStub(float* out, void*, const float*)
    {
        const Q4 identity{0, 0, 0, 1};
        std::memcpy(out, identity.data(), 16);
    }
    float* __fastcall SlerpStub(const float*, void*, float* out, float factor, const float* other)
    {
        blendFactor = factor;
        std::memcpy(out, other, 16);
        return out;
    }
    void __fastcall SampleStub(void* track, void*, float time, int* pc, int* rc, int* sc, float* p,
                               float* r, float* s, char* hp, char* hr, char* hs)
    {
        ++samples;
        sampledTime = time;
        Check(pc[0] == 101 && rc[0] == 201 && sc[0] == 301,
              "sampler receives three key-cache arrays");
        pc[1] = 102;
        rc[1] = 202;
        sc[1] = 302;
        const auto& value = *static_cast<Sample*>(track);
        std::memcpy(p, value.pos.data(), 12);
        std::memcpy(r, value.rot.data(), 16);
        std::memcpy(s, value.scale.data(), 12);
        *hp = value.hasPos;
        *hr = value.hasRot;
        *hs = value.hasScale;
    }
    struct Node
    {
        unsigned char prefix[0x20]{};
        V3 pos{1, 2, 3};
        std::uint32_t parent = 0;
        V3 scale{1, 1, 1};
        unsigned char rest[0x74]{};
        std::uint32_t flags = 0;
    };
    static_assert(offsetof(Node, flags) == 0xb0 && sizeof(Node) == 0xb4);
    struct Controller
    {
        void* vtable = nullptr;
        unsigned char prefix[12]{};
        Node* node = nullptr;
        void* evaluator = nullptr;
    };
    struct State
    {
        unsigned char prefix[12]{};
        float weight = 1;
        unsigned char middle[0x24]{};
        float time = 7;
        unsigned char tail[0x10]{};
        unsigned uses = 0;
    };
    struct Input
    {
        State* state = nullptr;
        Sample* track = nullptr;
        int slot = -1;
        int pc[3]{101, 0, 0}, rc[3]{201, 0, 0}, sc[3]{301, 0, 0};
    };
    struct Evaluator
    {
        void* vtable = nullptr;
        unsigned char prefix[12]{};
        int slot = -1;
        unsigned count = 0;
        Input input[2];
    };
    static_assert(sizeof(Input) == 0x30 && sizeof(Evaluator) == 0x78);
    static_assert(offsetof(State, time) == 0x34);
} // namespace

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 2)
        return 2;
    try
    {
        std::ifstream in(argv[1], std::ios::binary);
        std::vector<unsigned char> file{std::istreambuf_iterator<char>(in), {}};
        if (file.size() < 4096)
            throw std::runtime_error("input missing");
        Body interval(file, 0x00478F90, 0xCD);
        Body compare(file, 0x00439290, 0x60);
        Body quatCompare(file, 0x005FF140, 0x63);
        Body apply(file, 0x005FF1B0, 0x9B);
        Body blend(file, 0x005FF250, 0x185);
        Body evaluate(file, 0x005FEBB0, 0x44A);
        Body toMatrix(file, 0x004647F0, 0xC4);
        Body fromMatrix(file, 0x00464CB0, 0x130);
        Body interpolate(file, 0x004648C0, 0x1D2);
        Body sine(file, 0x00467B60, 0x7);
        Body multiply(file, 0x00426B00, 0x239);
        Body copyMatrix(file, 0x0041D330, 0x67);
        Body insert(file, 0x005FE9C0, 0x1AA);
        Body clear(file, 0x005FEB70, 0x17);
        Body forwardPoint(file, 0x00420660, 0xA4);
        Body inversePoint(file, 0x00420710, 0xC8);
        Body trackSample(file, 0x00479290, 0x4CF);
        Body linearVector(file, 0x00479060, 0x69);
        Body realBlend(file, 0x005FF250, 0x185);
        Body realEvaluate(file, 0x005FEBB0, 0x44A);
        Body multiply3(file, 0x00420C00, 0x111);
        inversePoint.Constant(0x006DBAAC, &one);
        linearVector.Constant(0x006DBAAC, &one);
        trackSample.Constant(0x006DBAAC, &one);
        for (auto at :
             {0x0047931CU, 0x004793CCU, 0x00479476U, 0x004795B1U, 0x0047961CU, 0x004796F8U})
            trackSample.Call(at, 0x00478F90, interval.memory);
        for (auto at : {0x00479427U, 0x0047974EU})
            trackSample.Call(at, 0x00479060, linearVector.memory);
        for (auto at : {0x00479401U, 0x00479721U})
            trackSample.Call(at, 0x00479110, reinterpret_cast<void*>(&UnexpectedBranch));
        trackSample.Call(0x00479519, 0x00464C80, reinterpret_cast<void*>(&UnexpectedBranch));
        trackSample.Call(0x00479526, 0x00464760, reinterpret_cast<void*>(&UnexpectedBranch));
        trackSample.Call(0x00479656, 0x00464B20, reinterpret_cast<void*>(&UnexpectedBranch));
        trackSample.Call(0x00479680, 0x004790D0, reinterpret_cast<void*>(&UnexpectedBranch));
        compare.Constant(0x006DF264, &epsilon);
        quatCompare.Constant(0x00711440, &epsilon);
        apply.Call(0x005FF218, 0x00420640, reinterpret_cast<void*>(&RotationStub));
        blend.Call(0x005FF2AF, 0x00439290, compare.memory);
        blend.Call(0x005FF350, 0x00464CB0, reinterpret_cast<void*>(&FromMatrixStub));
        blend.Call(0x005FF35E, 0x005FF140, quatCompare.memory);
        blend.Call(0x005FF37A, 0x004648C0, reinterpret_cast<void*>(&SlerpStub));
        blend.Call(0x005FF3A2, 0x00420640, reinterpret_cast<void*>(&RotationStub));
        blend.Constant(0x006DBAAC, &one);
        evaluate.Call(0x005FEC21, 0x00479290, reinterpret_cast<void*>(&SampleStub));
        evaluate.Call(0x005FEED6, 0x004648C0, reinterpret_cast<void*>(&SlerpStub));
        evaluate.Constant(0x006DBAAC, &one);
        evaluate.Constant(0x006DBA9C, &zero);
        toMatrix.Constant(0x006DBAAC, &one);
        fromMatrix.Constant(0x006DBAAC, &one);
        fromMatrix.Constant(0x006DBA9C, &zero);
        fromMatrix.Constant(0x006DC3D4, &half);
        fromMatrix.Constant(0x00740350, nextAxis);
        interpolate.Constant(0x006DBAAC, &one);
        interpolate.Constant(0x006DBA9C, &zero);
        interpolate.Constant(0x006E7E18, &epsilon);
        interpolate.Call(0x00464978, 0x00467B50, reinterpret_cast<void*>(&AcosStub));
        interpolate.Call(0x00464986, 0x00467B60, sine.memory);
        interpolate.Call(0x004649D1, 0x00467B60, sine.memory);
        interpolate.Call(0x004649E9, 0x00467B60, sine.memory);
        multiply.Call(0x00426D2B, 0x0041D330, copyMatrix.memory);
        realBlend.Call(0x005FF2AF, 0x00439290, compare.memory);
        realBlend.Call(0x005FF350, 0x00464CB0, fromMatrix.memory);
        realBlend.Call(0x005FF35E, 0x005FF140, quatCompare.memory);
        realBlend.Call(0x005FF37A, 0x004648C0, interpolate.memory);
        realBlend.Call(0x005FF3A2, 0x00420640, reinterpret_cast<void*>(&RotationStub));
        realBlend.Constant(0x006DBAAC, &one);
        realEvaluate.Call(0x005FEC21, 0x00479290, reinterpret_cast<void*>(&SampleStub));
        realEvaluate.Call(0x005FEED6, 0x004648C0, interpolate.memory);
        realEvaluate.Constant(0x006DBAAC, &one);
        realEvaluate.Constant(0x006DBA9C, &zero);
        for (auto* body :
             {&interval,  &compare,      &quatCompare,  &apply,        &blend,       &evaluate,
              &toMatrix,  &fromMatrix,   &interpolate,  &sine,         &multiply,    &copyMatrix,
              &insert,    &clear,        &forwardPoint, &inversePoint, &trackSample, &linearVector,
              &realBlend, &realEvaluate, &multiply3})
            body->Seal();

        using IntervalFn = int(__stdcall*)(const float*, int, float, float*, int);
        auto find = reinterpret_cast<IntervalFn>(interval.memory);
        const float times[]{0, 10, 20};
        float fraction = -1;
        Check(find(times, 3, 5, &fraction, 0) == 0 && Near(fraction, .5f),
              "interval interior forward");
        Check(find(times, 3, 5, &fraction, 2) == 0 && Near(fraction, .5f),
              "interval cached backward");
        Check(find(times, 3, -1, &fraction, 0) == 0 && fraction == 0, "interval before first");
        Check(find(times, 3, 21, &fraction, 0) == 1 && fraction == 1,
              "interval after last, three keys");
        Check(find(times, 2, 10, &fraction, 0) == 0 && fraction == 0,
              "native two-key endpoint returns fraction zero");
        Check(find(times, 2, 11, &fraction, 0) == 0 && fraction == 0,
              "native two-key after-last returns fraction zero");
        Check(find(times, 3, 15, &fraction, 99) == 1 && Near(fraction, .5f),
              "out-of-range cache restarts at zero");
        struct Keys
        {
            unsigned count = 2, representation = 1;
            const float* times;
            const V3* values;
        };
        const V3 vectors[]{V3{0, 0, 0}, V3{10, 20, 30}, V3{20, 40, 60}};
        Keys keys{2, 1, times, vectors};
        struct Track
        {
            unsigned char prefix[0x18]{};
            Keys* pos[3]{};
            Keys* rotation[3]{};
            Keys* scale[3]{};
            unsigned flags = 0;
        };
        Track nativeTrack;
        nativeTrack.pos[0] = &keys;
        using SampleFn = void(__thiscall*)(void*, float, int*, int*, int*, float*, float*, float*,
                                           char*, char*, char*);
        auto sampler = reinterpret_cast<SampleFn>(trackSample.memory);
        int positionCache[3]{}, rotationCache[3]{}, scaleCache[3]{};
        V3 sampledPosition{}, sampledScale{};
        Q4 sampledRotation{};
        char pv = 0, rv = 0, sv = 0;
        auto sampleAt = [&](float time) {
            sampler(&nativeTrack, time, positionCache, rotationCache, scaleCache,
                    sampledPosition.data(), sampledRotation.data(), sampledScale.data(), &pv, &rv,
                    &sv);
        };
        sampleAt(5);
        Check(pv && !rv && !sv && sampledPosition == V3{5, 10, 15},
              "native track linear position midpoint");
        sampleAt(10);
        Check(sampledPosition == vectors[0],
              "two-key endpoint quirk reaches native track sampler output");
        keys.count = 3;
        sampleAt(20);
        Check(sampledPosition == vectors[2], "native three-key position reaches final key");

        alignas(4) unsigned char cachedNode[0xb4]{};
        const V3 worldPosition{10, 20, 30}, worldScale{2, 3, 4};
        const float worldOrientation[]{0, 1, 0, -1, 0, 0, 0, 0, 1};
        std::memcpy(cachedNode + 0x74, worldPosition.data(), 12);
        std::memcpy(cachedNode + 0x80, worldScale.data(), 12);
        std::memcpy(cachedNode + 0x8c, worldOrientation, 36);
        using PointFn = void(__thiscall*)(void*, const float*, float*);
        auto pointForward = reinterpret_cast<PointFn>(forwardPoint.memory);
        auto pointInverse = reinterpret_cast<PointFn>(inversePoint.memory);
        V3 point{1, 2, 3}, worldPoint{}, restoredPoint{};
        pointForward(cachedNode, point.data(), worldPoint.data());
        pointInverse(cachedNode, worldPoint.data(), restoredPoint.data());
        Check(worldPoint == V3{4, 22, 42},
              "cached world point scales then rotates then translates");
        Check(restoredPoint == point, "cached inverse point reverses orthonormal transform");

        std::array<void*, 9> vt{};
        vt[8] = reinterpret_cast<void*>(&EvalStub);
        void* fakeEval = vt.data();
        Node node;
        Controller controller;
        controller.node = &node;
        controller.evaluator = &fakeEval;
        using ApplyFn = void(__thiscall*)(void*, float);
        using BlendFn = void(__thiscall*)(void*, float, float);
        auto direct = reinterpret_cast<ApplyFn>(apply.memory);
        auto blended = reinterpret_cast<BlendFn>(blend.memory);
        direct(&controller, 3.25f);
        Check(node.pos == sample.pos && node.scale == sample.scale,
              "direct PRS writes node position and scale");
        Check(rotations == 1 && writtenRotation == sample.rot,
              "direct PRS forwards quaternion setter");
        Check(node.flags == 1 && forwardedTime == 3.25f,
              "direct PRS dirty bit and time forwarding");
        sample.hasPos = sample.hasRot = sample.hasScale = 0;
        node = Node{};
        rotations = 0;
        direct(&controller, 4);
        Check(node.pos == V3{1, 2, 3} && node.scale == V3{1, 1, 1} && node.flags == 0 &&
                  rotations == 0,
              "invalid channels preserve node state");
        sample = Sample{};
        sample.hasRot = 0;
        node = Node{};
        blended(&controller, 5, .25f);
        Check(node.pos == V3{3.25f, 6.5f, 9.75f}, "blended position uses supplied factor");
        Check(node.scale == sample.scale, "blended scale is copied, not interpolated");
        node = Node{};
        sample.hasScale = 0;
        sample.pos = {1.0001f, 2, 3};
        blended(&controller, 6, .5f);
        Check(node.pos == V3{1, 2, 3} && node.flags == 0,
              "near-equal position skips write and dirty bit");
        sample = Sample{};
        sample.hasPos = sample.hasScale = 0;
        sample.rot = {0, 0, 1, 0};
        rotations = 0;
        blended(&controller, 8, .375f);
        Check(rotations == 1 && blendFactor == .375f,
              "blended quaternion uses factor through helper seam");

        using EvalFn = void(__thiscall*)(void*, float, float*, float*, float*, char*, char*, char*);
        auto eval = reinterpret_cast<EvalFn>(evaluate.memory);
        Evaluator evaluator;
        V3 p{-7, -7, -7}, s{-8, -8, -8};
        Q4 r{-9, -9, -9, -9};
        char hp = 1, hr = 1, hs = 1;
        eval(&evaluator, 999, p.data(), r.data(), s.data(), &hp, &hr, &hs);
        Check(hp == 0 && hr == 0 && hs == 0 && p[0] == -7 && r[0] == -9 && s[0] == -8,
              "empty evaluator clears validity without changing outputs");
        State state;
        Sample track;
        evaluator.count = 1;
        evaluator.input[0].state = &state;
        evaluator.input[0].track = &track;
        eval(&evaluator, 999, p.data(), r.data(), s.data(), &hp, &hr, &hs);
        Check(samples == 1 && sampledTime == 7,
              "evaluator samples state+0x34, ignoring outer time");
        Check(p == track.pos && r == track.rot && s == track.scale && hp && hr && hs,
              "single evaluator input copies PRS");
        Check(evaluator.input[0].pc[1] == 102 && evaluator.input[0].rc[1] == 202 &&
                  evaluator.input[0].sc[1] == 302,
              "sampler cache changes persist in evaluator");
        State secondState;
        secondState.weight = 3;
        Sample secondTrack;
        secondTrack.pos = {30, 40, 50};
        secondTrack.scale = {6, 7, 8};
        track.hasRot = secondTrack.hasRot = 0;
        evaluator.count = 2;
        evaluator.input[1].state = &secondState;
        evaluator.input[1].track = &secondTrack;
        eval(&evaluator, 999, p.data(), r.data(), s.data(), &hp, &hr, &hs);
        Check(p == V3{25, 35, 45} && s == V3{5, 6, 7},
              "two-input evaluator uses cumulative weights");
        evaluator.input[0].track = nullptr;
        eval(&evaluator, 999, p.data(), r.data(), s.data(), &hp, &hr, &hs);
        Check(p == secondTrack.pos && s == secondTrack.scale,
              "null track skipped while preserving input identity");

        using ConvertFn = void(__thiscall*)(const float*, float*);
        using InterpolateFn = float*(__thiscall*)(const float*, float*, float, const float*);
        auto nativeToMatrix = reinterpret_cast<ConvertFn>(toMatrix.memory);
        auto nativeFromMatrix = reinterpret_cast<ConvertFn>(fromMatrix.memory);
        auto nativeInterpolate = reinterpret_cast<InterpolateFn>(interpolate.memory);
        namespace math = sparkplug::evidence::pc::animation_math;
        bool matricesAgree = true, rotationsAgree = true, blendsAgree = true;
        std::uint32_t random = 0x5DAF152D;
        auto randomQuat = [&random]() {
            Q4 q{};
            float length = 0;
            for (auto& value : q)
            {
                random = random * 1664525U + 1013904223U;
                value = static_cast<float>(static_cast<std::int32_t>(random)) / 2147483648.0f;
                length += value * value;
            }
            for (auto& value : q)
                value /= std::sqrt(length);
            return q;
        };
        for (int i = 0; i < 128; ++i)
        {
            Q4 first = randomQuat(), second = randomQuat(), nativeRotation{}, nativeBlend{};
            math::Matrix3 nativeMatrix{};
            nativeToMatrix(first.data(), nativeMatrix.data());
            auto hostMatrix = math::ToMatrix(first);
            for (int axis = 0; axis < 9; ++axis)
                matricesAgree &= Near(nativeMatrix[axis], hostMatrix[axis]);
            // Native matrix-to-quaternion uses ECX=output, stack=matrix.
            nativeFromMatrix(nativeRotation.data(), nativeMatrix.data());
            auto hostRotation = math::FromMatrix(nativeMatrix);
            for (int axis = 0; axis < 4; ++axis)
                rotationsAgree &= Near(nativeRotation[axis], hostRotation[axis]);
            for (float factor : {0.f, .25f, .5f, 1.f, 1.5f})
            {
                nativeInterpolate(first.data(), nativeBlend.data(), factor, second.data());
                auto hostBlend = math::Interpolate(first, second, factor);
                for (int axis = 0; axis < 4; ++axis)
                    blendsAgree &= Near(nativeBlend[axis], hostBlend[axis]);
            }
        }
        Check(matricesAgree, "128 native quaternion-to-matrix comparisons with portable math");
        Check(rotationsAgree, "128 native matrix-to-quaternion comparisons with portable math");
        Check(blendsAgree,
              "640 native interpolation comparisons with portable math (CRT acos seam)");
        sample = Sample{};
        sample.hasPos = sample.hasScale = 0;
        sample.rot = {0, 0, 1, 0};
        node = Node{};
        rotations = 0;
        const auto identityMatrix = math::ToMatrix(Q4{0, 0, 0, 1});
        std::memcpy(reinterpret_cast<unsigned char*>(&node) + 0x40, identityMatrix.data(), 36);
        reinterpret_cast<BlendFn>(realBlend.memory)(&controller, 8, .25f);
        const auto expectedRotation = math::Interpolate(Q4{0, 0, 0, 1}, sample.rot, .25f);
        bool rotationChain = rotations == 1;
        for (int axis = 0; axis < 4; ++axis)
            rotationChain &= Near(writtenRotation[axis], expectedRotation[axis]);
        Check(rotationChain, "native controller with actual matrix-to-quaternion and slerp chain");
        track = Sample{};
        secondTrack = Sample{};
        secondTrack.rot = {0, 0, 1, 0};
        evaluator.input[0].track = &track;
        track.hasPos = secondTrack.hasPos = 0;
        reinterpret_cast<EvalFn>(realEvaluate.memory)(&evaluator, 999, p.data(), r.data(), s.data(),
                                                      &hp, &hr, &hs);
        const auto expectedMixed = math::Interpolate(track.rot, secondTrack.rot, .75f);
        bool rotationMix = hr != 0 && hp == 0;
        for (int axis = 0; axis < 4; ++axis)
            rotationMix &= Near(r[axis], expectedMixed[axis]);
        Check(rotationMix, "native evaluator weighted quaternion through actual slerp");
        state.weight = 0;
        track.hasPos = secondTrack.hasPos = 1;
        secondTrack.pos = {30, 40, 50};
        reinterpret_cast<EvalFn>(realEvaluate.memory)(&evaluator, 999, p.data(), r.data(), s.data(),
                                                      &hp, &hr, &hs);
        Check(p == secondTrack.pos && r == track.rot,
              "zero old weight replaces position but preserves first quaternion");
        state.weight = 1;
        sample.hasRot = sample.hasScale = 0;
        sample.hasPos = 1;
        sample.pos = {2, 4, 6};
        node = Node{};
        node.pos = {0, 0, 0};
        blended(&controller, 0, 1.5f);
        Check(node.pos == V3{3, 6, 9}, "native position transition factor is not clamped");
        using MatrixFn = float*(__thiscall*)(const float*, float*, const float*);
        auto nativeMultiply = reinterpret_cast<MatrixFn>(multiply.memory);
        std::array<float, 16> translation{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 0, 0, 1};
        std::array<float, 16> scale{2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1};
        std::array<float, 16> product{}, reverseProduct{};
        nativeMultiply(translation.data(), product.data(), scale.data());
        nativeMultiply(scale.data(), reverseProduct.data(), translation.data());
        Check(product[12] == 20 && reverseProduct[12] == 10,
              "native matrix product is this * argument in row-indexed storage");
        bool productsAgree = true;
        for (int test = 0; test < 64; ++test)
        {
            std::array<float, 16> left{}, right{}, native{};
            for (auto* matrix : {&left, &right})
                for (auto& value : *matrix)
                {
                    random = random * 1664525U + 1013904223U;
                    value = static_cast<float>(static_cast<std::int32_t>(random) % 101) / 16;
                }
            nativeMultiply(left.data(), native.data(), right.data());
            for (int row = 0; row < 4; ++row)
                for (int column = 0; column < 4; ++column)
                {
                    float expected = 0;
                    for (int k = 0; k < 4; ++k)
                        expected += left[4 * row + k] * right[4 * k + column];
                    productsAgree &= Near(expected, native[4 * row + column]);
                }
        }
        Check(productsAgree, "64 native 4x4 matrix product comparisons including native copy");
        auto nativeMultiply3 = reinterpret_cast<MatrixFn>(multiply3.memory);
        bool products3Agree = true;
        for (int test = 0; test < 64; ++test)
        {
            math::Matrix3 left{}, right{}, native{};
            for (auto* matrix : {&left, &right})
                for (auto& value : *matrix)
                {
                    random = random * 1664525U + 1013904223U;
                    value = static_cast<float>(static_cast<std::int32_t>(random) % 101) / 16;
                }
            nativeMultiply3(left.data(), native.data(), right.data());
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                {
                    float expected = 0;
                    for (int k = 0; k < 3; ++k)
                        expected += left[3 * row + k] * right[3 * k + column];
                    products3Agree &= Near(expected, native[3 * row + column]);
                }
        }
        Check(products3Agree,
              "64 native 3x3 products confirm local orientation times parent orientation");
        using InsertFn = void(__thiscall*)(void*, State*, Sample*, unsigned, char);
        using ClearFn = void(__thiscall*)(void*, unsigned);
        auto nativeInsert = reinterpret_cast<InsertFn>(insert.memory);
        auto nativeClear = reinterpret_cast<ClearFn>(clear.memory);
        Evaluator inserted;
        State incoming, other;
        nativeInsert(&inserted, &incoming, &track, 10, 1);
        Check(inserted.count == 1 && inserted.input[0].state == &incoming &&
                  inserted.input[0].slot == 10 && incoming.uses == 1,
              "exclusive input sets priority and increments state use count");
        nativeInsert(&inserted, &other, &secondTrack, 5, 1);
        Check(inserted.input[0].state == &incoming && other.uses == 0,
              "exclusive lower-priority input is rejected");
        nativeInsert(&inserted, &other, &secondTrack, 5, 0);
        Check(inserted.count == 2 && inserted.input[0].state == &other &&
                  inserted.input[1].state == &incoming && other.uses == 1,
              "blended insertion orders two input priorities ascending");
        const auto usesBefore = other.uses;
        nativeClear(&inserted, 0);
        Check(!inserted.input[0].state && !inserted.input[0].track && inserted.count == 2 &&
                  inserted.input[0].slot == 5 && other.uses == usesBefore,
              "input clear only zeroes pointers, keeps count priority and external counter");
        std::printf("RESULT %s checks=%d/%d\n", failures ? "FAIL" : "PASS", checks - failures,
                    checks);
        return failures ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 2;
    }
}
