# Remote SSH setup helper

On the **remote authorized Linux laptop**:

```bash
sudo apt update
sudo apt install openssh-server -y
sudo systemctl enable --now ssh
```

On the **main laptop**:

```bash
sudo apt install openssh-client -y
ssh-keygen -t ed25519
ssh-copy-id user@REMOTE_HOST
ssh user@REMOTE_HOST
```

The final `ssh` command must succeed without an interactive password prompt before the dashboard can use BatchMode. The project itself is **not** copied to the remote laptop.
