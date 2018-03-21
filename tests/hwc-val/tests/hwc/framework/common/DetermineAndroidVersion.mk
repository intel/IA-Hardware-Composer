# define ANDROID_VERSION (e.g., 4.3.2 would be 432)
ifneq (, $(filter $(PLATFORM_VERSION),L Lollipop%))
    major := 5
    minor := 0
    rev   := 0
else
    ifeq ($(PLATFORM_VERSION), O)
        major := 8
        minor := 0
        rev   := 0
    else
        major := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
        minor := $(word 2, $(subst ., , $(PLATFORM_VERSION).0))
        rev   := $(word 3, $(subst ., , $(PLATFORM_VERSION).0.0))
    endif
endif
