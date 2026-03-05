FROM golang:1.22-bookworm AS builder
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 go build -o /assets-api ./cmd/assets-api

FROM gcr.io/distroless/static-debian12:nonroot
COPY --from=builder /assets-api /assets-api
EXPOSE 8083
ENTRYPOINT ["/assets-api"]
