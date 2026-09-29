# Security Notes

- Default bind address: `127.0.0.1`.
- `--bind-lan` is explicit and prints a warning.
- No SSH password field exists in the frontend.
- No private keys are stored in the project repository.
- Remote host/user/port validation rejects shell metacharacters.
- `ssh` is invoked through `execvp()` using fixed options/argv.
- Remote scripts are fixed application strings and are sent over stdin.
- PID routes accept decimal positive process IDs only.
- Request, body, URL, static-file and SSH-output sizes are bounded.
- Arbitrary browser command execution is not implemented.
- Privileged process reads fail safely with unavailable status.

- The WSL Windows-application bridge executes a fixed PowerShell query only; browser input is never appended to the command.
- Open Applications reads process/window metadata only. It does not collect browser history, every tab, passwords, or arbitrary URLs.
