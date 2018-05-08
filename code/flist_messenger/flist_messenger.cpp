/*
* Copyright (c) 2011, F-list.net
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this list of conditions and the
* following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
* following disclaimer in the documentation and/or other materials provided with the distribution.
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*/

#include "flist_messenger.h"
#include "flist_global.h"

#include <iostream>
#include <QString>
#include <QSplitter>
#include <QClipboard>
#include <QTimer>
#include <QDesktopServices>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QScrollArea>
#include <QStatusBar>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTextBrowser>
#include <QLineEdit>
#include <QScrollBar>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QIcon>
#include <QSettings>

#include "flist_character.h"
#include "flist_account.h"
#include "flist_server.h"
#include "flist_session.h"
#include "flist_message.h"
#include "flist_settings.h"
#include "flist_attentionsettingswidget.h"
#include "flist_channelpanel.h"
#include "flist_channeltab.h"
#include "flist_logtextbrowser.h"

#include "ui/characterinfodialog.h"
#include "ui/channellistdialog.h"
#include "ui/helpdialog.h"
#include "ui/aboutdialog.h"
#include "ui/makeroomdialog.h"
#include "ui/statusdialog.h"
#include "ui/friendsdialog.h"

QString flist_messenger::settingsPath = QSL("./settings.ini");

void flist_messenger::init()
{
	settingsPath = QApplication::applicationDirPath() + QSL("/settings.ini");
}

flist_messenger::flist_messenger(bool d)
{
	server = new FServer(this);
	account = server->addAccount();
	account->ui = this;
	doingWS = true;
	notificationsAreaMessageShown = false;
	console = nullptr;
	chatview = nullptr;
	debugging = d;
	disconnected = true;
	friendsDialog = nullptr;
	makeRoomDialog = nullptr;
	setStatusDialog = nullptr;
	ci_dialog = nullptr;
	recentCharMenu = nullptr;
	recentChannelMenu = nullptr;
	reportDialog = nullptr;
	helpDialog = nullptr;
	aboutDialog = nullptr;
	timeoutDialog = nullptr;
	settingsDialog = nullptr;
	trayIcon = nullptr;
	trayIconMenu = nullptr;
	channelSettingsDialog = nullptr;
	createTrayIcon();
	loadSettings();
	loginController = new FLoginController(fapi,account,this);
	setupLoginBox();
	cl_data = new FChannelListModel();
	cl_dialog = nullptr;
	selfStatus = QSL("online");

	FCharacter::initClass();
	FChannelPanel::initClass();
}

void flist_messenger::closeEvent(QCloseEvent *event)
{
	if (disconnected)
		quitApp();
	if (trayIcon->isVisible()) {
		if (!notificationsAreaMessageShown) {
			QString title(QSL("Still running~"));
			QString msg(QSL("F-chat Messenger is still running! "
			            "To close it completely, right click the tray icon and click \"Quit\"."));
			trayIcon->showMessage(title, msg, QSystemTrayIcon::Information, 10000);
			notificationsAreaMessageShown = true;
		}
		hide();
		event->ignore();
	}
}

void flist_messenger::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason) {
	case QSystemTrayIcon::Trigger:
	case QSystemTrayIcon::DoubleClick:
		showNormal();
		activateWindow();
		break;
	default:
		break;
	}
}

void flist_messenger::enterPressed()
{
	if (currentPanel == nullptr) return;

	QString input = plainTextEdit->toPlainText();
	if (currentPanel->getMode() == ChannelMode::Ads && !input.startsWith('/')) {
		btnSendAdvClicked();
	}
	else {
		parseInput();
	}
}

void flist_messenger::createTrayIcon()
{
	trayIconMenu = new QMenu(this);
	trayIconMenu->addAction(QSL("Minimize"), this, &flist_messenger::hide);
	trayIconMenu->addAction(QSL("Maximize"), this, &flist_messenger::showMaximized);
	trayIconMenu->addAction(QSL("Restore"), this, &flist_messenger::showNormal);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(QIcon(QSL(":/images/cross.png")), QSL("Quit"), this, &flist_messenger::quitApp);

	trayIcon = new QSystemTrayIcon(this);
	connect(trayIcon, &QSystemTrayIcon::activated, this, &flist_messenger::iconActivated);

	trayIcon->setContextMenu(trayIconMenu);
	trayIcon->setIcon(QIcon(QSL(":/images/icon.png")));
	trayIcon->setToolTip(QSL("F-chat Messenger"));
	trayIcon->setVisible(true);
}

flist_messenger::~flist_messenger()
{
	// TODO: Delete everything
	delete cl_dialog;
	delete cl_data;
}

void flist_messenger::printDebugInfo(std::string s)
{
	if (debugging) {
		std::cout << s << std::endl;
		if (console) {
			QString q = s.c_str();
			console->addLine(q.toHtmlEscaped(), true);
		}
	}
}

void flist_messenger::setupLoginBox()
{
	this->setWindowTitle(QSL("F-chat messenger - Login"));
	this->setWindowIcon(QIcon(QSL(":/images/icon.png")));

	loginWidget = new FLoginWindow(this);
	loginController->setWidget(loginWidget);
	this->setCentralWidget(loginWidget);

	this->resize(265, 100);
	centerOnScreen(this);
}

void flist_messenger::startConnect(QString charName)
{
	this->charName = charName;
	FSession *session = account->addSession(charName);
	session->autojoinchannels = defaultChannels;
	this->centralWidget()->deleteLater();

	setupRealUI();
	connect(session, &FSession::socketErrorSignal, this, &flist_messenger::socketError);
	
	connect(session, &FSession::notifyCharacterOnline, this, &flist_messenger::notifyCharacterOnline);
	connect(session, &FSession::notifyCharacterStatusUpdate, this, &flist_messenger::notifyCharacterStatusUpdate);
	connect(session, &FSession::notifyIgnoreAdd, this, &flist_messenger::notifyIgnoreRemove);
	connect(session, &FSession::notifyIgnoreRemove, this, &flist_messenger::notifyIgnoreRemove);
	
	session->connectSession();
}

void flist_messenger::destroyMenu()
{
	if (recentCharMenu )
		recentCharMenu->deleteLater();
}

void flist_messenger::destroyChanMenu()
{
	if (recentChannelMenu)
		recentChannelMenu->deleteLater();
}

void flist_messenger::setupReportDialog()
{
	reportDialog = new QDialog(this);
	re_vblOverview = new QVBoxLayout;
	re_hblButtons = new QHBoxLayout;
	re_lblWho = new QLabel(QSL("Reporting user:"));
	re_lblProblem = new QLabel(QSL("Describe your problem."));
	re_lblInstructions = new QLabel(QSL("Note that channel mods do <b>not</b> receive staff alerts. If you are reporting a problem in a private channel, please contact the channel's moderator instead. Furthermore, if you are reporting something that is not against the rules, we will not be able to help you. If you are reporting private messages, try using /ignore first."));
	re_lblInstructions->setWordWrap(true);
	re_leWho = new QLineEdit();
	re_teProblem = new QTextEdit();
	re_teProblem->setObjectName(QSL("reportinput"));
	re_btnSubmit = new QPushButton(QIcon(QSL(":/images/tick.png")), QSL("Submit"));
	re_btnCancel = new QPushButton(QIcon(QSL(":/images/cross.png")), QSL("Cancel"));

	reportDialog->setLayout(re_vblOverview);
	re_vblOverview->addWidget(re_lblProblem);
	re_vblOverview->addWidget(re_teProblem);
	re_vblOverview->addWidget(re_lblWho);
	re_vblOverview->addWidget(re_leWho);
	re_vblOverview->addWidget(re_lblInstructions);
	re_vblOverview->addLayout(re_hblButtons);
	re_hblButtons->addStretch();
	re_hblButtons->addWidget(re_btnSubmit);
	re_hblButtons->addWidget(re_btnCancel);

	connect(re_btnSubmit, &QPushButton::clicked, this, &flist_messenger::re_btnSubmitClicked);
	connect(re_btnCancel, &QPushButton::clicked, this, &flist_messenger::re_btnCancelClicked);
}

void flist_messenger::helpDialogRequested()
{
	if (helpDialog == nullptr || helpDialog->parent() != this) {
		helpDialog = new FHelpDialog(this);
	}
	helpDialog->show();
}
void flist_messenger::channelSettingsDialogRequested()
{
	// This is one that always needs to be setup, because its contents may vary.
	if (setupChannelSettingsDialog())
		channelSettingsDialog->show();
}
bool flist_messenger::setupChannelSettingsDialog()
{
	FChannelPanel *channelpanel = channelList.value(tb_recent_name);
	if (!channelpanel) {
		debugMessage(QSL("Tried to setup a channel settings dialog for the panel '%1', but the panel does not exist.").arg(tb_recent_name));
		return false;
	}
	FSession *session = account->getSession(channelpanel->getSessionID());

	bool is_pm = channelpanel->type() == FChannel::CHANTYPE_PM;
	//todo: Allow to work with offline characters.
	//if (is_pm && !session->isCharacterOnline(channelpanel->recipient())) { // Recipient is offline
	//	return false;
	//}

	cs_chanCurrent = channelpanel;

	channelSettingsDialog = new QDialog(this);
	channelSettingsDialog->setWindowIcon(channelpanel->pushButton->icon());
	FCharacter* character = nullptr;
	if (is_pm) {
		character = session->getCharacter(channelpanel->recipient());
		if (character) {
			channelSettingsDialog->setWindowTitle(character->name());
			cs_qsPlainDescription = character->statusMsg();
		}
		else {
			channelSettingsDialog->setWindowTitle(channelpanel->recipient());
			cs_qsPlainDescription = QSL("");
		}
	}
	else {
		channelSettingsDialog->setWindowTitle(channelpanel->title());
		cs_qsPlainDescription = channelpanel->description();
	}

	QTabWidget *twOverview = new QTabWidget;
	QGroupBox* gbNotifications = new QGroupBox(QSL("Notifications"));
	QVBoxLayout* vblNotifications = new QVBoxLayout;

	if (is_pm) {
		cs_attentionsettings = new FAttentionSettingsWidget(channelpanel->recipient(), channelpanel->recipient(), channelpanel->type());
	}
	else {
		cs_attentionsettings = new FAttentionSettingsWidget(channelpanel->getChannelName(), channelpanel->title(), channelpanel->type());
	}

	cs_vblOverview = new QVBoxLayout;
	cs_gbDescription = new QGroupBox(is_pm ? QSL("Status") : QSL("Description"));
	cs_vblDescription = new QVBoxLayout;
	cs_tbDescription = new QTextBrowser;
	cs_tbDescription->setHtml(bbparser.parse(cs_qsPlainDescription));
	cs_tbDescription->setReadOnly(true);
	cs_teDescription = new QTextEdit;
	cs_teDescription->setPlainText(cs_qsPlainDescription);
	cs_teDescription->hide();
	if (!is_pm) {
		cs_chbEditDescription = new QCheckBox(QSL("Editable mode (OPs only)"));
		if (!(channelpanel->isOp(session->getCharacter(session->character)) || session->isCharacterOperator(session->character))) {
			cs_chbEditDescription->setEnabled(false);
		}
	}
	cs_hblButtons = new QHBoxLayout;
	cs_btnCancel = new QPushButton(QIcon ( QSL(":/images/cross.png") ), QSL("Cancel"));
	cs_btnSave = new QPushButton(QIcon ( QSL(":/images/tick.png") ), QSL("Save"));

	channelSettingsDialog->setLayout(cs_vblOverview);
	cs_vblOverview->addWidget(twOverview);
	twOverview->addTab(cs_gbDescription, QSL("General"));
	cs_gbDescription->setLayout(cs_vblDescription);
	if (is_pm) {
		QLabel* lblStatus = new QLabel(character ? character->statusString() : QSL("Offline"));
		cs_vblDescription->addWidget(lblStatus);
	}
	cs_vblDescription->addWidget(cs_teDescription);
	cs_vblDescription->addWidget(cs_tbDescription);
	if (!is_pm) {
		cs_vblDescription->addWidget(cs_chbEditDescription);
	}

	twOverview->addTab(gbNotifications, QSL("Notifications"));
	gbNotifications->setLayout(vblNotifications);
	vblNotifications->addWidget(cs_attentionsettings);
	
	
	cs_vblOverview->addLayout(cs_hblButtons);
	cs_hblButtons->addStretch();
	cs_hblButtons->addWidget(cs_btnSave);
	cs_hblButtons->addWidget(cs_btnCancel);

	if (!is_pm) {
		connect(cs_chbEditDescription, &QCheckBox::toggled, this, &flist_messenger::cs_chbEditDescriptionToggled);
	}

	cs_attentionsettings->loadSettings();

	connect(cs_btnCancel, &QPushButton::clicked, this, &flist_messenger::cs_btnCancelClicked);
	connect(cs_btnSave, &QPushButton::clicked, this, &flist_messenger::cs_btnSaveClicked);
	connect(cs_tbDescription, &QTextBrowser::anchorClicked, this, &flist_messenger::anchorClicked);

	return true;
}

void flist_messenger::cs_chbEditDescriptionToggled(bool state)
{
	if (state) {
		// display plain text BBCode
		cs_teDescription->setPlainText(cs_qsPlainDescription);
		cs_teDescription->show();
		cs_tbDescription->hide();
	}
	else {
		// display html
		cs_qsPlainDescription = cs_teDescription->toPlainText();
		cs_tbDescription->setHtml(bbparser.parse(cs_qsPlainDescription));
		cs_tbDescription->show();
		cs_teDescription->hide();
	}
}

