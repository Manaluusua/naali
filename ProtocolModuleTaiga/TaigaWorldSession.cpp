// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"

#include "TaigaWorldSession.h"
#include "ProtocolModuleTaiga.h"

#include "Framework.h"
#include "ModuleManager.h"

namespace TaigaProtocol
{
    TaigaWorldSession::TaigaWorldSession(Foundation::Framework *framework)
        : framework_(framework), credentials_(0), serverEntryPointUrl_(0)
    {
        networkTaiga_ = framework_->GetModuleManager()->GetModule<TaigaProtocol::ProtocolModuleTaiga>();
    }

    TaigaWorldSession::~TaigaWorldSession()
    {
    }

    bool TaigaWorldSession::StartSession(ProtocolUtilities::LoginCredentialsInterface *credentials, QUrl *serverEntryPointUrl)
    {
        bool success = false;
        ProtocolUtilities::TaigaCredentials *testCredentials = dynamic_cast<ProtocolUtilities::TaigaCredentials *>(credentials);
        if (testCredentials)
        {
            // Set Url and Credentials
            serverEntryPointUrl_ = ValidateUrl(serverEntryPointUrl->toString(), WorldSessionInterface::OpenSimServer);
            serverEntryPointUrl = &serverEntryPointUrl_;
            credentials_ = testCredentials;

            success = LoginToServer(serverEntryPointUrl_.toString(), QString::number(serverEntryPointUrl_.port()),
                credentials_->GetIdentity(), GetConnectionThreadState());
        }
        else
        {
            ProtocolModuleTaiga::LogInfo("Invalid credential type, must be TaigaCredentials for TaigaWorldSession");
            success = false;
        }

        return success;
    }

    bool TaigaWorldSession::LoginToServer(const QString& address, const QString& port, const QString& identityUrl,
        ProtocolUtilities::ConnectionThreadState *thread_state)
    {
        // Get ProtocolModuleTaiga
        boost::shared_ptr<TaigaProtocol::ProtocolModuleTaiga> spTaiga = networkTaiga_.lock();

        if (spTaiga.get())
        {
            spTaiga->GetLoginWorker()->PrepareTaigaLogin(address, port, thread_state);
            spTaiga->SetAuthenticationType(ProtocolUtilities::AT_Taiga);
            spTaiga->SetIdentityUrl(identityUrl);
            spTaiga->SetHostUrl(address + port);
            // Start the thread.
            boost::thread(boost::ref( *spTaiga->GetLoginWorker() ));
        }
        else
        {
            ProtocolModuleTaiga::LogInfo("Could not lock ProtocolModuleTaiga");
            return false;
        }

        return true;
    }

    QUrl TaigaWorldSession::ValidateUrl(const QString &urlString, const UrlType urlType)
    {
        QUrl returnUrl(urlString);
        if (returnUrl.isValid())
        {
            if (returnUrl.port() == -1)
                returnUrl.setPort(80);
            return returnUrl;
        }
        else
        {
            ProtocolModuleTaiga::LogInfo("Invalid Taiga connection url");
            return QUrl();
        }
    }

    ProtocolUtilities::LoginCredentialsInterface* TaigaWorldSession::GetCredentials() const
    {
        return credentials_;
    }

    QUrl TaigaWorldSession::GetServerEntryPointUrl() const
    {
        return serverEntryPointUrl_;
    }
    
    void TaigaWorldSession::GetWorldStream() const
    {

    }

    void TaigaWorldSession::SetCredentials(ProtocolUtilities::LoginCredentialsInterface *newCredentials)
    {
        ProtocolUtilities::TaigaCredentials *testCredentials = dynamic_cast<ProtocolUtilities::TaigaCredentials *>(newCredentials);
        if (testCredentials)
            credentials_ = testCredentials;
        else
            ProtocolModuleTaiga::LogInfo("Could not set credentials, invalid type. Must be Taiga Credentials for TaigaWorldSession");
    }

    void TaigaWorldSession::SetServerEntryPointUrl(const QUrl &newUrl)
    {
        serverEntryPointUrl_ = newUrl;
    }
}
