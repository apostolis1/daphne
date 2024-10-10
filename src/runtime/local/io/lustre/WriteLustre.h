#pragma once

#include <runtime/local/datastructures/DataObjectFactory.h>
#include <runtime/local/datastructures/DenseMatrix.h>
#include <runtime/local/datastructures/CSRMatrix.h>
#include <runtime/local/datastructures/Frame.h>

#include <runtime/local/io/File.h>
#include <runtime/local/io/utils.h>
#include <runtime/local/io/DaphneSerializer.h>
#include <runtime/distributed/coordinator/kernels/DistributedWrite.h>
#include <runtime/local/datastructures/AllocationDescriptorGRPC.h>
#include <runtime/local/io/lustre/WriteLustreCsv.h>
#include <runtime/local/context/DaphneContext.h>

#include <util/preprocessor_defs.h>


// ****************************************************************************
// Struct for partial template specialization
// ****************************************************************************

template <class DTArg>
struct WriteLustre
{
    static void apply(const DTArg *arg, const char *filename, DCTX(dctx)) 
    {
        // TOOD Do the work here, check if we need to dispatch to distributed or local kernel
        std::cout << "Local write Lustre called \n";
        std::filesystem::path filePath(filename);
        auto extension = filePath.stem().extension().string();
        if (dctx->config.use_distributed) {
            std::cout << "Distributed write initiated\n";
            // distributedWrite<DTArg>(arg, filename, dctx);
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