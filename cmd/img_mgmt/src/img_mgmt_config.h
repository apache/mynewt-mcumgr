#ifndef H_IMG_MGMT_CONFIG_
#define H_IMG_MGMT_CONFIG_

#if defined MYNEWT

#include "syscfg/syscfg.h"

#define IMG_MGMT_UL_CHUNK_SIZE  MYNEWT_VAL(IMG_MGMT_UL_CHUNK_SIZE)

#elif defined __ZEPHYR__

#define IMG_MGMT_UL_CHUNK_SIZE  CONFIG_IMG_MGMT_UL_CHUNK_SIZE

#else

/* No direct support for this OS.  The application needs to define the above
 * settings itself.
 */

#endif

#endif
