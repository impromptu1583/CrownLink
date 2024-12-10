#include "CrownMPQ.h"

#define SNET_CAPS_PAGELOCKEDBUFFERS 0x00000001
#define SNET_CAPS_BASICINTERFACE 0x00000002
#define SNET_CAPS_DEBUGONLY 0x10000000
#define SNET_CAPS_RETAILONLY 0x20000000

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
    // 9 total lines
    std::stringstream ss;
    ss << (char)0x4 << "P2P Lobbies for Cosmonarchy!\n" << (char)0x1; // line 1
    ss << subtitle << std::endl; // line 2
    ss << "\n\n\n\n\n\n"; // lines 3-8
    ss << "Version: " << CL_VERSION_STRING; // line 9
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
    std::stringstream clnk_description;
    std::stringstream cldb_description;



    Dat clnk{
        "BNET", "CrownLink",
        //std::format("A new connection method for Cosmonarchy!\n\n\n\n\n\n\nVersion: {}", CL_VERSION_STRING),
        build_description("Standard Modee"), Caps{36, 0x00000001, 504, 16, 256, 100000, 50, 8, 2}
    };
    clnk.write(ss);
    Dat cldb{
        "CLDB", std::format("CrownLink Double Brain Cells"),
        build_description("Extreme Latency Mode"),

        Caps{36, 0x00000001, 504, 16, 256, 100000, 50, 4, 2}
    };
    cldb.write(ss);
    save_mpq(file_path, ss.str());
}
