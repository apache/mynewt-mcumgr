#ifndef H_FS_MGMT_CONFIG_
#define H_FS_MGMT_CONFIG_

#if defined MYNEWT

#include "syscfg/syscfg.h"

#define FS_MGMT_DL_CHUNK_SIZE   MYNEWT_VAL(FS_MGMT_DL_CHUNK_SIZE)
#define FS_MGMT_PATH_SIZE       MYNEWT_VAL(FS_MGMT_PATH_SIZE)
#define FS_MGMT_UL_CHUNK_SIZE   MYNEWT_VAL(FS_MGMT_UL_CHUNK_SIZE)

#elif defined __ZEPHYR__

#define FS_MGMT_DL_CHUNK_SIZE   CONFIG_FS_MGMT_DL_CHUNK_SIZE
#define FS_MGMT_PATH_SIZE       CONFIG_FS_MGMT_PATH_SIZE
#define FS_MGMT_UL_CHUNK_SIZE   CONFIG_FS_MGMT_UL_CHUNK_SIZE

#else

/* No direct support for this OS.  The application needs to define the above
 * settings itself.
 */

#endif

#endif
