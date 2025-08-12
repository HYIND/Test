#pragma once

#include "stdafx.h"

// 文件状态枚举
enum class FileStoreStatus
{
    NOT_UPLOADED = 0,
    UPLOADING = 1,
    COMPLETED = 2
};

// 文件记录结构体
struct FileRecord
{
    std::string fileid;
    FileStoreStatus status;
    std::string md5;
    std::string path;
    uint64_t filesize;

    FileRecord(std::string id = "", FileStoreStatus s = FileStoreStatus::NOT_UPLOADED,
               std::string m = "",
               uint64_t size = 0,
               std::string p = "");
};

class FileRecordStore
{
public:
    static FileRecordStore *Instance();

private:
    explicit FileRecordStore();

public:
    ~FileRecordStore() = default;

    FileRecordStore(const FileRecordStore &) = delete;
    FileRecordStore &operator=(const FileRecordStore &) = delete;

    // 文件记录操作接口
    bool addFileRecord(std::string fileId,
                       std::string md5 = "",
                       uint64_t filesize = 0);

    void deleteFileRecord(const std::string &fileId);
    bool updateFileRecordStatus(const std::string &fileId, FileStoreStatus newStatus);
    bool updateFileRecordMd5(const std::string &fileId, const std::string &newMd5);

    // 查询接口
    bool getRecord(const std::string &fileId, FileRecord &record);
    std::string getPath(const std::string &fileId);
    FileStoreStatus getStatus(const std::string &fileId);
    std::string getMd5(const std::string &fileId);

private:
    void loadRecords();
    void saveRecords();

private:
    std::string filePath_;
    SafeMap<std::string, FileRecord *> records_;
};

#define FILERECORDSTORE FileRecordStore::Instance()