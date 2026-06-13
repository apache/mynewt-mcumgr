<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# Agent guidance

This file is read by automated agents (security scanners, code analyzers,
AI assistants) operating on this repository. It points them at the
human-authored references they should consult before producing output.

## Security

Security model: [SECURITY.md](./SECURITY.md), which links to the project's
threat model at [THREAT_MODEL.md](./THREAT_MODEL.md).

Apache mcumgr is the device-side server for SMP (the Simple Management
Protocol). Before reporting a security issue, consult `THREAT_MODEL.md`:
SMP provides no authentication, authorization, or encryption of its own
(those are the transport's and the integrator's responsibility), and an
uploaded firmware image is gated by the bootloader's signature verification
(MCUboot), not by mcumgr. The §11a "known non-findings" list enumerates the
reports that are out of model by design; the in-model surface is memory
safety in the frame-parse / dispatch / CBOR-decode path.
