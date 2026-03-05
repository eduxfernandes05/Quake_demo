# Task: Infrastructure — Azure Deployment with Bicep IaC

**Phase**: 0–1 (Foundation + Headless Worker)  
**Priority**: P0  
**Estimated Effort**: 5–7 days  
**Prerequisites**: Docker images building for game worker

## Objective

Create Bicep IaC templates to provision all Azure resources for the cloud gaming platform, and configure `azd` for streamlined deployment.

## Acceptance Criteria

- [ ] `infra/main.bicep` provisions all required Azure resources
- [ ] Parameterized for dev / staging / prod environments
- [ ] `azd up` provisions infrastructure and deploys all services
- [ ] All secrets stored in Azure Key Vault with managed identity access
- [ ] Container Apps environment with VNet integration
- [ ] Azure Files shared volume for game assets
- [ ] Monitoring: Log Analytics + Application Insights configured
- [ ] ACR with Defender for Containers enabled
- [ ] All resources tagged with environment + project

## Azure Resources

| Resource | Purpose | Bicep Module |
| --- | --- | --- |
| Resource Group | Container for all resources | main.bicep |
| Container Registry (ACR) | Store container images | container-registry.bicep |
| Container Apps Environment | Host all ACA apps | container-apps-env.bicep |
| Container App: game-worker | Headless Quake engine | container-app.bicep |
| Container App: streaming-gateway | Video/audio streaming | container-app.bicep |
| Container App: session-manager | Session lifecycle API | container-app.bicep |
| Container App: assets-api | Serve game assets | container-app.bicep |
| Container App: telemetry-api | Metrics ingestion | container-app.bicep |
| Key Vault | Secrets management | key-vault.bicep |
| Storage Account (Files) | Shared game assets | storage.bicep |
| Storage Account (Blob) | Extracted PAK assets for CDN | storage.bicep |
| Cosmos DB | Session state | cosmos-db.bicep |
| Log Analytics Workspace | Centralized logging | monitoring.bicep |
| Application Insights | Metrics + traces | monitoring.bicep |
| Front Door + WAF | Edge security + load balancing | front-door.bicep (Prod only) |
| VNet + Subnets | Network isolation | networking.bicep |
| Entra ID App Registration | OAuth authentication | (manual or script) |

## Project Structure

```
infra/
├── main.bicep                    # Root orchestration
├── modules/
│   ├── container-registry.bicep
│   ├── container-apps-env.bicep
│   ├── container-app.bicep       # Generic — reused for each service
│   ├── key-vault.bicep
│   ├── storage.bicep
│   ├── cosmos-db.bicep
│   ├── monitoring.bicep
│   ├── networking.bicep
│   └── front-door.bicep
├── parameters/
│   ├── dev.bicepparam
│   ├── staging.bicepparam
│   └── prod.bicepparam
└── azure.yaml                    # azd project definition
```

## Implementation Steps

### 1. Main Orchestration (main.bicep)

```bicep
targetScope = 'resourceGroup'

param environmentName string
param location string = resourceGroup().location

module monitoring 'modules/monitoring.bicep' = {
  name: 'monitoring'
  params: { environmentName: environmentName, location: location }
}

module networking 'modules/networking.bicep' = {
  name: 'networking'
  params: { environmentName: environmentName, location: location }
}

module acr 'modules/container-registry.bicep' = {
  name: 'acr'
  params: { environmentName: environmentName, location: location }
}

module keyVault 'modules/key-vault.bicep' = {
  name: 'keyVault'
  params: { environmentName: environmentName, location: location }
}

module storage 'modules/storage.bicep' = {
  name: 'storage'
  params: { environmentName: environmentName, location: location }
}

module acaEnv 'modules/container-apps-env.bicep' = {
  name: 'acaEnv'
  params: {
    environmentName: environmentName
    location: location
    logAnalyticsWorkspaceId: monitoring.outputs.logAnalyticsWorkspaceId
    subnetId: networking.outputs.acaSubnetId
  }
}

// Deploy each service as a container app
module gameWorker 'modules/container-app.bicep' = { ... }
module streamingGateway 'modules/container-app.bicep' = { ... }
module sessionManager 'modules/container-app.bicep' = { ... }
module assetsApi 'modules/container-app.bicep' = { ... }
module telemetryApi 'modules/container-app.bicep' = { ... }
```

### 2. Container App Module (reusable)

```bicep
param appName string
param environmentId string
param imageName string
param cpu string = '0.5'
param memory string = '1Gi'
param minReplicas int = 0
param maxReplicas int = 10
param isExternal bool = false
param envVars array = []

resource containerApp 'Microsoft.App/containerApps@2024-03-01' = {
  name: appName
  location: location
  properties: {
    managedEnvironmentId: environmentId
    configuration: {
      ingress: isExternal ? { external: true, targetPort: 8080 } : null
      secrets: []
    }
    template: {
      containers: [{
        name: appName
        image: imageName
        resources: { cpu: json(cpu), memory: memory }
        env: envVars
      }]
      scale: {
        minReplicas: minReplicas
        maxReplicas: maxReplicas
      }
    }
  }
  identity: { type: 'SystemAssigned' }
}
```

### 3. Environment Parameters

**dev.bicepparam**:
```
environmentName = 'quake-dev'
// Minimal resources: 1 worker, no Front Door, no WAF
```

**prod.bicepparam**:
```
environmentName = 'quake-prod'
// Full resources: auto-scaled workers, Front Door + WAF, Defender enabled
```

### 4. azd Project

**azure.yaml**:
```yaml
name: quake-cloud
services:
  game-worker:
    project: ./WinQuake
    host: containerapp
    docker:
      path: ./Dockerfile
  streaming-gateway:
    project: ./streaming-gateway
    host: containerapp
  session-manager:
    project: ./session-manager
    host: containerapp
  assets-api:
    project: ./assets-api
    host: containerapp
  telemetry-api:
    project: ./telemetry-api
    host: containerapp
```

## Validation

```bash
# Provision dev environment
azd env new dev
azd provision

# Deploy all services
azd deploy

# Verify resources
az containerapp list -g quake-dev -o table
az keyvault list -g quake-dev -o table
az monitor log-analytics workspace list -g quake-dev -o table

# Verify worker health
az containerapp logs show -n game-worker -g quake-dev
```

## Risks

- Bicep module compatibility with ACA API version
- VNet integration may limit ACA features in some regions
- Front Door + ACA integration requires specific configuration

## Rollback

```bash
azd down  # Destroys all provisioned resources
```
