
#include "flist_enums.h"

#include <QStringList>

EnumLookup AttentionModeEnum(QString(ATTENTION_MODE_ENUM));
EnumLookup BoolTristate(QString(BOOL_TRISTATE_ENUM), QString(BOOL_TRISTATE_DEFAULT));
EnumLookup ChannelModeEnum(QString(CHANNEL_MODE_ENUM), QString(CHANNEL_MODE_DEFAULT));


EnumLookup::EnumLookup(QString enumlist, QString dflt)
{
	QStringList keys = enumlist.split(",");
	valuelookup.reserve(keys.size());
	keylookup.reserve(keys.size());
	for(int i = 0; i < keys.size(); i++) {
		QString s = keys[i].trimmed();
		valuelookup[s] = i;
		keylookup[i] = s;
	}
	if(dflt.isEmpty()) {
		defaultvalue = 0;
		defaultkey = keylookup[defaultvalue];
	} else {
		defaultkey = dflt;
		defaultvalue = valuelookup[defaultkey];
	}
}

#define ENUMDEF_STRINGTABENTRY(Y,X) QString(#X),
#define ENUMDEF_HASHINITENTRY(Y,X) { QString(#X), Y::X },

#define ENUMDEF_MAKE(enum_type)                                \
	namespace {                                                \
	    QString enum_type##_strings[] = {                      \
	        ENUMDEF_##enum_type(ENUMDEF_STRINGTABENTRY,)        \
	    };                                                     \
	    QHash<QString, enum_type> enum_type##_ints = {            \
	         ENUMDEF_##enum_type(ENUMDEF_HASHINITENTRY,enum_type)      \
	    };                                                     \
	}                                                          \
	template<> QString enumToKey<enum_type>(enum_type p) {     \
	    return enum_type##_strings[(int)p];                    \
	}                                                          \
	template<> enum_type keyToEnum<enum_type>(QString s) {     \
	    return enum_type##_ints[s];                            \
	}                                                          

#include "flist_enums.def"
