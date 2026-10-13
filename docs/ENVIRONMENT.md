# Experimental Environment

## Hypervisor
VMware Workstation

## VM Resources
```text
4 vCPUs per VM
4 GB RAM per VM
```

## Network
```text
Network: VMnet2
Mode: Host-only
Subnet: 192.168.250.0/24
DHCP: Disabled
```

Use static IP addresses. Before external tests verify connectivity with `ping`.

## CPU Affinity
Some proposed runs use:
```bash
taskset -c CPU_ID ./program ...
```
This reduces process migration and scheduler-related jitter.

## Before Each Session
Verify both VMs, VMnet2, static IPs, connectivity, host load, correct binary, and message size.
