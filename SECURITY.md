# Security

## Reporting a problem

If you find a security issue, please don't open a public issue. Report it privately from the **Security** tab of this repo (**Report a vulnerability**), and I'll get back to you.

## Get TrayVitals only from here

Download TrayVitals only from this repo's releases page: https://github.com/Fakem1d/TrayVitals/releases

If someone sends you TrayVitals from anywhere else (a file host, a Discord message, another website), don't run it.

## Public code, public builds

All of TrayVitals' code is in this repo, in [`src`](src). Nothing is left out.

Releases aren't built on anyone's PC. GitHub builds each one on its own machines with [this workflow](.github/workflows/build.yml), straight from the code of that release, and the workflow attaches the installer to the release. You can open any release's build under **Actions** and see every step.

### Check that your download is the real one

**Check the checksum.** In PowerShell:

```powershell
Get-FileHash .\TrayVitals-Setup.exe
```

The hash should match the `sha256:` value next to TrayVitals-Setup.exe under **Assets** on the release page, and the one in `TrayVitals-Setup.exe.sha256`.

**Or check the build attestation** with the [GitHub CLI](https://cli.github.com/):

```
gh attestation verify TrayVitals-Setup.exe --repo Fakem1d/TrayVitals
```

This proves the file was built by this repo's workflow on GitHub, not uploaded from somewhere else.

### Build it yourself

1. Get [w64devkit](https://github.com/skeeto/w64devkit) (a free, portable MinGW-w64 compiler for Windows) and open `w64devkit.exe`.
2. Download this repo (**Code > Download ZIP**), unzip it, and go to that folder.
3. Run `sh build.sh`.

You get `TrayVitals-Setup.exe` in the same folder. The script downloads the PawnIO installer from its official release and checks its SHA-256 first (see [THIRD_PARTY.md](THIRD_PARTY.md)).

Your build will usually differ from the release by a few bytes, because a different compiler version makes slightly different machine code. Built with the same compiler version, it comes out identical.

## What TrayVitals does on your PC

**Runs as administrator.** Only with your OK (Windows asks first):
- the installer needs administrator rights to install to Program Files;
- TrayVitals itself needs them only for CPU temperature and CPU power, because Windows only lets admin programs read those sensors. Then it starts at login as administrator through a scheduled task named "TrayVitals". Without CPU temperature it runs as a normal program.

That task always runs the copy in `C:\Program Files\TrayVitals`, which only administrators can change. So even if you install TrayVitals to a folder anyone can write to, nobody can swap in a file that then runs as admin.

**Goes online.** Only for these:
- **The update check, once a day.** It asks `api.github.com` for the latest release of this repo. You can turn it off in **right-click > Updates**.
- **The update download, when you click Update now.** TrayVitals downloads `TrayVitals-Setup.exe` only from `github.com`. Before running it, TrayVitals checks its SHA-256 against the checksum GitHub lists for that file, and if they don't match it isn't run. Windows asks for administrator rights before it installs.
- **The PawnIO installer, if needed.** If the CPU temperature driver is missing and you turn on CPU temperature, TrayVitals downloads the official PawnIO 2.2.0 installer from PawnIO's own GitHub release and runs it only if its SHA-256 matches the one built into TrayVitals.

TrayVitals doesn't send anything about you or your PC, and it has no tracking or ads.

**Installs a driver.** The PawnIO driver, only if you leave its box ticked in the installer. See [THIRD_PARTY.md](THIRD_PARTY.md).

**Uninstalls cleanly.** Uninstalling removes the program, its shortcuts, the startup task, its settings and, if you choose, the PawnIO driver.

## Why Windows warns about it

Windows SmartScreen shows "Windows protected your PC" for programs that aren't signed with a paid code-signing certificate and that few people have downloaded yet. TrayVitals isn't signed, so you'll see it. It isn't a virus warning. If you want to be sure, check the checksum or the attestation above, or build it yourself.
