#ifndef incl_QtGUI_h
#define incl_QtGUI_h

#include "StableHeaders.h"
#include "Foundation.h"
#include "UICanvas.h"

#include <QtGui>

#include "CommunicationManager.h"
#include "Connection.h"
#include "TextChatSession.h"
#include "TextChatSessionRequest.h"
#include "User.h"
#include "Contact.h"

using namespace TpQt4Communication;
using namespace QtUI;

namespace CommunicationUI
{
	// QtGUI CLASS

	class QtGUI : public QObject
	{
	
	Q_OBJECT
	
	MODULE_LOGGING_FUNCTIONS
	static const std::string NameStatic() { return "CommunicationModule::QtGUI"; } // for logging functionality

	public:
		QtGUI(Foundation::Framework *framework);
		~QtGUI(void);

	public slots:
		void setWindowSize(QSize &size);

	private:
		Foundation::Framework *framework_;
		boost::shared_ptr<UICanvas> canvas_;

	};

	// UIContainer CLASS

	class UIContainer : public QWidget
	{
	
	Q_OBJECT

	friend class Conversation;
	MODULE_LOGGING_FUNCTIONS
	static const std::string NameStatic() { return "CommunicationModule::UIController"; } // for logging functionality

	public:
		UIContainer(QWidget *parent);
		~UIContainer(void);

	public slots:
		void connectToServer(QString server, int port, QString username, QString password);
		void managerReady();
		void connectionEstablished();
		void connectionFailed(QString &reason);

		void statusChanged(const QString &newStatus);
		void statusMessageChanged();
		void startNewChat(QListWidgetItem *clickedItem);
		void newChatSessionRequest(TextChatSessionRequest *);
		void sendNewChatMessage();
		void closeTab(int index);

	private:
		void loadUserInterface(bool connected);
		void loadConnectedUserData(User *userData);
		void setAllEnabled(bool enabled);

		QString currentMessage;
		QWidget *loginWidget_;
		QWidget *chatWidget_;
		QTabWidget *tabWidgetCoversations_;
		QListWidget *listWidgetFriends_;
		QPushButton *buttonSendMessage_;
		QLineEdit *lineEditMessage_;
		QLabel *labelUsername_;
		QLineEdit *lineEditStatus_;
		QComboBox *comboBoxStatus_;
		QLabel *connectionStatus_;

		
		Credentials credentials;
		CommunicationManager* commManager_;
		Connection* im_connection_;

	signals:
		void resized(QSize &);

	};

	// LOGIN CLASS

	class Login : public QWidget
	{
	
	Q_OBJECT

	friend class UIContainer;

	public:
		Login(QWidget *parent, QString &message);
		~Login(void);

	public slots:
		void checkInput(bool clicked);

	private:
		void initWidget(QString &message);
		void connectSignals();

		QWidget *internalWidget_;
		QLabel *labelStatus;
		QLineEdit *textEditServer_;
		QLineEdit *textEditPort_;
		QLineEdit *textEditUsername_;
		QLineEdit *textEditPassword_;
		QPushButton *buttonConnect_;
		QPushButton *buttonCancel_;

	signals:
		void userdataSet(QString, int, QString, QString);

	};

	// CONVERSATION CLASS

	class Conversation : public QWidget
	{
	
	Q_OBJECT

	friend class UIContainer;

	MODULE_LOGGING_FUNCTIONS
		static const std::string NameStatic() { return "CommunicationModule::Conversation"; } // for logging functionality

	public:
		Conversation(QWidget *parent, TextChatSessionPtr chatSession, Contact *contact); // Add as inparam also the "conversation object" from mattiku
		~Conversation(void);

		void onMessageSent(QString message);
		void showMessageHistory(MessageVector messageHistory);

	private:
		void initWidget();
		void connectSignals();
		QString generateTimeStamp();
		void appendLineToConversation(QString line);
		void appendHTMLToConversation(QString html);

		QWidget *internalWidget_;
		QPlainTextEdit *textEditChat_;

		TextChatSessionPtr chatSession_;
		Contact *contact_;

	private	slots:
		void onMessageReceived(Message &message);
		void contactStateChanged();

	};

	// CUSTOM QListWidgetItem CLASS

	class ContactListItem : public QObject, QListWidgetItem
	{

	Q_OBJECT

	friend class UIContainer;

	public:
		ContactListItem(QString &name, QString &status, QString &statusmessage, Contact *contact);
		~ContactListItem(void);

	public slots:
		void statusChanged();

	private:
		void updateItem();

		QString name_;
		QString status_;
		QString statusmessage_;
		Contact *contact_;

	};

} //end if namespace: CommunicationUI

#endif // incl_QtGUI_h