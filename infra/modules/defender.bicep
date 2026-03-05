// modules/defender.bicep — Microsoft Defender for Containers on ACR

param acrName string

resource defenderForContainers 'Microsoft.Security/pricings@2024-01-01' = {
  name: 'Containers'
  properties: {
    pricingTier: 'Standard'
    extensions: [
      {
        name: 'ContainerRegistriesVulnerabilityAssessments'
        isEnabled: 'True'
      }
    ]
  }
}

// Note: Defender for Containers is enabled at the subscription level.
// The ACR name is provided for documentation but the actual enablement
// is subscription-scoped.
output defenderEnabled bool = true
output protectedRegistry string = acrName
