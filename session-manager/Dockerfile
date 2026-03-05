FROM golang:1.22-bookworm AS builder
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 go build -o /session-manager ./cmd/session-manager

FROM gcr.io/distroless/static-debian12:nonroot
COPY --from=builder /session-manager /session-manager
EXPOSE 8082
ENTRYPOINT ["/session-manager"]