void flist_messenger::cs_btnCancelClicked()
{
	cs_qsPlainDescription = QSLE;
	cs_chanCurrent = nullptr;
	channelSettingsDialog->deleteLater();
}
void flist_messenger::cs_btnSaveClicked()
{
	if (cs_qsPlainDescription != cs_chanCurrent->description())
	{
		std::cout << "Editing description." << std::endl;
		// Update description
		account->getSessionByCharacter(charName)->setChannelDescription(cs_chanCurrent->getChannelName(), cs_qsPlainDescription);
	}
	//Save settings to the ini file.
	cs_attentionsettings->saveSettings();
	//And reload those settings into the channel panel.
	cs_chanCurrent->loadSettings();

	cs_qsPlainDescription = QSLE;
	cs_chanCurrent = nullptr;
	channelSettingsDialog->deleteLater();
}

void flist_messenger::re_btnSubmitClicked()
{
	submitReport();
	reportDialog->hide();
}

void flist_messenger::submitReport()
{
	//todo: fix this
	QUrl lurl = QSL("https://www.f-list.net/json/getApiTicket.json?secure=no&account=%0&password=%1")
	        .arg(account->getUserName(), account->getPassword());
	lreply = qnam.get ( QNetworkRequest ( lurl ) );
	//todo: fix this!
	lreply->ignoreSslErrors();

	connect(lreply, &QNetworkReply::finished, this, &flist_messenger::reportTicketFinished);
}

void flist_messenger::handleReportFinished()
{
	if (lreply->error() != QNetworkReply::NoError ) {
		auto message = QSL("Response error during sending of report of type %0")
		        .arg(lreply->error());
		QMessageBox::critical ( this, QSL("FATAL ERROR DURING TICKET RETRIEVAL!"), message );
		return;
	}
	else {
		QByteArray respbytes = lreply->readAll();
		lreply->deleteLater();
		std::string response ( respbytes.begin(), respbytes.end() );
		JSONNode respnode = libJSON::parse ( response );
		JSONNode childnode = respnode.at ( "error" );
		if (childnode.as_string() != "" ) {
			std::string message = "Error from server: " + childnode.as_string();
			QMessageBox::critical ( this, QSL("Error"), message.c_str() );
			return;
		}
		else {
			childnode = respnode.at ( "log_id" );
			std::string logid = childnode.as_string();
			QString problem = re_teProblem->toPlainText().toHtmlEscaped();
			QString who = re_leWho->text().toHtmlEscaped();
			if (who.trimmed().isEmpty()) who = QSL("None");
			QString report = QSL("Current Tab/Channel: %0 | Reporting User: %1 | %2").arg(currentPanel->title(), who, problem);
			FSession *session = account->getSession(charName);
			session->sendSubmitStaffReport(charName, logid, report);
			reportDialog->hide();
			re_leWho->clear();
			re_teProblem->clear();
			QString message = QSL("Your report was uploaded");
			QMessageBox::information ( this, QSL("Success."), message );
		}
	}
}

void flist_messenger::reportTicketFinished()
{
	if (lreply->error() != QNetworkReply::NoError ) {
		auto message = QSL("Response error during fetching of ticket of type %0")
		        .arg(lreply->error());
		QMessageBox::critical( this, QSL("FATAL ERROR DURING TICKET RETRIEVAL!"), message );
		return;
	}
	QByteArray respbytes = lreply->readAll();
	lreply->deleteLater();
	std::string response(respbytes.begin(), respbytes.end());
	JSONNode respnode = libJSON::parse ( response );
	JSONNode childnode = respnode.at ( "error" );
	if (childnode.as_string() != "") {
		std::string message = "Error from server: " + childnode.as_string();
		QMessageBox::critical ( this, QSL("Error"), message.c_str() );
		return;
	}
	JSONNode subnode = respnode.at ( "ticket" );
	account->ticket = subnode.as_string().c_str();
	QString lurl = QSL("https://www.f-list.net/json/api/report-submit.php?account=%0&character=%1&ticket=%2")
	        .arg(account->getUserName(), charName, account->ticket);
	std::cout << lurl.toStdString() << std::endl;
	QByteArray postData;
	JSONNode* lognodes;
	lognodes = currentPanel->toJSON();
	std::string toWrite;
	toWrite = lognodes->write();
	QNetworkRequest request(lurl);
	fix_broken_escaped_apos(toWrite);
	lparam = QUrlQuery();
	lparam.addQueryItem(QSL("log"), toWrite.c_str());
	postData = lparam.query(QUrl::FullyEncoded).toUtf8();
	lreply = qnam.post ( request, postData );
	//todo: fix this!
	lreply->ignoreSslErrors();

	connect(lreply, &QNetworkReply::finished, this, &flist_messenger::handleReportFinished);
	delete lognodes;
}

void flist_messenger::re_btnCancelClicked()
{
	reportDialog->hide();
}

void flist_messenger::se_btnSubmitClicked()
{
	//se_helpdesk = se_chbHelpdesk->isChecked();
	settings->setShowJoinLeaveMessage(se_chbLeaveJoin->isChecked());
	settings->setPlaySounds(!se_chbMute->isChecked());
	settings->setLogChat(se_chbEnableChatLogs->isChecked());
	settings->setShowOnlineOfflineMessage(se_chbOnlineOffline->isChecked());

	se_attentionsettings->saveSettings();
	saveSettings();
	loadSettings();
	settingsDialog->hide();
}

void flist_messenger::se_btnCancelClicked()
{
	settingsDialog->hide();
}

void flist_messenger::closeChannelPanel(QString panelname)
{
	FChannelPanel *channelpanel = channelList.value(panelname);
	if (!channelpanel) {
		debugMessage(QSL("[BUG] Told to close channel panel '%1', but the panel does not exist.'").arg(panelname));
		return;
	}
	if (channelpanel == console) {
		debugMessage(QSL("[BUG] Was told to close the chanel panel for the console! Channel panel '%1'.").arg(panelname));
		return;
	}
	if (channelpanel->type() == FChannel::CHANTYPE_PM) {
		channelpanel->setTyping(TYPING_STATUS_CLEAR);
	}
	else {
		channelpanel->emptyCharList();
	}
	channelpanel->setActive(false);
	channelpanel->pushButton->setVisible(false);
	if (channelpanel == currentPanel) {
		QString c = QSL("CONSOLE");
		switchTab(c);
	}
}

/**
Close a channel panel and if it has an associated channel, leave the channel too.
 */
void flist_messenger::leaveChannelPanel(QString panelname)
{
	FChannelPanel *channelpanel = channelList.value(panelname);
	if (!channelpanel) {
		debugMessage(QSL("[BUG] Told to leave channel panel '%1', but the panel does not exist.'").arg(panelname));
		return;
	}
	if (channelpanel == console) {
		debugMessage(QSL("[BUG] Was told to leave the chanel panel for the console! Channel panel '%1'.").arg(panelname));
		return;
	}
	if (channelpanel->getSessionID().isEmpty()) {
		debugMessage(QSL("[BUG] Was told to leave the chanel panel '%1', but it has no session associated with it!.").arg(panelname));
		return;
	}
	FSession *session = account->getSession(channelpanel->getSessionID());
	QString channelname;
	if (channelpanel->type() != FChannel::CHANTYPE_PM) {
		channelname = channelpanel->getChannelName();
	}
	closeChannelPanel(panelname);
	if (!channelname.isEmpty()) {
		session->sendChannelLeave(channelname);
	}
}
void flist_messenger::tb_closeClicked()
{
	leaveChannelPanel(tb_recent_name);
}
void flist_messenger::tb_settingsClicked()
{
	channelSettingsDialogRequested();
}

