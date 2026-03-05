# Contingency Plans

## Emergency Procedures

### CP-01: Game Worker Instability (Crash Loops)

**Trigger**: Worker restarts > 3 times in 10 minutes, or crash rate > 5% of active sessions.

**Immediate Actions** (< 15 minutes):
1. Check ACA logs: `az containerapp logs show -n game-worker -g <rg>`
2. Identify crash pattern (segfault? OOM? specific map?)
3. If specific map/environment: disable that map via config, restart workers
4. If systematic: rollback worker image to previous revision

**Short-term** (< 4 hours):
1. Reproduce crash locally with AddressSanitizer build
2. Capture core dump if available
3. Deploy hotfix to staging, validate, promote to production

**Escalation**: If unfixable within 4 hours, put service in maintenance mode (session creation disabled, existing sessions continue).

---

### CP-02: Streaming Quality Degradation

**Trigger**: P95 input-to-display latency > 200ms, or frame rate < 20 fps for > 20% of sessions.

**Immediate Actions** (< 15 minutes):
1. Check container CPU/memory metrics — are workers overloaded?
2. Check network metrics — bandwidth saturation?
3. If CPU: reduce encoding quality (`-preset ultrafast`, lower resolution)
4. If network: reduce bitrate target

**Short-term** (< 4 hours):
1. Scale up worker/gateway container resources (more CPU)
2. Adjust adaptive bitrate parameters
3. If ACA capacity-bound: increase max replicas

**Fallback**: Drop to 540p@24fps globally until root cause resolved.

---

### CP-03: Authentication Service Down

**Trigger**: Microsoft Entra ID is unreachable or returning errors.

**Immediate Actions** (< 5 minutes):
1. Existing sessions continue (already have tokens)
2. New session creation fails — show user-friendly error with retry guidance
3. Check Azure Status page for Entra ID outage

**Short-term**:
1. If Entra ID outage: wait for resolution (external dependency)
2. Consider cached token validation (JWT verification with cached signing keys — already standard)
3. Signing key cache TTL: 24 hours — can validate tokens offline

**Fallback**: Emergency mode — allow anonymous sessions with reduced functionality and rate limits (only if business approves).

---

### CP-04: Azure Container Apps Environment Failure

**Trigger**: Entire ACA environment unresponsive, all services down.

**Immediate Actions** (< 15 minutes):
1. Check Azure Status page for regional outage
2. If regional outage: redirect Front Door to secondary region (if multi-region)
3. If ACA-specific: contact Azure support (Sev A)

**Short-term** (< 1 hour):
1. Re-deploy all services from ACR images to new ACA environment
2. Use Bicep IaC: `az deployment group create` with alternate ACA environment
3. Update Front Door backend to point to new environment

**Long-term**: Implement multi-region active-passive with automated failover.

---

### CP-05: Data Breach / Security Incident

**Trigger**: Defender for Cloud alert, unusual container activity, reported vulnerability exploitation.

**Immediate Response** (< 15 minutes):
1. **Contain**: Isolate affected containers — scale to 0 or network disconnect
2. **Assess**: Review Defender alerts, Log Analytics security queries
3. **Communicate**: Notify security team lead

**Investigation** (< 4 hours):
1. Preserve logs: export Log Analytics data for affected time period
2. Preserve container state: snapshot if possible before termination
3. Identify attack vector: WAF logs, audit logs, container logs
4. Determine data exposure: what data was accessible?

**Remediation**:
1. Patch vulnerability or reconfigure affected service
2. Rotate all secrets in Key Vault
3. Rotate Entra ID app credentials
4. Rebuild container images from verified source
5. Re-deploy all services from fresh images

**Post-incident**:
1. Incident report within 48 hours
2. If PII exposed: GDPR notification within 72 hours
3. Update threat model and security controls
4. Add detection rule for specific attack pattern

---

### CP-06: Cloud Cost Runaway

**Trigger**: Daily spend > 200% of 7-day average, or budget alert at 100%.

**Immediate Actions** (< 30 minutes):
1. Check ACA scaling: are containers scaling beyond expected limits?
2. Check for DDoS triggering auto-scale
3. If abuse: enable aggressive rate limiting, block offending IPs

**Cost Reduction** (< 2 hours):
1. Reduce max replicas to limit spending while investigating
2. Shorten idle session timeout (15 min → 5 min)
3. Reduce pre-warmed pool size

**Root Cause**:
1. Audit session creation logs: is it legitimate traffic or abuse?
2. Check for resource leaks: sessions not cleaning up containers?
3. Review auto-scaling rules: are thresholds too aggressive?

---

### CP-07: Performance Degradation — Mesa/Encoding Pipeline

**Trigger**: Proof of concept in Phase 1 shows LLVMpipe cannot meet 720p@30fps target.

**Fallback 1: Reduce quality** (fastest to implement)
- Drop to 540p@24fps or 480p@30fps
- Disable expensive GL features (shadows, particles)
- Acceptable for demo/prototype

**Fallback 2: Software renderer** (moderate effort)
- Use original Quake software renderer instead of GL path
- Simpler framebuffer capture (no FBO readback)
- Lower visual quality but predictable performance
- Requires different `VID_CaptureFrame` implementation (read from `vid.buffer`)

**Fallback 3: GPU instances** (highest cost)
- ACA with GPU SKU or migrate to AKS with GPU node pool
- Hardware GL rendering + NVENC encoding
- < 5ms combined render + encode per frame
- Cost: 3-5x per session vs. CPU-only

**Fallback 4: Hybrid architecture** (complex)
- Render on CPU, encode on shared GPU
- Use GPU container as encoding service, multiple workers share one encoder
- Better cost efficiency than per-worker GPU

---

### CP-08: Azure Container Apps Service Limitations

**Trigger**: ACA lacks required feature (shared memory, GPU, custom networking, etc.).

**Contingency: Migrate to AKS**
1. ACA and AKS both use containers — same images, same ACR
2. Replace ACA Bicep modules with AKS modules
3. Create Kubernetes manifests (Deployments, Services, Ingress) from ACA config
4. Use KEDA for auto-scaling (same autoscale semantics as ACA)
5. Additional operational complexity but more flexibility

**Migration effort**: ~1-2 weeks to convert IaC + deployment pipeline. Application code unchanged.

---

## Contingency Decision Tree

```
Service issue detected
├── Single service affected?
│   ├── Yes → Rollback that service's ACA revision
│   └── No → Check Azure status page
│       ├── Azure outage → Wait or failover to secondary region
│       └── Not Azure → Full investigation
│
├── Performance problem?
│   ├── CPU-bound → Scale up resources or reduce quality
│   ├── Network-bound → Reduce bitrate or enable TURN
│   └── Unknown → Isolate, profile, reproduce
│
├── Security incident?
│   ├── Contain immediately
│   ├── Preserve evidence
│   └── Follow CP-05 procedure
│
└── Cost problem?
    ├── Abuse → Block + rate limit
    ├── Scaling → Reduce max replicas
    └── Leak → Audit session cleanup
```

## Contingency Testing Schedule

| Procedure | Frequency | Environment | Method |
| --- | --- | --- | --- |
| CP-01: Worker rollback | Monthly | Staging | Deploy bad image, execute rollback |
| CP-02: Quality fallback | Quarterly | Staging | Artificially constrain CPU, verify adaptation |
| CP-04: Environment rebuild | Semi-annual | Staging | Full teardown and re-provision |
| CP-05: Incident response | Annual | Tabletop | Simulated security incident walkthrough |
| CP-06: Cost containment | Monthly | Production | Verify budget alerts fire correctly |
