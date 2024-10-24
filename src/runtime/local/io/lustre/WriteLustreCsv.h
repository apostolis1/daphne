#pragma once

#include <runtime/local/context/DaphneContext.h>
#include <runtime/local/datastructures/DataObjectFactory.h>
#include <runtime/local/datastructures/DenseMatrix.h>
#include "LustreUtils.h"
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

// Utility functions
ssize_t writeBufferToFile(int fd, char* buffer, size_t size, size_t offset) {
    ssize_t res = pwrite(fd, buffer, size, offset);
    return res;

    // TODO Check if I need a persistent write, something like
    // https://stackoverflow.com/questions/694188/when-does-the-write-system-call-write-all-of-the-requested-buffer-versus-just
    //     while(size > 0 && (res=write(fd,buff,size))!=size) {
    //     if(res<0 && errno==EINTR) 
    //     continue;
    //     if(res < 0) {
    //         // real error processing
    //         break;
    //     }
    //     size-=res;
    //     buf+=res;
    // }
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
            // TODO: These can be moved somewhere else
            int stripe_size = 65536;    /* System default is 4M */
            int stripe_offset = -1;     /* Start at default */
            int stripe_count = 1;       /* Amount of stripes, eg fragments */
            int stripe_pattern = 0;     /* only RAID 0 at this time */

            fd = llapi_file_open(static_cast<const char *>(mdtFn.c_str()), O_CREAT | O_WRONLY , 0644, stripe_size, -1, -1, 0);
            if (fd < 0)
                throw std::runtime_error("Error opening Metadata file");

            // Write metadata
            
            dprintf(fd, "%s", fmdStr.c_str());
            if (close(fd) < 0) {
                    fprintf(stderr, "File close failed: %d (%s)\n", errno, strerror(errno));
                    return ;
            }
        }
        // Open .lustre file
        // If file exists don't pass the O_CREAT flag
        std::filesystem::path filePath(filename);

        if (!std::filesystem::exists(filePath)) {

            int stripe_size = 65536;    /* System default is 4M */
            int stripe_offset = -1;     /* Start at default */
            int stripe_count = 1;       /* Amount of stripes, eg fragments */
            int stripe_pattern = 0;     /* only RAID 0 at this time */
            
            // TODO: Maybe llapi_file_create here?
            fd = llapi_file_open(static_cast<const char *>(fn.c_str()), O_CREAT | O_WRONLY , 0644, stripe_size, stripe_offset, stripe_count, stripe_pattern);
            if (fd < 0)
                throw std::runtime_error("Error opening Lustre file");

        }
        
        // Open lustre file
        fd = open(filename, O_WRONLY, 0644);
        
        // Write actual data
        const VT * valuesArg = arg->getValues();
        const size_t rowSkip = arg->getRowSkip();
        const size_t argNumCols = arg->getNumCols();

        int charsPerCell = 12;
        size_t lineSize = argNumCols * charsPerCell + (argNumCols-1) * sizeof(',') + sizeof('\n');
        size_t offset = start_row * lineSize;
        char buffer[1UL << 20];
        size_t charsWrittenToBuffer = 0;

        for (size_t i = 0; i < arg->getNumRows(); ++i)
        {
            for(size_t j = 0; j < argNumCols; ++j)
            {
                if (sizeof(buffer) > charsWrittenToBuffer + charsPerCell) {
                    sprintf(
                        buffer+charsWrittenToBuffer,
                        std::is_floating_point<VT>::value ? "%12f" : (std::is_same<VT, long int>::value ? "%12ld" : "%12d"),
                        valuesArg[i*rowSkip + j]
                    );
                    charsWrittenToBuffer += charsPerCell;
                }
                else {
                    // Write buffer with pwrite
                    ssize_t res = writeBufferToFile(fd, buffer, charsWrittenToBuffer, offset);
                    charsWrittenToBuffer = 0;
                    offset += res;
                    sprintf(
                        buffer+charsWrittenToBuffer,
                        std::is_floating_point<VT>::value ? "%12f" : (std::is_same<VT, long int>::value ? "%12ld" : "%12d"),
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
                    ssize_t res = writeBufferToFile(fd, buffer, charsWrittenToBuffer, offset);
                    charsWrittenToBuffer = 0;
                    offset += res;
                    sprintf(buffer+charsWrittenToBuffer, c.c_str());
                    charsWrittenToBuffer++;
                }
            }
        }

        // Finally write any data that might still be in buffer
        ssize_t res = writeBufferToFile(fd, buffer, charsWrittenToBuffer, offset);
        if (close(fd) < 0) {
                fprintf(stderr, "File close failed: %d (%s)\n", errno, strerror(errno));
                return ;
        }
        return;
    }
};