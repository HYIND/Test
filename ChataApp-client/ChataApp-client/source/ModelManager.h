#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <ChatItemListModel.h>
#include "ChatSessionModel.h"
#include "UserInfoModel.h"
#include "LoginModel.h"

class ModelManager:public QObject
{
    Q_OBJECT
    Q_PROPERTY(ChatItemListModel* chatitemlistmodel READ chatitemlistmodel CONSTANT)

public:
    static ModelManager* Instance();
    ChatItemListModel* chatitemlistmodel() const { return m_chatitemlistmodel; }
    ChatSessionModel* chatsessionmodel() const { return m_chatsessionmodel; }
    UserInfoModel* userinfomodel() const { return m_userinfomodel; }
    LoginModel* loginmodel() const { return m_loginmodel; }

private:
    explicit ModelManager(QObject* parent = nullptr);

private:
    ChatItemListModel* m_chatitemlistmodel;
    ChatSessionModel* m_chatsessionmodel;
    UserInfoModel* m_userinfomodel;
    LoginModel* m_loginmodel;
};

#define MODELMANAGER ModelManager::Instance()
#define CHATITEMMODEL ModelManager::Instance()->chatitemlistmodel()
#define SESSIONMODEL ModelManager::Instance()->chatsessionmodel()
#define USERINFOMODEL ModelManager::Instance()->userinfomodel()
#define LOGINMODEL ModelManager::Instance()->loginmodel()


#endif // MODELMANAGER_H
