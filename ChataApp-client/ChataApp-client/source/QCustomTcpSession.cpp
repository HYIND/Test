#include "QCustomTcpSession.h"
#include <QEventLoop>
#include <QTimer>

const char CustomProtocolTryToken[] = "alskdjfhg";      // 客户端发起连接发送的请求Token
const char CustomProtocolConfirmToken[] = "qpwoeiruty"; // 服务端接收到请求后返回的确认Token

const char HeartBuffer[] = "23388990";
bool IsHeartBeat(const Buffer &Buffer)
{
    static int HeartSize = sizeof(HeartBuffer) - 1;
    return (Buffer.Length() == HeartSize && 0 == strncmp(HeartBuffer, Buffer.Byte(), HeartSize));
}

struct CustomTcpMsgHeader
{
    int seq = 0;
    int ack = -1;
    int length = 0;
};

using Base = BaseNetWorkSession;

// 处理包内容，从包的Buffer中取出seq、ack，使用者获取的Buffer中不应包含相应字段
void ShiftPakHeader(CustomPackage *pak)
{
    if (!pak || pak->buffer.Length() < 8)
        return;
    pak->buffer.Read(&(pak->seq), 4);
    pak->buffer.Read(&(pak->ack), 4);
    pak->buffer.Shift(12);
}
// 处理流内容，在流的头部添加seq和ack字段
void AddPakHeader(Buffer *buf, CustomTcpMsgHeader header)
{
    if (!buf)
        return;
    buf->Unshift(&header, 12);
    buf->Seek(buf->Postion() + 12);
}

bool AnalysisDataPackage(Buffer *buf, CustomPackage *outPak)
{
    if (!buf || !outPak)
    {
        std::cout << "CustomTcpSession::AnalysisDataPackage null buf or null outPak!\n";
        return false;
    }

    if (buf->Remaind() < sizeof(CustomTcpMsgHeader))
    {
        return false;
    }

    int oriPos = buf->Postion();

    CustomTcpMsgHeader header;
    buf->Read(&header, sizeof(CustomTcpMsgHeader));

    if (buf->Remaind() < header.length)
    {
        buf->Seek(oriPos);
        return false;
    }

    outPak->seq = header.seq;
    outPak->ack = header.ack;
    outPak->buffer.Append(*buf, header.length);

    return true;
}

CustomTcpSession::CustomTcpSession(TCPClient *client)
{
    cachePak = new CustomPackage();
    if (client)
        BaseClient = client;
    else
        BaseClient = new TCPClient();
}

CustomTcpSession::~CustomTcpSession()
{
    Release();
    SAFE_DELETE(BaseClient);
    SAFE_DELETE(cachePak);
}

bool CustomTcpSession::Connect(const QString &IP, quint16 Port)
{
    return Base::Connect(IP, Port);
}

bool CustomTcpSession::Release()
{
    bool result = Base::Release();

    CustomPackage *pak = nullptr;
    while (_RecvPaks.dequeue(pak))
        SAFE_DELETE(pak);
    while (_SendPaks.dequeue(pak))
        SAFE_DELETE(pak);

    _AwaitMap.EnsureCall(
        [&](std::map<int, AwaitTask *> &map) -> void
        {
            for (auto it = map.begin(); it != map.end(); it++)
            {
                SAFE_DELETE(it->second);
            }
            map.clear();
        });

    cacheBuffer.Release();
    if (cachePak)
    {
        cachePak->seq = 0;
        cachePak->ack = -1;
        cachePak->buffer.Release();
    }

    emit handshakeCompleted();
    // _tryHandshakeCV.notify_all();

    return true;
}

bool CustomTcpSession::AsyncSend(const Buffer &buffer)
{
    return Send(buffer, -1);
}

bool CustomTcpSession::AwaitSend(const Buffer &buffer, Buffer &response)
{
    try
    {
        if (!buffer.Data() || buffer.Length() < 0)
            return true;

        bool result = false;

        int seq = this->seq++;

        AwaitTask *task = new AwaitTask();
        task->respsonse = &response;
        task->seq = seq;
        if (!_AwaitMap.Insert(task->seq, task))
            return false;

        Buffer buf(buffer);
        AddPakHeader(&buf, {seq, -1, buffer.Length()});
        if (BaseClient->Send(buf)) // 发送
        {
            std::unique_lock<std::mutex> awaitlck(task->_mtx);
            task->status = 1;
            // printf("wait_for , task->seq:%d\n", task->seq);
            result = task->_cv.wait_for(awaitlck, std::chrono::seconds(8)) != std::cv_status::timeout; // 等待返回并超时检查
        }

        task->status = 0;
        _AwaitMap.EnsureCall(
            [&](std::map<int, AwaitTask *> &map) -> void
            {
                auto it = map.find(task->seq);
                if (it != map.end())
                    map.erase(it);
                delete task;
            });
        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }
}

bool CustomTcpSession::OnSessionClose()
{
    auto callback = _callbackSessionClose;
    Release();
    if (callback)
        callback(this);
    return true;
}

bool CustomTcpSession::OnRecvData(Buffer *buffer)
{
    if (!isHandshakeComplete)
    {
        if (CheckHandshakeConfirmMsg(*buffer) != CheckHandshakeStatus::Success)
            return false;
    }

    if (buffer->Remaind() > 0)
        cacheBuffer.Append(*buffer);

    while (cacheBuffer.Remaind() > 0)
    {
        // 解析数据包
        int result = AnalysisDataPackage(&cacheBuffer, cachePak);
        if (result)
        {
            // 数据包解析成功，获得完整Package
            cacheBuffer.Shift(cacheBuffer.Postion());

            cachePak->buffer.Seek(0);
            CustomPackage *newPak = cachePak;
            cachePak = new CustomPackage();

            std::lock_guard<SpinLock> lock(_ProcessLock);
            ProcessPakage(newPak);
        }
        else
        {
            break;
        }
        // else
        // {
        //     std::cout << "AnalysisDataPackage Error!\n";
        //     Release();
        // }
    }
    return true;
}

