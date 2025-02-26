#include <iostream>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <vector>
#include <string>

namespace fs = std::filesystem;

const fs::path CLIENT_MODS_DIR = "E:/SteamLibrary/steamapps/common/DayZ/!Workshop/"; // Your Client's workshop folder
const fs::path SERVER_MODS_DIR = "H:/DayZServer"; // Your Server folder 
const fs::path SERVER_EXECUTABLE = "H:/DayZServer/DayZServer_x64.exe"; // Your Server EXE
const fs::path SERVER_KEYS_DIR = "H:/DayZServer/keys"; // Server keys directory

// Function to get the list of mods that start with '@' in the server mods directory
std::vector<std::string> getModList() {
    std::vector<std::string> modList;
    try {
        // Iterate over the server mods directory to find mods starting with '@'
        for (const auto& entry : fs::directory_iterator(SERVER_MODS_DIR)) {
            if (fs::is_directory(entry.path())) {
                std::string modName = entry.path().filename().string();

                // Debugging output to check what mods are found
                std::cout << "Found mod: " << modName << std::endl;

                // Only add the mod to the list if it starts with "@"
                if (modName.rfind("@", 0) == 0) {
                    modList.push_back(modName);  // Do not add an extra "@"
                }
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error while reading mods directory: " << e.what() << "\n";
    }

    return modList;
}

// Function to generate the mod parameter string for the DayZ server
std::string generateModParameter(const std::vector<std::string>& mods) {
    std::string modParam = "-mod=";
    for (size_t i = 0; i < mods.size(); ++i) {
        // Replace spaces with underscores (or any other character you prefer)
        std::string mod = mods[i];
        std::replace(mod.begin(), mod.end(), ' ', '_');  // Replace spaces with underscores
        modParam += mod;
        if (i < mods.size() - 1) {
            modParam += ";";
        }
    }
    return modParam;
}



size_t countFilesToCopy(const fs::path& directory) {
    size_t fileCount = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (fs::is_regular_file(entry)) {
                fileCount++;
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error counting files in directory: " << e.what() << "\n";
    }
    return fileCount;
}

bool copyUpdatedMods() {
    bool updated = false;
    size_t totalFiles = 0;
    size_t copiedFiles = 0;

    try {
        if (!fs::exists(CLIENT_MODS_DIR)) {
            std::cerr << "Client mods directory does not exist: " << CLIENT_MODS_DIR << "\n";
            return false;
        }

        // Count the total files to copy
        for (const auto& entry : fs::directory_iterator(CLIENT_MODS_DIR)) {
            if (fs::is_directory(entry.path())) {
                fs::path modDir = entry.path();
                totalFiles += countFilesToCopy(modDir);
            }
        }

        if (totalFiles == 0) {
            std::cout << "No files to copy.\n";
            return false;
        }

        std::cout << "Total files to copy: " << totalFiles << "\n";

        for (const auto& entry : fs::directory_iterator(CLIENT_MODS_DIR)) {
            if (fs::is_directory(entry.path())) {
                // Skip mod folders starting with "!" (like "!DO_NOT_CHANGE_FILES_IN_THESE_FOLDERS")
                if (entry.path().filename().string().rfind("!", 0) == 0) {
                    std::cout << "Skipping mod folder: " << entry.path().filename().string() << "\n";
                    continue;
                }

                // Properly construct the destination path for the mod folder
                fs::path dest = SERVER_MODS_DIR / entry.path().filename().string(); // Using the / operator for path concatenation

                // Check if mod exists in server directory
                if (fs::exists(dest)) {
                    auto client_time = fs::last_write_time(entry.path());
                    auto server_time = fs::last_write_time(dest);

                    // Only copy if client mod is newer
                    if (client_time > server_time) {
                        std::cout << "Updating mod: " << entry.path().string() << " to " << dest.string() << "\n";
                        fs::copy(entry.path(), dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                        updated = true;
                        copiedFiles += countFilesToCopy(entry.path());
                    }
                    else {
                        std::cout << "Skipping up-to-date mod: " << entry.path().filename().string() << "\n";
                    }
                }
                else {
                    // Copy if mod does not exist on server
                    std::cout << "Copying new mod: " << entry.path().string() << " to " << dest.string() << "\n";
                    fs::copy(entry.path(), dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                    updated = true;
                    copiedFiles += countFilesToCopy(entry.path());
                }

                // Check and copy the .bikey files from the mod's keys folder to the server keys folder
                fs::path keysDir = entry.path() / "keys";
                if (fs::exists(keysDir) && fs::is_directory(keysDir)) {
                    for (const auto& keyEntry : fs::directory_iterator(keysDir)) {
                        if (keyEntry.path().extension() == ".bikey") {
                            fs::path destKey = SERVER_KEYS_DIR / keyEntry.path().filename();
                            std::cout << "Copying key: " << keyEntry.path() << " to " << destKey << "\n";
                            fs::copy(keyEntry.path(), destKey, fs::copy_options::overwrite_existing);
                            copiedFiles++;
                        }
                    }
                }

                // Show progress percentage
                double progress = (static_cast<double>(copiedFiles) / totalFiles) * 100;
                std::cout << "\rProgress: " << progress << "% " << std::flush;
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error while copying mods or keys: " << e.what() << "\n";
    }

    std::cout << "\nCopy process complete.\n";
    return updated;
}

void startServer() {
    std::vector<std::string> modList = getModList();
    std::string modParam = generateModParameter(modList);

    std::string serverCommand = SERVER_EXECUTABLE.string() + " -config=serverDZ.cfg -port=2302 " + modParam;

    std::cout << "Starting DayZ server with command: " << serverCommand << "\n";
    system(serverCommand.c_str());
}

int main() {
    try {
        std::cout << "Checking and copying updated mods from client to server...\n";
        bool modsUpdated = copyUpdatedMods();

        if (!modsUpdated) {
            std::cout << "All mods are up to date. Starting server...\n";
            startServer();
        }
        else {
            std::cout << "Mod update process completed. Please restart the server manually if needed.\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << "\n";
    }
    while (true) {

    }
    return 0;
}
