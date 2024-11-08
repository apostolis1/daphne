#pragma once

// #include <runtime/local/datastructures/DenseMatrix.h>
// #include <runtime/local/datastructures/DataObjectFactory.h>
#include <runtime/local/context/DaphneContext.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <runtime/local/io/utils.h>

template <class DTRes>
struct ReadLustreCsv
{
    static void apply(DTRes *&res, const char *lustreFilename, size_t numRows,
                      size_t numCols, char delim, DCTX(dctx),
                      size_t startRow = 0) = delete;
};


// ****************************************************************************
// Convenience function
// ****************************************************************************

template <class DTRes>
void readLustreCsv(DTRes *&res, const char *lustreFilename, size_t numRows,
                 size_t numCols, char delim, DCTX(dctx), size_t startRow = 0) {
    ReadLustreCsv<DTRes>::apply(res, lustreFilename, numRows, numCols, delim, dctx,
                              startRow);
}

// ****************************************************************************
// (Partial) template specializations for different data/value types
// ****************************************************************************

// ----------------------------------------------------------------------------
// DenseMatrix
// ----------------------------------------------------------------------------


template <typename VT> struct ReadLustreCsv<DenseMatrix<VT>> {
    static void apply(DenseMatrix<VT> *&res, const char *lustreFilename, size_t numRows,
                      size_t numCols, char delim, DCTX(dctx),
                      size_t startRow = 0) {
        // Reads numRows to the DenseMatrix res, starting from startRow
        if (lustreFilename == nullptr) {
            throw std::runtime_error("File required");
        }
        if (numRows <= 0) {
            throw std::runtime_error("numRows must be > 0");
        }
        if (numRows <= 0) {
            throw std::runtime_error("numCols must be > 0");
        }
        
        if (res == nullptr) {
            res = DataObjectFactory::create<DenseMatrix<VT>>(numRows, numCols,
                                                             false);
        }

        if (res == nullptr) {
            throw std::runtime_error("Could not initialize result matrix");
        }


        int fd;
        // The example here uses simple open instead of llapi_file_open https://doc.lustre.org/lustre_manual.xhtml#example_using_llapi
        // TODO: Check if this is correct or for some reason we want llapi_file_open

        fd = open(lustreFilename, O_RDONLY, 0644);
        if (fd < 0) {
            throw std::runtime_error("Can't open file");
        }
        // TODO: This need to be replaced by the LustreUtils method
        // Currently the LustreUtils can't be linked to this file due to the compilation issue
        // int charsPerCell = LustreUtils::getCharsPerCSVCell();
        int charsPerCell = 17;

        size_t lineSize = numCols * charsPerCell + (numCols-1) * sizeof(delim) + sizeof('\n');
        size_t parsedRows = 0;
        // TODO: check if skiprows affects offset somehow
        size_t offset = startRow * lineSize;
        VT *valuesRes = res->getValues();

        // TODO: Increate this buffer size, it is small only to cause multiple writes to catch potential errors during testing 
        // Should be something like char buffer[1UL << 20];
        char buffer[1UL << 20];
        char *cur = nullptr;
        ssize_t n = 0;
        // Read until numRows
        while (parsedRows < numRows) {
            std::string line;

            do {
                if (cur == nullptr) { // buffer is empty or all data in buffer are parsed already
                    n = pread(fd, buffer, sizeof(buffer), offset);
                    // Move the offset according to data read
                    offset += n;
                    if (n < 0) {
                        throw std::runtime_error(
                            "Could not read lustre file");
                    }
                    cur = buffer;
                }
                // buffer contains n useful chars
                // Check if there is an \n at the remaining characters, meaning from cur till n position on buffer
                char *eol = (char *)std::memchr(cur, '\n',  static_cast<ssize_t>(buffer+n - cur));
                
                if (eol == nullptr || static_cast<ssize_t>(eol - cur) >= n) { // End of line not found or eol found after the n chars
                    line.append(cur, static_cast<ssize_t>(buffer+n - cur)); // I have already consumed cur elements from buffer, take that into account
                    cur = nullptr;
                } else { // End of line found
                    line.append(cur, eol - cur);
                    cur = eol + 1; // Keep track of start of remaining data in buffer
                }
            } while (cur == nullptr);

            // TODO: Check if we need to skip rows for some reason, is it done in  HDFS ?

            // Parse row
            size_t pos = 0;
            for (size_t c = 0; c < numCols; c++) {
                VT val;
                convertCstr(line.c_str() + pos, &val);

                // TODO This assumes that rowSkip == numCols.
                *valuesRes = val;
                // std::cout << "Found val: " << val << std::endl;
                ++valuesRes;
                // TODO We could even exploit the fact that the strtoX
                // functions can return a pointer to the first character
                // after the parsed input, then we wouldn't have to search
                // for that ourselves, just would need to check if it is
                // really the delimiter.
                if (c < numCols - 1) {
                    while (line[pos] != delim)
                        pos++;
                    pos++; // skip delimiter
                }
            }
            parsedRows++;
            if (parsedRows == numRows)
                break;
        }

        if (close(fd) < 0) {
            throw std::runtime_error("Can't close file");
        }

    }
};