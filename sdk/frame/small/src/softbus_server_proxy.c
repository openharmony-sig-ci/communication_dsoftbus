/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "iproxy_client.h"
#include "liteipc_adapter.h"
#include "samgr_lite.h"
#include "softbus_def.h"
#include "softbus_errcode.h"
#include "softbus_ipc_def.h"
#include "softbus_log.h"
#include "softbus_os_interface.h"
#include "softbus_server_proxy.h"

static IClientProxy *g_serverProxy = NULL;

static int ClientSimpleResultCb(IOwner owner, int code, IpcIo *reply)
{
    *(int *)owner = IpcIoPopInt32(reply);
    LOG_INFO("retvalue:%d", *(int *)owner);
    return EC_SUCCESS;
}

static IClientProxy *GetServerProxy(void)
{
    IClientProxy *clientProxy = NULL;
    IUnknown *iUnknown = NULL;
    int ret;

    LOG_INFO("start get client proxy");
    while (clientProxy == NULL) {
        iUnknown = SAMGR_GetInstance()->GetDefaultFeatureApi(SOFTBUS_SERVICE);
        if (iUnknown == NULL) {
            SoftBusSleepMs(WAIT_SERVER_READY_INTERVAL);
            continue;
        }

        ret = iUnknown->QueryInterface(iUnknown, CLIENT_PROXY_VER, (void **)&clientProxy);
        if (ret != EC_SUCCESS || clientProxy == NULL) {
            LOG_ERR("QueryInterface failed [%d]", ret);
            SoftBusSleepMs(WAIT_SERVER_READY_INTERVAL);
            continue;
        }
    }

    LOG_INFO("get client proxy ok");
    return clientProxy;
}

int32_t RegisterService(const char *name, const struct CommonScvId *svcId)
{
    LOG_INFO("server register service client push.");
    if ((svcId == NULL) || (name == NULL)) {
        LOG_ERR("Invalid param");
        return SOFTBUS_INVALID_PARAM;
    }
    uint8_t data[MAX_SOFT_BUS_IPC_LEN] = {0};

    IpcIo request = {0};
    IpcIoInit(&request, data, MAX_SOFT_BUS_IPC_LEN, 1);
    IpcIoPushString(&request, name);

    SvcIdentity svc = {0};
    svc.handle = svcId->handle;
    svc.token = svcId->token;
    svc.cookie = svcId->cookie;
#ifdef __LINUX__
    svc.ipcContext = svcId->ipcCtx;
#endif
    IpcIoPushSvc(&request, &svc);

    int ret = SOFTBUS_ERR;
    if (g_serverProxy->Invoke(g_serverProxy, MANAGE_REGISTER_SERVICE, &request, &ret,
        ClientSimpleResultCb) != EC_SUCCESS) {
        LOG_INFO("Call back ret(%d)", ret);
        return SOFTBUS_ERR;
    }
    return ret;
}

void __attribute__((weak)) HOS_SystemInit(void)
{
    SAMGR_Bootstrap();
    return;
}

int32_t ServerProxyInit(void)
{
    HOS_SystemInit();
    g_serverProxy = GetServerProxy();
    if (g_serverProxy == NULL) {
        LOG_ERR("get ipc client proxy failed");
        return SOFTBUS_ERR;
    }
    LOG_INFO("ServerProvideInterfaceInit ok");
    return SOFTBUS_OK;
}