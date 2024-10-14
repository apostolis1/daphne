#pragma once

#include <stdio.h>
// #include <lustre/lustreapi.h>
#include <runtime/local/io/lustre/WriteLustreCsv.h>

#include <runtime/distributed/coordinator/kernels/DistributedWrite.h>

// ****************************************************************************
// Struct for partial template specialization
// ****************************************************************************

template <class DTArg>
struct WriteLustre
{
    static void apply(const DTArg *arg, const char *filename, DCTX(dctx)) 
    {
        std::filesystem::path filePath(filename);
        auto extension = filePath.stem().extension().string();
        if (dctx->config.use_distributed) {
            std::cout << "Distributed write initiated\n";
            distributedWrite<DTArg>(arg, filename, dctx);
        } else {
            if (extension == ".csv") {              
                writeLustreCsv(arg, filename, dctx);
            } else if (extension == ".dbdf") {
                // writeDaphneHDFS(arg, hdfsfilename.c_str(), dctx);
            }
        }
    }
};


// ****************************************************************************
// Convenience function
// ****************************************************************************

template <class DTArg>
void writeLustre(const DTArg *arg, const char *filename, DCTX(dctx))
{
    WriteLustre<DTArg>::apply(arg, filename, dctx);
}