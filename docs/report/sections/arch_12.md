# systemd Deployment

Example:

```ini
[Unit]
Description=DRS Synchronization Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/drs_sync
Restart=on-failure
RestartSec=5s
CPUAffinity=3
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=85
LimitRTPRIO=95
LimitMEMLOCK=infinity
NoNewPrivileges=true

[Install]
WantedBy=multi-user.target
```


