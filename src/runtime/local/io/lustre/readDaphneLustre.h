#pragma once

#include <runtime/local/datastructures/DataObjectFactory.h>
#include <runtime/local/datastructures/DenseMatrix.h>
#include <runtime/local/datastructures/CSRMatrix.h>
#include <runtime/local/datastructures/Frame.h>
#include <runtime/local/context/DaphneContext.h>

template <class DTRes>
struct ReadDaphneLustre
{
    static void apply(DTRes *&res, const char *filename, DCTX(dctx), size_t startRow = 0) = delete;
};

// ****************************************************************************
// Convenience function
// ****************************************************************************

template <class DTRes>
void readDaphneLustre(DTRes *&res, const char *filename, DCTX(dctx), size_t startRow = 0)
{
    ReadDaphneLustre<DTRes>::apply(res, filename, dctx, startRow);
}