#pragma once

#include "stdafx.h"
#include "FileTransferUploadTask.h"
#include "FileTransferDownLoadTask.h"
#include "LoginUserManager.h"

using namespace std;

struct FileTransTaskContent
{
    string fileid;
    FileTransferTask *task = nullptr;
    BaseNetWorkSession *session = nullptr;

    FileTransTaskContent(const string &id, FileTransferTask *t, BaseNetWorkSession *s);
    ~FileTransTaskContent();
};

class FileTransManager
{
public:
    static FileTransManager *Instance();

public:
    // 处理消息的入口
    bool ProcessMsg(BaseNetWorkSession *session, const string &ip, const uint16_t port, const json &js);
    bool DistributeMsg(BaseNetWorkSession *session, const json &js);

protected:
    void AckTaskReq(BaseNetWorkSession *session, const json &js);

public:
    void OnUploadFinish(FileTransferUploadTask *task);
    void OnUploadError(FileTransferUploadTask *task);
    void OnUploadInterrupt(FileTransferUploadTask *task);
    void OnUploadProgress(FileTransferUploadTask *task, uint32_t progress);

    void OnDownloadFinish(FileTransferDownLoadTask *task);
    void OnDownloadError(FileTransferDownLoadTask *task);
    void OnDownloadInterrupt(FileTransferDownLoadTask *task);
    void OnDownloadProgress(FileTransferDownLoadTask *task, uint32_t progress);

    void SetLoginUserManager(LoginUserManager *m);

private:
    FileTransManager();
    bool AddUploadTask(const string &fileid, const string &taskid, const string &filepath, BaseNetWorkSession *session);
    bool AddDownloadTask(const string &fileid, const string &taskid, const string &filepath, uint32_t filesize, BaseNetWorkSession *session);
    void DeleteTask(const string &taskid);

private:
    SafeMap<string, FileTransTaskContent *> m_tasks; // taskid->content
    LoginUserManager *HandleLoginUser = nullptr;
};

#define FILETRANSMANAGER FileTransManager::Instance()