void flist_messenger::setupRealUI()
{
	// Setting up console first because it needs to receive server output.
	//todo: Make account->getSession(currentPanel->getSessionID()) code robust to having a sessionless panel.
	//todo: Remove 'charName' so that this panel can be sessionless
	console = new FChannelPanel(this, charName, QSL("FCHATSYSTEMCONSOLE"), QSL("FCHATSYSTEMCONSOLE"), FChannel::CHANTYPE_CONSOLE);
	QString name = QSL("Console");
	console->setTitle ( name );
	channelList[QSL("FCHATSYSTEMCONSOLE")] = console;

	if (objectName().isEmpty()) {
		setObjectName ( QSL("MainWindow") );
	}

	resize ( 836, 454 );
	setWindowTitle(QSL(FLIST_VERSION));
	setWindowIcon(QIcon(QSL(":/images/icon.ico")));
	actionDisconnect = new QAction(this);
	actionDisconnect->setObjectName(QSL("actionDisconnect"));
	actionDisconnect->setText(QSL("Disconnect (WIP)") );
	actionDisconnect->setIcon(QIcon(QSL(":/images/plug-disconnect.png")));
	actionQuit = new QAction(this);
	actionQuit->setObjectName(QSL("actionQuit"));
	actionQuit->setText(QSL("&Quit"));
	actionQuit->setIcon(QIcon(QSL(":/images/cross-button.png")));
	actionHelp = new QAction(this);
	actionHelp->setObjectName(QSL("actionHelp"));
	actionHelp->setText(QSL("Help"));
	actionHelp->setIcon(QIcon(QSL(":/images/question.png")));
	actionAbout = new QAction(this);
	actionAbout->setObjectName(QSL("actionAbout"));
	actionAbout->setText(QSL("About"));
	actionAbout->setIcon(QIcon(QSL(":/images/icon.ico")));
	verticalLayoutWidget = new QWidget(this);
	verticalLayoutWidget->setObjectName(QSL("overview"));
	verticalLayoutWidget->setGeometry(QRect(5, -1, 841, 511));
	verticalLayout = new QVBoxLayout(verticalLayoutWidget);
	verticalLayout->setObjectName(QSL("overviewLayout"));
	verticalLayout->setContentsMargins(0, 0, 0, 0);
	horizontalLayoutWidget = new QWidget(this);
	horizontalLayoutWidget->setObjectName(QSL("main"));
	horizontalLayoutWidget->setGeometry(QRect(0, -1, 831, 401));
	horizontalLayout = new QHBoxLayout ( horizontalLayoutWidget);
	horizontalLayout->setObjectName(QSL("mainLayout"));
	horizontalLayout->setContentsMargins(0, 0, 0, 0);
	activePanels = new QScrollArea(horizontalLayoutWidget);
	activePanels->setObjectName(QSL("activePanels"));
	QSizePolicy sizePolicy ( QSizePolicy::Minimum, QSizePolicy::Expanding );
	sizePolicy.setHorizontalStretch ( 0 );
	sizePolicy.setVerticalStretch ( 0 );
	sizePolicy.setHeightForWidth ( activePanels->sizePolicy().hasHeightForWidth() );
	activePanels->setSizePolicy ( sizePolicy );
	activePanels->setMinimumSize ( QSize ( 67, 0 ) );
	activePanels->setMaximumSize ( QSize ( 16777215, 16777215 ) );
	activePanels->setBaseSize ( QSize ( 0, 0 ) );
	activePanels->setFrameShape ( QFrame::StyledPanel );
	activePanels->setFrameShadow ( QFrame::Sunken );
	activePanels->setVerticalScrollBarPolicy ( Qt::ScrollBarAsNeeded );
	activePanels->setHorizontalScrollBarPolicy ( Qt::ScrollBarAlwaysOff );
	activePanels->setWidgetResizable ( true );
	activePanelsContents = new QVBoxLayout();
	activePanelsContents->setObjectName ( QSL("activePanelsLayout") );
	activePanelsContents->setGeometry ( QRect ( 0, 0, 61, 393 ) );
	activePanels->setFixedWidth ( 45 );
	activePanels->setLayout ( activePanelsContents );
	horizontalLayout->addWidget ( activePanels );
	horizontalsplitter = new QSplitter(Qt::Horizontal);
	centralstuffwidget = new QWidget(horizontalsplitter);
	centralstuffwidget->setContentsMargins(0, 0, 0, 0);
	centralStuff = new QVBoxLayout(centralstuffwidget);
	centralButtonsWidget = new QWidget;
	centralButtons = new QHBoxLayout ( centralButtonsWidget );
	centralButtonsWidget->setLayout ( centralButtons );
	lblChannelName = new QLabel ( QSL("") );
	lblChannelName->setObjectName( QSL("currentpanelname") );
	lblChannelName->setWordWrap( true );
	QSizePolicy lblchannelnamesizepolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	lblChannelName->setSizePolicy(lblchannelnamesizepolicy);
	btnSettings = new QPushButton;
	btnSettings->setIcon(QIcon(QSL(":/images/terminal.png")));
	btnSettings->setToolTip(QSL("Settings"));
	btnSettings->setObjectName(QSL("settingsbutton"));
	btnChannels = new QPushButton;
	btnChannels->setIcon ( QIcon ( QSL(":/images/hash.png") ) );
	btnChannels->setToolTip(QSL("Channels"));
	btnChannels->setObjectName(QSL("channelsbutton"));
	btnMakeRoom = new QPushButton;
	btnMakeRoom->setIcon ( QIcon ( QSL(":/images/key.png") ) );
	btnMakeRoom->setToolTip(QSL("Make a room!"));
	btnMakeRoom->setObjectName(QSL("makeroombutton"));
	btnSetStatus = new QPushButton;
	btnSetStatus->setIcon ( QIcon ( QSL(":/images/status-default.png") ) );
	btnSetStatus->setToolTip(QSL("Set your status"));
	btnSetStatus->setObjectName(QSL("setstatusbutton"));
	btnFriends = new QPushButton;
	btnFriends->setIcon ( QIcon ( QSL(":/images/users.png") ) );
	btnFriends->setToolTip(QSL("Friends and bookmarks"));
	btnFriends->setObjectName(QSL("friendsbutton"));
	btnReport = new QPushButton;
	btnReport->setIcon(QIcon(QSL(":/images/auction-hammer--exclamation.png")));
	btnReport->setToolTip(QSL("Alert Staff!"));
	btnReport->setObjectName(QSL("alertstaffbutton"));
	centralButtons->addWidget ( lblChannelName );
	centralButtons->addWidget(btnSettings);
	centralButtons->addWidget(btnReport);
	centralButtons->addWidget(btnFriends);
	centralButtons->addWidget(btnSetStatus);
	centralButtons->addWidget(btnMakeRoom);
	centralButtons->addWidget(btnChannels);
	connect(btnSettings, &QPushButton::clicked, this, &flist_messenger::settingsDialogRequested);
	connect(btnReport, &QPushButton::clicked, this, &flist_messenger::reportDialogRequested);
	connect(btnFriends, &QPushButton::clicked, this, &flist_messenger::friendsDialogRequested);
	connect(btnChannels, &QPushButton::clicked, this, &flist_messenger::channelsDialogRequested);
	connect(btnMakeRoom, &QPushButton::clicked, this, &flist_messenger::makeRoomDialogRequested);
	connect(btnSetStatus, &QPushButton::clicked, this, &flist_messenger::setStatusDialogRequested);
	chatview = new FLogTextBrowser(this);
	chatview->setOpenLinks(false);
	chatview->setObjectName ( QSL("chatoutput") );
	chatview->setContextMenuPolicy ( Qt::DefaultContextMenu );
	chatview->setFrameShape ( QFrame::NoFrame );
	connect(chatview, &FLogTextBrowser::anchorClicked, this, &flist_messenger::anchorClicked);
	centralStuff->addWidget ( centralButtonsWidget );
	centralStuff->addWidget ( chatview );
	horizontalsplitter->addWidget (centralstuffwidget);
	QSizePolicy centralstuffwidgetsizepolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	centralstuffwidgetsizepolicy.setHorizontalStretch(4);
	centralstuffwidgetsizepolicy.setVerticalStretch(0);
	centralstuffwidgetsizepolicy.setHeightForWidth(false);
	centralstuffwidget->setSizePolicy(centralstuffwidgetsizepolicy);
	
	listWidget = new QListWidget(horizontalsplitter);
	listWidget->setObjectName ( QSL("userlist") );
	QSizePolicy sizePolicy1 ( QSizePolicy::Preferred, QSizePolicy::Expanding );
	sizePolicy1.setHorizontalStretch ( 1 );
	sizePolicy1.setVerticalStretch ( 0 );
	sizePolicy1.setHeightForWidth ( listWidget->sizePolicy().hasHeightForWidth() );
	listWidget->setSizePolicy ( sizePolicy1 );
	listWidget->setMinimumSize ( QSize ( 30, 0 ) );
	listWidget->setMaximumSize(QSize(16777215, 16777215));
	listWidget->setBaseSize ( QSize ( 100, 0 ) );
	listWidget->setContextMenuPolicy ( Qt::CustomContextMenu );
	listWidget->setIconSize ( QSize ( 16, 16 ) );
	connect(listWidget, &QListWidget::customContextMenuRequested, this, &flist_messenger::userListContextMenuRequested);
	horizontalsplitter->addWidget(listWidget);
	horizontalLayout->addWidget(horizontalsplitter);
	verticalLayout->addWidget ( horizontalLayoutWidget );
	QWidget *textFieldWidget = new QWidget;
	textFieldWidget->setObjectName(QSL("inputarea"));
	QHBoxLayout *inputLayout = new QHBoxLayout ( textFieldWidget );
	inputLayout->setObjectName(QSL("inputareaLayout"));
	plainTextEdit = new QPlainTextEdit;
	plainTextEdit->setObjectName ( QSL("chatinput") );
	QSizePolicy sizePolicy2 ( QSizePolicy::Expanding, QSizePolicy::Maximum );
	sizePolicy2.setHorizontalStretch ( 0 );
	sizePolicy2.setVerticalStretch ( 0 );
	sizePolicy2.setHeightForWidth ( plainTextEdit->sizePolicy().hasHeightForWidth() );
	plainTextEdit->setSizePolicy ( sizePolicy2 );
	plainTextEdit->setMaximumSize ( QSize ( 16777215, 75 ) );
	returnFilter = new UseReturn ( this );
	plainTextEdit->installEventFilter ( returnFilter );
	plainTextEdit->setFrameShape ( QFrame::NoFrame );
	connect(plainTextEdit, &QPlainTextEdit::textChanged, this, &flist_messenger::inputChanged);
	QVBoxLayout *vblSouthButtons = new QVBoxLayout;
	btnSendChat = new QPushButton(QSL("Send Message"));
	btnSendAdv = new QPushButton(QSL("Post RP ad"));
	btnSendChat->setObjectName(QSL("sendmsgbutton"));
	btnSendAdv->setObjectName(QSL("sendadvbutton"));
	vblSouthButtons->addWidget(btnSendChat);
	vblSouthButtons->addWidget(btnSendAdv);
	QSizePolicy sizePolicy4(QSizePolicy::Minimum, QSizePolicy::Maximum);
	sizePolicy4.setHorizontalStretch(0);
	sizePolicy4.setVerticalStretch(0);
	connect(btnSendChat, &QPushButton::clicked, this, &flist_messenger::parseInput);
	connect(btnSendAdv,  &QPushButton::clicked, this, &flist_messenger::btnSendAdvClicked);

	inputLayout->addWidget(plainTextEdit);
	inputLayout->addLayout(vblSouthButtons);
	verticalLayout->addWidget ( textFieldWidget );
	setCentralWidget ( verticalLayoutWidget );
	menubar = new QMenuBar ( this );
	menubar->setObjectName ( QSL("menubar") );
	menubar->setGeometry ( QRect ( 0, 0, 836, 23 ) );
	menuHelp = new QMenu ( menubar );
	menuHelp->setObjectName ( QSL("menuHelp") );
	menuHelp->setTitle ( QSL("Help") );
	menuFile = new QMenu ( menubar );
	menuFile->setObjectName ( QSL("menuFile") );
	menuFile->setTitle ( QSL("&File") );
	setMenuBar ( menubar );
	menubar->addAction ( menuFile->menuAction() );
	menubar->addAction ( menuHelp->menuAction() );
	menuHelp->addAction ( actionHelp );
	menuHelp->addSeparator();
	menuHelp->addAction ( actionAbout );
	menuFile->addAction ( actionDisconnect );
	menuFile->addSeparator();
	menuFile->addAction ( actionQuit );
	connect(actionHelp,  &QAction::triggered, this, &flist_messenger::helpDialogRequested);
	connect(actionAbout, &QAction::triggered, this, &flist_messenger::aboutApp);
	connect(actionQuit,  &QAction::triggered, this, &flist_messenger::quitApp);
	centerOnScreen(this);
	setupConsole();
}

void flist_messenger::setupConsole()
{
	activePanelsSpacer = new QSpacerItem ( 0, 20, QSizePolicy::Ignored, QSizePolicy::Expanding );
	currentPanel = console;
	pushButton = new QPushButton();
	pushButton->setObjectName ( QSL("CONSOLE") );
	pushButton->setGeometry ( QRect ( 0, 0, 30, 30 ) );
	pushButton->setFixedSize ( 30, 30 );
	pushButton->setIcon ( QIcon ( QSL(":/images/terminal.png") ) );
	pushButton->setToolTip ( QSL("View Console") );
	pushButton->setCheckable ( true );
	pushButton->setStyleSheet ( QSL("background-color: rgb(255, 255, 255);") );
	pushButton->setChecked ( true );
	console->pushButton = pushButton;
	connect(pushButton, &QPushButton::clicked, this, &flist_messenger::channelButtonClicked);
	
	activePanelsContents->addWidget ( pushButton, 0, Qt::AlignTop );
	activePanelsContents->addSpacerItem ( activePanelsSpacer );
}

void flist_messenger::userListContextMenuRequested ( const QPoint& point ) {
	QListWidgetItem* lwi = listWidget->itemAt ( point );

	if (lwi) {
		FSession *session = account->getSession(currentPanel->getSessionID());
		ul_recent_name = lwi->text();
		FCharacter* ch = session->getCharacter(ul_recent_name);
		displayCharacterContextMenu ( ch );
	}
}

void flist_messenger::friendListContextMenuRequested(QString character)
{
	FSession *session = account->getSessionByCharacter(charName); //todo: fix this
	if (session->isCharacterOnline(character))
	{
		ul_recent_name = character;
		FCharacter* ch = session->getCharacter(ul_recent_name);
		displayCharacterContextMenu ( ch );
	}
}

void flist_messenger::displayChannelContextMenu(FChannelPanel *ch)
{
	if (!ch) {
		printDebugInfo("ERROR: Tried to display a context menu for a null pointer channel.");
	}
	else {
		QMenu* menu = new QMenu(this);
		recentChannelMenu = menu;
		menu->addAction(QIcon(QSL(":/images/cross.png")), QSL("Close tab"), this, &flist_messenger::tb_closeClicked);
		menu->addAction(QIcon(QSL(":/images/terminal.png")), QSL("Settings"), this, &flist_messenger::tb_settingsClicked);
		connect(menu, &QMenu::aboutToHide, this, &flist_messenger::destroyChanMenu);
		menu->exec ( QCursor::pos() );
	}
}

void flist_messenger::displayCharacterContextMenu ( FCharacter* ch )
{
	if (!ch )  {
		printDebugInfo("ERROR: Tried to display a context menu for a null pointer character.");
	}
	else {
		QMenu* menu = new QMenu ( this );
		recentCharMenu = menu;
		menu->addAction(QIcon(QSL(":/images/users.png")), QSL("Private Message"), this, &flist_messenger::ul_pmRequested);
		menu->addAction(QIcon(QSL(":/images/book-open-list.png")), QSL("F-list Profile"), this, &flist_messenger::ul_profileRequested);
		menu->addAction(QIcon(QSL(":/images/tag-label.png")), QSL("Quick Profile"), this, &flist_messenger::ul_infoRequested);
		menu->addAction(QSL("Copy Profile Link"), this, &flist_messenger::ul_copyLink);
		FSession *session = account->getSession(currentPanel->getSessionID());
		if (session->isCharacterIgnored(ch->name()))
			menu->addAction(QIcon(QSL(":/images/heart.png")), QSL("Unignore"), this, &flist_messenger::ul_ignoreRemove);
		else
			menu->addAction(QIcon(QSL(":/images/heart-break.png")), QSL("Ignore"), this, &flist_messenger::ul_ignoreAdd);
		bool op = session->isCharacterOperator(session->character);
		if (op) {
			menu->addAction(QIcon(QSL(":/images/fire.png")), QSL("Chat Kick"), this, &flist_messenger::ul_chatKick);
			menu->addAction(QIcon(QSL(":/images/auction-hammer--exclamation.png")), QSL("Chat Ban"), this, &flist_messenger::ul_chatBan);
			menu->addAction(QIcon(QSL(":/images/alarm-clock.png")), QSL("Timeout..."), this, &flist_messenger::timeoutDialogRequested);
		}
		
		if (op || currentPanel->isOwner(session->getCharacter(session->character))) {
			if (currentPanel->isOp(ch))
				menu->addAction(QIcon(QSL(":/images/auction-hammer--minus.png")), QSL("Remove Channel OP"), this, &flist_messenger::ul_channelOpRemove);
			else
				menu->addAction(QIcon(QSL(":/images/auction-hammer--plus.png")), QSL("Add Channel OP"), this, &flist_messenger::ul_channelOpAdd);
		}
		
		if ((op || currentPanel->isOp(session->getCharacter(session->character))) && !ch->isChatOp()) {
			menu->addAction(QIcon(QSL(":/images/lightning.png")), QSL("Channel Kick"), this, &flist_messenger::ul_channelKick);
			menu->addAction(QIcon(QSL(":/images/auction-hammer.png")), QSL("Channel Ban"), this, &flist_messenger::ul_channelBan);
		}
		connect(menu, &QMenu::aboutToHide, this, &flist_messenger::destroyMenu);
		menu->exec (QCursor::pos());
	}
}

void flist_messenger::cl_joinRequested(QStringList channels)
{
	FSession *session = account->getSession(charName); //TODO: fix this
	foreach(QString channel, channels) {
		FChannelPanel *channelpanel = channelList.value(channel);
		if (!channelpanel || !channelpanel->getActive() ) {
			session->joinChannel(channel);
		}
	}
}

