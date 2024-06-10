#include "CrownMPQ.h"

static int SaveMPQ(fs::path filename, std::string& dat) {
    HANDLE mpq = NULL;
    HANDLE file = NULL;
    auto filename_str = filename.string();

    if (fs::remove(filename)) {
        std::cout << "removed pre-existing MPQ file\n";
    }

    if (!SFileCreateArchive(filename_str.c_str(), MPQ_CREATE_LISTFILE | MPQ_CREATE_SIGNATURE | MPQ_CREATE_ARCHIVE_V1, 0x08, &mpq)) {
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

int main(int argc, char* argv[]) {
    DAT CLNK{
        "CNLK",
        "CrownLink",
        std::string("A new connection method for Cosmonarchy!\n\n\n\n\n\n\nVersion: ")+CL_VERSION,
        CAPS{
            36,
            0x00000003, // 0x20000003
            504,
            16,
            256,
            100000,
            50,
            8,
            2
        }
    };
    DAT CLDB{
        "CLDB",
        "CrownLink Double Brain Cells",
        std::string("A new connection method for Cosmonarchy!\n\nUse this version for extreme latency situations.\n\n\n\nVersion: ")+CL_VERSION,
        CAPS{
            36,
            0x00000003, // 0x20000003
            504,
            16,
            256,
            100000,
            50,
            4,
            2
        }
    };

    auto file_path = fs::path{"caps.mpq"};

    if (argc == 2) {
        // argument should be filename
        file_path = argv[1];
    }
    std::cout << "target file:" << file_path << "\n";

    std::stringstream ss;
    CLNK.get_dat(ss);
    CLDB.get_dat(ss);

    auto datstr = ss.str();
    SaveMPQ(file_path, datstr);
}
