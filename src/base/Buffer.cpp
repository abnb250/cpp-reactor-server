#include "base/Buffer.h"
#include <sys/uio.h>
#include <cerrno>

namespace reactor{

ssize_t Buffer::readFd(int fd, int* savedErrno){
    char extrabuf[65536];
    size_t writable = writableBytes();
    iovec vec[2];
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    size_t iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    ssize_t n = ::readv(fd, vec, iovcnt);
    if(n < 0){
        *savedErrno = errno;
    }
    else if(static_cast<size_t>(n) <= writable){
        writerIndex_ += static_cast<size_t>(n);
    }
    else{
        writerIndex_ = buffer_.size();
        append(extrabuf, static_cast<size_t>(n) - writable);
    }

    return n;
}

}//namespace reactor;