bool CustomTcpSession::Send(const Buffer &buffer, int ack)
{
    try
    {
        if (!buffer.Data() || buffer.Length() < 0)
            return true;

        int seq = this->seq++;
        Buffer buf(buffer);
        AddPakHeader(&buf, {seq, ack, buffer.Length()});
        return BaseClient->Send(buf);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }
}

void CustomTcpSession::ProcessPakage(CustomPackage *newPak)
{

    if (newPak)
    {
        if (newPak->ack != -1)
        {
            AwaitTask *task = nullptr;
            if (_AwaitMap.Find(newPak->ack, task))
            {
                if (task->respsonse)
                    task->respsonse->CopyFromBuf(newPak->buffer);

                int count = 0;
                while (!task->status == -1 && count < 5)
                {
                    count++;
                    Sleep(10);
                }
                Sleep(5);
                // printf("notify_all , pak->ack:%d\n", pak->ack);
                task->_cv.notify_all();
            }
            SAFE_DELETE(newPak);
        }
        else
        {
            if (_RecvPaks.size() > 300)
            {
                CustomPackage *pak = nullptr;
                if (_RecvPaks.dequeue(pak))
                    SAFE_DELETE(pak);
            }
            _RecvPaks.enqueue(newPak);
        }
    }

    int count = 10;
    CustomPackage *pak = nullptr;
    while (_RecvPaks.front(pak) && count > 0)
    {
        if (_callbackRecvData)
        {
            Buffer resposne;
            _callbackRecvData(this, &pak->buffer, &resposne);
            if (resposne.Length() > 0)
                Send(resposne, pak->seq);
            resposne.Release();
            _RecvPaks.dequeue(pak);
            SAFE_DELETE(pak);
        }
        count--;
    }
}

void CustomTcpSession::OnBindRecvDataCallBack()
{
    if (_ProcessLock.trylock())
    {
        try
        {
            ProcessPakage();
        }
        catch (const std::exception &e)
        {
            _ProcessLock.unlock();
            std::cerr << e.what() << '\n';
        }
        _ProcessLock.unlock();
    }
}

void CustomTcpSession::OnBindSessionCloseCallBack()
{
}

bool CustomTcpSession::TryHandshake(uint32_t timeOutMs)
{
    // std::unique_lock<std::mutex> lock(_tryHandshakeMutex);

    Buffer token(CustomProtocolTryToken, sizeof(CustomProtocolTryToken) - 1);
    if (!BaseClient->Send(token))
        return false;

    // 2. 创建局部事件循环和超时控制器
    QEventLoop localLoop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    // 3. 超时或握手完成时退出事件循环
    QObject::connect(&timeoutTimer, &QTimer::timeout, &localLoop,
                     &QEventLoop::quit);
    QObject::connect(this, &CustomTcpSession::handshakeCompleted, &localLoop,
                     &QEventLoop::quit);

    // 4. 启动超时计时器
    timeoutTimer.start(timeOutMs);

    // 5. 进入局部事件循环（非阻塞式等待）
    //    此时主事件循环仍在运行，可以处理 readyRead 等信号
    localLoop.exec();

    return isHandshakeComplete;

    // 等待握手结果，超时或者被唤醒时触发，返回isHandshakeComplete
    // return _tryHandshakeCV.wait_for(lock,
    //                                 std::chrono::milliseconds(timeOutMs),
    //                                 [this]
    //                                 { return isHandshakeComplete; });
}

CheckHandshakeStatus CustomTcpSession::CheckHandshakeTryMsg(Buffer &buffer)
{
    if (isHandshakeComplete)
    {
        return CheckHandshakeStatus::None;
    }

    cacheBuffer.WriteFromOtherBufferPos(buffer);

    int tokenLength = sizeof(CustomProtocolTryToken) - 1;
    if (cacheBuffer.Length() < tokenLength)
        return CheckHandshakeStatus::BufferAgain;

    if (strncmp(cacheBuffer.Byte(), CustomProtocolTryToken, tokenLength) != 0)
        return CheckHandshakeStatus::Fail;

    Buffer rsp(CustomProtocolConfirmToken, sizeof(CustomProtocolConfirmToken) - 1);
    if (!BaseClient->Send(rsp))
        return CheckHandshakeStatus::Fail;

    cacheBuffer.Shift(tokenLength);
    cacheBuffer.Seek(0);

    isHandshakeComplete = true;
    return CheckHandshakeStatus::Success;
}

CheckHandshakeStatus CustomTcpSession::CheckHandshakeConfirmMsg(Buffer &buffer)
{
    if (isHandshakeComplete)
    {
        return CheckHandshakeStatus::None;
    }

    cacheBuffer.WriteFromOtherBufferPos(buffer);

    int tokenLength = sizeof(CustomProtocolConfirmToken) - 1;
    if (cacheBuffer.Length() < tokenLength)
        return CheckHandshakeStatus::BufferAgain;

    if (strncmp(cacheBuffer.Byte(), CustomProtocolConfirmToken, tokenLength) != 0)
        return CheckHandshakeStatus::Fail;

    cacheBuffer.Shift(tokenLength);
    cacheBuffer.Seek(0);

    isHandshakeComplete = true;

    emit handshakeCompleted();
    // _tryHandshakeCV.notify_all();
    return CheckHandshakeStatus::Success;
}

TCPClient *CustomTcpSession::GetBaseClient()
{
    return BaseClient;
}
