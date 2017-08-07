#ifndef FLIST_ENUMS_H
#define FLIST_ENUMS_H

#include <QString>

enum MessageType {
	MESSAGE_TYPE_LOGIN,
	MESSAGE_TYPE_ONLINE,
	MESSAGE_TYPE_OFFLINE,
	MESSAGE_TYPE_STATUS,
	MESSAGE_TYPE_CHANNEL_DESCRIPTION,
	MESSAGE_TYPE_CHANNEL_MODE,
	MESSAGE_TYPE_JOIN,
	MESSAGE_TYPE_LEAVE,
	MESSAGE_TYPE_CHANNEL_INVITE,
	MESSAGE_TYPE_KICK,
	MESSAGE_TYPE_KICKBAN,
	MESSAGE_TYPE_IGNORE_UPDATE,
	MESSAGE_TYPE_SYSTEM,
	MESSAGE_TYPE_REPORT,
	MESSAGE_TYPE_ERROR,
	MESSAGE_TYPE_BROADCAST,
	MESSAGE_TYPE_FEEDBACK,
	MESSAGE_TYPE_RPAD,
	MESSAGE_TYPE_CHAT,
	MESSAGE_TYPE_ROLL,
	MESSAGE_TYPE_NOTE,
	MESSAGE_TYPE_BOOKMARK,
	MESSAGE_TYPE_FRIEND,
};

enum TypingStatus {
	TYPING_STATUS_CLEAR,
	TYPING_STATUS_TYPING,
	TYPING_STATUS_PAUSED,
};

/*enum ChannelMode {
	//TODO: Use the unknown value for stuff
	CHANNEL_MODE_UNKNOWN,
	CHANNEL_MODE_CHAT,
	CHANNEL_MODE_ADS,
	CHANNEL_MODE_BOTH
};
#define CHANNEL_MODE_ENUM "unknown, chat, ads, both"
#define CHANNEL_MODE_DEFAULT "unknown"
extern EnumLookup ChannelModeEnum;*/

// These two functions are auto-implemented in flist_enums.cpp, and convert
// between FooEnum::SomeVal and QString("SomeVal").
template<typename T> QString enumToKey(T p);
template<typename T> T keyToEnum(QString s, T defval = (T)0);

#define ENUMDEF_ENUMMEMBER(y,x) x,
#define ENUMDEF_MAKE(what)                       \
	enum class what : int {                      \
	    ENUMDEF_##what(ENUMDEF_ENUMMEMBER,)      \
	};                                           \
	template<> QString enumToKey<what>(what p);  \
	template<> what keyToEnum<what>(QString s, what defval);

#include "flist_enums.def"

#endif // FLIST_ENUMS_H
