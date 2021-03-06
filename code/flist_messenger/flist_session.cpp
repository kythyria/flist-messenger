
#include <QTime>
#include <QtWebSockets/QWebSocket>

#include "flist_session.h"
#include "flist_account.h"
#include "flist_global.h"
#include "flist_server.h"
#include "flist_character.h"
#include "flist_iuserinterface.h"
#include "flist_channel.h"
#include "flist_parser.h"
#include "flist_message.h"


#include "../libjson/libJSON.h"

FSession::FSession(FAccount *account, QString &character, QObject *parent) :
	QObject(parent),
	account(account),
	sessionid(character),
	character(character),
    socket(nullptr),
	characterlist(),
	friendslist(),
	bookmarklist(),
	operatorlist(),
	ignorelist(),
	channellist(),
	joinQueue(),
	autojoinchannels(),
	servervariables(),
	knownchannellist(),
	knownopenroomlist()
{
}

FSession::~FSession()
{
	if(socket) {
		socket->abort();
		socket->deleteLater();
	}
}

FCharacter *FSession::addCharacter(QString name)
{
	FCharacter *character = getCharacter(name);
	if(!character) {
		character = new FCharacter(name, friendslist.contains(name));
		characterlist[name] = character;
	}
	return character;
}

void FSession::removeCharacter(QString name)
{
	//Get and remove the character from the list.
	FCharacter *character = characterlist.take(name);
	//Delete if not null.
	if(character) {
		delete character;
	}
}

/**
Convert a character's name into a formated hyperlinked HTML text. It will use the correct colors if they're known.
 */
QString FSession::getCharacterHtml(QString name) {
	QString html = "<b><a style=\"color: %1\" href=\"%2\">%3</a></b>";
	FCharacter *character = getCharacter(name);
	if(character) {
		html = html
			.arg(character->genderColor().name())
			.arg(character->getUrl())
			.arg(name);
	} else {
		html = html
			.arg(FCharacter::genderColors[FCharacter::GENDER_OFFLINE_UNKNOWN].name())
			.arg(getCharacterUrl(name))
			.arg(name);
	}
	return html;
}


/**
Tell the server that we wish to join the given channel, but only have one such request in flight
at a time. The rest of the logic is in processQueue(), called from the ERR and CDS handlers.
 */
void FSession::joinChannel(QString name)
{
	if(joinQueue.empty())
	{
		JSONNode joinnode;
		joinnode.push_back(JSONNode("channel", name.toStdString()));
		wsSend("JCH", joinnode);
	}
	joinQueue.enqueue(name);
}

void FSession::processJoinQueue()
{
	joinQueue.dequeue();
	if(!joinQueue.empty())
	{
		QString chan = joinQueue.head();
		JSONNode joinnode;
		joinnode.push_back(JSONNode("channel", chan.toStdString()));
		wsSend("JCH", joinnode);
	}
}

void FSession::createPublicChannel(QString name)
{
	// [0:59 AM]>>CRC {"channel":"test"}
	JSONNode node;
	JSONNode channode ( "channel", name.toStdString() );
	node.push_back ( channode );
	std::string out = "CRC " + node.write();
	wsSend( out );
}
void FSession::createPrivateChannel(QString name)
{
	// [17:24 PM]>>CCR {"channel":"abc"}
	JSONNode makenode;
	JSONNode namenode ( "channel", name.toStdString() );
	makenode.push_back ( namenode );
	std::string out = "CCR " + makenode.write();
	wsSend ( out );
}

FChannel *FSession::addChannel(QString name, QString title)
{
	FChannel *channel;
	if(channellist.contains(name)) {
		channel = channellist[name];
		//Ensure that the channel's title is set correctly for ad-hoc channels.
		if(name != title && channel->getTitle() != title) {
			channel->setTitle(title);
		}
		return channel;
	}
	channel = new FChannel(this, this, name, title);
	channellist[name] = channel;
	return channel;
	
}
FChannel *FSession::getChannel(QString name)
{
	return channellist.contains(name) ? channellist[name] : 0;
}


//todo: All the web socket stuff should really go into its own class.
void FSession::connectSession()
{
	debugMessage("session->connectSession()");
	if(socket) {
		return;
	}
	
	socket = new QWebSocket("file:///", QWebSocketProtocol::VersionLatest, this);
	connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &FSession::socketError);
	connect(socket, &QWebSocket::sslErrors, this, &FSession::socketSslError);
	connect(socket, &QWebSocket::connected, this, &FSession::socketConnected);
	connect(socket, &QWebSocket::textMessageReceived, this, &FSession::socketReceivedTextMessage);
	socket->open(QUrl(account->server->chatserver_url));
	debugMessage("Connecting...");
}

void FSession::socketConnected()
{
	debugMessage("Connected.");
	
	JSONNode loginnode;
	loginnode.push_back(JSONNode("method", "ticket"));
	loginnode.push_back(JSONNode("ticket", account->ticket.toStdString()));
	loginnode.push_back(JSONNode("account", account->getUserName().toStdString()));
	loginnode.push_back(JSONNode("cname", FLIST_CLIENTID));
	loginnode.push_back(JSONNode("cversion", FLIST_VERSIONNUM));
	loginnode.push_back(JSONNode("character", character.toStdString()));
	std::string idenStr = "IDN " + loginnode.write();
	
	QString msg = QString::fromStdString(idenStr);
	
	socket->sendTextMessage(msg);
}

void FSession::socketError(QAbstractSocket::SocketError error)
{
	emit socketErrorSignal(error);
	if(socket) {
		socket->abort();
		socket->deleteLater();
		socket = nullptr;
	}
}

void FSession::socketSslError(QList<QSslError> sslerrors)
{
	//QMessageBox msgbox;
	QString errorstring;
	foreach(const QSslError &error, sslerrors) {
		if(!errorstring.isEmpty()) {
			errorstring += ", ";
		}
		errorstring += error.errorString();
	}
	//msgbox.critical ( this, "SSL ERROR DURING LOGIN!", errorstring );
	//messageSystem(0, errorstring, MessageType::Error);
	QString message = QString("SSL Socket Error: %1").arg(errorstring);
	//todo: This should really display message box.
	account->ui->messageSystem(this, message, MessageType::Error);
	debugMessage(message);
}

void FSession::socketReceivedTextMessage(const QString &message) {
	wsRecv(message.toStdString());
}

void FSession::wsSend(const char *command)
{
	std::string cmd = command;
	wsSend(cmd);
}

void FSession::wsSend(const char *command, JSONNode &nodes)
{
	std::string cmd = command + (" " + nodes.write());
	wsSend(cmd);
}

void FSession::wsSend(std::string &input)
{
	fix_broken_escaped_apos ( input );
	debugMessage( ">>" + input);
	socket->sendTextMessage(QString::fromStdString(input));
}

void FSession::wsRecv(std::string packet)
{
	debugMessage(QString("Recv size %0").arg(packet.length()));
	try {
		std::string cmd = packet.substr(0, 3);
		JSONNode nodes;
		if(packet.length() > 4) {
			nodes = libJSON::parse(packet.substr(4, packet.length() - 4));
		}
#define CMD(name) if(cmd == #name) {cmd##name(packet, nodes); return;}
		CMD(ADL); //List of all chat operators.
		CMD(AOP); //Add a chat operator.
		CMD(DOP); //Remove a chat operator.

		CMD(SFC); //Staff report.

		CMD(CDS); //Channel description.
		CMD(CIU); //Channel invite.
		CMD(ICH); //Initial channel data.
		CMD(JCH); //Join channel.
		CMD(LCH); //Leave channel.
		CMD(RMO); //Room mode.

		CMD(LIS); //List of online characters.
		CMD(NLN); //Character is now online.
		CMD(FLN); //Character is now offline.
		CMD(STA); //Status change.

		CMD(CBU); //kick and ban character from channel.
		CMD(CKU); //Kick character from channel.

		CMD(COL); //Channel operator list.
		CMD(COA); //Channel operator add.
		CMD(COR); //Channel operator remove.

		CMD(BRO); //Broadcast message.
		CMD(SYS); //System message.

		CMD(CON); //User count.
		CMD(HLO); //Server hello.
		CMD(IDN); //Identity acknowledged.
		CMD(VAR); //Server variable.

		CMD(FRL); //Friends and bookmarks list.
		CMD(IGN); //Ignore list update.

		CMD(LRP); //Looking for RP message.
		CMD(MSG); //Channel message.
		CMD(PRI); //Private message.
		CMD(RLL); //Dice roll or bottle spin result.

		CMD(TPN); //Typing status.

		CMD(KID); //Custom kink data.
		CMD(PRD); //Profile data.

		CMD(CHA); //Channel list.
		CMD(ORS); //Open room list.

		CMD(RTB); //Real time bridge.

		CMD(ZZZ); //Debug test command.

		CMD(ERR); //Error message.

		CMD(PIN); //Ping.

		debugMessage(QString("The command '%1' was received, but is unknown and could not be processed. %2").arg(QString::fromStdString(cmd)).arg(QString::fromStdString(packet)));
	} catch(std::invalid_argument) {
                debugMessage("Server returned invalid json in its response: " + packet);
        } catch(std::out_of_range) {
                debugMessage("Server produced unexpected json without a field we expected: " + packet);
        }
}

