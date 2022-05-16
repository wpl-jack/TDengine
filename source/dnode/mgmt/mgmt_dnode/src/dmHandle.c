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
 * along with this program. If not, see <http:www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE
#include "dmInt.h"

static void dmUpdateDnodeCfg(SDnodeMgmt *pMgmt, SDnodeCfg *pCfg) {
  if (pMgmt->data.dnodeId == 0 || pMgmt->data.clusterId == 0) {
    dInfo("set dnodeId:%d clusterId:%" PRId64, pCfg->dnodeId, pCfg->clusterId);
    taosWLockLatch(&pMgmt->data.latch);
    pMgmt->data.dnodeId = pCfg->dnodeId;
    pMgmt->data.clusterId = pCfg->clusterId;
    dmWriteEps(pMgmt);
    taosWUnLockLatch(&pMgmt->data.latch);
  }
}

static void dmProcessStatusRsp(SDnodeMgmt *pMgmt, SRpcMsg *pRsp) {
  if (pRsp->code != 0) {
    if (pRsp->code == TSDB_CODE_MND_DNODE_NOT_EXIST && !pMgmt->data.dropped && pMgmt->data.dnodeId > 0) {
      dInfo("dnode:%d, set to dropped since not exist in mnode", pMgmt->data.dnodeId);
      pMgmt->data.dropped = 1;
      dmWriteEps(pMgmt);
    }
  } else {
    SStatusRsp statusRsp = {0};
    if (pRsp->pCont != NULL && pRsp->contLen > 0 &&
        tDeserializeSStatusRsp(pRsp->pCont, pRsp->contLen, &statusRsp) == 0) {
      pMgmt->data.dnodeVer = statusRsp.dnodeVer;
      dmUpdateDnodeCfg(pMgmt, &statusRsp.dnodeCfg);
      dmUpdateEps(pMgmt, statusRsp.pDnodeEps);
    }
    rpcFreeCont(pRsp->pCont);
    tFreeSStatusRsp(&statusRsp);
  }
}

void dmSendStatusReq(SDnodeMgmt *pMgmt) {
  SStatusReq req = {0};

  taosRLockLatch(&pMgmt->data.latch);
  req.sver = tsVersion;
  req.dnodeVer = pMgmt->data.dnodeVer;
  req.dnodeId = pMgmt->data.dnodeId;
  req.clusterId = pMgmt->data.clusterId;
  if (req.clusterId == 0) req.dnodeId = 0;
  req.rebootTime = pMgmt->data.rebootTime;
  req.updateTime = pMgmt->data.updateTime;
  req.numOfCores = tsNumOfCores;
  req.numOfSupportVnodes = pMgmt->data.supportVnodes;
  tstrncpy(req.dnodeEp, pMgmt->data.localEp, TSDB_EP_LEN);

  req.clusterCfg.statusInterval = tsStatusInterval;
  req.clusterCfg.checkTime = 0;
  char timestr[32] = "1970-01-01 00:00:00.00";
  (void)taosParseTime(timestr, &req.clusterCfg.checkTime, (int32_t)strlen(timestr), TSDB_TIME_PRECISION_MILLI, 0);
  memcpy(req.clusterCfg.timezone, tsTimezoneStr, TD_TIMEZONE_LEN);
  memcpy(req.clusterCfg.locale, tsLocale, TD_LOCALE_LEN);
  memcpy(req.clusterCfg.charset, tsCharset, TD_LOCALE_LEN);
  taosRUnLockLatch(&pMgmt->data.latch);

  SMonVloadInfo vinfo = {0};
  dmGetVnodeLoads(pMgmt, &vinfo);
  req.pVloads = vinfo.pVloads;
  pMgmt->data.unsyncedVgId = 0;
  pMgmt->data.vndState = TAOS_SYNC_STATE_LEADER;
  for (int32_t i = 0; i < taosArrayGetSize(req.pVloads); ++i) {
    SVnodeLoad *pLoad = taosArrayGet(req.pVloads, i);
    if (pLoad->syncState != TAOS_SYNC_STATE_LEADER && pLoad->syncState != TAOS_SYNC_STATE_FOLLOWER) {
      pMgmt->data.unsyncedVgId = pLoad->vgId;
      pMgmt->data.vndState = pLoad->syncState;
    }
  }

  SMonMloadInfo minfo = {0};
  dmGetMnodeLoads(pMgmt, &minfo);
  pMgmt->data.mndState = minfo.load.syncState;

  int32_t contLen = tSerializeSStatusReq(NULL, 0, &req);
  void   *pHead = rpcMallocCont(contLen);
  tSerializeSStatusReq(pHead, contLen, &req);
  tFreeSStatusReq(&req);

  SRpcMsg rpcMsg = {.pCont = pHead, .contLen = contLen, .msgType = TDMT_MND_STATUS, .ahandle = (void *)0x9527};
  SRpcMsg rpcRsp = {0};

  dTrace("send req:%s to mnode, app:%p", TMSG_INFO(rpcMsg.msgType), rpcMsg.ahandle);
  tmsgSendMnodeRecv(&rpcMsg, &rpcRsp);
  dmProcessStatusRsp(pMgmt, &rpcRsp);
}

