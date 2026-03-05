// modules/acr.bicep — Azure Container Registry

param baseName string
param location string

// ACR names must be alphanumeric, 5-50 chars
var acrName = replace('${baseName}acr', '-', '')

resource acr 'Microsoft.ContainerRegistry/registries@2023-11-01-preview' = {
  name: acrName
  location: location
  sku: {
    name: 'Basic'
  }
  properties: {
    adminUserEnabled: false
  }
}

output loginServer string = acr.properties.loginServer
output acrId string = acr.id
