#include "CrownMPQ.h"

static int save_mpq(const fs::path& filename, const std::string& dat) {
    HANDLE mpq{};
    HANDLE file{};
    auto filename_str = filename.string();

    if (fs::remove(filename)) {
        std::cout << "removed pre-existing MPQ file\n";
    }

    if (!SFileCreateArchive(
            filename_str.c_str(), MPQ_CREATE_LISTFILE | MPQ_CREATE_SIGNATURE | MPQ_CREATE_ARCHIVE_V1, 0x08, &mpq
        )) {
        std::cout << "couldn't make archive\n";
        return -1;
    } else {
        std::cout << "created successfully\n";
    }

    if (SFileCreateFile(mpq, "caps.dat", 0, dat.size(), 0, 0, &file)) {
        if (!SFileWriteFile(file, dat.c_str(), dat.size(), 0)) {
            std::cout << "Failed to write data to the MPQ\n";
        } else {
            std::cout << "Wrote caps.dat successfully\n";
        }
        SFileFinishFile(file);
        SFileCloseFile(file);
    } else {
        std::cout << "Couldn't create file in MPQ\n";
    }

    SFileCloseArchive(mpq);
    return 0;
}

static std::string build_description(const char* subtitle = "") {
    // 9 total lines are available in the Starcraft description box
    std::stringstream ss;
    ss << "Version: " << CL_VERSION_STRING << std::endl;
    ss << (char)0x4 << "Peer to peer networking using ICE\n" << (char)0x1;
    ss << subtitle << std::endl;
    return ss.str();
}

int main(int argc, char* argv[]) {
    fs::path file_path{"caps.mpq"};
    if (argc == 2) {
        // Argument should be filename
        file_path = argv[1];
    }
    std::cout << "Target file:" << file_path << "\n";

    std::stringstream ss;

    const auto caps_flags =
        std::to_underlying(CapsFlags::PageLockedBuffers) | std::to_underlying(CapsFlags::BasicInterface);

    Dat clnk{
        "CNLK", "CrownLink",
        build_description("Standard Mode"),
        Caps{sizeof(Caps), caps_flags, MaxPacketSize, 16, 256, 100000, 50,
            std::to_underlying(TurnsPerSecond::Standard), 2}
    };
    clnk.write(ss);
    Dat cldb{
        "CLDB", "CrownLink High Latency",
        build_description("Use this version when network conditions are poor, causing stuttering."),
        Caps{sizeof(Caps), caps_flags, MaxPacketSize, 16, 256, 100000, 50,
            std::to_underlying(TurnsPerSecond::UltraLow), 2}
    };
    cldb.write(ss);
    save_mpq(file_path, ss.str());
}
