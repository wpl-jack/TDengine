/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TD_SDB_INT_H_
#define _TD_SDB_INT_H_

#include "os.h"

#include "sdb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define mFatal(...) { if (mDebugFlag & DEBUG_FATAL) { taosPrintLog("MND FATAL ", DEBUG_FATAL, 255, __VA_ARGS__); }}
#define mError(...) { if (mDebugFlag & DEBUG_ERROR) { taosPrintLog("MND ERROR ", DEBUG_ERROR, 255, __VA_ARGS__); }}
#define mWarn(...)  { if (mDebugFlag & DEBUG_WARN)  { taosPrintLog("MND WARN ", DEBUG_WARN, 255, __VA_ARGS__); }}
#define mInfo(...)  { if (mDebugFlag & DEBUG_INFO)  { taosPrintLog("MND ", DEBUG_INFO, 255, __VA_ARGS__); }}
#define mDebug(...) { if (mDebugFlag & DEBUG_DEBUG) { taosPrintLog("MND ", DEBUG_DEBUG, mDebugFlag, __VA_ARGS__); }}
#define mTrace(...) { if (mDebugFlag & DEBUG_TRACE) { taosPrintLog("MND ", DEBUG_TRACE, mDebugFlag, __VA_ARGS__); }}

typedef struct SSdbRaw {
  int8_t  type;
  int8_t  status;
  int8_t  sver;
  int8_t  reserved;
  int32_t dataLen;
  char    pData[];
} SSdbRaw;

typedef struct SSdbRow {
  ESdbType   type;
  ESdbStatus status;
  int32_t    refCount;
  char       pObj[];
} SSdbRow;

const char *sdbTableName(ESdbType type);
void        sdbPrintOper(SSdb *pSdb, SSdbRow *pRow, const char *oper);

void sdbFreeRow(SSdb *pSdb, SSdbRow *pRow, bool callFunc);

#ifdef __cplusplus
}
#endif

#endif /*_TD_SDB_INT_H_*/