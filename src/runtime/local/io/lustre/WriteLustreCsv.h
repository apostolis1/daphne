#pragma once

#include <runtime/local/context/DaphneContext.h>
#include <runtime/local/datastructures/DataObjectFactory.h>
#include <runtime/local/datastructures/DenseMatrix.h>
// #include "LustreUtils.h"
#include <runtime/local/io/lustre/LustreUtils.h>
#include <filesystem>

template <class DTArg>
struct WriteLustreCsv
{
    static void apply(const DTArg *arg, const char *filename, DCTX(dctx), size_t start_row = 0) = delete;
};

// ****************************************************************************
// Convenience function
// ****************************************************************************

template <class DTArg>
void writeLustreCsv(const DTArg *arg, const char *filename, DCTX(dctx), size_t start_row = 0) {
    WriteLustreCsv<DTArg>::apply(arg, filename, dctx, start_row);
}


// ****************************************************************************
// (Partial) template specializations for different data/value types
// ****************************************************************************

// ----------------------------------------------------------------------------
// DenseMatrix
// ----------------------------------------------------------------------------

template <typename VT>
struct WriteLustreCsv<DenseMatrix<VT>> {
    static void apply(const DenseMatrix<VT> *arg, const char *filename, DCTX(dctx), size_t start_row = 0) {
        if (filename == nullptr)
            throw(std::runtime_error("File path required"));
        
        std::string fn(filename);
        int fd;

        auto mdtFn = fn + ".meta";
        std::filesystem::path metadatafilePath(mdtFn);

        // If metadata file doesn't exist, create it and write metadata
        // Metadata file already exists if called by distributed runtime, because the coordinator is responsible for creating it
        // Similar for the actual lustre data file
        // We might want to change that, depending on what the intented behavior is when writing a file that already exists (we delete it / throw an error ?)
        if (!std::filesystem::exists(metadatafilePath)) {        
            // Write file metadata
            FileMetaData fmd(arg->getNumRows(), arg->getNumCols(), true, ValueTypeUtils::codeFor<VT>);
            auto fmdStr = MetaDataParser::writeMetaDataToString(fmd);
            if (fmdStr.size() == 0 ) {
                throw std::runtime_error("Metadata string could not be parsed\n");
                return ;
            }

            // Open metadata file for writing
            // fd = LustreUtils::openFile(static_cast<const char *>(mdtFn.c_str()), O_CREAT | O_WRONLY);
            fd = LustreUtils::openMetadataFile(static_cast<const char *>(mdtFn.c_str()), O_CREAT | O_WRONLY);
            if (fd < 0)
                throw std::runtime_error("Error opening Metadata file");

            // Write metadata
            
            dprintf(fd, "%s", fmdStr.c_str());
            LustreUtils::closeFile(fd);
        }
        // Open .lustre file
        // If file exists don't pass the O_CREAT flag
        std::filesystem::path filePath(filename);

        if (!std::filesystem::exists(filePath)) {
            
            // TODO: Maybe llapi_file_create here?
            fd = LustreUtils::openFile(fn.c_str(), O_CREAT | O_WRONLY, dctx);
            if (fd < 0)
                throw std::runtime_error("Error opening Lustre file");

        }
        
        // Open lustre file
        fd = open(filename, O_WRONLY, 0644);
        
        // Write actual data
        const VT * valuesArg = arg->getValues();
        const size_t rowSkip = arg->getRowSkip();
        const size_t argNumCols = arg->getNumCols();

        int charsPerCell = LustreUtils::getCharsPerCSVCell();
        // size_t lineSize = argNumCols * charsPerCell + (argNumCols-1) * sizeof(',') + sizeof('\n');
        size_t lineSize = LustreUtils::getCSVLineSize(argNumCols);
        size_t offset = start_row * lineSize;
        char buffer[1UL << 20];
        size_t charsWrittenToBuffer = 0;
        // TODO: Replace the hardcoded 20f with a variable considering CHARSPERCELL macro
        for (size_t i = 0; i < arg->getNumRows(); ++i)
        {
            for(size_t j = 0; j < argNumCols; ++j)
            {
                if (sizeof(buffer) > charsWrittenToBuffer + charsPerCell) {
                    sprintf(
                        buffer+charsWrittenToBuffer,
                        // std::is_floating_point<VT>::value ? "%20f" : (std::is_same<VT, long int>::value ? "%20ld" : "%20d"),
                        LustreUtils::get_format_specifier<VT>(),
                        valuesArg[i*rowSkip + j]
                    );
                    charsWrittenToBuffer += charsPerCell;
                }
                else {
                    // Write buffer with pwrite
                    ssize_t res = LustreUtils::writeBufferToFile(fd, buffer, charsWrittenToBuffer, offset);
                    charsWrittenToBuffer = 0;
                    offset += res;
                    sprintf(
                        buffer+charsWrittenToBuffer,
                        // std::is_floating_point<VT>::value ? "%20f" : (std::is_same<VT, long int>::value ? "%20ld" : "%20d"),
                        LustreUtils::get_format_specifier<VT>(),
                        valuesArg[i*rowSkip + j]
                    );
                    charsWrittenToBuffer += charsPerCell;
                }

                std::string c = j < (arg->getNumCols() - 1) ? "," : "\n"; 
                if (sizeof(buffer) > charsWrittenToBuffer + sizeof(c)) 
                {
                    sprintf(buffer+charsWrittenToBuffer, c.c_str());
                    charsWrittenToBuffer++;
                }
                else 
                {
                    // Pwrite buffer
                    ssize_t res = LustreUtils::writeBufferToFile(fd, buffer, charsWrittenToBuffer, offset);
                    charsWrittenToBuffer = 0;
                    offset += res;
                    sprintf(buffer+charsWrittenToBuffer, c.c_str());
                    charsWrittenToBuffer++;
                }
            }
        }

        // Finally write any data that might still be in buffer
        ssize_t res = LustreUtils::writeBufferToFile(fd, buffer, charsWrittenToBuffer, offset);
        LustreUtils::closeFile(fd);
        return;
    }
};