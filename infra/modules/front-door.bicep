// modules/front-door.bicep — Azure Front Door with WAF v2
//
// Provides global load balancing, DDoS protection, and WAF rules
// for all public-facing APIs.

param baseName string
param location string = 'global'
param sessionManagerFqdn string
param gatewayFqdn string

var frontDoorName = '${baseName}-fd'
var wafPolicyName = '${baseName}-waf'

// WAF Policy with managed rule sets
resource wafPolicy 'Microsoft.Network/FrontDoorWebApplicationFirewallPolicies@2024-02-01' = {
  name: wafPolicyName
  location: location
  sku: {
    name: 'Premium_AzureFrontDoor'
  }
  properties: {
    policySettings: {
      enabledState: 'Enabled'
      mode: 'Prevention'
      requestBodyCheck: 'Enabled'
    }
    managedRules: {
      managedRuleSets: [
        {
          ruleSetType: 'Microsoft_DefaultRuleSet'
          ruleSetVersion: '2.1'
          ruleSetAction: 'Block'
        }
        {
          ruleSetType: 'Microsoft_BotManagerRuleSet'
          ruleSetVersion: '1.1'
          ruleSetAction: 'Block'
        }
      ]
    }
    customRules: {
      rules: [
        {
          name: 'RateLimitRule'
          enabledState: 'Enabled'
          priority: 100
          ruleType: 'RateLimitRule'
          rateLimitThreshold: 1000
          rateLimitDurationInMinutes: 1
          matchConditions: [
            {
              matchVariable: 'RequestUri'
              operator: 'RegEx'
              matchValue: [
                '/api/.*'
              ]
            }
          ]
          action: 'Block'
        }
      ]
    }
  }
}

// Front Door profile
resource frontDoor 'Microsoft.Cdn/profiles@2024-02-01' = {
  name: frontDoorName
  location: location
  sku: {
    name: 'Premium_AzureFrontDoor'
  }
  properties: {
    originResponseTimeoutSeconds: 30
  }
}

// Endpoint
resource endpoint 'Microsoft.Cdn/profiles/afdEndpoints@2024-02-01' = {
  parent: frontDoor
  name: '${baseName}-endpoint'
  location: location
  properties: {
    enabledState: 'Enabled'
  }
}

// Origin group — session manager
resource sessionOriginGroup 'Microsoft.Cdn/profiles/originGroups@2024-02-01' = {
  parent: frontDoor
  name: 'session-origins'
  properties: {
    loadBalancingSettings: {
      sampleSize: 4
      successfulSamplesRequired: 3
      additionalLatencyInMilliseconds: 50
    }
    healthProbeSettings: {
      probePath: '/healthz'
      probeRequestType: 'GET'
      probeProtocol: 'Https'
      probeIntervalInSeconds: 30
    }
    sessionAffinityState: 'Enabled'
  }
}

resource sessionOrigin 'Microsoft.Cdn/profiles/originGroups/origins@2024-02-01' = {
  parent: sessionOriginGroup
  name: 'session-manager'
  properties: {
    hostName: sessionManagerFqdn
    httpPort: 80
    httpsPort: 443
    originHostHeader: sessionManagerFqdn
    priority: 1
    weight: 1000
  }
}

// Origin group — gateway
resource gatewayOriginGroup 'Microsoft.Cdn/profiles/originGroups@2024-02-01' = {
  parent: frontDoor
  name: 'gateway-origins'
  properties: {
    loadBalancingSettings: {
      sampleSize: 4
      successfulSamplesRequired: 3
      additionalLatencyInMilliseconds: 50
    }
    healthProbeSettings: {
      probePath: '/healthz'
      probeRequestType: 'GET'
      probeProtocol: 'Https'
      probeIntervalInSeconds: 30
    }
  }
}

resource gatewayOrigin 'Microsoft.Cdn/profiles/originGroups/origins@2024-02-01' = {
  parent: gatewayOriginGroup
  name: 'gateway'
  properties: {
    hostName: gatewayFqdn
    httpPort: 80
    httpsPort: 443
    originHostHeader: gatewayFqdn
    priority: 1
    weight: 1000
  }
}

// Route: /api/* → session manager
resource apiRoute 'Microsoft.Cdn/profiles/afdEndpoints/routes@2024-02-01' = {
  parent: endpoint
  name: 'api-route'
  properties: {
    originGroup: {
      id: sessionOriginGroup.id
    }
    patternsToMatch: [
      '/api/*'
    ]
    supportedProtocols: [
      'Https'
    ]
    httpsRedirect: 'Enabled'
    forwardingProtocol: 'HttpsOnly'
    linkToDefaultDomain: 'Enabled'
  }
  dependsOn: [
    sessionOrigin
  ]
}

// Route: /ws/* → gateway
resource wsRoute 'Microsoft.Cdn/profiles/afdEndpoints/routes@2024-02-01' = {
  parent: endpoint
  name: 'ws-route'
  properties: {
    originGroup: {
      id: gatewayOriginGroup.id
    }
    patternsToMatch: [
      '/ws/*'
      '/'
    ]
    supportedProtocols: [
      'Https'
    ]
    httpsRedirect: 'Enabled'
    forwardingProtocol: 'HttpsOnly'
    linkToDefaultDomain: 'Enabled'
  }
  dependsOn: [
    gatewayOrigin
  ]
}

// Security policy — attach WAF to endpoint
resource securityPolicy 'Microsoft.Cdn/profiles/securityPolicies@2024-02-01' = {
  parent: frontDoor
  name: 'waf-policy'
  properties: {
    parameters: {
      type: 'WebApplicationFirewall'
      wafPolicy: {
        id: wafPolicy.id
      }
      associations: [
        {
          domains: [
            {
              id: endpoint.id
            }
          ]
          patternsToMatch: [
            '/*'
          ]
        }
      ]
    }
  }
}

output frontDoorEndpoint string = endpoint.properties.hostName
output wafPolicyId string = wafPolicy.id
