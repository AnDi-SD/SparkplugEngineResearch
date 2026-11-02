#pragma once
#include "WorldStateIpc.h"
#include "Networking/SessionCore.h"
#include <array>
#include <optional>
#include <vector>

namespace winxcoop::world_state
{
    constexpr std::uint32_t ApplicationClass = 0x31534357, ProtocolVersion = 1;
    // Snapshot payload schema is versioned independently of its unchanged
    // WCS1 envelope and IPC ABI. Schema1 is deliberately rejected by schema2.
    constexpr std::uint32_t SnapshotVersion = 2, MaximumSnapshotBlocks = 4;
    constexpr std::uint32_t SnapshotHeaderBytes = 40, ActorRecordBytes = 100;
    constexpr std::uint32_t EntrySeedVersion = 1, EntrySeedBytes = 448;
    constexpr std::uint32_t StarPresentationVersion = 1, StarPresentationHeaderBytes = 16;
    constexpr std::uint32_t StarRecordBytes = 88, MaximumStars = 16;
    constexpr std::uint32_t WireHeaderBytes = 72;
    constexpr std::uint32_t MaximumChunkBytes = static_cast<std::uint32_t>(networking::TcpPacketFramer::MaximumPayloadSize)-WireHeaderBytes;
    static_assert(MaximumChunkBytes==184, "Preserve existing256-byte Packet capacity");
    enum class Kind : std::uint32_t { Hello=1, Ready=2, Snapshot=3, AppliedAck=4, SourceState=5, Pause=6, Stop=7 };
    enum class BlockType : std::uint32_t { SharedProfileV1=1, EntryWitnessV1=2, EntrySeedV1=3, StarPresentationV1=4 };
    enum class Animation : std::uint32_t { Idle=0, Walk=1, Jump=2, Ordinary=3, Dying=4, Other=5 };
    constexpr std::uint32_t NativePaused=1;
    constexpr std::uint32_t Present=1, HasHealth=2, Dead=4, HasVelocity=8, Visible=16, ActorFlags=31;
    struct ActorRecord final
    {
        std::uint32_t stableId{}, typeId{}, stateId{}, animation{}, flags{};
        std::array<float,3> position{}, scale{{1,1,1}};
        std::array<float,9> rotation{{1,0,0,0,1,0,0,0,1}};
        std::array<float,3> velocity{};
        std::int32_t hp=-1, maximumHp=-1;
    };
    static_assert(sizeof(ActorRecord)==ActorRecordBytes);
    // Pre-entry values only. Runtime pointers and a native scene graph are
    // never part of the seed. The native consumer must verify the level too.
    struct EntrySeed final
    {
        std::uint32_t version=EntrySeedVersion, level{}, levelEpoch{}, generation{}, hostEntryOrdinal{}, reserved{};
        std::array<std::uint8_t,SharedProfileBytes> preProfile{};
        std::uint8_t preSavedSpawn6E0{};
        std::array<float,3> preSavedPosition584{};
    };
    static_assert(sizeof(EntrySeed)==EntrySeedBytes);
    static_assert(offsetof(EntrySeed,preProfile)==24 && offsetof(EntrySeed,preSavedSpawn6E0)==435 &&
        offsetof(EntrySeed,preSavedPosition584)==436);
    struct StarRecord final
    {
        std::uint64_t activationId{};
        std::uint32_t category{}, slot{}, stateC8{}, flags=Present|Visible;
        std::array<float,3> position{}, scale{{1,1,1}};
        std::array<float,9> rotation{{1,0,0,0,1,0,0,0,1}};
        std::uint32_t reserved{};
    };
    static_assert(sizeof(StarRecord)==StarRecordBytes);
    static_assert(offsetof(StarRecord,position)==24 && offsetof(StarRecord,scale)==36 &&
        offsetof(StarRecord,rotation)==48 && offsetof(StarRecord,reserved)==84);
    // All block encoding/decoding uses the existing MemoryStream primitives.
    // The Star list is absolute: an omitted activation is absent, so every
    // included record requires Present. StateC8 remains a raw native scalar.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeEntrySeed(const EntrySeed&);
    [[nodiscard]] std::optional<EntrySeed> DecodeEntrySeed(const void*,std::size_t);
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeStarPresentation(const std::vector<StarRecord>&);
    [[nodiscard]] std::optional<std::vector<StarRecord>> DecodeStarPresentation(const void*,std::size_t);
    struct Block final { BlockType type=BlockType::SharedProfileV1; std::vector<std::uint8_t> data; };
    struct Snapshot final
    {
        std::uint32_t flags{};
        std::uint64_t revision{}, nativeTick{};
        std::vector<ActorRecord> bodies, entities;
        std::vector<Block> blocks;
    };
    // Scalar order: version,kind,session,scenario,token,levelEpoch,generation,
    // syncEpoch,revision,flags,totalBytes,offset,chunkBytes,terminalReason.
    // Hello/SourceState/Ready revision is native readinessRevision; Snapshot/
    // AppliedAck revision is absolute snapshot revision. No base-delta patch.
    struct Message final
    {
        Kind kind=Kind::Hello;
        std::uint64_t sessionId{}, scenarioId{}, connectionToken{};
        std::uint32_t levelEpoch{}, generation{}, syncEpoch{};
        std::uint64_t revision{};
        std::uint32_t flags{}, totalBytes{}, offset{}, terminalReason{};
        std::vector<std::uint8_t> chunk;
    };
    [[nodiscard]] bool ValidSnapshot(const Snapshot&) noexcept;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeSnapshot(const Snapshot&);
    [[nodiscard]] std::optional<Snapshot> DecodeSnapshot(const void*,std::size_t);
    [[nodiscard]] bool ValidMessage(const Message&) noexcept;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> Encode(const Message&);
    [[nodiscard]] std::optional<Message> Decode(const networking::SessionMessage&);
    // One ordered fragmented snapshot; no allocation above16KiB, no revision
    // interleaving or duplicate/overlap acceptance. Caller clears on epoch end.
    class Assembly final
    {
    public:
        [[nodiscard]] bool Push(const Message&);
        [[nodiscard]] std::optional<Snapshot> Complete() const;
        void Clear() noexcept;
        [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
        [[nodiscard]] std::size_t Size() const noexcept { return data_.size(); }
    private:
        std::uint64_t revision_{}; std::uint32_t total_{}, epoch_{};
        std::vector<std::uint8_t> data_;
    };
}
