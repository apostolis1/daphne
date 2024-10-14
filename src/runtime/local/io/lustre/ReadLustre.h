#pragma once

#include <stdio.h>

#include <runtime/local/io/lustre/ReadLustreCsv.h>
#include <runtime/local/io/lustre/readDaphneLustre.h>

#include <runtime/distributed/coordinator/kernels/DistributedRead.h>
#include <lustre/lustreapi.h>


// ****************************************************************************
// Struct for partial template specialization
// ****************************************************************************

template <class DTRes>
struct ReadLustre
{
    static void apply(DTRes *&res, const char *filename, DCTX(dctx))
    {
        // // Grab metadata
        std::cout << "ReadLustre struct called\n";
        FileMetaData fmd = MetaDataParser::readMetaData(filename);
        std::cout << "Fmd read succesfully\n";
        res = DataObjectFactory::create<DTRes>(fmd.numRows, fmd.numCols, false);

        // // Get nested file extension
        std::filesystem::path filePath(filename);
        auto extension = filePath.stem().extension().string();
        
        if (dctx->config.use_distributed) {
            // Assign the work to distributedRead kernel
            distributedRead<DTRes>(res, filename, dctx);
        } else {
            std::cout << "Local runtime\n";

            // Assign the work to corresponding local kernels
            if (extension == ".csv") {
                readLustreCsv(res, filename, fmd.numRows, fmd.numCols, ',', dctx);
            } else if (extension == ".dbdf") {
                // readDaphneLustre(res, filename, dctx);
                ;
            }
        }
    }
};


// ****************************************************************************
// Convenience function
// ****************************************************************************

template <class DTRes>
void readLustre(DTRes *&res, const char *filename, DCTX(dctx)) 
{
    ReadLustre<DTRes>::apply(res, filename, dctx);
}