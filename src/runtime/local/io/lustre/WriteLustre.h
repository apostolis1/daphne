#pragma once

#include <stdio.h>
// #include <lustre/lustreapi.h>
#include <parser/metadata/MetaDataParser.h>
#include <runtime/local/io/lustre/WriteLustreCsv.h>
#include <runtime/local/io/lustre/WriteDaphneLustre.h>

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
            distributedWrite<DTArg>(arg, filename, dctx);
        } else {
            if (extension == ".csv") {              
                writeLustreCsv(arg, filename, dctx);
            } else if (extension == ".dbdf") {
                writeDaphneLustre(arg, filename, dctx);
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