#define COMMAND(name) void FSession::cmd##name(std::string &rawpacket, JSONNode &nodes)


//todo: Merge common code in ADL, AOP and DOP into a separate function.
COMMAND(ADL)
{
	(void)rawpacket;
	//The list of current chat-ops.
	//ADL {"ops": ["name1", "name2"]}
	JSONNode childnode = nodes.at("ops");
	int size = childnode.size();

	for(int i = 0; i < size; ++i) {
		QString op = childnode[i].as_string().c_str();
		operatorlist[op.toLower()] = op;

		if(isCharacterOnline(op)) {
			// Set flag in character
			FCharacter* character = characterlist[op];
			character->setIsChatOp(true);
		}
		account->ui->setChatOperator(this, op, true);
	}
	
}
COMMAND(AOP)
{
	(void)rawpacket;
	//Add a character to the list of known chat-operators.
	//AOP {"character": "Viona"}
	QString op = nodes.at("character").as_string().c_str();
	operatorlist[op.toLower()] = op;
	
	if(isCharacterOnline(op)) {
		// Set flag in character
		FCharacter *character = characterlist[op];
		character->setIsChatOp(true);
	}
	account->ui->setChatOperator(this, op, true);
}

COMMAND(DOP)
{
	(void)rawpacket;
	//Remove a character from the list of  chat operators.
	//DOP {"character": "Viona"}
	QString op = nodes.at("character").as_string().c_str();
	operatorlist.remove(op.toLower());

	if(isCharacterOnline(op)) {
		// Set flag in character
		FCharacter *character = characterlist[op];
		character->setIsChatOp(false);
	}
	account->ui->setChatOperator(this, op, false);
}

COMMAND(SFC)
{
	//Staff report.
	//SFC {"action": actionenum, ???}
	//SFC {"action": "report", "callid": "ID?", "character": "Character Name", "logid": "LogID", "report": "Report Text"}
	//SFC {"action": "confirm", "moderator": "Character Name", "character": "Character Name"}
	//The wiki has no documentation on this command.
	QString action = nodes.at("action").as_string().c_str();
	if(action == "report") {
		QString callid = nodes.at("callid").as_string().c_str();
		QString character = nodes.at("character").as_string().c_str();
		QString report = nodes.at("report").as_string().c_str();
		QString logid;
		QString logstring;
		try {
			logid = nodes.at("logid").as_string().c_str();
			logstring = QString("<a href=\"https://www.f-list.net/fchat/getLog.php?log=%1\" ><b>Log~</b></a> | ").arg(logid);
		} catch(std::out_of_range) {
			logstring.clear();
		}
		QString message = QString("<b>STAFF ALERT!</b> From %1<br />"
					  "%2<br />"
					  "%3" 
					  "<a href=\"#CSA-%4\"><b>Confirm Alert</b></a>").arg(character).arg(report).arg(logstring).arg(callid);
		account->ui->messageSystem(this, message, MessageType::Report);
	} else if(action == "confirm") {
		QString moderator = nodes.at("moderator").as_string().c_str();
		QString character = nodes.at("character").as_string().c_str();
		QString message = QString("<b>%1</b> is handling <b>%2</b>'s report.").arg(moderator).arg(character);
		account->ui->messageSystem(this, message, MessageType::Report);
	} else {
		debugMessage(QString("Received a staff report with an action of '%1' but we don't know how to handle it. %2").arg(action).arg(QString::fromStdString(rawpacket)));
	}
}

COMMAND(CDS)
{
	//Channel description.
	//CDS {"channel": "Channel Name", "description": "Description Text"}
	QString channelname = nodes.at("channel").as_string().c_str();
	QString description = nodes.at("description").as_string().c_str();
	FChannel *channel = channellist.value(channelname);
	if(!channel) {
		debugMessage(QString("[SERVER BUG] Was give the description for the channel '%1', but the channel '%1' is unknown (or never joined).  %2").arg(channelname).arg(QString::fromStdString(rawpacket)));
		return;
	}
	channel->setDescription(description);
	account->ui->setChannelDescription(this, channelname, description);

	// Queued joins. See also FSession::joinChannel for the rest of the logic.
	if(joinQueue.head() == channelname)
	{
		processJoinQueue();
	}
}
COMMAND(CIU)
{
	//Channel invite.
	//CIU {"sender": "Character Name", "name": "Channel Name", "title": "Channel Title"}
	QString charactername = nodes.at("sender").as_string().c_str();
	QString channelname = nodes.at("name").as_string().c_str();
	QString channeltitle = nodes.at("title").as_string().c_str();
	FCharacter *character = getCharacter(charactername);
	if(!character) {
		debugMessage(QString("Received invite to the channel '%1' (title '%2') by '%3' but the character '%3' does not exist. %4")
			     .arg(channelname).arg(channeltitle).arg(charactername).arg(QString::fromStdString(rawpacket)));
		return;
	}
	//todo: Filter the title for problem BBCode characters.
	QString message = makeMessage(QString("/me has invited you to [session=%1]%2[/session].*").arg(channeltitle).arg(channelname), charactername, character, 0, "<font color=\"yellow\"><b>Channel invite:</b></font> ", "");
	account->ui->messageSystem(this, message, MessageType::ChannelInvite);
}
COMMAND(ICH)
{
	(void)rawpacket;
	//Initial channel/room data. Sent when this session joins a channel.
	//ICH { "users": [object], "channnel": "Channel Name", "title": "Channel Title", "mode": enum }
	//Where object is: {"identity":"Character Name"}
	//Where enum is: "ads", "chat", "both"
	//ICH {"users": [{"identity": "Shadlor"}, {"identity": "Bunnie Patcher"}, {"identity": "DemonNeko"}, {"identity": "Desbreko"}, {"identity": "Robert Bell"}, {"identity": "Jayson"}, {"identity": "Valoriel Talonheart"}, {"identity": "Jordan Costa"}, {"identity": "Skip Weber"}, {"identity": "Niruka"}, {"identity": "Jake Brian Purplecat"}, {"identity": "Hexxy"}], "channel": "Frontpage", "mode": "chat"}
	FChannel *channel;
	QString channelname = nodes.at("channel").as_string().c_str();;
	//debugMessage(QString("ICH: channel: %1").arg(channelname));
	QString channelmode = nodes.at("mode").as_string().c_str();;
	//debugMessage(QString("ICH: mode: %1").arg(channelmode));
	JSONNode childnode = nodes.at("users");
	//debugMessage(QString("ICH: users: #%1").arg(childnode.size()));
	QString channeltitle;
	if(channelname.startsWith("ADH-")) {
		try {
			//todo: Wiki says to expect "title" in the ICH command, but none is received
			channeltitle = nodes.at("title").as_string().c_str();
		} catch(std::out_of_range) {
			channeltitle = channelname;
		}
	} else {
		channeltitle = channelname;
	}
	channel = addChannel(channelname, channeltitle);
	account->ui->addChannel(this, channelname, channeltitle);
	if(channelmode == "both") {
		channel->mode = ChannelMode::Both;
	} else if(channelmode == "ads") {
		channel->mode = ChannelMode::Ads;
	} else if(channelmode == "chat") {
		channel->mode = ChannelMode::Chat;
	} else {
		channel->mode = ChannelMode::Unknown;
		debugMessage("[SERVER BUG]: Received unknown channel mode '" + channelmode + "' for channel '" + channelname + "'. <<" + QString::fromStdString(rawpacket));
	}
	account->ui->setChannelMode(this, channelname, channel->mode);

	int size = childnode.size();
	debugMessage("Initial channel data for '" + channelname + "', charcter count: " + QString::number(size));
	for(int i = 0; i < size; i++) {
		QString charactername = childnode.at(i).at("identity").as_string().c_str();
		if(!isCharacterOnline(charactername)) {
			debugMessage("[SERVER BUG] Server gave us a character in the channel user list that we don't know about yet: " + charactername.toStdString() + ", " + rawpacket);
			continue;
		}
		debugMessage("Add character '" + charactername + "' to channel '" + channelname + "'.");
		channel->addCharacter(charactername, false);
	}
	account->ui->notifyChannelReady(this, channelname);
}
COMMAND(JCH)
{
	(void)rawpacket;
	//Join channel notification. Sent when a character joins a channel.
	//JCH {"character": {"identity": "Character Name"}, "channel": "Channel Name", "title": "Channel Title"}
	FChannel *channel;
	QString channelname = nodes.at("channel").as_string().c_str();;
	QString channeltitle;
	QString charactername = nodes.at("character").at("identity").as_string().c_str();
	if(channelname.startsWith("ADH-")) {
		channeltitle = nodes.at("title").as_string().c_str();
	} else {
		channeltitle = channelname;
	}
	channel = addChannel(channelname, channeltitle);
	account->ui->addChannel(this, channelname, channeltitle);
	channel->addCharacter(charactername, true);
	if(charactername == character) {
		channel->join();
	}
}
COMMAND(LCH)
{
	//Leave a channel. Sent when a character leaves a channel.
	//LCH {"channel": "Channel Name", "character", "Character Name"}
	FChannel *channel;
	QString channelname = nodes.at("channel").as_string().c_str();;
	QString charactername = nodes.at("character").as_string().c_str();
	channel = getChannel(channelname);
	if(!channel) {
		debugMessage("[SERVER BUG] Was told about character '" + charactername + "' leaving unknown channel '" + channelname + "'.  " + QString::fromStdString(rawpacket));
		return;
	}
	channel->removeCharacter(charactername);
	if(charactername == character) {
		channel->leave();
		//todo: ui->removeChannel() ?
	}
	
}
COMMAND(RMO)
{
	(void)rawpacket;
	//Room mode.
	//RMO {"mode": mode_enum, "channel": "Channel Name"}
	//Where mode_enum
	QString channelname = nodes.at("channel").as_string().c_str();
	QString channelmode = nodes.at("mode").as_string().c_str();
	FChannel *channel = channellist[channelname];
	if(!channel) {
		//todo: Determine if RMO can be sent even if we're not in the channel in question.
		return;
	}
	QString modedescription;
	if(channelmode == "both") {
		channel->mode = ChannelMode::Both;
		modedescription = "chat and ads";
	} else if(channelmode == "ads") {
		channel->mode = ChannelMode::Ads;
		modedescription = "ads only";
	} else if(channelmode == "chat") {
		channel->mode = ChannelMode::Chat;
		modedescription = "chat only";
	} else {
		debugMessage(QString("[SERVER BUG]: Received channel mode update '%1' for channel '%2'. %3").arg(channelmode).arg(channelname).arg(QString::fromStdString(rawpacket)));
		return;
	}
	QString message = "[session=%1]%2[/session]'s mode has been changed to: %3";
	message = bbcodeparser->parse(message).arg(channel->getTitle()).arg(channelname).arg(modedescription);
	account->ui->setChannelMode(this, channelname, channel->mode);
	account->ui->messageChannel(this, channelname, message, MessageType::ChannelMode, true);
}

