using '../main.bicep'

param baseName = 'quake-dev'
param location = 'eastus2'
param logRetentionDays = 30
param vnetAddressPrefix = '10.0.0.0/16'
param infraSubnetPrefix = '10.0.0.0/23'
param workerImageTag = 'latest'
