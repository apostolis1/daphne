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
    std::cout << *lustreFilename << std::endl;
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
        std::cout << "Here\n";
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

        std::cout << "ReadLustreCSV for DenseMatrix called\n";

        int fd;
        // The example here uses simple open instead of llapi_file_open https://doc.lustre.org/lustre_manual.xhtml#example_using_llapi
        // TODO: Check if this is correct or for some reason we want llapi_file_open

        fd = open(lustreFilename, O_RDONLY, 0644);
        if (fd < 0) {
            throw std::runtime_error("Can't open file");
        }

        int charsPerCell = 12;
        size_t lineSize = numCols * charsPerCell + (numCols-1) * sizeof(delim) + sizeof('\n');
        size_t parsedRows = 0;
        // TODO: check if skiprows affects offset somehow
        // TODO: Testing changes, try to read everything but the first row
        // startRow = 10;
        // numRows = numRows - startRow;
        std::cout << "StartRow is : "<< startRow << std::endl;
        size_t offset = startRow * lineSize;
        VT *valuesRes = res->getValues();


        printf("Trying to read %li rows, starting from row %li \n", numRows, startRow);
        char buffer[1UL << 7];
        char *cur = nullptr;
        ssize_t n = 0;
        // Read until numRows
        while (parsedRows < numRows) {
            printf("Parsed Rows: %li\n", parsedRows);
            std::string line;

            do {
                if (cur == nullptr) { // buffer is empty or all data in buffer are parsed already
                    printf("Attempting to read at offset %li\n", offset);
                    n = pread(fd, buffer, sizeof(buffer), offset);
                    // Move the offset according to data read
                    printf("Succesfully read %li bytes\n", n);
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
                    printf("Can't find eol, appending to line\n");
                    std::cout << "Line before append is: **:" << line << ":** " << std::endl ;
                    line.append(cur, static_cast<ssize_t>(buffer+n - cur)); // I have already consumed cur elements from buffer, take that into account
                    std::cout << "Line after append is: **:" << line << ":** " << std::endl ;
                    cur = nullptr;
                } else { // End of line found
                    printf("Found eol, appending to line\n");
                    std::cout << "Line before append is: **:" << line << ":** " << std::endl ;
                    printf("Trying to append %li chars\n", eol - cur);
                    line.append(cur, eol - cur);
                    std::cout << "Line after append is: **:" << line << ":** " << std::endl ;
                    cur = eol + 1; // Keep track of start of remaining data in buffer
                }
            } while (cur == nullptr);

            // TODO: Check if we need to skip rows for some reason, they do in HDFS

            // Parse row
            printf("Parsing line %li\n", startRow + parsedRows);
            std::cout << "Raw data is: **:" << line << ":** " << std::endl ;
            size_t pos = 0;
            for (size_t c = 0; c < numCols; c++) {
                VT val;
                convertCstr(line.c_str() + pos, &val);

                // TODO This assumes that rowSkip == numCols.
                *valuesRes = val;
                std::cout << "Found val: " << val << std::endl;
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
            printf("Finished parsing line %li\n", startRow + parsedRows);
            parsedRows++;
            if (parsedRows == numRows)
                break;
        }

        if (close(fd) < 0) {
            throw std::runtime_error("Can't close file");
        }

    }
};