COMMAND(NLN)
{
	(void)rawpacket;
	//Character is now online.
	//NLN {"identity": "Character Name", "gender": genderenum, "status": statusenum}
	//Where 'statusenum' is one of: "online"
	//Where 'genderenum' is one of: "Male", "Female", 
	QString charactername = nodes.at("identity").as_string().c_str();
	QString gender = nodes.at("gender").as_string().c_str();
	QString status = nodes.at("status").as_string().c_str();
	FCharacter *character = addCharacter(charactername);
	character->setGender(gender);
	character->setStatus(status);
	if(operatorlist.contains(charactername.toLower())) {
		character->setIsChatOp(true);
	}
	emit notifyCharacterOnline(this, charactername, true);
}
COMMAND(LIS)
{
	(void)rawpacket;
	//List of online characters. This can be sent in multiple blocks.
	//LIS {"characters": [character, character]
	//Where 'character' is: ["Character Name", genderenum, statusenum, "Status Message"]
	nodes.preparse();
	JSONNode childnode = nodes.at("characters");
	int size = childnode.size();
	//debugMessage("Character list count: " + QString::number(size));
	//debugMessage(childnode.write());
	for(int i = 0; i < size; i++) {
		JSONNode characternode = childnode.at(i);
		//debugMessage("char #" + QString::number(i) + " : " + QString::fromStdString(characternode.write()));
		QString charactername = characternode.at(0).as_string().c_str();
		//debugMessage("charactername: " + charactername);
		QString gender = characternode.at(1).as_string().c_str();
		//debugMessage("gender: " + gender);
		QString status = characternode.at(2).as_string().c_str();
		//debugMessage("status: " + status);
		QString statusmessage = characternode.at(3).as_string().c_str();
		//debugMessage("statusmessage: " + statusmessage);
		FCharacter *character;
		character = addCharacter(charactername);
		character->setGender(gender);
		character->setStatus(status);
		character->setStatusMsg(statusmessage);
		if(operatorlist.contains(charactername.toLower())) {
			character->setIsChatOp(true);
		}
		emit notifyCharacterOnline(this, charactername, true);
	}
}
COMMAND(FLN)
{
	(void)rawpacket;
	//Character is now offline.
	//FLN {"character": "Character Name"}
	QString charactername = nodes.at("character").as_string().c_str();
	if(!isCharacterOnline(charactername)) {
		debugMessage("[SERVER BUG] Received offline message for '" + charactername + "' but they're not listed as being online.");
		return;
	}
	//Iterate over all channels and make the chracacter leave them if they're present.
	for(QHash<QString, FChannel *>::const_iterator iter = channellist.begin(); iter != channellist.end(); iter++) {
		if((*iter)->isCharacterPresent(charactername)) {
			(*iter)->removeCharacter(charactername);
		}
	}
	emit notifyCharacterOnline(this, charactername, false);
	removeCharacter(charactername);
}
COMMAND(STA)
{
	//Status change.
	//STA {"character": "Character Name", "status": statusenum, "statusmsg": "Status message"}
	QString charactername = nodes.at("character").as_string().c_str();
	QString status = nodes.at("status").as_string().c_str();
	QString statusmessage;
	FCharacter *character = getCharacter(charactername);
	if(!character) {
		debugMessage(QString("[SERVER BUG] Received a status update message from the character '%1', but the character '%1' is unknown. %2").arg(charactername).arg(QString::fromStdString(rawpacket)));
		return;
	}
	character->setStatus(status);
	try {
		statusmessage = nodes.at("statusmsg").as_string().c_str();
		character->setStatusMsg(statusmessage);
	} catch (std::out_of_range) {
		// Crown messages can cause there to be no statusmsg.
		/*do nothing*/
	}
	emit notifyCharacterStatusUpdate(this, charactername);
}

COMMAND(CBUCKU)
{
	//CBU and CKU commands commoned up. Except for their messages, their behaviour is identical.
	FChannel *channel;
	QString channelname = nodes.at("channel").as_string().c_str();
	QString charactername = nodes.at("character").as_string().c_str();
	QString operatorname = nodes.at("operator").as_string().c_str();
	bool banned = rawpacket.substr(0, 3) == "CBU";
	QString kicktype = banned ? "kicked and banned" : "kicked";
	channel = getChannel(channelname);
	if(!channel) {
		debugMessage(QString("[SERVER BUG] Was told about character '%1' being %4 from channel '%2' by '%3', but the channel '%2' is unknown (or never joined).  %5").arg(charactername).arg(channelname).arg(operatorname).arg(kicktype).arg(QString::fromStdString(rawpacket)));
		return;
	}
	if(!channel->isJoined()) {
		debugMessage(QString("[SERVER BUG] Was told about character '%1' being %4 from channel '%2' by '%3', but this session is no longer joined with channel '%2'.  %5").arg(charactername).arg(channelname).arg(operatorname).arg(kicktype).arg(QString::fromStdString(rawpacket)));
		return;
	}
	if(!channel->isCharacterPresent(charactername)) {
		debugMessage(QString("[SERVER BUG] Was told about character '%1' being %4 from channel '%2' by '%3', but '%1' is not present in the channel.  %5").arg(charactername).arg(channelname).arg(operatorname).arg(kicktype).arg(QString::fromStdString(rawpacket)));
		return;
	}
	if(!channel->isCharacterOperator(operatorname) && !isCharacterOperator(operatorname)) {
		debugMessage(QString("[SERVER BUG] Was told about character '%1' being %4 from channel '%2' by '%3', but '%3' is not a channel operator or a server operator!  %5").arg(charactername).arg(channelname).arg(operatorname).arg(kicktype).arg(QString::fromStdString(rawpacket)));
	}
	QString message = QString("<b>%1</b> has %4 <b>%2</b> from %3.").arg(operatorname).arg(charactername).arg(channel->getTitle()).arg(kicktype);
	if(charactername == character) {
		account->ui->messageChannel(this, channelname, message, banned ? MessageType::KickBan : MessageType::Kick, true, true);
		channel->removeCharacter(charactername);
		channel->leave();
	} else {
		account->ui->messageChannel(this, channelname, message, banned ? MessageType::KickBan : MessageType::Kick, channel->isCharacterOperator(character), false);
		channel->removeCharacter(charactername);
	}
}
COMMAND(CBU)
{
	//Kick and ban character from channel.
	//CBU {"operator": "Character Name", "channel": "Channel Name", "character": "Character Name"}
	cmdCBUCKU(rawpacket, nodes);
}
COMMAND(CKU)
{
	//Kick character from channel.
	//CKU {"operator": "Character Name", "channel": "Channel Name", "character": "Character Name"}
	cmdCBUCKU(rawpacket, nodes);
}

