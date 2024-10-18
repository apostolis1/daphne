#pragma once

#include <runtime/local/datastructures/DataObjectFactory.h>
#include <runtime/local/datastructures/DenseMatrix.h>
#include <runtime/local/datastructures/CSRMatrix.h>
#include <runtime/local/datastructures/Frame.h>
#include <runtime/local/context/DaphneContext.h>
#include <runtime/local/io/DaphneSerializer.h>


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

// ****************************************************************************
// (Partial) template specializations for different data/value types
// ****************************************************************************

// ----------------------------------------------------------------------------
// DenseMatrix
// ----------------------------------------------------------------------------

template <typename VT> struct ReadDaphneLustre<DenseMatrix<VT>> {
    static void apply(DenseMatrix<VT> *&res, const char *lustreFilename, DCTX(dctx),
                    size_t startRow = 0) {
        if (res == NULL) {
            throw std::runtime_error("Could not initialize result matrix");
        }
        std::cout << "Reading daphne object from lustre" << std::endl;
        size_t numRows = res->getNumRows();
        size_t numCols = res->getNumCols();

        auto headerSize = DaphneSerializer<DenseMatrix<VT>>::headerSize(res);
        
        auto fileSize = 1UL << 7;
        // Allocate buffer
        std::vector<char> buffer(fileSize);

        // I can check whether I have parsed the region of the file I am reponsible for 
        // by checking the bytes read instead of the rows
        // I can also change this to check the rows by dividing by lineSize
        size_t matrixBytes = numRows * numCols * sizeof(VT);
        size_t bytesToParse = matrixBytes;
        size_t parsedBytes = 0;
        size_t offset; // The position of the file from where we are reading
        // We don't care about the header because the matrix is allready created
        // We just want to read the actual data of the matrix
        offset = startRow * numCols * sizeof(VT) + headerSize;
        size_t startSerByte = headerSize;
        size_t bufferEnd;

        int fd;
        // The example here uses simple open instead of llapi_file_open https://doc.lustre.org/lustre_manual.xhtml#example_using_llapi
        // TODO: Check if this is correct or for some reason we want llapi_file_open
        fd = open(lustreFilename, O_RDONLY, 0644);
        if (fd < 0) {
            throw std::runtime_error("Can't open file");
        }

        while (parsedBytes < bytesToParse) {
            // Since we always skip the header, we don't need to check for n >= headerSize as specified
            // on the deserialize method
            ssize_t n = pread(fd, buffer.data(), sizeof(buffer), offset);
            if (n < 0) {
                std::cout << "Error reading from file" << std::endl;
            }
            offset += n;
            parsedBytes += n;
            std::cout << "Parsedbytes : " << parsedBytes << std::endl; 
            res = DaphneSerializer<DenseMatrix<VT>>::deserialize(
                buffer.data(), n, res, startSerByte);
            startSerByte += n;
        }
        close(fd);
    }
};