#pragma once

#include <runtime/local/context/DaphneContext.h>
#include <runtime/local/datastructures/DataObjectFactory.h>
#include <runtime/local/datastructures/DenseMatrix.h>
#include <lustre/lustreapi.h>
#include <nlohmannjson/json.hpp>


template <class DTArg>
struct WriteLustreCsv
{
    static void apply(const DTArg *arg, const char *filename, DCTX(dctx)) = delete;
};

// ****************************************************************************
// Convenience function
// ****************************************************************************

template <class DTArg>
void writeLustreCsv(const DTArg *arg, const char *filename, DCTX(dctx)) {
    std:: cout << "writeLustreCsv convenience function called \n";
    WriteLustreCsv<DTArg>::apply(arg, filename, dctx);
}

// ****************************************************************************
// (Partial) template specializations for different data/value types
// ****************************************************************************

// ----------------------------------------------------------------------------
// DenseMatrix
// ----------------------------------------------------------------------------

template <typename VT>
struct WriteLustreCsv<DenseMatrix<VT>> {
    static void apply(const DenseMatrix<VT> *arg, const char *filename, DCTX(dctx)) {
        if (filename == nullptr)
            throw(std::runtime_error("File path required"));
        
        std::string fn(filename);
        
        // TODO Check if path exists
        // Write file metadata
        FileMetaData fmd(arg->getNumRows(), arg->getNumCols(), true, ValueTypeUtils::codeFor<VT>);
        auto fmdStr = MetaDataParser::writeMetaDataToString(fmd);
        if (fmdStr.size() == 0 ) {
            throw std::runtime_error("Metadata string could not be parsed\n");
            return ;
        }
        auto mdtFn = fn + ".meta";

        // Open metadata file for writing
        // TODO what if the file already exists

        int stripe_size = 65536;    /* System default is 4M */
        int stripe_offset = -1;     /* Start at default */
        int stripe_count = 1;       /* Amount of stripes, eg fragments */
        int stripe_pattern = 0;     /* only RAID 0 at this time */

        int fd = llapi_file_open(static_cast<const char *>(mdtFn.c_str()), O_CREAT | O_WRONLY | O_TRUNC, 0644, stripe_size, -1, -1, 0);
        if (fd < 0)
            throw std::runtime_error("Error opening Metadata file");

        // Write metadata
        
        dprintf(fd, "%s", fmdStr.c_str());
        std::cout << "Successfull metadata write \n";
        if (close(fd) < 0) {
                fprintf(stderr, "File close failed: %d (%s)\n", errno, strerror(errno));
                return ;
        }
        // Open .lustre file
        // If file exists don't pass the O_CREAT flag
        fd = llapi_file_open(static_cast<const char *>(fn.c_str()), O_CREAT | O_WRONLY | O_TRUNC, 0644, stripe_size, stripe_offset, stripe_count, stripe_pattern);
        if (fd < 0)
            throw std::runtime_error("Error opening Lustre file");


        // Write actual data
        const VT * valuesArg = arg->getValues();
        const size_t rowSkip = arg->getRowSkip();
        const size_t argNumCols = arg->getNumCols();

        for (size_t i = 0; i < arg->getNumRows(); ++i)
        {
            for(size_t j = 0; j < argNumCols; ++j)
            {
                dprintf(
                        fd,
                        std::is_floating_point<VT>::value ? "%12f" : (std::is_same<VT, long int>::value ? "%12ld" : "%12d"),
                        valuesArg[i*rowSkip + j]
                );
                if(j < (arg->getNumCols() - 1))
                    dprintf(fd, ",");
                else
                    dprintf(fd, "\n");
            }
        }

        std::cout << "Successfull content write \n";
        if (close(fd) < 0) {
                fprintf(stderr, "File close failed: %d (%s)\n", errno, strerror(errno));
                return ;
        } 
        return;
    }
};