COMMAND(COL)
{
	//Channel operator list.
	//COL {"channel":"Channel Name", "oplist":["Character Name"]}
	QString channelname = nodes.at("channel").as_string().c_str();
	FChannel *channel = getChannel(channelname);
	if(!channel) {
		debugMessage(QString("[SERVER BUG] Was given the channel operator list for the channel '%1', but the channel '%1' is unknown (or never joined).  %2").arg(channelname).arg(QString::fromStdString(rawpacket)));
		return;
	}
	JSONNode childnode = nodes.at("oplist");
	//todo: clear the existing operator list first
	int size = childnode.size();
	for(int i = 0; i < size; i++) {
		QString charactername = childnode.at(i).as_string().c_str();
		channel->addOperator(charactername);
	}
}
COMMAND(COA)
{
	//Channel operator add.
	//COA {"channel":"Channel Name", "character":"Character Name"}
	QString channelname = nodes.at("channel").as_string().c_str();
	QString charactername = nodes.at("character").as_string().c_str();
	FChannel *channel = getChannel(channelname);
	if(!channel) {
		debugMessage(QString("[SERVER BUG] Was told to add '%2' as a channel operator for channel '%1', but the channel '%1' is unknown (or never joined).  %3").arg(channelname).arg(charactername).arg(QString::fromStdString(rawpacket)));
		return;
	}
	//todo: Print a BUG message about adding operators twice.
	channel->addOperator(charactername);
}
COMMAND(COR)
{
	//Channel operator remove.
	//COR {"channel":"Channel Name", "character":"Character Name"}
	QString channelname = nodes.at("channel").as_string().c_str();
	QString charactername = nodes.at("character").as_string().c_str();
	FChannel *channel = getChannel(channelname);
	if(!channel) {
		debugMessage(QString("[SERVER BUG] Was told to remove '%2' from the list of channel operators for channel '%1', but the channel '%1' is unknown (or never joined).  %3").arg(channelname).arg(charactername).arg(QString::fromStdString(rawpacket)));
		return;
	}
	//todo: Print a bug message  if we're removing someone not on the operator list and skip the whole removal process.
	channel->removeOperator(charactername);
}

COMMAND(BRO)
{
	(void)rawpacket;
	//Broadcast message.
	//BRO {"message": "Message Text"}
	QString message = nodes.at("message").as_string().c_str();
	account->ui->messageAll(this, QString("<b>Broadcast message:</b> %1").arg(bbcodeparser->parse(message)), MessageType::System);
}
COMMAND(SYS)
{
	(void)rawpacket;
	//System message
	//SYS {"message": "Message Text"}
	QString message = nodes.at("message").as_string().c_str();
	account->ui->messageSystem(this, QString("<b>System message:</b> %1").arg(message), MessageType::System);
}

COMMAND(CON)
{
	(void)rawpacket;
	//User count.
	//CON {"count": usercount}
	QString count = nodes.at("count").as_string().c_str();
	//The message doesn't handle the plural case correctly, but that only happens on the test server.
	account->ui->messageSystem(this, QString("%1 users are currently connected.").arg(count), MessageType::Login);
}
COMMAND(HLO)
{
	(void) rawpacket;
	//Server hello. Sent during the initial connection traffic after identification.
	//HLO {"message": "Server Message"}
	QString message = nodes.at("message").as_string().c_str();
	account->ui->messageSystem(this, QString("<b>%1</b>").arg(message), MessageType::Login);
	foreach(QString channelname, autojoinchannels) {
		joinChannel(channelname);
	}
}
COMMAND(IDN)
{
	//Identity acknowledged.
	//IDN {"character": "Character Name"}
	QString charactername = nodes.at("character").as_string().c_str();

	QString message = QString("<b>%1</b> connected.").arg(charactername);
	account->ui->messageSystem(this, message, MessageType::Login);
	if(charactername != character) {
		debugMessage(QString("[SERVER BUG] Received IDN response for '%1', but this session is for '%2'. %3").arg(charactername).arg(character).arg(QString::fromStdString(rawpacket)));
	}
}
COMMAND(VAR)
{
	(void)rawpacket;
	//Server variable
	//VAR {"value":value, "variable":"Variable_Name"}
	QString value = nodes.at("value").as_string().c_str();
	QString variable = nodes.at("variable").as_string().c_str();
	servervariables[variable] = value;
	debugMessage(QString("Server variable: %1 = '%2'").arg(variable).arg(value));
	//todo: Parse and store variables of interest.
}

COMMAND(FRL)
{
	(void) rawpacket;
	//Friends and bookmarks list.
	//FRL {"characters":["Character Name"]}
	JSONNode childnode = nodes.at("characters");
	int size = childnode.size();
	for(int i = 0; i < size; i++) {
		QString charactername = childnode.at(i).as_string().c_str();
		if(!friendslist.contains(charactername)) {
			friendslist.append(charactername);
			//debugMessage(QString("Added friend '%1'.").arg(charactername));
		}
	}
	
}
COMMAND(IGN)
{
	//Ignore list update. Behaviour of the command depends on the "action" field.
	//IGN {"action": "init", "characters":, ["Character Name"]}
	//IGN {"action": "add", "characters":, "Character Name"}
	//IGN {"action": "delete", "characters":, "Character Name"}
	QString action = nodes.at("action").as_string().c_str();
	if(action == "init") {
		JSONNode childnode = nodes.at("characters");
		ignorelist.clear();
		int size = childnode.size();
		for(int i = 0; i < size; i++) {
			QString charactername = childnode.at(i).as_string().c_str();
			if(!ignorelist.contains(charactername, Qt::CaseInsensitive)) {
				ignorelist.append(charactername.toLower());
			}
		}
		emit notifyIgnoreList(this);
	} else if(action == "add") {
		QString charactername = nodes.at("character").as_string().c_str();
		if(ignorelist.contains(charactername, Qt::CaseInsensitive)) {
			debugMessage(QString("[BUG] Was told to add '%1' to our ignore list, but '%1' is already on our ignore list. %2").arg(charactername).arg(QString::fromStdString(rawpacket)));
		} else {
			ignorelist.append(charactername.toLower());
		}
		emit notifyIgnoreAdd(this, charactername);
	} else if(action =="delete") {
		QString charactername = nodes.at("character").as_string().c_str();
		if(!ignorelist.contains(charactername, Qt::CaseInsensitive)) {
			debugMessage(QString("[BUG] Was told to remove '%1' from our ignore list, but '%1' is not on our ignore list. %2").arg(charactername).arg(QString::fromStdString(rawpacket)));
		} else {
			ignorelist.removeAll(charactername.toLower());
		}
		emit notifyIgnoreRemove(this, charactername);
	} else {
		debugMessage(QString("[SERVER BUG] Received ignore command(IGN) but the action '%1' is unknown. %2").arg(action).arg(QString::fromStdString(rawpacket)));
		return;
	}
}

