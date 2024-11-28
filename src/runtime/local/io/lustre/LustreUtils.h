#pragma once

#include <lustre/lustreapi.h>

#define LUSTRE_STRIPE_COUNT 4
#define FILE_SIZE 1057968 // This is a constant for testing, should implement a method that calculates it
#define LUSTRE_STRIPE_SIZE 65536
#define CHARS_PER_CSV_CELL 8
#define LUSTRE_DELETE_FILES_IF_EXIST false

struct LustreUtils {
    static int getStripeCount() {
        return LUSTRE_STRIPE_COUNT;
    }

    static int getStripeSize() {
        // It is not that simple, it has to be an even multiple of 65536
        // return FILE_SIZE / LUSTRE_STRIPE_COUNT;
        return LUSTRE_STRIPE_SIZE;
    }

    static int openMetadataFile(const char* filename, int flags) {
        /*
         * Specifically used to create metadata files, since metadata files are very small and we probably want them to only have 1 stripe and 1 ost
         */
        int stripe_size = 65536;    /* System default is 4M */
        int stripe_offset = 0;     /* Put all metadata files on the first OST for testing purposes, in general it should be -1 */
        int stripe_count = 1;       /* Amount of stripes, eg fragments */
        int stripe_pattern = 0;     /* only RAID 0 at this time */
        int fd = llapi_file_open(filename, flags , 0644, stripe_size, stripe_offset, stripe_count, stripe_pattern);
        return fd;
    }

    static int openFile(const char* filename, int flags) {
        int stripe_size = getStripeSize();    /* System default is 4M */
        int stripe_offset = 0;     /* Start at OST0 for testing purposes, in general it should be -1 */
        int stripe_count = getStripeCount();       /* Amount of stripes, eg fragments */
        int stripe_pattern = 0;     /* only RAID 0 at this time */
        int fd = llapi_file_open(filename, flags , 0644, stripe_size, stripe_offset, stripe_count, stripe_pattern);
        return fd;
    }

    static void closeFile(int fd) {
        if (close(fd) < 0) {
            throw std::runtime_error("Can't close file");
        }
    }

    static ssize_t writeBufferToFile(int fd, char* buffer, size_t size, size_t offset) {
        ssize_t res = pwrite(fd, buffer, size, offset);
        return res;

        // TODO Check if I need a persistent write, something like
        // https://stackoverflow.com/questions/694188/when-does-the-write-system-call-write-all-of-the-requested-buffer-versus-just
        //     while(size > 0 && (res=write(fd,buff,size))!=size) {
        //     if(res<0 && errno==EINTR) 
        //     continue;
        //     if(res < 0) {
        //         // real error processing
        //         break;
        //     }
        //     size-=res;
        //     buf+=res;
        // }
    }

    static int getCharsPerCSVCell() {
        return CHARS_PER_CSV_CELL;
    }

    static size_t getCSVLineSize(int argNumCols) {         
        size_t lineSize = argNumCols * getCharsPerCSVCell() + (argNumCols-1) * sizeof(',') + sizeof('\n');
        return lineSize;
    }

    template <typename VT>
    static const char * get_format_specifier() {
        if constexpr (std::is_floating_point<VT>::value) {
            return ("%" + std::to_string(CHARS_PER_CSV_CELL) + "f").c_str();
        } else if constexpr (std::is_same<VT, long int>::value) {
            return ("%" + std::to_string(CHARS_PER_CSV_CELL) + "ld").c_str();
        } else {
            return ("%" + std::to_string(CHARS_PER_CSV_CELL) + "d").c_str();
        }
    }

};