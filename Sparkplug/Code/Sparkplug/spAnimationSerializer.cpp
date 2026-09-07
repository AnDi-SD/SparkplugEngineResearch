// Exact PC original source path documented in the header.
#include "spAnimationSerializer.h"
#include "spDataBlockSerializer.h"
#include "../SparkBase/spMemoryStream.h"
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    spAnimationSerializer::spAnimationSerializer()
    {
        // Host registration/linkage adapter, like MeshDataSerializer: merely
        // constructing a serializer must make its wire target available even
        // when that target uses a lazy StaticRTTI record. Not native startup.
        (void)spAnimation::StaticRTTI();
    }

    const spRTTIRecord& spAnimationSerializer::StaticRTTI() noexcept
    {
        static const spRTTIRecord record{ClassID,
                                         spSerializer::ClassID,
                                         "spAnimationSerializer",
                                         &spSerializer::StaticRTTI(),
                                         +[]() -> std::unique_ptr<spBaseObject> {
                                             return std::make_unique<spAnimationSerializer>();
                                         },
                                         nullptr};
        static const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        (void)registered;
        return record;
    }
    const spRTTIRecord& spAnimationSerializer::vfunc_18() const noexcept
    {
        return StaticRTTI();
    }
    std::unique_ptr<spBaseObject> spAnimationSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto result = std::make_unique<spAnimationSerializer>();
        manager.RegisterClone(*this, *result);
        return result; // native no-payload serializer copy
    }

    std::unique_ptr<spAnimation> spAnimationSerializer::ReadFieldsWithBindingsForAnalysis(
        spStream& source, std::uint32_t byteCount, spAnimationManager& manager,
        ReadObservationsForAnalysis* observations, std::string* error) const
    {
        if (observations)
            *observations = {};
        ReadObservationsForAnalysis observed;
        auto animation = ReadFieldsForAnalysis(source, byteCount, {}, &observed, error);
        if (!animation)
            return nullptr;
        for (std::size_t i = 0; i < animation->GetTrackCountForAnalysis(); ++i)
            if (!animation->GetTrackForAnalysis(i)->BindNameForAnalysis(manager))
            {
                if (error)
                    *error = "Cannot acquire animation track name binding";
                return nullptr;
            }
        if (observations)
            *observations = std::move(observed);
        return animation;
    }

    bool spAnimationSerializer::WriteFieldsForAnalysis(
        spStream& destination, const spAnimation& animation, std::string* error) const
    {
        if (error) error->clear();
        const auto fail = [error](const char* message) {
            if (error) *error = message;
            return false;
        };
        const auto count = animation.GetTrackCountForAnalysis();
        if (count >= MaximumTracksForAnalysis)
            return fail("Animation track count cannot be round-tripped by bounded reader");
        std::array<std::uint32_t, 7> pools{};
        std::uint64_t extent = 47; // duration/count/seven pools + terminator
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& track = *animation.GetTrackForAnalysis(index);
            const auto* keys = track.GetKeysForAnalysis();
            if (!keys)
                return fail("Native animation writer requires initialized key descriptors");
            for (std::size_t role = 0; role < 3; ++role)
            {
                const auto& channels = (*keys)[role];
                if (!channels[0])
                    return fail("Missing native role descriptor: original writer dereferences it");
                const std::size_t axes = channels[0]->representation >= 3 ? 3 : 1;
                extent += 5; // forced UInt32 size header for field2/3/4
                for (std::size_t axis = 0; axis < axes; ++axis)
                {
                    if (!channels[axis])
                        return fail("Incomplete scalar key descriptors");
                    const auto& key = *channels[axis];
                    const auto rep = key.representation;
                    const auto pool = rep == 3 ? 0U : rep == 4 ? 1U :
                                      role == 1 ? (rep == 1 ? 4U : 5U) : (rep == 1 ? 2U : 3U);
                    if (key.times.size() > MaximumFieldBytesForAnalysis / 4 - pools[pool] ||
                        key.times.size() > MaximumFieldBytesForAnalysis / 4 - pools[6])
                        return fail("Shared key pools exceed bounded writer");
                    pools[pool] += static_cast<std::uint32_t>(key.times.size());
                    pools[6] += static_cast<std::uint32_t>(key.times.size());
                    extent += 8 + 4ULL * (key.times.size() + key.values.size());
                }
            }
            const char* name = track.GetName();
            const auto length = name ? std::strlen(name) : 0;
            if (!name || length >= 0xFFFFU)
                return fail("Native track name must have a bounded NUL-terminated wire value");
            extent += length + 9; // conservative maximum direct name header+u16+NUL
        }
        for (const auto& tag : animation.GetTagsForAnalysis())
        {
            if (tag.name.size() > 0xFFF8U || tag.name.find('\0') != std::string::npos)
                return fail("Animation tag name cannot be represented by native string writer");
            extent += tag.name.size() + 10; // UInt16 field5 header + string + time
        }
        std::uint32_t start = 0;
        if (extent > MaximumFieldBytesForAnalysis || !destination.GetCurrentPosition(start) ||
            extent + start > std::uint32_t(std::numeric_limits<std::int32_t>::max()))
            return fail("Animation output extent exceeds bounded stream position");

        spDataBlockSerializer blocks;
        const auto scalar = [&](std::uint32_t id, const auto& value) {
            return blocks.WriteFieldForAnalysis(destination, id, &value, sizeof(value));
        };
        const auto totalTime = animation.GetTotalTimeForAnalysis();
        const auto trackCount = static_cast<std::uint32_t>(count);
        if (!blocks.BeginObjectForAnalysis(destination, &animation) || !scalar(0, totalTime) ||
            !scalar(64, trackCount))
            return fail("Cannot write animation duration/track count");
        std::uint32_t poolStart = 0;
        if (!destination.GetCurrentPosition(poolStart))
            return fail("Cannot record key-pool patch position");
        const std::uint32_t zero = 0;
        if (!scalar(12, zero)) return fail("Cannot reserve time-pool counter");
        for (std::uint32_t pool = 0; pool < 6; ++pool)
            if (!scalar(pool + 6, zero)) return fail("Cannot reserve key-pool counters");
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& track = *animation.GetTrackForAnalysis(index);
            const auto& keys = *track.GetKeysForAnalysis();
            for (std::size_t role = 0; role < 3; ++role)
            {
                if (!blocks.WriteBeginForAnalysis(static_cast<std::uint32_t>(role) + 2))
                    return fail("Cannot reserve track role field");
                const auto& channels = keys[role];
                const std::size_t axes = channels[0]->representation >= 3 ? 3 : 1;
                for (std::size_t axis = 0; axis < axes; ++axis)
                {
                    const auto& key = *channels[axis];
                    if (!destination.Write(static_cast<std::uint32_t>(key.representation)) ||
                        !destination.Write(static_cast<std::uint32_t>(key.times.size())) ||
                        (!key.times.empty() &&
                         (!destination.WriteData(key.times.data(), static_cast<std::uint32_t>(key.times.size() * 4)) ||
                          !destination.WriteData(key.values.data(), static_cast<std::uint32_t>(key.values.size() * 4)))))
                        return fail("Cannot write animation key arrays");
                }
                if (!blocks.WriteEndForAnalysis(static_cast<std::uint32_t>(role) + 2))
                    return fail("Cannot patch track role field");
            }
            spMemoryStream name;
            if (!name.Open(nullptr) || !name.Write(track.GetName()))
                return fail("Cannot encode track name");
            std::uint32_t nameBytes = 0;
            if (!name.GetSize(&nameBytes) || !blocks.WriteFieldForAnalysis(destination, 1, name.GetBuffer(), nameBytes))
                return fail("Cannot write track name field");
        }
        for (const auto& tag : animation.GetTagsForAnalysis())
            if (!blocks.WriteBeginForAnalysis(5, spDataBlockSerializer::SizeCode::UInt16) || !destination.Write(tag.name.c_str()) ||
                !destination.Write(tag.time) || !blocks.WriteEndForAnalysis(5))
                return fail("Cannot write animation tag");
        std::uint32_t end = 0;
        if (!destination.GetCurrentPosition(end) ||
            !destination.Seek(spStream::SeekSource::essStart, static_cast<std::int32_t>(poolStart + 5)))
            return fail("Cannot seek to animation value-pool counters");
        for (std::uint32_t pool = 0; pool < 6; ++pool)
            if (!scalar(pool + 6, pools[pool])) return fail("Cannot patch animation key-pool counters");
        if (!destination.Seek(spStream::SeekSource::essStart, static_cast<std::int32_t>(poolStart)) ||
            !scalar(12, pools[6]))
            return fail("Cannot patch animation time-pool counter");
        if (!destination.Seek(spStream::SeekSource::essStart, static_cast<std::int32_t>(end)) ||
            !blocks.FinalizeObjectForAnalysis())
            return fail("Cannot finalize animation output");
        return true;
    }

    bool spAnimationSerializer::IndexRelationshipsForAnalysis(spBaseObject& object) const
    {
        // PC secondary slot5A7DB0 is true/no-op. Type check is a host guard.
        return object.IsExactly(spAnimation::ClassID);
    }

    bool spAnimationSerializer::WritePayloadForAnalysis(
        spStream& destination, const spBaseObject& object, std::string* error) const
    {
        const auto* animation = dynamic_cast<const spAnimation*>(&object);
        if (!animation)
        {
            if (error) *error = "Animation serializer requires an animation object";
            return false;
        }
        return WriteFieldsForAnalysis(destination, *animation, error);
    }

    std::unique_ptr<spAnimation> spAnimationSerializer::ReadFieldsForAnalysis(
        spStream& source, std::uint32_t byteCount, const BindingResolverForAnalysis& resolveBinding,
        ReadObservationsForAnalysis* observations, std::string* error) const
    {
        auto animation = std::make_unique<spAnimation>();
        if (!ReadFieldsIntoForAnalysis(source, byteCount, *animation, resolveBinding, observations, error))
            return nullptr;
        return animation;
    }

    bool spAnimationSerializer::ReadPayloadForAnalysis(
        spSerializerReadContextForAnalysis& context, spStream& source,
        std::uint32_t byteCount, spBaseObject& object, std::string* error) const
    {
        auto* animation = dynamic_cast<spAnimation*>(&object);
        if (!animation)
        {
            if (error) *error = "Animation serializer received another factory type";
            return false;
        }
        if (!ReadFieldsIntoForAnalysis(source, byteCount, *animation, {}, nullptr, error)) return false;
        if (context.animationBindings)
            for (std::size_t i = 0; i < animation->GetTrackCountForAnalysis(); ++i)
                if (!animation->GetTrackForAnalysis(i)->BindNameForAnalysis(*context.animationBindings))
                {
                    if (error) *error = "Cannot acquire animation track name binding";
                    return false;
                }
        return true;
    }

    bool spAnimationSerializer::ReadFieldsIntoForAnalysis(spStream& source,
        std::uint32_t byteCount, spAnimation& target, const BindingResolverForAnalysis& resolveBinding,
        ReadObservationsForAnalysis* observations, std::string* error) const
    {
        if (error)
            error->clear();
        if (observations)
            *observations = {};
        const auto fail = [error](const char* message) {
            if (error)
                *error = message;
            return false;
        };
        if (target.GetTrackCountForAnalysis() || !target.GetTagsForAnalysis().empty())
            return fail("Animation payload target must be fresh");
        std::uint32_t start = 0, size = 0;
        if (!byteCount || byteCount > MaximumFieldBytesForAnalysis ||
            !source.GetCurrentPosition(start) || !source.GetSize(&size) ||
            source.GetLogicalOriginForAnalysis() > size)
            return fail("Invalid bounded animation field extent");
        size -= source.GetLogicalOriginForAnalysis();
        if (start > size || byteCount > size - start)
            return fail("Animation fields exceed logical data segment");
        const auto end = start + byteCount;
        spDataBlockSerializer blocks;
        auto* animation = &target;
        ReadObservationsForAnalysis observed;
        spAnimTrack* pending = nullptr;
        spAnimTrack::TrackDataForAnalysis pendingKeys{};
        std::uint32_t tagOrdinal = 0;
        bool terminated = false;
        for (std::uint32_t fieldCount = 0; fieldCount < 100000; ++fieldCount)
        {
            std::uint32_t position = 0;
            if (!source.GetCurrentPosition(position) || position >= end)
                return fail("Missing animation section terminator");
            const auto* header = blocks.ReadHeaderForAnalysis(source);
            if (!header || header->dataStreamPosition > end ||
                header->payloadSize > end - header->dataStreamPosition)
                return fail("Truncated animation field header/payload");
            if (header->IsTerminator())
            {
                if (pending || header->dataStreamPosition != end)
                    return fail("Unfinished track or trailing bytes at terminator");
                terminated = true;
                break;
            }
            const auto field = header->fieldID;
            if (field > 12 && field != 64)
            {
                observed.unknownFields.push_back(field);
                if (!spDataBlockSerializer::SkipDataForAnalysis(source, *header))
                    return fail("Cannot skip unknown animation field");
                continue;
            }
            spMemoryStream payload;
            if (!payload.ResizeAndSetSize(header->payloadSize) ||
                !source.ReadData(payload.GetBuffer(), header->payloadSize))
                return fail("Cannot read bounded animation field");
            if (field == 0)
            {
                float time = 0;
                if (!payload.Read(time) || !animation->SetTotalTimeForAnalysis(time))
                    return fail("Invalid total animation time");
            }
            else if (field >= 6 && field <= 12)
            {
                std::uint32_t count = 0;
                if (!payload.Read(count) || count > MaximumFieldBytesForAnalysis / 4)
                    return fail("Invalid shared key pool size");
                const auto pool = field - 6;
                if (observed.declaredPools[pool] || observed.usedPools[pool])
                    return fail("Repeated or late shared key pool declaration");
                observed.declaredPools[pool] = count;
            }
            else if (field == 64)
            {
                std::uint32_t count = 0;
                if (!payload.Read(count) || count >= MaximumTracksForAnalysis ||
                    animation->GetTrackCountForAnalysis() != 0 || observed.trackReserveHint)
                    return fail("Invalid/repeated/late animation track reserve");
                observed.trackReserveHint = count;
                if (!animation->ResizeTrackCapacityForAnalysis(count + 1))
                    return fail("Cannot reserve animation tracks");
            }
            else if (field >= 1 && field <= 4)
            {
                if (!pending)
                {
                    if (animation->GetTrackCountForAnalysis() >= MaximumTracksForAnalysis)
                        return fail("Animation track bound exceeded");
                    pending = animation->AppendTrackForAnalysis();
                    pendingKeys = {};
                }
                if (field == 1)
                {
                    std::string name;
                    if (!payload.ReadString(name) ||
                        !pending->SetKeysForAnalysis(std::move(pendingKeys)))
                        return fail("Invalid track name or key representation");
                    pending->SetName(name);
                    pending = nullptr;
                }
                else
                {
                    const auto role = field - 2;
                    std::uint32_t axes = 1;
                    for (std::uint32_t axis = 0; axis < axes; ++axis)
                    {
                        std::uint32_t rep = 0, count = 0;
                        if (!payload.Read(rep))
                            return fail("Missing key representation");
                        if (!rep)
                        {
                            if (axis)
                                return fail("Incomplete scalar key triple");
                            break; // original representation zero does not clear old channel
                        }
                        if (rep > 4 || !payload.Read(count) || count > 100000)
                            return fail("Invalid key representation/count");
                        if (!axis && rep >= 3)
                            axes = 3;
                        if ((rep >= 3) != (axes == 3))
                            return fail("Mixed packed/scalar key triple");
                        const auto stride = rep == 4   ? 5U
                                            : rep == 3 ? 1U
                                            : rep == 2 ? (role == 1 ? 8U : 15U)
                                                       : (role == 1 ? 4U : 3U);
                        const auto pool = rep == 3    ? 0U
                                          : rep == 4  ? 1U
                                          : role == 1 ? (rep == 1 ? 4U : 5U)
                                                      : (rep == 1 ? 2U : 3U);
                        if (count > observed.declaredPools[pool].value_or(0) -
                                        observed.usedPools[pool] ||
                            count > observed.declaredPools[6].value_or(0) - observed.usedPools[6])
                            return fail("Key payload exceeds declared shared pools");
                        std::uint32_t consumed = 0;
                        if (!payload.GetCurrentPosition(consumed) ||
                            count > (header->payloadSize - consumed) / (4 * (stride + 1)))
                            return fail("Key arrays exceed their field payload");
                        evidence::pc::animation_keys::KeyDataForAnalysis key;
                        key.representation = rep;
                        key.times.resize(count);
                        key.values.resize(count * stride);
                        if (count && (!payload.ReadData(key.times.data(), count * 4) ||
                                      !payload.ReadData(key.values.data(), count * stride * 4)))
                            return fail("Truncated key arrays");
                        observed.usedPools[pool] += count;
                        observed.usedPools[6] += count;
                        pendingKeys[role][axis] = std::move(key);
                    }
                }
            }
            else if (field == 5)
            {
                spAnimation::TagForAnalysis tag;
                tag.wireOrdinal = tagOrdinal++;
                if (!payload.ReadString(tag.name) || !payload.Read(tag.time) ||
                    !animation->InsertTagForAnalysis(std::move(tag)))
                    return fail("Invalid animation tag");
            }
            std::uint32_t consumed = 0;
            if (!payload.GetCurrentPosition(consumed) || consumed != header->payloadSize)
                return fail("Unaccounted bytes in known animation field");
        }
        if (!terminated)
            return fail("Animation field-count bound exceeded");
        if (resolveBinding)
            for (std::size_t i = 0; i < animation->GetTrackCountForAnalysis(); ++i)
            {
                auto* track = animation->GetTrackForAnalysis(i);
                const char* name = track->GetName();
                const auto slot =
                    resolveBinding(name ? std::string_view(name) : std::string_view{});
                if (!slot)
                    return fail("Animation binding lookup failed");
                track->SetBindingSlotForAnalysis(*slot);
            }
        if (observations)
            *observations = std::move(observed);
        return true;
    }
} // namespace sparkplug::reconstruction