QString FSession::makeMessage(QString message, QString charactername, FCharacter *character, FChannel *channel, QString prefix, QString postfix)
{
	QString characterprefix;
	QString characterpostfix;
	if(isCharacterOperator(charactername)) {
		//todo: choose a different icon
		characterprefix += "<img src=\":/images/auction-hammer.png\" />";
	}
	if(isCharacterOperator(charactername) || (channel && channel->isCharacterOperator(charactername))) {
		characterprefix += "<img src=\":/images/auction-hammer.png\" />";
	}
	QString messagebody;
	if(message.startsWith("/me 's ")) {
		messagebody = message.mid(7, -1);
		messagebody = bbcodeparser->parse(messagebody);
		characterpostfix += "'s"; //todo: HTML escape
	} else if(message.startsWith("/me ")) {
		messagebody = message.mid(4, -1);
		messagebody = bbcodeparser->parse(messagebody);
	} else if(message.startsWith("/warn ")) {
		messagebody = message.mid(6, -1);
		messagebody = QString("<span id=\"warning\">%1</span>").arg(bbcodeparser->parse(messagebody));
	} else {
		messagebody = bbcodeparser->parse(message);
	}
	QString messagefinal;
	if(character != NULL) {
		messagefinal = QString("<b><a style=\"color: %1\" href=\"%2\">%3%4%5</a></b> %6")
			.arg(character->genderColor().name())
			.arg(character->getUrl())
			.arg(characterprefix)
			.arg(charactername) //todo: HTML escape
			.arg(characterpostfix)
			.arg(messagebody);
	} else {
		messagefinal = QString("<b><a style=\"color: %1\" href=\"%2\">%3%4%5</a></b> %6")
			.arg(FCharacter::genderColors[FCharacter::GENDER_OFFLINE_UNKNOWN].name())
			.arg(getCharacterUrl(charactername))
			.arg(characterprefix)
			.arg(charactername) //todo: HTML escape
			.arg(characterpostfix)
			.arg(messagebody);
	}
	if(message.startsWith("/me")) {
		messagefinal = QString("%1<i>*%2</i>%3")
			.arg(prefix)
			.arg(messagefinal)
			.arg(postfix);
	} else {
		messagefinal = QString("%1%2%3")
			.arg(prefix)
			.arg(messagefinal)
			.arg(postfix);
	}

	return messagefinal;
}

COMMAND(LRP)
{
	//Looking for RP message.
	//LRP {"channel": "Channel Name", "character": "Character Name", "message": "Message Text"}
	QString channelname = nodes.at("channel").as_string().c_str();
	QString charactername = nodes.at("character").as_string().c_str();
	QString message = nodes.at("message").as_string().c_str();
	FChannel *channel = getChannel(channelname);
	FCharacter *character = getCharacter(charactername);
	if(!channel) {
		debugMessage(QString("[SERVER BUG] Received an RP ad from the channel '%1' but the channel '%1' is unknown. %2").arg(channelname).arg(QString::fromStdString(rawpacket)));
		return;
	}
	if(!character) {
		debugMessage(QString("[SERVER BUG] Received an RP ad from '%1' in the channel '%2' but the character '%1' is unknown. %3").arg(charactername).arg(channelname).arg(QString::fromStdString(rawpacket)));
		//todo: Allow it to be displayed anyway?
		return;
	}
	if(isCharacterIgnored(charactername)) {
		//Ignore message
		return;
	}
	QString messagefinal = makeMessage(message, charactername, character, channel, "<font color=\"green\"><b>Roleplay ad by</b></font> ", "");
	FMessage fmessage(messagefinal, MessageType::RpAd);
	fmessage.toChannel(channelname).fromChannel(channelname).fromCharacter(charactername).fromSession(sessionid);
	account->ui->messageMessage(fmessage);
}
COMMAND(MSG)
{
	//Channel message.
	//MSG {"channel": "Channel Name", "character": "Character Name", "message": "Message Text"}
	QString channelname = nodes.at("channel").as_string().c_str();
	QString charactername = nodes.at("character").as_string().c_str();
	QString message = nodes.at("message").as_string().c_str();
	FChannel *channel = getChannel(channelname);
	FCharacter *character = getCharacter(charactername);
	if(!channel) {
		debugMessage(QString("[SERVER BUG] Received a message from the channel '%1' but the channel '%1' is unknown. %2").arg(channelname).arg(QString::fromStdString(rawpacket)));
		return;
	}
	if(!character) {
		debugMessage(QString("[SERVER BUG] Received a message from '%1' in the channel '%2' but the character '%1' is unknown. %3").arg(charactername).arg(channelname).arg(QString::fromStdString(rawpacket)));
		//todo: Allow it to be displayed anyway?
		return;
	}
	if(isCharacterIgnored(charactername)) {
		//Ignore message
		return;
	}
	QString messagefinal = makeMessage(message, charactername, character, channel);
	FMessage fmessage(messagefinal, MessageType::Chat);
	fmessage.toChannel(channelname).fromChannel(channelname).fromCharacter(charactername).fromSession(sessionid);
	account->ui->messageMessage(fmessage);
}
COMMAND(PRI)
{
	//Private message.
	//PRI {"character": "Character Name", "message": "Message Text"}
	QString charactername = nodes.at("character").as_string().c_str();
	QString message = nodes.at("message").as_string().c_str();
	FCharacter *character = getCharacter(charactername);
	if(!character) {
		debugMessage(QString("[SERVER BUG] Received a message from the character '%1', but the character '%1' is unknown. %2").arg(charactername).arg(QString::fromStdString(rawpacket)));
		//todo: Allow it to be displayed anyway?
		return;
	}
	if(isCharacterIgnored(charactername)) {
		//Ignore message
		return;
	}
	
	QString messagefinal = makeMessage(message, charactername, character);
	account->ui->addCharacterChat(this, charactername);
	FMessage fmessage(messagefinal, MessageType::Chat);
	fmessage.toCharacter(charactername).fromCharacter(charactername).fromSession(sessionid);
	account->ui->messageMessage(fmessage);
}
COMMAND(RLL)
{
	//Dice roll or bottle spin result.
	//Bottle spin:
	//RLL {"type": "bottle", "message": "Message Text", "channel": "Channel Name", "character": "Character Name", "target": "Character Name"}
	//Dice roll:
	//RLL {"type": "dice", "message": "Message Text", "channel": "Channel Name", "character": "Character Name", "results": [number], "endresult": number}
	//PM roll:
	//RLL {"type": "dice", "message": "Message Text", "recipient": "Partner Character Name", "character": "Rolling Character Name", "endresult": number, "rolls": ["2d20", "3", "-1d10"], "results": [number, number, number]}
	QString channelname;
	try {
		channelname = nodes.at("channel").as_string().c_str();
	} catch(std::out_of_range) {
	}
	QString charactername = nodes.at("character").as_string().c_str();
	if(channelname.isEmpty() && charactername == this->character) {
		//We're the character rolling in a PM, so the "recipient" field contains the character we want.
		charactername = nodes.at("recipient").as_string().c_str();
	}
	QString message = nodes.at("message").as_string().c_str();
	if(isCharacterIgnored(charactername)) {
		//Ignore message
		return;
	}
	if(!channelname.isEmpty()) {
		FChannel *channel = getChannel(channelname);
		//FCharacter *character = getCharacter(charactername);
		if(!channel) {
			debugMessage(QString("[SERVER BUG] Received a dice roll result from the channel '%1' but the channel '%1' is unknown. %2").arg(channelname).arg(QString::fromStdString(rawpacket)));
			//todo: Dump the message to console anyway?
			return;
		}
		//todo: Maybe extract character name and make it a link and colored like normal.
		account->ui->messageChannel(this, channelname, bbcodeparser->parse(message), MessageType::DiceRoll, true);
	} else {
		account->ui->addCharacterChat(this, charactername);
		QString messagefinal = bbcodeparser->parse(message);
		FMessage fmessage(messagefinal, MessageType::DiceRoll);
		fmessage.toCharacter(charactername).fromCharacter(this->character).fromSession(sessionid);
		account->ui->messageMessage(fmessage);
	}
}

COMMAND(TPN)
{
	//Typing status.
	//TPN {"status": typing_enum, "character": "Character Name"}
	//Where 'typing_enum' is one of: "typing", "paused", "clear"
	QString charactername = nodes.at("character").as_string().c_str();
	QString typingstatus = nodes.at("status").as_string().c_str();
	FCharacter *character = getCharacter(charactername);
	if(!character) {
		debugMessage(QString("[SERVER BUG] Received a typing status update for the character '%1' but the character '%1' is unknown. %2").arg(charactername).arg(QString::fromStdString(rawpacket)));
		return;
	}
	TypingStatus status;
	if(typingstatus == "typing") {
		status = TYPING_STATUS_TYPING;
	} else if(typingstatus == "paused") {
		status = TYPING_STATUS_PAUSED;
	} else if(typingstatus == "clear") {
		status = TYPING_STATUS_CLEAR;
	} else {
		debugMessage(QString("[SERVER BUG] Received a typing status update of '%2' for the character '%1' but the typing status '%2' is unknown. %3").arg(charactername).arg(typingstatus).arg(QString::fromStdString(rawpacket)));
		status = TYPING_STATUS_CLEAR;
	}
	account->ui->setCharacterTypingStatus(this, charactername, status);

	
}

