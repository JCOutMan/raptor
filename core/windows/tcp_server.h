/*
 *
 * Copyright (c) 2020 The Raptor Authors. All rights reserved.
 *
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
 *
 */

#ifndef __RAPTOR_CORE_WINDOWS_TCP_SERVER__
#define __RAPTOR_CORE_WINDOWS_TCP_SERVER__

#include <stdint.h>
#include <time.h>

#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "core/mpscq.h"
#include "core/resolve_address.h"
#include "core/windows/connection.h"
#include "core/windows/iocp_thread.h"
#include "util/atomic.h"
#include "util/sync.h"
#include "util/time.h"
#include "util/status.h"
#include "raptor/protocol.h"
#include "raptor/service.h"

namespace raptor {
class IProtocol;
class TcpListener;
struct TcpMessageNode;
class TcpServer : public internal::IAcceptor
                , public internal::IIocpReceiver
                , public internal::INotificationTransfer {
public:
    explicit TcpServer(IServerReceiver *service);
    ~TcpServer();

    raptor_error Init(const RaptorOptions* options);
    raptor_error AddListening(const char* addr);
    raptor_error Start();
    void Shutdown();
    void SetProtocol(IProtocol* proto);

    bool Send(ConnectionId cid, const void* buf, size_t len);
    bool SendWithHeader(ConnectionId cid,
        const void* hdr, size_t hdr_len, const void* data, size_t data_len);
    bool CloseConnection(ConnectionId cid);

    // internal::IAcceptor impl
    void OnNewConnection(
        SOCKET sock, int listen_port,
        const raptor_resolved_address* addr) override;

    // internal::IIocpReceiver impl
    void OnErrorEvent(void* ptr, size_t err_code) override;
    void OnRecvEvent(void* ptr, size_t transferred_bytes) override;
    void OnSendEvent(void* ptr, size_t transferred_bytes) override;
    void OnCheckingEvent(time_t current) override;

    // internal::INotificationTransfer impl
    void OnConnectionArrived(ConnectionId cid, const raptor_resolved_address* addr) override;
    void OnDataReceived(ConnectionId cid, const Slice* s) override;
    void OnConnectionClosed(ConnectionId cid) override;

    // user data
    bool SetUserData(ConnectionId cid, void* ptr);
    bool GetUserData(ConnectionId cid, void** ptr);
    bool SetExtendInfo(ConnectionId cid, uint64_t data);
    bool GetExtendInfo(ConnectionId cid, uint64_t& data);

private:

    void MessageQueueThread(void*);
    uint32_t CheckConnectionId(ConnectionId cid) const;
    void Dispatch(struct TcpMessageNode* msg);
    void DeleteConnection(uint32_t index);
    void RefreshTime(uint32_t index);
    std::shared_ptr<Connection> GetConnection(uint32_t index);

private:

    using TimeoutRecord = std::multimap<time_t, uint32_t>;
    using ConnectionData =
        std::pair<std::shared_ptr<Connection>, TimeoutRecord::iterator>;

    enum { RESERVED_CONNECTION_COUNT = 100 };

    IServerReceiver* _service;
    IProtocol* _proto;

    bool _shutdown;
    RaptorOptions _options;

    MultiProducerSingleConsumerQueue _mpscq;
    Thread _mq_thd;
    Mutex _mutex;
    ConditionVariable _cv;
    AtomicUInt32 _count;

    std::shared_ptr<SendRecvThread> _rs_thread;
    std::shared_ptr<TcpListener> _listener;

    Mutex _conn_mtx;
    std::vector<ConnectionData> _mgr;
    // key: timeout deadline, value: index for _mgr
    TimeoutRecord _timeout_record_list;
    std::list<uint32_t> _free_index_list;
    uint16_t _magic_number;
    Atomic<time_t> _last_timeout_time;
};
} // namespace raptor
#endif  // __RAPTOR_CORE_WINDOWS_TCP_SERVER__
