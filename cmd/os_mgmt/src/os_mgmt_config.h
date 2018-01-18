#ifndef H_OS_MGMT_CONFIG_
#define H_OS_MGMT_CONFIG_

#if defined MYNEWT

#include "syscfg/syscfg.h"

#define OS_MGMT_RESET_MS    MYNEWT_VAL(OS_MGMT_RESET_MS)

#elif defined __ZEPHYR__

#define OS_MGMT_RESET_MS    CONFIG_OS_MGMT_RESET_MS

#else

/* No direct support for this OS.  The application needs to define the above
 * settings itself.
 */

#endif

#endif
