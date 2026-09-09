#include "../BorrowedInput.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        using Input = spvhost::BorrowedInput;
        using Seek = sparkplug::reconstruction::spStream::SeekSource;
        std::array<std::uint8_t,8> bytes{0,1,2,3,4,5,6,7};
        const auto original = bytes;
        Input input(bytes.data(), static_cast<std::uint32_t>(bytes.size()));
        std::uint32_t size = 99, position = 99;
        Check(input.GetSize(&size) && size == 8, "physical size before origin");
        Check(input.GetCurrentPosition(position) && position == 0, "initial cursor");
        Check(input.Seek(Seek::essStart, 3), "seek to data origin");
        input.SetLogicalOriginForAnalysis(3);
        Check(input.GetSize(&size) && size == 8, "size remains physical after origin");
        Check(input.GetCurrentPosition(position) && position == 0, "Tell is relative to data origin");
        Check(input.Seek(Seek::essStart, 1), "start seek includes data origin");
        std::uint8_t value = 0xCC;
        Check(input.ReadData(&value, 1) && value == 4, "start-relative read uses physical bytes");
        Check(input.GetCurrentPosition(position) && position == 2, "read advances logical Tell");
        Check(input.Seek(Seek::essCurrent, -1), "current seek remains relative to physical cursor");
        Check(input.ReadData(&value, 1) && value == 4, "current-relative read");
        Check(input.Seek(Seek::essEnd, -1), "end seek remains physical end plus offset");
        Check(input.GetCurrentPosition(position) && position == 4, "end-relative Tell subtracts origin");
        Check(input.ReadData(&value, 1) && value == 7, "end-relative read");
        Check(input.GetCurrentPosition(position) && position == 5, "EOF Tell");
        value = 0xCC;
        Check(!input.ReadData(&value, 1) && value == 0xCC, "failed EOF read preserves output");
        Check(input.GetCurrentPosition(position) && position == 5, "failed read preserves cursor");
        Check(input.ReadData(nullptr, 0), "empty EOF read is valid");
        Check(!input.Seek(Seek::essStart, 6), "past-end start seek fails");
        Check(!input.Seek(Seek::essEnd, 1), "past-end end seek fails");
        Check(!input.Seek(Seek::essCurrent, std::numeric_limits<std::int32_t>::min()), "negative seek overflow is bounded");
        Check(!input.Seek(Seek::essCurrent, std::numeric_limits<std::int32_t>::max()), "positive seek overflow is bounded");
        Check(!input.Seek(static_cast<Seek>(0), 0), "unknown seek source fails");
        Check(input.GetCurrentPosition(position) && position == 5, "all failed seeks preserve cursor");
        Check(input.Seek(Seek::essStart, 0), "return to logical data start");
        Check(!input.ReadData(nullptr, 1), "nonempty null destination read fails");
        Check(!input.ReadData(&value, std::numeric_limits<std::uint32_t>::max()), "oversized read fails without arithmetic overflow");
        Check(input.GetCurrentPosition(position) && position == 0, "invalid read arguments preserve cursor");
        Check(!input.GetSize(nullptr), "null size output fails");
        Check(!input.WriteData(&value, 1), "writes are rejected");
        Check(!input.WriteData(nullptr, 0), "empty writes are rejected");
        Check(!input.vfunc_WriteFromStream(&input, 1), "stream writes are rejected");
        Check(input.GetBuffer() == nullptr, "immutable input has no writable buffer export");
        Check(!input.Open("unused") && !input.Open(0, "unused") && !input.Close(), "borrowed backend does not own stream lifecycle");
        Check(input.GetCurrentPosition(position) && position == 0, "rejected lifecycle and writes preserve cursor");
        input.SetLogicalOriginForAnalysis(9);
        position = 0xAABBCCDD;
        Check(!input.GetCurrentPosition(position) && position == 0xAABBCCDD, "invalid origin Tell preserves output");
        Check(!input.Seek(Seek::essStart, -1) && !input.Seek(Seek::essCurrent, 0)
            && !input.Seek(Seek::essEnd, 0), "invalid origin refuses every seek mode");
        value = 0xCC;
        Check(!input.ReadData(&value, 1) && value == 0xCC && !input.ReadData(nullptr, 0), "invalid origin refuses reads");
        Check(input.GetSize(&size) && size == 8, "physical size remains observable with invalid origin");
        input.SetLogicalOriginForAnalysis(3);
        Check(input.GetCurrentPosition(position) && position == 0, "invalid-origin failures preserved physical cursor");
        Check(bytes == original, "all operations preserved input bytes");
        Input empty(nullptr, 0);
        Check(empty.ReadData(nullptr, 0) && empty.GetCurrentPosition(position) && position == 0, "empty null input permits a no-op read");
        Check(!empty.ReadData(&value, 1) && empty.Seek(Seek::essEnd, 0), "empty null input remains bounded");
        Input missing(nullptr, 8);
        Check(!missing.ReadData(&value, 1) && value == 0xCC, "missing nonempty source fails safely");
        Check(missing.GetCurrentPosition(position) && position == 0, "missing source preserves cursor");
        std::cout << checks << " borrowed-input checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "Borrowed input check failed: " << error.what() << '\n';
        return 1;
    }
}