void flist_messenger::anchorClicked ( QUrl link )
{
	QString linktext = link.toString();
	/* Anchor commands:
	 * AHI: Ad hoc invite. Join channel.
	 * CSA: Confirm staff report.
	 */
	if (linktext.startsWith(QSL("#AHI-"))) {
		QString channel = linktext.mid(5);
		FSession *session = account->getSession(currentPanel->getSessionID());
		session->joinChannel(channel);
	}
	else if (linktext.startsWith(QSL("#CSA-"))) {
		// Confirm staff alert
		FSession *session = account->getSession(currentPanel->getSessionID());
		session->sendConfirmStaffReport(linktext.mid(5));
	}
	else if (linktext.startsWith(QSL("http://")) || linktext.startsWith(QSL("https://"))) {
		//todo: Make this configurable between opening, copying, or doing nothing.
		QDesktopServices::openUrl(link);
	}
	else {
		//todo: Show a debug or error message?
	}
	chatview->verticalScrollBar()->setSliderPosition(chatview->verticalScrollBar()->maximum());
}
void flist_messenger::insertLineBreak()
{
	if (chatview) {
		plainTextEdit->insertPlainText(QSL("\n"));
	}
}
void flist_messenger::refreshUserlist()
{
	if (currentPanel == nullptr) {
		return;
	}
	//todo: Should 'listWidget' be renewed like this?
	listWidget = this->findChild<QListWidget *> ( QSL("userlist") );

	//Save current scroll position.
	int scrollpos = listWidget->verticalScrollBar()->value();

	//Remember currently selected characters, so that we can restore them.
	//Probably overkill, but this does support the selection of multiple rows.
	QList<QListWidgetItem *> selecteditems = listWidget->selectedItems();
	QStringList selectedcharacters;
	foreach(QListWidgetItem *selecteditem, selecteditems) {
		selectedcharacters.append(selecteditem->text());
	}
	int currentrow = -1;
	int oldrow = listWidget->currentRow();
	QString currentname; 
	{
		QListWidgetItem *currentitem = listWidget->currentItem();
		if (currentitem) {
			currentname = currentitem->text();
		}
	}
	selecteditems.clear();

	//Clear the existing list and reload it.
	listWidget->clear();
	QList<FCharacter*> charList = currentPanel->charList();
	foreach(FCharacter *character, charList) {
		QListWidgetItem *charitem = new QListWidgetItem(character->name());
		QIcon* i = character->statusIcon();
		charitem->setIcon ( *i );

		QFont f = charitem->font();

		if (character->isChatOp())  {
			f.setBold(true);
			f.setItalic(true);
		}
		else if (currentPanel->isOp(character)) {
			f.setBold (true);
		}
		charitem->setFont ( f );
		charitem->setTextColor ( character->genderColor() );
		listWidget->addItem ( charitem );
		//Is this what the current row was previously?
		if (character->name() == currentname) {
			currentrow = listWidget->count() - 1;
		}
		//or a selected item?
		if (selectedcharacters.contains(character->name())) {
			if (currentrow < 0) {
				currentrow = listWidget->count() - 1;
			}
			selecteditems.append(charitem);
		}
	}

	//Restore selection.
	if (selecteditems.count() <= 0) {
		//Could not find any previous matches, just set the current row to the same as the old row.
		listWidget->setCurrentRow(oldrow);
	}
	else {
		//Set current row.
		listWidget->setCurrentRow(currentrow, QItemSelectionModel::NoUpdate);
		//Restore selected items.
		foreach(QListWidgetItem *selecteditem, selecteditems) {
			selecteditem->setSelected(true);
		}
	}

	//Restore scroll position.
	listWidget->verticalScrollBar()->setValue(scrollpos);

	//Hide/show widget based upon panel type.
	if (currentPanel->type() == FChannel::CHANTYPE_PM || currentPanel->type() == FChannel::CHANTYPE_CONSOLE) {
		listWidget->hide();
	}
	else {
		listWidget->show();
	}
}

void flist_messenger::settingsDialogRequested()
{
	if (settingsDialog == nullptr || settingsDialog->parent() != this) {
		setupSettingsDialog();
	}

	se_chbLeaveJoin->setChecked(settings->getShowJoinLeaveMessage());
	se_chbMute->setChecked(!settings->getPlaySounds());
	se_chbEnableChatLogs->setChecked(settings->getLogChat());
	se_chbOnlineOffline->setChecked(settings->getShowOnlineOfflineMessage());
	se_attentionsettings->loadSettings();

	settingsDialog->show();
}

void flist_messenger::setupSettingsDialog()
{
	if (settingsDialog) {
		settingsDialog->deleteLater();
	}

	settingsDialog = new QDialog(this);
	se_chbLeaveJoin = new QCheckBox(QSL("Display leave/join notices"));
	se_chbOnlineOffline = new QCheckBox(QSL("Display online/offline notices for friends"));
	se_chbEnableChatLogs = new QCheckBox(QSL("Save chat logs"));
	se_chbMute = new QCheckBox(QSL("Mute sounds"));
	se_attentionsettings = new FAttentionSettingsWidget("");

	QTabWidget* twOverview = new QTabWidget;
	QGroupBox* gbGeneral = new QGroupBox(QSL("General"));
	QGroupBox* gbNotifications = new QGroupBox(QSL("Notifications"));
	QGroupBox* gbSounds = new QGroupBox(QSL("Sounds"));
	QVBoxLayout* vblOverview = new QVBoxLayout;
	QVBoxLayout* vblGeneral = new QVBoxLayout;
	QVBoxLayout* vblNotifications = new QVBoxLayout;
	QVBoxLayout* vblSounds = new QVBoxLayout;
	QHBoxLayout* hblButtons = new QHBoxLayout;
	QPushButton* btnSubmit = new QPushButton(QIcon(QSL(":/images/tick.png")), QSL("Save settings"));
	QPushButton* btnCancel = new QPushButton(QIcon(QSL(":/images/cross.png")), QSL("Cancel"));
	
	settingsDialog->setLayout(vblOverview);
	vblOverview->addWidget(twOverview);
	vblOverview->addLayout(hblButtons);
	hblButtons->addStretch();
	hblButtons->addWidget(btnSubmit);
	hblButtons->addWidget(btnCancel);

	twOverview->addTab(gbGeneral, QSL("General"));
	gbGeneral->setLayout(vblGeneral);
	vblGeneral->addWidget(se_chbLeaveJoin);
	vblGeneral->addWidget(se_chbOnlineOffline);
	vblGeneral->addWidget(se_chbEnableChatLogs);
	vblGeneral->addStretch(0);
	twOverview->addTab(gbNotifications, QSL("Notifications"));
	gbNotifications->setLayout(vblNotifications);
	vblNotifications->addWidget(se_attentionsettings);
	twOverview->addTab(gbSounds, QSL("Sounds"));
	vblSounds->addWidget(se_chbMute);
	vblSounds->addStretch(0);
	gbSounds->setLayout(vblSounds);

	connect(btnSubmit, &QPushButton::clicked, this, &flist_messenger::se_btnSubmitClicked);
	connect(btnCancel, &QPushButton::clicked, this, &flist_messenger::se_btnCancelClicked);
}

void flist_messenger::setupFriendsDialog()
{
	if (friendsDialog == nullptr || friendsDialog->parent() != this ) {
		friendsDialog = new FriendsDialog(account->getSessionByCharacter(charName), this);
		connect(friendsDialog, &FriendsDialog::privateMessageRequested, this, &flist_messenger::openPMTab);
		connect(friendsDialog, &FriendsDialog::privateMessageRequested, this, &flist_messenger::openPMTab);
		connect(friendsDialog, &FriendsDialog::friendContextMenuRequested, this, &flist_messenger::friendListContextMenuRequested);
	}
}

void flist_messenger::friendsDialogRequested()
{
	setupFriendsDialog();
	friendsDialog->showFriends();
}

void flist_messenger::ignoreDialogRequested()
{
	setupFriendsDialog();
	friendsDialog->showIgnore();
}

void flist_messenger::makeRoomDialogRequested()
{
	if (makeRoomDialog == nullptr || makeRoomDialog->parent() != this ) {
		makeRoomDialog = new FMakeRoomDialog(this);
		connect(makeRoomDialog, &FMakeRoomDialog::requestedRoomCreation, this, &flist_messenger::createPrivateChannel);
	}
	makeRoomDialog->show();
}

void flist_messenger::setStatusDialogRequested()
{
	if (setStatusDialog == nullptr || setStatusDialog->parent() != this ) {
		setStatusDialog = new StatusDialog(this);
		connect(setStatusDialog, &StatusDialog::statusSelected, this, &flist_messenger::changeStatus);
	}

	setStatusDialog->setShownStatus(selfStatus, selfStatusMessage);
	setStatusDialog->show();
}

void flist_messenger::characterInfoDialogRequested()
{
	//[19:48 PM]>>PRO {"character":"Hexxy"}
	//[19:41 PM]>>KIN {"character":"Cinnamon Flufftail"}
	FSession *session = account->getSession(currentPanel->getSessionID());
	FCharacter* ch = session->getCharacter(ul_recent_name);
	if (!ch) {
		debugMessage(QSL("Tried to request character info on the character '%1' but they went offline already!").arg(ul_recent_name));
		return;
	}
	session->requestProfileKinks(ch->name());

	if (!ci_dialog) { ci_dialog = new FCharacterInfoDialog(this); }
	ci_dialog->setDisplayedCharacter(ch);
	ci_dialog->show();
}
void flist_messenger::reportDialogRequested()
{
	if (reportDialog == nullptr || reportDialog->parent() != this) {
		setupReportDialog();
	}

	re_leWho->setText(QSL("None"));
	re_teProblem->clear();
	reportDialog->show();

}

void flist_messenger::channelsDialogRequested()
{
	account->getSessionByCharacter(charName)->requestChannels();

	if (cl_dialog == nullptr) {
		cl_dialog = new FChannelListDialog(cl_data, this);
		connect(cl_dialog, &FChannelListDialog::joinRequested, this, &flist_messenger::cl_joinRequested);
	}
	cl_dialog->show();
}

void flist_messenger::refreshChatLines()
{
	if (currentPanel == nullptr) { return; }

	chatview->clear();
	currentPanel->printChannel ( chatview );
}

FChannelTab* flist_messenger::addToActivePanels ( QString& panelname, QString &channelname, QString& tooltip )
{
	printDebugInfo("Joining " + channelname.toStdString());
	channelTab = this->findChild<FChannelTab *> ( panelname );

	if (channelTab != nullptr ) {
		channelTab->setVisible ( true );
	}
	else {
		activePanelsContents->removeItem ( activePanelsSpacer );
		channelTab = new FChannelTab();
		channelTab->setObjectName ( panelname );
		channelTab->setGeometry ( QRect ( 0, 0, 30, 30 ) );
		channelTab->setFixedSize ( 30, 30 );
		channelTab->setStyleSheet ( QSL("background-color: rgb(255, 255, 255);") );
		channelTab->setAutoFillBackground ( true );
		channelTab->setCheckable ( true );
		channelTab->setChecked ( false );
		channelTab->setToolTip ( tooltip );
		channelTab->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(channelTab, &FChannelTab::clicked, this, &flist_messenger::channelButtonClicked);
		connect(channelTab, &FChannelTab::customContextMenuRequested, this, &flist_messenger::tb_channelRightClicked);
		activePanelsContents->addWidget ( channelTab, 0, Qt::AlignTop );

		if (panelname.startsWith(QSL("PM|||")) ) {
			avatarFetcher.applyAvatarToButton(channelTab, channelname);
			//channelTab->setIconSize(channelTab->iconSize()*1.5);
		}
		else if (panelname.startsWith(QSL("ADH|||")) ) {
			//todo: custom buttons
			channelTab->setIcon ( QIcon ( QSL(":/images/key.png") ) );
		}
		else {
			//todo: custom buttons
			channelTab->setIcon ( QIcon ( QSL(":/images/hash.png") ) );
		}

		activePanelsContents->addSpacerItem ( activePanelsSpacer );
	}

	return channelTab;
}

void flist_messenger::aboutApp()
{
	if (!aboutDialog)
	{
		aboutDialog = new FAboutDialog(this);
		connect(aboutDialog, &FAboutDialog::anchorClicked, this, &flist_messenger::anchorClicked);
	}
	aboutDialog->show();
}

void flist_messenger::quitApp()
{
	if (trayIcon) {
		trayIcon->hide();
	}
	QApplication::quit();
}

void flist_messenger::socketError ( QAbstractSocket::SocketError socketError )
{
	(void) socketError;
	FSession *session = account->getSession(charName); //todo: fix this
	QString sockErrorStr = session->socket->errorString();
	if (currentPanel )
	{
		QString errorstring = QSL("<b>Socket Error: </b>") + sockErrorStr;
		messageSystem(nullptr, errorstring, MessageType::Error);
	}
	else {
		QMessageBox::critical ( this, QSL("Socket Error!"), QSL("Socket Error: ") + sockErrorStr );
	}

	disconnected = true;
}

void flist_messenger::changeStatus (QString status, QString statusmsg )
{
	selfStatus = status;
	selfStatusMessage = statusmsg;
	account->getSessionByCharacter(charName)->sendStatus(status, statusmsg);

	if (status == QSL("online"))
		btnSetStatus->setIcon(QIcon(QSL(":/images/status-default.png")));
	else if (status == QSL("busy"))
		btnSetStatus->setIcon(QIcon(QSL(":/images/status-away.png")));
	else if (status == QSL("dnd"))
		btnSetStatus->setIcon(QIcon(QSL(":/images/status-busy.png")));
	else if (status == QSL("looking"))
		btnSetStatus->setIcon(QIcon(QSL(":/images/status.png")));
	else if (status == QSL("away"))
		btnSetStatus->setIcon(QIcon(QSL(":/images/status-blue.png")));

	QString output = QSL("Status changed successfully.");

	messageSystem(nullptr, output, MessageType::Feedback);
}