COMMAND(KID)
{
	//Custom kink data.
	//KID {"type": kinkdataenum, "character": "Character Name", ???}
	//KID {"type": "start", "character": "Character Name", "message": "Custom kinks of Character Name"}
	//KID {"type": "end", "character": "Character Name", "message": "End of custom kinks."}
	//KID {"type": "custom", "character": "Character Name", "key": "Key Text", "value": "Value Text"}
	//Where "kinkdataenum" is one of: "start", "end", "custom"
	QString type = nodes.at("type").as_string().c_str();
	QString charactername = nodes.at("character").as_string().c_str();
	FCharacter *character = getCharacter(charactername);
	if(!character) {
		debugMessage(QString("[SERVER BUG] Received custom kink data for the character '%1' but the character '%1' is unknown. %2").arg(charactername).arg(QString::fromStdString(rawpacket)));
		return;
	}
	if(type == "start") {
		character->clearCustomKinkData();
		//account->ui->notifyCharacterCustomKinkDataReset(this, charactername);
	} else if(type == "end") {
		account->ui->notifyCharacterCustomKinkDataUpdated(this, charactername);
	} else if(type == "custom") {
		QString key = nodes.at("key").as_string().c_str();
		QString value = nodes.at("value").as_string().c_str();
		character->addCustomKinkData(key, value);
	} else {
		debugMessage(QString("[BUG] Received custom kink data for the character '%1' with a type of '%2' but we don't know how to handle '%2'. %3").arg(charactername).arg(QString::fromStdString(rawpacket)));
	}
}
COMMAND(PRD)
{
	//Profile data.
	//PRD {"type": profiledataenum, "character": "Character Name", ???}
	//PRD {"type": "start", "character": "Character Name", "message": "Profile of Character Name"}
	//PRD {"type": "end", "character": "Character Name", "message": "End of Profile"}
	//PRD {"type": "info", "character": "Character Name", "key": "Key Text", "value": "Value Text"}
	//PRD {"type": "select", "character": "Character Name", ???}
	//Where "profiledataenum" is one of: "start", "end", "info", "select"
	QString type = nodes.at("type").as_string().c_str();
	QString charactername = nodes.at("character").as_string().c_str();
	FCharacter *character = getCharacter(charactername);
	if(!character) {
		debugMessage(QString("[SERVER BUG] Received profile data for the character '%1' but the character '%1' is unknown. %2").arg(charactername).arg(QString::fromStdString(rawpacket)));
		return;
	}
	if(type == "start") {
		character->clearProfileData();
		//account->ui->notifyCharacterProfileDataReset(this, charactername);
	} else if(type == "end") {
		account->ui->notifyCharacterProfileDataUpdated(this, charactername);
	} else if(type == "info") {
		QString key = nodes.at("key").as_string().c_str();
		QString value = nodes.at("value").as_string().c_str();
		character->addProfileData(key, value);
	} else {
		//todo: "select" is referred to in the wiki but no additional detail is given.
		debugMessage(QString("[BUG] Received profile data for the character '%1' with a type of '%2' but we don't know how to handle '%2'. %3").arg(charactername).arg(QString::fromStdString(rawpacket)));
	}
	
}

COMMAND(CHA)
{
	(void)rawpacket;
	//Channel list.
	//CHA {"channels": [{"name": "Channel Name", "characters": character_count}]}
	knownchannellist.clear();
	JSONNode childnode = nodes.at("channels");
	int size = childnode.size();
	for(int i = 0; i < size; i++) {
		JSONNode channelnode = childnode.at(i);
		QString channelname = channelnode.at("name").as_string().c_str();
		QString channelcountstring = channelnode.at("characters").as_string().c_str();
		//todo: Verify the count string can be converted properly.
		int channelcount = channelcountstring.toInt();
		knownchannellist.append(FChannelSummary(FChannelSummary::Public, channelname, channelcount));
	}
	account->ui->updateKnownChannelList(this);
}
COMMAND(ORS)
{
	(void)rawpacket;
	//Open room list.
	//CHA {"channels": [{"name": "Channel Name", "title": "Channel Title", "characters": character_count}]}
	knownopenroomlist.clear();
	JSONNode childnode = nodes.at("channels");
	int size = childnode.size();
	for(int i = 0; i < size; i++) {
		JSONNode channelnode = childnode.at(i);
		QString channelname = channelnode.at("name").as_string().c_str();
		QString channeltitle = channelnode.at("title").as_string().c_str();
		QString channelcountstring = channelnode.at("characters").as_string().c_str();
		int channelcount = channelcountstring.toInt();
		knownopenroomlist.append(FChannelSummary(FChannelSummary::Private, channelname, channeltitle, channelcount));
	}
	account->ui->updateKnownOpenRoomList(this);
}

COMMAND(RTB)
{
	(void)rawpacket; (void)nodes;
	//Real time bridge.
	//RTB {"type":typeenum, ???}
	//RTB {"type":"note", "sender": "Character Name", "subject": "Subject Text", "id":id}
	//RTB {"type":"trackadd","name":"Character Name"}
	//RTB {"type":"trackrem","name":"Character Name"}
	//RTB {"type":"friendrequest","name":"Character Name"}
	//RTB {"type":"friendadd","name":"Character Name"}
	//RTB {"type":"friendremove","name":"Character Name"}

	//todo: Determine all the RTB messages.
	QString type = nodes.at("type").as_string().c_str();
	if(type == "note") {
		QString charactername = nodes.at("sender").as_string().c_str();
		QString subject = nodes.at("subject").as_string().c_str();
		QString id = nodes.at("id").as_string().c_str();
		
		QString message = "Note recieved from %1: <a href=\"https://www.f-list.net/view_note.php?note_id=%2\">%3</a>";
		message = message
			.arg(getCharacterHtml(charactername))
			.arg(id)
			.arg(subject);
		FMessage fmessage(message, MessageType::Note);
		fmessage.toUser().toCharacter(charactername).fromCharacter(charactername).fromSession(sessionid);
		account->ui->messageMessage(fmessage);
		//account->ui->messageSystem(this, message, MessageType::Note);
	} else if(type == "trackadd") {
		QString charactername = nodes.at("name").as_string().c_str();
		if(!friendslist.contains(charactername)) {
			friendslist.append(charactername);
			//debugMessage(QString("Added friend '%1'.").arg(charactername));
		}
		QString message = "Bookmark update: %1 has been added to your bookmarks."; //todo: escape characters?
		message = message.arg(getCharacterHtml(charactername));
		FMessage fmessage(message, MessageType::Bookmark);
		fmessage.toUser().toCharacter(charactername).fromCharacter(charactername).fromSession(sessionid);
		account->ui->messageMessage(fmessage);
		//account->ui->messageSystem(this, message, MessageType::Bookmark);
	} else if(type == "trackrem") {
		//todo: Update bookmark list? (Removing has the complication in that bookmarks and friends aren't distinguished and multiple instances of friends may exist.)
		QString charactername = nodes.at("name").as_string().c_str();
		QString message = "Bookmark update: %1 has been removed from your bookmarks."; //todo: escape characters?
		message = message.arg(getCharacterHtml(charactername));
		FMessage fmessage(message, MessageType::Bookmark);
		fmessage.toUser().toCharacter(charactername).fromCharacter(charactername).fromSession(sessionid);
		account->ui->messageMessage(fmessage);
		//account->ui->messageSystem(this, message, MessageType::Bookmark);
	} else if(type == "friendrequest") {
		QString charactername = nodes.at("name").as_string().c_str();
		QString message = "Friend update: %1 has requested to be friends with one of your characters. Visit <a href=\"%2\">%2</a> to view the request on F-List."; //todo: escape characters?
		message = message.arg(charactername).arg("https://www.f-list.net/messages.php?show=friends");
		FMessage fmessage(message, MessageType::Friend);
		fmessage.toUser().toCharacter(charactername).fromCharacter(charactername).fromSession(sessionid);
		account->ui->messageMessage(fmessage);
		//account->ui->messageSystem(this, message, MessageType::Friend);
	} else if(type == "friendadd") {
		QString charactername = nodes.at("name").as_string().c_str();
		if(!friendslist.contains(charactername)) {
			friendslist.append(getCharacterHtml(charactername));
			//debugMessage(QString("Added friend '%1'.").arg(charactername));
		}
		QString message = "Friend update: %1 has become friends with one of your characters."; //todo: escape characters?
		message = message.arg(getCharacterHtml(charactername));
		FMessage fmessage(message, MessageType::Friend);
		fmessage.toUser().toCharacter(charactername).fromCharacter(charactername).fromSession(sessionid);
		account->ui->messageMessage(fmessage);
		//account->ui->messageSystem(this, message, MessageType::Friend);
	} else if(type == "friendremove") {
		//todo: Update bookmark/friend list? (Removing has the complication in that bookmarks and friends aren't distinguished and multiple instances of friends may exist.)
		QString charactername = nodes.at("name").as_string().c_str();
		QString message = "Friend update: %1 was removed as a friend from one of your characters."; //todo: escape characters?
		message = message.arg(getCharacterHtml(charactername));
		FMessage fmessage(message, MessageType::Friend);
		fmessage.toUser().toCharacter(charactername).fromCharacter(charactername).fromSession(sessionid);
		account->ui->messageMessage(fmessage);
		//account->ui->messageSystem(this, message, MessageType::Friend);
	} else {
		QString message = "Received an unknown/unhandled Real Time Bridge message of type \"%1\". Received packet: %2"; //todo: escape characters?
		message = message.arg(type).arg(rawpacket.c_str());
		account->ui->messageSystem(this, message, MessageType::Error);
	}
	//debugMessage(QString("Real time bridge: %1").arg(QString::fromStdString(rawpacket)));
}

