#pragma once

template <class DTArg>
struct WriteDaphneLustre
{
    static void apply(const DTArg *arg, const char *filename, DCTX(dctx), size_t start_row = 0) = delete;
};

// ****************************************************************************
// Convenience function
// ****************************************************************************

template <class DTArg>
void writeDaphneLustre(const DTArg *arg, const char *filename, DCTX(dctx), size_t start_row = 0) {
    std:: cout << "writeDaphneLustre convenience function called \n";
    WriteDaphneLustre<DTArg>::apply(arg, filename, dctx, start_row);
}


// ****************************************************************************
// (Partial) template specializations for different data/value types
// ****************************************************************************

// ----------------------------------------------------------------------------
// DenseMatrix
// ----------------------------------------------------------------------------


template <typename VT>
struct WriteDaphneLustre<DenseMatrix<VT>> {
    static void apply(const DenseMatrix<VT> *arg, const char *filename, DCTX(dctx), size_t start_row = 0) {
        std::cout << "Template for Densematrix" << std::endl;

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
            std::cout << "Successfull metadata write \n";
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
        
        std::cout << "Writing data: rows: " << arg->getNumRows() << " cols: " << arg->getNumCols() << " starting from row: " << start_row << std::endl;
        
        // Write actual data
        const VT * valuesArg = arg->getValues();
        const size_t rowSkip = arg->getRowSkip();
        const size_t argNumCols = arg->getNumCols();


    }
    
};