void flist_messenger::tb_channelRightClicked ( const QPoint & point )
{
	(void) point;
	QObject* sender = this->sender();
	QPushButton* button = qobject_cast<QPushButton*> ( sender );
	if (button) {
		tb_recent_name = button->objectName();
		channelButtonMenuRequested();
	}
}

void flist_messenger::channelButtonMenuRequested()
{
	displayChannelContextMenu(channelList.value(tb_recent_name));
}

void flist_messenger::channelButtonClicked()
{
	QObject* sender = this->sender();
	QPushButton* button = qobject_cast<QPushButton*> ( sender );

	if (button)
	{
		QString tab = button->objectName();
		switchTab ( tab );
	}
}

void flist_messenger::usersCommand()
{
	FSession *session = account->getSession(currentPanel->getSessionID());
	QString msg = QSL("<b>%0 users online.</b>").arg(session->getCharacterCount());
	messageSystem(session, msg, MessageType::Feedback);
}

void flist_messenger::inputChanged()
{
	if (currentPanel && currentPanel->type() == FChannel::CHANTYPE_PM ) {
		if (plainTextEdit->toPlainText().simplified().isEmpty() ) {
			typingCleared ( currentPanel );
			currentPanel->setTypingSelf ( TYPING_STATUS_CLEAR );
		}
		else {
			if (currentPanel->getTypingSelf() != TYPING_STATUS_TYPING ) {
				typingContinued ( currentPanel );
				currentPanel->setTypingSelf ( TYPING_STATUS_TYPING );
			}
		}
	}
}

void flist_messenger::typingCleared ( FChannelPanel* channel )
{
	account->getSessionByCharacter(charName)->sendTypingNotification(channel->recipient(), TYPING_STATUS_CLEAR);
}

void flist_messenger::typingContinued ( FChannelPanel* channel )
{
	account->getSessionByCharacter(charName)->sendTypingNotification(channel->recipient(), TYPING_STATUS_TYPING);
}

void flist_messenger::typingPaused ( FChannelPanel* channel )
{
	account->getSessionByCharacter(charName)->sendTypingNotification(channel->recipient(), TYPING_STATUS_PAUSED);
}

void flist_messenger::ul_pmRequested()
{
	openPMTab(ul_recent_name);
}

void flist_messenger::ul_infoRequested()
{
	characterInfoDialogRequested();
}

void flist_messenger::ul_ignoreAdd()
{
	FSession *session = account->getSession(charName); //todo: fix this
	if (session->isCharacterIgnored(ul_recent_name)) {
		printDebugInfo("[CLIENT BUG] Tried to ignore somebody who is already on the ignorelist.");
	}
	else {
		session->sendIgnoreAdd(ul_recent_name);
	}
}
void flist_messenger::ul_ignoreRemove()
{
	FSession *session = account->getSession(charName); //todo: fix this
	if (!session->isCharacterIgnored(ul_recent_name)) {
		printDebugInfo("[CLIENT BUG] Tried to unignore somebody who is not on the ignorelist.");
	}
	else {
		session->sendIgnoreDelete(ul_recent_name);
	}
}
void flist_messenger::ul_channelBan()
{
	account->getSessionByCharacter(charName)->banFromChannel(currentPanel->getChannelName(), ul_recent_name);
}
void flist_messenger::ul_channelKick()
{
	account->getSessionByCharacter(charName)->kickFromChannel(currentPanel->getChannelName(), ul_recent_name);
}
void flist_messenger::ul_chatBan()
{
	account->getSessionByCharacter(charName)->banFromChat(ul_recent_name);
}
void flist_messenger::ul_chatKick()
{
	account->getSessionByCharacter(charName)->kickFromChat(ul_recent_name);
}
void flist_messenger::ul_chatTimeout()
{
	timeoutDialogRequested();
}
void flist_messenger::ul_channelOpAdd()
{
	account->getSessionByCharacter(charName)->giveChanop(currentPanel->getChannelName(), ul_recent_name);
}
void flist_messenger::ul_channelOpRemove()
{
	account->getSessionByCharacter(charName)->takeChanop(currentPanel->getChannelName(), ul_recent_name);
}
void flist_messenger::ul_chatOpAdd()
{
	account->getSessionByCharacter(charName)->giveGlobalop(ul_recent_name);
}
void flist_messenger::ul_chatOpRemove()
{
	account->getSessionByCharacter(charName)->takeGlobalop(ul_recent_name);
}
void flist_messenger::ul_profileRequested()
{
	QUrl link(QSL("https://www.f-list.net/c/%1/").arg(ul_recent_name));
	QDesktopServices::openUrl(link);
}

void flist_messenger::ul_copyLink()
{
	QString link = QSL("https://www.f-list.net/c/%1/").arg(ul_recent_name);
	
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(link, QClipboard::Clipboard);
	clipboard->setText(link, QClipboard::Selection);
}

void flist_messenger::setupTimeoutDialog()
{
	QVBoxLayout* to_vblOverview;
	QHBoxLayout* to_hblButtons;
	QLabel* to_lblWho;
	QLabel* to_lblLength;
	QLabel* to_lblReason;
	QPushButton* to_btnSubmit;
	QPushButton* to_btnCancel;
	timeoutDialog = new QDialog(this);
	to_lblWho = new QLabel(QSL("Who?"));
	to_lblLength = new QLabel(QSL("How long? (Minutes, up to 90)"));
	to_lblReason = new QLabel(QSL("Why?"));
	to_leWho = new QLineEdit;
	to_leLength = new QLineEdit;
	to_leReason = new QLineEdit;
	to_vblOverview = new QVBoxLayout;
	to_hblButtons = new QHBoxLayout;
	to_btnSubmit = new QPushButton(QIcon(QSL(":/images/tick.png")), QSL("Submit"));
	to_btnCancel = new QPushButton(QIcon(QSL(":/images/cross.png")), QSL("Cancel"));

	timeoutDialog->setLayout(to_vblOverview);
	to_vblOverview->addWidget(to_lblWho);
	to_vblOverview->addWidget(to_leWho);
	to_vblOverview->addWidget(to_lblLength);
	to_vblOverview->addWidget(to_leLength);
	to_vblOverview->addWidget(to_lblReason);
	to_vblOverview->addWidget(to_leReason);
	to_vblOverview->addLayout(to_hblButtons);
	to_hblButtons->addStretch();
	to_hblButtons->addWidget(to_btnSubmit);
	to_hblButtons->addWidget(to_btnCancel);
	connect(to_btnSubmit, &QPushButton::clicked, this, &flist_messenger::to_btnSubmitClicked);
	connect(to_btnCancel, &QPushButton::clicked, this, &flist_messenger::to_btnCancelClicked);
}

void flist_messenger::timeoutDialogRequested() {
	if (timeoutDialog == nullptr || timeoutDialog->parent() != this) {
		setupTimeoutDialog();
	}
	to_leWho->setText(ul_recent_name);
	timeoutDialog->show();
}

void flist_messenger::to_btnSubmitClicked()
{
	QString who = to_leWho->text();
	QString length = to_leLength->text();
	QString why = to_leReason->text();
	bool ok = true;
	int minutes = length.toInt(&ok);

	if (! ok || minutes <= 0 || 90 < minutes) {
		messageSystem(nullptr, QSL("Wrong length."), MessageType::Feedback);
	}
	else if (who.simplified().isEmpty() || why.simplified().isEmpty()) {
		messageSystem(nullptr, QSL("Didn't fill out all fields."), MessageType::Feedback);
	}
	else {
		account->getSessionByCharacter(charName)->sendCharacterTimeout(who.simplified(), minutes, why.simplified());
	}
	timeoutDialog->hide();
}

void flist_messenger::to_btnCancelClicked()
{
	timeoutDialog->hide();
}

/**
Update the user interface based upon the mode of the current panel.
 */
void flist_messenger::updateChannelMode()
{
	if (currentPanel->getMode() == ChannelMode::Chat ||
	    currentPanel->type() == FChannel::CHANTYPE_PM || 
	    currentPanel->type() == FChannel::CHANTYPE_CONSOLE)
	{
		//Chat only, so disable ability to send ads.
		btnSendAdv->setDisabled(true);
		btnSendChat->setDisabled(false);
	}
	else if (currentPanel->getMode() == ChannelMode::Ads) {
		//Ads only, disable the ability to send regular chat messages.
		btnSendAdv->setDisabled(false);
		btnSendChat->setDisabled(true);
	}
	else {
		//Regular channel, allow both ads and chat messages.
		btnSendAdv->setDisabled(false);
		btnSendChat->setDisabled(false);
	}
}
void flist_messenger::switchTab ( QString& tabname )
{
	if (channelList.count ( tabname ) == 0 && tabname != QSL("CONSOLE") ) {
		printDebugInfo( "ERROR: Tried to switch to " + tabname.toStdString() + " but it doesn't exist.");
		return;
	}

	QString input = plainTextEdit->toPlainText();

	if (currentPanel && currentPanel->type() == FChannel::CHANTYPE_PM && currentPanel->getTypingSelf() == TYPING_STATUS_TYPING )  {
		typingPaused ( currentPanel );
	}

	currentPanel->setInput ( input );

	currentPanel->pushButton->setChecked ( false );
	FChannelPanel* chan = nullptr;

	if (tabname == QSL("CONSOLE"))
		chan = console;
	else
		chan = channelList.value ( tabname );

	currentPanel = chan;
	lblChannelName->setText ( chan->title() );
	input = currentPanel->getInput();
	plainTextEdit->setPlainText ( input );
	plainTextEdit->setFocus();
	currentPanel->setHighlighted ( false );
	currentPanel->setHasNewMessages ( false );
	currentPanel->updateButtonColor();
	currentPanel->pushButton->setChecked ( true );
	refreshUserlist();
	refreshChatLines();
	chatview->verticalScrollBar()->setSliderPosition(chatview->verticalScrollBar()->maximum());
	QTimer::singleShot(0, this, &flist_messenger::scrollChatViewEnd);
	updateChannelMode();
	chatview->setSessionID(currentPanel->getSessionID());
}

/**
A slot used to scroll to the very end of the chat view. This is
intended to be used from a 0 second one shot timer, to ensure that any
widgets that have deferred their resizing have done their business.
 */
void flist_messenger::scrollChatViewEnd()
{
	chatview->verticalScrollBar()->setSliderPosition(chatview->verticalScrollBar()->maximum());
}

void flist_messenger::openPMTab ( QString character )
{
	FSession *session = account->getSession(currentPanel->getSessionID());
	if (character.toLower() == session->character.toLower()) {
		messageSystem(session, QSL("You can't PM yourself!"), MessageType::Feedback);
		return;
	}
	if (!session->isCharacterOnline(character)) {
		messageSystem(session, QSL("That character is not logged in."), MessageType::Feedback);
		return;
	}

	QString panelname = QSL("PM|||%0|||%1").arg(session->getSessionID(), character);

	if (channelList.count ( panelname ) != 0 ) {
		channelList[panelname]->setActive(true);
		channelList[panelname]->pushButton->setVisible(true);
		switchTab ( panelname );
	}
	else {
		channelList[panelname] = new FChannelPanel(this, session->getSessionID(), panelname, character, FChannel::CHANTYPE_PM);
		FCharacter* charptr = session->getCharacter(character);
		QString paneltitle;
		if (charptr != nullptr) {
			paneltitle = charptr->PMTitle();
		}
		else {
			paneltitle = QSL("Private chat with ") + character;
		}
		FChannelPanel* pmPanel = channelList.value(panelname);
		pmPanel->setTitle ( paneltitle );
		pmPanel->setRecipient ( character );
		pmPanel->pushButton = addToActivePanels ( panelname, character, paneltitle );
		switchTab ( panelname );
	}
}