int32_t dmProcessAuthRsp(SDnodeMgmt *pMgmt, SNodeMsg *pMsg) {
  SRpcMsg *pRsp = &pMsg->rpcMsg;
  dError("auth rsp is received, but not supported yet");
  return 0;
}

int32_t dmProcessGrantRsp(SDnodeMgmt *pMgmt, SNodeMsg *pMsg) {
  SRpcMsg *pRsp = &pMsg->rpcMsg;
  dError("grant rsp is received, but not supported yet");
  return 0;
}

int32_t dmProcessConfigReq(SDnodeMgmt *pMgmt, SNodeMsg *pMsg) {
  SRpcMsg       *pReq = &pMsg->rpcMsg;
  SDCfgDnodeReq *pCfg = pReq->pCont;
  dError("config req is received, but not supported yet");
  return TSDB_CODE_OPS_NOT_SUPPORT;
}

static void dmGetServerRunStatus(SDnodeMgmt *pMgmt, SServerStatusRsp *pStatus) {
  pStatus->statusCode = TSDB_SRV_STATUS_SERVICE_OK;
  pStatus->details[0] = 0;

  SServerStatusRsp statusRsp = {0};
  SMonMloadInfo    minfo = {0};
  dmGetMnodeLoads(pMgmt, &minfo);
  if (minfo.isMnode && minfo.load.syncState != TAOS_SYNC_STATE_LEADER &&
      minfo.load.syncState != TAOS_SYNC_STATE_CANDIDATE) {
    pStatus->statusCode = TSDB_SRV_STATUS_SERVICE_DEGRADED;
    snprintf(pStatus->details, sizeof(pStatus->details), "mnode sync state is %s", syncStr(minfo.load.syncState));
    return;
  }

  SMonVloadInfo vinfo = {0};
  dmGetVnodeLoads(pMgmt, &vinfo);
  for (int32_t i = 0; i < taosArrayGetSize(vinfo.pVloads); ++i) {
    SVnodeLoad *pLoad = taosArrayGet(vinfo.pVloads, i);
    if (pLoad->syncState != TAOS_SYNC_STATE_LEADER && pLoad->syncState != TAOS_SYNC_STATE_FOLLOWER) {
      pStatus->statusCode = TSDB_SRV_STATUS_SERVICE_DEGRADED;
      snprintf(pStatus->details, sizeof(pStatus->details), "vnode:%d sync state is %s", pLoad->vgId,
               syncStr(pLoad->syncState));
      break;
    }
  }

  taosArrayDestroy(vinfo.pVloads);
}

int32_t dmProcessServerRunStatus(SDnodeMgmt *pMgmt, SNodeMsg *pMsg) {
  dDebug("server run status req is received");
  SServerStatusRsp statusRsp = {0};
  dmGetServerRunStatus(pMgmt, &statusRsp);

  SRpcMsg rspMsg = {.handle = pMsg->rpcMsg.handle, .ahandle = pMsg->rpcMsg.ahandle, .refId = pMsg->rpcMsg.refId};
  int32_t rspLen = tSerializeSServerStatusRsp(NULL, 0, &statusRsp);
  if (rspLen < 0) {
    rspMsg.code = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  void *pRsp = rpcMallocCont(rspLen);
  if (pRsp == NULL) {
    rspMsg.code = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  tSerializeSServerStatusRsp(pRsp, rspLen, &statusRsp);
  pMsg->pRsp = pRsp;
  pMsg->rspLen = rspLen;
  return 0;
}

SArray *dmGetMsgHandles() {
  int32_t code = -1;
  SArray *pArray = taosArrayInit(16, sizeof(SMgmtHandle));
  if (pArray == NULL) goto _OVER;

  // Requests handled by DNODE
  if (dmSetMgmtHandle(pArray, TDMT_DND_CREATE_MNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_DROP_MNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_CREATE_QNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_DROP_QNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_CREATE_SNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_DROP_SNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_CREATE_BNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_DROP_BNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_CONFIG_DNODE, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_DND_SERVER_STATUS, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;

  // Requests handled by MNODE
  if (dmSetMgmtHandle(pArray, TDMT_MND_GRANT_RSP, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;
  if (dmSetMgmtHandle(pArray, TDMT_MND_AUTH_RSP, dmPutNodeMsgToMgmtQueue, 0) == NULL) goto _OVER;

  code = 0;

_OVER:
  if (code != 0) {
    taosArrayDestroy(pArray);
    return NULL;
  } else {
    return pArray;
  }
}