
#include "FileIOHandler.h"
#include <system_error>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

FileIOHandler::FileIOHandler()
{
}

FileIOHandler::FileIOHandler(const std::string &filepath, OpenMode mode)
{
    Open(filepath, mode);
}

FileIOHandler::~FileIOHandler()
{
    Close();
}

bool FileIOHandler::Open(const std::string &path, OpenMode mode)
{
    mutex.Enter();
    bool result = false;
    try
    {
        Close();

        fd = ::open(path.c_str(), mode, file_mode_);
        if (fd != -1)
        {
            result = true;
            file_path = path;
            offset = 0;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    mutex.Leave();
    return result;
}

void FileIOHandler::Close()
{
    mutex.Enter();
    try
    {
        if (fd != -1)
        {
            ::close(fd);
            fd = -1;
        }
        file_path.clear();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    mutex.Leave();
}

bool FileIOHandler::IsOpen() const
{
    return fd != -1;
}

std::string FileIOHandler::FilePath() const
{
    return file_path;
}

long FileIOHandler::Read(char *buf, size_t bytesToRead)
{
    mutex.Enter();
    long result = 0;
    try
    {
        if (!CheckOpen())
        {
            mutex.Leave();
            return result;
        }

        result = ::read(fd, (void *)buf, bytesToRead);
        if (result < 0)
        {
            mutex.Leave();
            throw std::system_error(errno, std::system_category(), "Read failed");
        }
        offset += result;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    mutex.Leave();
    return result;
}

long FileIOHandler::Read(Buffer &buffer, size_t bytesToRead)
{
    // 确保Buffer有足够空间
    if (buffer.Remaind() < static_cast<int>(bytesToRead))
    {
        buffer.ReSize(buffer.Postion() + bytesToRead);
    }
    int result = Read(buffer.Byte() + buffer.Postion(), bytesToRead);
    return result;
}

long FileIOHandler::Write(const char *buf, size_t bytesToWrite)
{
    mutex.Enter();
    long result = 0;
    try
    {
        if (!CheckOpen())
            return result;
        result = ::write(fd, (const void *)buf, bytesToWrite);
        if (result < 0)
        {
            throw std::system_error(errno, std::system_category(), "Write failed");
        }
        offset += result;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    mutex.Leave();
    return result;
}

long FileIOHandler::Write(const Buffer &buf)
{
    return Write(buf.Byte(), buf.Length());
}

long FileIOHandler::Seek(SeekOrigin origin, long offset)
{
    mutex.Enter();
    long result = 0;
    try
    {
        if (!CheckOpen())
            return result;

        result = ::lseek(fd, offset, origin);
        if (result == -1)
        {
            throw std::system_error(errno, std::system_category(), "Seek failed");
        }
        offset = result;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    mutex.Leave();
    return result;
}

bool FileIOHandler::Flush()
{
    mutex.Enter();
    bool result = false;
    try
    {
        if (!CheckOpen())
            return false;
        result = ::fsync(fd) == 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    mutex.Leave();
    return result;
}

long FileIOHandler::GetSize() const
{
    mutex.Enter();
    long result = 0;
    try
    {
        if (!CheckOpen())
        {
            mutex.Leave();
            return result;
        }

        struct stat st;
        if (::fstat(fd, &st) == -1)
        {
            throw std::system_error(errno, std::system_category(), "GetSize failed");
        }
        result = st.st_size;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    mutex.Leave();
    return result;
}

bool FileIOHandler::Truncate(off_t size)
{
    mutex.Enter();
    bool result = false;
    try
    {
        if (!CheckOpen())
        {
            mutex.Leave();
            return result;
        }
        result = ::ftruncate(fd, size) == 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
    mutex.Leave();
    return result;
}

bool FileIOHandler::Exists(const std::string &path)
{
    return ::access(path.c_str(), F_OK) == 0;
}

bool FileIOHandler::Remove(const std::string &path)
{
    return ::unlink(path.c_str()) == 0;
}

bool FileIOHandler::CreateFolder(const std::string &path)
{
    if (path.empty()) return false;

    // 检查目录是否已存在
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode); // 存在且是目录
    }

    // 递归创建目录
    size_t pos = 0;
    std::string dir;
    while ((pos = path.find_first_of('/', pos + 1)) != std::string::npos) {
        dir = path.substr(0, pos);
        if (dir.empty()) continue; // 跳过根目录
        if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
            return false; // 创建失败
        }
    }
    // 创建最后一层目录
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;

}

bool FileIOHandler::CheckOpen() const
{
    if (fd == -1)
    {
        return false;
        errno = EBADF;
    }
    return true;
}