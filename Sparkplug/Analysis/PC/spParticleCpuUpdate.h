#pragma once

#include "spBallisticCpuDraw.h"

namespace sparkplug::evidence::pc
{
    struct ParticleCpuUpdateParametersForAnalysis
    {
        float currentTime=0, delta=0;
        float lifetimeParameter=1; // ParticleSystem+BC; also clamps the stored delta.
        float integrationRate=30; // +B0, distinct from emission rate +B4.
        bool loop=false, storedPosition=false; // +AC/+AE.
        std::array<float,3> accelerationBegin{}, accelerationEnd{};
    };
    struct ParticleCpuMaintenanceForAnalysis
    {
        float time=0, delta=0;
        std::uint32_t activeBefore=0, activeAfter=0;
    };
    struct ParticleCpuUpdateForAnalysis
    {
        float currentTime=0, effectiveDelta=0, frameStart=0;
        std::uint32_t activeCount=0;
        // Logical first-to-next order of the initially active ring. Retired
        // suffix records remain here with their last bytes, as in the CPU pool.
        std::vector<BallisticCpuParticleRecord> records;
        std::vector<ParticleCpuMaintenanceForAnalysis> maintenance;
    };

    // Shared48D590 integration step, also used by the physical class ring.
    // Caller owns prevalidation/transaction semantics. False may follow writes.
    [[nodiscard]] inline bool IntegrateParticleCpuRecordForAnalysis(
        BallisticCpuParticleRecord& record,float frameStart,float subDelta,
        const std::array<float,3>& accelerationBegin,const std::array<float,3>& accelerationEnd)
    {
        if(!(record[7]>0))return false;
        const double age=(double(frameStart)-record[6])/record[7];
        for(unsigned c=0;c<3;++c)
        {
            double movement=double(subDelta)*record[c+3];
            if(c==2)movement=float(movement);
            record[c]=float(double(record[c])+movement);
            double end=double(accelerationEnd[c])*double(0.01f);
            double begin=double(accelerationBegin[c])*double(0.01f);
            if(c==2){end=float(end);begin=float(begin);}
            end=float(end*age);begin*=1.0-age;
            if(c!=2)begin=float(begin);
            double change=begin+end;
            if(c!=0)change=float(change);
            record[c+3]=float(double(record[c+3])+change);
            if(!std::isfinite(record[c])||!std::isfinite(record[c+3]))return false;
        }
        return true;
    }

    // PC48D450 plus the retirement part of48D210, for an existing active ring
    // with emission disabled (+58=0) and no modifiers (+84=0). This is neither
    // the full ParticleSystem update nor a replacement for its emitter/pool.
    // Finite positive lifetimes, <=1024 records and <=128 maintenance calls
    // are host bounds. Unsupported conversion ranges preserve output on refusal.
    [[nodiscard]] inline bool AdvanceExistingParticleCpuRecordsForAnalysis(
        const std::vector<BallisticCpuParticleRecord>& records,
        const ParticleCpuUpdateParametersForAnalysis& parameters,
        ParticleCpuUpdateForAnalysis& output)
    {
        const auto& p=parameters;
        if (records.size()>1024 || !std::isfinite(p.currentTime) || !std::isfinite(p.delta)
            || !std::isfinite(p.lifetimeParameter) || !(p.lifetimeParameter>0)
            || !std::isfinite(p.integrationRate) || p.integrationRate<0) return false;
        for (const auto& record : records)
        {
            for (const float value : record) if (!std::isfinite(value)) return false;
            if (!(record[7]>0)) return false;
        }
        for (const auto& acceleration : {p.accelerationBegin,p.accelerationEnd})
            for (const float value : acceleration) if (!std::isfinite(value)) return false;
        ParticleCpuUpdateForAnalysis result;
        result.records=records;result.activeCount=static_cast<std::uint32_t>(records.size());
        result.currentTime=float(double(p.currentTime)+p.delta);
        result.effectiveDelta=p.delta>p.lifetimeParameter ? p.lifetimeParameter : p.delta;
        if (!std::isfinite(result.currentTime)) return false;
        if (p.loop)
        {
            if (result.currentTime>p.lifetimeParameter)
                result.currentTime=float(double(result.currentTime)-p.lifetimeParameter);
            output=std::move(result);
            return true; // No maintenance or integration in this original branch.
        }
        std::uint32_t count=1;
        if (p.storedPosition && p.integrationRate!=0)
        {
            const double truncated=std::trunc(double(p.integrationRate)*result.effectiveDelta);
            if (truncated<0 || truncated>128) return false;
            count=std::max(1u,static_cast<std::uint32_t>(truncated));
        }
        result.frameStart=float(double(result.currentTime)-result.effectiveDelta);
        const float subDelta=float(double(result.effectiveDelta)/count);
        if (!std::isfinite(result.frameStart)) return false;
        for (std::uint32_t step=1; step<=count; ++step)
        {
            const float time=p.storedPosition
                ? float(double(step)*subDelta+result.frameStart) : result.currentTime;
            ParticleCpuMaintenanceForAnalysis maintenance{time,subDelta,result.activeCount,0};
            if (!std::isfinite(time)) return false;
            while (result.activeCount)
            {
                const auto& last=result.records[result.activeCount-1];
                if (!(double(last[6])+last[7]<double(time))) break;
                --result.activeCount;
            }
            maintenance.activeAfter=result.activeCount;result.maintenance.push_back(maintenance);
            if (!p.storedPosition) continue;
            for (std::uint32_t particle=0; particle<result.activeCount; ++particle)
            {
                auto& record=result.records[particle];
                if(!IntegrateParticleCpuRecordForAnalysis(record,result.frameStart,subDelta,
                    p.accelerationBegin,p.accelerationEnd))return false;
            }
        }
        output=std::move(result);
        return true;
    }
}
