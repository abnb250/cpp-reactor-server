#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>
#include <cstring>
#include <sys/types.h>

namespace reactor{

class Buffer{
public:
    static constexpr size_t kCheapPrepend = 8;
    static constexpr size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize) : 
        buffer_(kCheapPrepend + initialSize), 
        readerIndex_(kCheapPrepend), 
        writerIndex_(kCheapPrepend) {}

    size_t readableBytes () const {return writerIndex_ - readerIndex_; }
    size_t writableBytes () const {return buffer_.size() - writerIndex_; }
    size_t prependableBytes () const {return readerIndex_; }

    const char* peek () const {return begin() + readerIndex_; }

    const char* findEOL() const{
        return static_cast<const char*>(std::memchr(peek(), '\n',
                                        readableBytes() ) );
    }

    ////
    void retrieve(size_t len){
        if(len < readableBytes()) readerIndex_ += len;
        else retrieveAll();
    }

    void retrieveAll(){
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAllAsString(){
        std::string s(peek(), readableBytes());
        retrieveAll();
        return s;
    }

    void ensureWritableBytes(size_t len){
        if(writableBytes() < len) makeSpace(len);
    }

    char* beginWrite() {return begin() + writerIndex_; }
    void hasWritten(size_t len) {writerIndex_ += len; }

    void append(const char* data, size_t len){
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        hasWritten(len);
    }

    ssize_t readFd(int fd, int* sacedErrno);
    ////

private:
    char* begin() {return buffer_.data(); }
    const char* begin() const {return buffer_.data(); }

    void makeSpace(size_t len){
        if(prependableBytes() + writableBytes() < len + kCheapPrepend){
            buffer_.resize(writerIndex_ + len);
        }
        else{
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

}//namespace reactor