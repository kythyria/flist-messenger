#ifndef FLIST_ENUMS_H
#define FLIST_ENUMS_H

#include <QString>
#include <QDebug>

enum TypingStatus {
	TYPING_STATUS_CLEAR,
	TYPING_STATUS_TYPING,
	TYPING_STATUS_PAUSED,
};

// These two functions are auto-implemented in flist_enums.cpp, and convert
// between FooEnum::SomeVal and QString("SomeVal").
template<typename T> QString enumToKey(T p);
template<typename T> T keyToEnum(QString s, T defval = (T)0);

#define ENUMDEF_ENUMMEMBER(y,x) x,
#define ENUMDEF_MAKE(what)                       \
	enum class what : int {                      \
	    ENUMDEF_##what(ENUMDEF_ENUMMEMBER,)      \
	    Max                                      \
	};                                           \
	template<> QString enumToKey<what>(what p);  \
	template<> what keyToEnum<what>(QString s, what defval); \
	QDebug operator<<(QDebug dbg, const what val);

#include "flist_enums.def"

#endif // FLIST_ENUMS_H
