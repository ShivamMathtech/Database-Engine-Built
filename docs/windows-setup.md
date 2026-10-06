# Windows setup for a first run

CDB's implemented file backend uses POSIX APIs. Run it on Windows through **WSL2 and Ubuntu**. This ZIP does not contain a native `cdb.exe`, and its C sources are not currently an MSVC project. Windows/WSL execution was not available for validation here.

## 1. Install Ubuntu through WSL

On a supported Windows 10/11 system, open **PowerShell as Administrator**:

```powershell
wsl --install
```

Restart if requested. Open **Ubuntu** from the Start menu and create its Linux username/password. If WSL is already installed but Ubuntu is missing, use:

```powershell
wsl --install -d Ubuntu
wsl --list --verbose
```

Microsoft documents current prerequisites and troubleshooting in its [official WSL install guide](https://learn.microsoft.com/en-us/windows/wsl/install). WSL installation may require Windows/virtualization configuration and a restart; it is independent of CDB.

## 2. Install the build tools inside Ubuntu

All remaining commands in this guide run in the **Ubuntu terminal**, not PowerShell:

```bash
sudo apt update
sudo apt install build-essential unzip
```

Your password is intentionally not echoed while typing after `sudo`. Build tools are system dependencies; CDB itself has no external database or C library dependency beyond the system toolchain/POSIX runtime.

## 3. Extract the ZIP into Linux storage

Save `CDB-v0.1.0-Source.zip` in your Windows Downloads folder. Replace `YOUR_WINDOWS_USER` below with the Windows folder name under `C:\Users`:

```bash
mkdir -p ~/projects
unzip /mnt/c/Users/YOUR_WINDOWS_USER/Downloads/CDB-v0.1.0-Source.zip -d ~/projects
cd ~/projects/cdb
```

If the folder already exists from an earlier extraction, use a new destination folder so files are not mixed between versions. Building inside the Linux home directory avoids several permissions/filesystem differences of running directly in `/mnt/c`. The database and its WAL must stay together on a filesystem with working sync and locking.

## 4. Compile and verify

```bash
make
make test
```

Expected result: `bin/cdb` is created, deterministic tests print PASS, and the CLI test passes. No VS Code extension, Python package, SQLite, database server, or network port is needed.

## 5. Run the example

```bash
./bin/cdb my-university.cdb --file examples/university.sql
./bin/cdb my-university.cdb
```

Inside CDB:

```sql
SELECT * FROM students;
SELECT name, age FROM students WHERE age > 23;
```

Then type `.quit`. Reopen `./bin/cdb my-university.cdb` and SELECT again to see persistence.

## 6. Understand the prompts

| Prompt/location | What belongs there |
|---|---|
| PowerShell | `wsl --install` and Windows setup commands |
| Ubuntu shell (`$`) | `cd`, `make`, `./bin/cdb ...` |
| CDB (`cdb>`) | SQL ending in `;`, or a meta command such as `.tables` |
| CDB (`...>`) | Continue the current multiline SQL statement |

Do not paste `$` or `cdb>` as part of a command. `.quit` closes CDB; `exit` closes the Ubuntu shell. CDB creates a database when the path does not already exist.

## Troubleshooting

- **`make: command not found`:** repeat the Ubuntu build-tool installation, not a PowerShell command.
- **`./bin/cdb: No such file`:** run `pwd`, confirm the directory contains `Makefile`, then run `make` and read the first error.
- **Table already exists:** use the existing file interactively, or choose a new filename for the initialization script.
- **BUSY / database locked:** close the other CDB process using that file. Do not delete its WAL.
- **Missing semicolon:** finish SQL with `;`. Meta commands such as `.quit` do not take one.
- **Wrong path:** filenames are case-sensitive in Ubuntu. A Windows path like `C:\Users\...` becomes `/mnt/c/Users/...`.
- **Sanitizer leak-enumeration failure:** see the documented ASan option in the README; disabling leak detection is not a leak-check pass.

Optional tools: `sudo apt install clang clang-format gdb valgrind`. They are not required for a normal database session.
