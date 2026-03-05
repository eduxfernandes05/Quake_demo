// infra/main.bicep — Azure infrastructure for Quake Cloud Streaming Platform
//
// Provisions all Azure resources required by Steps 7–11:
//   - Azure Container Registry (ACR)
//   - Azure Container Apps environment (with VNet)
//   - Azure Files storage account (game assets)
//   - Azure Key Vault
//   - Azure Log Analytics workspace + Application Insights
//
// Usage:
//   az deployment group create \
//     --resource-group <rg> \
//     --template-file infra/main.bicep \
//     --parameters infra/parameters/dev.bicepparam

targetScope = 'resourceGroup'

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

@description('Base name prefix for all resources')
param baseName string

@description('Azure region for all resources')
param location string = resourceGroup().location

@description('Log Analytics retention in days')
@minValue(30)
@maxValue(730)
param logRetentionDays int = 30

@description('Container Apps environment VNet address prefix')
param vnetAddressPrefix string = '10.0.0.0/16'

@description('Container Apps infrastructure subnet prefix')
param infraSubnetPrefix string = '10.0.0.0/23'

@description('Container image tag for the game worker')
param workerImageTag string = 'latest'

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

module logAnalytics 'modules/log-analytics.bicep' = {
  name: 'log-analytics'
  params: {
    baseName: baseName
    location: location
    retentionDays: logRetentionDays
  }
}

module containerRegistry 'modules/acr.bicep' = {
  name: 'container-registry'
  params: {
    baseName: baseName
    location: location
  }
}

module keyVault 'modules/key-vault.bicep' = {
  name: 'key-vault'
  params: {
    baseName: baseName
    location: location
  }
}

module storageAccount 'modules/storage.bicep' = {
  name: 'storage-account'
  params: {
    baseName: baseName
    location: location
  }
}

module containerAppsEnv 'modules/container-apps-env.bicep' = {
  name: 'container-apps-env'
  params: {
    baseName: baseName
    location: location
    logAnalyticsWorkspaceId: logAnalytics.outputs.workspaceId
    logAnalyticsSharedKey: logAnalytics.outputs.sharedKey
    vnetAddressPrefix: vnetAddressPrefix
    infraSubnetPrefix: infraSubnetPrefix
  }
}

module gameWorker 'modules/game-worker.bicep' = {
  name: 'game-worker'
  params: {
    baseName: baseName
    location: location
    containerAppsEnvId: containerAppsEnv.outputs.environmentId
    acrLoginServer: containerRegistry.outputs.loginServer
    workerImageTag: workerImageTag
    storageAccountName: storageAccount.outputs.accountName
    storageShareName: storageAccount.outputs.shareName
  }
}

// ---------------------------------------------------------------------------
// Outputs
// ---------------------------------------------------------------------------

output acrLoginServer string = containerRegistry.outputs.loginServer
output keyVaultUri string = keyVault.outputs.vaultUri
output appInsightsConnectionString string = logAnalytics.outputs.appInsightsConnectionString
output storageAccountName string = storageAccount.outputs.accountName
output containerAppsEnvId string = containerAppsEnv.outputs.environmentId
