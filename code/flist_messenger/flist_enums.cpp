#include "flist_enums.h"
#include <QHash>

#define ENUMDEF_STRINGTABENTRY(Y,X) QString(#X),
#define ENUMDEF_HASHINITENTRY(Y,X) { QString(#X), Y::X },

#define ENUMDEF_MAKE(enum_type)                          \
	namespace {                                          \
	    QString enum_type##_strings[] = {                \
	        ENUMDEF_##enum_type(ENUMDEF_STRINGTABENTRY,) \
	    };                                               \
	    QHash<QString, enum_type> enum_type##_ints = {   \
	         ENUMDEF_##enum_type(ENUMDEF_HASHINITENTRY,enum_type) \
	    };                                                  \
	}                                                       \
	template<> QString enumToKey<enum_type>(enum_type p) {  \
	    return enum_type##_strings[(int)p];                 \
	}                                                       \
	template<> enum_type keyToEnum<enum_type>(QString s, enum_type defval) { \
	    return enum_type##_ints.value(s, defval); \
	}                                             

#include "flist_enums.def"