//todo: Move most of this functionality into FSession
void flist_messenger::btnSendAdvClicked()
{
	if (settings->getPlaySounds()) {
		soundPlayer.play ( SoundName::Chat );
	}
	QPlainTextEdit *messagebox = this->findChild<QPlainTextEdit *>(QSL("chatinput"));
	QString inputText = messagebox->toPlainText();
	if (currentPanel == nullptr) {
		printDebugInfo("[CLIENT ERROR] currentPanel == 0");
		return;
	}
	FSession *session = account->getSession(currentPanel->getSessionID());
	if (inputText.isEmpty()) {
		messageSystem(session, QSL("<b>Error:</b> No message."), MessageType::Feedback);
		return;
	}
	if (currentPanel == console || currentPanel->getMode() == ChannelMode::Chat || currentPanel->type() == FChannel::CHANTYPE_PM ) {
		messageSystem(session, QSL("<b>Error:</b> Can't advertise here."), MessageType::Feedback);
		return;
	}
	//todo: Use the configuration stored in the session.
	if (inputText.length() > flist_messenger::BUFFERPUB ) {
		messageSystem(session, QSL("<b>Error:</b> Message exceeds the maximum number of characters."), MessageType::Feedback);
		return;
	}
	plainTextEdit->clear();
	session->sendChannelAdvertisement(currentPanel->getChannelName(), inputText);
}
//todo: Move some of this functionality into the FSession class.
void flist_messenger::parseInput()
{
	if (settings->getPlaySounds()) {
		soundPlayer.play ( SoundName::Chat );
	}

	bool pm = currentPanel->type() == FChannel::CHANTYPE_PM;
	QPlainTextEdit *messagebox = this->findChild<QPlainTextEdit *>(QSL("chatinput"));
	QString inputText = QString ( messagebox->toPlainText() );

	bool isCommand = inputText[0] == '/';

	if (!isCommand && currentPanel == console) { return; }

	FSession *session = account->getSession(currentPanel->getSessionID());

	bool success = false;
	bool plainmessage = false;
	QString ownText = inputText.toHtmlEscaped();
	if (inputText.isEmpty()) {
		messageSystem(session, QSL("<b>Error:</b> No message."), MessageType::Feedback);
		return;
	}
	if (currentPanel == nullptr ) {
		printDebugInfo("[CLIENT ERROR] currentPanel == 0");
		return;
	}

	//todo: Make these read the configuration value from the session.
	int buffermaxlength;
	if (pm) {
		buffermaxlength = flist_messenger::BUFFERPRIV;
	}
	else {
		buffermaxlength = flist_messenger::BUFFERPUB;
	}

	//todo: Should the maximum length only apply to stuff tagged 'plainmessage'?
	if (inputText.length() > buffermaxlength) {
		messageSystem(session, QSL("<B>Error:</B> Message exceeds the maximum number of characters."), MessageType::Feedback);
		return;
	}

	if (isCommand ) {
		QStringList parts = inputText.split ( ' ' );
		QString slashcommand = parts[0].toLower();
		
		if (slashcommand == QSL("/clear") )
		{
			chatview->clear();
			if (currentPanel) {
				currentPanel->clearLines();
			}
			success = true;
		}
		
		else if (slashcommand == QSL("/debug") )
		{
			session->sendDebugCommand(parts[1]);
			success = true;
		}
		
		else if (slashcommand == QSL("/debugrecv")) {
			//Artificially receive a packet from the server. The packet is not validated.
			if (session) {
				session->wsRecv(ownText.mid(11).toStdString());
				success = true;
			}
			else {
				messageSystem(session, QSL("Can't do '/debugrecv', as there is no session associated with this console."), MessageType::Feedback);
			}
		}
		
		else if (slashcommand == QSL("/debugsend")) {
			//Send a packet directly to the server. The packet is not validated.
			if (session) {
				std::string send = ownText.mid(11).toStdString();
				session->wsSend(send);
				success = true;
			}
			else {
				messageSystem(session, QSL("Can't do '/debugsend', as there is no session associated with this console."), MessageType::Feedback);
			}
		}
		
		else if (slashcommand == QSL("/me") ) {
			plainmessage = true;
			success = true;
		}
		
		else if (slashcommand == QSL("/me's") ) {
			plainmessage = true;
			success = true;
		}
		
		else if (slashcommand == QSL("/join")) {
			QString channel = inputText.mid ( 6, -1 ).simplified();
			session->joinChannel(channel);
			success = true;
		}
		
		else if (slashcommand == QSL("/leave") || slashcommand == QSL("/close") )  {
			//todo: Detect other console types.
			if (currentPanel == console || !session) {
				messageSystem(session, QSL("Can't close and leave, as this is a console."), MessageType::Feedback);
				success = false;
			}
			else {
				leaveChannelPanel(currentPanel->getPanelName());
				success = true;
			}
		}
		
		else if (slashcommand == QSL("/status") ) {
			QString status = parts[1];
			QString statusMsg = inputText.mid ( 9 + status.length(), -1 ).simplified();
			changeStatus ( status, statusMsg );
			success = true;
		}
		
		else if (slashcommand == QSL("/users") ) {
			usersCommand();
			success = true;
		}
		
		else if (slashcommand == QSL("/priv") ) {
			QString character = inputText.mid ( 6 ).simplified();
			plainTextEdit->clear();
			openPMTab ( character );
			success = true;
		}
		
		else if (slashcommand == QSL("/ignore") ) {
			QString character = inputText.mid ( 8 ).simplified();
			if (!character.isEmpty()) {
				session->sendIgnoreAdd(character);
				success = true;
			}
		}
		
		else if (slashcommand == QSL("/unignore")) {
			QString character = inputText.mid ( 10 ).simplified();
			if (!character.isEmpty()) {
				if (!session->isCharacterIgnored(character)) {
					QString out = QSL("This character is not in your ignore list.");
					messageSystem(session, out, MessageType::Feedback);
				}
				else {
					session->sendIgnoreDelete(character);
					success = true;
				}
			}
		}
		
		else if (slashcommand == QSL("/ignorelist") ) {
			ignoreDialogRequested();
			success = true;
		}
		
		else if (slashcommand == QSL("/channels") || slashcommand == QSL("/prooms") ) {
			channelsDialogRequested();
			success = true;
		}
		
		else if (slashcommand == QSL("/kick") ) {
			session->kickFromChannel(currentPanel->getChannelName(), inputText.mid(6).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/gkick") ) {
			session->kickFromChat(inputText.mid ( 7 ).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/ban") ) {
			session->banFromChannel(currentPanel->getChannelName(), inputText.mid(5).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/accountban") ) {
			session->banFromChat(inputText.mid(12).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/makeroom") ) {
			createPrivateChannel(inputText.mid(10));
		}
		
		else if (slashcommand == QSL("/closeroom")) {
			session->setRoomIsPublic(currentPanel->getChannelName(), false);
			success = true;
		}
		
		else if (slashcommand == QSL("/openroom")) {
			session->setRoomIsPublic(currentPanel->getChannelName(), true);
			success = true;
		}
		
		else if (slashcommand == QSL("/invite") ) {
			session->inviteToChannel(currentPanel->getChannelName(), inputText.mid(8).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/warn") ) {
			plainmessage = true;
			success = true;
		}
		
		else if (slashcommand == QSL("/cop") ) {
			session->giveChanop(currentPanel->getChannelName(), inputText.mid(5).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/cdeop") ) {
			session->takeChanop(currentPanel->getChannelName(), inputText.mid(7).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/op") ) {
			session->giveGlobalop(inputText.mid(4).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/reward") ) {
			session->giveReward(inputText.mid(8).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/deop") ) {
			session->takeGlobalop(inputText.mid(6).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/code")) {
			QString out;
			switch(currentPanel->type())
			{
			case FChannel::CHANTYPE_NORMAL:
				out = QSL("Copy this code to your message: [b][noparse][channel]%0[/channel][/noparse][/b]").arg(currentPanel->getChannelName());
				break;
			case FChannel::CHANTYPE_ADHOC:
				out = QSL("Copy this code to your message: [b][noparse][session=%0]%1[/session][/noparse][/b]").arg(currentPanel->title(), currentPanel->getChannelName());
				break;
			default:
				out = QSL("This command is only for channels!");
				break;
			}
			messageSystem(session, out, MessageType::Feedback);
			success = true;
		}
		
		else if (slashcommand == QSL("/unban") ) {
			session->unbanFromChannel(currentPanel->getChannelName(), inputText.mid(7).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/banlist") ) {
			session->requestChannelBanList(currentPanel->getChannelName());
			success = true;
		}
		
		else if (slashcommand == QSL("/getdescription")) {
			QString desc = currentPanel->description();
			messageSystem(session, desc, MessageType::Feedback);
			success = true;
		}
		
		else if (slashcommand == QSL("/setdescription") ) {
			// [17:31 PM]>>CDS {"channel":"ADH-cbae3bdf02cd39e8949e","description":":3!"}
			//todo: Does this require more intelligent filtering on excess whitespace?
			session->setChannelDescription(currentPanel->getChannelName(), inputText.mid(16).trimmed());
			success = true;
		}
		
		else if (slashcommand == QSL("/setowner")) {
			QString errmsg;
			FChannel *channel = session->getChannel(currentPanel->getChannelName());

			success = true;
			if (parts.count() < 2) {
				errmsg = QSL("Must supply a character name");
				messageSystem(session, errmsg, MessageType::Feedback);
				success = false;
			}
			else if (!channel) {
				errmsg = QSL("You must be in a channel to set the owner");
				messageSystem(session, errmsg, MessageType::Feedback);
			}
			else {
				QString newOwner = inputText.mid(10);
				session->setChannelOwner(currentPanel->getChannelName(), newOwner);
			}
		}
		
		else if (slashcommand == QSL("/coplist") ) {
			session->requestChanopList(currentPanel->getChannelName());
			success = true;
		}
		
		else if (slashcommand == "/timeout" )
		{
			// [17:16 PM]>>TMO {"time":1,"character":"Arisato Hamuko","reason":"Test."}
			QStringList tparts = inputText.mid ( 9 ).split ( ',' );
			bool isInt;
			int time = tparts[1].simplified().toInt ( &isInt );

			if (isInt == false )
			{
				QString err = QSL("Time is not a number.");
				messageSystem(session, err, MessageType::Feedback);
			}
			else
			{
				QString who(tparts[0].simplified());
				QString why(tparts[2].simplified());
				session->sendCharacterTimeout(who, time, why);
				success = true;
			}
		}
		
		else if (slashcommand == QSL("/gunban") ) {
			session->unbanFromChat(inputText.mid(8).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/createchannel") ) {
			createPublicChannel(inputText.mid(15));
		}
		
		else if (slashcommand == QSL("/killchannel") ) {
			session->killChannel(inputText.mid(13).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/broadcast") ) {
			session->broadcastMessage(inputText.mid(11).simplified());
			success = true;
		}
		
		else if (slashcommand == QSL("/setmode") ) {
			ChannelMode mode = ChannelMode::Unknown;
			if (parts.length() == 2) {
				mode = keyToEnum<ChannelMode>(parts[1]);
			}
			if (mode == ChannelMode::Unknown) {
				QString err;
				messageSystem(session, QSL("Correct usage: /setmode &lt;chat|ads|both&gt;"), MessageType::Feedback);
			}
			else if (currentPanel->isOp(session->getCharacter(session->character)) || session->isCharacterOperator(session->character)) {
				session->setChannelMode(currentPanel->getChannelName(), mode);
				success = true;
			}
			else {
				messageSystem(session, QSL("You can only do that in channels you moderate."), MessageType::Feedback);
				success = true;
			}
		}
		
		else if (slashcommand == QSL("/bottle") ) {
			if (currentPanel == nullptr
			        || currentPanel->type() == FChannel::CHANTYPE_CONSOLE 
			        || currentPanel->type() == FChannel::CHANTYPE_PM)
			{
				messageSystem(session, QSL("You can't use that in this panel."), MessageType::Feedback);
			}
			else {
				session->spinBottle(currentPanel->getChannelName());
			}
			success = true;
		}
		
		else if (slashcommand == QSL("/roll") ) {
			if (currentPanel == nullptr || currentPanel->type() == FChannel::CHANTYPE_CONSOLE) {
				messageSystem(session, QSL("You can't use that in this panel."), MessageType::Feedback);
			}
			else {
				QString roll;
				if (parts.count() < 2 ) {
					roll = QSL("1d10");
				}
				else {
					roll = parts[1].toLower();
				}
				if (currentPanel->type() == FChannel::CHANTYPE_PM) {
					session->rollDicePM(currentPanel->recipient(), roll);
				}
				else {
					session->rollDiceChannel(currentPanel->getChannelName(), roll);
				}
			}
			success = true;
		}
		
		else if (debugging && slashcommand == QSL("/channeltojson")) {
			QString output = QSL("[noparse]%0[/noparse]");
			JSONNode* node = currentPanel->toJSON();
			messageSystem(session, output.arg(node->write().c_str()), MessageType::Feedback);
			delete node;
			success = true;
		}
		
		else if (debugging && slashcommand == QSL("/refreshqss")) {
			QFile stylefile(QSL("default.qss"));
			stylefile.open(QFile::ReadOnly);
			QString stylesheet = QLatin1String(stylefile.readAll());
			setStyleSheet(stylesheet);
			messageSystem(session, QSL("Refreshed stylesheet from default.qss"), MessageType::Feedback);
			success = true;
		}
		
		else if (slashcommand == QSL("/channeltostring")) {
			QString* output = currentPanel->toString();
			messageSystem(session, *output, MessageType::Feedback);
			success = true;
			delete output;
		}
	}
	else {
		plainmessage = true;
		success = true;
	}

	if (success) {
		plainTextEdit->clear();
	}

	if (plainmessage) {
		if (pm) {
			session->sendCharacterMessage(currentPanel->recipient(), inputText);
		}
		else {
			session->sendChannelMessage(currentPanel->getChannelName(), inputText);
		}
	}
}

void flist_messenger::saveSettings()
{
	FChannelPanel* c;
	defaultChannels.clear();
	foreach(c, channelList) {
		if (c->getActive() && c->type() != FChannel::CHANTYPE_CONSOLE && c->type() != FChannel::CHANTYPE_PM) {
			defaultChannels.append(c->getChannelName());
		}
	}
	settings->setDefaultChannels(defaultChannels.join(QSL("|||")));
}

void flist_messenger::loadSettings()
{
	account->setUserName(settings->getUserAccount());
	defaultChannels.clear();
	defaultChannels = settings->getDefaultChannels().split(QSL("|||"), QString::SkipEmptyParts);

	keywordlist = settings->qsettings->value(QSL("Global/keywords")).toString().split(QSL(","), QString::SkipEmptyParts);
	for(int i = 0; i < keywordlist.size(); i++) {
		keywordlist[i] = keywordlist[i].trimmed();
		if (keywordlist[i].isEmpty()) {
			keywordlist.removeAt(i);
			i--;
		}
	}
}

void flist_messenger::loadDefaultSettings()
{
}

#define PANELNAME(channelname,charname)  (((channelname).startsWith(QSL("ADH-")) ? QSL("ADH|||") : QSL("CHAN|||")) + (charname) + "|||" + (channelname))

void flist_messenger::flashApp(QString& reason)
{
	printDebugInfo(reason.toStdString());
	QApplication::alert(this, 10000);
}

FSession *flist_messenger::getSession(QString sessionid)
{
	return server->getSession(sessionid);
}

void flist_messenger::setChatOperator(FSession *session, QString characteroperator, bool opstatus)
{
	debugMessage((opstatus ? "Added chat operator: " : "Removed chat operator: ") + characteroperator);
	// Sort userlists that contain this character
	if (session->isCharacterOnline(characteroperator)) {
		// Set flag in character
		FCharacter* character = session->getCharacter(characteroperator);
		FChannelPanel* channel = nullptr;
		foreach(channel, channelList) {
			//todo: filter by session
			if (channel->charList().contains(character)) {
				//todo: Maybe queue channel sorting as an idle task?
				channel->sortChars();
				if (currentPanel == channel) {
					refreshUserlist();
				}
			}
		}
	}
}
void flist_messenger::openCharacterProfile(FSession *session, QString charactername)
{
	(void) session;
	ul_recent_name = charactername;
	characterInfoDialogRequested();
}

void flist_messenger::addCharacterChat(FSession *session, QString charactername)
{
	QString panelname = QSL("PM|||%0|||%1").arg(session->getSessionID(), charactername);
	FChannelPanel *channelpanel = channelList.value(panelname);
	if (!channelpanel) {
		channelpanel = new FChannelPanel(this, session->getSessionID(), panelname, charactername, FChannel::CHANTYPE_PM);
		channelList[panelname] = channelpanel;
		channelpanel->setTitle(charactername);
		channelpanel->setRecipient(charactername);
		FCharacter *character = session->getCharacter(charactername);
		QString title;
		if (character) {
			title = character->PMTitle();
		}
		else {
			title = QSL("Private chat with %1 (Offline)").arg(charactername);
		}
		channelpanel->pushButton = addToActivePanels(panelname, charactername, title);
	}
	if (channelpanel->getActive() == false) {
		channelpanel->setActive(true);
		channelpanel->pushButton->setVisible(true);
	}
	channelpanel->setTyping(TYPING_STATUS_CLEAR);
	channelpanel->updateButtonColor();
}

void flist_messenger::addChannel(FSession *session, QString channelname, QString title)
{
	debugMessage("addChannel(\"" + channelname + "\", \"" + title + "\")");
	FChannelPanel* channelpanel;
	QString panelname = PANELNAME(channelname, session->getSessionID());
	if (!channelList.contains(panelname)) {
		if (channelname.startsWith(QSL("ADH-"))) {
			channelpanel = new FChannelPanel(this, session->getSessionID(), panelname, channelname, FChannel::CHANTYPE_ADHOC);
		}
		else {
			channelpanel = new FChannelPanel(this, session->getSessionID(), panelname, channelname, FChannel::CHANTYPE_NORMAL);
		}
		channelList[panelname] = channelpanel;
		channelpanel->setTitle(title);
		channelpanel->pushButton = addToActivePanels(panelname, channelname, title);
	}
	else {
		channelpanel = channelList.value(panelname);
		//Ensure that the channel's title is set correctly for ad-hoc channels.
		if (channelname != title && channelpanel->title() != title) {
			channelpanel->setTitle(title);
		}
		if (channelpanel->getActive() == false) {
			channelpanel->setActive(true);
			channelpanel->pushButton->setVisible(true);
		}
	}
	//todo: Update UI elements with base on channel mode? (chat/RP AD/both)
}

// TODO: Should this be noop?
void flist_messenger::removeChannel(FSession *session, QString channelname)
{
	(void)session; (void) channelname;
}

void flist_messenger::addChannelCharacter(FSession *session, QString channelname, QString charactername, bool notify)
{
	FChannelPanel* channelpanel;
	QString panelname = PANELNAME(channelname, session->getSessionID());
	if (!session->isCharacterOnline(charactername)) {
		printDebugInfo("[SERVER BUG]: Server told us about a character joining a channel, but we don't know about them yet. " + charactername.toStdString());
		return;
	}
	if (!channelList.contains(panelname)) {
		printDebugInfo("[BUG]: Told about a character joining a channel, but the panel for the channel doesn't exist. " + channelname.toStdString());
		return;
	}
	channelpanel = channelList.value(panelname);
	channelpanel->addChar(session->getCharacter(charactername), notify);
	if (charactername == session->character) {
		switchTab(panelname);
	}
	else {
		if (notify) {
			if (currentPanel->getChannelName() == channelname) {
				//Only refresh the userlist if the panel has focus and notify is set.
				refreshUserlist();
			}
			QString msg = QSL("<b>%0</b> has joined the channel").arg(charactername);
			messageChannel(session, channelname, msg, MessageType::Join);
		}
	}
}

void flist_messenger::removeChannelCharacter(FSession *session, QString channelname, QString charactername)
{
	FChannelPanel* channelpanel;
	QString panelname = PANELNAME(channelname, session->getSessionID());
	if (!session->isCharacterOnline(charactername)) {
		printDebugInfo("[SERVER BUG]: Server told us about a character leaving a channel, but we don't know about them yet. " + charactername.toStdString());
		return;
	}
	if (!channelList.contains(panelname)) {
		printDebugInfo("[BUG]: Told about a character leaving a channel, but the panel for the channel doesn't exist. " + channelname.toStdString());
		return;
	}
	channelpanel = channelList.value(panelname);
	channelpanel->remChar(session->getCharacter(charactername));
	if (currentPanel->getChannelName() == channelname) {
		refreshUserlist();
	}
	QString msg = QSL("<b>%0</b> has left the channel").arg(charactername);
	messageChannel(session, channelname, msg, MessageType::Leave);
}

void flist_messenger::setChannelOperator(FSession *session, QString channelname, QString charactername, bool opstatus)
{
	QString panelname = PANELNAME(channelname, session->getSessionID());
	FChannelPanel *channelpanel = channelList.value(panelname);
	if (channelpanel) {
		if (opstatus) {
			channelpanel->addOp(charactername);
		}
		else {
			channelpanel->removeOp(charactername);
		}
		if (currentPanel == channelpanel) {
			refreshUserlist();
		}
	}
}

void flist_messenger::joinChannel(FSession *session, QString channelname)
{
	(void) session;
	if (currentPanel->getChannelName() == channelname) {
		refreshUserlist();
	}
	//todo: Refresh any elements in a half prepared state.
}

/**
This notifies the UI that the given session has now left the given channel. The UI should close or disable the releavent widgets. It should not send any leave commands.
 */
void flist_messenger::leaveChannel(FSession *session, QString channelname)
{
	QString panelname = PANELNAME(channelname, session->getSessionID());
	if (!channelList.contains(panelname)) {
		printDebugInfo("[BUG]: Told to leave a channel, but the panel for the channel doesn't exist. " + channelname.toStdString());
		return;
	}
	closeChannelPanel(panelname);
}

void flist_messenger::setChannelDescription(FSession *session, QString channelname, QString description)
{
	QString panelname = PANELNAME(channelname, session->getSessionID());
	FChannelPanel *channelpanel = channelList.value(panelname);
	if (!channelpanel) {
		printDebugInfo(QSL("[BUG]: Was told the description of the channel '%1', but the panel for the channel doesn't exist.").arg(channelname).toStdString());
		return;
	}
	channelpanel->setDescription(description);
	QString message = QSL("You have joined <b>%1</b>: %2").arg(channelpanel->title(), bbparser.parse(description));
	messageChannel(session, channelname, message, MessageType::ChannelDescription, true, false);
}

void flist_messenger::setChannelMode(FSession *session, QString channelname, ChannelMode mode)
{
	QString panelname = PANELNAME(channelname, session->getSessionID());
	FChannelPanel *channelpanel = channelList.value(panelname);
	if (!channelpanel) {
		printDebugInfo(QSL("[BUG]: Was told the mode of the channel '%1', but the panel for the channel doesn't exist.").arg(channelname).toStdString());
		return;
	}
	channelpanel->setMode(mode);
	if (channelpanel == currentPanel) {
		//update UI with mode state
		updateChannelMode();
	}
}

/**
The initial flood of channel data is complete and delayed tasks like sorting can now be performed.
 */
void flist_messenger::notifyChannelReady(FSession *session, QString channelname)
{
	QString panelname = PANELNAME(channelname, session->getSessionID());
	FChannelPanel *channelpanel = channelList.value(panelname);
	if (!channelpanel) {
		printDebugInfo(QSL("[BUG]: Was notified that the channel '%1' was ready, but the panel for the channel doesn't exist.").arg(channelname).toStdString());
		return;
	}
	channelpanel->sortChars();
	if (currentPanel == channelpanel) {
		refreshUserlist();
	}
}

void flist_messenger::notifyCharacterOnline(FSession *session, QString charactername, bool online)
{
	QString panelname = QSL("PM|||%0|||%1").arg(session->getSessionID(), charactername);
	QList<QString> channels;
	QList<QString> characters;
	bool system = session->isCharacterFriend(charactername);
	if (channelList.contains(panelname)) {
		characters.append(charactername);
		system = true;
		//todo: Update panel with changed online/offline status.
	}
	if (characters.count() > 0 || channels.count() > 0 || system) {
		QString msg = QSL("<b>%0</b> is now %1.").arg(charactername, (online ? QSL("online") : QSL("offline")));
		messageMany(session, channels, characters, system, msg, online ? MessageType::Online : MessageType::Offline);
	}
}

void flist_messenger::notifyCharacterStatusUpdate(FSession *session, QString charactername)
{
	QString panelname = QSL("PM|||%0|||%1").arg(session->getSessionID(), charactername);
	QList<QString> channels;
	QList<QString> characters;
	bool system = session->isCharacterFriend(charactername);
	if (channelList.contains(panelname)) {
		characters.append(charactername);
		system = true;
		//todo: Update panel with changed status.
	}
	if (characters.count() > 0 || channels.count() > 0 || system) {
		FCharacter *character = session->getCharacter(charactername);
		QString statusmessage =  character->statusMsg();
		if (!statusmessage.isEmpty()) {
			statusmessage = QSL(" (%1)").arg(statusmessage);
		}
		QString message = QString("<b>%1</b> is now %2%3").arg(charactername, character->statusString(), statusmessage);
		messageMany(session, channels, characters, system, message, MessageType::Status);
	}
	//Refresh character list if they are present in the current panel.
	if (currentPanel->hasCharacter(session->getCharacter(charactername))) {
		refreshUserlist();
	}
}

void flist_messenger::setCharacterTypingStatus(FSession *session, QString charactername, TypingStatus typingstatus)
{
	QString panelname = QSL("PM|||%0|||%1").arg(session->getSessionID(), charactername);
	FChannelPanel *channelpanel;
	channelpanel = channelList.value(panelname);
	if (!channelpanel) {
		return;
	}
	channelpanel->setTyping(typingstatus);
	channelpanel->updateButtonColor();
}

void flist_messenger::notifyCharacterCustomKinkDataUpdated(FSession *session, QString charactername)
{
	if (!ci_dialog) {
		debugMessage(QSL("Received custom kink data for the character '%1' but the profile window has not been created.").arg(charactername));
		ci_dialog = new FCharacterInfoDialog(this);
	}
	FCharacter *character = session->getCharacter(charactername);
	if (!character) {
		debugMessage(QSL("Received custom kink data for the character '%1' but the character is not known.").arg(charactername));
		return;
	}
	ci_dialog->updateKinks(character);
}
void flist_messenger::notifyCharacterProfileDataUpdated(FSession *session, QString charactername)
{
	if (!ci_dialog) {
		debugMessage(QSL("Received profile data for the character '%1' but the profile window has not been created.").arg(charactername));
		ci_dialog = new FCharacterInfoDialog(this);
	}
	FCharacter *character = session->getCharacter(charactername);
	if (!character) {
		debugMessage(QSL("Received profile data for the character '%1' but the character is not known.").arg(charactername));
		return;
	}

	ci_dialog->updateProfile(character);
}

void flist_messenger::notifyIgnoreAdd(FSession *s, QString character)
{
	messageSystem(s, QSL("%1 has been added to your ignore list.").arg(character), MessageType::IgnoreUpdate);
}

void flist_messenger::notifyIgnoreRemove(FSession *s, QString character)
{
	messageSystem(s, QSL("%1 has been removed from your ignore list.").arg(character), MessageType::IgnoreUpdate);
}

//todo: Making channelkey should be moved out to messageMessage().
bool flist_messenger::getChannelBool(QString key, FChannelPanel *channelpanel, bool dflt)
{
	QString channelkey;
	if (channelpanel->type() == FChannel::CHANTYPE_PM) {
		channelkey = QSL("Character/%1/%2").arg(channelpanel->recipient(), key);
	}
	else {
		channelkey = QSL("Character/%1/%2").arg(channelpanel->getChannelName(), key);
	}
	return settings->getBool(channelkey, dflt);
}
bool flist_messenger::needsAttention(QString key, FChannelPanel *channelpanel, AttentionMode dflt)
{
	//todo: check settings from the channel itself
	AttentionMode attentionmode;
	QString channelkey;
	if (channelpanel->type() == FChannel::CHANTYPE_PM) {
		channelkey = QSL("Character/%1/%2").arg(channelpanel->recipient(), key);
	}
	else {
		channelkey = QSL("Character/%1/%2").arg(channelpanel->getChannelName(), key);
	}
	attentionmode = keyToEnum(settings->getString(channelkey), AttentionMode::Default);
	
	if (attentionmode == AttentionMode::Default) {
		attentionmode = keyToEnum(settings->getString(QSL("Global/") + key), AttentionMode::Default);
	}
	
	if (attentionmode == AttentionMode::Default) {
		attentionmode = dflt;
	}
	
	switch(attentionmode) {
	case AttentionMode::Default:
	case AttentionMode::Never:
		return false;
	case AttentionMode::IfNotFocused:
		if (channelpanel == currentPanel) {
			return false;
		}
		return true;
	case AttentionMode::Always:
		return true;
	default:
		debugMessage("Nonsensical attention mode in settings!");
		return false;
	}
}

void flist_messenger::messageMessage(FMessage message)
{
	QStringList panelnames;
	QString sessionid = message.getSessionID();
	FSession *session = getSession(sessionid);
	//bool destinationchannelalwaysding = false; //1 or more destination channels that are set to always ding
	
	bool message_ding = false;
	bool message_flash = false;
	SoundName soundtype = SoundName::None;
	
	QString panelname;
	bool globalkeywordmatched = false;
	QString plaintext = message.getPlainTextMessage();
	
	switch(message.getMessageType()) {
	case MessageType::DiceRoll:
	case MessageType::RpAd:
	case MessageType::Chat:
		foreach(QString keyword, keywordlist) {
			if (plaintext.contains(keyword, Qt::CaseInsensitive)) {
				globalkeywordmatched = true;
				break;
			}
		}
		break;
	default:
		break;
	}

	if (!session) {
		debugMessage("[CLIENT BUG] Sessionless messages are not handled yet.");
		//todo: Sanity check that character and  channel lists are empty
		//todo: check console
		//todo: check notify
	}
	else {
		if (message.getBroadcast()) {
			//Doing a broadcast, find all panels for this session and flag them.
			foreach(FChannelPanel *channelpanel, channelList) {
				if (channelpanel->getSessionID() == sessionid) {
					panelnames.append(channelpanel->getPanelName());
				}
			}
		}
		else {
			foreach(QString charactername, message.getDestinationCharacterList()) {
				QString panelname = QSL("PM|||%0|||%1").arg(sessionid, charactername);
				panelnames.append(panelname);
			}
			foreach(QString channelname, message.getDestinationChannelList()) {
				panelname = PANELNAME(channelname, sessionid);
				panelnames.append(panelname);
			}
			if (message.getConsole()) {
				panelnames.append(QSL("FCHATSYSTEMCONSOLE"));
			}
		}
	}
	if (message.getNotify()) {
		//todo: should this be made session aware?
		if (!panelnames.contains(currentPanel->getPanelName())) {
			panelnames.append(currentPanel->getPanelName());
		}
	}
	foreach(QString panelname, panelnames) {
		FChannelPanel *channelpanel;
		channelpanel = channelList.value(panelname);
		if (!channelpanel) {
			debugMessage("[BUG] Tried to put a message on '" + panelname + "' but there is no channel panel for it. message:" + message.getFormattedMessage());
			continue;
		}
		//Filter based on message type.
		switch(message.getMessageType()) {
		case MessageType::Online:
		case MessageType::Offline:
		case MessageType::Status:
			if (!settings->getShowOnlineOfflineMessage()) {
				continue;
			}
			break;
		case MessageType::Join:
		case MessageType::Leave:
			if (!settings->getShowJoinLeaveMessage()) {
				continue;
			}
			break;
		case MessageType::DiceRoll:
		case MessageType::RpAd:
		case MessageType::Chat:
			switch(message.getMessageType()) {
			case MessageType::DiceRoll:
				//todo: should rolls treated like ads or messages or as their own thing?
			case MessageType::RpAd:
				message_ding |= needsAttention(QSL("message_rpad_ding"), channelpanel, AttentionMode::Never);
				message_flash |= needsAttention(QSL("message_rpad_flash"), channelpanel, AttentionMode::Never);
				break;
			case MessageType::Chat:
				if (channelpanel->type() == FChannel::CHANTYPE_PM) {
					message_ding |= needsAttention(QSL("message_character_ding"), channelpanel, AttentionMode::Always);
					message_flash |= needsAttention(QSL("message_character_flash"), channelpanel, AttentionMode::Never);
				}
				else {
					message_ding |= needsAttention(QSL("message_channel_ding"), channelpanel, AttentionMode::Never);
					message_flash |= needsAttention(QSL("message_channel_flash"), channelpanel, AttentionMode::Never);
				}
				break;
			default:
				break;
			}
			//Per channel keyword matching.
			foreach(QString keyword, channelpanel->getKeywordList()) {
				if (plaintext.contains(keyword, Qt::CaseInsensitive)) {
					message_ding |= true;
					message_flash |= true;
					break;
				}
			}
			//Can it match global keywords?
			if (globalkeywordmatched && !getChannelBool(QSL("ignore_global_keywords"), channelpanel, false)) {
				message_ding |= true;
				message_flash |= true;
			}
			channelpanel->setHasNewMessages(true);
			if (panelname.startsWith(QSL("PM"))) {
				channelpanel->setHighlighted(true);
			}
			channelpanel->updateButtonColor();
			break;
		case MessageType::Report:
			soundtype = SoundName::ModAlert;
			break;
		case MessageType::Broadcast:
			soundtype = SoundName::Attention;
			break;
		case MessageType::Login:
		case MessageType::ChannelDescription:
		case MessageType::ChannelMode:
		case MessageType::ChannelInvite:
		case MessageType::Error:
		case MessageType::System:
		case MessageType::Feedback:
		case MessageType::Kick:
		case MessageType::KickBan:
		case MessageType::IgnoreUpdate:
			break;
		default:
			debugMessage("Unhandled message type " + enumToKey(message.getMessageType()) + " for message '" + message.getFormattedMessage() + "'.");
		}
		channelpanel->addLine(message.getFormattedMessage(), settings->getLogChat());
		if (channelpanel == currentPanel) {
			chatview->append(message.getFormattedMessage());
		}
	}

	if (message_ding) {
		if (session && message.getSourceCharacter() != session->character) {
			soundtype = SoundName::Attention;
		}
	}

	if (soundtype != SoundName::None && settings->getPlaySounds()) {
		soundPlayer.play(soundtype);
	}
	if (message_flash) {
		//todo: Special handling on message type?
		QString reason(message.getFormattedMessage());
		flashApp(reason);
	}
}

void flist_messenger::messageMany(QList<QString> &panelnames, QString message, MessageType messagetype)
{
	//Put the message on all the given channel panels.
	QString panelname;
	QString messageout = QSL("<small>%0]</small> %1").arg(QTime::currentTime().toString("hh:mm:ss AP"), message);
	foreach(panelname, panelnames) {
		FChannelPanel *channelpanel;
		channelpanel = channelList.value(panelname);
		if (!channelpanel) {
			debugMessage("[BUG] Tried to put a message on '" + panelname + "' but there is no channel panel for it. message:" + message);
			continue;
		}
		//Filter based on message type.
		switch(messagetype) {
		case MessageType::Login:
			break;
		case MessageType::Online:
		case MessageType::Offline:
		case MessageType::Status:
			if (!settings->getShowOnlineOfflineMessage()) {
				continue;
			}
			break;
		case MessageType::ChannelDescription:
		case MessageType::ChannelMode:
		case MessageType::ChannelInvite:
			break;
		case MessageType::Join:
		case MessageType::Leave:
			if (!settings->getShowJoinLeaveMessage()) {
				continue;
			}
			break;
		case MessageType::DiceRoll:
		case MessageType::RpAd:
		case MessageType::Chat:
			//todo: trigger sounds
			channelpanel->setHasNewMessages(true);
			if (panelname.startsWith(QSL("PM"))) {
				channelpanel->setHighlighted(true);
			}
			channelpanel->updateButtonColor();
			break;
		case MessageType::Report:
		case MessageType::Error:
		case MessageType::System:
		case MessageType::Broadcast:
		case MessageType::Feedback:
		case MessageType::Kick:
		case MessageType::KickBan:
		case MessageType::IgnoreUpdate:
			break;
		default:
			debugMessage("Unhandled message type " + enumToKey(messagetype) + " for message '" + message + "'.");
		}
		channelpanel->addLine(messageout, true);
		if (channelpanel == currentPanel) {
			chatview->append(messageout);
		}
	}
	//todo: Sound support is still less than what it was originally.
	if (/*se_ping &&*/ settings->getPlaySounds()) {
		switch(messagetype) {
		case MessageType::Login:
		case MessageType::Online:
		case MessageType::Offline:
		case MessageType::Status:
		case MessageType::ChannelDescription:
		case MessageType::ChannelMode:
		case MessageType::ChannelInvite:
		case MessageType::Join:
		case MessageType::Leave:
			break;
		case MessageType::DiceRoll:
		case MessageType::RpAd:
		case MessageType::Chat:
			soundPlayer.play(SoundName::Chat);
			break;
		case MessageType::Report:
			soundPlayer.play(SoundName::ModAlert);
			break;
		case MessageType::Error:
		case MessageType::System:
		case MessageType::Broadcast:
			soundPlayer.play(SoundName::Attention);
			break;
		case MessageType::Feedback:
		case MessageType::Kick:
		case MessageType::KickBan:
		case MessageType::IgnoreUpdate:
			break;
		case MessageType::Note:
			soundPlayer.play(SoundName::NewNote);
			break;
		case MessageType::Friend:
			soundPlayer.play(SoundName::FriendUpdate);
			break;
		case MessageType::Bookmark:
			break;
		default:
			debugMessage("Unhandled sound for message type " + enumToKey(messagetype) + " for message '" + message + "'.");
		}
	}
	//todo: flashing the window/panel tab
}
void flist_messenger::messageMany(FSession *session, QList<QString> &channels, QList<QString> &characters, bool system, QString message, MessageType messagetype)
{
	QList<QString> panelnames;
	QString charactername;
	QString channelname;
	QString panelnamemidfix = "|||" + session->getSessionID() + "|||";
	if (system) {
		//todo: session based consoles?
		panelnames.append(QSL("FCHATSYSTEMCONSOLE"));
	}
	foreach(charactername, characters) {
		panelnames.append(QSL("PM|||%0|||%1").arg(session->getSessionID(), charactername));
	}
	foreach(channelname, channels) {
		// TODO: can channel names start with anything else?
		if (channelname.startsWith(QSL("ADH-"))) {
			panelnames.append(QSL("ADH|||%0|||%1").arg(session->getSessionID(), charactername));
		}
		else {
			panelnames.append(QSL("CHAN|||%0|||%1").arg(session->getSessionID(), charactername));
		}
	}
	if (system) {
		if (!panelnames.contains(currentPanel->getPanelName())) {
			panelnames.append(currentPanel->getPanelName());
		}
	}
	messageMany(panelnames, message, messagetype);
}

void flist_messenger::messageAll(FSession *session, QString message, MessageType messagetype)
{
	QList<QString> panelnames;
	FChannelPanel *channelpanel;
	//todo: session based consoles?
	panelnames.append(QSL("FCHATSYSTEMCONSOLE"));
	QString match = QSL("|||%0|||").arg(session->getSessionID());
	//Extract all panels that are relevant to this session.
	foreach(channelpanel, channelList) {
		QString panelname = channelpanel->getPanelName();
		if (panelname.contains(match)) {
			panelnames.append(panelname);
		}
	}
	messageMany(panelnames, message, messagetype);
}

void flist_messenger::messageChannel(FSession *session, QString channelname, QString message, MessageType messagetype, bool console, bool notify)
{
	QList<QString> panelnames;
	panelnames.append(PANELNAME(channelname, session->getSessionID()));
	if (console) {
		panelnames.append(QSL("FCHATSYSTEMCONSOLE"));
	}
	if (notify) {
		QString panelname = currentPanel->getPanelName();
		if (!panelnames.contains(panelname)) {
			panelnames.append(panelname);
		}
	}
	messageMany(panelnames, message, messagetype);
}

void flist_messenger::messageCharacter(FSession *session, QString charactername, QString message, MessageType messagetype)
{
	QList<QString> panelnames;
	panelnames.append(QSL("PM|||%0|||%1").arg(session->getSessionID(), charactername));
	messageMany(panelnames, message, messagetype);
}

void flist_messenger::messageSystem(FSession *session, QString message, MessageType messagetype)
{
	(void) session; //todo: session based consoles?
	QList<QString> panelnames;
	panelnames.append(QSL("FCHATSYSTEMCONSOLE"));
	if (currentPanel && !panelnames.contains(currentPanel->getPanelName())) {
		panelnames.append(currentPanel->getPanelName());
	}
	messageMany(panelnames, message, messagetype);
}

void flist_messenger::updateKnownChannelList(FSession *session)
{
	cl_data->updateChannels(
				session->knownchannellist.begin(),
				session->knownchannellist.end());
}
void flist_messenger::updateKnownOpenRoomList(FSession *session)
{
	cl_data->updateRooms(
				session->knownopenroomlist.begin(),
				session->knownopenroomlist.end()
				);
}

void flist_messenger::createPublicChannel(QString name)
{
	account->getSession(charName)->createPublicChannel(name);
}

void flist_messenger::createPrivateChannel(QString name)
{
	account->getSession(charName)->createPrivateChannel(name);
}
