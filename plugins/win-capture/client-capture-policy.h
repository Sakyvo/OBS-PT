#pragma once
#include <stdbool.h>

static inline bool client_capture_should_attempt(bool force_legacy,
					 bool compatibility, unsigned failures)
{
	return !force_legacy && !compatibility && failures < 2;
}
