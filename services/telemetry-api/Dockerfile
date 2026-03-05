FROM golang:1.22-bookworm AS builder
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 go build -o /telemetry-api ./cmd/telemetry-api

FROM gcr.io/distroless/static-debian12:nonroot
COPY --from=builder /telemetry-api /telemetry-api
EXPOSE 8084
ENTRYPOINT ["/telemetry-api"]
