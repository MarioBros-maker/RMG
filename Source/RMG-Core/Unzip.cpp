/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "Unzip.hpp"
#include "Error.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <vector>
#include <unzip.h>

//
// Local Defines
//

#define UNZIP_READ_SIZE 67108860 /* 64 MiB */

//
// Exported Functions
//

bool CoreUnzip(std::filesystem::path file, std::filesystem::path path)
{
    std::string error;

    unzFile           zipFile;
    unz_global_info   zipInfo;
    std::ofstream     outputStream;
    std::vector<char> readBuffer(UNZIP_READ_SIZE);
    int bytes_read = 0;

    zipFile = unzOpen(file.string().c_str());
    if (zipFile == nullptr)
    {
        error = "CoreUnzip: unzOpen Failed!";
        CoreSetError(error);
        return false;
    }

    if (unzGetGlobalInfo(zipFile, &zipInfo) != UNZ_OK)
    {
        error = "CoreUnzip: unzGetGlobalInfo Failed!";
        CoreSetError(error);
        return false;
    }


    for (uint64_t i = 0; i < zipInfo.number_entry; i++)
    {
        unz_file_info fileInfo;
        char          fileName[PATH_MAX];
        std::filesystem::path targetPath;

        // ensure we can retrieve the current file info
        if (unzGetCurrentFileInfo(zipFile, &fileInfo, fileName, PATH_MAX, nullptr, 0, nullptr, 0) != UNZ_OK)
        {
            unzClose(zipFile);
            error = "CoreUnzip: unzGetCurrentFileInfo Failed!";
            CoreSetError(error);
            return false;
        }

        targetPath = path;
        targetPath += "/";
        targetPath += fileName;

        if (targetPath.string().ends_with("/"))
        { // directory
            try
            {
                if (!std::filesystem::is_directory(targetPath) && 
                    !std::filesystem::create_directory(targetPath))
                {
                    throw std::exception();
                }
            }
            catch (...)
            {
                unzClose(zipFile);
                error = "CoreUnzip: std::filesystem::create_directory(";
                error += targetPath.string();
                error += ") Failed!";
                CoreSetError(error);
                return false;
            }
        }
        else
        { // file
            if (unzOpenCurrentFile(zipFile) != UNZ_OK)
            {
                unzClose(zipFile);
                error = "CoreUnzip: unzOpenCurrentFile Failed!";
                CoreSetError(error);
                return false;
            }

            outputStream.open(targetPath, std::ios::trunc | std::ios::binary);
            if (!outputStream.is_open())
            {
                unzCloseCurrentFile(zipFile);
                unzClose(zipFile);
                error = "CoreUnzip: failed to open file!";
                error += targetPath.string();
                CoreSetError(error);
                return false;
            }

            do
            {
                bytes_read = unzReadCurrentFile(zipFile, readBuffer.data(), UNZIP_READ_SIZE);
                if (bytes_read < 0)
                {
                    unzCloseCurrentFile(zipFile);
                    unzClose(zipFile);
                    error = "CoreUnzip: unzReadCurrentFile Failed!";
                    CoreSetError(error);
                    return false;
                }
                else if (bytes_read > 0)
                { // write data to file
                    outputStream.write(readBuffer.data(), bytes_read);
                }
            } while (bytes_read > 0);

            outputStream.close();
            unzCloseCurrentFile(zipFile);
        }

        // break when we've iterated over all entries
        if ((i + 1) >= zipInfo.number_entry)
        {
            break;
        }

        // move to next file
        if (unzGoToNextFile(zipFile) != UNZ_OK)
        {
            unzClose(zipFile);
            error = "CoreUnzip: unzGoToNextFile Failed!";
            CoreSetError(error);
            return false;
        }
    }

    unzClose(zipFile);
    return true;
}
