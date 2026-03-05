// modules/game-worker.bicep — Game worker Container App

param baseName string
param location string
param containerAppsEnvId string
param acrLoginServer string
param workerImageTag string
param storageAccountName string
param storageShareName string

var appName = '${baseName}-worker'

resource gameWorker 'Microsoft.App/containerApps@2024-03-01' = {
  name: appName
  location: location
  properties: {
    managedEnvironmentId: containerAppsEnvId
    configuration: {
      ingress: {
        external: false
        targetPort: 8080
        transport: 'http'
      }
      registries: [
        {
          server: acrLoginServer
          identity: 'system'
        }
      ]
    }
    template: {
      containers: [
        {
          name: 'quake-worker'
          image: '${acrLoginServer}/quake-worker:${workerImageTag}'
          resources: {
            cpu: json('1.0')
            memory: '2Gi'
          }
          env: [
            {
              name: 'QUAKE_BASEDIR'
              value: '/game'
            }
            {
              name: 'QUAKE_HEALTH_PORT'
              value: '8080'
            }
          ]
          volumeMounts: [
            {
              volumeName: 'gamedata'
              mountPath: '/game'
            }
          ]
          probes: [
            {
              type: 'Liveness'
              httpGet: {
                port: 8080
                path: '/healthz'
              }
              initialDelaySeconds: 5
              periodSeconds: 10
            }
            {
              type: 'Readiness'
              httpGet: {
                port: 8080
                path: '/healthz'
              }
              initialDelaySeconds: 3
              periodSeconds: 5
            }
          ]
        }
      ]
      scale: {
        minReplicas: 0
        maxReplicas: 1
      }
      volumes: [
        {
          name: 'gamedata'
          storageType: 'AzureFile'
          storageName: storageAccountName
        }
      ]
    }
  }
  identity: {
    type: 'SystemAssigned'
  }
}

output workerFqdn string = gameWorker.properties.configuration.ingress.fqdn
