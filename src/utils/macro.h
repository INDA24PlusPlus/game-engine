#define GENERATE_ENUM_ELEMENT(EL) EL,

#define GENERATE_ENUM(NAME, FOREACH_LIST) \
    enum NAME { \
        FOREACH_LIST(GENERATE_ENUM_ELEMENT) \
    };

#define GENERATE_ENUM_TO_STR(EL) case EL: return #EL;

#define GENERATE_ENUM_TO_STR_FUNC(NAME) \
    const char * NAME##_to_string(enum NAME el)

#define GENERATE_ENUM_TO_STR_FUNC_BODY(NAME, FOREACH_LIST) \
    GENERATE_ENUM_TO_STR_FUNC(NAME) { \
        switch(el) { \
            FOREACH_LIST(GENERATE_ENUM_TO_STR) \
            default: return "UNDEFINED"; \
        } \
    }
