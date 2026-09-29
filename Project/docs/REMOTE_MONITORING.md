# Agentless Remote Monitoring — IP-only Dashboard Experience

## Goal

Monitor another **authorized Linux laptop** from the main laptop without installing this project on the remote machine. The browser interaction is intentionally simple: after one-time SSH authorization, you type only the remote IPv4 address and press **Connect**.

```text
Browser (IP only)
      ↓
C monitor_web backend
      ↓ controlled fork/exec/pipe
OpenSSH client
      ↓ authorized key / SSH config
Remote Linux laptop
      ↓
/proc + /sys
```

## Why one-time authorization still exists

An IPv4 address identifies a machine; it does not grant permission to read that machine's process list, CPU, RAM, files, or kernel interfaces. The remote laptop must authorize the monitoring account once. After that, the website itself requires only the IP address.

## One-time remote laptop setup

```bash
sudo apt update
sudo apt install openssh-server -y
sudo systemctl enable --now ssh
```

Check:

```bash
systemctl status ssh
hostname -I
```

## One-time main laptop / WSL setup

```bash
sudo apt update
sudo apt install openssh-client -y
ssh-keygen -t ed25519
ssh-copy-id user@REMOTE_IP
ssh user@REMOTE_IP
```

The final command must enter the remote Linux account without an interactive password prompt.

### If the remote username differs from your normal local username

The IP-only backend first lets OpenSSH use its normal account resolution. The cleanest permanent setup is an SSH config entry:

```text
Host 192.168.1.42
    User aryan
    IdentityFile ~/.ssh/id_ed25519
```

Or start the monitor with a backend-only override:

```bash
export MONITOR_SSH_USER=aryan
./monitor_web
```

No username field is shown in the website.

## Dashboard flow

1. Open **Machines** from the left sidebar, or click **Add remote machine** on Overview.
2. Type only the authorized remote IPv4 address, for example `192.168.1.42`.
3. Press **Connect**.
4. `POST /api/quick-connect` validates the IPv4 address.
5. The backend tests SSH and verifies remote `/proc` is readable.
6. The machine is saved automatically if it is new.
7. It becomes the selected monitoring source automatically.
8. Overview, Processes, Lifecycle, Network and Process Inspector all change to the remote machine.

## What the remote laptop needs

- Linux
- OpenSSH server
- ordinary authorized SSH account/key
- standard tools such as `sh`, `awk`, `tr`, `df`
- readable `/proc` and `/sys`

It does **not** need `monitor_web`, the frontend, this repository, or the project ZIP.

## Security model

- Browser quick connect accepts only a strict IPv4 address.
- `BatchMode=yes` prevents interactive password collection.
- Port defaults to 22 and can only be overridden on the backend.
- The local program executes `ssh` using `execvp()` argv, not browser-built `sh -c`.
- The remote monitoring scripts are fixed in C source.
- No private key is saved in the project directory or sent to the browser.
- The UI is monitoring-only; there is no arbitrary remote terminal.

## Expected connection errors

- **SSH client is not installed** — install `openssh-client` on the main Linux/WSL environment.
- **SSH authentication required** — complete the one-time key authorization / SSH config.
- **Remote host unreachable** — verify IPv4 and routing.
- **Connection timed out** — check Wi-Fi/LAN/firewall.
- **SSH connection refused** — start OpenSSH server on the remote laptop.
- **Remote Linux /proc unavailable** — the selected target is not exposing the expected Linux `/proc`.

## Final two-physical-laptop validation

1. Enable SSH on the remote Linux laptop and note its IPv4.
2. Complete key authorization once and verify manual SSH.
3. Start `./monitor_web` on the main laptop.
4. Open **Machines** and type only the remote IPv4.
5. Press **Connect** and verify the source changes to `REMOTE — LIVE`.
6. Confirm hostname/kernel/distro match the remote laptop.
7. Start `sleep 20` on the remote laptop and confirm its PID appears in **Processes**.
8. Inspect the PID from the main dashboard.
9. Let it exit and confirm **Lifecycle** records `EXITED`.
10. Verify **Network** reports the remote interfaces.
11. Disconnect the remote network and confirm the dashboard reports unavailable instead of falling back to local values.