COMMAND(ZZZ)
{
	(void)rawpacket;
	//Debug test command.
	//ZZZ {"message": "???"}
	//This command is not documented.
	QString message = nodes.at("message").as_string().c_str();
	account->ui->messageSystem(this, QString("<b>Debug Reply:</b> %1").arg(message), MessageType::System);
}

COMMAND(ERR)
{
	//Error message.
	//ERR {"number": error_number, "message": "Error Message"}
	QString errornumberstring = nodes.at("number").as_string().c_str();
	QString errormessage = nodes.at("message").as_string().c_str();
	QString message = QString("<b>Error %1: </b> %2").arg(errornumberstring).arg(errormessage);
	account->ui->messageSystem(this, message, MessageType::Error);
	bool ok;
	int errornumber = errornumberstring.toInt(&ok);
	if(!ok) {
		debugMessage(QString("Received an error message but could not convert the error number to an integer. Error number '%1', error message '%2' : %3").arg(errornumberstring).arg(errormessage).arg(QString::fromStdString(rawpacket)));
			     return;
	}
	//Handle special cases.
	//todo: Parse the error and pass along to the UI for more informative feedback.
	switch(errornumber) {
	case 34: //Error 34 is not in the wiki, but the existing code sends out another identification if it is received.
	{
		JSONNode loginnode;
		loginnode.push_back(JSONNode("method", "ticket"));
		loginnode.push_back(JSONNode("ticket", account->ticket.toStdString()));
		loginnode.push_back(JSONNode("account", account->getUserName().toStdString()));
		loginnode.push_back(JSONNode("cname", FLIST_CLIENTID));
		loginnode.push_back(JSONNode("cversion", FLIST_VERSIONNUM));
		loginnode.push_back(JSONNode("character", character.toStdString()));
		std::string idenStr = "IDN " + loginnode.write();
		//debugMessage("Indentify...");
		wsSend(idenStr);
		break;
	}
	case 28: // Already in channel
	case 26: // No such channel
	case 44: // Not invited to an invite-only channel
	case 48: // Banned from that channel
		processJoinQueue();
		break;
	default:
		break;
	}
		
}

COMMAND(PIN)
{
	(void)rawpacket; (void)nodes;
	//debugMessage("Ping!");
	std::string cmd = "PIN";
	wsSend(cmd);
}

//todo: Lots of duplicated between sendChannelMessage() and sendChannelAdvertisement() that can be refactored into a common function. 
void FSession::sendChannelMessage(QString channelname, QString message)
{
	//Confirm channel is known, joined and has the right permissions.
	FChannel *channel = getChannel(channelname);
	if(!channel) {
		account->ui->messageSystem(this, QString("Tried to send a message to '%1' but the channel is unknown or has never been joined. Message: %2").arg(channelname).arg(message), MessageType::Feedback);
		return;
	}
	if(!channel->isJoined()) {
		account->ui->messageSystem(this, QString("Tried to send a message to '%1' but you are not currently in the channel. Message: %2").arg(channelname).arg(message), MessageType::Feedback);
		return;
	}
	if(channel->mode == ChannelMode::Ads) {
		account->ui->messageSystem(this, QString("Tried to send a message to '%1' but the channel only allows advertisements. Message: %2").arg(channelname).arg(message), MessageType::Feedback);
		return;
	}
	JSONNode nodes;
	nodes.push_back(JSONNode("channel", channelname.toStdString()));
	nodes.push_back(JSONNode("message", message.toStdString()));
	wsSend("MSG", nodes);
	//Send the message to the UI now.
	QString messagefinal = makeMessage(message.toHtmlEscaped(), character, getCharacter(character), channel);
	FMessage fmessage(messagefinal, MessageType::Chat);
	fmessage.toChannel(channelname).fromChannel(channelname).fromCharacter(this->character).fromSession(sessionid);
	account->ui->messageMessage(fmessage);
}
void FSession::sendChannelAdvertisement(QString channelname, QString message)
{
	//Confirm channel is known, joined and has the right permissions.
	FChannel *channel = getChannel(channelname);
	if(!channel) {
		account->ui->messageSystem(this, QString("Tried to send a message to '%1' but the channel is unknown or has never been joined. Message: %2").arg(channelname).arg(message), MessageType::Feedback);
		return;
	}
	if(!channel->isJoined()) {
		account->ui->messageSystem(this, QString("Tried to send a message to '%1' but you are not currently in the channel. Message: %2").arg(channelname).arg(message), MessageType::Feedback);
		return;
	}
	if(channel->mode == ChannelMode::Chat) {
		account->ui->messageSystem(this, QString("Tried to send a message to '%1' but the channel does not allow advertisements. Message: %2").arg(channelname).arg(message), MessageType::Feedback);
		return;
	}
	JSONNode nodes;
	nodes.push_back(JSONNode("channel", channelname.toStdString()));
	nodes.push_back(JSONNode("message", message.toStdString()));
	wsSend("LRP", nodes);
	//Send the message to the UI now.
	QString messagefinal = makeMessage(message.toHtmlEscaped(), character, getCharacter(character), channel, "<font color=\"green\"><b>Roleplay ad by</font> ", "");
	FMessage fmessage(messagefinal, MessageType::RpAd);
	fmessage.toChannel(channelname).fromChannel(channelname).fromCharacter(this->character).fromSession(sessionid);
	account->ui->messageMessage(fmessage);
}
void FSession::sendCharacterMessage(QString charactername, QString message)
{
	//Confirm character is known, online and we are not ignoring them.
	if(!isCharacterOnline(charactername)) {
		account->ui->messageSystem(this, QString("Tried to send a message to '%1' but they are offline or unknown. Message: %2").arg(charactername).arg(message), MessageType::Feedback);
		return;
	}
	if(isCharacterIgnored(charactername)) {
		account->ui->messageSystem(this, QString("Tried to send a message to '%1' but YOU are ignoring them. Message: %2").arg(charactername).arg(message), MessageType::Feedback);
		return;
	}
	//Make packet and send it.
	JSONNode nodes;
	nodes.push_back(JSONNode("recipient", charactername.toStdString()));
	nodes.push_back(JSONNode("message", message.toStdString()));
	wsSend("PRI", nodes);
	//Send the message to the UI now.
	QString messagefinal = makeMessage(message.toHtmlEscaped(), this->character, getCharacter(this->character));
	FMessage fmessage(messagefinal, MessageType::Chat);
	fmessage.toCharacter(charactername).fromCharacter(this->character).fromSession(sessionid);
	account->ui->messageMessage(fmessage);
}

void FSession::sendChannelLeave(QString channelname)
{
	JSONNode nodes;
	nodes.push_back(JSONNode("channel", channelname.toStdString()));
	wsSend("LCH", nodes);
} 

void FSession::sendConfirmStaffReport(QString callid)
{
	JSONNode nodes;
	nodes.push_back(JSONNode("action", "confirm"));
	nodes.push_back(JSONNode("moderator", character.toStdString()));
	nodes.push_back(JSONNode("callid", callid.toStdString()));
	wsSend("SFC", nodes);
}

void FSession::sendSubmitStaffReport(QString character, std::string logId, QString report) {
	JSONNode node;
	JSONNode actionnode("action", "report");
	JSONNode logidnode("logid", logId);
	JSONNode charnode("character", character.toStdString());
	JSONNode reportnode("report", report.toStdString());
	node.push_back(actionnode);
	node.push_back(charnode);
	node.push_back(reportnode);
	node.push_back(logidnode);
	wsSend("SFC", node);
}

