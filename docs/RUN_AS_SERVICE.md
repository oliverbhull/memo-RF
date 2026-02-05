# Running memo-RF as a Service (survive closing terminal / Jetson session)

To keep memo-RF running after you close the terminal or disconnect from the Jetson (e.g. SSH), run it as a **systemd user service** and optionally enable **user lingering** so it keeps running after logout.

## 1. One-time setup

### 1.1 Install the user service

From the memo-RF repo root:

```bash
mkdir -p ~/.config/systemd/user
cp scripts/memo-rf.service ~/.config/systemd/user/
# If your repo is not in ~/dev/memo-RF, edit the service file:
#   WorkingDirectory=...
#   ExecStart=...
nano ~/.config/systemd/user/memo-rf.service
```

Reload and enable (start at login):

```bash
systemctl --user daemon-reload
systemctl --user enable memo-rf
```

### 1.2 (Optional) Keep running after you log out

By default, user services stop when you log out. To keep memo-RF running even after you close SSH or log out:

```bash
loginctl enable-linger $USER
```

After this, your user's systemd instance runs at boot and stays running after logout, so `memo-rf` will keep running.

## 2. Start / stop / status

```bash
# Start
systemctl --user start memo-rf

# Stop
systemctl --user stop memo-rf

# Status
systemctl --user status memo-rf

# View recent logs
journalctl --user -u memo-rf -f
```

## 3. Prerequisites

- **Ollama** (or your LLM server) must be running. If you use Ollama, start it before memo-rf or run it as a service too (e.g. `ollama serve` in a separate unit or use Ollama’s own systemd service if installed).
- **Build** the binary once: from repo root, `./build.sh` (so `build/memo-rf` exists).
- **Config** at `config/config.json` (paths in the service are relative to the repo, so `WorkingDirectory` must be the memo-RF repo root).

## 4. Paths

The service file uses `%h` (your home directory). If your repo is not under `~/dev/memo-RF`, edit `~/.config/systemd/user/memo-rf.service` and set:

- `WorkingDirectory=` to your memo-RF repo root
- `ExecStart=` to the full path of `build/memo-rf` (or `.../memo-rf config/config.json`)

Example for repo at `/home/oliver/dev/memo-RF`:

- `WorkingDirectory=%h/dev/memo-RF`
- `ExecStart=%h/dev/memo-RF/build/memo-rf config/config.json`
