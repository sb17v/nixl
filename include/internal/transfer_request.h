/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __TRANSFER_REQUEST_H_
#define __TRANSFER_REQUEST_H_

// Contains pointers to corresponding backend engine and its handler, and populated
// and verified DescLists, and other state and metadata needed for a NIXL transfer
class nixlXferReqH {
    private:
        nixlBackendEngine* engine;
        nixlBackendReqH*   backendHandle;

        nixl_meta_dlist_t* initiatorDescs;
        nixl_meta_dlist_t* targetDescs;

        std::string        remoteAgent;
        std::string        notifMsg;

        nixl_xfer_op_t     backendOp;
        nixl_xfer_state_t  state;

    public:
        inline nixlXferReqH() {
            initiatorDescs = nullptr;
            targetDescs    = nullptr;
            engine         = nullptr;
            backendHandle  = nullptr;
        }

        inline ~nixlXferReqH() {
            // delete checks for nullptr itself
            delete initiatorDescs;
            delete targetDescs;
            if (backendHandle != nullptr)
                engine->releaseReqH(backendHandle);
        }

    friend class nixlAgent;
};

class nixlXferSideH {
    private:
        nixl_meta_dlist_t* descs;

        nixlBackendEngine* engine;
        std::string        remoteAgent;
        bool               isLocal;

    public:
        inline nixlXferSideH() {
            engine = nullptr;
        }

        inline ~nixlXferSideH() {
            delete descs;
        }

    friend class nixlAgent;
};

#endif