void FSession::sendIgnoreAdd(QString character)
{
	character = character.toLower();
	JSONNode ignorenode;
	JSONNode targetnode ( "character", character.toStdString() );
	JSONNode actionnode ( "action", "add" );
	ignorenode.push_back ( targetnode );
	ignorenode.push_back ( actionnode );
	wsSend("IGN", ignorenode);
}

void FSession::sendIgnoreDelete(QString character)
{
	character = character.toLower();
	JSONNode ignorenode;
	JSONNode targetnode ( "character", character.toStdString() );
	JSONNode actionnode ( "action", "delete" );
	ignorenode.push_back ( targetnode );
	ignorenode.push_back ( actionnode );
	wsSend("IGN", ignorenode);
}

void FSession::sendStatus(QString status, QString statusmsg)
{
	JSONNode stanode;
	JSONNode statusnode ( "status", status.toStdString() );
	JSONNode stamsgnode ( "statusmsg", statusmsg.toStdString() );
	stanode.push_back ( statusnode );
	stanode.push_back ( stamsgnode );
	wsSend("STA", stanode);
}

void FSession::sendCharacterTimeout(QString character, int minutes, QString reason)
{
	JSONNode node;
	JSONNode charnode("character", character.toStdString());
	JSONNode timenode("time", QString::number(minutes).toStdString());
	JSONNode renode("reason", reason.toStdString());
	node.push_back ( charnode );
	node.push_back ( timenode );
	node.push_back ( renode );
	wsSend("TMO", node);
}

void FSession::sendTypingNotification(QString character, TypingStatus status)
{
	std::string statusText = "clear";
	switch(status)
	{
	case TYPING_STATUS_CLEAR: statusText = "clear"; break;
	case TYPING_STATUS_PAUSED: statusText = "paused"; break;
	case TYPING_STATUS_TYPING: statusText = "typing"; break;
	}

	JSONNode node;
	JSONNode statusnode("status", statusText);
	JSONNode charnode("character", character.toStdString());
	node.push_back(statusnode);
	node.push_back(charnode);
	wsSend("TPN", node);
}

void FSession::sendDebugCommand(QString payload)
{
	JSONNode node;
	JSONNode cmd("command", payload.toStdString());
	node.push_back(cmd);
	wsSend("ZZZ", node);
}

void FSession::kickFromChannel(QString channel, QString character)
{
	JSONNode kicknode;
	JSONNode charnode ( "character", character.toStdString() );
	JSONNode channode ( "channel", channel.toStdString() );
	kicknode.push_back ( charnode );
	kicknode.push_back ( channode );
	wsSend("CKU", kicknode);
}

void FSession::kickFromChat(QString character)
{
	JSONNode kicknode;
	JSONNode charnode ( "character", character.toStdString() );
	kicknode.push_back ( charnode );
	wsSend("KIK", kicknode);
}

void FSession::banFromChannel(QString channel, QString character)
{
	JSONNode bannode;
	JSONNode charnode ( "character", character.toStdString() );
	JSONNode channode ( "channel", channel.toStdString() );
	bannode.push_back ( charnode );
	bannode.push_back ( channode );
	wsSend("CBU", bannode);
}

void FSession::banFromChat(QString character)
{
	JSONNode node;
	JSONNode charnode ( "character", character.toStdString() );
	node.push_back ( charnode );
	wsSend("ACB", node);
}

void FSession::unbanFromChannel(QString channel, QString character)
{
	JSONNode bannode;
	JSONNode charnode ( "character", character.toStdString() );
	JSONNode channode ( "channel", channel.toStdString() );
	bannode.push_back ( charnode );
	bannode.push_back ( channode );
	wsSend("CUB", bannode);
}

void FSession::unbanFromChat(QString character)
{
	JSONNode node;
	JSONNode charnode ( "character", character.toStdString() );
	node.push_back ( charnode );
	wsSend("UNB", node);
}

void FSession::setRoomIsPublic(QString channel, bool isPublic)
{
	JSONNode statusnode;
	JSONNode channelNode("channel", channel.toStdString());
	JSONNode statNode("status", isPublic ? "public" : "private");
	statusnode.push_back(channelNode);
	statusnode.push_back(statNode);
	wsSend("RST", statusnode);
}

void FSession::inviteToChannel(QString channel, QString character)
{
	JSONNode invitenode;
	JSONNode charnode("character", character.toStdString());
	JSONNode channode("channel", channel.toStdString());
	invitenode.push_back(channode);
	invitenode.push_back(charnode);
	wsSend("CIU", invitenode);
}

void FSession::giveChanop(QString channel, QString character)
{
	JSONNode opnode;
	JSONNode charnode("character", character.toStdString());
	JSONNode channode("channel", channel.toStdString());
	opnode.push_back(charnode);
	opnode.push_back(channode);
	wsSend("COA", opnode);
}

void FSession::takeChanop(QString channel, QString character)
{
	JSONNode opnode;
	JSONNode charnode("character", character.toStdString());
	JSONNode channode("channel", channel.toStdString());
	opnode.push_back(charnode);
	opnode.push_back(channode);
	wsSend("COR", opnode);
}

void FSession::giveGlobalop(QString character)
{
	JSONNode opnode;
	JSONNode charnode("character", character.toStdString());
	opnode.push_back(charnode);
	wsSend("AOP", opnode);
}

void FSession::takeGlobalop(QString character)
{
	JSONNode opnode;
	JSONNode charnode("character", character.toStdString());
	opnode.push_back(charnode);
	wsSend("DOP", opnode);
}

void FSession::giveReward(QString character)
{
	JSONNode node;
	JSONNode charnode("character", character.toStdString());
	node.push_back(charnode);
	wsSend("RWD", node);
}

void FSession::requestChannelBanList(QString channel)
{
	JSONNode node;
	JSONNode channode("channel", channel.toStdString());
	node.push_back(channode);
	wsSend("CBL", node);
}

void FSession::requestChanopList(QString channel)
{
	JSONNode node;
	JSONNode channode("channel", channel.toStdString());
	node.push_back(channode);
	wsSend("COL", node);
}

void FSession::killChannel(QString channel)
{
	JSONNode node;
	JSONNode channode("channel", channel.toStdString());
	node.push_back(channode);
	wsSend("KIC", node);
}

void FSession::broadcastMessage(QString message)
{
	JSONNode node;
	JSONNode msgnode("message", message.toStdString());
	node.push_back(msgnode);
	wsSend("BRO", node);
}

void FSession::setChannelDescription(QString channelname, QString description)
{
	JSONNode node;
	JSONNode channode("channel", channelname.toStdString());
	JSONNode descnode("description", description.toStdString());
	node.push_back(channode);
	node.push_back(descnode);
	wsSend("CDS", node);
}

void FSession::setChannelMode(QString channel, ChannelMode mode)
{
	JSONNode node;
	JSONNode channode("channel", channel.toStdString());
	JSONNode modenode("mode", enumToKey(mode).toLower().toStdString());
	node.push_back(channode);
	node.push_back(modenode);
	wsSend("RMO", node);
}

void FSession::setChannelOwner(QString channel, QString newOwner)
{
	JSONNode node;
	JSONNode channode("channel", channel.toStdString());
	JSONNode ownernode("character", newOwner.toStdString());
	node.push_back(channode);
	node.push_back(ownernode);
	wsSend("CSO", node);
}

void FSession::spinBottle(QString channel)
{
	JSONNode node;
	JSONNode channode("channel",channel.toStdString());
	JSONNode dicenode("dice", "bottle");
	node.push_back(channode);
	node.push_back(dicenode);
	wsSend("RLL", node);
}

void FSession::rollDiceChannel(QString channel, QString dice)
{
	JSONNode node;
	JSONNode channode("channel",channel.toStdString());
	JSONNode dicenode("dice", dice.toStdString());
	node.push_back(channode);
	node.push_back(dicenode);
	wsSend("RLL", node);
}

void FSession::rollDicePM(QString recipient, QString dice)
{
	JSONNode node;
	JSONNode channode("recipient",recipient.toStdString());
	JSONNode dicenode("dice", dice.toStdString());
	node.push_back(channode);
	node.push_back(dicenode);
	wsSend("RLL", node);
}

void FSession::requestChannels()
{
	wsSend("CHA");
	wsSend("ORS");
}

void FSession::requestProfileKinks(QString character)
{
	JSONNode node;
	JSONNode cn("character", character.toStdString());
	node.push_back(cn);
	wsSend("PRO", node);
	wsSend("KIN", node);
}
