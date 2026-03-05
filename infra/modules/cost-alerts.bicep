// modules/cost-alerts.bicep — Azure cost management alerts and budgets

param baseName string
param budgetAmount int = 500  // Monthly budget in USD
param alertThresholds array = [50, 80, 100]
param contactEmails array

var budgetName = '${baseName}-monthly-budget'

resource budget 'Microsoft.Consumption/budgets@2023-11-01' = {
  name: budgetName
  properties: {
    category: 'Cost'
    amount: budgetAmount
    timeGrain: 'Monthly'
    timePeriod: {
      startDate: '2025-01-01'
    }
    notifications: {
      forecast80: {
        enabled: true
        operator: 'GreaterThanOrEqualTo'
        threshold: alertThresholds[1]
        thresholdType: 'Forecasted'
        contactEmails: contactEmails
      }
      actual50: {
        enabled: true
        operator: 'GreaterThanOrEqualTo'
        threshold: alertThresholds[0]
        thresholdType: 'Actual'
        contactEmails: contactEmails
      }
      actual80: {
        enabled: true
        operator: 'GreaterThanOrEqualTo'
        threshold: alertThresholds[1]
        thresholdType: 'Actual'
        contactEmails: contactEmails
      }
      actual100: {
        enabled: true
        operator: 'GreaterThanOrEqualTo'
        threshold: alertThresholds[2]
        thresholdType: 'Actual'
        contactEmails: contactEmails
      }
    }
  }
}
