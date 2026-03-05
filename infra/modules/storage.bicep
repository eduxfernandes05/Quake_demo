// modules/storage.bicep — Azure Files storage for game assets

param baseName string
param location string

// Storage account names: 3-24 lowercase alphanumeric
var storageAccountName = replace('${baseName}stg', '-', '')
var shareName = 'gamedata'

resource storageAccount 'Microsoft.Storage/storageAccounts@2023-05-01' = {
  name: storageAccountName
  location: location
  sku: {
    name: 'Standard_LRS'
  }
  kind: 'StorageV2'
  properties: {
    minimumTlsVersion: 'TLS1_2'
    supportsHttpsTrafficOnly: true
  }
}

resource fileService 'Microsoft.Storage/storageAccounts/fileServices@2023-05-01' = {
  parent: storageAccount
  name: 'default'
}

resource fileShare 'Microsoft.Storage/storageAccounts/fileServices/shares@2023-05-01' = {
  parent: fileService
  name: shareName
  properties: {
    shareQuota: 1 // 1 GiB — enough for shareware PAK files
  }
}

output accountName string = storageAccount.name
output shareName string = shareName
output accountId string = storageAccount.id
