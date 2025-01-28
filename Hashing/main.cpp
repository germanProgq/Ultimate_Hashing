#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <fstream>      // for std::ifstream
#include <limits>       // for std::numeric_limits

#include "QuantumProtection.h"
#include "SelfHeal.h"
#include "UniversalData.h"
#include "Performance.h"

int main(int argc, char* argv[]) {
    // --------------------------------------------------------------------
    // 1) Create and initialize our 2048-bit quantum fortress state
    // --------------------------------------------------------------------
    QFState fortress;
    qfInit(fortress);

    // Create a self-healing context and store an initial snapshot
    SelfHealContext healCtx;
    selfHealInit(healCtx, fortress);

    // --------------------------------------------------------------------
    // 2) Parse command-line arguments to decide how to handle input data
    // --------------------------------------------------------------------
    if (argc < 2) {
        std::cerr << "Usage:\n"
            << "  " << argv[0] << " <file|string> [data]\n\n"
            << "Examples:\n"
            << "  " << argv[0] << " file myBinary.dat\n"
            << "  " << argv[0] << " string \"Hello, Universe!\"\n";
        return EXIT_FAILURE;
    }

    std::string mode = argv[1];
    if (mode == "file") {
        // We expect: main.exe file somefilename
        if (argc < 3) {
            std::cerr << "[Error] No filename provided.\n";
            return EXIT_FAILURE;
        }
        std::string filename = argv[2];

        // Check if the file can be opened
        std::ifstream testFile(filename.c_str());
        if (!testFile.good()) {
            // File doesn't exist or can't be opened => fallback
            std::cout << "[Main] File \"" << filename << "\" not found.\n"
                << "[Main] Please enter a string to be hashed instead:\n";

            // Clear any leftover newline in input buffer
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            // Read a line from the user
            std::string fallbackInput;
            if (!std::getline(std::cin, fallbackInput)) {
                std::cerr << "[Error] Could not read fallback string from stdin.\n";
                return EXIT_FAILURE;
            }

            // Process the user-provided string
            processString(fortress, fallbackInput);
            std::cout << "[Main] Processed user string: \"" << fallbackInput << "\"\n";
        }
        else {
            // The file is accessible; proceed with processFile
            bool ok = processFile(fortress, filename);
            if (!ok) {
                std::cerr << "[Error] Failed to process file: " << filename << "\n";
                return EXIT_FAILURE;
            }
            std::cout << "[Main] Processed file: " << filename << "\n";
        }

    }
    else if (mode == "string") {
        // main.exe string "some text..."
        if (argc < 3) {
            std::cerr << "[Error] No string provided.\n";
            return EXIT_FAILURE;
        }
        // Build the string from remaining args (in case it has spaces)
        std::string inputData;
        for (int i = 2; i < argc; i++) {
            if (i > 2) inputData += " ";
            inputData += argv[i];
        }
        processString(fortress, inputData);
        std::cout << "[Main] Processed string: \"" << inputData << "\"\n";

    }
    else {
        std::cerr << "[Error] Unknown mode: " << mode << "\n";
        return EXIT_FAILURE;
    }

    // --------------------------------------------------------------------
    // After ingesting data, save a snapshot
    // --------------------------------------------------------------------
    selfHealSaveSnapshot(healCtx, fortress);

    // --------------------------------------------------------------------
    // 3) (Optional) Random corruption demonstration:
    /*
    if ((rand() % 2) == 0) {
        std::cerr << "[Main] Randomly corrupting fortress.state[5].\n";
        fortress.state[5] ^= 0xDEADBEEFCAFEBABEULL;
    }
    */

    // Check for anomaly & attempt recovery if needed
    if (selfHealDetect(fortress, healCtx)) {
        std::cerr << "[Main] Anomaly detected in fortress! Attempting recovery...\n";
        bool recovered = selfHealAttemptRecovery(fortress, healCtx);
        if (!recovered) {
            std::cerr << "[Main] We had to do a full re-init!\n";
        }
        else {
            std::cerr << "[Main] Self-healing recovered from a valid snapshot.\n";
        }
    }

    // --------------------------------------------------------------------
    // 4) Apply performance optimization
    // --------------------------------------------------------------------
    speedOptimize(fortress);

    // --------------------------------------------------------------------
    // 5) Finalize (example: produce a 64-byte digest via qfSqueeze)
    // --------------------------------------------------------------------
    const size_t DIGEST_SIZE = 64; // 512 bits
    std::vector<uint8_t> digest(DIGEST_SIZE);
    qfSqueeze(fortress, digest.data(), DIGEST_SIZE);

    std::cout << "\n[Main] Final 512-bit digest (" << DIGEST_SIZE << " bytes):\n";
    for (size_t i = 0; i < DIGEST_SIZE; i++) {
        std::printf("%02x", digest[i]);
    }
    std::cout << std::endl;

    // --------------------------------------------------------------------
    // 6) Print final QFState for demonstration
    // --------------------------------------------------------------------
    std::cout << "\n[Main] Final QFState:\n";
    for (int i = 0; i < QFState::STATE_WORDS; i++) {
        std::cout << "  fortress.state[" << i << "] = 0x"
            << std::hex << fortress.state[i] << std::dec << "\n";
    }
    std::cout << "\nabsorbedBytes = " << fortress.absorbedBytes << "\n";

    std::cout << "[Main] End of demonstration.\n";
    return 0;
}
