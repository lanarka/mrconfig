// Example application config

[server]
host: "0.0.0.0"
port: 8080
workers: 4
tags: ("web", "api", "v2")

[database]
host: server.host
port: 5432
pool: {
    min:     2
    max:     20
    timeout: 30
}
